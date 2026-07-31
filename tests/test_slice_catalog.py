from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "slice_status.py"
SPEC = importlib.util.spec_from_file_location("slice_status", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
slice_status = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(slice_status)


class SliceCatalogTests(unittest.TestCase):
    def test_catalog_is_complete_and_valid(self) -> None:
        catalog = slice_status.load_catalog()
        self.assertEqual([], slice_status.validate_catalog(catalog))
        self.assertGreaterEqual(len(catalog["slices"]), 25)

    def test_broad_playability_tracks_stay_visible(self) -> None:
        catalog = slice_status.load_catalog()
        tracks = {entry["track"] for entry in catalog["slices"]}
        self.assertEqual(set(catalog["tracks"]), tracks)
        priorities = {entry["priority"] for entry in catalog["slices"]}
        self.assertIn("now", priorities)
        self.assertIn("next", priorities)
        self.assertIn("post-first-playable", priorities)

    def test_scenario_names_match_the_cpp_catalog(self) -> None:
        source = (ROOT / "apps" / "room_slice" / "scenario_catalog.cpp").read_text(
            encoding="utf-8"
        )
        for scenario in slice_status.SCENARIOS:
            with self.subTest(scenario=scenario):
                self.assertIn(f'"{scenario}"', source)


if __name__ == "__main__":
    unittest.main()
