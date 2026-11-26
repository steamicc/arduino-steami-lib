# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/check-footprint.py."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "check-footprint.py"

# Section sizes of the v0.35.0 Temperature build (PlatformIO: Flash 65 728 B, RAM 5 044 B);
# .noinit and MAPPING_TABLE addresses as measured, the other flash addresses laid end to end.
SIZE_A = """\
.pio/build/steami/firmware.elf  :
section               size         addr
.isr_vector            316    134217728
.text                58824    134218044
.rodata               4632    134276868
.ARM                     8    134281500
.init_array             24    134281508
.fini_array             12    134281532
.data                 2272    536870916
.bss                  2772    536873188
.noinit                  0    536875960
._user_heap_stack     1536    536875960
MAPPING_TABLE           40    537067520
.ARM.attributes         48            0
.debug_info         123456            0
Total               193940
"""

FLASH_SIZE = 524288


def load_module():
    spec = importlib.util.spec_from_file_location("check_footprint", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class CheckFootprintTest(unittest.TestCase):
    def setUp(self) -> None:
        self.mod = load_module()

    def test_parse_sections(self) -> None:
        sections = self.mod.parse_sections(SIZE_A)
        self.assertEqual(sections[".text"], (58824, 134218044))
        self.assertNotIn("Total", sections)
        self.assertNotIn("section", sections)

    def test_measure(self) -> None:
        fp = self.mod.measure(self.mod.parse_sections(SIZE_A), FLASH_SIZE)
        self.assertEqual(fp, self.mod.Footprint(flash=66088, ram=5044))

    def test_measure_counts_noinit(self) -> None:
        text = SIZE_A.replace(".noinit                  0", ".noinit                 16")
        fp = self.mod.measure(self.mod.parse_sections(text), FLASH_SIZE)
        self.assertEqual(fp.ram, 5060)

    def test_measure_without_bss_raises(self) -> None:
        text = "\n".join(
            line for line in SIZE_A.splitlines() if not line.startswith(".bss")
        )
        with self.assertRaises(self.mod.FootprintError):
            self.mod.measure(self.mod.parse_sections(text), FLASH_SIZE)

    def test_limits_from_the_board(self) -> None:
        lim = self.mod.limits(ROOT / "boards" / "steami.json")
        self.assertEqual(lim, self.mod.Limits(flash=458752, ram=53248))

    def test_violations(self) -> None:
        lim = self.mod.Limits(flash=458752, ram=53248)
        flash = self.mod.violations("s", self.mod.Footprint(458753, 53248), lim)
        self.assertEqual(len(flash), 1)
        self.assertIn("flash", flash[0])
        ram = self.mod.violations("s", self.mod.Footprint(458752, 53249), lim)
        self.assertEqual(len(ram), 1)
        self.assertIn("RAM", ram[0])
        self.assertEqual(
            self.mod.violations("s", self.mod.Footprint(458752, 53248), lim), []
        )


if __name__ == "__main__":
    unittest.main()
