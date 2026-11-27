# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile-time rules of the display colour type (lib/steami_board)."""

from __future__ import annotations

import re
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
                '#include "SteamiRgb.h"\n'
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
                check=False,
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

    def test_value_rejects_long_long(self) -> None:
        self.assertTrue(self._compiles("SteamiValue v(1L); (void)v;", header="SteamiValue.h"))
        self.assertFalse(self._compiles("SteamiValue v(1LL); (void)v;", header="SteamiValue.h"))

    def test_history_capacity_bounds(self) -> None:
        self.assertTrue(
            self._compiles(
                "SteamiHistory<1> a; SteamiHistory<65535> b; (void)a; (void)b;",
                "SteamiHistory.h",
            )
        )
        for body in ("SteamiHistory<0> a; (void)a;", "SteamiHistory<65536> a; (void)a;"):
            with self.subTest(body=body):
                self.assertFalse(self._compiles(body, "SteamiHistory.h"))

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

    def test_drawing_example_draws_the_gallery(self) -> None:
        # The example copies the native gallery body line for line (comments and blanks aside).
        def body(path: Path) -> list[str]:
            lines = path.read_text(encoding="utf-8").splitlines()
            marker = "drawGallery(SteamiDisplay& d) {"
            start = next((i for i, line in enumerate(lines) if marker in line), None)
            if start is None:
                self.fail(f"{path.name} has no '{marker}': was drawGallery renamed?")
            out: list[str] = []
            for line in lines[start + 1 :]:
                if line == "}":
                    break
                code = line.split("//")[0].strip()  # no string of the gallery holds "//"
                if code:
                    out.append(code)
            return out

        sketch = ROOT / "lib" / "steami_board" / "examples" / "Drawing" / "Drawing.ino"
        gallery = ROOT / "tests" / "native" / "helpers" / "SteamiGallery.h"
        self.assertTrue(sketch.is_file(), "Drawing.ino is missing")
        sketch_body, gallery_body = body(sketch), body(gallery)
        self.assertTrue(sketch_body)
        self.assertTrue(gallery_body)
        self.assertEqual(sketch_body, gallery_body)
        self.assertIn("drawGallery(screen);", sketch.read_text(encoding="utf-8"))

    # One token of a statement: a string literal, a SteamiAt::, SteamiFace:: or STEAMI_ name,
    # nullptr, an identifier (skipped, so that the digits of a name are no number) or a numeric
    # literal.
    _TOKEN = re.compile(
        r'"(?:[^"\\]|\\.)*"'
        r"|SteamiAt::\w+|SteamiFace::\w+|\bSTEAMI_\w+|\bnullptr\b"
        r"|(?P<ident>\b[A-Za-z_]\w*)"
        r"|(?<![\w.])-?\d+(?:\.\d+)?[fFuUlL]*"
    )

    @classmethod
    def _calls(cls, text: str) -> list[tuple[str, list[str]]]:
        """Every statement of text that calls "screen.": (method name, literal tokens).

        The call may follow an if or an assignment ("selected = screen.menu(...)"); the tokens
        are read from "screen." on, so the condition or the assigned name adds none.
        """
        code = "\n".join(line.split("//")[0] for line in text.splitlines())
        out: list[tuple[str, list[str]]] = []
        for raw in re.split(r"[;{}]", code):  # no string of the mockups holds ; { or }
            statement = " ".join(raw.split())
            method = re.search(r"\bscreen\.(\w+)", statement)
            if method is None:
                continue
            call = statement[method.start() :]
            tokens = [m.group(0) for m in cls._TOKEN.finditer(call) if m.group("ident") is None]
            out.append((method.group(1), tokens))
        return out

    def _body(self, path: Path, marker: str) -> str:
        """The lines after the one holding marker, up to the first line "}"."""
        lines = path.read_text(encoding="utf-8").splitlines()
        start = next((i for i, line in enumerate(lines) if marker in line), None)
        if start is None:
            self.fail(f"{path.name} has no '{marker}'")
        end = lines.index("}", start + 1)
        return "\n".join(lines[start + 1 : end])

    def test_calls_tokens(self) -> None:
        # Positive control of the tokenizer: literals, names and signs; no digit of a name.
        self.assertEqual(
            self._calls(
                'screen.value(battery.percent(), "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2,'
                ' -12);  // rows 72..79\nif (x) {\n}\nscreen.vline(cx, cy - r / 2, r, STEAMI_DARK);'
                " delay(500); x2 = 3; screen.face(SteamiFace::HAPPY);"
            ),
            [
                ("value", ['"%"', "SteamiAt::CENTER", "nullptr", "STEAMI_WHITE", "2", "-12"]),
                ("vline", ["2", "STEAMI_DARK"]),
                ("face", ["SteamiFace::HAPPY"]),
            ],
        )
        # A call after an if or an assignment counts, from "screen." on; another object's does not.
        self.assertEqual(
            self._calls(
                "if (c) screen.graph(h, 950, 1050);\nselected = screen.menu(ITEMS, selected);"
                "\nx = notscreen.foo(1);"
            ),
            [("graph", ["950", "1050"]), ("menu", [])],
        )

    def test_screen_examples_draw_the_mockups(self) -> None:
        # Each example's loop() makes the screen calls of its mockup helper, with the same
        # literals; the helper takes the sensor readings as parameters.
        helpers = ROOT / "tests" / "native" / "helpers" / "SteamiMockups.h"
        examples = ROOT / "lib" / "steami_board" / "examples"
        self.assertTrue(helpers.is_file(), "SteamiMockups.h is missing")
        for example, helper in (
            ("ScreenTemperature", "mockupTemperature"),
            ("ScreenBattery", "mockupBattery"),
            ("ScreenComfort", "mockupComfort"),
            ("ScreenGauge", "mockupGauge"),
            ("ScreenCompass", "mockupCompass"),
            ("ScreenFaces", "mockupFaces"),
            ("ScreenWatch", "mockupWatch"),
        ):
            with self.subTest(example=example):
                sketch = examples / example / f"{example}.ino"
                self.assertTrue(sketch.is_file(), f"{example}.ino is missing")
                loop = self._body(sketch, "void loop() {")
                drawing = self._body(helpers, f"inline void {helper}(")
                sketch_calls, helper_calls = self._calls(loop), self._calls(drawing)
                self.assertTrue(sketch_calls)
                self.assertTrue(helper_calls)
                self.assertEqual(sketch_calls, helper_calls)
                self.assertIn("screen.show();", loop)
                self.assertTrue(drawing.rstrip().endswith("screen.show();"))


if __name__ == "__main__":
    unittest.main()
