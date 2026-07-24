#!/usr/bin/env python3
"""Catalog source ownership and relocation-normalized routine structure.

This is a second-stage analysis over ``oracles_reference.py``. It uses the same
source-referenced routine seeds and verified US retail ROMs, then emits:

* per-game routine fingerprints;
* shared-routine classifications;
* subsystem-level overlap summaries.

Three comparisons are kept distinct:

* raw instruction bytes;
* normalized operands, where resolved addresses become symbol names;
* opcode shape, which ignores ordinary immediates and is only a weak signal.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import oracles_reference as reference
from rom_catalog import (
    CALLS,
    INVALID_OPCODES,
    JP_CONDITIONAL,
    JP_UNCONDITIONAL,
    JR_CONDITIONAL,
    JR_UNCONDITIONAL,
    RSTS,
    opcode_length,
)


LABEL_DECLARATION_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_.@]*)\s*:(?!:)"
)
CONDITIONAL_RETURNS = {0xC0, 0xC8, 0xD0, 0xD8}
UNCONDITIONAL_RETURNS = {0xC9, 0xD9}
INDIRECT_JUMPS = {0xE9}
HARD_STOPS = {0x10, 0x76}
MAX_ROUTINE_INSTRUCTIONS = 4096


@dataclass(frozen=True)
class SourceDeclaration:
    name: str
    path: str
    line: int
    ownership: str
    subsystem: str


@dataclass(frozen=True)
class SourceMatch:
    path: str = ""
    line: int = 0
    ownership: str = "unknown"
    subsystem: str = "unknown"
    confidence: str = "unmapped"


@dataclass(frozen=True)
class RoutineFingerprint:
    name: str
    bank: int
    address: int
    instruction_count: int
    decoded_bytes: int
    block_entries: int
    complete: bool
    invalid_paths: int
    truncated: bool
    raw_hash: str
    normalized_hash: str
    shape_hash: str

    @property
    def location(self) -> str:
        return f"{self.bank:02X}:{self.address:04X}"


def source_ownership(path: Path, game: str) -> str:
    parts = {part.casefold() for part in path.parts}
    return "campaign" if game in parts else "common"


def source_subsystem(path: Path, game: str) -> str:
    parts = [part.casefold() for part in path.parts]
    if not parts:
        return "unknown"
    root = parts[0]
    tail = [part for part in parts[1:] if part != game]
    stem = Path(tail[-1]).stem if tail else Path(parts[-1]).stem

    if root == "object_code":
        categories = {
            "interactions": "interactions",
            "enemies": "enemies",
            "parts": "parts",
            "items": "items",
            "itemparents": "item-parents",
            "specialobjects": "special-objects",
        }
        category = next(
            (label for part, label in categories.items() if part in tail),
            "other",
        )
        return f"objects/{category}"
    if root == "code":
        lowered_stem = stem.casefold()
        if lowered_stem == "audio":
            return "code/audio"
        if re.fullmatch(r"(?:bank[0-9a-f]+|code_[0-9a-f]+)", lowered_stem):
            return "code/core-banks"
        if any(token in lowered_stem for token in ("cutscene", "credits", "intro")):
            return "code/cutscenes"
        if any(token in lowered_stem for token in ("room", "warp", "tileset", "tile")):
            return "code/rooms-tiles"
        if any(token in lowered_stem for token in ("item", "treasure", "inventory")):
            return "code/items-inventory"
        if any(token in lowered_stem for token in ("file", "save", "secret")):
            return "code/save-link"
        if any(token in lowered_stem for token in ("object", "interaction", "enemy")):
            return "code/object-framework"
        return "code/gameplay-other"
    if root in {"scripts", "audio", "data", "objects"}:
        return root
    return root


def extract_source_declarations(
    reference_dir: Path, game: str
) -> dict[str, list[SourceDeclaration]]:
    declarations: dict[str, list[SourceDeclaration]] = defaultdict(list)
    for path in reference.source_files_for_game(reference_dir, game):
        relative = path.relative_to(reference_dir)
        with path.open(encoding="utf-8", errors="replace") as handle:
            for line_number, raw_line in enumerate(handle, 1):
                code = raw_line.split(";", 1)[0]
                match = LABEL_DECLARATION_RE.match(code)
                if not match or match.group(1).startswith("@"):
                    continue
                name = match.group(1)
                declarations[name].append(
                    SourceDeclaration(
                        name=name,
                        path=relative.as_posix(),
                        line=line_number,
                        ownership=source_ownership(relative, game),
                        subsystem=source_subsystem(relative, game),
                    )
                )
    return dict(declarations)


def source_name_candidates(symbol_name: str) -> list[tuple[str, str]]:
    candidates = [(symbol_name, "exact")]
    global_name = symbol_name.split("@", 1)[0]
    if global_name != symbol_name:
        candidates.append((global_name, "scoped-local-parent"))
    if "." in global_name:
        candidates.append((global_name.rsplit(".", 1)[-1], "namespace-suffix"))
    seen = set()
    return [
        (name, confidence)
        for name, confidence in candidates
        if name and not (name in seen or seen.add(name))
    ]


def match_source_declaration(
    symbol_name: str,
    declarations: dict[str, list[SourceDeclaration]],
    symbol: reference.Symbol | None = None,
) -> SourceMatch:
    for candidate, confidence in source_name_candidates(symbol_name):
        matches = declarations.get(candidate, [])
        if not matches:
            continue
        expected_bank_file = (
            f"code/bank{symbol.bank:x}.s" if symbol is not None else ""
        )

        def declaration_rank(item: SourceDeclaration) -> tuple:
            bank_match = item.path.casefold() == expected_bank_file
            audio_match = (
                symbol is not None
                and symbol.bank == 0x39
                and item.path.casefold() == "code/audio.s"
            )
            return (
                not (bank_match or audio_match),
                item.path,
                item.line,
                item.name,
            )

        ordered = sorted(set(matches), key=declaration_rank)
        chosen = ordered[0]
        suffix = "-ambiguous" if len(ordered) > 1 else ""
        return SourceMatch(
            path=chosen.path,
            line=chosen.line,
            ownership=chosen.ownership,
            subsystem=chosen.subsystem,
            confidence=confidence + suffix,
        )
    return SourceMatch()


def parse_memory_symbols(path: Path) -> dict[int, reference.Symbol]:
    section = ""
    candidates: dict[int, list[reference.Symbol]] = defaultdict(list)
    with path.open(encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if line.startswith("[") and line.endswith("]"):
                section = line
                continue
            if section != "[labels]":
                continue
            match = reference.SYMBOL_RE.match(line)
            if not match:
                continue
            symbol = reference.Symbol(
                bank=int(match.group(1), 16),
                address=int(match.group(2), 16),
                name=match.group(3),
            )
            if symbol.address >= 0x8000:
                candidates[symbol.address].append(symbol)

    def rank(symbol: reference.Symbol) -> tuple:
        return (
            symbol.bank != 0,
            reference.canonical_symbol_rank(symbol),
            symbol.bank,
        )

    return {
        address: min(symbols, key=rank)
        for address, symbols in candidates.items()
    }


def symbolic_operand(
    symbols: reference.SymbolFile,
    memory_symbols: dict[int, reference.Symbol],
    source_bank: int,
    value: int,
) -> str:
    if value >= 0x8000:
        symbol = memory_symbols.get(value)
        return f"mem:{symbol.name}" if symbol else f"mem:${value:04X}"
    location = reference.target_location(source_bank, value)
    if location is not None:
        symbol = symbols.canonical_by_address.get(location)
        if symbol:
            return f"rom:{symbol.name}"
        return f"rom:{location[0]:02X}:{location[1]:04X}"
    return f"unresolved:${value:04X}"


def instruction_tokens(
    rom: bytes,
    offset: int,
    bank: int,
    symbols: reference.SymbolFile,
    memory_symbols: dict[int, reference.Symbol],
) -> tuple[str, str, bytes]:
    opcode = rom[offset]
    length = opcode_length(opcode)
    raw = rom[offset : offset + length]

    if opcode == 0xCB:
        token = f"CB{raw[1]:02X}"
        return token, token, raw

    shape = f"{opcode:02X}"
    if length == 1:
        return shape, shape, raw

    if length == 2:
        if opcode in JR_UNCONDITIONAL | JR_CONDITIONAL:
            return shape + ":rel", shape + ":rel", raw
        return shape + f":{raw[1]:02X}", shape + ":imm8", raw

    value = int.from_bytes(raw[1:3], "little")
    operand = symbolic_operand(symbols, memory_symbols, bank, value)
    return shape + ":" + operand, shape + ":imm16", raw


def apply_far_call_normalization(
    rom: bytes,
    bank: int,
    instruction_addresses: list[int],
    normalized: dict[int, str],
    symbols: reference.SymbolFile,
) -> None:
    addresses = set(instruction_addresses)
    for pc in instruction_addresses:
        offset = reference.rom_file_offset(bank, pc)
        if offset is None or offset + 8 > len(rom):
            continue
        if (
            rom[offset] != 0x21
            or rom[offset + 3] != 0x1E
            or rom[offset + 5] not in CALLS | JP_UNCONDITIONAL
        ):
            continue
        if pc + 3 not in addresses or pc + 5 not in addresses:
            continue
        trampoline = int.from_bytes(rom[offset + 6 : offset + 8], "little")
        location = reference.target_location(bank, trampoline)
        trampoline_symbol = (
            symbols.canonical_by_address.get(location)
            if location is not None else None
        )
        if trampoline_symbol is None or trampoline_symbol.name != "interBankCall":
            continue
        far_address = int.from_bytes(rom[offset + 1 : offset + 3], "little")
        far_bank = rom[offset + 4]
        far_location = (
            (far_bank, far_address)
            if reference.rom_file_offset(far_bank, far_address) is not None
            else None
        )
        far_symbol = (
            symbols.canonical_by_address.get(far_location)
            if far_location is not None else None
        )
        target = (
            far_symbol.name
            if far_symbol is not None
            else f"{far_bank:02X}:{far_address:04X}"
        )
        normalized[pc] = f"21:far:{target}"
        normalized[pc + 3] = "1E:far-bank"


def digest_tokens(tokens: Iterable[str]) -> str:
    return hashlib.sha256("\n".join(tokens).encode("utf-8")).hexdigest()


def decode_routine(
    name: str,
    start: reference.Symbol,
    rom: bytes,
    symbols: reference.SymbolFile,
    memory_symbols: dict[int, reference.Symbol],
    seed_addresses: set[tuple[int, int]],
) -> RoutineFingerprint:
    bank = start.bank
    queue = deque([start.address])
    block_entries = {start.address}
    visited: set[int] = set()
    normalized: dict[int, str] = {}
    shapes: dict[int, str] = {}
    raw_bytes: dict[int, bytes] = {}
    invalid_paths = 0
    truncated = False

    def enqueue(address: int) -> None:
        if address not in visited:
            block_entries.add(address)
            queue.append(address)

    while queue:
        pc = queue.popleft()
        while True:
            if len(visited) >= MAX_ROUTINE_INSTRUCTIONS:
                truncated = True
                queue.clear()
                break
            if (bank, pc) in seed_addresses and pc != start.address:
                break
            offset = reference.rom_file_offset(bank, pc)
            if offset is None or offset >= len(rom) or pc in visited:
                break
            opcode = rom[offset]
            if opcode in INVALID_OPCODES:
                invalid_paths += 1
                break
            length = opcode_length(opcode)
            if offset + length > len(rom):
                invalid_paths += 1
                break

            visited.add(pc)
            normalized[pc], shapes[pc], raw_bytes[pc] = instruction_tokens(
                rom, offset, bank, symbols, memory_symbols
            )
            next_pc = pc + length

            if opcode in JP_UNCONDITIONAL | JP_CONDITIONAL:
                target = int.from_bytes(rom[offset + 1 : offset + 3], "little")
                location = reference.target_location(bank, target)
                if (
                    location is not None
                    and location[0] == bank
                    and location not in seed_addresses
                ):
                    enqueue(location[1])
                if opcode in JP_UNCONDITIONAL:
                    break
            elif opcode in JR_UNCONDITIONAL | JR_CONDITIONAL:
                displacement = int.from_bytes(
                    rom[offset + 1 : offset + 2], "little", signed=True
                )
                target = (next_pc + displacement) & 0xFFFF
                if (bank, target) not in seed_addresses:
                    enqueue(target)
                if opcode in JR_UNCONDITIONAL:
                    break
            elif opcode in UNCONDITIONAL_RETURNS | INDIRECT_JUMPS | HARD_STOPS:
                break
            elif opcode in CONDITIONAL_RETURNS | CALLS | RSTS:
                pass
            pc = next_pc

    ordered_addresses = sorted(visited)
    apply_far_call_normalization(
        rom, bank, ordered_addresses, normalized, symbols
    )
    return RoutineFingerprint(
        name=name,
        bank=bank,
        address=start.address,
        instruction_count=len(ordered_addresses),
        decoded_bytes=sum(len(raw_bytes[address]) for address in ordered_addresses),
        block_entries=len(block_entries),
        complete=not truncated and invalid_paths == 0,
        invalid_paths=invalid_paths,
        truncated=truncated,
        raw_hash=hashlib.sha256(
            b"".join(raw_bytes[address] for address in ordered_addresses)
        ).hexdigest(),
        normalized_hash=digest_tokens(
            normalized[address] for address in ordered_addresses
        ),
        shape_hash=digest_tokens(shapes[address] for address in ordered_addresses),
    )


def choose_named_symbol(
    symbols: reference.SymbolFile, name: str
) -> reference.Symbol:
    return min(
        symbols.by_name[name],
        key=lambda symbol: (
            reference.canonical_symbol_rank(symbol),
            symbol.bank,
            symbol.address,
        ),
    )


def write_csv(path: Path, fieldnames: list[str], rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def percentage(numerator: int, denominator: int) -> float:
    return round(100 * numerator / denominator, 3) if denominator else 0.0


def generate(reference_dir: Path, rom_dir: Path, output_dir: Path) -> dict:
    roms = reference.find_verified_us_roms(rom_dir)
    symbol_files = {
        game: reference.parse_symbol_file(reference_dir / f"{game}.sym")
        for game in ("ages", "seasons")
    }
    referenced_names = {
        game: reference.extract_routine_references(reference_dir, game)
        for game in ("ages", "seasons")
    }
    resolved_names = {
        game: referenced_names[game] & symbol_files[game].by_name.keys()
        for game in ("ages", "seasons")
    }
    seeds = {
        game: reference.resolve_seed_symbols(
            symbol_files[game], referenced_names[game]
        )
        for game in ("ages", "seasons")
    }
    memory_symbols = {
        game: parse_memory_symbols(reference_dir / f"{game}.sym")
        for game in ("ages", "seasons")
    }
    declarations = {
        game: extract_source_declarations(reference_dir, game)
        for game in ("ages", "seasons")
    }

    fingerprints: dict[str, dict[str, RoutineFingerprint]] = {}
    sources: dict[str, dict[str, SourceMatch]] = {}
    routine_fields = [
        "game", "name", "location", "section", "section_kind",
        "source_path", "source_line", "source_ownership", "subsystem",
        "source_confidence", "instruction_count", "decoded_bytes",
        "block_entries", "complete", "invalid_paths", "truncated",
        "raw_hash", "normalized_hash", "shape_hash",
    ]
    for game in ("ages", "seasons"):
        parsed = symbol_files[game]
        seed_addresses = set(seeds[game])
        fingerprints[game] = {}
        sources[game] = {}
        rows = []
        for name in sorted(resolved_names[game]):
            symbol = choose_named_symbol(parsed, name)
            fingerprint = decode_routine(
                name,
                symbol,
                roms[game][1],
                parsed,
                memory_symbols[game],
                seed_addresses,
            )
            source = match_source_declaration(
                name, declarations[game], symbol
            )
            section = parsed.section_for(symbol)
            fingerprints[game][name] = fingerprint
            sources[game][name] = source
            rows.append({
                "game": game,
                "name": name,
                "location": fingerprint.location,
                "section": section.name if section else "",
                "section_kind": section.kind if section else "unknown",
                "source_path": source.path,
                "source_line": source.line or "",
                "source_ownership": source.ownership,
                "subsystem": source.subsystem,
                "source_confidence": source.confidence,
                "instruction_count": fingerprint.instruction_count,
                "decoded_bytes": fingerprint.decoded_bytes,
                "block_entries": fingerprint.block_entries,
                "complete": fingerprint.complete,
                "invalid_paths": fingerprint.invalid_paths,
                "truncated": fingerprint.truncated,
                "raw_hash": fingerprint.raw_hash,
                "normalized_hash": fingerprint.normalized_hash,
                "shape_hash": fingerprint.shape_hash,
            })
        write_csv(output_dir / f"{game}-routines.csv", routine_fields, rows)

    common_names = sorted(resolved_names["ages"] & resolved_names["seasons"])
    shared_rows = []
    classification_counts: dict[str, int] = defaultdict(int)
    subsystem_rows: dict[str, dict[str, int]] = defaultdict(
        lambda: defaultdict(int)
    )
    module_rows: dict[str, dict[str, int | str]] = defaultdict(
        lambda: defaultdict(int)
    )
    ownership_pairs: dict[str, int] = defaultdict(int)
    for name in common_names:
        ages = fingerprints["ages"][name]
        seasons = fingerprints["seasons"][name]
        ages_source = sources["ages"][name]
        seasons_source = sources["seasons"][name]
        complete = ages.complete and seasons.complete
        same_raw = complete and ages.raw_hash == seasons.raw_hash
        same_normalized_hash = (
            complete and ages.normalized_hash == seasons.normalized_hash
        )
        same_shape_hash = complete and ages.shape_hash == seasons.shape_hash
        normalized_equivalent = same_raw or same_normalized_hash
        shape_equivalent = normalized_equivalent or same_shape_hash
        if not complete:
            classification = "incomplete"
        elif same_raw:
            classification = "byte-identical"
        elif same_normalized_hash:
            classification = "relocation-normalized"
        elif same_shape_hash:
            classification = "opcode-shape-only"
        else:
            classification = "different"
        classification_counts[classification] += 1
        ownership_pairs[
            f"{ages_source.ownership}/{seasons_source.ownership}"
        ] += 1

        subsystem = (
            ages_source.subsystem
            if ages_source.subsystem == seasons_source.subsystem
            else f"{ages_source.subsystem}|{seasons_source.subsystem}"
        )
        aggregate = subsystem_rows[subsystem]
        aggregate["shared_routines"] += 1
        aggregate["complete_pairs"] += complete
        aggregate["byte_identical"] += same_raw
        aggregate["relocation_normalized"] += normalized_equivalent
        aggregate["opcode_shape_equal"] += shape_equivalent
        aggregate[classification] += 1

        module = (
            ages_source.path
            if ages_source.path == seasons_source.path
            else f"{ages_source.path}|{seasons_source.path}"
        )
        module_aggregate = module_rows[module]
        module_aggregate["ownership"] = (
            ages_source.ownership
            if ages_source.ownership == seasons_source.ownership
            else f"{ages_source.ownership}/{seasons_source.ownership}"
        )
        module_aggregate["subsystem"] = subsystem
        module_aggregate["shared_routines"] += 1
        module_aggregate["complete_pairs"] += complete
        module_aggregate["normalized_equivalent"] += normalized_equivalent
        module_aggregate["opcode_shape_equal"] += shape_equivalent
        module_aggregate["different"] += classification == "different"
        module_aggregate["incomplete"] += classification == "incomplete"
        module_aggregate["ages_instructions"] += ages.instruction_count
        module_aggregate["seasons_instructions"] += seasons.instruction_count

        shared_rows.append({
            "name": name,
            "classification": classification,
            "ages_location": ages.location,
            "seasons_location": seasons.location,
            "ages_source": ages_source.path,
            "seasons_source": seasons_source.path,
            "ages_ownership": ages_source.ownership,
            "seasons_ownership": seasons_source.ownership,
            "ages_subsystem": ages_source.subsystem,
            "seasons_subsystem": seasons_source.subsystem,
            "ages_instruction_count": ages.instruction_count,
            "seasons_instruction_count": seasons.instruction_count,
            "both_complete": complete,
            "same_raw_instruction_bytes": same_raw,
            "same_normalized_instructions": normalized_equivalent,
            "same_opcode_shape": shape_equivalent,
        })
    write_csv(
        output_dir / "shared-routines.csv",
        [
            "name", "classification", "ages_location", "seasons_location",
            "ages_source", "seasons_source", "ages_ownership",
            "seasons_ownership", "ages_subsystem", "seasons_subsystem",
            "ages_instruction_count", "seasons_instruction_count",
            "both_complete", "same_raw_instruction_bytes",
            "same_normalized_instructions", "same_opcode_shape",
        ],
        shared_rows,
    )

    subsystem_output = []
    for subsystem, counts in sorted(subsystem_rows.items()):
        complete_pairs = counts["complete_pairs"]
        subsystem_output.append({
            "subsystem": subsystem,
            "shared_routines": counts["shared_routines"],
            "complete_pairs": complete_pairs,
            "byte_identical": counts["byte_identical"],
            "byte_identical_percent":
                percentage(counts["byte_identical"], complete_pairs),
            "relocation_normalized": counts["relocation_normalized"],
            "relocation_normalized_percent":
                percentage(counts["relocation_normalized"], complete_pairs),
            "opcode_shape_equal": counts["opcode_shape_equal"],
            "opcode_shape_equal_percent":
                percentage(counts["opcode_shape_equal"], complete_pairs),
            "incomplete": counts["incomplete"],
            "different": counts["different"],
        })
    write_csv(
        output_dir / "subsystem-overlap.csv",
        [
            "subsystem", "shared_routines", "complete_pairs",
            "byte_identical", "byte_identical_percent",
            "relocation_normalized", "relocation_normalized_percent",
            "opcode_shape_equal", "opcode_shape_equal_percent",
            "incomplete", "different",
        ],
        subsystem_output,
    )

    module_output = []
    for module, counts in sorted(
        module_rows.items(),
        key=lambda item: (-int(item[1]["shared_routines"]), item[0]),
    ):
        complete = int(counts["complete_pairs"])
        module_output.append({
            "module": module,
            "ownership": counts["ownership"],
            "subsystem": counts["subsystem"],
            "shared_routines": counts["shared_routines"],
            "complete_pairs": complete,
            "normalized_equivalent": counts["normalized_equivalent"],
            "normalized_equivalent_percent":
                percentage(int(counts["normalized_equivalent"]), complete),
            "opcode_shape_equal": counts["opcode_shape_equal"],
            "opcode_shape_equal_percent":
                percentage(int(counts["opcode_shape_equal"]), complete),
            "different": counts["different"],
            "incomplete": counts["incomplete"],
            "ages_instructions": counts["ages_instructions"],
            "seasons_instructions": counts["seasons_instructions"],
        })
    write_csv(
        output_dir / "module-overlap.csv",
        [
            "module", "ownership", "subsystem", "shared_routines",
            "complete_pairs", "normalized_equivalent",
            "normalized_equivalent_percent", "opcode_shape_equal",
            "opcode_shape_equal_percent", "different", "incomplete",
            "ages_instructions", "seasons_instructions",
        ],
        module_output,
    )

    complete_pairs = sum(
        row["both_complete"] for row in shared_rows
    )
    summary = {
        "schema_version": 1,
        "reference_commit": reference.reference_commit(reference_dir),
        "games": {
            game: {
                "resolved_routine_names": len(resolved_names[game]),
                "source_mapped": sum(
                    source.confidence != "unmapped"
                    for source in sources[game].values()
                ),
                "source_mapping_percent": percentage(
                    sum(
                        source.confidence != "unmapped"
                        for source in sources[game].values()
                    ),
                    len(resolved_names[game]),
                ),
                "common_owned": sum(
                    source.ownership == "common"
                    for source in sources[game].values()
                ),
                "campaign_owned": sum(
                    source.ownership == "campaign"
                    for source in sources[game].values()
                ),
                "complete_fingerprints": sum(
                    fingerprint.complete
                    for fingerprint in fingerprints[game].values()
                ),
            }
            for game in ("ages", "seasons")
        },
        "shared": {
            "routine_names": len(common_names),
            "complete_pairs": complete_pairs,
            "complete_pair_percent": percentage(
                complete_pairs, len(common_names)
            ),
            "classifications": dict(sorted(classification_counts.items())),
            "source_ownership_pairs": dict(sorted(ownership_pairs.items())),
            "same_source_module": sum(
                row["ages_source"] == row["seasons_source"]
                for row in shared_rows
            ),
            "byte_identical_percent_of_complete": percentage(
                classification_counts["byte-identical"], complete_pairs
            ),
            "relocation_normalized_percent_of_complete": percentage(
                classification_counts["byte-identical"]
                + classification_counts["relocation-normalized"],
                complete_pairs,
            ),
            "opcode_shape_equal_percent_of_complete": percentage(
                classification_counts["byte-identical"]
                + classification_counts["relocation-normalized"]
                + classification_counts["opcode-shape-only"],
                complete_pairs,
            ),
        },
        "method": {
            "max_instructions_per_routine": MAX_ROUTINE_INSTRUCTIONS,
            "raw_hash": "SHA-256 over decoded instruction bytes ordered by ROM address",
            "normalized_hash":
                "resolved 16-bit operands become symbol names; relative offsets and recognized callab/jpab banks are normalized",
            "shape_hash":
                "opcodes and CB sub-opcodes retained; ordinary immediate values omitted",
        },
        "limitations": [
            "Routine boundaries are conservative graph walks stopped by another known routine entry.",
            "Source ownership uses exact or suffix label matching and records ambiguity.",
            "Opcode-shape equality is a weak similarity signal, not behavioral proof.",
            "Data-driven and local-label-only routines remain outside the seed set.",
        ],
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "routine-summary.json").write_text(
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
