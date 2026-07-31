from __future__ import annotations

import unittest
import xml.etree.ElementTree as ET
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
        self.assertIn(r"..\..\docs\development\slice-management.md", solution)
        self.assertIn(r"..\..\specs\slices.json", solution)

    def test_room_slice_explicitly_lists_every_source_for_solution_explorer(self) -> None:
        namespace = {"msb": "http://schemas.microsoft.com/developer/msbuild/2003"}

        def items(path: Path, require_src_filter: bool = False) -> set[str]:
            root = ET.parse(path).getroot()
            result: set[str] = set()
            for tag in ("ClCompile", "ClInclude"):
                for item in root.findall(f".//msb:{tag}", namespace):
                    include = item.get("Include")
                    if include is not None:
                        result.add(include.removeprefix("$(OracleRoot)").replace("\\", "/"))
                    if require_src_filter:
                        filter_node = item.find("msb:Filter", namespace)
                        self.assertIsNotNone(filter_node)
                        assert filter_node is not None
                        self.assertTrue((filter_node.text or "").startswith("src"))
            return result

        expected = {
            path.relative_to(ROOT).as_posix()
            for parent, pattern in (
                (ROOT / "apps" / "room_slice", "*.cpp"),
                (ROOT / "apps" / "room_slice", "*.h"),
                (ROOT / "src", "*.cpp"),
                (ROOT / "include" / "oracle", "*.h"),
            )
            for path in parent.rglob(pattern)
        }
        project_items = items(MSVC / "OracleRoomSlice.vcxproj")
        filter_items = items(
            MSVC / "OracleRoomSlice.vcxproj.filters",
            require_src_filter=True,
        )
        self.assertEqual(expected, project_items)
        self.assertEqual(expected, filter_items)
        self.assertFalse(any("*" in entry for entry in project_items))

    def test_runtime_tests_track_engine_sources_recursively(self) -> None:
        project = (MSVC / "OracleRuntimeTests.vcxproj").read_text(
            encoding="utf-8"
        )
        self.assertIn(r'Include="$(OracleRoot)src\**\*.cpp"', project)
        self.assertIn(r'Include="$(OracleRoot)include\oracle\**\*.h"', project)

    def test_project_filters_mirror_source_responsibilities(self) -> None:
        runtime_expected = (
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
        room_filters = (MSVC / "OracleRoomSlice.vcxproj.filters").read_text(
            encoding="utf-8"
        )
        for entry in (
            r'Filter Include="src"',
            r"src\app\room_slice",
            r"src\content\world",
            r"src\gameplay\items",
            r"src\presentation\render",
        ):
            self.assertIn(entry, room_filters)

        test_filters = (MSVC / "OracleRuntimeTests.vcxproj.filters").read_text(
            encoding="utf-8"
        )
        for entry in runtime_expected:
            self.assertIn(entry, test_filters)
        self.assertIn(r"tests\cpp\runtime\*.cpp", test_filters)

    def test_slice_has_default_byo_rom_debug_argument(self) -> None:
        project = (MSVC / "OracleRoomSlice.vcxproj").read_text(
            encoding="utf-8"
        )
        self.assertIn("LocalDebuggerCommandArguments", project)
        self.assertIn("Oracle of Ages (USA).gbc", project)
        self.assertIn("--scenario latest", project)
        self.assertIn("OracleRomPath", project)
        self.assertIn(r'apps\room_slice\main.cpp', project)
        self.assertIn(r'apps\room_slice\scenario_catalog.cpp', project)


if __name__ == "__main__":
    unittest.main()
