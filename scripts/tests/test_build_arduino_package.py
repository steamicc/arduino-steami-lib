# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/build-arduino-package.py."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "build-arduino-package.py"


def load_module():
    spec = importlib.util.spec_from_file_location("build_arduino_package", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write(path: Path, content: str = "") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class BuildPackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.mod = load_module()
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name) / "repo"
        self.out = Path(self._tmp.name) / "out"
        self.lib = self.root / "lib"
        write(self.root / "LICENSE", "GPL fixture\n")
        write(self.root / "scripts" / "arduino-package" / "README.md", "Version v{version}\n")
        write(self.lib / "aaa" / "src" / "AAA.h")
        write(self.lib / "aaa" / "src" / "AAA.cpp")
        write(self.lib / "aaa" / "src" / "AAA_const.h")
        write(self.lib / "bbb" / "src" / "BBB.h")

    def build(self, version: str = "1.2.3", **kwargs) -> None:
        self.mod.build_package(self.root, self.out, version, **kwargs)

    def test_flattens_driver_sources(self) -> None:
        self.build()
        for name in ("AAA.h", "AAA.cpp", "AAA_const.h", "BBB.h"):
            self.assertTrue((self.out / "src" / name).is_file(), name)
        self.assertEqual(
            sorted(p.name for p in (self.out / "src").iterdir()),
            ["AAA.cpp", "AAA.h", "AAA_const.h", "BBB.h", "STeaMi.h"],
        )

    def test_placeholder_driver_skipped(self) -> None:
        write(self.lib / "ccc" / "src" / ".gitkeep")
        self.build()
        self.assertEqual(list((self.out / "src").glob("CCC*")), [])
        self.assertFalse((self.out / "examples" / "CCC").exists())
        self.assertNotIn("ccc", (self.out / "src" / "STeaMi.h").read_text().lower())

    def test_excluded_driver_skipped(self) -> None:
        self.build(exclude=frozenset({"bbb"}))
        self.assertTrue((self.out / "src" / "AAA.h").is_file())
        self.assertFalse((self.out / "src" / "BBB.h").exists())

    def test_umbrella_header(self) -> None:
        self.build()
        lines = (self.out / "src" / "STeaMi.h").read_text().splitlines()
        includes = [line for line in lines if line.startswith("#include")]
        self.assertEqual(includes, ['#include "AAA.h"', '#include "BBB.h"'])
        self.assertIn("#pragma once", lines)

    def test_library_properties(self) -> None:
        self.build()
        props = {}
        for line in (self.out / "library.properties").read_text().splitlines():
            key, _, value = line.partition("=")
            props[key] = value
        self.assertEqual(props["name"], "STeaMi")
        self.assertEqual(props["version"], "1.2.3")
        self.assertEqual(props["architectures"], "stm32")
        self.assertEqual(props["depends"], "STM32duinoBLE")
        self.assertEqual(props["includes"], "STeaMi.h")
        self.assertEqual(props["category"], "Sensors")
        self.assertEqual(props["url"], "https://github.com/steamicc/arduino-steami-lib")

    def test_examples_grouped_by_component(self) -> None:
        write(self.lib / "wsen-pads" / "src" / "SteamiWsenPads.h")
        write(self.lib / "wsen-pads" / "examples" / "altitude" / "altitude.ino", "// a\n")
        write(self.lib / "ble" / "examples" / "BleBeacon" / "BleBeacon.ino", "// b\n")
        self.build()
        self.assertTrue((self.out / "examples/WSEN-PADS/altitude/altitude.ino").is_file())
        self.assertTrue((self.out / "examples/BLE/BleBeacon/BleBeacon.ino").is_file())

    def test_example_extra_files_copied(self) -> None:
        write(self.lib / "ble" / "examples" / "BleIndoorMap" / "BleIndoorMap.ino")
        write(self.lib / "ble" / "examples" / "BleIndoorMap" / "BleIndoorMap.md", "doc\n")
        self.build()
        self.assertTrue((self.out / "examples/BLE/BleIndoorMap/BleIndoorMap.md").is_file())

    def test_stray_file_in_examples_ignored(self) -> None:
        write(self.lib / "aaa" / "examples" / ".gitkeep")
        self.build()
        self.assertFalse((self.out / "examples" / "AAA" / ".gitkeep").exists())

    def test_example_name_mismatch_rejected(self) -> None:
        write(self.lib / "aaa" / "examples" / "Foo" / "foo.ino")
        with self.assertRaises(self.mod.PackageError) as ctx:
            self.build()
        self.assertIn("Foo", str(ctx.exception))

    def test_source_collision_rejected(self) -> None:
        write(self.lib / "aaa" / "src" / "X.h")
        write(self.lib / "bbb" / "src" / "X.h")
        with self.assertRaises(self.mod.PackageError) as ctx:
            self.build()
        self.assertIn("X.h", str(ctx.exception))

    def test_case_insensitive_duplicate_is_rejected(self) -> None:
        write(self.lib / "a" / "src" / "Foo.h")
        write(self.lib / "b" / "src" / "foo.h")
        with self.assertRaises(self.mod.PackageError):
            self.build()

    def test_umbrella_includes_function_classes(self) -> None:
        real_root = Path(__file__).resolve().parent.parent.parent
        self.mod.build_package(real_root, self.out, "1.2.3")
        umbrella = (self.out / "src" / "STeaMi.h").read_text()
        for header in (
            "SteamiTemperature.h",
            "SteamiHumidity.h",
            "SteamiPressure.h",
            "SteamiDistance.h",
            "SteamiBattery.h",
            "SteamiHandle.h",
        ):
            self.assertIn(f'#include "{header}"', umbrella)

    def test_umbrella_name_is_reserved(self) -> None:
        write(self.lib / "aaa" / "src" / "Steami.h")
        with self.assertRaises(self.mod.PackageError) as ctx:
            self.build()
        self.assertIn("STeaMi.h", str(ctx.exception))

    def test_umbrella_includes_display_headers(self) -> None:
        real_root = Path(__file__).resolve().parent.parent.parent
        self.mod.build_package(real_root, self.out, "1.2.3")
        umbrella = (self.out / "src" / "STeaMi.h").read_text()
        for header in (
            "SteamiDisplay.h",
            "SteamiDisplayBackend.h",
            "SteamiRgb.h",
            "SteamiFont.h",
            "SteamiTrig.h",
            "SteamiSsd1327Backend.h",
            "SteamiGc9a01Backend.h",
            "SteamiSSD1327.h",
            "SteamiGC9A01.h",
            "SteamiScreen.h",
            "SteamiValue.h",
            "SteamiScreenConst.h",
        ):
            self.assertIn(f'#include "{header}"', umbrella)

    def test_keywords_concatenated(self) -> None:
        write(self.lib / "aaa" / "keywords.txt", "AAA\tKEYWORD1\n")
        write(self.lib / "bbb" / "keywords.txt", "BBB\tKEYWORD1\n")
        self.build()
        text = (self.out / "keywords.txt").read_text()
        self.assertIn("AAA\tKEYWORD1", text)
        self.assertIn("BBB\tKEYWORD1", text)

    def test_license_and_readme(self) -> None:
        self.build()
        self.assertEqual((self.out / "LICENSE").read_text(), "GPL fixture\n")
        self.assertIn("1.2.3", (self.out / "README.md").read_text())

    def test_out_dir_rebuilt(self) -> None:
        write(self.out / "stale.txt", "old")
        self.build()
        self.assertFalse((self.out / "stale.txt").exists())

    def test_version_must_be_semver(self) -> None:
        for bad in ("v1.2.3", "", "1.2"):
            with self.subTest(version=bad), self.assertRaises(self.mod.PackageError):
                self.build(bad)


if __name__ == "__main__":
    unittest.main()
