from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MSVC = ROOT / "projects" / "msvc"


class MsvcProjectTests(unittest.TestCase):
    def test_solution_keeps_runtime_and_slice_projects(self) -> None:
        solution = (MSVC / "oracle-redux.sln").read_text(encoding="utf-8")
        self.assertIn("OracleRoomSlice.vcxproj", solution)
        self.assertIn("OracleRuntimeTests.vcxproj", solution)
        self.assertIn(r"..\..\reference\SDL\VisualC\SDL\SDL.vcxproj", solution)
        for folder in (
            '"Applications", "Applications"',
            '"Tests", "Tests"',
            '"Dependencies", "Dependencies"',
            '"Project Guidance", "Project Guidance"',
        ):
            self.assertIn(folder, solution)
        self.assertIn(r"..\..\PROJECT_STATUS.md", solution)
        self.assertIn(r"..\..\docs\development\conventions.md", solution)

    def test_projects_track_engine_sources_recursively(self) -> None:
        for name in ("OracleRoomSlice.vcxproj", "OracleRuntimeTests.vcxproj"):
            with self.subTest(project=name):
                project = (MSVC / name).read_text(encoding="utf-8")
                self.assertIn(r'Include="$(OracleRoot)src\**\*.cpp"', project)
                self.assertIn(
                    r'Include="$(OracleRoot)include\oracle\**\*.h"',
                    project,
                )

    def test_project_filters_mirror_source_responsibilities(self) -> None:
        expected = (
            r"Sources\Content\ROM",
            r"Sources\Content\World",
            r"Sources\Content\Actors",
            r"Sources\Content\Sprites",
            r"Sources\Gameplay\Actors",
            r"Sources\Gameplay\Combat",
            r"Sources\Gameplay\Interactions",
            r"Sources\Gameplay\Items",
            r"Sources\Gameplay\Player",
            r"Public Headers\Content\World",
            r"Public Headers\Gameplay\Items",
        )
        for name in (
            "OracleRoomSlice.vcxproj.filters",
            "OracleRuntimeTests.vcxproj.filters",
        ):
            with self.subTest(filters=name):
                filters = (MSVC / name).read_text(encoding="utf-8")
                for entry in expected:
                    self.assertIn(entry, filters)

        room_filters = (MSVC / "OracleRoomSlice.vcxproj.filters").read_text(
            encoding="utf-8"
        )
        self.assertIn(r"apps\room_slice\main.cpp", room_filters)
        test_filters = (MSVC / "OracleRuntimeTests.vcxproj.filters").read_text(
            encoding="utf-8"
        )
        self.assertIn(r"tests\cpp\runtime\*.cpp", test_filters)

    def test_slice_has_default_byo_rom_debug_argument(self) -> None:
        project = (MSVC / "OracleRoomSlice.vcxproj").read_text(
            encoding="utf-8"
        )
        self.assertIn("LocalDebuggerCommandArguments", project)
        self.assertIn("Oracle of Ages (USA).gbc", project)
        self.assertNotIn("--octorok-scenario", project)


if __name__ == "__main__":
    unittest.main()
