# SPDX-License-Identifier: GPL-3.0-or-later
"""Rules of the STEAMI_BOARD_ compile-time override (lib/steami_board)."""

from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

# A preprocessor use of the board macros: #ifdef / #ifndef / defined(...).
MACRO_USE = re.compile(r"(#\s*if(n)?def\s+|defined\s*\(?\s*)STEAMI_BOARD_")


class SteamiBoardRulesTest(unittest.TestCase):
    def test_two_macros_in_one_unit_fail_to_compile(self) -> None:
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("no c++ compiler on PATH")
        includes = ["-I", str(ROOT / "tests" / "native" / "helpers")]
        for src in sorted((ROOT / "lib").glob("*/src")):
            includes += ["-I", str(src)]
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp) / "two_macros.cpp"
            unit.write_text(
                "#define STEAMI_BOARD_V1B\n"
                "#define STEAMI_BOARD_V1C\n"
                '#include "SteamiBoard.h"\n',
                encoding="utf-8",
            )
            result = subprocess.run(
                [compiler, "-std=c++17", "-fsyntax-only", *includes, str(unit)],
                capture_output=True,
                text=True,
                cwd=ROOT,
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Define only one of STEAMI_BOARD_V1A", result.stderr)

    def test_no_board_macro_in_library_sources(self) -> None:
        self.assertTrue(MACRO_USE.search("#ifdef STEAMI_BOARD_V1C"), "matcher is blind")
        self.assertTrue(MACRO_USE.search("#if defined(STEAMI_BOARD_V1B)"), "matcher is blind")
        sources = sorted((ROOT / "lib").glob("*/src/*.cpp"))
        self.assertTrue(sources, "no library sources found")
        offenders = [
            str(p.relative_to(ROOT))
            for p in sources
            if MACRO_USE.search(p.read_text(encoding="utf-8", errors="replace"))
        ]
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
