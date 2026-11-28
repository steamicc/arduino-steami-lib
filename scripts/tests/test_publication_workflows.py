# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that the publication compiles the examples once.

publish-arduino-package.yml calls arduino-package-check.yml with
`binaires: true`: the check compiles every example with --output-dir and
uploads the .bin files as the `exemples-bin` artifact. examples-archive.yml,
called with `artefacts: true`, downloads that artifact and the package instead
of installing the toolchain and compiling again. A manual run of
examples-archive.yml (workflow_dispatch, no `artefacts` input) still compiles
everything itself.
"""

from __future__ import annotations

import importlib.util
import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"
CHECK = "arduino-package-check.yml"
ARCHIVE = "examples-archive.yml"
PUBLISH = "publish-arduino-package.yml"

SKIPPED = "if: ${{ !inputs.artefacts }}"
SKIPPED_INSTALL = "if: ${{ !inputs.artefacts && steps.arduino15.outputs.cache-hit != 'true' }}"
DOWNLOADED = "if: ${{ inputs.artefacts }}"


def load_helpers():
    spec = importlib.util.spec_from_file_location(
        "workflow_versions", HERE / "test_workflow_versions.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


wv = load_helpers()


def read(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8")


def named_steps(text: str) -> dict[str, str]:
    """The steps of a workflow that start with `- name:`, by name."""
    steps = {}
    for step in wv.workflow_steps(text):
        match = re.match(r"^\s*-\s+name:\s*(.+?)\s*$", step.splitlines()[0])
        if match:
            steps[match.group(1)] = step
    return steps


def block(text: str, key: str, indent: int) -> str:
    """The lines under `key:` written at the given indent (empty if absent)."""
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if line == " " * indent + key + ":" or line.startswith(" " * indent + key + ": "):
            found = []
            for following in lines[i + 1 :]:
                if following.strip() and len(following) - len(following.lstrip()) <= indent:
                    break
                found.append(following)
            return "\n".join(found)
    return ""


def step_if(step: str) -> str:
    """The `if:` line of a step (stripped), empty if the step always runs."""
    lines = wv.step_lines(step, "if:")
    return lines[0] if lines else ""


def run_script(step: str) -> str:
    """The `run: |` script of a step, dedented."""
    lines = step.splitlines()
    for i, line in enumerate(lines):
        match = re.match(r"^(\s*)run:\s*\|\s*$", line)
        if not match:
            continue
        indent = len(match.group(1))
        body = []
        for following in lines[i + 1 :]:
            if following.strip() and len(following) - len(following.lstrip()) <= indent:
                break
            body.append(following)
        return textwrap.dedent("\n".join(body)) + "\n"
    raise AssertionError(f"no run: | block in {lines[0]}")


def with_value(step: str, key: str) -> str:
    values = [line.split(":", 1)[1].strip() for line in wv.step_lines(step, key + ":")]
    return values[0] if values else ""


class CheckWorkflowTest(unittest.TestCase):
    def setUp(self) -> None:
        self.text = read(CHECK)
        self.steps = named_steps(self.text)

    def test_binaires_input(self) -> None:
        inputs = block(block(self.text, "workflow_call", 2), "inputs", 4)
        binaires = block(inputs, "binaires", 6)
        self.assertIn("type: boolean", binaires)
        self.assertIn("default: false", binaires)
        self.assertIn("required: false", binaires)

    def test_upload_of_the_compiled_examples(self) -> None:
        step = self.steps["Upload the compiled examples"]
        self.assertTrue(wv.step_uses(step).startswith("actions/upload-artifact@v4"))
        self.assertEqual(step_if(step), "if: ${{ inputs.binaires }}")
        self.assertEqual(with_value(step, "name"), "exemples-bin")
        self.assertEqual(with_value(step, "path"), "build/exemples-bin")
        self.assertEqual(with_value(step, "if-no-files-found"), "error")

    def test_inputs_are_checked_before_any_work(self) -> None:
        names = list(self.steps)
        self.assertIn("Check the inputs", names)
        self.assertEqual(step_if(self.steps["Check the inputs"]), "")
        self.assertLess(names.index("Check the inputs"), names.index("Build the package"))


@unittest.skipUnless(shutil.which("jq") and shutil.which("bash"), "needs bash and jq")
class CheckWorkflowScriptsTest(unittest.TestCase):
    """Run the scripts of the check steps, with a stub compile script."""

    def setUp(self) -> None:
        self.steps = named_steps(read(CHECK))
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        stub = self.tmp / "scripts" / "compile-package-examples.sh"
        stub.parent.mkdir()
        stub.write_text('#!/usr/bin/env bash\necho "$@" > "$RUNNER_TEMP/args"\n')
        stub.chmod(0o755)

    def run_step(self, name: str, binaires: str, exemples: str) -> subprocess.CompletedProcess:
        env = dict(os.environ, BINAIRES=binaires, EXEMPLES=exemples, RUNNER_TEMP=str(self.tmp))
        return subprocess.run(
            ["bash", "-e", "-c", run_script(self.steps[name])],
            cwd=self.tmp,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

    def compile_args(self, binaires: str, exemples: str) -> str:
        result = self.run_step("Compile every example", binaires, exemples)
        self.assertEqual(result.returncode, 0, result.stderr)
        return (self.tmp / "args").read_text().strip()

    def test_compile_every_example(self) -> None:
        self.assertEqual(self.compile_args("false", ""), "build/arduino-package")

    def test_compile_every_example_with_binaries(self) -> None:
        self.assertEqual(
            self.compile_args("true", ""), "build/arduino-package --output-dir build/exemples-bin"
        )

    def test_compile_listed_examples(self) -> None:
        only = self.tmp / "exemples.txt"
        args = self.compile_args("false", '["lib/bq27441/examples/battery_logger"]')
        self.assertEqual(args, f"build/arduino-package --only {only}")
        self.assertEqual(only.read_text(), "lib/bq27441/examples/battery_logger\n")

    def test_guard_accepts(self) -> None:
        for binaires, exemples in (("false", ""), ("false", "[]"), ("true", "")):
            with self.subTest(binaires=binaires, exemples=exemples):
                result = self.run_step("Check the inputs", binaires, exemples)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_guard_rejects_binaries_of_some_examples(self) -> None:
        # The archive needs a .bin for every example: binaires=true with no
        # example ("[]") or a list would upload no or a partial artifact.
        for exemples in ("[]", '["lib/bq27441/examples/battery_logger"]'):
            with self.subTest(exemples=exemples):
                result = self.run_step("Check the inputs", "true", exemples)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("::error::", result.stdout)


class ArchiveWorkflowTest(unittest.TestCase):
    def setUp(self) -> None:
        self.text = read(ARCHIVE)
        self.steps = named_steps(self.text)

    def test_artefacts_input_only_when_called(self) -> None:
        called = block(block(self.text, "workflow_call", 2), "inputs", 4)
        artefacts = block(called, "artefacts", 6)
        self.assertIn("type: boolean", artefacts)
        self.assertIn("default: false", artefacts)
        self.assertIn("required: false", artefacts)
        # A manual run always compiles (repair path).
        dispatched = block(block(self.text, "workflow_dispatch", 2), "inputs", 4)
        self.assertTrue(dispatched.strip(), "workflow_dispatch has inputs")
        self.assertEqual(block(dispatched, "artefacts", 6), "")

    def test_compilation_steps_are_skipped_with_artefacts(self) -> None:
        for name in (
            "Checkout the tag",
            "Build the package",
            "Restore the Arduino toolchain",
            "Install Arduino CLI",
            "Compile every example",
        ):
            with self.subTest(step=name):
                self.assertEqual(step_if(self.steps[name]), SKIPPED)
        for name in ("Install the STM32 core", "Install library dependencies"):
            with self.subTest(step=name):
                self.assertEqual(step_if(self.steps[name]), SKIPPED_INSTALL)

    def test_artefacts_are_downloaded(self) -> None:
        names = list(self.steps)
        for name, artifact, path in (
            ("Download the package", "arduino-package", "build/arduino-package"),
            ("Download the compiled examples", "exemples-bin", "build/exemples-bin"),
        ):
            with self.subTest(step=name):
                step = self.steps[name]
                self.assertTrue(wv.step_uses(step).startswith("actions/download-artifact@v4"))
                self.assertEqual(step_if(step), DOWNLOADED)
                self.assertEqual(with_value(step, "name"), artifact)
                self.assertEqual(with_value(step, "path"), path)
                self.assertLess(names.index(name), names.index("Build the archive"))

    def test_archive_is_built_in_both_paths(self) -> None:
        for name in ("Checkout the scripts", "Build the archive"):
            with self.subTest(step=name):
                self.assertEqual(step_if(self.steps[name]), "")
        script = self.steps["Build the archive"]
        self.assertIn("--pkg build/arduino-package --bins build/exemples-bin", script)
        compile_step = self.steps["Compile every example"]
        self.assertIn("--output-dir build/exemples-bin", compile_step)

    def test_artifact_names_match_the_check(self) -> None:
        check = named_steps(read(CHECK))
        self.assertEqual(with_value(check["Upload the package"], "name"), "arduino-package")
        self.assertEqual(with_value(check["Upload the compiled examples"], "name"), "exemples-bin")


class PublishWorkflowTest(unittest.TestCase):
    def setUp(self) -> None:
        self.jobs = block(read(PUBLISH), "jobs", 0)

    def test_check_uploads_the_binaries(self) -> None:
        job = block(self.jobs, "check", 2)
        self.assertIn(f"uses: ./.github/workflows/{CHECK}", job)
        self.assertEqual(block(job, "with", 4).split("\n").count("      binaires: true"), 1)

    def test_archive_reuses_them(self) -> None:
        job = block(self.jobs, "examples-archive", 2)
        self.assertIn(f"uses: ./.github/workflows/{ARCHIVE}", job)
        self.assertEqual(block(job, "with", 4).split("\n").count("      artefacts: true"), 1)
        # The artifacts come from the check job of the same run.
        needs = wv.step_lines(job, "needs:")
        self.assertEqual(needs, ["needs: [resolve, publish]"])
        self.assertIn("needs: [resolve, check]", block(self.jobs, "publish", 2))


if __name__ == "__main__":
    unittest.main()
