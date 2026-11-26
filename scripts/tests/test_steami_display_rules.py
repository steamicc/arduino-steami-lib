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
    def _compiles(self, body: str, header: str = "SteamiRgb.h") -> bool:
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("no c++ compiler on PATH")
        includes = ["-I", str(ROOT / "tests" / "native" / "helpers")]
        for src in sorted((ROOT / "lib").glob("*/src")):
            includes += ["-I", str(src)]
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp) / "colour_rule.cpp"
            unit.write_text(
                f'#include "{header}"\n'
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

    def test_display_takes_no_integer_colour(self) -> None:
        # Positive control first: the header is found and the colour overload compiles.
        self.assertTrue(
            self._compiles("SteamiDisplay d; d.clear(SteamiRgb::gray(15));", "SteamiDisplay.h")
        )
        for body in ("SteamiDisplay d; d.clear(15);", "SteamiDisplay d; d.pixel(1, 1, 15);"):
            with self.subTest(body=body):
                self.assertFalse(self._compiles(body, "SteamiDisplay.h"))

    def test_backends_never_call_driver_drawing(self) -> None:
        # The layer draws; a backend only forwards pixels, rectangles and controls. Inline
        # forwarding overrides live in the headers, so both files of each backend are checked.
        src = ROOT / "lib" / "steami_board" / "src"
        files = [
            src / f"Steami{chip}Backend.{ext}"
            for chip in ("Ssd1327", "Gc9a01")
            for ext in ("h", "cpp")
        ]
        pattern = r"\.(line|text|scroll)\s*\("
        # Positive controls: the pattern catches the calls it forbids, and no file is missing.
        for call in ("_oled.line(", "_oled.text(", "_oled.scroll("):
            with self.subTest(control=call):
                self.assertRegex(call, pattern)
        for path in files:
            with self.subTest(path=path.name):
                self.assertTrue(path.is_file() and path.stat().st_size > 0)
                self.assertNotRegex(path.read_text(encoding="utf-8"), pattern)


if __name__ == "__main__":
    unittest.main()
