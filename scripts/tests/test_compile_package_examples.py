# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for scripts/compile-package-examples.sh, with a fake arduino-cli."""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "compile-package-examples.sh"

FAKE_ARDUINO_CLI = """\
#!/usr/bin/env bash
set -eu
if [ "$1 $2 $3" = "config get directories.user" ]; then
    echo "$FAKE_USER_DIR"
    exit 0
fi
if [ "$1" = "compile" ]; then
    out=""
    prev=""
    for arg in "$@"; do
        [ "$prev" = "--output-dir" ] && out="$arg"
        prev="$arg"
    done
    sketch="${!#}"
    echo "$sketch" >>"$FAKE_CALLS_LOG"
    if [ -n "$out" ]; then
        echo bin >"$out/$(basename "$sketch").ino.bin"
    fi
    exit 0
fi
exit 1
"""


class CompilePackageExamplesTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.user = self.tmp / "user"
        self.calls = self.tmp / "appels.log"
        self.calls.touch()

        bin_dir = self.tmp / "bin"
        bin_dir.mkdir()
        cli = bin_dir / "arduino-cli"
        cli.write_text(FAKE_ARDUINO_CLI, encoding="utf-8")
        cli.chmod(0o755)
        self.bin_dir = bin_dir

        self.pkg = self.tmp / "pkg"
        (self.pkg / "library.properties").parent.mkdir()
        (self.pkg / "library.properties").write_text("name=STeaMi\n", encoding="utf-8")
        for component, example in (("A", "x"), ("WSEN-HIDS", "y")):
            sketch = self.pkg / "examples" / component / example
            sketch.mkdir(parents=True)
            (sketch / f"{example}.ino").write_text("void setup() {}\n", encoding="utf-8")

    def run_script(self, *args, only_lines=None):
        args = list(args)
        if only_lines is not None:
            only = self.tmp / "exemples.txt"
            only.write_text("\n".join(only_lines) + "\n", encoding="utf-8")
            args += ["--only", str(only)]
        env = dict(os.environ)
        env["PATH"] = f"{self.bin_dir}:{env['PATH']}"
        env["FAKE_USER_DIR"] = str(self.user)
        env["FAKE_CALLS_LOG"] = str(self.calls)
        result = subprocess.run(
            ["bash", str(SCRIPT), str(self.pkg), *args],
            capture_output=True,
            text=True,
            env=env,
            check=False,
        )
        appels = self.calls.read_text(encoding="utf-8").split()
        return result, appels

    def test_only_compile_la_liste(self):
        result, appels = self.run_script(only_lines=["", "lib/wsen-hids/examples/y", ""])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Total: 1 | Passed: 1 | Failed: 0", result.stdout)
        self.assertEqual(appels, [f"{self.user}/libraries/STeaMi/examples/WSEN-HIDS/y"])

    def test_sans_only_compile_tout(self):
        result, appels = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Total: 2 | Passed: 2 | Failed: 0", result.stdout)
        self.assertEqual(len(appels), 2)

    def test_only_refuse_un_exemple_absent(self):
        result, appels = self.run_script(only_lines=["lib/a/examples/absent"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("lib/a/examples/absent", result.stderr)
        self.assertEqual(appels, [])

    def test_only_refuse_une_ligne_mal_formee(self):
        result, appels = self.run_script(only_lines=["lib/a/x"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("lib/a/x", result.stderr)
        self.assertEqual(appels, [])

    def test_output_dir_et_only_ensemble(self):
        out = self.tmp / "bins"
        result, appels = self.run_script(
            "--output-dir", str(out), only_lines=["lib/wsen-hids/examples/y"]
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(appels, [f"{self.user}/libraries/STeaMi/examples/WSEN-HIDS/y"])
        self.assertTrue((out / "WSEN-HIDS" / "y.bin").is_file())
        self.assertFalse((out / "A").exists())

    def test_options_dans_n_importe_quel_ordre(self):
        out = self.tmp / "bins"
        only = self.tmp / "exemples.txt"
        only.write_text("lib/a/examples/x\n", encoding="utf-8")
        env = dict(os.environ)
        env["PATH"] = f"{self.bin_dir}:{env['PATH']}"
        env["FAKE_USER_DIR"] = str(self.user)
        env["FAKE_CALLS_LOG"] = str(self.calls)
        result = subprocess.run(
            ["bash", str(SCRIPT), str(self.pkg), "--only", str(only), "--output-dir", str(out)],
            capture_output=True,
            text=True,
            env=env,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((out / "A" / "x.bin").is_file())


if __name__ == "__main__":
    unittest.main()
