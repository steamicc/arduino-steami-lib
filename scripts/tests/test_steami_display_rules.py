# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile-time rules of the display colour type (lib/steami_board)."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class SteamiDisplayRulesTest(unittest.TestCase):
    def _compiles(self, body: str) -> bool:
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("no c++ compiler on PATH")
        includes = ["-I", str(ROOT / "tests" / "native" / "helpers")]
        for src in sorted((ROOT / "lib").glob("*/src")):
            includes += ["-I", str(src)]
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp) / "colour_rule.cpp"
            unit.write_text(
                '#include "SteamiRgb.h"\n'
                "void f(SteamiRgb) {}\n"
                f"int main() {{ {body} }}\n",
                encoding="utf-8",
            )
            result = subprocess.run(
                [compiler, "-std=c++17", "-fsyntax-only", *includes, str(unit)],
                capture_output=True,
                text=True,
                cwd=ROOT,
            )
        return result.returncode == 0

    def test_integer_does_not_convert_to_a_colour(self) -> None:
        for body in ("SteamiRgb c = 15;", "f(15);", "f(0);", "SteamiRgb c{15};"):
            with self.subTest(body=body):
                self.assertFalse(self._compiles(body))

    def test_explicit_forms_compile(self) -> None:
        self.assertTrue(
            self._compiles("f(SteamiRgb::gray(15)); f({255, 128, 0}); f(STEAMI_BLUE);")
        )


if __name__ == "__main__":
    unittest.main()
