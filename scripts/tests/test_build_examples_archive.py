# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/build-examples-archive.py."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path


SCRIPT = Path(__file__).resolve().parent.parent / "build-examples-archive.py"

EXAMPLES = ("VL53L1X/GestureDetection", "APDS9960/ambient_light")


def load_module():
    spec = importlib.util.spec_from_file_location("build_examples_archive", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write(path: Path, content: bytes = b"") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


class BuildExamplesArchiveTest(unittest.TestCase):
    def setUp(self) -> None:
        self.mod = load_module()
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        root = Path(self._tmp.name)
        self.pkg = root / "pkg"
        self.bins = root / "bins"
        self.out = root / "out.zip"
        for example in EXAMPLES:
            name = example.split("/")[1]
            write(self.pkg / "examples" / example / f"{name}.ino", b"void setup() {}")
            write(self.bins / f"{example}.bin", f"bin-{example}".encode())

    def test_empty_package_fails(self) -> None:
        empty = Path(self._tmp.name) / "empty"
        (empty / "examples").mkdir(parents=True)
        with self.assertRaises(SystemExit):
            self.mod.construire_index(empty, self.bins, "v0.34.0")

    def test_archive_is_reproducible(self) -> None:
        index = self.mod.construire_index(self.pkg, self.bins, "v0.34.0")
        a = Path(self._tmp.name) / "a.zip"
        b = Path(self._tmp.name) / "b.zip"
        self.mod.ecrire_archive(index, self.bins, a)
        self.mod.ecrire_archive(index, self.bins, b)
        self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_core_version_is_configurable(self) -> None:
        index = self.mod.construire_index(self.pkg, self.bins, "v0.34.0", "9.9.9")
        self.assertEqual(index["coeur"], "9.9.9")

    def test_index_lists_every_example(self) -> None:
        index = self.mod.construire_index(self.pkg, self.bins, "v0.34.0")
        self.assertEqual(index["version"], "v0.34.0")
        self.assertEqual(index["fqbn"], "STMicroelectronics:stm32:STeaMi")
        self.assertEqual(index["coeur"], "3.1.0")
        self.assertEqual(
            [e["id"] for e in index["exemples"]],
            ["APDS9960/ambient_light", "VL53L1X/GestureDetection"],
        )
        entry = index["exemples"][1]
        data = (self.bins / "VL53L1X/GestureDetection.bin").read_bytes()
        self.assertEqual(entry["composant"], "VL53L1X")
        self.assertEqual(entry["nom"], "GestureDetection")
        self.assertEqual(
            entry["source"], "examples/VL53L1X/GestureDetection/GestureDetection.ino"
        )
        self.assertEqual(entry["bin"], "VL53L1X/GestureDetection.bin")
        self.assertEqual(entry["taille"], len(data))
        self.assertEqual(entry["sha256"], hashlib.sha256(data).hexdigest())

    def test_missing_bin_fails(self) -> None:
        (self.bins / "APDS9960/ambient_light.bin").unlink()
        with self.assertRaises(SystemExit) as ctx:
            self.mod.construire_index(self.pkg, self.bins, "v0.34.0")
        self.assertIn("APDS9960/ambient_light", str(ctx.exception))

    def test_archive_layout(self) -> None:
        index = self.mod.construire_index(self.pkg, self.bins, "v0.34.0")
        self.mod.ecrire_archive(index, self.bins, self.out)
        with zipfile.ZipFile(self.out) as zf:
            self.assertEqual(
                sorted(zf.namelist()),
                [
                    "APDS9960/ambient_light.bin",
                    "VL53L1X/GestureDetection.bin",
                    "index.json",
                ],
            )
            self.assertEqual(json.loads(zf.read("index.json")), index)

    def test_rejects_bad_tag(self) -> None:
        with self.assertRaises(SystemExit):
            self.mod.construire_index(self.pkg, self.bins, "0.34.0")


if __name__ == "__main__":
    unittest.main()
