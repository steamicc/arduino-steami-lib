#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the footprint sketches and check their flash and RAM use.

Usage:

    python3 scripts/check-footprint.py

Each sketch is built with PlatformIO for the steami environment, then
`arm-none-eabi-size -A -d` measures the ELF. Flash counts every
flash-resident byte (PlatformIO's report is about 360 B lower: it counts
.text + .rodata + .data only). RAM is .data + .bss + .noinit.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PIO = ROOT / ".venv" / "bin" / "pio"
BOARD_JSON = ROOT / "boards" / "steami.json"
ELF = ROOT / ".pio" / "build" / "steami" / "firmware.elf"

SKETCHES: tuple[str, ...] = ("steami_board/Drawing", "steami_board/ScreenGauge")
FLASH_START = 0x08000000
FLASH_MARGIN = 65536
MIN_FREE_RAM = 143360


class FootprintError(Exception):
    pass


@dataclass(frozen=True)
class Footprint:
    flash: int
    ram: int


@dataclass(frozen=True)
class Limits:
    flash: int
    ram: int


def parse_sections(text: str) -> dict[str, tuple[int, int]]:
    """Map each section name to (size, addr) from `size -A -d` output."""
    sections: dict[str, tuple[int, int]] = {}

    for line in text.splitlines():
        parts = line.split()

        if len(parts) != 3 or parts[0] in ("section", "Total"):
            continue

        if not (parts[1].isdigit() and parts[2].isdigit()):
            continue

        sections[parts[0]] = (int(parts[1]), int(parts[2]))

    return sections


def measure(sections: dict[str, tuple[int, int]], flash_size: int) -> Footprint:
    for name in (".data", ".bss"):
        if name not in sections:
            raise FootprintError(f"section {name} missing from the size output")

    flash_end = FLASH_START + flash_size
    flash = sum(
        size for size, addr in sections.values() if FLASH_START <= addr < flash_end
    )
    flash += sections[".data"][0]
    ram = sum(sections[name][0] for name in (".data", ".bss", ".noinit") if name in sections)

    return Footprint(flash=flash, ram=ram)


def board_upload(board_json: Path) -> dict:
    return json.loads(board_json.read_text(encoding="utf-8"))["upload"]


def flash_size(board_json: Path) -> int:
    return board_upload(board_json)["maximum_size"]


def limits(board_json: Path) -> Limits:
    upload = board_upload(board_json)

    return Limits(
        flash=upload["maximum_size"] - FLASH_MARGIN,
        ram=upload["maximum_ram_size"] - MIN_FREE_RAM,
    )


def violations(name: str, fp: Footprint, lim: Limits) -> list[str]:
    messages: list[str] = []

    if fp.flash > lim.flash:
        messages.append(f"{name}: flash {fp.flash} B exceeds the limit of {lim.flash} B")

    if fp.ram > lim.ram:
        messages.append(f"{name}: RAM {fp.ram} B exceeds the limit of {lim.ram} B")

    return messages


def find_size_tool() -> str:
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR") or Path.home() / ".platformio")
    candidate = core / "packages" / "toolchain-gccarmnoneeabi" / "bin" / "arm-none-eabi-size"

    if candidate.is_file():
        return str(candidate)

    found = shutil.which("arm-none-eabi-size")

    if found is None:
        raise FootprintError("arm-none-eabi-size not found")

    return found


def main() -> int:
    argparse.ArgumentParser(description=__doc__.splitlines()[0]).parse_args()

    if not PIO.exists():
        print(
            "Error: .venv/bin/pio not found. Run 'make setup' first.",
            file=sys.stderr,
        )
        return 1

    lim = limits(BOARD_JSON)
    flash = flash_size(BOARD_JSON)
    failed = False

    try:
        size_tool = find_size_tool()
    except FootprintError as error:
        print(error, file=sys.stderr)
        return 1

    for sketch in SKETCHES:
        env = dict(os.environ)
        env["PLATFORMIO_SRC_DIR"] = str(ROOT / "lib" / sketch.replace("/", "/examples/"))
        build = subprocess.run(
            [str(PIO), "run", "-e", "steami"],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

        if build.returncode != 0:
            print(build.stdout + build.stderr)
            return 1

        size = subprocess.run(
            [size_tool, "-A", "-d", str(ELF)],
            capture_output=True,
            text=True,
            check=False,
        )

        if size.returncode != 0:
            print(size.stdout + size.stderr)
            return 1

        try:
            fp = measure(parse_sections(size.stdout), flash)
        except FootprintError as error:
            print(f"{sketch}: {error}", file=sys.stderr)
            return 1

        print(
            f"{sketch}: flash {fp.flash} B (limit {lim.flash}; all flash sections, "
            f"PlatformIO counts .text+.rodata+.data), "
            f"RAM {fp.ram} B (limit {lim.ram}, {lim.ram + MIN_FREE_RAM - fp.ram} B free)"
        )

        for message in violations(sketch, fp, lim):
            print(message, file=sys.stderr)
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
