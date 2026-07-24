from __future__ import annotations

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import rom_catalog  # noqa: E402


class RomCatalogTests(unittest.TestCase):
    def test_candidate_shared_regions_finds_relocation(self) -> None:
        common = bytes(range(32, 128))
        left = b"\x00" * 17 + common + b"\xff" * 29
        right = b"\x11" * 41 + common + b"\xee" * 7

        regions = rom_catalog.candidate_shared_regions(
            left, right, anchor_size=16, minimum=64
        )

        self.assertIn((17, 41, len(common)), regions)

    def test_padding_bank_recognizes_constant_and_rom_fill(self) -> None:
        self.assertTrue(rom_catalog.is_padding_bank(bytes([0x37]) * 0x4000))
        self.assertTrue(
            rom_catalog.is_padding_bank(bytes([0xFF]) * 0x3FFF + b"\x00")
        )
        self.assertFalse(
            rom_catalog.is_padding_bank(bytes(range(256)) * (0x4000 // 256))
        )

    def test_header_checksum_formula(self) -> None:
        data = bytearray(0x150)
        data[0x104:0x134] = rom_catalog.NINTENDO_LOGO
        data[0x134:0x13F] = b"ZELDA DIN\0\0"
        data[0x13F:0x143] = b"AZ7P"
        data[0x143] = 0xC0
        data[0x144:0x146] = b"01"
        data[0x147] = 0x1B
        data[0x148] = 0x06
        data[0x149] = 0x02
        checksum = 0
        for value in data[0x134:0x14D]:
            checksum = (checksum - value - 1) & 0xFF
        data[0x14D] = checksum
        global_checksum = sum(data) & 0xFFFF
        data[0x14E:0x150] = global_checksum.to_bytes(2, "big")

        header = rom_catalog.parse_header(bytes(data))

        self.assertEqual("seasons", header.detected_game)
        self.assertTrue(header.header_checksum_valid)
        self.assertTrue(header.nintendo_logo_valid)

    def test_region_from_game_code(self) -> None:
        self.assertEqual("usa", rom_catalog.region_from_game_code("AZ8E"))
        self.assertEqual("europe", rom_catalog.region_from_game_code("AZ7P"))
        self.assertEqual("japan", rom_catalog.region_from_game_code("AZ8J"))


if __name__ == "__main__":
    unittest.main()
