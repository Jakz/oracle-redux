#!/usr/bin/env python3
"""Catalog and compare Game Boy Color ROMs without modifying them.

The analysis is deliberately conservative:

* header and checksum facts are exact;
* fixed-bank code is found by recursive traversal from hardware vectors;
* ROM-wide reuse is measured with exact byte shingles and must not be read as
  a code-only percentage;
* shared regions are candidate code/data regions until manually classified.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from collections import Counter, defaultdict, deque
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


BANK_SIZE = 0x4000
NINTENDO_LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D"
    "0008111F8889000EDCCC6EE6DDDDD999"
    "BBBB67636E0EECCCDDDC999FBBB9333E"
)

CART_TYPES = {
    0x00: "ROM ONLY",
    0x01: "MBC1",
    0x02: "MBC1+RAM",
    0x03: "MBC1+RAM+BATTERY",
    0x19: "MBC5",
    0x1A: "MBC5+RAM",
    0x1B: "MBC5+RAM+BATTERY",
    0x1C: "MBC5+RUMBLE",
    0x1D: "MBC5+RUMBLE+RAM",
    0x1E: "MBC5+RUMBLE+RAM+BATTERY",
}
ROM_SIZES = {
    0x00: (32 * 1024, 2),
    0x01: (64 * 1024, 4),
    0x02: (128 * 1024, 8),
    0x03: (256 * 1024, 16),
    0x04: (512 * 1024, 32),
    0x05: (1024 * 1024, 64),
    0x06: (2 * 1024 * 1024, 128),
    0x07: (4 * 1024 * 1024, 256),
    0x08: (8 * 1024 * 1024, 512),
}
RAM_SIZES = {
    0x00: (0, 0),
    0x01: (2 * 1024, 0),
    0x02: (8 * 1024, 1),
    0x03: (32 * 1024, 4),
    0x04: (128 * 1024, 16),
    0x05: (64 * 1024, 8),
}
GAME_CODES = {
    "AZ7P": "seasons",
    "AZ8P": "ages",
    "AZ7E": "seasons",
    "AZ8E": "ages",
    "AZ7J": "seasons",
    "AZ8J": "ages",
}
REGION_CODES = {
    "E": "usa",
    "P": "europe",
    "J": "japan",
}
ORACLES_DISASM_MD5 = {
    ("ages", "usa"): "c4639cc61c049e5a085526bb6cac03bb",
    ("seasons", "usa"): "f2dc6c4e093e4f8c6cbea80e8dbd62cb",
}


@dataclass(frozen=True)
class Header:
    title: str
    game_code: str
    detected_game: str
    cgb_flag: int
    new_licensee: str
    sgb_flag: int
    cartridge_type_code: int
    cartridge_type: str
    rom_size_code: int
    declared_rom_bytes: int | None
    declared_rom_banks: int | None
    ram_size_code: int
    declared_ram_bytes: int | None
    declared_ram_banks: int | None
    destination_code: int
    old_licensee_code: int
    mask_rom_version: int
    header_checksum: int
    computed_header_checksum: int
    header_checksum_valid: bool
    global_checksum: int
    computed_global_checksum: int
    global_checksum_valid: bool
    nintendo_logo_valid: bool


def ascii_field(value: bytes) -> str:
    return value.rstrip(b"\0 ").decode("ascii", errors="replace")


def parse_header(data: bytes) -> Header:
    if len(data) < 0x150:
        raise ValueError("file is too small to contain a Game Boy header")

    # CGB-era headers use 11 title bytes followed by a four-byte game code.
    title = ascii_field(data[0x134:0x13F])
    game_code = ascii_field(data[0x13F:0x143])
    header_checksum = data[0x14D]
    computed_header_checksum = 0
    for value in data[0x134:0x14D]:
        computed_header_checksum = (computed_header_checksum - value - 1) & 0xFF

    global_checksum = int.from_bytes(data[0x14E:0x150], "big")
    computed_global_checksum = (
        sum(data) - data[0x14E] - data[0x14F]
    ) & 0xFFFF
    rom_size = ROM_SIZES.get(data[0x148])
    ram_size = RAM_SIZES.get(data[0x149])

    return Header(
        title=title,
        game_code=game_code,
        detected_game=GAME_CODES.get(game_code, "unknown"),
        cgb_flag=data[0x143],
        new_licensee=ascii_field(data[0x144:0x146]),
        sgb_flag=data[0x146],
        cartridge_type_code=data[0x147],
        cartridge_type=CART_TYPES.get(data[0x147], "UNKNOWN"),
        rom_size_code=data[0x148],
        declared_rom_bytes=rom_size[0] if rom_size else None,
        declared_rom_banks=rom_size[1] if rom_size else None,
        ram_size_code=data[0x149],
        declared_ram_bytes=ram_size[0] if ram_size else None,
        declared_ram_banks=ram_size[1] if ram_size else None,
        destination_code=data[0x14A],
        old_licensee_code=data[0x14B],
        mask_rom_version=data[0x14C],
        header_checksum=header_checksum,
        computed_header_checksum=computed_header_checksum,
        header_checksum_valid=header_checksum == computed_header_checksum,
        global_checksum=global_checksum,
        computed_global_checksum=computed_global_checksum,
        global_checksum_valid=global_checksum == computed_global_checksum,
        nintendo_logo_valid=data[0x104:0x134] == NINTENDO_LOGO,
    )


def region_from_game_code(game_code: str) -> str:
    return REGION_CODES.get(game_code[-1:] if game_code else "", "unknown")


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    length = len(data)
    return -sum(
        (count / length) * math.log2(count / length)
        for count in counts.values()
    )


def cpu_address(bank: int, offset: int) -> int:
    return offset if bank == 0 else 0x4000 + offset


def fmt_location(bank: int, offset: int) -> str:
    return f"{bank:02X}:{cpu_address(bank, offset):04X}"


def informative(chunk: bytes) -> bool:
    return len(set(chunk)) >= 6 and chunk.count(0x00) < len(chunk) * 3 // 4


def shingles(bank: bytes, size: int = 32, stride: int = 16) -> set[bytes]:
    return {
        chunk
        for offset in range(0, len(bank) - size + 1, stride)
        if informative(chunk := bank[offset : offset + size])
    }


def matching_runs_same_offset(a: bytes, b: bytes, minimum: int = 32) -> list[tuple[int, int]]:
    runs: list[tuple[int, int]] = []
    start: int | None = None
    for i, (left, right) in enumerate(zip(a, b)):
        if left == right:
            if start is None:
                start = i
        elif start is not None:
            if i - start >= minimum:
                runs.append((start, i - start))
            start = None
    if start is not None and len(a) - start >= minimum:
        runs.append((start, len(a) - start))
    return runs


def candidate_shared_regions(
    a: bytes,
    b: bytes,
    *,
    anchor_size: int = 24,
    minimum: int = 64,
) -> list[tuple[int, int, int]]:
    """Find maximal exact regions using unique, byte-granular anchors."""
    occurrences: dict[bytes, int] = {}
    duplicates: set[bytes] = set()
    for offset in range(0, len(b) - anchor_size + 1):
        anchor = b[offset : offset + anchor_size]
        if not informative(anchor):
            continue
        if anchor in occurrences:
            duplicates.add(anchor)
        else:
            occurrences[anchor] = offset
    for anchor in duplicates:
        occurrences.pop(anchor, None)

    regions: set[tuple[int, int, int]] = set()
    covered_until = -1
    for a_offset in range(0, len(a) - anchor_size + 1):
        if a_offset < covered_until:
            continue
        anchor = a[a_offset : a_offset + anchor_size]
        b_offset = occurrences.get(anchor)
        if b_offset is None:
            continue

        left = 0
        while (
            a_offset - left - 1 >= 0
            and b_offset - left - 1 >= 0
            and a[a_offset - left - 1] == b[b_offset - left - 1]
        ):
            left += 1
        right = anchor_size
        while (
            a_offset + right < len(a)
            and b_offset + right < len(b)
            and a[a_offset + right] == b[b_offset + right]
        ):
            right += 1

        length = left + right
        if length >= minimum:
            region = (a_offset - left, b_offset - left, length)
            regions.add(region)
            covered_until = max(covered_until, a_offset - left + length)
    return sorted(regions, key=lambda item: (-item[2], item[0], item[1]))


# LR35902 instruction lengths needed for conservative fixed-bank traversal.
THREE_BYTE = {
    0x01, 0x08, 0x11, 0x21, 0x31,
    0xC2, 0xC3, 0xC4, 0xCA, 0xCC, 0xCD,
    0xD2, 0xD4, 0xDA, 0xDC, 0xEA, 0xFA,
}
TWO_BYTE = {
    0x06, 0x0E, 0x10, 0x16, 0x18, 0x1E,
    0x20, 0x26, 0x28, 0x2E, 0x30, 0x36, 0x38, 0x3E,
    0xC6, 0xCB, 0xCE, 0xD6, 0xDE,
    0xE0, 0xE6, 0xE8, 0xEE, 0xF0, 0xF6, 0xF8, 0xFE,
}
INVALID_OPCODES = {0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD}
JP_UNCONDITIONAL = {0xC3}
JP_CONDITIONAL = {0xC2, 0xCA, 0xD2, 0xDA}
JR_UNCONDITIONAL = {0x18}
JR_CONDITIONAL = {0x20, 0x28, 0x30, 0x38}
CALLS = {0xC4, 0xCC, 0xCD, 0xD4, 0xDC}
TERMINATORS = {0xC9, 0xD9, 0xE9}
RSTS = {0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF}


def opcode_length(opcode: int) -> int:
    if opcode in THREE_BYTE:
        return 3
    if opcode in TWO_BYTE:
        return 2
    return 1


@dataclass
class FixedBankCode:
    instruction_starts: set[int]
    code_bytes: set[int]
    block_entries: set[int]
    banked_references: list[tuple[int, int, str]]
    invalid_paths: list[int]


def traverse_bank(
    data: bytes,
    bank_number: int,
    seeds: set[int],
) -> FixedBankCode:
    bank = data[bank_number * BANK_SIZE : (bank_number + 1) * BANK_SIZE]
    queue = deque(sorted(seeds))
    visited: set[int] = set()
    instruction_starts: set[int] = set()
    code_bytes: set[int] = set()
    block_entries: set[int] = set(seeds)
    banked_references: list[tuple[int, int, str]] = []
    invalid_paths: list[int] = []

    def enqueue(target: int) -> None:
        if 0 <= target < BANK_SIZE and target not in visited:
            block_entries.add(target)
            queue.append(target)

    while queue:
        pc = queue.popleft()
        while 0 <= pc < BANK_SIZE and pc not in visited:
            visited.add(pc)
            opcode = bank[pc]
            if opcode in INVALID_OPCODES:
                invalid_paths.append(pc)
                break
            length = opcode_length(opcode)
            if pc + length > BANK_SIZE:
                break
            instruction_starts.add(pc)
            code_bytes.update(range(pc, pc + length))
            next_pc = pc + length

            if opcode in JP_UNCONDITIONAL | JP_CONDITIONAL | CALLS:
                target = int.from_bytes(bank[pc + 1 : pc + 3], "little")
                kind = "call" if opcode in CALLS else "jump"
                if bank_number == 0 and target < BANK_SIZE:
                    enqueue(target)
                elif bank_number != 0 and BANK_SIZE <= target < 0x8000:
                    enqueue(target - BANK_SIZE)
                elif target < 0x8000:
                    banked_references.append((pc, target, kind))
                if opcode in JP_UNCONDITIONAL:
                    break
            elif opcode in JR_UNCONDITIONAL | JR_CONDITIONAL:
                displacement = int.from_bytes(
                    bank[pc + 1 : pc + 2], "little", signed=True
                )
                enqueue((next_pc + displacement) & 0xFFFF)
                if opcode in JR_UNCONDITIONAL:
                    break
            elif opcode in RSTS:
                enqueue(opcode & 0x38)
            elif opcode in TERMINATORS:
                break
            pc = next_pc

    return FixedBankCode(
        instruction_starts=instruction_starts,
        code_bytes=code_bytes,
        block_entries=block_entries,
        banked_references=sorted(set(banked_references)),
        invalid_paths=sorted(set(invalid_paths)),
    )


def traverse_fixed_bank(data: bytes) -> FixedBankCode:
    seeds = {
        0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030, 0x0038,
        0x0040, 0x0048, 0x0050, 0x0058, 0x0060, 0x0100,
    }
    return traverse_bank(data, 0, seeds)


def is_padding_bank(bank: bytes) -> bool:
    counts = Counter(bank)
    return max(counts.values(), default=0) >= len(bank) - 1


def write_csv(path: Path, fieldnames: list[str], rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def analyze(rom_paths: list[Path], output_dir: Path) -> dict:
    if len(rom_paths) != 2:
        raise ValueError(f"expected exactly two .gb/.gbc files, found {len(rom_paths)}")

    roms = []
    for path in rom_paths:
        data = path.read_bytes()
        if len(data) % BANK_SIZE:
            raise ValueError(f"{path} size is not a multiple of 16 KiB")
        header = parse_header(data)
        banks = [
            data[offset : offset + BANK_SIZE]
            for offset in range(0, len(data), BANK_SIZE)
        ]
        fixed_code = traverse_fixed_bank(data)
        entry_bank_code = traverse_bank(data, 1, {0})
        roms.append(
            {
                "path": path,
                "data": data,
                "header": header,
                "banks": banks,
                "shingles": [shingles(bank) for bank in banks],
                "fixed_code": fixed_code,
                "entry_bank_code": entry_bank_code,
            }
        )

    left, right = roms
    comparison_length = min(len(left["data"]), len(right["data"]))
    aligned_bank_count = min(len(left["banks"]), len(right["banks"]))

    bank_rows: list[dict] = []
    best_pairs: dict[int, list[tuple[float, int, int]]] = {}
    pair_scores: dict[tuple[int, int], tuple[float, int]] = {}
    for left_bank, left_set in enumerate(left["shingles"]):
        scored = []
        for right_bank, right_set in enumerate(right["shingles"]):
            shared = len(left_set & right_set)
            denominator = len(left_set) + len(right_set)
            dice = (2 * shared / denominator) if denominator else 0.0
            pair_scores[(left_bank, right_bank)] = (dice, shared)
            scored.append((dice, shared, right_bank))
        best_pairs[left_bank] = sorted(scored, reverse=True)[:3]

    for bank_number, (left_bank_data, right_bank_data) in enumerate(
        zip(left["banks"], right["banks"])
    ):
        same = sum(a == b for a, b in zip(left_bank_data, right_bank_data))
        left_counts = Counter(left_bank_data)
        right_counts = Counter(right_bank_data)
        best_dice, best_shared, best_right = best_pairs[bank_number][0]
        bank_rows.append(
            {
                "bank": f"{bank_number:02X}",
                "left_sha256": hashlib.sha256(left_bank_data).hexdigest(),
                "right_sha256": hashlib.sha256(right_bank_data).hexdigest(),
                "identical_bank": left_bank_data == right_bank_data,
                "same_offset_bytes": same,
                "same_offset_percent": f"{100 * same / BANK_SIZE:.3f}",
                "left_entropy": f"{entropy(left_bank_data):.4f}",
                "right_entropy": f"{entropy(right_bank_data):.4f}",
                "left_zero_bytes": left_counts[0],
                "right_zero_bytes": right_counts[0],
                "left_ff_bytes": left_counts[0xFF],
                "right_ff_bytes": right_counts[0xFF],
                "best_right_bank": f"{best_right:02X}",
                "best_dice_percent": f"{100 * best_dice:.3f}",
                "shared_informative_shingles": best_shared,
            }
        )

    candidate_pairs = {
        (left_bank, right_bank)
        for left_bank, matches in best_pairs.items()
        for _, _, right_bank in matches
    }
    for right_bank in range(len(right["banks"])):
        reverse_scores = sorted(
            (
                (pair_scores[(left_bank, right_bank)][0],
                 pair_scores[(left_bank, right_bank)][1],
                 left_bank)
                for left_bank in range(len(left["banks"]))
            ),
            reverse=True,
        )
        candidate_pairs.update(
            (left_bank, right_bank)
            for _, _, left_bank in reverse_scores[:3]
        )
    candidate_pairs.update((bank, bank) for bank in range(aligned_bank_count))
    shared_regions: list[dict] = []
    for left_bank, right_bank in sorted(candidate_pairs):
        dice, shared = pair_scores[(left_bank, right_bank)]
        if shared < 2 and left_bank != right_bank:
            continue
        for left_offset, right_offset, length in candidate_shared_regions(
            left["banks"][left_bank], right["banks"][right_bank]
        ):
            shared_regions.append(
                {
                    "left_bank": f"{left_bank:02X}",
                    "left_cpu_address": f"{cpu_address(left_bank, left_offset):04X}",
                    "right_bank": f"{right_bank:02X}",
                    "right_cpu_address": f"{cpu_address(right_bank, right_offset):04X}",
                    "length": length,
                    "same_bank": left_bank == right_bank,
                    "bank_pair_dice_percent": f"{100 * dice:.3f}",
                }
            )
    shared_regions.sort(key=lambda row: (-row["length"], row["left_bank"], row["left_cpu_address"]))

    left_fixed: FixedBankCode = left["fixed_code"]
    right_fixed: FixedBankCode = right["fixed_code"]
    common_code_positions = left_fixed.code_bytes & right_fixed.code_bytes
    equal_common_code_positions = {
        offset
        for offset in common_code_positions
        if left["data"][offset] == right["data"][offset]
    }
    left_entry: FixedBankCode = left["entry_bank_code"]
    right_entry: FixedBankCode = right["entry_bank_code"]
    common_entry_positions = left_entry.code_bytes & right_entry.code_bytes
    equal_common_entry_positions = {
        offset
        for offset in common_entry_positions
        if left["banks"][1][offset] == right["banks"][1][offset]
    }

    bank_one_regions = candidate_shared_regions(
        left["banks"][1], right["banks"][1]
    )
    relocated_entry_code: set[int] = set()
    for left_offset, right_offset, length in bank_one_regions:
        for delta in range(length):
            if (
                left_offset + delta in left_entry.code_bytes
                and right_offset + delta in right_entry.code_bytes
            ):
                relocated_entry_code.add(left_offset + delta)

    fixed_rows = []
    for rom in roms:
        fixed: FixedBankCode = rom["fixed_code"]
        entry: FixedBankCode = rom["entry_bank_code"]
        fixed_rows.append(
            {
                "detected_game": rom["header"].detected_game,
                "filename": rom["path"].name,
                "instruction_starts": len(fixed.instruction_starts),
                "candidate_code_bytes": len(fixed.code_bytes),
                "candidate_code_percent_of_bank": f"{100 * len(fixed.code_bytes) / BANK_SIZE:.3f}",
                "block_entries": len(fixed.block_entries),
                "banked_references": len(fixed.banked_references),
                "invalid_paths": len(fixed.invalid_paths),
                "entry_bank_instruction_starts": len(entry.instruction_starts),
                "entry_bank_candidate_code_bytes": len(entry.code_bytes),
                "entry_bank_candidate_code_percent": f"{100 * len(entry.code_bytes) / BANK_SIZE:.3f}",
                "entry_bank_block_entries": len(entry.block_entries),
                "entry_bank_external_references": len(entry.banked_references),
                "entry_bank_invalid_paths": len(entry.invalid_paths),
            }
        )

    total_same_offset = sum(
        left["data"][offset] == right["data"][offset]
        for offset in range(comparison_length)
    )
    same_offset_runs = matching_runs_same_offset(
        left["data"][:comparison_length],
        right["data"][:comparison_length],
    )

    # Aligned exact-block reuse is a fast, reproducible lower-bound proxy.
    block_size = 16
    right_blocks = {
        right["data"][offset : offset + block_size]
        for offset in range(0, len(right["data"]) - block_size + 1, block_size)
    }
    left_blocks = [
        left["data"][offset : offset + block_size]
        for offset in range(0, len(left["data"]) - block_size + 1, block_size)
    ]
    reusable_blocks = sum(block in right_blocks for block in left_blocks)
    informative_left_blocks = [block for block in left_blocks if informative(block)]
    informative_right_blocks = {
        block
        for block in right_blocks
        if informative(block)
    }
    reusable_informative_blocks = sum(
        block in informative_right_blocks for block in informative_left_blocks
    )

    covered_left_bytes: set[int] = set()
    for region in shared_regions:
        bank = int(region["left_bank"], 16)
        address = int(region["left_cpu_address"], 16)
        offset = address if bank == 0 else address - BANK_SIZE
        global_offset = bank * BANK_SIZE + offset
        covered_left_bytes.update(range(global_offset, global_offset + region["length"]))

    padding_banks = [
        bank
        for bank, (left_bank, right_bank) in enumerate(
            zip(left["banks"], right["banks"])
        )
        if is_padding_bank(left_bank) and is_padding_bank(right_bank)
    ]
    populated_positions = [
        offset
        for offset in range(comparison_length)
        if offset // BANK_SIZE not in padding_banks
    ]
    populated_same_offset = sum(
        left["data"][offset] == right["data"][offset]
        for offset in populated_positions
    )
    for row in bank_rows:
        bank = int(row["bank"], 16)
        bank_start = bank * BANK_SIZE
        covered = sum(
            offset in covered_left_bytes
            for offset in range(bank_start, bank_start + BANK_SIZE)
        )
        row["candidate_shared_region_bytes"] = covered
        row["candidate_shared_region_percent"] = f"{100 * covered / BANK_SIZE:.3f}"

    serializable_roms = []
    for rom in roms:
        header_dict = asdict(rom["header"])
        serializable_roms.append(
            {
                "filename": rom["path"].name,
                "region": region_from_game_code(rom["header"].game_code),
                "bytes": len(rom["data"]),
                "banks": len(rom["banks"]),
                "sha256": hashlib.sha256(rom["data"]).hexdigest(),
                "md5": hashlib.md5(rom["data"]).hexdigest(),
                "matches_oracles_disasm": (
                    hashlib.md5(rom["data"]).hexdigest()
                    == ORACLES_DISASM_MD5.get(
                        (
                            rom["header"].detected_game,
                            region_from_game_code(rom["header"].game_code),
                        )
                    )
                ),
                "header": header_dict,
            }
        )

    filename_mismatches = [
        {
            "filename": rom["path"].name,
            "detected_game": rom["header"].detected_game,
        }
        for rom in roms
        if rom["header"].detected_game not in rom["path"].stem.lower()
    ]

    report = {
        "schema_version": 2,
        "bank_size": BANK_SIZE,
        "roms": serializable_roms,
        "filename_identity_mismatches": filename_mismatches,
        "comparison": {
            "left_filename": left["path"].name,
            "right_filename": right["path"].name,
            "left_game": left["header"].detected_game,
            "right_game": right["header"].detected_game,
            "left_region": region_from_game_code(left["header"].game_code),
            "right_region": region_from_game_code(right["header"].game_code),
            "left_bytes": len(left["data"]),
            "right_bytes": len(right["data"]),
            "compared_same_offset_bytes": comparison_length,
            "same_offset_bytes": total_same_offset,
            "same_offset_percent": 100 * total_same_offset / comparison_length,
            "same_offset_runs_at_least_32_bytes": len(same_offset_runs),
            "same_offset_run_bytes_at_least_32": sum(length for _, length in same_offset_runs),
            "aligned_16_byte_blocks": len(left_blocks),
            "aligned_16_byte_blocks_found_anywhere_in_right": reusable_blocks,
            "aligned_16_byte_reuse_percent": 100 * reusable_blocks / len(left_blocks),
            "informative_aligned_16_byte_blocks": len(informative_left_blocks),
            "informative_aligned_16_byte_blocks_found_anywhere_in_right": reusable_informative_blocks,
            "informative_aligned_16_byte_reuse_percent": (
                100 * reusable_informative_blocks / len(informative_left_blocks)
                if informative_left_blocks
                else 0.0
            ),
            "identical_banks": sum(row["identical_bank"] for row in bank_rows),
            "identical_non_padding_banks": sum(
                row["identical_bank"]
                and int(row["bank"], 16) not in padding_banks
                for row in bank_rows
            ),
            "common_padding_banks": [f"{bank:02X}" for bank in padding_banks],
            "populated_same_offset_bytes": populated_same_offset,
            "populated_same_offset_percent": (
                100 * populated_same_offset / len(populated_positions)
                if populated_positions
                else 0.0
            ),
            "candidate_shared_regions_at_least_64_bytes": len(shared_regions),
            "candidate_shared_region_bytes_sum_overlaps_allowed": sum(
                row["length"] for row in shared_regions
            ),
            "candidate_shared_region_unique_left_bytes": len(covered_left_bytes),
            "candidate_shared_region_unique_left_percent": (
                100 * len(covered_left_bytes) / len(left["data"])
            ),
            "candidate_shared_region_unique_left_percent_of_populated": (
                100 * len(covered_left_bytes) / len(populated_positions)
                if populated_positions
                else 0.0
            ),
            "fixed_bank_code": {
                "left_candidate_code_bytes": len(left_fixed.code_bytes),
                "right_candidate_code_bytes": len(right_fixed.code_bytes),
                "same_offset_candidate_code_bytes_in_both": len(common_code_positions),
                "equal_bytes_among_same_offset_candidate_code": len(equal_common_code_positions),
                "equal_percent_among_same_offset_candidate_code": (
                    100 * len(equal_common_code_positions) / len(common_code_positions)
                    if common_code_positions
                    else 0.0
                ),
            },
            "entry_bank_code": {
                "left_candidate_code_bytes": len(left_entry.code_bytes),
                "right_candidate_code_bytes": len(right_entry.code_bytes),
                "same_offset_candidate_code_bytes_in_both": len(common_entry_positions),
                "equal_bytes_among_same_offset_candidate_code": len(equal_common_entry_positions),
                "equal_percent_among_same_offset_candidate_code": (
                    100 * len(equal_common_entry_positions) / len(common_entry_positions)
                    if common_entry_positions
                    else 0.0
                ),
                "left_candidate_code_bytes_matched_in_exact_regions": len(relocated_entry_code),
                "left_candidate_code_exact_region_match_percent": (
                    100 * len(relocated_entry_code) / len(left_entry.code_bytes)
                    if left_entry.code_bytes
                    else 0.0
                ),
            },
        },
    }

    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "rom-catalog.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(output_dir / "bank-overlap.csv", list(bank_rows[0]), bank_rows)
    write_csv(
        output_dir / "shared-regions.csv",
        list(shared_regions[0]) if shared_regions else [
            "left_bank", "left_cpu_address", "right_bank",
            "right_cpu_address", "length", "same_bank",
            "bank_pair_dice_percent",
        ],
        shared_regions,
    )
    write_csv(output_dir / "fixed-bank-code.csv", list(fixed_rows[0]), fixed_rows)

    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rom-dir",
        type=Path,
        default=Path("roms"),
        help="directory containing Oracle .gb/.gbc files",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("analysis/generated"),
        help="directory for reproducible JSON and CSV reports",
    )
    args = parser.parse_args()
    rom_paths = sorted(
        [
            *args.rom_dir.glob("*.gb"),
            *args.rom_dir.glob("*.gbc"),
        ]
    )
    try:
        if len(rom_paths) < 2:
            raise ValueError(
                f"expected at least two .gb/.gbc files, found {len(rom_paths)}"
            )

        identities: dict[tuple[str, str], Path] = {}
        manifest_roms = []
        for path in rom_paths:
            data = path.read_bytes()
            header = parse_header(data)
            region = region_from_game_code(header.game_code)
            identity = (header.detected_game, region)
            if identity in identities:
                raise ValueError(
                    f"duplicate ROM identity {identity}: "
                    f"{identities[identity].name} and {path.name}"
                )
            identities[identity] = path
            md5 = hashlib.md5(data).hexdigest()
            manifest_roms.append(
                {
                    "filename": path.name,
                    "game": header.detected_game,
                    "region": region,
                    "game_code": header.game_code,
                    "bytes": len(data),
                    "banks": len(data) // BANK_SIZE,
                    "md5": md5,
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "header_checksum_valid": header.header_checksum_valid,
                    "global_checksum_valid": header.global_checksum_valid,
                    "nintendo_logo_valid": header.nintendo_logo_valid,
                    "matches_oracles_disasm": (
                        md5 == ORACLES_DISASM_MD5.get(identity)
                    ),
                    "filename_identity_matches": (
                        header.detected_game in path.stem.lower()
                    ),
                }
            )

        pair_specs = [
            (
                "us-games",
                ("seasons", "usa"),
                ("ages", "usa"),
                "Primary shared-engine comparison",
            ),
            (
                "europe-games",
                ("seasons", "europe"),
                ("ages", "europe"),
                "European cross-game comparison",
            ),
            (
                "ages-us-to-europe",
                ("ages", "usa"),
                ("ages", "europe"),
                "Ages localization/version alignment",
            ),
            (
                "seasons-us-to-europe",
                ("seasons", "usa"),
                ("seasons", "europe"),
                "Seasons localization/version alignment",
            ),
        ]
        comparison_reports = {}
        for name, left_identity, right_identity, purpose in pair_specs:
            if left_identity not in identities or right_identity not in identities:
                continue
            report = analyze(
                [identities[left_identity], identities[right_identity]],
                args.output_dir / "comparisons" / name,
            )
            comparison_reports[name] = {
                "purpose": purpose,
                "report_directory": f"comparisons/{name}",
                **report["comparison"],
            }

        if not comparison_reports:
            report = analyze(rom_paths[:2], args.output_dir)
            comparison_reports["provided-pair"] = {
                "purpose": "Comparison of the two provided ROMs",
                "report_directory": ".",
                **report["comparison"],
            }

        manifest = {
            "schema_version": 1,
            "bank_size": BANK_SIZE,
            "oracles_disasm_reference": {
                "repository": "https://github.com/Stewmath/oracles-disasm",
                "ages_md5_manifest": "https://github.com/Stewmath/oracles-disasm/blob/master/ages.md5",
                "seasons_md5_manifest": "https://github.com/Stewmath/oracles-disasm/blob/master/seasons.md5",
                "supported_region": "usa",
            },
            "roms": manifest_roms,
            "comparisons": comparison_reports,
            "primary_comparison": (
                "us-games"
                if "us-games" in comparison_reports
                else next(iter(comparison_reports))
            ),
        }
        args.output_dir.mkdir(parents=True, exist_ok=True)
        (args.output_dir / "rom-manifest.json").write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        json.dumps(
            {
                "primary_comparison": manifest["primary_comparison"],
                "roms": manifest["roms"],
                "comparisons": {
                    name: {
                        "same_offset_percent": comparison["same_offset_percent"],
                        "candidate_shared_region_unique_left_percent": comparison[
                            "candidate_shared_region_unique_left_percent"
                        ],
                        "candidate_shared_region_unique_left_percent_of_populated": comparison[
                            "candidate_shared_region_unique_left_percent_of_populated"
                        ],
                    }
                    for name, comparison in manifest["comparisons"].items()
                },
            },
            indent=2,
        )
    )
    mismatches = [
        rom for rom in manifest["roms"] if not rom["filename_identity_matches"]
    ]
    if mismatches:
        print("WARNING: filename/header identity mismatches detected:")
        for mismatch in mismatches:
            print(f"  {mismatch['filename']} -> {mismatch['game']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
