from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import oracles_reference  # noqa: E402


class OraclesReferenceTests(unittest.TestCase):
    def test_parse_symbol_file_keeps_rom_labels_and_sections(self) -> None:
        contents = """\
[labels]
00:0150 begin
01:4000 bankedRoutine
00:c000 wRam

[sections]
00000150 00:0150 0150 00000020 Bank_0
00004000 01:0000 4000 00000010 Bank_1_Code
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.sym"
            path.write_text(contents, encoding="utf-8")
            parsed = oracles_reference.parse_symbol_file(path)

        self.assertEqual(["begin", "bankedRoutine"], [
            symbol.name for symbol in parsed.labels
        ])
        self.assertEqual("mixed", parsed.sections[0].kind)
        self.assertEqual("code", parsed.sections[1].kind)

    def test_extract_routine_references_handles_direct_and_far_calls(self) -> None:
        contents = """\
entry:
    call directTarget
    call nz, conditionalTarget ; comment
    callab farTarget
    jpab $12, explicitFarTarget
    jr @local
    jp hl
"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "code").mkdir()
            (root / "code" / "sample.s").write_text(contents, encoding="utf-8")
            references = oracles_reference.extract_routine_references(root, "ages")

        self.assertIn("directTarget", references)
        self.assertIn("conditionalTarget", references)
        self.assertIn("farTarget", references)
        self.assertIn("explicitFarTarget", references)
        self.assertNotIn("@local", references)
        self.assertNotIn("hl", references)

    def test_decode_call_graph_recognizes_callab_pattern(self) -> None:
        rom = bytearray(2 * oracles_reference.BANK_SIZE)
        # Bank 1 $4000: ld hl,$4100; ld e,$01; call interBankCall; ret
        rom[0x4000:0x4009] = bytes.fromhex("21 00 41 1e 01 cd 8a 00 c9")
        contents = """\
[labels]
00:008a interBankCall
01:4000 caller
01:4100 callee

[sections]
00000068 00:0068 0068 00000040 Bank_0_Early_Functions
00004000 01:0000 4000 00000200 Bank_1_Code
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.sym"
            path.write_text(contents, encoding="utf-8")
            symbols = oracles_reference.parse_symbol_file(path)
        seeds = {
            (1, 0x4000): symbols.canonical_by_address[(1, 0x4000)],
            (1, 0x4100): symbols.canonical_by_address[(1, 0x4100)],
        }

        rows = oracles_reference.decode_call_graph(
            "ages", bytes(rom), symbols, seeds
        )

        far_edges = [row for row in rows if row["kind"] == "far-call"]
        self.assertEqual(1, len(far_edges))
        self.assertEqual("callee", far_edges[0]["target"])
        self.assertEqual("verified-pattern", far_edges[0]["confidence"])

    def test_decode_call_graph_names_known_rst_vector(self) -> None:
        rom = bytearray(2 * oracles_reference.BANK_SIZE)
        rom[0x4000:0x4002] = bytes.fromhex("d7 c9")  # rst $10; ret
        contents = """\
[labels]
01:4000 caller

[sections]
00004000 01:0000 4000 00000010 Bank_1_Code
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.sym"
            path.write_text(contents, encoding="utf-8")
            symbols = oracles_reference.parse_symbol_file(path)
        seeds = {
            (1, 0x4000): symbols.canonical_by_address[(1, 0x4000)],
        }

        rows = oracles_reference.decode_call_graph(
            "ages", bytes(rom), symbols, seeds
        )

        self.assertEqual("rst_addAToHl", rows[0]["target"])
        self.assertTrue(rows[0]["resolved_symbol"])
        self.assertEqual("named hardware RST vector", rows[0]["note"])


if __name__ == "__main__":
    unittest.main()
