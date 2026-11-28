# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that CI installs the same, pinned toolchains everywhere.

The release archive of compiled examples is built with the STM32duinoBLE
version named by STM32DUINOBLE_VERSION in the workflows. It must not drift
between workflows, every install must be pinned, and it must equal the lower
bound of stm32duino/STM32duinoBLE in platformio.ini.

The toolchain caches are saved by the `caches` job of ci.yml only, on main,
and restored everywhere else: arduino-cli and the STM32 core are pinned to
the same versions in every workflow that names them, the ~/.arduino15 cache
steps are identical, and no other workflow saves ~/.platformio or
~/.arduino15 (a job that saved an incomplete cache poisoned it for all).
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"
PINNED_WORKFLOWS = ("arduino-package-check.yml", "examples-archive.yml", "ci.yml")
# The only workflow allowed to save the toolchain caches (its `caches` job).
SAVING_WORKFLOW = "ci.yml"
ARDUINO_CLI_VERSION = "1.6.0"
STM32_CORE_VERSION = "3.1.0"

ENV_RE = re.compile(r"^\s*STM32DUINOBLE_VERSION:\s*['\"]?([^'\"\s#]+)['\"]?\s*(?:#.*)?$", re.M)
INSTALL_RE = re.compile(r"lib\s+install\s+['\"]?STM32duinoBLE(?:@([^'\"\s]+))?")
PIO_RE = re.compile(r"^\s*stm32duino/STM32duinoBLE\s*@\s*\^(\d+\.\d+\.\d+)\s*$", re.M)
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
VAR_REFS = ("${STM32DUINOBLE_VERSION}", "$STM32DUINOBLE_VERSION")
# A YAML list item: in a workflow, a step (or an entry of another list).
ITEM_RE = re.compile(r"^(\s*)-\s")
USES_RE = re.compile(r"^\s*-?\s*uses:\s*(\S+)", re.M)
SETUP_CLI = "arduino/setup-arduino-cli@"
CLI_VERSION_LINE = "version: ${{ env.ARDUINO_CLI_VERSION }}"
CACHE_SAVE = "actions/cache@"
CACHE_RESTORE = "actions/cache/restore@"


def env_versions(text: str) -> list[str]:
    return ENV_RE.findall(text)


def env_values(text: str, name: str) -> list[str]:
    pattern = rf"^\s*{re.escape(name)}:\s*['\"]?([^'\"\s#]+)['\"]?\s*(?:#.*)?$"
    return re.findall(pattern, text, re.M)


def workflow_steps(text: str) -> list[str]:
    """Each list item of the workflow, with its indented lines (a step)."""
    steps: list[str] = []
    lines = text.splitlines()
    for i, line in enumerate(lines):
        start = ITEM_RE.match(line)
        if not start:
            continue
        indent = len(start.group(1))
        block = [line]
        for following in lines[i + 1 :]:
            if following.strip() and len(following) - len(following.lstrip()) <= indent:
                break
            block.append(following)
        steps.append("\n".join(block))
    return steps


def step_uses(step: str) -> str:
    found = USES_RE.findall(step)
    return found[0] if found else ""


def step_lines(step: str, prefix: str) -> list[str]:
    """The stripped lines of a step that start with prefix (e.g. "key:")."""
    return [line.strip() for line in step.splitlines() if line.strip().startswith(prefix)]


def cache_path(step: str) -> list[str]:
    """The paths of a cache step ("path: x" or a "path: |" block)."""
    lines = step.splitlines()
    for i, line in enumerate(lines):
        match = re.match(r"^(\s*)path:\s*(.*)$", line)
        if not match:
            continue
        if match.group(2) not in ("|", ">-", ""):
            return [match.group(2).strip()]
        indent = len(match.group(1))
        paths = []
        for following in lines[i + 1 :]:
            if following.strip() and len(following) - len(following.lstrip()) <= indent:
                break
            if following.strip():
                paths.append(following.strip())
        return paths
    return []


def cache_steps(path: str) -> list[tuple[str, str]]:
    """(workflow, step) for every actions/cache step of a workflow that caches path."""
    found = []
    for workflow in sorted(WORKFLOWS.glob("*.yml")):
        for step in workflow_steps(workflow.read_text(encoding="utf-8")):
            if step_uses(step).startswith("actions/cache") and path in cache_path(step):
                found.append((workflow.name, step))
    return found


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


class ToolchainCacheTest(unittest.TestCase):
    def test_arduino_cli_is_pinned(self) -> None:
        setups = 0
        for path in sorted(WORKFLOWS.glob("*.yml")):
            text = path.read_text(encoding="utf-8")
            for step in workflow_steps(text):
                if not step_uses(step).startswith(SETUP_CLI):
                    continue
                setups += 1
                self.assertIn(CLI_VERSION_LINE, step_lines(step, "version:"), path.name)
                self.assertEqual(
                    env_values(text, "ARDUINO_CLI_VERSION"), [ARDUINO_CLI_VERSION], path.name
                )
        self.assertGreaterEqual(setups, len(PINNED_WORKFLOWS))
        for name in PINNED_WORKFLOWS:
            text = (WORKFLOWS / name).read_text(encoding="utf-8")
            self.assertEqual(env_values(text, "ARDUINO_CLI_VERSION"), [ARDUINO_CLI_VERSION], name)

    def test_core_version_is_the_same(self) -> None:
        for name in PINNED_WORKFLOWS:
            text = (WORKFLOWS / name).read_text(encoding="utf-8")
            self.assertEqual(env_values(text, "STM32_CORE_VERSION"), [STM32_CORE_VERSION], name)

    def test_arduino15_cache_keys_match(self) -> None:
        steps = cache_steps("~/.arduino15")
        workflows = [workflow for workflow, _ in steps]
        for name in PINNED_WORKFLOWS:
            self.assertIn(name, workflows, f"{name}: no ~/.arduino15 cache")
        # Same key and same paths: the paths are part of the cache version, a
        # restore with other paths never finds the saved cache.
        keys = {(tuple(step_lines(step, "key:")), tuple(cache_path(step))) for _, step in steps}
        self.assertEqual(len(keys), 1, keys)
        ((key_lines, _),) = keys
        self.assertEqual(len(key_lines), 1, key_lines)
        self.assertTrue(key_lines[0].startswith("key: arduino15-"), key_lines)
        for workflow, step in steps:
            expected = CACHE_SAVE if workflow == SAVING_WORKFLOW else CACHE_RESTORE
            self.assertTrue(step_uses(step).startswith(expected), f"{workflow}: {step_uses(step)}")
        self.assertEqual(workflows.count(SAVING_WORKFLOW), 1, "one save in ci.yml")

    def test_platformio_cache_is_saved_once(self) -> None:
        steps = cache_steps("~/.platformio")
        saves = [workflow for workflow, step in steps if step_uses(step).startswith(CACHE_SAVE)]
        self.assertEqual(saves, [SAVING_WORKFLOW], "only the caches job of ci.yml saves")
        restores = [w for w, step in steps if step_uses(step).startswith(CACHE_RESTORE)]
        self.assertEqual(len(restores) + len(saves), len(steps), "actions/cache/save is not used")
        self.assertTrue(restores, "no job restores ~/.platformio")
        keys = {tuple(step_lines(step, "key:")) for _, step in steps}
        self.assertEqual(len(keys), 1, keys)
        ((key,),) = keys
        self.assertTrue(key.startswith("key: pio-v2-"), key)


if __name__ == "__main__":
    unittest.main()
