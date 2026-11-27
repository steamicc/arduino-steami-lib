# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that CI installs the same, pinned STM32duinoBLE as platformio.ini.

The release archive of compiled examples is built with the STM32duinoBLE
version named by STM32DUINOBLE_VERSION in the workflows. It must not drift
between workflows, every install must be pinned, and it must equal the lower
bound of stm32duino/STM32duinoBLE in platformio.ini.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"
PINNED_WORKFLOWS = ("arduino-package-check.yml", "examples-archive.yml")

ENV_RE = re.compile(r"^\s*STM32DUINOBLE_VERSION:\s*['\"]?([^'\"\s#]+)['\"]?\s*(?:#.*)?$", re.M)
INSTALL_RE = re.compile(r"lib\s+install\s+['\"]?STM32duinoBLE(?:@([^'\"\s]+))?")
PIO_RE = re.compile(r"^\s*stm32duino/STM32duinoBLE\s*@\s*\^(\d+\.\d+\.\d+)\s*$", re.M)
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
VAR_REFS = ("${STM32DUINOBLE_VERSION}", "$STM32DUINOBLE_VERSION")


def env_versions(text: str) -> list[str]:
    return ENV_RE.findall(text)


def platformio_lower_bound() -> str:
    found = PIO_RE.findall((ROOT / "platformio.ini").read_text(encoding="utf-8"))
    if len(found) != 1:
        raise AssertionError(
            f"expected one stm32duino/STM32duinoBLE@^X.Y.Z in platformio.ini, found {found}"
        )
    return found[0]


class Stm32duinoBleVersionTest(unittest.TestCase):
    def test_pinned_workflows_define_the_same_version(self) -> None:
        versions = {}
        for name in PINNED_WORKFLOWS:
            found = env_versions((WORKFLOWS / name).read_text(encoding="utf-8"))
            self.assertEqual(
                len(found), 1, f"{name}: expected one STM32DUINOBLE_VERSION, got {found}"
            )
            self.assertRegex(found[0], VERSION_RE, name)
            versions[name] = found[0]
        self.assertEqual(len(set(versions.values())), 1, versions)

    def test_version_matches_platformio_lower_bound(self) -> None:
        bound = platformio_lower_bound()
        for name in PINNED_WORKFLOWS:
            found = env_versions((WORKFLOWS / name).read_text(encoding="utf-8"))
            self.assertEqual(found, [bound], f"{name} vs platformio.ini ^{bound}")

    def test_every_install_is_pinned(self) -> None:
        bound = platformio_lower_bound()
        installs = 0
        for path in sorted(WORKFLOWS.glob("*.yml")):
            text = path.read_text(encoding="utf-8")
            for match in INSTALL_RE.finditer(text):
                installs += 1
                pin = match.group(1)
                line = text.count("\n", 0, match.start()) + 1
                where = f"{path.name}:{line}"
                self.assertIsNotNone(pin, f"{where}: STM32duinoBLE installed without @version")
                if pin in VAR_REFS:
                    found = env_versions(text)
                    self.assertEqual(len(found), 1, f"{where}: {pin} but no STM32DUINOBLE_VERSION")
                    pin = found[0]
                self.assertEqual(pin, bound, f"{where} vs platformio.ini ^{bound}")
        self.assertGreaterEqual(installs, len(PINNED_WORKFLOWS))


if __name__ == "__main__":
    unittest.main()
