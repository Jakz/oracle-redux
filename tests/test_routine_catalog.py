from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import oracles_reference  # noqa: E402
import routine_catalog  # noqa: E402
import trace_contract  # noqa: E402


class RoutineCatalogTests(unittest.TestCase):
    def test_trace_schema_is_valid_json_with_required_definitions(self) -> None:
        schema_path = (
            Path(__file__).resolve().parents[1]
            / "specs"
            / "traces"
            / "oracle-routine-trace.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))

        self.assertEqual(
            "https://json-schema.org/draft/2020-12/schema",
            schema["$schema"],
        )
        self.assertEqual("object", schema["type"])
        self.assertIn("observation", schema["$defs"])
        self.assertIn("stateDelta", schema["$defs"])
        self.assertIn("event", schema["$defs"])

    def test_planned_item_traces_satisfy_semantic_contract(self) -> None:
        plans = (
            Path(__file__).resolve().parents[1]
            / "specs"
            / "traces"
            / "plans"
        )
        plan_paths = sorted(plans.glob("*.json"))
        self.assertGreaterEqual(len(plan_paths), 3)
        for path in plan_paths:
            with self.subTest(path=path.name):
                document = json.loads(path.read_text(encoding="utf-8"))
                self.assertEqual([], trace_contract.validate_trace(document))

    def test_source_mapping_exact_and_namespace_suffix(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "object_code" / "common" / "interactions"
            source.mkdir(parents=True)
            (source / "sample.s").write_text(
                "commonHandler:\n\tret\n", encoding="utf-8"
            )
            declarations = routine_catalog.extract_source_declarations(
                root, "ages"
            )

        exact = routine_catalog.match_source_declaration(
            "commonHandler", declarations
        )
        namespaced = routine_catalog.match_source_declaration(
            "someNamespace.commonHandler", declarations
        )
        self.assertEqual("exact", exact.confidence)
        self.assertEqual("namespace-suffix", namespaced.confidence)
        self.assertEqual("common", exact.ownership)
        self.assertEqual("objects/interactions", exact.subsystem)

    def test_normalized_hash_ignores_resolved_relocation(self) -> None:
        sym_a = """\
[labels]
01:4000 routine
01:4100 target
[sections]
00004000 01:0000 4000 00000200 Bank_1_Code
"""
        sym_b = """\
[labels]
02:4000 routine
02:4200 target
[sections]
00008000 02:0000 4000 00000300 Bank_2_Code
"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path_a = root / "a.sym"
            path_b = root / "b.sym"
            path_a.write_text(sym_a, encoding="utf-8")
            path_b.write_text(sym_b, encoding="utf-8")
            symbols_a = oracles_reference.parse_symbol_file(path_a)
            symbols_b = oracles_reference.parse_symbol_file(path_b)

        rom_a = bytearray(3 * oracles_reference.BANK_SIZE)
        rom_b = bytearray(3 * oracles_reference.BANK_SIZE)
        # call target; ret
        rom_a[0x4000:0x4004] = bytes.fromhex("cd 00 41 c9")
        rom_b[0x8000:0x8004] = bytes.fromhex("cd 00 42 c9")
        routine_a = symbols_a.by_name["routine"][0]
        routine_b = symbols_b.by_name["routine"][0]
        fingerprint_a = routine_catalog.decode_routine(
            "routine", routine_a, bytes(rom_a), symbols_a, {},
            {(1, 0x4000), (1, 0x4100)}
        )
        fingerprint_b = routine_catalog.decode_routine(
            "routine", routine_b, bytes(rom_b), symbols_b, {},
            {(2, 0x4000), (2, 0x4200)}
        )

        self.assertNotEqual(fingerprint_a.raw_hash, fingerprint_b.raw_hash)
        self.assertEqual(
            fingerprint_a.normalized_hash, fingerprint_b.normalized_hash
        )
        self.assertTrue(fingerprint_a.complete)
        self.assertTrue(fingerprint_b.complete)

    def test_far_call_bank_is_normalized(self) -> None:
        sym_a = """\
[labels]
00:008a interBankCall
01:4000 routine
02:4100 target
[sections]
00004000 01:0000 4000 00000100 Bank_1_Code
"""
        sym_b = """\
[labels]
00:008a interBankCall
01:4000 routine
03:4100 target
[sections]
00004000 01:0000 4000 00000100 Bank_1_Code
"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path_a = root / "a.sym"
            path_b = root / "b.sym"
            path_a.write_text(sym_a, encoding="utf-8")
            path_b.write_text(sym_b, encoding="utf-8")
            symbols_a = oracles_reference.parse_symbol_file(path_a)
            symbols_b = oracles_reference.parse_symbol_file(path_b)

        rom_a = bytearray(4 * oracles_reference.BANK_SIZE)
        rom_b = bytearray(4 * oracles_reference.BANK_SIZE)
        rom_a[0x4000:0x4009] = bytes.fromhex("21 00 41 1e 02 cd 8a 00 c9")
        rom_b[0x4000:0x4009] = bytes.fromhex("21 00 41 1e 03 cd 8a 00 c9")
        start_a = symbols_a.by_name["routine"][0]
        start_b = symbols_b.by_name["routine"][0]
        fingerprint_a = routine_catalog.decode_routine(
            "routine", start_a, bytes(rom_a), symbols_a, {},
            {(1, 0x4000), (2, 0x4100)}
        )
        fingerprint_b = routine_catalog.decode_routine(
            "routine", start_b, bytes(rom_b), symbols_b, {},
            {(1, 0x4000), (3, 0x4100)}
        )

        self.assertNotEqual(fingerprint_a.raw_hash, fingerprint_b.raw_hash)
        self.assertEqual(
            fingerprint_a.normalized_hash, fingerprint_b.normalized_hash
        )


if __name__ == "__main__":
    unittest.main()
