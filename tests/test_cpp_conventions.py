from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class CppConventionTests(unittest.TestCase):
    def test_public_cpp_headers_use_dot_h(self) -> None:
        include_root = ROOT / "include"
        headers = [path for path in include_root.rglob("*") if path.is_file()]

        self.assertGreater(len(headers), 0)
        wrong_suffix = [
            path.relative_to(ROOT).as_posix()
            for path in headers
            if path.suffix != ".h"
        ]
        self.assertEqual([], wrong_suffix)

    def test_implementation_responsibilities_use_subfolders(self) -> None:
        expected = (
            "src/content/rom",
            "src/content/world",
            "src/content/actors",
            "src/content/sprites",
            "src/core/actors",
            "src/core/items",
            "src/core/world",
            "src/gameplay/actors",
            "src/gameplay/combat",
            "src/gameplay/interactions",
            "src/gameplay/items",
            "src/gameplay/player",
            "src/presentation/camera",
            "src/presentation/render",
            "src/presentation/timing",
            "src/script/runtime",
            "src/script/state",
        )
        for relative in expected:
            with self.subTest(directory=relative):
                directory = ROOT / relative
                self.assertTrue(directory.is_dir())
                self.assertGreater(len(list(directory.glob("*.cpp"))), 0)

        for relative in (
            "src/content",
            "src/core",
            "src/gameplay",
            "src/presentation",
            "src/script",
        ):
            with self.subTest(flat_directory=relative):
                self.assertEqual([], list((ROOT / relative).glob("*.cpp")))

    def test_progress_and_conventions_are_persistent(self) -> None:
        status = (ROOT / "PROJECT_STATUS.md").read_text(encoding="utf-8")
        for heading in (
            "## Current checkpoint",
            "## Completed slices",
            "## Next implementation queue",
            "## Known fidelity gaps",
            "## Verification baseline",
            "## Slice completion record",
        ):
            self.assertIn(heading, status)

        agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
        self.assertIn("update `PROJECT_STATUS.md`", agents)
        self.assertIn("leaving the worktree clean", agents)
        self.assertTrue((ROOT / "docs/development/conventions.md").is_file())
        self.assertTrue((ROOT / "docs/development/slice-management.md").is_file())
        self.assertTrue((ROOT / "specs/slices.json").is_file())
        self.assertIn("specs/slices.json", agents)


if __name__ == "__main__":
    unittest.main()
