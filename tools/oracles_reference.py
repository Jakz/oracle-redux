#!/usr/bin/env python3
"""Import oracles-disasm symbols and build conservative, bank-aware maps.

The reference project supplies names and linker sections. Control-flow edges
are decoded from the user's verified US retail ROMs, not from the reconstructed
ROM output. Routine entry seeds are labels used as direct call/jump operands in
the reference assembly. This avoids treating every data label as executable
code while still covering ordinary and callab/jpab-dispatched routines.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import subprocess
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from rom_catalog import (
    BANK_SIZE,
    CALLS,
    INVALID_OPCODES,
    JP_CONDITIONAL,
    JP_UNCONDITIONAL,
    JR_CONDITIONAL,
    JR_UNCONDITIONAL,
    ORACLES_DISASM_MD5,
    RSTS,
    TERMINATORS,
    opcode_length,
    parse_header,
    region_from_game_code,
)


ROM_BANKS = 0x40
CONDITIONAL_RETURNS = {0xC0, 0xC8, 0xD0, 0xD8}
HARD_TERMINATORS = TERMINATORS | {0x10, 0x76}
KNOWN_RST_NAMES = {
    0x00: "rst_jumpTable",
    0x10: "rst_addAToHl",
    0x18: "rst_addDoubleIndex",
    0x38: "rst_unused_38",
}
SOURCE_CONTROL_FLOW_RE = re.compile(
    r"^\s*(callab|jpab|call|jp|jr)\s+(.+?)\s*$", re.IGNORECASE
)
SYMBOL_RE = re.compile(r"^([0-9a-fA-F]{2}):([0-9a-fA-F]{4})\s+(.+?)\s*$")
SECTION_RE = re.compile(
    r"^([0-9a-fA-F]{8})\s+"
    r"([0-9a-fA-F]{2}):([0-9a-fA-F]{4})\s+"
    r"([0-9a-fA-F]{4})\s+([0-9a-fA-F]{8})\s+(.+?)\s*$"
)


@dataclass(frozen=True)
class Symbol:
    bank: int
    address: int
    name: str

    @property
    def location(self) -> str:
        return f"{self.bank:02X}:{self.address:04X}"

    @property
    def file_offset(self) -> int | None:
        return rom_file_offset(self.bank, self.address)


@dataclass(frozen=True)
class Section:
    file_offset: int
    bank: int
    address: int
    size: int
    name: str
    kind: str

    @property
    def end_offset(self) -> int:
        return self.file_offset + self.size

    @property
    def location(self) -> str:
        return f"{self.bank:02X}:{self.address:04X}"


@dataclass
class SymbolFile:
    labels: list[Symbol]
    sections: list[Section]

    def __post_init__(self) -> None:
        by_address: dict[tuple[int, int], list[Symbol]] = defaultdict(list)
        by_name: dict[str, list[Symbol]] = defaultdict(list)
        for symbol in self.labels:
            by_address[(symbol.bank, symbol.address)].append(symbol)
            by_name[symbol.name].append(symbol)
        self.by_address = dict(by_address)
        self.by_name = dict(by_name)
        self.canonical_by_address = {
            address: min(symbols, key=canonical_symbol_rank)
            for address, symbols in self.by_address.items()
        }
        self.sections_by_offset = sorted(
            self.sections, key=lambda section: section.file_offset
        )

    def section_for(self, symbol: Symbol) -> Section | None:
        offset = symbol.file_offset
        if offset is None:
            return None
        matches = [
            section
            for section in self.sections_by_offset
            if section.file_offset <= offset < section.end_offset
        ]
        return min(matches, key=lambda section: section.size) if matches else None


def rom_file_offset(bank: int, address: int) -> int | None:
    if bank == 0 and 0 <= address < 0x4000:
        return address
    if 0 < bank < ROM_BANKS and 0x4000 <= address < 0x8000:
        return bank * BANK_SIZE + address - 0x4000
    return None


def offset_location(offset: int) -> tuple[int, int]:
    bank = offset // BANK_SIZE
    within = offset % BANK_SIZE
    return bank, within if bank == 0 else 0x4000 + within


def is_rom_symbol(symbol: Symbol) -> bool:
    return symbol.file_offset is not None


def canonical_symbol_rank(symbol: Symbol) -> tuple[int, int, int, str]:
    name = symbol.name
    local = "@" in name
    generated = (
        name.startswith("_label_")
        or name.startswith("RAM_USAGE_")
        or name.startswith("__")
    )
    address_named = bool(re.search(r"(?:^|_)[0-9a-fA-F]{4,6}$", name))
    return (local + generated, address_named, len(name), name.casefold())


def infer_section_kind(name: str) -> str:
    lowered = name.casefold()
    if "audio" in lowered:
        return "code" if "code" in lowered else "audio"
    if any(
        token in lowered
        for token in (
            "gfx",
            "graphic",
            "tile_mapping",
            "tile mappings",
            "objects",
            "animation",
            "palette",
            "oam",
        )
    ):
        return "graphics-or-object-data"
    if any(
        token in lowered
        for token in (
            "code",
            "file_management",
            "serialcode",
            "terrain_effect",
        )
    ):
        return "code"
    if any(
        token in lowered
        for token in (
            "scripts",
            "data",
            "layout",
            "mapping",
            "collision",
            "pointer",
        )
    ):
        return "structured-data"
    if re.fullmatch(r"bank(?:_|)[0-9a-f]+", lowered):
        return "mixed"
    return "unknown"


def parse_symbol_file(path: Path) -> SymbolFile:
    section_name = ""
    labels: list[Symbol] = []
    sections: list[Section] = []
    with path.open(encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line.startswith("[") and line.endswith("]"):
                section_name = line
                continue
            if not line or line.startswith(";"):
                continue
            if section_name == "[labels]":
                match = SYMBOL_RE.match(line)
                if match:
                    labels.append(
                        Symbol(
                            bank=int(match.group(1), 16),
                            address=int(match.group(2), 16),
                            name=match.group(3),
                        )
                    )
            elif section_name == "[sections]":
                match = SECTION_RE.match(line)
                if match:
                    name = match.group(6)
                    sections.append(
                        Section(
                            file_offset=int(match.group(1), 16),
                            bank=int(match.group(2), 16),
                            address=int(match.group(4), 16),
                            size=int(match.group(5), 16),
                            name=name,
                            kind=infer_section_kind(name),
                        )
                    )
    return SymbolFile(
        labels=[symbol for symbol in labels if is_rom_symbol(symbol)],
        sections=sections,
    )


def source_files_for_game(reference_dir: Path, game: str) -> Iterable[Path]:
    other_game = "seasons" if game == "ages" else "ages"
    for path in sorted(reference_dir.rglob("*.s")):
        relative_parts = {part.casefold() for part in path.relative_to(reference_dir).parts}
        if other_game in relative_parts:
            continue
        if any(
            part in {"build_ages_v", "build_seasons_v", "precompressed"}
            for part in relative_parts
        ):
            continue
        yield path


def normalize_operand(operand_text: str) -> str | None:
    operands = [part.strip() for part in operand_text.split(",")]
    if not operands:
        return None
    if operands[0].casefold() in {"z", "nz", "c", "nc"}:
        operands = operands[1:]
    target = operands[-1].split()[0].strip("()")
    if (
        not target
        or target.startswith(("@", "\\", "$", "%"))
        or target.casefold() in {"hl", "bc", "de", "sp"}
        or target[0].isdigit()
        or any(character in target for character in "+-*/[]{}")
    ):
        return None
    return target


def extract_routine_references(reference_dir: Path, game: str) -> set[str]:
    references = {
        "begin",
        "resetGame",
        "vblankInterrupt",
        "lcdInterrupt",
        "timerInterrupt",
        "serialInterrupt",
    }
    for path in source_files_for_game(reference_dir, game):
        with path.open(encoding="utf-8", errors="replace") as handle:
            for raw_line in handle:
                code = raw_line.split(";", 1)[0]
                match = SOURCE_CONTROL_FLOW_RE.match(code)
                if not match:
                    continue
                target = normalize_operand(match.group(2))
                if target:
                    references.add(target)
    return references


def resolve_seed_symbols(
    symbols: SymbolFile, referenced_names: set[str]
) -> dict[tuple[int, int], Symbol]:
    seeds: dict[tuple[int, int], Symbol] = {}
    for name in sorted(referenced_names):
        candidates = symbols.by_name.get(name, [])
        for candidate in candidates:
            address = (candidate.bank, candidate.address)
            seeds[address] = symbols.canonical_by_address[address]
    for vector in (0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030,
                   0x0038, 0x0040, 0x0048, 0x0050, 0x0058, 0x0060, 0x0100):
        symbol = symbols.canonical_by_address.get((0, vector))
        if symbol:
            seeds[(0, vector)] = symbol
    return seeds


def target_location(source_bank: int, address: int) -> tuple[int, int] | None:
    if 0 <= address < 0x4000:
        return 0, address
    if source_bank > 0 and 0x4000 <= address < 0x8000:
        return source_bank, address
    return None


def format_target(
    symbols: SymbolFile, location: tuple[int, int] | None, raw_address: int
) -> tuple[str, str, str, bool]:
    if location is None:
        return "", f"{raw_address:04X}", f"${raw_address:04X}", False
    bank, address = location
    symbol = symbols.canonical_by_address.get(location)
    return (
        f"{bank:02X}",
        f"{address:04X}",
        symbol.name if symbol else f"{bank:02X}:{address:04X}",
        symbol is not None,
    )


def edge_row(
    game: str,
    caller: Symbol,
    instruction_bank: int,
    instruction_address: int,
    kind: str,
    symbols: SymbolFile,
    location: tuple[int, int] | None,
    raw_address: int,
    confidence: str,
    note: str = "",
) -> dict:
    target_bank, target_address, target_name, resolved = format_target(
        symbols, location, raw_address
    )
    return {
        "game": game,
        "caller": caller.name,
        "caller_location": caller.location,
        "instruction_location": f"{instruction_bank:02X}:{instruction_address:04X}",
        "kind": kind,
        "target": target_name,
        "target_bank": target_bank,
        "target_address": target_address,
        "resolved_symbol": resolved,
        "confidence": confidence,
        "note": note,
    }


def decode_call_graph(
    game: str,
    rom: bytes,
    symbols: SymbolFile,
    seeds: dict[tuple[int, int], Symbol],
) -> list[dict]:
    """Decode intra-routine flow from source-referenced entry labels.

    Direct calls/jumps are bank-resolved when the LR35902 mapping makes the
    target unambiguous. callab/jpab are recognized from their emitted
    ``ld hl,target; ld e,bank; call/jp interBankCall`` byte sequence.
    """
    rows: list[dict] = []
    row_keys: set[tuple] = set()
    seed_addresses = set(seeds)

    def add(row: dict) -> None:
        key = tuple(row[field] for field in (
            "caller_location", "instruction_location", "kind",
            "target_bank", "target_address"
        ))
        if key not in row_keys:
            row_keys.add(key)
            rows.append(row)

    for start, caller in sorted(seeds.items()):
        bank, start_address = start
        queue = deque([start_address])
        visited: set[int] = set()
        decoded_count = 0
        while queue and decoded_count < 4096:
            pc = queue.popleft()
            while decoded_count < 4096:
                if (bank, pc) in seed_addresses and pc != start_address:
                    break
                offset = rom_file_offset(bank, pc)
                if offset is None or offset >= len(rom) or pc in visited:
                    break
                visited.add(pc)
                decoded_count += 1
                opcode = rom[offset]
                if opcode in INVALID_OPCODES:
                    break
                length = opcode_length(opcode)
                if offset + length > len(rom):
                    break
                next_pc = pc + length

                if opcode in JP_UNCONDITIONAL | JP_CONDITIONAL | CALLS:
                    raw_target = int.from_bytes(rom[offset + 1 : offset + 3], "little")
                    location = target_location(bank, raw_target)
                    is_call = opcode in CALLS
                    kind = (
                        "call"
                        if opcode == 0xCD
                        else "conditional-call"
                        if is_call
                        else "tail-jump"
                        if opcode in JP_UNCONDITIONAL
                        else "conditional-jump"
                    )
                    add(
                        edge_row(
                            game, caller, bank, pc, kind, symbols, location,
                            raw_target, "corroborated"
                        )
                    )

                    target_symbol = (
                        symbols.canonical_by_address.get(location)
                        if location is not None else None
                    )
                    if (
                        target_symbol is not None
                        and target_symbol.name == "interBankCall"
                        and offset >= 5
                        and rom[offset - 5] == 0x21
                        and rom[offset - 2] == 0x1E
                    ):
                        far_address = int.from_bytes(rom[offset - 4 : offset - 2], "little")
                        far_bank = rom[offset - 1]
                        far_location = (
                            (far_bank, far_address)
                            if rom_file_offset(far_bank, far_address) is not None
                            else None
                        )
                        add(
                            edge_row(
                                game, caller, bank, pc,
                                "far-call" if is_call else "far-tail-jump",
                                symbols, far_location, far_address,
                                "verified-pattern",
                                "decoded callab/jpab trampoline sequence",
                            )
                        )

                    if not is_call and location is not None:
                        if location in seed_addresses:
                            break
                        if location[0] == bank:
                            queue.append(location[1])
                    if opcode in JP_UNCONDITIONAL:
                        break
                elif opcode in JR_UNCONDITIONAL | JR_CONDITIONAL:
                    displacement = int.from_bytes(
                        rom[offset + 1 : offset + 2], "little", signed=True
                    )
                    target = (next_pc + displacement) & 0xFFFF
                    location = (bank, target)
                    if location in seed_addresses:
                        target_symbol = seeds[location]
                        add(
                            edge_row(
                                game, caller, bank, pc,
                                "relative-tail-jump"
                                if opcode in JR_UNCONDITIONAL
                                else "relative-conditional-jump",
                                symbols, location, target, "corroborated",
                            )
                        )
                    else:
                        queue.append(target)
                    if opcode in JR_UNCONDITIONAL:
                        break
                elif opcode in RSTS:
                    raw_target = opcode & 0x38
                    row = edge_row(
                        game, caller, bank, pc, "rst", symbols,
                        (0, raw_target), raw_target, "verified",
                    )
                    if raw_target in KNOWN_RST_NAMES and not row["resolved_symbol"]:
                        row["target"] = KNOWN_RST_NAMES[raw_target]
                        row["resolved_symbol"] = True
                        row["note"] = "named hardware RST vector"
                    add(row)
                elif opcode in HARD_TERMINATORS:
                    break
                elif opcode in CONDITIONAL_RETURNS:
                    pass
                pc = next_pc

    return sorted(
        rows,
        key=lambda row: (
            row["caller_location"],
            row["instruction_location"],
            row["kind"],
            row["target_bank"],
            row["target_address"],
        ),
    )


def exact_prefix(left: bytes, left_offset: int, right: bytes, right_offset: int,
                 cap: int = 256) -> int:
    limit = min(cap, len(left) - left_offset, len(right) - right_offset)
    length = 0
    while length < limit and left[left_offset + length] == right[right_offset + length]:
        length += 1
    return length


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def md5(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()


def find_verified_us_roms(rom_dir: Path) -> dict[str, tuple[Path, bytes]]:
    found: dict[str, tuple[Path, bytes]] = {}
    for path in sorted((*rom_dir.glob("*.gb"), *rom_dir.glob("*.gbc"))):
        data = path.read_bytes()
        header = parse_header(data)
        game = header.detected_game
        if region_from_game_code(header.game_code) != "usa":
            continue
        expected_md5 = ORACLES_DISASM_MD5.get((game, "usa"))
        if expected_md5 and md5(data) == expected_md5:
            found[game] = (path, data)
    missing = {"ages", "seasons"} - found.keys()
    if missing:
        raise ValueError(
            "missing exact US reference ROM(s): " + ", ".join(sorted(missing))
        )
    return found


def differing_runs(retail: bytes, rebuilt: bytes) -> list[dict]:
    rows: list[dict] = []
    limit = min(len(retail), len(rebuilt))
    start: int | None = None
    for offset in range(limit):
        different = retail[offset] != rebuilt[offset]
        if different and start is None:
            start = offset
        elif not different and start is not None:
            bank, address = offset_location(start)
            rows.append({
                "file_offset": f"{start:06X}",
                "location": f"{bank:02X}:{address:04X}",
                "length": offset - start,
                "retail_byte": f"{retail[start]:02X}",
                "rebuilt_byte": f"{rebuilt[start]:02X}",
            })
            start = None
    if start is not None:
        bank, address = offset_location(start)
        rows.append({
            "file_offset": f"{start:06X}",
            "location": f"{bank:02X}:{address:04X}",
            "length": limit - start,
            "retail_byte": f"{retail[start]:02X}",
            "rebuilt_byte": f"{rebuilt[start]:02X}",
        })
    if len(retail) != len(rebuilt):
        rows.append({
            "file_offset": f"{limit:06X}",
            "location": "",
            "length": abs(len(retail) - len(rebuilt)),
            "retail_byte": "<eof>" if len(retail) < len(rebuilt) else "",
            "rebuilt_byte": "<eof>" if len(rebuilt) < len(retail) else "",
        })
    return rows


def write_csv(path: Path, fieldnames: list[str], rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def reference_commit(reference_dir: Path) -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(reference_dir), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        pass

    # Sandboxed Windows accounts can trigger Git's dubious-ownership check
    # even for a checkout we just cloned. Resolve a simple local HEAD without
    # modifying global safe.directory configuration.
    git_dir = reference_dir / ".git"
    try:
        head = (git_dir / "HEAD").read_text(encoding="ascii").strip()
        if not head.startswith("ref: "):
            return head
        ref_name = head.removeprefix("ref: ")
        loose_ref = git_dir / Path(ref_name)
        if loose_ref.is_file():
            return loose_ref.read_text(encoding="ascii").strip()
        for line in (git_dir / "packed-refs").read_text(
            encoding="ascii"
        ).splitlines():
            if line and not line.startswith(("#", "^")):
                commit, name = line.split(" ", 1)
                if name == ref_name:
                    return commit
    except (OSError, ValueError):
        pass
    return "unknown"


def generate(reference_dir: Path, rom_dir: Path, output_dir: Path) -> dict:
    roms = find_verified_us_roms(rom_dir)
    symbol_files = {
        game: parse_symbol_file(reference_dir / f"{game}.sym")
        for game in ("ages", "seasons")
    }
    references = {
        game: extract_routine_references(reference_dir, game)
        for game in ("ages", "seasons")
    }
    seeds = {
        game: resolve_seed_symbols(symbol_files[game], references[game])
        for game in ("ages", "seasons")
    }
    graphs = {
        game: decode_call_graph(
            game, roms[game][1], symbol_files[game], seeds[game]
        )
        for game in ("ages", "seasons")
    }

    symbol_fields = [
        "game", "name", "bank", "address", "location", "file_offset",
        "canonical_at_address", "section", "section_kind",
    ]
    section_fields = [
        "game", "name", "kind", "bank", "address", "location",
        "file_offset", "size",
    ]
    for game in ("ages", "seasons"):
        parsed = symbol_files[game]
        symbol_rows = []
        for symbol in sorted(
            parsed.labels, key=lambda item: (item.bank, item.address, item.name)
        ):
            section = parsed.section_for(symbol)
            symbol_rows.append({
                "game": game,
                "name": symbol.name,
                "bank": f"{symbol.bank:02X}",
                "address": f"{symbol.address:04X}",
                "location": symbol.location,
                "file_offset": f"{symbol.file_offset:06X}",
                "canonical_at_address":
                    parsed.canonical_by_address[(symbol.bank, symbol.address)].name,
                "section": section.name if section else "",
                "section_kind": section.kind if section else "unknown",
            })
        write_csv(output_dir / f"{game}-symbols.csv", symbol_fields, symbol_rows)

        section_rows = [{
            "game": game,
            "name": section.name,
            "kind": section.kind,
            "bank": f"{section.bank:02X}",
            "address": f"{section.address:04X}",
            "location": section.location,
            "file_offset": f"{section.file_offset:06X}",
            "size": section.size,
        } for section in sorted(parsed.sections, key=lambda item: item.file_offset)]
        write_csv(output_dir / f"{game}-sections.csv", section_fields, section_rows)

        write_csv(
            output_dir / f"{game}-call-graph.csv",
            [
                "game", "caller", "caller_location", "instruction_location",
                "kind", "target", "target_bank", "target_address",
                "resolved_symbol", "confidence", "note",
            ],
            graphs[game],
        )

    ages_by_name = {
        name: min(items, key=canonical_symbol_rank)
        for name, items in symbol_files["ages"].by_name.items()
    }
    seasons_by_name = {
        name: min(items, key=canonical_symbol_rank)
        for name, items in symbol_files["seasons"].by_name.items()
    }
    common_names = sorted(ages_by_name.keys() & seasons_by_name.keys())
    resolved_routine_names = {
        game: references[game] & symbol_files[game].by_name.keys()
        for game in ("ages", "seasons")
    }
    common_seed_names = (
        resolved_routine_names["ages"] & resolved_routine_names["seasons"]
    )
    shared_rows = []
    for name in common_names:
        ages_symbol = ages_by_name[name]
        seasons_symbol = seasons_by_name[name]
        ages_offset = ages_symbol.file_offset
        seasons_offset = seasons_symbol.file_offset
        assert ages_offset is not None and seasons_offset is not None
        ages_section = symbol_files["ages"].section_for(ages_symbol)
        seasons_section = symbol_files["seasons"].section_for(seasons_symbol)
        prefix = exact_prefix(
            roms["ages"][1], ages_offset,
            roms["seasons"][1], seasons_offset,
        )
        shared_rows.append({
            "name": name,
            "routine_seed_in_both": name in common_seed_names,
            "ages_location": ages_symbol.location,
            "seasons_location": seasons_symbol.location,
            "same_location": ages_symbol.location == seasons_symbol.location,
            "exact_prefix_bytes_capped_256": prefix,
            "same_first_8_bytes": prefix >= 8,
            "same_first_16_bytes": prefix >= 16,
            "ages_section": ages_section.name if ages_section else "",
            "seasons_section": seasons_section.name if seasons_section else "",
            "ages_section_kind": ages_section.kind if ages_section else "unknown",
            "seasons_section_kind":
                seasons_section.kind if seasons_section else "unknown",
        })
    write_csv(
        output_dir / "shared-symbols.csv",
        [
            "name", "routine_seed_in_both", "ages_location",
            "seasons_location", "same_location",
            "exact_prefix_bytes_capped_256", "same_first_8_bytes",
            "same_first_16_bytes", "ages_section", "seasons_section",
            "ages_section_kind", "seasons_section_kind",
        ],
        shared_rows,
    )

    semantic_edge_sets = {}
    semantic_edge_counts = {}
    for game in ("ages", "seasons"):
        counts: dict[tuple[str, str, str], int] = defaultdict(int)
        for row in graphs[game]:
            if row["resolved_symbol"]:
                counts[(row["caller"], row["kind"], row["target"])] += 1
        semantic_edge_counts[game] = counts
        semantic_edge_sets[game] = set(counts)
    shared_semantic_edges = sorted(
        semantic_edge_sets["ages"] & semantic_edge_sets["seasons"]
    )
    write_csv(
        output_dir / "shared-call-edges.csv",
        ["caller", "kind", "target", "ages_occurrences", "seasons_occurrences"],
        ({
            "caller": caller,
            "kind": kind,
            "target": target,
            "ages_occurrences":
                semantic_edge_counts["ages"][(caller, kind, target)],
            "seasons_occurrences":
                semantic_edge_counts["seasons"][(caller, kind, target)],
        } for caller, kind, target in shared_semantic_edges),
    )

    build_info = {}
    diff_fields = [
        "file_offset", "location", "length", "retail_byte", "rebuilt_byte"
    ]
    for game in ("ages", "seasons"):
        rebuilt_path = reference_dir / f"{game}.gbc"
        rebuilt = rebuilt_path.read_bytes()
        differences = differing_runs(roms[game][1], rebuilt)
        write_csv(
            output_dir / f"{game}-rebuild-differences.csv",
            diff_fields,
            differences,
        )
        build_info[game] = {
            "retail_rom": str(roms[game][0]),
            "retail_md5": md5(roms[game][1]),
            "retail_sha256": sha256(roms[game][1]),
            "rebuilt_rom": str(rebuilt_path),
            "rebuilt_md5": md5(rebuilt),
            "rebuilt_sha256": sha256(rebuilt),
            "byte_exact": rebuilt == roms[game][1],
            "differing_bytes": sum(
                left != right for left, right in zip(roms[game][1], rebuilt)
            ) + abs(len(roms[game][1]) - len(rebuilt)),
            "difference_runs": len(differences),
        }

    summary = {
        "schema_version": 1,
        "reference": {
            "repository": "https://github.com/Stewmath/oracles-disasm",
            "commit": reference_commit(reference_dir),
        },
        "builds": build_info,
        "symbols": {
            game: {
                "rom_labels": len(symbol_files[game].labels),
                "unique_rom_names": len(symbol_files[game].by_name),
                "unique_rom_addresses":
                    len(symbol_files[game].canonical_by_address),
                "sections": len(symbol_files[game].sections),
                "source_referenced_names": len(references[game]),
                "resolved_routine_names": len(resolved_routine_names[game]),
                "resolved_routine_entries": len(seeds[game]),
            }
            for game in ("ages", "seasons")
        },
        "shared": {
            "symbol_names": len(common_names),
            "routine_seed_names": len(common_seed_names),
            "routine_seed_names_same_first_8_bytes": sum(
                row["routine_seed_in_both"] and row["same_first_8_bytes"]
                for row in shared_rows
            ),
            "routine_seed_names_same_first_16_bytes": sum(
                row["routine_seed_in_both"] and row["same_first_16_bytes"]
                for row in shared_rows
            ),
            "symbol_names_same_location": sum(
                row["same_location"] for row in shared_rows
            ),
            "routine_name_coverage_percent": {
                game: round(
                    100 * len(common_seed_names)
                    / len(resolved_routine_names[game]),
                    3,
                )
                for game in ("ages", "seasons")
            },
            "routine_name_jaccard_percent": round(
                100 * len(common_seed_names)
                / len(
                    resolved_routine_names["ages"]
                    | resolved_routine_names["seasons"]
                ),
                3,
            ),
            "semantic_call_edges": len(shared_semantic_edges),
            "semantic_call_edge_coverage_percent": {
                game: round(
                    100 * len(shared_semantic_edges)
                    / len(semantic_edge_sets[game]),
                    3,
                )
                for game in ("ages", "seasons")
            },
            "semantic_call_edge_jaccard_percent": round(
                100 * len(shared_semantic_edges)
                / len(
                    semantic_edge_sets["ages"] | semantic_edge_sets["seasons"]
                ),
                3,
            ),
        },
        "call_graph": {
            game: {
                "edges": len(graphs[game]),
                "resolved_edges": sum(row["resolved_symbol"] for row in graphs[game]),
                "far_edges": sum(
                    row["kind"] in {"far-call", "far-tail-jump"}
                    for row in graphs[game]
                ),
                "callers": len({row["caller_location"] for row in graphs[game]}),
            }
            for game in ("ages", "seasons")
        },
        "limitations": [
            "Routine seeds come from direct call/jump operands in reference assembly.",
            "Local-label-only and fully data-driven dispatch are not complete yet.",
            "A banked target from bank 00 is unresolved unless a callab/jpab byte pattern identifies its bank.",
            "Symbol-name overlap includes code and data; routine_seed_names is the safer code-oriented measure.",
            "Exact prefix comparisons are capped at 256 bytes and are not function-size measurements.",
        ],
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("--rom-dir", type=Path, default=Path("roms"))
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("analysis/generated/reference"),
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    summary = generate(args.reference_dir, args.rom_dir, args.output_dir)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
