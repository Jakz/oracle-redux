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


if __name__ == "__main__":
    unittest.main()
