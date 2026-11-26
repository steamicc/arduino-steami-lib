#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble the per-driver libraries into one STeaMi Arduino library.

The output follows the Arduino Library Manager layout: a root
library.properties, a flat src/ and examples grouped by component.

Usage:

    python3 scripts/build-arduino-package.py --version 1.2.3 --out build/arduino-package
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path


EXCLUDED_DRIVERS = frozenset()

# Directory holding examples but no driver sources.
EXAMPLES_ONLY = ("ble",)

SEMVER = re.compile(r"^\d+\.\d+\.\d+$")

LIBRARY_PROPERTIES = """\
name=STeaMi
version={version}
author=STeaMi contributors
maintainer=STeaMi contributors
sentence=Drivers for the sensors, buttons, battery gauge and display of the STeaMi board.
paragraph=Accelerometer and gyroscope (ISM330DL), magnetometer (LIS2MDL), distance (VL53L1X), D-PAD (MCP23009E), battery gauge (BQ27441), pressure, humidity and light sensors and OLED display of the V1a/V1b boards, and the BME280 pressure, humidity and temperature sensor and round colour display (GC9A01) of the V1c board. Detects the board release (V1a, V1b or V1c) and shares the sensor drivers (STeaMi Board). Select the "STeaMi Board" in the STM32 MCU based boards package.
category=Sensors
url=https://github.com/steamicc/arduino-steami-lib
architectures=stm32
depends=STM32duinoBLE
includes=STeaMi.h
"""


class PackageError(Exception):
    """The package cannot be built from the current sources."""


def _sources(driver: Path) -> list[Path]:
    src = driver / "src"
    if not src.is_dir():
        return []
    return sorted(
        p for p in src.iterdir() if p.is_file() and p.suffix in (".h", ".cpp")
    )


def _drivers(lib_root: Path, exclude: frozenset[str]) -> list[Path]:
    return [
        d
        for d in sorted(lib_root.iterdir())
        if d.is_dir() and d.name not in exclude and _sources(d)
    ]


def _copy_sources(drivers: list[Path], out_dir: Path) -> list[str]:
    src_out = out_dir / "src"
    src_out.mkdir(parents=True)
    origin: dict[str, Path] = {}
    for driver in drivers:
        for source in _sources(driver):
            key = source.name.lower()
            if key == "steami.h":
                raise PackageError(
                    f"{source.name} in {driver.name} collides with the generated"
                    " umbrella header STeaMi.h (file names are compared ignoring case)"
                )
            if key in origin:
                raise PackageError(
                    f"{source.name} exists in both {origin[key].parent.parent.name}"
                    f" and {driver.name} (file names are compared ignoring case)"
                )
            origin[key] = source
            shutil.copy2(source, src_out / source.name)
    return sorted(p.name for p in origin.values())


def _write_umbrella(out_dir: Path, file_names: list[str]) -> None:
    headers = [
        n for n in file_names if n.endswith(".h") and not n.endswith("_const.h")
    ]
    lines = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "#pragma once",
        "",
        "// Umbrella header: includes every STeaMi driver.",
        *(f'#include "{h}"' for h in headers),
        "",
    ]
    (out_dir / "src" / "STeaMi.h").write_text("\n".join(lines), encoding="utf-8")


def _copy_examples(example_sources: list[Path], out_dir: Path) -> None:
    for driver in example_sources:
        examples = driver / "examples"
        if not examples.is_dir():
            continue
        component = driver.name.upper()
        for example in sorted(p for p in examples.iterdir() if p.is_dir()):
            if not (example / f"{example.name}.ino").is_file():
                raise PackageError(
                    f"{driver.name}: example folder {example.name} must contain"
                    f" {example.name}.ino"
                )
            shutil.copytree(example, out_dir / "examples" / component / example.name)


def _write_keywords(drivers: list[Path], out_dir: Path) -> None:
    parts = [
        (d / "keywords.txt").read_text(encoding="utf-8").rstrip("\n")
        for d in drivers
        if (d / "keywords.txt").is_file()
    ]
    (out_dir / "keywords.txt").write_text("\n".join(parts) + "\n", encoding="utf-8")


def build_package(
    repo_root: Path,
    out_dir: Path,
    version: str,
    exclude: frozenset[str] = EXCLUDED_DRIVERS,
) -> None:
    """Build the STeaMi package for ``version`` into ``out_dir`` (rebuilt)."""
    if not SEMVER.match(version):
        raise PackageError(f"version must look like X.Y.Z, got {version!r}")

    lib_root = repo_root / "lib"
    drivers = _drivers(lib_root, exclude)

    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    file_names = _copy_sources(drivers, out_dir)
    _write_umbrella(out_dir, file_names)

    example_sources = list(drivers)
    for name in EXAMPLES_ONLY:
        extra = lib_root / name
        if extra.is_dir() and extra not in example_sources:
            example_sources.append(extra)
    _copy_examples(sorted(example_sources), out_dir)

    _write_keywords(drivers, out_dir)
    (out_dir / "library.properties").write_text(
        LIBRARY_PROPERTIES.format(version=version), encoding="utf-8"
    )
    shutil.copy2(repo_root / "LICENSE", out_dir / "LICENSE")
    readme = (repo_root / "scripts" / "arduino-package" / "README.md").read_text(
        encoding="utf-8"
    )
    (out_dir / "README.md").write_text(readme.format(version=version), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--version", required=True, help="package version X.Y.Z")
    parser.add_argument("--out", required=True, type=Path, help="output directory")
    args = parser.parse_args(argv)

    root = Path(__file__).resolve().parent.parent
    try:
        build_package(root, args.out, args.version)
    except PackageError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
