# SPDX-License-Identifier: GPL-3.0-or-later
"""EXTI line 0 rule: the examples and READMEs leave it to MENU_BUTTON.

On the STM32WB55, INT_EXPANDER (PB0) shares EXTI line 0 with MENU_BUTTON
(PA0). A SteamiMCP23009E built with INT_EXPANDER calls attachInterrupt() on
PB0 and replaces the MENU interrupt of SteamiButtons. The examples and the
READMEs build the expander without its interrupt pin (poll() and
interruptEvent() read the chip latch) or use Steami.mcp23009e().
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# A SteamiMCP23009E construction that passes INT_EXPANDER.
EXPANDER_WITH_IRQ = re.compile(r"SteamiMCP23009E\s+\w+\s*[({][^;]*\bINT_EXPANDER\b", re.S)


def taught_sources() -> list[Path]:
    """Sketches and READMEs a user copies from."""
    files = sorted((ROOT / "lib").glob("*/examples/**/*.ino"))
    files += sorted((ROOT / "lib").glob("*/README.md"))
    files.append(ROOT / "README.md")
    return files


class Exti0RulesTest(unittest.TestCase):
    def test_matcher_sees_the_interrupt_pin(self) -> None:
        self.assertTrue(
            EXPANDER_WITH_IRQ.search(
                "SteamiMCP23009E expander(Steami.wire(), RST_EXPANDER, MCP23009_I2C_ADDR, "
                "INT_EXPANDER);"
            ),
            "matcher is blind",
        )
        self.assertFalse(
            EXPANDER_WITH_IRQ.search(
                "SteamiMCP23009E expander(Steami.wire(), RST_EXPANDER, MCP23009_I2C_ADDR);"
            )
        )

    def test_no_example_or_readme_attaches_the_expander_interrupt(self) -> None:
        sources = taught_sources()
        self.assertTrue(any(p.suffix == ".ino" for p in sources), "no example found")
        offenders = [
            str(p.relative_to(ROOT))
            for p in sources
            if EXPANDER_WITH_IRQ.search(p.read_text(encoding="utf-8", errors="replace"))
        ]
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
