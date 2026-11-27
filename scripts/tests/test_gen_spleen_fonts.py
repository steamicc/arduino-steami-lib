# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/gen-spleen-fonts.py."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import re
import sys
import tarfile
import tempfile
import unittest
import unittest.mock
from contextlib import redirect_stderr
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "gen-spleen-fonts.py"
FONT_CPP = ROOT / "lib" / "steami_board" / "src" / "SteamiFont.cpp"
TABLES = ROOT / "lib" / "steami_board" / "src" / "SteamiFontSpleen.cpp"
CACHED_TARBALL = ROOT / ".spleen-cache" / "spleen-2.1.0.tar.gz"

# (w, h, descent) of each face, as in Spleen 2.1.0.
SIZES = {"6x12": (6, 12, 3), "8x16": (8, 16, 4), "12x24": (12, 24, 5), "16x32": (16, 32, 6)}


def load_module():
    spec = importlib.util.spec_from_file_location("gen_spleen_fonts", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


MOD = load_module()


def make_bdf(w, h, descent, glyphs=None, skip=(), dwidth=None, bbx=None) -> str:
    """A BDF holding every code point of FULL_CODES except `skip`.

    `glyphs` maps a code point to its rows as "#"/"." strings (blank rows fill the rest);
    `dwidth` and `bbx` override one glyph as a (code point, value) pair.
    """
    glyphs = glyphs or {}
    stride = (w + 7) // 8
    codes = [cp for cp in MOD.FULL_CODES if cp not in skip]
    lines = [
        "STARTFONT 2.1",
        f"FONTBOUNDINGBOX {w} {h} 0 {-descent}",
        f"CHARS {len(codes)}",
    ]
    for cp in codes:
        rows = list(glyphs.get(cp, ()))
        rows += ["." * w] * (h - len(rows))
        dw = dwidth[1] if dwidth is not None and dwidth[0] == cp else w
        box = bbx[1] if bbx is not None and bbx[0] == cp else (w, h, 0, -descent)
        lines += [
            f"STARTCHAR U+{cp:04X}",
            f"ENCODING {cp}",
            "SWIDTH 500 0",
            f"DWIDTH {dw} 0",
            "BBX " + " ".join(str(v) for v in box),
            "BITMAP",
        ]
        for row in rows:
            bits = int(row.replace("#", "1").replace(".", "0"), 2) << (8 * stride - w)
            lines.append(f"{bits:0{2 * stride}X}")
        lines.append("ENDCHAR")
    lines.append("ENDFONT")
    return "\n".join(lines) + "\n"


def pattern(w, h, cp, value_set):
    """Distinct rows for code point cp that pass the generator's checks (inside columns 1..w-2;
    '.' and ':' on the column left of the centre, so the bold column stays in the window)."""
    if cp in (ord("."), ord(":")) and w % 4 == 0:
        col = w // 2 - 1
        dot = "." * col + "#" + "." * (w - col - 1)
        return [dot if (cp + r) % 3 else "." * w for r in range(h)]
    rows = []
    for r in range(h):
        col = 1 + (cp * 7 + r * 3) % (w - 2) if value_set or w % 4 == 0 else (cp + r) % w
        rows.append("." * col + "#" + "." * (w - col - 1))
    return rows


def synthetic_bdfs() -> dict[str, str]:
    out = {}
    for name, (w, h, descent) in SIZES.items():
        value_set = name in ("12x24", "16x32")
        glyphs = {cp: pattern(w, h, cp, value_set) for cp in MOD.FULL_CODES}
        out[name] = make_bdf(w, h, descent, glyphs)
    return out


def glyph_bytes(face, index):
    size = face.h * ((face.w + 7) // 8)
    return face.data[index * size : (index + 1) * size]


def cpp_arrays(text: str) -> dict[str, bytes]:
    """The bytes of each kSpleen array of a generated SteamiFontSpleen.cpp."""
    arrays = {}
    for name, body in re.findall(r"const uint8_t (kSpleen\w+)\[\d+\] = \{(.*?)\};", text, re.S):
        arrays[name] = bytes(int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    return arrays


def k_folds() -> list[tuple[int, str]]:
    source = FONT_CPP.read_text(encoding="utf-8")
    block = source.split("const Fold kFolds[] = {", 1)[1].split("};", 1)[0]
    return [(int(h, 16), c) for h, c in re.findall(r"\{(0x[0-9A-Fa-f]{2}), '(.)'\}", block)]


def comment_block(text: str) -> list[str]:
    """The header comment of a generated file, each line without its "// " or bare "//"."""
    out = []
    for line in text.splitlines():
        if line == "//":
            out.append("")
        elif line.startswith("// "):
            out.append(line[3:])
        else:
            break
    return out


def contains_run(lines: list[str], run: list[str]) -> bool:
    return any(lines[i : i + len(run)] == run for i in range(len(lines) - len(run) + 1))


def write_tarball(path: Path, bdfs: dict[str, str], license_text: str) -> None:
    with tarfile.open(path, "w:gz") as tar:
        members = {f"spleen-2.1.0/spleen-{name}.bdf": text for name, text in bdfs.items()}
        members["spleen-2.1.0/LICENSE"] = license_text
        for member, text in members.items():
            data = text.encode("utf-8")
            info = tarfile.TarInfo(member)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class GenSpleenFontsTest(unittest.TestCase):
    def test_packs_rows_msb_left(self) -> None:
        a = make_bdf(6, 12, 3, {ord("A"): ["#....#"]})
        face = MOD.build_face("6x12", MOD.parse_bdf(a), False)
        self.assertEqual(face.data[33 * 12], 0x84)
        five = make_bdf(12, 24, 5, {ord("5"): [".#........#."]})
        face = MOD.build_face("12x24", MOD.parse_bdf(five), True)
        self.assertEqual(face.data[10 * 24 * 2 : 10 * 24 * 2 + 2], bytes([0x40, 0x20]))
        faces = MOD.build_faces(synthetic_bdfs())
        self.assertEqual([len(f.data) for f in faces], [1536, 2048, 720, 960])
        self.assertEqual(sum(len(f.data) for f in faces), 5264)

    def test_full_set_code_order(self) -> None:
        codes = MOD.FULL_CODES
        self.assertEqual(len(codes), 128)
        self.assertEqual(codes[:95], tuple(range(32, 127)))
        self.assertEqual(
            (codes[95], codes[96], codes[116], codes[126], codes[127]),
            (0xB0, 0xC0, 0xE9, 0xFF, 0x178),
        )
        self.assertEqual(MOD.EXTRA, "°ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜàâäçèéêëîïôöùûüÿŸ")
        y_rows = ["#....#", ".#..#.", "..##.."]
        text = make_bdf(6, 12, 3, {ord("Y"): y_rows}, skip=(0x178,))
        face = MOD.build_face("6x12", MOD.parse_bdf(text), False)
        self.assertEqual(glyph_bytes(face, 127), glyph_bytes(face, ord("Y") - 32))
        self.assertEqual(glyph_bytes(face, 127)[:3], bytes([0x84, 0x48, 0x30]))

    def test_value_set_follows_the_key(self) -> None:
        self.assertEqual(MOD.VALUE_KEY, " +-.:0123456789")
        bdf = MOD.parse_bdf(synthetic_bdfs()["16x32"])
        face = MOD.build_face("16x32", bdf, True)
        self.assertEqual(face.key, MOD.VALUE_KEY)
        self.assertEqual(len(face.data), 15 * 32 * 2)
        for i, ch in enumerate(MOD.VALUE_KEY):
            rows = bdf.glyphs[ord(ch)].rows
            expected = b"".join(row.to_bytes(2, "big") for row in rows)
            self.assertEqual(glyph_bytes(face, i), expected, ch)
        full = MOD.build_face("8x16", MOD.parse_bdf(synthetic_bdfs()["8x16"]), False)
        self.assertIsNone(full.key)

    def assert_fails(self, name, value_set, text, code_point) -> None:
        with self.assertRaises(MOD.FontError) as ctx:
            MOD.build_face(name, MOD.parse_bdf(text), value_set)
        self.assertIn(name, str(ctx.exception))
        self.assertIn(f"U+{code_point:04X}", str(ctx.exception))

    def test_failures(self) -> None:
        a = ord("A")
        cases = [
            ("bbx", "6x12", False, make_bdf(6, 12, 3, bbx=(a, (6, 12, 0, -2))), a),
            ("dwidth", "6x12", False, make_bdf(6, 12, 3, dwidth=(a, 7)), a),
            ("bold col 0", "12x24", True, make_bdf(12, 24, 5, {ord("5"): ["#..........."]}), 0x35),
            (
                "bold col 15",
                "16x32",
                True,
                make_bdf(16, 32, 6, {ord("7"): ["...............#"]}),
                0x37,
            ),
            (
                "narrow bold",
                "16x32",
                True,
                make_bdf(16, 32, 6, {ord("."): ["...........#...."]}),
                0x2E,
            ),
            ("narrow 8x16", "8x16", False, make_bdf(8, 16, 4, {ord(":"): [".#......"]}), 0x3A),
            ("6x12 no A", "6x12", False, make_bdf(6, 12, 3, skip=(a,)), a),
            ("8x16 no Y diaeresis", "8x16", False, make_bdf(8, 16, 4, skip=(0x178,)), 0x178),
            ("16x32 no 5", "16x32", True, make_bdf(16, 32, 6, skip=(ord("5"),)), 0x35),
        ]
        for label, name, value_set, text, cp in cases:
            with self.subTest(label):
                self.assert_fails(name, value_set, text, cp)
        # Positive controls: the windows' edges are allowed.
        ok16 = make_bdf(16, 32, 6, {ord("."): ["....#.....#....."], ord("7"): [".#............#."]})
        MOD.build_face("16x32", MOD.parse_bdf(ok16), True)
        MOD.build_face("8x16", MOD.parse_bdf(make_bdf(8, 16, 4, {ord(":"): ["..#..#.."]})), False)
        MOD.build_face("6x12", MOD.parse_bdf(make_bdf(6, 12, 3, {ord(":"): ["#....."]})), False)

    def test_release_checks(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            bdfs = synthetic_bdfs()
            bdf_sha = {name: sha256(text.encode("utf-8")) for name, text in bdfs.items()}
            good = tmp / "good.tar.gz"
            write_tarball(good, bdfs, MOD.NOTICE)
            good_sha = sha256(good.read_bytes())

            texts, license_text = MOD.read_release(good, good_sha, bdf_sha)
            self.assertEqual(texts, bdfs)
            self.assertEqual(license_text, MOD.NOTICE)
            MOD.build_faces(texts)

            with self.assertRaises(MOD.FontError):
                MOD.read_release(good, "0" * 64, bdf_sha)
            with self.assertRaises(MOD.FontError) as ctx:
                MOD.read_release(good, good_sha, {**bdf_sha, "8x16": "0" * 64})
            self.assertIn("8x16", str(ctx.exception))

            bad = tmp / "bad.tar.gz"
            write_tarball(bad, bdfs, MOD.NOTICE.replace("2018-2024", "2018-2025"))
            for tarball, expected in ((bad, 1), (good, 0)):
                out = tmp / "x.cpp"
                out.unlink(missing_ok=True)
                stderr = io.StringIO()
                tar_sha = sha256(tarball.read_bytes())
                with (
                    unittest.mock.patch.object(MOD, "TARBALL_SHA256", tar_sha),
                    unittest.mock.patch.object(MOD, "BDF_SHA256", bdf_sha),
                    redirect_stderr(stderr),
                ):
                    argv = ["--tarball", str(tarball), "--cache", str(tmp), "--out", str(out)]
                    code = MOD.main(argv)
                    cpp = MOD.render_cpp(MOD.build_faces(bdfs)).encode("ascii")
                with self.subTest(tarball=tarball.name):
                    self.assertEqual(code, expected)
                    self.assertEqual(out.is_file(), expected == 0)
                    if expected:
                        self.assertIn("LICENSE", stderr.getvalue())
                    else:
                        self.assertEqual(out.read_bytes(), cpp)

    def test_cpp_output(self) -> None:
        faces = MOD.build_faces(synthetic_bdfs())
        text = MOD.render_cpp(faces)
        lines = text.split("\n")
        self.assertEqual(lines[0], "// SPDX-License-Identifier: BSD-2-Clause")
        self.assertTrue(contains_run(comment_block(text), MOD.NOTICE.splitlines()))
        self.assertIn("\n// clang-format off\n", text)
        self.assertIn("\n// clang-format on\n", text)
        self.assertLess(text.index("// clang-format off"), text.index("// clang-format on"))
        for line in (
            "const SteamiFontFace STEAMI_SPLEEN_6X12 = {6, 12, 9, 128, kSpleen6x12};",
            "const SteamiFontFace STEAMI_SPLEEN_8X16 = {8, 16, 12, 128, kSpleen8x16};",
            "const SteamiFontFace STEAMI_SPLEEN_12X24 = {12, 24, 19, 15, kSpleen12x24};",
            "const SteamiFontFace STEAMI_SPLEEN_16X32 = {16, 32, 26, 15, kSpleen16x32};",
        ):
            self.assertIn(line, lines)
        for name, size in (("6x12", 1536), ("8x16", 2048), ("12x24", 720), ("16x32", 960)):
            self.assertIn(f"const uint8_t kSpleen{name}[{size}] = {{", lines)
        text.encode("ascii")
        self.assertNotIn("\r", text)
        self.assertTrue(text.endswith("\n"))
        self.assertFalse(text.endswith("\n\n"))
        for number, line in enumerate(lines, 1):
            self.assertEqual(line, line.rstrip(), f"line {number}")
            comment = line.partition("//")[2]
            self.assertNotIn("0x", comment, f"line {number}")
            self.assertNotIn("};", comment, f"line {number}")
        self.assertEqual(MOD.render_cpp(MOD.build_faces(synthetic_bdfs())), text)

    def test_micropython_contract(self) -> None:
        faces = MOD.build_faces(synthetic_bdfs())
        arrays = cpp_arrays(MOD.render_cpp(faces))
        source = MOD.render_micropython(faces, "abc123")
        with tempfile.TemporaryDirectory() as tmp_name:
            path = Path(tmp_name) / "spleen.py"
            path.write_text(source, encoding="utf-8")
            spec = importlib.util.spec_from_file_location("spleen_under_test", path)
            spleen = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(spleen)

        self.assertEqual(spleen.VERSION, "2.1.0")
        self.assertEqual(spleen.VALUE_KEY, " +-.:0123456789")
        self.assertEqual(spleen.EXTRA, "°ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜàâäçèéêëîïôöùûüÿŸ")
        self.assertEqual(len(spleen.FOLD), 33)
        self.assertEqual(spleen.FOLD[0], "°")
        self.assertEqual(spleen.FOLD[32], "Y")
        folds = k_folds()
        self.assertEqual(len(folds), 31)
        self.assertEqual([ord(c) for c in spleen.EXTRA[1:32]], [code for code, _ in folds])
        self.assertEqual(spleen.FOLD[1:32], "".join(base for _, base in folds))

        expected = (
            ("F6X12", (6, 12, 9, None), 1536, "kSpleen6x12"),
            ("F8X16", (8, 16, 12, None), 2048, "kSpleen8x16"),
            ("F12X24", (12, 24, 19, spleen.VALUE_KEY), 720, "kSpleen12x24"),
            ("F16X32", (16, 32, 26, spleen.VALUE_KEY), 960, "kSpleen16x32"),
        )
        for name, shape, size, array in expected:
            with self.subTest(name):
                face = getattr(spleen, name)
                self.assertEqual(len(face), 5)
                self.assertEqual(face[:4], shape)
                self.assertIs(type(face[4]), bytes)
                self.assertEqual(len(face[4]), size)
                stride = (face[0] + 7) // 8 * face[1]
                for i in range(size // stride):
                    glyph = face[4][i * stride : (i + 1) * stride]
                    self.assertEqual(glyph, arrays[array][i * stride : (i + 1) * stride], i)
        self.assertNotIn("fromhex", source)

        header = []
        for line in source.splitlines():
            if not line.startswith("#"):
                break
            header.append(line)
        header_text = "\n".join(header)
        self.assertEqual(header[0], "# SPDX-License-Identifier: BSD-2-Clause")
        for needle in (
            "2.1.0",
            MOD.TARBALL_SHA256,
            *MOD.BDF_SHA256.values(),
            "scripts/gen-spleen-fonts.py",
            "abc123",
        ):
            self.assertIn(needle, header_text)
        stripped = [line[2:] if line.startswith("# ") else "" for line in header]
        self.assertTrue(contains_run(stripped, MOD.NOTICE.splitlines()))

    @unittest.skipUnless(
        CACHED_TARBALL.is_file() and TABLES.is_file(),
        "needs .spleen-cache/spleen-2.1.0.tar.gz and the committed SteamiFontSpleen.cpp",
    )
    def test_committed_tables_match_the_release(self) -> None:
        bdfs, license_text = MOD.read_release(CACHED_TARBALL)
        self.assertEqual(license_text, MOD.NOTICE)
        text = MOD.render_cpp(MOD.build_faces(bdfs))
        self.assertEqual(text.encode("ascii"), TABLES.read_bytes())


if __name__ == "__main__":
    unittest.main()
