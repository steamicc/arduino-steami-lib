# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden layout of SteamiScreen at 128 and 240 px (spec 5.8, 5.9), on the real glyph tables.

The kernel below is the plan's, written once: the C++ of SteamiScreen.cpp mirrors it in int32_t.
It asserts every worked number of the spec, then prints what the native tests pin.
Run: python3 golden_layout.py
"""
import math
import pathlib
import re
from math import isqrt

FONT_CPP = pathlib.Path(__file__).resolve().parents[3] / "lib/steami_board/src/SteamiFont.cpp"
FIRST_CODE, DEGREE, GLYPHS = 32, 127, 96


def _table(source, header, width):
    """The rows of the table that starts at `header`, up to the next `};`: `width` hex bytes each."""
    body = source.split(header, 1)[1].split("};", 1)[0]
    rows = []
    for line in body.splitlines():
        found = re.findall(r"0x[0-9A-Fa-f]{2}", line.split("//", 1)[0])
        if found:
            assert len(found) == width, line
            rows.append([int(h, 16) for h in found])
    assert len(rows) == GLYPHS, (header, len(rows))
    return rows


_SOURCE = FONT_CPP.read_text(encoding="utf-8")
FONT8 = _table(_SOURCE, "const uint8_t kFont8x8[kGlyphCount][8] = {", 8)
FONT5 = _table(_SOURCE, "const uint8_t kFont5x7[kGlyphCount][5] = {", 5)
FOLDS = {int(h, 16): c for h, c in re.findall(r"\{(0x[0-9A-Fa-f]{2}), '(.)'\}",
                                              _SOURCE.split("const Fold kFolds[] = {", 1)[1].split("};", 1)[0])}


def glyphs(s):
    """steamiNextGlyph over a str: ASCII 32..126, the degree sign, folded accents, else '?'."""
    out = []
    for ch in s:
        cp = ord(ch)
        if 32 <= cp <= 126:
            out.append(cp)
        elif cp == 0xB0:
            out.append(DEGREE)
        elif cp in FOLDS:
            out.append(ord(FOLDS[cp]))
        else:
            out.append(ord("?"))
    return out


def count(s):
    return len(glyphs(s))


TEXT, LABEL, VALUE = "TEXT", "LABEL", "VALUE"
NARROW = (ord("."), ord(":"))  # value text draws them from the column window (2, 4)


def columns(code, ink):
    """The columns that ink draws for one glyph."""
    if ink == LABEL:
        return [c & 0x7F for c in FONT5[code - FIRST_CODE]]
    cols = FONT8[code - FIRST_CODE]
    return cols[2:6] if ink == VALUE and code in NARROW else cols


def advance(code, ink):
    """Advance of one glyph at f 1."""
    if ink == LABEL:
        return 6
    return 4 if ink == VALUE and code in NARROW else 8


def ink_rows(s, n, ink):
    m = 0
    for code in glyphs(s)[:n]:
        for c in columns(code, ink):
            m |= c
    return m


def cell_width(s, n, ink, f):
    return sum(advance(code, ink) for code in glyphs(s)[:n]) * f


def value_width(s, f):
    return cell_width(s, count(s), VALUE, f)


def lowest_bit(m):
    return (m & -m).bit_length() - 1


def highest_bit(m):
    return m.bit_length() - 1


class Panel:
    """The layout kernel of one panel of W x H physical pixels."""

    def __init__(self, W, H=None):
        self.W, self.H = W, W if H is None else H
        self.k = max(1, (W + 64) // 128)
        self.r = min(self.W, self.H) // 2
        self.cx, self.cy = self.W // 2, self.H // 2

    def u(self, v):
        return int(v * self.W / 128)  # truncation toward zero: u(-12) is -22 on 240

    def chord(self, top, bottom):  # narrowest of the two end rows; None = empty
        hs = []
        for y in (top, bottom):
            dy = y - self.cy
            if abs(dy) > self.r:
                return None
            hs.append(isqrt(self.r * self.r - dy * dy))
        return (self.cx - min(hs), self.cx + min(hs))

    def ink_chord(self, s, n, ink, y, f):  # mask == 0 -> "no ink": always fits
        m = ink_rows(s, n, ink)
        if m == 0:
            return (0, self.W - 1)
        first, last = lowest_bit(m), highest_bit(m)
        return self.chord(y + first * f, y + (last + 1) * f - 1)

    def cap(self):  # 29 on 128, 63 on 240; r + 2 when r <= 52k
        r = self.r
        return r - isqrt(max(0, r * r - (13 * 8 * self.k // 2) ** 2)) + 2

    def margin_ns(self, tw, f):
        r, tw = self.r, max(tw, 0)
        m = self.cap() if tw >= 2 * r else r - isqrt(r * r - (tw // 2) ** 2) + 2  # tw < 2r: argument > 0
        return max((16 * f + 4 * self.k) * self.W // (128 * self.k), min(m, self.cap()))

    def margin_ew(self, f):
        return (8 * f + 4 * self.k) * self.W // (128 * self.k)

    def resolve(self, at, tw, f):
        W, H = self.W, self.H
        mid = self.u(64 - 4 * (f // self.k))  # rule 3 row of CENTER, W, E (text only)
        n, s = self.margin_ns(tw, f), H - self.margin_ns(tw, f) - 8 * f
        x = {"W": self.margin_ew(f), "NW": self.margin_ew(f), "SW": self.margin_ew(f),
             "E": W - self.margin_ew(f) - tw, "NE": W - self.margin_ew(f) - tw,
             "SE": W - self.margin_ew(f) - tw}.get(at, self.cx - tw // 2)
        y = {"N": n, "NE": n, "NW": n, "S": s, "SE": s, "SW": s}.get(at, mid)
        return x, y

    def fit_glyphs(self, s, ink, f, y, limit, shrink, pad):  # longest prefix, in glyphs
        for n in range(count(s), 0, -1):
            if ink_rows(s, n, ink) == 0:
                return n
            c = self.ink_chord(s, n, ink, y, f)
            if c is None:
                continue
            left, right = max(c[0] + shrink, limit[0]), min(c[1] - shrink, limit[1])
            if cell_width(s, n, ink, f) + pad <= right - left + 1:
                return n
        return 0


def draw_glyphs(s, n, ink, x, y, f):
    """The pixels drawGlyphs() lights: a set of (x, y)."""
    lit = set()
    for code in glyphs(s)[:n]:
        for i, col in enumerate(columns(code, ink)):
            for row in range(7 if ink == LABEL else 8):
                if col >> row & 1:
                    lit |= {(x + i * f + a, y + row * f + b) for a in range(f) for b in range(f)}
        x += advance(code, ink) * f
    return lit


def box(lit):
    xs, ys = [p[0] for p in lit], [p[1] for p in lit]
    return (min(xs), min(ys), max(xs), max(ys))


def device_resolve(W, at, tw):
    """device.py's _safe_margin and _resolve (micropython-steami-lib 927e96b,
    lib/steami_screen/steami_screen/device.py l.67-101) at scale 1, tw = text_len * 8."""
    width = height = W
    cx, cy = width // 2, height // 2
    r = min(width, height) // 2
    ch = 8

    def safe_margin(tw, from_edge):
        half_tw = tw / 2
        if half_tw >= r:
            return r
        max_d = math.sqrt(r * r - half_tw * half_tw)
        min_margin = r - int(max_d)
        return max(min_margin + 2, from_edge)

    margin_ns = safe_margin(tw, ch * 2 + 4)
    margin_ew = ch + 4
    positions = {
        "N": (cx - tw // 2, margin_ns),
        "NE": (width - margin_ew - tw, margin_ns),
        "E": (width - margin_ew - tw, cy - ch // 2),
        "SE": (width - margin_ew - tw, height - margin_ns - ch),
        "S": (cx - tw // 2, height - margin_ns - ch),
        "SW": (margin_ew, height - margin_ns - ch),
        "W": (margin_ew, cy - ch // 2),
        "NW": (margin_ew, margin_ns),
        "CENTER": (cx - tw // 2, cy - ch // 2),
    }
    return positions.get(at, positions["CENTER"])


ANCHORS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW", "CENTER"]
P128, P240, P0, P96 = Panel(128), Panel(240), Panel(0), Panel(96)


def width(c):
    return c[1] - c[0] + 1


# ---- Self-check: every number the spec states ----
assert (P128.k, P240.k) == (1, 2) and (P128.cap(), P240.cap()) == (29, 63)
assert [P128.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [8, 11, 4, 3, 20, 40, -12]
assert [P240.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [15, 20, 7, 5, 37, 75, -22]
# floor_f: 20/37 at scale 1, 36/67 at scale 2, 52 for f = 3 on 240; margin_ew 12/22, 30 for f = 3
assert (P128.margin_ns(8, 1), P240.margin_ns(16, 2), P128.margin_ns(8, 2), P240.margin_ns(16, 4)) == (20, 37, 36, 67)
assert P240.margin_ns(16, 3) == 52 and (P128.margin_ew(1), P240.margin_ew(2), P240.margin_ew(3)) == (12, 22, 30)
assert P128.chord(20, 20) == (18, 110)  # 93 px
assert width(P128.chord(29, 29)) == 107
assert width(P128.chord(36, 36)) == 115
assert width(P240.chord(63, 63)) == 211
assert width(P240.chord(67, 67)) == 215
assert width(P240.chord(37, 37)) == 173 and P240.chord(41, 41) == (30, 210)
assert P128.chord(9, 15) == (32, 96) and P240.chord(1, 14) == (105, 135)
assert P128.chord(-1, 0) is None and P240.chord(239, 241) is None
# no screen (W 0) and a narrow injected panel: isqrt never sees a negative number (B9)
assert (P0.k, P0.cap(), P0.margin_ns(8, 1), P0.margin_ns(160, 1)) == (1, 2, 2, 2)
assert (P96.cap(), P96.margin_ns(200, 1), P96.margin_ns(160, 1)) == (50, 50, 50)  # r 48 < 52: cap r + 2
# the 128 anchors are device.py's wherever 5.9 changes nothing (f 1, tw <= 104: below the cap)
assert all(P128.resolve(at, tw, 1) == device_resolve(128, at, tw) for at in ANCHORS for tw in range(0, 105, 8))

# ---- Values pinned by test_steami_screen (Task 2) ----
print("scale quantities")
print("  128 u(8) u(-12) k maxChars", P128.u(8), P128.u(-12), P128.k, 128 // (8 * P128.k))
print("  128 marginEw(1)", P128.margin_ew(1), "marginNs(8,1) (104,1) (112,1) (160,1) (8,2)",
      *(P128.margin_ns(tw, f) for tw, f in ((8, 1), (104, 1), (112, 1), (160, 1), (8, 2))))
print("  240 u(8) u(-12) u(40) k maxChars", P240.u(8), P240.u(-12), P240.u(40), P240.k, 240 // (8 * P240.k))
print("  240 marginEw(2) (3)", P240.margin_ew(2), P240.margin_ew(3),
      "marginNs(16,2) (176,2) (208,2) (16,4) (16,3)",
      *(P240.margin_ns(tw, f) for tw, f in ((16, 2), (176, 2), (208, 2), (16, 4), (16, 3))))
print("  96 k marginNs(200,1) (160,1)", P96.k, P96.margin_ns(200, 1), P96.margin_ns(160, 1))
print("chords")
print("  128 (20,20) (9,15) (64,64)", P128.chord(20, 20), P128.chord(9, 15), P128.chord(64, 64),
      "w(29,29)", width(P128.chord(29, 29)), "(-1,0)", P128.chord(-1, 0))
print("  240 (41,41) (1,14)", P240.chord(41, 41), P240.chord(1, 14), "w(37,37) w(63,63)",
      width(P240.chord(37, 37)), width(P240.chord(63, 63)), "(239,241)", P240.chord(239, 241))
print("ink rows and widths")
print("  inkRows _ TEXT 0x%02X, 8. VALUE 0x%02X, spaces 0x%02X, TEMP LABEL 0x%02X" % (
    ink_rows("_", 1, TEXT), ink_rows("8.", 2, VALUE), ink_rows("   ", 3, TEXT), ink_rows("TEMP", 4, LABEL)))
print("  cellWidth 23.5 TEXT", cell_width("23.5", 4, TEXT, 1), "valueWidth 23.5 f1 f2 12:34:56",
      value_width("23.5", 1), value_width("23.5", 2), value_width("12:34:56", 1),
      "TEMP LABEL f2", cell_width("TEMP", 4, LABEL, 2), "Température", cell_width("Température", 11, TEXT, 1))
print("fitGlyphs")
for P, y, f in ((P128, 29, 1), (P240, 63, 2)):
    print("  %d y %d f %d:" % (P.W, y, f), *(P.fit_glyphs(s, TEXT, f, y, (0, P.W - 1), 0, 0) for s in
                                              ("ABCDEFGHIJKLMNOPQRST", "_" * 14 + "T", " " * 14)))
print("  128 TEMP LABEL y 9 shrink 1:", P128.fit_glyphs("TEMP", LABEL, 1, 9, (0, 127), 1, 0),
      "limit (0, 50):", P128.fit_glyphs("TEMP", LABEL, 1, 9, (0, 50), 1, 0))
print("resolve")
for P, tw, f in ((P128, 16, 1), (P240, 32, 2)):
    print("  %d tw %d f %d:" % (P.W, tw, f), *("%s %s" % (at, P.resolve(at, tw, f)) for at in ANCHORS))
print("  240 f 4 CENTER", P240.resolve("CENTER", 32, 4))
print("drawGlyphs VALUE at (10, 10), f 2")
for s in ("8.8", "8:8"):
    lit = draw_glyphs(s, 3, VALUE, 10, 10, 2)
    narrow = {p for p in lit if 26 <= p[0] < 34}
    second = {p for p in lit if p[0] >= 34}
    print("  %s box %s, middle glyph box %s, third glyph first column %d" % (
        s, box(lit), box(narrow), min(p[0] for p in second)))
