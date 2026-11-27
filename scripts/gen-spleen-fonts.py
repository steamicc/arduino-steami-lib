#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the Spleen font tables of the 240 px display from the Spleen release.

Usage:

    python3 scripts/gen-spleen-fonts.py [--tarball PATH] [--cache DIR] [--out PATH]
                                        [--micropython PATH] [--commit SHA]

The release tarball is downloaded once into .spleen-cache/ (or read from
--tarball) and checked against its sha256, then each BDF file against its own.
The script writes lib/steami_board/src/SteamiFontSpleen.cpp, which is
committed: regenerate it with this script, never edit it by hand. With
--micropython, it also writes the MicroPython module spleen.py from the same
faces; its header names the repository commit (--commit, else
`git rev-parse HEAD`).

Faces: 6x12 and 8x16 hold the 128 glyph codes 32..159 (32..126 ASCII, 127 the
degree sign, 128..158 the accented letters of EXTRA, 159 U+0178); 12x24 and
16x32 hold the 15 value glyphs of VALUE_KEY. The script fails when the release
breaks an assumption of the drawing code (cell box, advance, the bold and
narrow columns of the value glyphs, a missing glyph).
"""

from __future__ import annotations

import argparse
import hashlib
import io
import subprocess
import sys
import tarfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GENERATOR = "scripts/gen-spleen-fonts.py"
DEFAULT_CACHE = ROOT / ".spleen-cache"
DEFAULT_OUT = ROOT / "lib" / "steami_board" / "src" / "SteamiFontSpleen.cpp"

VERSION = "2.1.0"
TARBALL = f"spleen-{VERSION}.tar.gz"
URL = f"https://github.com/fcambus/spleen/releases/download/{VERSION}/{TARBALL}"
TARBALL_SHA256 = "8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f"
BDF_SHA256: dict[str, str] = {
    "6x12": "f40e935bdd194976d30ef296920d5ccca29eb84bb7b3459a88fe7932fa8ba872",
    "8x16": "b2b05484ba4380c9c6854cc57637235a9fb039c65b5ba38a65289540f8fab7dc",
    "12x24": "eb690d1dd190e21ae31f800e63fe1188409e3d94a11da222196de0777ed8c9e8",
    "16x32": "3b6ae73ef1dd3c78bad873697c18b15df82d8e5c0258114fda401556e01d0e67",
}

VALUE_KEY = " +-.:0123456789"
# Codes 127..159: the degree sign, the 31 letters of kFolds (SteamiFont.cpp) in its order, U+0178.
EXTRA = "°ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜàâäçèéêëîïôöùûüÿŸ"
# The 8x8 replacement of each EXTRA character: the degree glyph itself, the base letters, 'Y'.
FOLD = "°AAACEEEEIIOOUUUaaaceeeeiioouuuyY"
FULL_CODES: tuple[int, ...] = tuple(range(32, 127)) + tuple(ord(c) for c in EXTRA)

# (face, value set): 6x12 and 8x16 full sets, 12x24 and 16x32 value sets.
FACES: tuple[tuple[str, bool], ...] = (
    ("6x12", False),
    ("8x16", False),
    ("12x24", True),
    ("16x32", True),
)
# The LABEL face: never narrow, and the only face allowed to lack U+0178 (it draws its Y there).
LABEL_FACE = "6x12"
Y_DIAERESIS = 0x178
NARROW = (ord("."), ord(":"))

NOTICE = """\
Copyright (c) 2018-2024, Frederic Cambus
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
"""

FORMAT = """\
Format: one blob per face, rows top to bottom, (w + 7) / 8 bytes per row, most
significant bit = leftmost column (MicroPython framebuf.MONO_HLSB); glyph i is
the h * ((w + 7) / 8) bytes at i * h * ((w + 7) / 8). The 6x12 and 8x16 faces
hold the 128 glyph codes 32..159, indexed by code - 32: 32..126 ASCII, 127 the
degree sign, 128..158 the accented letters of EXTRA, 159 U+0178 (a font without
U+0178 holds its Y there). The 12x24 and 16x32 faces hold the 15 value glyphs
in the order of the key " +-.:0123456789"."""

MICROPYTHON_CODES = """\
Code of a character ch: o = ord(ch); 32 <= o <= 126 gives o; else
i = EXTRA.find(ch) >= 0 gives 127 + i; else ord("?"). In a value set the glyph
index is VALUE_KEY.find(ch). FOLD gives the 8x8 replacement of codes 127..159.
Each face is (w, h, ascent, key, data): key is None for a full set (index =
code - 32) and VALUE_KEY for a value set."""


class FontError(Exception):
    pass


@dataclass(frozen=True)
class Glyph:
    dwidth: int
    bbx: tuple[int, int, int, int]
    rows: tuple[int, ...]  # MSB = leftmost column, byte-padded


@dataclass(frozen=True)
class Bdf:
    w: int
    h: int
    ascent: int
    descent: int
    glyphs: dict[int, Glyph]


@dataclass(frozen=True)
class Face:
    name: str
    w: int
    h: int
    ascent: int
    key: str | None
    data: bytes


def _ints(fields: list[str], count: int, line: str) -> list[int]:
    try:
        values = [int(v) for v in fields[1 : 1 + count]]
    except ValueError:
        raise FontError(f"BDF: bad line {line!r}") from None
    if len(values) != count:
        raise FontError(f"BDF: bad line {line!r}")
    return values


def parse_bdf(text: str) -> Bdf:
    """Read the font box and every encoded glyph of a BDF file."""
    box: list[int] | None = None
    glyphs: dict[int, Glyph] = {}
    code = -1
    dwidth = 0
    bbx: tuple[int, int, int, int] = (0, 0, 0, 0)
    rows: list[int] | None = None

    for line in text.splitlines():
        fields = line.split()
        if not fields:
            continue
        word = fields[0]
        if rows is not None:
            if word == "ENDCHAR":
                if code >= 0:
                    glyphs[code] = Glyph(dwidth, bbx, tuple(rows))
                rows = None
                continue
            try:
                rows.append(int(word, 16))
            except ValueError:
                raise FontError(f"BDF: bad bitmap row {line!r}") from None
        elif word == "FONTBOUNDINGBOX":
            box = _ints(fields, 4, line)
        elif word == "STARTCHAR":
            code, dwidth, bbx = -1, 0, (0, 0, 0, 0)
        elif word == "ENCODING":
            code = _ints(fields, 1, line)[0]
        elif word == "DWIDTH":
            dwidth = _ints(fields, 1, line)[0]
        elif word == "BBX":
            a, b, c, d = _ints(fields, 4, line)
            bbx = (a, b, c, d)
        elif word == "BITMAP":
            rows = []

    if box is None:
        raise FontError("BDF: no FONTBOUNDINGBOX")
    w, h, _, y = box
    return Bdf(w=w, h=h, ascent=h + y, descent=-y, glyphs=glyphs)


def _columns(row: int, w: int) -> list[int]:
    top = 8 * ((w + 7) // 8) - 1
    return [c for c in range(w) if row >> (top - c) & 1]


def build_face(name: str, bdf: Bdf, value_set: bool) -> Face:
    """Pack the glyphs of one face (full set or value set) and check the drawing assumptions."""
    w, h = bdf.w, bdf.h
    stride = (w + 7) // 8
    cell = (w, h, 0, -bdf.descent)
    codes = tuple(ord(c) for c in VALUE_KEY) if value_set else FULL_CODES
    bold = value_set
    narrow = name != LABEL_FACE
    data = bytearray()

    for cp in codes:
        where = f"{name} U+{cp:04X}"
        glyph = bdf.glyphs.get(cp)
        if glyph is None and cp == Y_DIAERESIS and name == LABEL_FACE:
            glyph = bdf.glyphs.get(ord("Y"))
            where = f"{name} U+{cp:04X} (drawn as U+0059)"
        if glyph is None:
            raise FontError(f"{where}: missing glyph")
        if glyph.bbx != cell:
            raise FontError(f"{where}: BBX {glyph.bbx} is not the cell {cell}")
        if glyph.dwidth != w:
            raise FontError(f"{where}: DWIDTH {glyph.dwidth} is not the cell width {w}")
        if len(glyph.rows) != h:
            raise FontError(f"{where}: {len(glyph.rows)} bitmap rows, not {h}")
        if any(row >> (8 * stride) or row & ((1 << (8 * stride - w)) - 1) for row in glyph.rows):
            raise FontError(f"{where}: a row inks outside columns 0..{w - 1}")

        inked = sorted({c for row in glyph.rows for c in _columns(row, w)})
        if bold and inked and (inked[0] == 0 or inked[-1] == w - 1):
            raise FontError(f"{where}: a bold value glyph inks column 0 or {w - 1}")
        if narrow and cp in NARROW and inked:
            lo, hi = w // 4, w // 4 + w // 2
            last = inked[-1] + (1 if bold else 0)
            if inked[0] < lo or last >= hi:
                extra = " with its bold column" if bold else ""
                raise FontError(f"{where}: the ink{extra} leaves the narrow window [{lo}, {hi})")

        for row in glyph.rows:
            data += row.to_bytes(stride, "big")

    key = VALUE_KEY if value_set else None
    return Face(name=name, w=w, h=h, ascent=bdf.ascent, key=key, data=bytes(data))


def build_faces(bdfs: dict[str, str]) -> list[Face]:
    """The four faces, in the order 6x12, 8x16, 12x24, 16x32."""
    faces = []
    for name, value_set in FACES:
        if name not in bdfs:
            raise FontError(f"{name}: no BDF")
        bdf = parse_bdf(bdfs[name])
        if f"{bdf.w}x{bdf.h}" != name:
            raise FontError(f"{name}: the BDF cell is {bdf.w}x{bdf.h}")
        faces.append(build_face(name, bdf, value_set))
    return faces


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_release(
    tarball: Path,
    tarball_sha256: str = TARBALL_SHA256,
    bdf_sha256: dict[str, str] = BDF_SHA256,
) -> tuple[dict[str, str], str]:
    """The four BDF texts and the LICENSE of the release, each checked against its sha256."""
    try:
        blob = Path(tarball).read_bytes()
    except OSError as error:
        raise FontError(f"{tarball}: {error.strerror}") from None
    if _sha256(blob) != tarball_sha256:
        raise FontError(f"{tarball}: sha256 {_sha256(blob)}, expected {tarball_sha256}")

    def member(tar: tarfile.TarFile, name: str) -> bytes:
        path = f"spleen-{VERSION}/{name}"
        try:
            handle = tar.extractfile(path)
        except KeyError:
            handle = None
        if handle is None:
            raise FontError(f"{tarball}: no file {path}")
        return handle.read()

    bdfs: dict[str, str] = {}
    try:
        with tarfile.open(fileobj=io.BytesIO(blob), mode="r:gz") as tar:
            for name, _ in FACES:
                data = member(tar, f"spleen-{name}.bdf")
                if _sha256(data) != bdf_sha256[name]:
                    raise FontError(
                        f"{name}: spleen-{name}.bdf sha256 {_sha256(data)}, "
                        f"expected {bdf_sha256[name]}"
                    )
                bdfs[name] = data.decode("utf-8")
            license_text = member(tar, "LICENSE").decode("utf-8")
    except (tarfile.TarError, OSError, UnicodeDecodeError) as error:
        raise FontError(f"{tarball}: {error}") from None
    return bdfs, license_text


def _glyph_label(face: Face, index: int) -> str:
    if face.key is None:
        return f"code {32 + index} U+{FULL_CODES[index]:04X}"
    return f"key {index} U+{ord(face.key[index]):04X}"


def _glyphs(face: Face) -> int:
    return len(face.data) // (face.h * ((face.w + 7) // 8))


def _array(face: Face) -> str:
    return f"kSpleen{face.name}"


def _header(prefix: str, generated: list[str]) -> list[str]:
    lines = [
        f"Spleen {VERSION} bitmap fonts by Frederic Cambus, from",
        URL,
        f"  {TARBALL}  sha256 {TARBALL_SHA256}",
        *(f"  {f'spleen-{name}.bdf':{len(TARBALL)}}  sha256 {BDF_SHA256[name]}"
          for name, _ in FACES),
        "",
        *generated,
        "",
        "Spleen is distributed under this notice (the LICENSE file of the release):",
        "",
        *NOTICE.splitlines(),
        "",
        *FORMAT.splitlines(),
    ]
    return [(f"{prefix} " + line).rstrip() for line in lines]


def render_cpp(faces: list[Face]) -> str:
    """The text of SteamiFontSpleen.cpp."""
    out = ["// SPDX-License-Identifier: BSD-2-Clause", "//"]
    out += _header("//", [f"Generated by {GENERATOR}: regenerate with this script, "
                          "never edit by hand."])
    out += ["", '#include "SteamiFont.h"', "", "namespace {", "", "// clang-format off"]
    for face in faces:
        stride = (face.w + 7) // 8
        out.append(f"const uint8_t {_array(face)}[{len(face.data)}] = {{")
        for index in range(_glyphs(face)):
            out.append(f"    // {_glyph_label(face, index)}")
            base = index * face.h * stride
            for r in range(face.h):
                row = face.data[base + r * stride : base + (r + 1) * stride]
                inked = set(_columns(int.from_bytes(row, "big"), face.w))
                art = "".join("#" if c in inked else "." for c in range(face.w))
                hex_bytes = " ".join(f"0x{b:02x}," for b in row)
                out.append(f"    {hex_bytes}  // {art}")
        out.append("};")
        out.append("")
    out += ["// clang-format on", "", "}  // namespace", ""]
    for face in faces:
        out.append(
            f"const SteamiFontFace STEAMI_SPLEEN_{face.name.upper()} = "
            f"{{{face.w}, {face.h}, {face.ascent}, {_glyphs(face)}, {_array(face)}}};"
        )
    return "\n".join(out) + "\n"


def render_micropython(faces: list[Face], commit: str) -> str:
    """The text of the MicroPython module spleen.py."""
    generated = [
        f"Generated by {GENERATOR} --micropython from steamicc/arduino-steami-lib",
        f"at commit {commit}.",
        "Regenerate with that script, never edit by hand.",
    ]
    out = ["# SPDX-License-Identifier: BSD-2-Clause", "#"]
    out += _header("#", generated)
    out += [("# " + line).rstrip() for line in ["", *MICROPYTHON_CODES.splitlines()]]
    out += [
        "",
        f'VERSION = "{VERSION}"',
        f'VALUE_KEY = "{VALUE_KEY}"',
        f'EXTRA = "{EXTRA}"',
        f'FOLD = "{FOLD}"',
    ]
    for face in faces:
        stride = (face.w + 7) // 8
        size = face.h * stride
        key = "None" if face.key is None else "VALUE_KEY"
        out += ["", f"F{face.name.upper()} = ({face.w}, {face.h}, {face.ascent}, {key}, ("]
        for index in range(_glyphs(face)):
            out.append(f"    # {_glyph_label(face, index)}")
            glyph = face.data[index * size : (index + 1) * size]
            for start in range(0, size, 16):
                chunk = "".join(f"\\x{b:02x}" for b in glyph[start : start + 16])
                out.append(f'    b"{chunk}"')
        out.append("))")
    return "\n".join(out) + "\n"


def _download(path: Path) -> None:
    request = urllib.request.Request(URL, headers={"User-Agent": "gen-spleen-fonts"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            blob = response.read()
    except OSError as error:
        raise FontError(f"{URL}: {error}") from None
    if _sha256(blob) != TARBALL_SHA256:
        raise FontError(f"{URL}: sha256 {_sha256(blob)}, expected {TARBALL_SHA256}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(blob)


def _head_commit() -> str:
    result = subprocess.run(
        ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise FontError("git rev-parse HEAD failed; pass --commit")
    return result.stdout.strip()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tarball", type=Path, help=f"read this {TARBALL} (no download)")
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE,
                        help="download cache (default: .spleen-cache/)")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT,
                        help="C++ output (default: lib/steami_board/src/SteamiFontSpleen.cpp)")
    parser.add_argument("--micropython", type=Path, help="also write spleen.py here")
    parser.add_argument("--commit", help="commit named in spleen.py (default: git rev-parse HEAD)")
    args = parser.parse_args(argv)

    try:
        tarball = args.tarball
        if tarball is None:
            tarball = args.cache / TARBALL
            if not tarball.is_file():
                _download(tarball)
        bdfs, license_text = read_release(tarball, TARBALL_SHA256, BDF_SHA256)
        if license_text != NOTICE:
            raise FontError(f"{tarball}: the LICENSE of the release is not NOTICE; review it")
        faces = build_faces(bdfs)
        cpp = render_cpp(faces)
        module = None
        if args.micropython is not None:
            module = render_micropython(faces, args.commit or _head_commit())
    except FontError as error:
        print(f"gen-spleen-fonts: {error}", file=sys.stderr)
        return 1

    with open(args.out, "w", encoding="ascii", newline="\n") as handle:
        handle.write(cpp)
    if module is not None:
        with open(args.micropython, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(module)
    return 0


if __name__ == "__main__":
    sys.exit(main())
