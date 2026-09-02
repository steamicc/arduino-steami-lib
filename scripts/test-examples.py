#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile Arduino examples and report a consolidated test summary.

Usage:

    python3 scripts/test-examples.py
    python3 scripts/test-examples.py --driver hts221
    python3 scripts/test-examples.py --example hts221/dew_point
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
LIB_ROOT = ROOT / "lib"
PIO = ROOT / ".venv" / "bin" / "pio"

@dataclass
class ExampleResult:
    name: str
    passed: bool
    output: str


def discover_examples() -> list[str]:
    examples: list[str] = []

    for driver_dir in sorted(LIB_ROOT.iterdir()):
        examples_dir = driver_dir / "examples"

        if not examples_dir.is_dir():
            continue

        for example_dir in sorted(examples_dir.iterdir()):
            if not example_dir.is_dir():
                continue

            ino = example_dir / f"{example_dir.name}.ino"

            if ino.is_file():
                examples.append(f"{driver_dir.name}/{example_dir.name}")

    return examples


def example_path(name: str) -> Path:
    try:
        driver, example = name.split("/", 1)
    except ValueError:
        raise ValueError(
            f"Invalid example '{name}'. Expected <driver>/<example>."
        ) from None

    return LIB_ROOT / driver / "examples" / example


def compile_example(name: str) -> ExampleResult:
    src_dir = example_path(name)

    env = dict(os.environ)
    env["PLATFORMIO_PLATFORMIO_SRC_DIR"] = str(src_dir)

    result = subprocess.run(
        [str(PIO), "run", "-e", "steami"],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
    )

    output = result.stdout

    if result.stderr:
        output += result.stderr

    return ExampleResult(
        name=name,
        passed=result.returncode == 0,
        output=output,
    )


def select_examples(
    examples: list[str],
    driver: str | None,
    example: str | None,
) -> list[str]:
    if example:
        if example not in examples:
            raise ValueError(f"Example '{example}' not found.")

        return [example]

    if driver:
        selected = [
            name
            for name in examples
            if name.startswith(f"{driver}/")
        ]

        if not selected:
            raise ValueError(
                f"Driver '{driver}' has no examples."
            )

        return selected

    return examples


def print_summary(results: list[ExampleResult]) -> None:
    passed = sum(result.passed for result in results)
    failed = len(results) - passed

    print()
    print("=" * 72)
    print("Example validation summary")
    print("=" * 72)

    for result in results:
        status = "PASS" if result.passed else "FAIL"
        print(f"  [{status}] {result.name}")

    print("=" * 72)
    print(
        f"Total: {len(results)} | "
        f"Passed: {passed} | "
        f"Failed: {failed}"
    )
    print("=" * 72)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile Arduino examples."
    )
    group = parser.add_mutually_exclusive_group()

    group.add_argument(
        "--driver",
        help="Compile all examples belonging to one driver",
    )
    group.add_argument(
        "--example",
        help="Compile one example as <driver>/<example>",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Show full PlatformIO build output for every example",
    )

    args = parser.parse_args()

    if not PIO.exists():
        print(
            "Error: .venv/bin/pio not found. Run 'make setup' first.",
            file=sys.stderr,
        )
        return 1

    examples = discover_examples()

    try:
        selected = select_examples(
            examples,
            args.driver,
            args.example,
        )
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    if not selected:
        print("Error: no Arduino examples found.", file=sys.stderr)
        return 1

    results: list[ExampleResult] = []

    for index, name in enumerate(selected, start=1):
        print(
            f"[{index}/{len(selected)}] Testing {name}...",
            end="",
            flush=True,
        )

        result = compile_example(name)
        results.append(result)

        if result.passed:
            print(" PASS")
        else:
            print(" FAIL")

        if args.verbose or not result.passed:
            print()
            print("-" * 72)
            print(f"Build output: {name}")
            print("-" * 72)
            print(result.output.rstrip())
            print("-" * 72)
            print()

    print_summary(results)

    return 0 if all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
