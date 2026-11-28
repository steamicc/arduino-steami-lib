# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/test-examples.py."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "test-examples.py"


def load_module():
    spec = importlib.util.spec_from_file_location("test_examples_script", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class SelectExamplesTest(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()

    def test_select_examples_only(self):
        self.assertEqual(
            self.mod.select_examples(["a/x", "b/y"], None, None, only=["lib/b/examples/y"]),
            ["b/y"],
        )

    def test_select_examples_only_ignores_blank_lines(self):
        self.assertEqual(
            self.mod.select_examples(["a/x", "b/y"], None, None, only=["", "lib/a/examples/x", ""]),
            ["a/x"],
        )

    def test_select_examples_only_inconnu(self):
        with self.assertRaises(ValueError):
            self.mod.select_examples(["a/x"], None, None, only=["lib/a/examples/z"])

    def test_select_examples_only_malforme(self):
        with self.assertRaises(ValueError):
            self.mod.select_examples(["a/x"], None, None, only=["a/x"])


if __name__ == "__main__":
    unittest.main()
