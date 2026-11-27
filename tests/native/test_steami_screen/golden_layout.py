# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden layout of SteamiScreen at 128 and 240 px (spec 5.8, 5.9), on the real glyph tables.

The kernel below is the plan's, written once: the C++ of SteamiScreen.cpp mirrors it in int32_t.
It asserts every worked number of the spec, then prints what the native tests pin.
Run: python3 golden_layout.py
"""
import math
import pathlib
import re
import struct
from math import isqrt

FONT_CPP = pathlib.Path(__file__).resolve().parents[3] / "lib/steami_board/src/SteamiFont.cpp"
FIRST_CODE, DEGREE, GLYPHS = 32, 127, 96


def _table(source, header, width):
    """The rows of the table that starts at `header`, up to the next `};`.

    Each row holds `width` hex bytes.
    """
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
_FOLD_TABLE = _SOURCE.split("const Fold kFolds[] = {", 1)[1].split("};", 1)[0]
FOLDS = {int(h, 16): c for h, c in re.findall(r"\{(0x[0-9A-Fa-f]{2}), '(.)'\}", _FOLD_TABLE)}


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
        # tw < 2r: the argument of isqrt is > 0
        m = self.cap() if tw >= 2 * r else r - isqrt(r * r - (tw // 2) ** 2) + 2
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

    def polar(self, d, deg):
        """polarX/polarY (4b): the point at distance d and angle deg (0 = noon, clockwise), spec
        5.6's (cx + rnd(d sin a), cy - rnd(d cos a)) with the Q14 table of SteamiTrig.cpp."""
        return self.cx + round_q14(d * sin_q14(deg)), self.cy - round_q14(d * cos_q14(deg))


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


# ---- Widgets of Task 3: text(s, at), title and subtitle ----

def glyph_factor(P, scale):
    """f of a widget at `scale`: min(max(scale, 1), 255 // k) * k, so f always fits a uint8_t."""
    return min(max(scale, 1), 255 // P.k) * P.k


def clamp_into(P, s, n, ink, y, f, limit, x):
    """x clamped into the region of the prefix (its ink chord shrunk by k, within limit)."""
    if ink_rows(s, n, ink) == 0:
        return x
    c = P.ink_chord(s, n, ink, y, f)
    left, right = max(c[0] + P.k, limit[0]), min(c[1] - P.k, limit[1])
    return max(left, min(x, right - cell_width(s, n, ink, f) + 1))


def text_layout(P, s, at, scale):
    """(x, y, n): the cell top-left and the glyphs kept by text(s, at, c, scale).

    None draws nothing.
    """
    if not s:
        return None
    f = glyph_factor(P, scale)
    tw = cell_width(s, count(s), TEXT, f)
    px, py = P.resolve(at, tw, f)
    panel = (0, P.W - 1)
    if at in ("N", "S", "CENTER"):
        n = P.fit_glyphs(s, TEXT, f, py, panel, 0, 0)
        return P.cx - cell_width(s, n, TEXT, f) // 2, py, n
    limit = {"W": (px, P.W - 1), "E": (0, px + tw - 1)}.get(at, panel)
    n = P.fit_glyphs(s, TEXT, f, py, limit, P.k, 0)
    w = cell_width(s, n, TEXT, f)
    x = {"W": px, "E": px + tw - w}[at] if at in ("W", "E") else P.resolve(at, w, f)[0]
    return clamp_into(P, s, n, TEXT, py, f, limit, x), py, n


def text_box(W, s, at, scale=1):
    """The ink box (x0, y0, x1, y1) of text(s, at, c, scale) alone on a W panel; None if no ink."""
    P = Panel(W)
    place = text_layout(P, s, at, scale)
    if place is None:
        return None
    x, y, n = place
    lit = draw_glyphs(s, n, TEXT, x, y, glyph_factor(P, scale))
    return box(lit) if lit else None


def title_row(W, s):
    """(row, kept): the cell row of title(s) and the glyphs it keeps."""
    _, y, n = text_layout(Panel(W), s, "N", 1)
    return y, n


def subtitle_layout(P, lines):
    """[(x, y, n)] per line of subtitle(*lines): the block rises (B7), then each line is cut."""
    block = []
    for line in lines:
        if line is None:
            break
        block.append(line)
    if not block:
        return []
    f, panel = P.k, (0, P.W - 1)
    S = P.resolve("S", max(cell_width(text, count(text), TEXT, f) for text in block), f)[1]
    pitch, n = P.u(11), len(block)

    def fits(line, y):
        return P.fit_glyphs(line, TEXT, f, y, panel, 0, 0) == count(line)

    start = S + P.u(8) if n == 1 else S - (pitch * (n - 1)) // 2
    lowest = S - (n - 1) * pitch
    y = start
    while y > lowest and any(ink_rows(text, count(text), TEXT) and not fits(text, y + i * pitch)
                             for i, text in enumerate(block)):
        y -= 1
    out = []
    for i, line in enumerate(block):
        kept = P.fit_glyphs(line, TEXT, f, y + i * pitch, panel, 0, 0)
        out.append((P.cx - cell_width(line, kept, TEXT, f) // 2, y + i * pitch, kept))
    return out


def subtitle_rows(W, lines):
    """[(row, kept, ink box or None)] per line of subtitle(*lines) on a W panel."""
    P = Panel(W)
    out = []
    for line, (x, y, n) in zip(lines, subtitle_layout(P, lines), strict=False):
        lit = draw_glyphs(line, n, TEXT, x, y, P.k)
        out.append((y, n, box(lit) if lit else None))
    return out


ANCHORS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW", "CENTER"]
P128, P240, P0, P96 = Panel(128), Panel(240), Panel(0), Panel(96)


def width(c):
    return c[1] - c[0] + 1


# ---- Self-check: every number the spec states ----
assert (P128.k, P240.k) == (1, 2) and (P128.cap(), P240.cap()) == (29, 63)
assert [P128.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [8, 11, 4, 3, 20, 40, -12]
assert [P240.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [15, 20, 7, 5, 37, 75, -22]
# floor_f: 20/37 at scale 1, 36/67 at scale 2, 52 for f = 3 on 240; margin_ew 12/22, 30 for f = 3
assert (P128.margin_ns(8, 1), P240.margin_ns(16, 2),
        P128.margin_ns(8, 2), P240.margin_ns(16, 4)) == (20, 37, 36, 67)
assert P240.margin_ns(16, 3) == 52
assert (P128.margin_ew(1), P240.margin_ew(2), P240.margin_ew(3)) == (12, 22, 30)
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
# r 48 < 52: cap r + 2
assert (P96.cap(), P96.margin_ns(200, 1), P96.margin_ns(160, 1)) == (50, 50, 50)
# the 128 anchors are device.py's wherever 5.9 changes nothing (f 1, tw <= 104: below the cap)
assert all(P128.resolve(at, tw, 1) == device_resolve(128, at, tw)
           for at in ANCHORS for tw in range(0, 105, 8))

# ---- Values pinned by test_steami_screen (Task 2) ----
print("scale quantities")
print("  128 u(8) u(-12) k maxChars", P128.u(8), P128.u(-12), P128.k, 128 // (8 * P128.k))
print("  128 marginEw(1)", P128.margin_ew(1), "marginNs(8,1) (104,1) (112,1) (160,1) (8,2)",
      *(P128.margin_ns(tw, f) for tw, f in ((8, 1), (104, 1), (112, 1), (160, 1), (8, 2))))
print("  240 u(8) u(-12) u(40) k maxChars", P240.u(8), P240.u(-12), P240.u(40), P240.k,
      240 // (8 * P240.k))
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
print(f"  inkRows _ TEXT 0x{ink_rows('_', 1, TEXT):02X}, "
      f"8. VALUE 0x{ink_rows('8.', 2, VALUE):02X}, "
      f"spaces 0x{ink_rows('   ', 3, TEXT):02X}, TEMP LABEL 0x{ink_rows('TEMP', 4, LABEL):02X}")
print("  cellWidth 23.5 TEXT", cell_width("23.5", 4, TEXT, 1), "valueWidth 23.5 f1 f2 12:34:56",
      value_width("23.5", 1), value_width("23.5", 2), value_width("12:34:56", 1),
      "TEMP LABEL f2", cell_width("TEMP", 4, LABEL, 2),
      "Température", cell_width("Température", 11, TEXT, 1))
print("fitGlyphs")
for P, y, f in ((P128, 29, 1), (P240, 63, 2)):
    print(f"  {P.W} y {y} f {f}:",
          *(P.fit_glyphs(s, TEXT, f, y, (0, P.W - 1), 0, 0)
            for s in ("ABCDEFGHIJKLMNOPQRST", "_" * 14 + "T", " " * 14)))
print("  128 TEMP LABEL y 9 shrink 1:", P128.fit_glyphs("TEMP", LABEL, 1, 9, (0, 127), 1, 0),
      "limit (0, 50):", P128.fit_glyphs("TEMP", LABEL, 1, 9, (0, 50), 1, 0))
print("resolve")
for P, tw, f in ((P128, 16, 1), (P240, 32, 2)):
    print(f"  {P.W} tw {tw} f {f}:", *(f"{at} {P.resolve(at, tw, f)}" for at in ANCHORS))
print("  240 f 4 CENTER", P240.resolve("CENTER", 32, 4))
print("drawGlyphs VALUE at (10, 10), f 2")
for s in ("8.8", "8:8"):
    lit = draw_glyphs(s, 3, VALUE, 10, 10, 2)
    narrow = {p for p in lit if 26 <= p[0] < 34}
    second = {p for p in lit if p[0] >= 34}
    print(f"  {s} box {box(lit)}, middle glyph box {box(narrow)}, "
          f"third glyph first column {min(p[0] for p in second)}")

# ---- Self-check of Task 3: titles, text at a scale, subtitles (spec 5.8 rule 7, 5.9, B7, B8) ----
assert title_row(128, "Temperature") == (20, 11) and title_row(240, "Temperature") == (41, 11)
assert all(title_row(W, s) == (row, 7)
           for s in ("Comfort", "Battery") for W, row in ((128, 20), (240, 37)))
for s in ("ABCDEFGHIJKLM", "ABCDEFGHIJKLMNOPQRST"):
    assert title_row(128, s) == (29, 13) and title_row(240, s) == (63, 13), s
assert title_row(128, "_" * 14) == (29, 14) and title_row(240, "_" * 14) == (63, 13)
assert title_row(128, "_" * 14 + "T") == (29, 14) and title_row(240, "_" * 14 + "T") == (63, 13)
assert text_layout(P128, "ABCDEFGH", "N", 2)[1:] == (36, 7)
assert text_layout(P240, "ABCDEFGH", "N", 2)[1:] == (67, 6)
CHIPS = ("WSEN-HIDS", "BME280", "APDS-9960")
ONE_LINE = {("WSEN-HIDS",): (108, 202), ("BME280",): (108, 202), ("BQ27441",): (108, 202),
            ("VL53L1X",): (108, 202), ("SURPRISED",): (108, 202), ("Comfortable",): (104, 188),
            ("VL53L1X ToF",): (104, 188), ("Uncomfortable",): (95, 166),
            ("20s window",): (107, 196)}
TWO_LINES = {("3842 mV", "BQ27441"): ((95, 106), (177, 197))}
for chip in CHIPS:
    TWO_LINES[("Comfortable", chip)] = ((95, 106), (173, 193))
    TWO_LINES[("Uncomfortable", chip)] = ((86, 97), (151, 171))
    TWO_LINES[(chip, "20s window")] = ((95, 106), (176, 196))
for lines, rows in ONE_LINE.items():
    for W, row in zip((128, 240), rows, strict=True):
        got = [(y, n) for y, n, _ in subtitle_rows(W, lines)]
        assert got == [(row, count(lines[0]))], (W, lines)
for lines, rows in TWO_LINES.items():
    for W, pair in zip((128, 240), rows, strict=True):
        got = subtitle_rows(W, lines)
        assert tuple(y for y, _, _ in got) == pair, (W, lines, got)
        # every line whole
        assert all(n == count(line) for (_, n, _), line in zip(got, lines, strict=True)), (W, lines)
# Review Focus 3: 15 glyphs (20 bytes), 13 kept, cells 12..115, ink 13..114
# ('T' column 0, 'e' column 7)
assert text_layout(P128, "Températureéééé", "N", 1) == (12, 29, 13)
assert text_box(128, "Températureéééé", "N")[0::2] == (13, 114)
# the available width of a lowercase-only title comes from the rows it inks
assert width(P128.ink_chord("acemnorsuvwxz", 13, TEXT, 29, 1)) == 109
assert width(P128.ink_chord("ABCDEFGHIJKLM", 13, TEXT, 29, 1)) == 107
assert width(P240.ink_chord("acemnorsuvwxz", 13, TEXT, 63, 2)) == 215
assert width(P240.ink_chord("ABCDEFGHIJKLM", 13, TEXT, 63, 2)) == 211
assert title_row(128, "acemnorsuvwxzacemnorsuvwxz")[1] == 13
assert title_row(240, "acemnorsuvwxzacem")[1] == 13

# ---- Values pinned by test_steami_screen (Task 3) ----
print("text_box of Hi at the nine anchors (x0 y0 x1 y1)")
for W in (128, 240):
    for scale in (1, 2):
        print(f"  {W} scale {scale}:",
              *(f"{at} {text_box(W, 'Hi', at, scale)}" for at in ANCHORS))
TITLES = ("Temperature", "Comfort", "Battery", "ABCDEFGHIJKLM", "ABCDEFGHIJKLMNOPQRST", "_" * 14,
          "_" * 14 + "T", "Températureéééé")
print("titles (row, kept, ink box)")
for s in TITLES:
    print(f"  {s!r:<22}", *(f"{W}: {title_row(W, s)} {text_box(W, s, 'N')}" for W in (128, 240)))
print("  text('ABCDEFGH', N, 2)",
      *(f"{W}: {text_layout(Panel(W), 'ABCDEFGH', 'N', 2)} {text_box(W, 'ABCDEFGH', 'N', 2)}"
        for W in (128, 240)))
print("subtitles (row, kept, ink box) per line")
for lines in list(ONE_LINE) + list(TWO_LINES):
    for W in (128, 240):
        print(f"  {W} {lines!r}:", *subtitle_rows(W, lines))

# ---- Widget of Task 4: value() at CENTER, N, S, W and E (spec 5.9, B2, B6, B9) ----
REFERENCE = "88.8"  # the widest value that keeps the same f at W and E (B6)


def has_text(s):
    """device.py's `if unit:`: None and "" are no text."""
    return bool(s)


def block_height(P, f, has_unit):
    """The value block, physical for any f: 8f + 8f/3 + 8k with a unit, 8f without."""
    return 8 * f + 8 * f // 3 + 8 * P.k if has_unit else 8 * f


def region_width(r):
    return max(0, r[1] - r[0] + 1)


def half_limit(P, at):
    """The side of centerX() that a W or E value takes: W ends at cx - u(3) - 1, E mirrors it."""
    return (0, P.cx - P.u(3) - 1) if at == "W" else (P.cx + P.u(3) + 1, P.W - 1)


def half_region(P, at, c):
    """The W/E half region of a chord: [left edge + k, cx - u(3) - 1], E mirrored.

    (0, -1) if empty.
    """
    if c is None:
        return (0, -1)
    lo, hi = half_limit(P, at)
    return (max(c[0] + P.k, lo), min(c[1] - P.k, hi))


def kept_room(P, s, n, ink, y, f, limit, shrink):
    """fitGlyphs()'s room on return: the kept prefix's ink chord (the panel without ink) shrunk by
    `shrink`, within limit."""
    c = P.ink_chord(s, n, ink, y, f) if n and ink_rows(s, n, ink) else (0, P.W - 1)
    return (max(c[0] + shrink, limit[0]), min(c[1] - shrink, limit[1]))


CORNERS = ("NE", "NW", "SE", "SW")


def corner_region(P, top, f):
    """The corner region of a value cell row band: the chord at rows top..top + 8f - 1 shrunk by k
    at each end; (0, -1) if empty."""
    c = P.chord(top, top + 8 * f - 1)
    return (0, -1) if c is None else (c[0] + P.k, c[1] - P.k)


def corner_place(P, t, at, scale):
    """5.9 "Corner anchors" (B9): f, top, region, kept glyphs and x of a value at NE, NW, SE, SW."""
    f = glyph_factor(P, scale)
    while True:
        vw = value_width(t, f)
        x, top = P.resolve(at, vw, f)
        region = corner_region(P, top, f)
        if region_width(region) > 0 and region[0] <= x and x + vw - 1 <= region[1]:
            return {"f": f, "top": top, "region": region, "n": count(t), "x": x}
        if f == 1:
            break
        f -= 1
    # f = 1: p, top and region stay those of the whole text; only x follows the kept prefix. The
    # longest prefix with value_width <= region width, as fitGlyphs() keeps it (the region lies
    # inside the ink chord of any prefix; a prefix without ink always fits, B9).
    n = P.fit_glyphs(t, VALUE, 1, top, region, 0, 0)
    w = cell_width(t, n, VALUE, 1)
    x = max(region[0], min(P.resolve(at, w, 1)[0], region[1] - w + 1))
    return {"f": 1, "top": top, "region": region, "n": n, "x": x}


def value_place(P, t, at, unit, scale, y_offset):
    """f, top, region, kept glyphs and x of the value text t (the decision "Scale", 5.9)."""
    if at in CORNERS:
        return corner_place(P, t, at, scale)
    side = at in ("W", "E")
    pad = 0 if side else P.u(4)
    f = glyph_factor(P, scale)
    while True:
        vw = value_width(t, f)
        if at in ("N", "S"):
            top = P.resolve(at, vw, f)[1]
        else:  # CENTER, W, E
            top = P.cy + P.u(y_offset) - block_height(P, f, has_text(unit)) // 2
        if side:
            region = half_region(P, at, P.chord(top, top + 8 * f - 1))
            fits = max(vw, value_width(REFERENCE, f)) <= region_width(region)
        else:
            region = (0, P.W - 1)
            c = P.ink_chord(t, count(t), VALUE, top, f)
            fits = ink_rows(t, count(t), VALUE) == 0 or (c is not None and vw + pad <= width(c))
        if fits or f == 1:
            break
        f -= 1
    n = P.fit_glyphs(t, VALUE, f, top, region, 0, pad)  # whole whenever it fits; else cut at f 1
    w = cell_width(t, n, VALUE, f)
    x = region[0] + (region_width(region) - w) // 2 if side else P.cx - w // 2
    return {"f": f, "top": top, "region": region, "n": n, "x": x}


def part_place(P, s, ink, y, at, centre):
    """(x, kept, room) of a label (LABEL) or unit (TEXT) at row y, factor k; `centre` is the
    value's centre column, which a corner label or unit is centred on before the clamp."""
    if at in ("W", "E"):
        limit, shrink = half_limit(P, at), P.k
    elif at in CORNERS:  # the chord at its own inked rows shrunk by k
        limit, shrink = (0, P.W - 1), P.k
    else:
        limit, shrink = (0, P.W - 1), 0
    n = P.fit_glyphs(s, ink, P.k, y, limit, shrink, 0)
    room = kept_room(P, s, n, ink, y, P.k, limit, shrink)
    w = cell_width(s, n, ink, P.k)
    if at in ("W", "E"):
        x = room[0] + (region_width(room) - w) // 2
    elif at in CORNERS:
        x = max(room[0], min(centre - w // 2, room[1] - w + 1))
    else:
        x = P.cx - w // 2
    return x, n, room


def value_layout(W, t, at="CENTER", unit=None, label=None, scale=2, y_offset=0):
    """Boxes (value, label, unit) of value(t, unit, at, label, c, scale, yOffset) alone on a W
    panel, each None without ink, and the placement: f, top, region, kept glyphs of each part."""
    P = Panel(W)
    v = value_place(P, t, at, unit, scale, y_offset)
    out = dict(v)
    lit_value = draw_glyphs(t, v["n"], VALUE, v["x"], v["top"], v["f"])
    out["value"] = box(lit_value) if lit_value else None
    out["value_lit"] = lit_value
    for name, s, ink, y in (("label", label, LABEL, v["top"] - P.u(4) - 7 * P.k),
                            ("unit", unit, TEXT, v["top"] + 8 * v["f"])):
        out[name] = None
        out[name + "_n"] = 0
        out[name + "_lit"] = set()
        if has_text(s):
            centre = v["x"] + cell_width(t, v["n"], VALUE, v["f"]) // 2
            x, n, room = part_place(P, s, ink, y, at, centre)
            lit = draw_glyphs(s, n, ink, x, y, P.k)
            out[name] = box(lit) if lit else None
            out[name + "_n"], out[name + "_room"], out[name + "_x"] = n, room, x
            out[name + "_lit"] = lit
    return out


def value_cut(W, t, at="CENTER", unit=None, y_offset=0):
    """True when value(t) keeps fewer glyphs than t has."""
    return value_layout(W, t, at, unit, None, 2, y_offset)["n"] < count(t)


def first_cut(W, t, unit, step):
    """The first yOffset from 0 (by step -1 or +1) at which the value is cut."""
    y = 0
    while not value_cut(W, t, "CENTER", unit, y):
        y += step
    return y


# ---- Self-check of Task 4 ----
DEG_C = "°C"
assert all(value_width("23.5", f) == 28 * f for f in range(1, 9))
v01 = value_layout(128, "23.5", "CENTER", DEG_C)
assert (v01["f"], v01["top"], v01["x"]) == (2, 50, 36)
assert v01["value"][1::2] == (50, 63) and v01["unit"][1::2] == (66, 72)
assert value_layout(240, "23.5", "CENTER", DEG_C)["f"] == 4
for W, f, top, west, east in ((128, 2, 50, (3, 60), (68, 125)), (240, 3, 96, (5, 114), (126, 235))):
    a = value_layout(W, "23.5", "W", DEG_C, "TEMP")
    b = value_layout(W, "45", "E", "%", "HUM")
    assert (a["f"], a["top"], a["region"]) == (f, top, west), (W, a)
    assert (b["f"], b["top"], b["region"]) == (f, top, east), (W, b)
    assert value_layout(W, "1", "W", DEG_C)["f"] == f
for t, f128, f240 in (("1", 2, 3), ("8888", 1, 3), ("123456", 1, 2)):
    for W, f in ((128, f128), (240, f240)):
        for at in ("W", "E"):
            got = value_layout(W, t, at, DEG_C)
            assert got["f"] == f and got["n"] == count(t), (W, t, at, got)
assert value_layout(128, "8888", "W", DEG_C)["region"] == (2, 60)  # the x1 W region, 59 px
assert value_layout(240, "8888", "W", DEG_C, scale=1)["f"] == 2
assert value_layout(128, "12:34:56", "W", DEG_C)["n"] == 8
for t in ("12345678", "-1234.56", "Uncomfortable"):
    assert value_cut(128, t, "W", DEG_C) and value_cut(128, t, "E", DEG_C), t
assert not value_cut(240, "Uncomfortable", "W", DEG_C)
assert value_cut(240, "ABCDEFGHIJKLMNO", "W", DEG_C)
FIFTEEN, NUMERIC = "888888888888888", "-999999936.0000"
assert value_width(FIFTEEN, 1) == 120 and value_width(NUMERIC, 1) == 116
assert value_cut(128, FIFTEEN, "CENTER", DEG_C, -12)
assert value_cut(240, FIFTEEN, "CENTER", DEG_C, -50)
assert not value_cut(128, FIFTEEN, "CENTER", DEG_C) and not value_cut(240, FIFTEEN, "CENTER", DEG_C)
THRESHOLDS = {128: ((-7, 19), (-12, 14), (-14, 26)), 240: ((-48, 59), (-53, 54), (-49, 60))}
for W, expected in THRESHOLDS.items():
    got = tuple((first_cut(W, t, unit, -1), first_cut(W, t, unit, 1))
                for t, unit in ((FIFTEEN, DEG_C), (FIFTEEN, None), (NUMERIC, DEG_C)))
    assert got == expected, (W, got)
for W, whole in ((128, "TEMPERATU"), (240, "TEMPERAT")):
    for at in ("W", "E"):
        assert value_layout(W, "23.5", at, DEG_C, whole)["label_n"] == len(whole), (W, at)
        assert value_layout(W, "23.5", at, DEG_C, whole + "R")["label_n"] == len(whole), (W, at)
        assert value_layout(W, "23.5", at, DEG_C, "TEMPERATURE")["label_n"] == len(whole), (W, at)

# ---- Values pinned by test_steami_screen (Task 4) ----
VALUE_CASES = (
    ("01", "23.5", "CENTER", DEG_C, None, 0),
    ("02", "72", "CENTER", "%", None, -12),
    ("03 W", "23.5", "W", DEG_C, "TEMP", 0),
    ("03 E", "45.3", "E", "%", "HUM", 0),
    ("N unit", "23.5", "N", DEG_C, None, 0),
    ("N", "23.5", "N", None, None, 0),
    ("S unit", "23.5", "S", DEG_C, None, 0),
    ("S", "23.5", "S", None, None, 0),
)
print("value boxes (f top x; value, label, unit)")
for name, t, at, unit, label, y_offset in VALUE_CASES:
    for W in (128, 240):
        got = value_layout(W, t, at, unit, label, 2, y_offset)
        print(f"  {name:<6} {W}: f {got['f']} top {got['top']} x {got['x']} "
              f"region {got['region']}; {got['value']} {got['label']} {got['unit']}")
print("value regions and cuts")
for W in (128, 240):
    for t in ("1", "8888", "123456", "12:34:56", "Uncomfortable", "ABCDEFGHIJKLMNO"):
        for at in ("W", "E"):
            got = value_layout(W, t, at, DEG_C)
            print(f"  {W} {t!r:<16} {at}: f {got['f']} region {got['region']} "
                  f"kept {got['n']} box {got['value']}")
    for at in ("W", "E"):
        got = value_layout(W, "23.5", at, DEG_C, "TEMPERATURE")
        print(f"  {W} TEMPERATURE label {at}: kept {got['label_n']} "
              f"room {got['label_room']} box {got['label']}")
    for y_offset in (0, -12, -50):
        got = value_layout(W, FIFTEEN, "CENTER", DEG_C, None, 2, y_offset)
        print(f"  {W} {FIFTEEN} CENTER yOffset {y_offset}: kept {got['n']} box {got['value']}")

# ---- Self-check of Task 5: value() at the corners (5.9 "Corner anchors", B9) ----
CORNER_LABELS = ("TEMP", "TEMPERATURE", "TEMPERATURES12")
CORNER_TEXTS = ("-12345.7", "Uncomfortable", "ABCDEFGHIJKLMNO")  # -12345.678f formats as "-12345.7"


def inside(b, room):
    return b is None or (room[0] <= b[0] and b[2] <= room[1])


c128 = value_layout(128, "8", "NE", "C", "TEMP", 1)
assert (c128["f"], c128["x"], c128["top"], c128["n"]) == (1, 102, 20, 1), c128
assert c128["label"][1::2] == (9, 15) and c128["label_room"] == (33, 95), c128
assert c128["label_x"] == 72, c128
assert c128["label"][2] <= 95 and c128["label_n"] == 4
assert c128["x"] + 8 // 2 - 24 // 2 == 94  # "moves from x 94 to x 72"
s128 = value_layout(128, "8", "SE", "C", "TEMP", 1)
assert s128["unit"][1::2] == (108, 114) and s128["unit_room"] == (26, 102), s128
assert s128["unit_x"] == 95, s128
assert s128["x"] + 8 // 2 - 8 // 2 == 103  # "moves from x 103 to x 95"
c240 = value_layout(240, "8", "NE", "C", "TEMP", 1)
assert (c240["f"], c240["x"], c240["top"]) == (1, 180, 22), c240  # f 2 leaves the region
assert c240["label"][1::2] == (1, 14) and c240["label_room"] == (107, 133), c240
assert (c240["label_n"], c240["label_x"]) == (2, 110), c240  # "TE", 24 px
assert width(P240.chord(1, 14)) == 31 and region_width(c240["label_room"]) == 27
# every corner case: each part inside its region (the value) or its room (label, unit), so inside
# the circle; the 15-glyph text is cut on 128 (cap row 29, region 105 px) and whole on 240
for W in (128, 240):
    for at in CORNERS:
        cases = ([("8", "C", corner_label, sc) for corner_label in CORNER_LABELS for sc in (1, 2)] +
                 [(t, DEG_C, "TEMPERATURES12", 2) for t in CORNER_TEXTS])
        for t, unit, label, scale in cases:
            got = value_layout(W, t, at, unit, label, scale)
            assert inside(got["value"], got["region"]), (W, at, t, got)
            assert inside(got["label"], got["label_room"]), (W, at, t)
            assert inside(got["unit"], got["unit_room"]), (W, at, t)
        kept = value_layout(W, "ABCDEFGHIJKLMNO", at, DEG_C)["n"]
        assert kept == (13 if W == 128 else 15), (W, at)

# ---- Values pinned by test_steami_screen (Task 5) ----
print("value corners: value(8, \"C\", at, label, WHITE, scale) (f top x kept; value, label, unit)")
for W in (128, 240):
    for at in CORNERS:
        for scale in (1, 2):
            for label in CORNER_LABELS:
                got = value_layout(W, "8", at, "C", label, scale)
                print(f"  {W} {at} s{scale} {label:<15} f {got['f']} top {got['top']} "
                      f"x {got['x']} n {got['n']}; {got['value']} {got['label']} {got['unit']}; "
                      f"label n {got['label_n']} x {got['label_x']} room {got['label_room']}; "
                      f"unit x {got['unit_x']} room {got['unit_room']}")
print("value corners: value(text, \"°C\", at, \"TEMPERATURES12\", WHITE, 2)")
for W in (128, 240):
    for at in CORNERS:
        for t in CORNER_TEXTS:
            got = value_layout(W, t, at, DEG_C, "TEMPERATURES12", 2)
            print(f"  {W} {at} {t!r:<16} f {got['f']} top {got['top']} x {got['x']} "
                  f"n {got['n']} region {got['region']}; "
                  f"{got['value']} {got['label']} {got['unit']}; label n {got['label_n']}")
got = value_layout(128, "8", "NE", None, None, 1)
print(f"  128 value(8, nullptr, NE, nullptr, WHITE, 1): f {got['f']} top {got['top']} "
      f"x {got['x']} region {got['region']}; {got['value']}")

# ---- Widget of Task 6: bar() (spec 5.9 "bar", the decision "bar() geometry") ----


def bar_fill(w, v, max_v):
    """device.py's int(w * clamp(v, 0, maxV) / maxV), truncated: NAN v or maxV not above 0 (NAN
    included) gives 0, v >= maxV (+INF included) w, v <= 0 (-INF included) 0."""
    if math.isnan(v) or not max_v > 0 or v <= 0:
        return 0
    if v >= max_v:
        return w
    return min(int(w * v / max_v), w)


def bar_rect(W, v, max_v=100, y_offset=0):
    """(x, top, w, h, fill) of bar(v, maxV, yOffset) on a W panel, before any clipping."""
    P = Panel(W)
    w = P.W - P.u(40)
    return P.cx - w // 2, P.cy + P.u(20) + P.u(y_offset), w, P.u(8), bar_fill(w, v, max_v)


def bar_spans(W, v, max_v=100, y_offset=0):
    """{row: ((track x0, x1), (fill x0, x1) or None)}: each row of the bar on the panel, its span
    cut to the chord at that row; a row whose cut track is empty is left out."""
    P = Panel(W)
    x, top, w, h, fill = bar_rect(W, v, max_v, y_offset)
    rows = {}
    for y in range(max(top, 0), min(top + h, P.H)):
        c = P.chord(y, y)
        if c is None:
            continue
        left, right = max(x, c[0], 0), min(x + w - 1, c[1], P.W - 1)
        if right < left:
            continue
        f_right = min(x + fill - 1, c[1], P.W - 1)
        rows[y] = ((left, right), (left, f_right) if f_right >= left else None)
    return rows


def bar_boxes(W, v, max_v=100, y_offset=0):
    """(unfilled track box, fill box) as the native tests see them: the fill is drawn over the
    track, so GRID_DARK keeps only the unfilled columns; None when nothing is lit."""
    track, filled = set(), set()
    for y, ((t0, t1), f) in bar_spans(W, v, max_v, y_offset).items():
        lit = set(range(f[0], f[1] + 1)) if f else set()
        filled |= {(xx, y) for xx in lit}
        track |= {(xx, y) for xx in range(t0, t1 + 1) if xx not in lit}
    return (box(track) if track else None), (box(filled) if filled else None)


# ---- Self-check of Task 6 ----
assert bar_rect(128, 72, 100, 0)[:4] == (20, 84, 88, 8) and bar_rect(128, 72, 100, -12)[1] == 72
assert bar_rect(240, 72, 100, -12)[:4] == (38, 135, 165, 15)
assert bar_rect(128, 72)[4] == 63 and bar_rect(240, 72)[4] == 118
for y_offset, rows in ((0, (84, 91)), (-12, (72, 79))):
    spans = bar_spans(128, 72, 100, y_offset)
    assert (min(spans), max(spans)) == rows and all(t == (20, 107) for t, _ in spans.values())
assert bar_boxes(128, 72, 100, -12) == ((83, 72, 107, 79), (20, 72, 82, 79))
spans = bar_spans(240, 72, 100, -12)
assert (min(spans), max(spans)) == (135, 149) and all(t == (38, 202) for t, _ in spans.values())
# Review Focus 5: an empty or a full bar, never a crash
INF = float("inf")
NAN = float("nan")
for v, max_v, fill in ((-5, 100, 0), (-INF, 100, 0), (150, 100, 88), (INF, 100, 88), (NAN, 100, 0),
                       (50, 0, 0), (50, -10, 0), (50, NAN, 0), (50, INF, 0), (INF, INF, 88)):
    assert bar_rect(128, v, max_v)[4] == fill, (v, max_v)
# 88 * 3e38 overflows a float: the C++ takes the ratio first
assert bar_rect(128, 3e38, 3.4e38)[4] == 77
# yOffset 0: the whole track is inside the disc (88 / 165 px on every row)
for W, whole in ((128, 88), (240, 165)):
    spans = bar_spans(W, 50, 100, 0)
    assert len(spans) == Panel(W).u(8) and all(t[1] - t[0] + 1 == whole for t, _ in spans.values())
# Review Focus 4: rows at the int16_t limits leave the panel, nothing is drawn
for W in (128, 240):
    for y_offset in (32767, -32767, 20000, -20000):
        assert bar_spans(W, 50, 100, y_offset) == {}, (W, y_offset)

# ---- Values pinned by test_steami_screen (Task 6) ----
print("bar(72, 100, yOffset) (x top w h fill; unfilled track box, fill box)")
for W in (128, 240):
    for y_offset in (0, -12):
        track, filled = bar_boxes(W, 72, 100, y_offset)
        print(f"  {W} yOffset {y_offset}: {bar_rect(W, 72, 100, y_offset)}; {track} {filled}")
print("bar(50, 100, 0) (fill box)",
      *(f"{W}: {bar_boxes(W, 50, 100, 0)[1]}" for W in (128, 240)))

# ---- Widget of 4b Task 1: face() (spec 5.9 "face", the decision "face") ----
# const.py's FACES at micropython-steami-lib 927e96b (lines 23-30), in SteamiFace order: rows top to
# bottom, MSB = left column. Held here as literals, never parsed from SteamiScreenConst.h.
FACES = {
    "happy": (0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x3C, 0x00),
    "sad": (0x00, 0x24, 0x24, 0x00, 0x00, 0x3C, 0x42, 0x00),
    "surprised": (0x00, 0x24, 0x24, 0x00, 0x18, 0x24, 0x24, 0x18),
    "sleeping": (0x00, 0x00, 0x66, 0x00, 0x00, 0x18, 0x18, 0x00),
    "angry": (0x00, 0x42, 0x24, 0x24, 0x00, 0x3C, 0x42, 0x00),
    "love": (0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00),
}


def face_layout(W, compact):
    """(cell, ox, oy) of face(f, compact) on a W panel: cells of u(11), or u(8) when compact,
    the 8x8 grid centred, raised by half a cell when compact (device.py)."""
    P = Panel(W)
    cell = P.u(8 if compact else 11)
    return cell, P.cx - 4 * cell, P.cy - 4 * cell - (cell // 2 if compact else 0)


def face_cells(W, bitmap, compact):
    """The filled squares (x, y, w, h) of face(bitmap, compact), in drawing order."""
    cell, ox, oy = face_layout(W, compact)
    return [(ox + (i % 8) * cell, oy + (i // 8) * cell, cell, cell)
            for i in range(64) if bitmap[i // 8] & (0x80 >> (i % 8))]


# ---- Self-check of 4b Task 1 ----
assert list(FACES) == ["happy", "sad", "surprised", "sleeping", "angry", "love"]  # SteamiFace order
assert all(len(b) == 8 for b in FACES.values()) and len(FACES) * 8 == 48  # spec 5.12: faces 48 B
assert face_layout(128, False) == (11, 20, 20) and face_layout(240, False) == (20, 40, 40)
assert face_layout(128, True) == (8, 32, 28) and face_layout(240, True) == (15, 60, 53)
# Spec 5.9 and 5.12: 11 px cells (88 px) on 128, 20 px cells (160 px) on 240
assert [8 * face_layout(W, False)[0] for W in (128, 240)] == [88, 160]
# The cells u(11) and u(8) equal device.py's W * 11 // 128 and W // 16 on every width
for W in range(0, 1025):
    assert face_layout(W, False)[0] == W * 11 // 128 and face_layout(W, True)[0] == W // 16, W
# Review Focus 3: eight 0xFF rows, full and compact, stay inside radius() + 1 (the memory backend's
# test on the four corners of each square)
for W in (128, 240):
    P = Panel(W)
    for compact in (False, True):
        squares = face_cells(W, (0xFF,) * 8, compact)
        assert len(squares) == 64
        for x, y, w, h in squares:
            for px, py in ((x, y), (x + w - 1, y), (x, y + h - 1), (x + w - 1, y + h - 1)):
                assert (px - P.cx) ** 2 + (py - P.cy) ** 2 <= (P.r + 1) ** 2, (W, compact, px, py)

# ---- Values pinned by test_steami_screen (4b Task 1) ----
print("face_layout (cell, ox, oy)",
      *(f"{W} {'compact' if c else 'full'}: {face_layout(W, c)}"
        for W in (128, 240) for c in (False, True)))
for name, bitmap in FACES.items():
    print(f"  {name}: {len(face_cells(128, bitmap, False))} squares")

# ---- Q14 trigonometry of 4b (SteamiTrig.cpp, spec 5.6) ----
# kSinQ14[91] parsed from SteamiTrig.cpp (decimal values), and the three functions mirrored from it.
TRIG_CPP = FONT_CPP.with_name("SteamiTrig.cpp")
_SIN_BODY = TRIG_CPP.read_text(encoding="utf-8").split("kSinQ14[91] = {", 1)[1].split("};", 1)[0]
SIN_Q14 = [int(v) for v in re.findall(r"-?\d+", _SIN_BODY)]
assert len(SIN_Q14) == 91


def sin_q14(deg):
    """steamiSinQ14: round(16384 sin deg) for any integer degree."""
    d = deg % 360  # Python's % is already in 0..359
    if d <= 90:
        return SIN_Q14[d]
    if d <= 180:
        return SIN_Q14[180 - d]
    if d <= 270:
        return -SIN_Q14[d - 180]
    return -SIN_Q14[360 - d]


def cos_q14(deg):
    """steamiCosQ14: steamiSinQ14(deg + 90)."""
    return sin_q14(deg % 360 + 90)


def round_q14(v):
    """steamiRoundQ14: v / 16384 rounded to nearest, halves away from zero."""
    return (v + 8192) // 16384 if v >= 0 else -((-v + 8192) // 16384)


assert ([sin_q14(d) for d in (0, 30, 90, 180, 270, -90, 450)]
        == [0, 8192, 16384, 0, -16384, -16384, 16384])
assert [cos_q14(d) for d in (0, 60, 90, 180, 360)] == [16384, 8192, 0, -16384, 16384]
assert [round_q14(v) for v in (8191, 8192, -8191, -8192, 24576, -24576)] == [0, 1, 0, -1, 2, -2]
# The gauge label points (4b Task 4) and their mirror images
assert P128.polar(43, 233) == (30, 90) and P128.polar(43, 127) == (98, 90)
assert P240.polar(82, 233) == (55, 169) and P240.polar(82, 127) == (185, 169)
# rnd is odd-symmetric: the point at deg + 180 is the mirror image of the point at deg
for P in (P128, P240):
    for d in (3, 5, 44, 83):
        for deg in range(360):
            x, y = P.polar(d, deg)
            mx, my = P.polar(d, deg + 180)
            assert (mx - P.cx, my - P.cy) == (P.cx - x, P.cy - y), (P.W, d, deg)

# ---- Values pinned by test_steami_screen (4b Task 2: polarX, polarY) ----
print("polar (polarX, polarY)")
print("  128 (43,233) (43,127) (44,0) (0,77) (44,-90) (44,270)",
      *(P128.polar(d, a)
        for d, a in ((43, 233), (43, 127), (44, 0), (0, 77), (44, -90), (44, 270))))
print("  240 (82,233) (83,0)", P240.polar(82, 233), P240.polar(83, 0))

# ---- A raster of the primitives the round widgets draw through (SteamiDisplay.cpp) ----
# Each function adds {(x, y): colour} to a canvas dict, later calls painting over earlier ones,
# cut to the W x W panel. They mirror span(), walk() (strokeLine), triangle(..., true), arc() for
# a full turn (strokeCircle), circle(..., true) and text(): the goldens below read their pixels.


def r_span(cv, W, x, y, w, h, c):
    for yy in range(max(y, 0), min(y + h, W)):
        for xx in range(max(x, 0), min(x + w, W)):
            cv[(xx, yy)] = c


def r_stroke_line(cv, W, x1, y1, x2, y2, w, c):
    """walk(): canonical order, Bresenham, a w x w brush with its top-left at p - (w - 1) / 2."""
    if x2 < x1 or (x2 == x1 and y2 < y1):
        x1, y1, x2, y2 = x2, y2, x1, y1
    dx, dy = x2 - x1, -abs(y2 - y1)
    sy = 1 if y1 < y2 else -1
    err, x, y = dx + dy, x1, y1
    while True:
        r_span(cv, W, x - (w - 1) // 2, y - (w - 1) // 2, w, w, c)
        if (x, y) == (x2, y2):
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x += 1
        if e2 <= dx:
            err += dx
            y += sy


def _edge_x(e, y):
    xa, ya, xb, yb = e
    num, den = (xb - xa) * (y - ya), yb - ya
    q = (2 * abs(num) + den) // (2 * den)
    return xa + (-q if num < 0 else q)


def r_triangle(cv, W, pts, c):
    """triangle(..., true): one span per row between the leftmost and rightmost edge crossing."""
    edges = []
    for (x1, y1), (x2, y2) in ((pts[0], pts[1]), (pts[1], pts[2]), (pts[2], pts[0])):
        edges.append((x2, y2, x1, y1) if (y2, x2) < (y1, x1) else (x1, y1, x2, y2))
    for y in range(max(min(e[1] for e in edges), 0), min(max(e[3] for e in edges), W - 1) + 1):
        xs = []
        for e in edges:
            if e[1] <= y <= e[3]:
                xs += [e[0], e[2]] if e[1] == e[3] else [_edge_x(e, y)]
        r_span(cv, W, min(xs), y, max(xs) - min(xs) + 1, 1, c)


def r_ring(cv, W, cx, cy, r, w, c):
    """strokeCircle(cx, cy, r, w, c) = arc(..., 0, 360, c, w): the ring [ri - 1/2, r + 1/2]."""
    ri = max(r - w + 1, 0)
    outer, inner = (2 * r + 1) ** 2, (2 * ri - 1) ** 2
    for dy in range(max(-r, -cy), min(r, W - 1 - cy) + 1):
        xo = isqrt((outer - 4 * dy * dy) // 4)
        xi = 0
        if ri > 0 and inner > 4 * dy * dy:
            q = (inner - 4 * dy * dy + 3) // 4
            s = isqrt(q)
            xi = s if s * s == q else s + 1
        for lo, hi in (((-xo, xo),) if xi == 0 else ((-xo, -xi), (xi, xo))):
            r_span(cv, W, cx + lo, cy + dy, hi - lo + 1, 1, c)


def _half_width(r, k):
    """circleHalfWidth() of SteamiDisplay.cpp: the filled disc's half span on row offset k."""
    if 2 * k * k - k < r * r:
        return (isqrt(4 * (r * r - k * k - 1) + 1) + 1) // 2
    return isqrt(r * r - k * k + k - 1)


def r_disc(cv, W, cx, cy, r, c):
    for y in range(max(cy - r, 0), min(cy + r, W - 1) + 1):
        h = _half_width(r, abs(y - cy))
        r_span(cv, W, cx - h, y, 2 * h + 1, 1, c)


def r_text(cv, W, s, x, y, f, c):
    """SteamiDisplay::text(s, x, y, c, f): 8x8 glyphs advancing 8f."""
    for code in glyphs(s):
        for i, col in enumerate(FONT8[code - FIRST_CODE]):
            for row in range(8):
                if col >> row & 1:
                    r_span(cv, W, x + i * f, y + row * f, f, f, c)
        x += 8 * f


def colour_box(cv, c):
    lit = [p for p, v in cv.items() if v == c]
    return box(lit) if lit else None


def outside_circle(cv, P):
    """The lit pixels beyond radius() + 1, as the memory backend counts them."""
    return [p for p in cv if (p[0] - P.cx) ** 2 + (p[1] - P.cy) ** 2 > (P.r + 1) ** 2]


# ---- Widget of 4b Task 2: watch() (spec 5.9 "watch", the decision "watch(h, m, s, c)") ----
# Points use Q14 (spec 5.6), not device.py's int(); fractions of R truncate
# (R / 2, 3R / 4, R·85/100).
WATCH_NUMBERS = (("12", 0), ("3", 90), ("6", 180), ("9", 270))


def watch_radii(W):
    """(R, major tick start, minor tick start, numbers, hour length, hour half width,
    minute length, minute half width, second length)."""
    P = Panel(W)
    R = P.r - P.u(8)
    return (R, R - P.u(8), R - P.u(5), R - P.u(15), R // 2, P.u(3), 3 * R // 4, P.u(2),
            R * 85 // 100)


def watch_angles(h, m, s):
    """Integer angles of the hour, minute and second hands; integer division, no clamping."""
    return (h % 12) * 30 + m // 2, m * 6 + s // 10, s * 6


def number_cell(P, text, deg, d):
    """glyphsAt(): the top-left of the 8x8 x k cell of text centred on polar(d, deg)."""
    x, y = P.polar(d, deg)
    return x - cell_width(text, count(text), TEXT, P.k) // 2, y - 4 * P.k


def pointer_points(P, deg, length, half_width):
    """pointer(): tip at (length, deg), base from (half_width, deg + 90) to
    (half_width, deg - 90)."""
    return P.polar(length, deg), P.polar(half_width, deg + 90), P.polar(half_width, deg - 90)


def watch_render(W, h, m, s, c="LIGHT"):
    """The canvas of watch(h, m, s, c) on a cleared W panel, in device.py's drawing order: dial,
    ticks, numbers, hour hand, minute hand, second hand, pivot."""
    P = Panel(W)
    R, major, minor, numbers, h_len, h_half, m_len, m_half, s_len = watch_radii(W)
    ha, ma, sa = watch_angles(h, m, s)
    cv = {}
    r_ring(cv, W, P.cx, P.cy, R, P.k, "DARK")
    for i in range(12):
        start, colour = (major, "LIGHT") if i % 3 == 0 else (minor, "GRAY")
        r_stroke_line(cv, W, *P.polar(start, 30 * i), *P.polar(R, 30 * i), P.k, colour)
    for text, deg in WATCH_NUMBERS:
        r_text(cv, W, text, *number_cell(P, text, deg, numbers), P.k, "WHITE")
    r_triangle(cv, W, pointer_points(P, ha, h_len, h_half), c)
    r_triangle(cv, W, pointer_points(P, ma, m_len, m_half), c)
    r_stroke_line(cv, W, *P.polar(0, sa), *P.polar(s_len, sa), P.k, "GRAY")
    r_disc(cv, W, P.cx, P.cy, P.u(3), "GRAY")
    return cv


def watch_layout(W, h, m, s):
    """The angles, the hand tips (hour, minute, second), the number cells and the WHITE ink box of
    the numbers of watch(h, m, s) on a W panel."""
    P = Panel(W)
    R, _, _, numbers, h_len, _, m_len, _, s_len = watch_radii(W)
    ha, ma, sa = watch_angles(h, m, s)
    return {
        "angles": (ha, ma, sa),
        "tips": (P.polar(h_len, ha), P.polar(m_len, ma), P.polar(s_len, sa)),
        "cells": {text: number_cell(P, text, deg, numbers) for text, deg in WATCH_NUMBERS},
        "white": colour_box(watch_render(W, h, m, s), "WHITE"),
    }


def device_watch_tips(W, h, m, s):
    """device.py's watch() tips (927e96b l.373-397), int() of float trigonometry."""
    cx = cy = W // 2
    r = W // 2 - 8 * W // 128

    def tip(length, deg):
        rad = math.radians(deg)
        return cx + int(length * math.sin(rad)), cy - int(length * math.cos(rad))

    return (tip(int(r * 0.50), (h % 12 + m / 60) * 30), tip(int(r * 0.75), (m + s / 60) * 6),
            tip(int(r * 0.85), s * 6))


# ---- Self-check of 4b Task 2 ----
assert watch_radii(128) == (56, 48, 51, 41, 28, 3, 42, 2, 47)
assert watch_radii(240) == (105, 90, 96, 77, 52, 5, 78, 3, 89)
assert watch_angles(10, 10, 30) == (305, 63, 180) and watch_angles(3, 0, 0) == (90, 0, 0)
assert watch_angles(3, 0, 30) == (90, 3, 180) and watch_angles(10, 11, 59) == (305, 71, 354)
# Review Focus 4: an out-of-range time keeps integer angles; 25 h draws as 1 h
assert watch_angles(25, 61, 61) == watch_angles(1, 61, 61) == (60, 372, 366)
assert watch_angles(255, 255, 255) == (90 + 127, 1530 + 25, 1530)
W128_300, W240_300 = watch_layout(128, 3, 0, 0), watch_layout(240, 3, 0, 0)
assert W128_300["tips"] == ((92, 64), (64, 22), (64, 17))
assert W240_300["tips"] == ((172, 120), (120, 42), (120, 31))
# The cardinal points are exact in sin and cos: device.py's int() tips equal polar() at 128
assert device_watch_tips(128, 3, 0, 0) == W128_300["tips"]
assert W128_300["cells"] == {"12": (56, 19), "3": (101, 60), "6": (60, 101), "9": (19, 60)}
assert W240_300["cells"] == {"12": (104, 35), "3": (189, 112), "6": (112, 189), "9": (35, 112)}
assert watch_layout(128, 10, 10, 30)["tips"] == ((41, 48), (101, 45), (64, 111))
assert watch_layout(240, 10, 10, 30)["tips"] == ((77, 90), (189, 85), (120, 209))
# watch(3, 0, 0, RED): the hour tip is RED and ends the RED box; the GRAY second hand, at the same
# 0 degrees and drawn after the minute hand, covers the minute tip;
# the second tip and the pivot GRAY
for W, layout in ((128, W128_300), (240, W240_300)):
    P = Panel(W)
    cv = watch_render(W, 3, 0, 0, "RED")
    (hour, minute, second) = layout["tips"]
    assert cv[hour] == "RED" and colour_box(cv, "RED")[2] == hour[0]
    assert cv[minute] == "GRAY" and cv[second] == "GRAY" and cv[(P.cx, P.cy)] == "GRAY"
    assert not outside_circle(cv, P)
# watch(3, 0, 30, RED): the minute hand at 3 degrees, the second hand pointing down; nothing covers
# the minute tip and no RED lies above it (the hour hand's RED starts at row 61 / 115)
MINUTE_TIP_330 = {128: (66, 22), 240: (124, 42)}
for W in (128, 240):
    cv = watch_render(W, 3, 0, 30, "RED")
    tip = watch_layout(W, 3, 0, 30)["tips"][1]
    assert tip == MINUTE_TIP_330[W] and cv[tip] == "RED" and colour_box(cv, "RED")[1] == tip[1]
    hour_only = {}
    P = Panel(W)
    r_triangle(hour_only, W, pointer_points(P, 90, watch_radii(W)[4], watch_radii(W)[5]), "RED")
    assert box(list(hour_only))[1] == {128: 61, 240: 115}[W]
# 10:10:30 and 10:11:59: the hour and minute tips LIGHT, the second tip GRAY (nothing drawn later
# covers them; the 10:11:59 second tip is clear of the "12" cell and the noon tick)
for W in (128, 240):
    for t in ((10, 10, 30), (10, 11, 59)):
        cv = watch_render(W, *t)
        hour, minute, second = watch_layout(W, *t)["tips"]
        assert (cv[hour], cv[minute], cv[second]) == ("LIGHT", "LIGHT", "GRAY"), (W, t)
# watch(1, 37, 5): no hand crosses a number, so the WHITE box is the box of the four numbers' ink
for W in (128, 240):
    P = Panel(W)
    numbers = {}
    for text, deg in WATCH_NUMBERS:
        r_text(numbers, W, text, *number_cell(P, text, deg, watch_radii(W)[3]), P.k, "WHITE")
    cv = watch_render(W, 1, 37, 5)
    assert all(cv[p] == "WHITE" for p in numbers)
    assert watch_layout(W, 1, 37, 5)["white"] == box(list(numbers))
# Review Focus 3 and 4: nothing outside radius() + 1, the strokes k px wide on every time tested
for W in (128, 240):
    P = Panel(W)
    for t in ((0, 0, 0), (3, 0, 0), (10, 10, 30), (10, 11, 59), (25, 61, 61), (255, 255, 255),
              (7, 22, 45)):
        assert not outside_circle(watch_render(W, *t), P), (W, t)

# ---- Values pinned by test_steami_screen (4b Task 2) ----
print("watch (R, major, minor, numbers, hour len/half, minute len/half, second len)")
for W in (128, 240):
    print(f"  {W}: {watch_radii(W)}")
for t in ((3, 0, 0), (3, 0, 30), (10, 10, 30), (10, 11, 59)):
    print(f"  watch{t} angles {watch_angles(*t)}, tips (hour, minute, second) "
          f"128 {watch_layout(128, *t)['tips']} 240 {watch_layout(240, *t)['tips']}")
for W in (128, 240):
    print(f"  {W} number cells {watch_layout(W, 3, 0, 0)['cells']}")
    print(f"  {W} watch(3, 0, 0, RED) RED box "
          f"{colour_box(watch_render(W, 3, 0, 0, 'RED'), 'RED')}; "
          f"watch(3, 0, 30, RED) RED box {colour_box(watch_render(W, 3, 0, 30, 'RED'), 'RED')}")
    print(f"  {W} watch(1, 37, 5) WHITE box {watch_layout(W, 1, 37, 5)['white']}")

# ---- Widget of 4b Task 3: compass() (spec 5.9 "compass", decision 2, the decision "compass") ----
# A real rose: every element at angle a is drawn at a - h. Points use Q14 (spec 5.6), not
# device.py's int(); fractions of R truncate (R·7/10, R·85/100). Draw order: circles, ticks,
# letters, north half, south half, pivot (the spec's listing order, so letters paint over ticks at
# diagonal headings).
COMPASS_LETTERS = (("N", 0, "WHITE"), ("E", 90, "GRAY"), ("S", 180, "GRAY"), ("W", 270, "GRAY"))


def compass_radii(W):
    """(R, inner circle, tick start, letters, needle length, needle half width, pivot)."""
    P = Panel(W)
    R = P.r - P.u(12)
    return R, R * 7 // 10, R - P.u(6), R + P.u(5), R * 85 // 100, P.u(3), P.u(3)


def compass_heading(heading):
    """roundedHeading(): lroundf(fmodf(h, 360)) on the float32 value of h, halves away from zero;
    None (no needle, rose at 0) for NAN and +-INF."""
    h = struct.unpack("<f", struct.pack("<f", heading))[0]
    if math.isnan(h) or math.isinf(h):
        return None
    m = math.fmod(h, 360.0)  # exact, as fmodf; m keeps the sign of h
    return int(math.copysign(math.floor(abs(m) + 0.5), m))


def compass_render(W, heading, c="LIGHT"):
    """The canvas of compass(heading, c) on a cleared W panel."""
    P = Panel(W)
    R, inner, tick, letters, needle, half, pivot = compass_radii(W)
    h = compass_heading(heading)
    turn = 0 if h is None else h
    cv = {}
    r_ring(cv, W, P.cx, P.cy, R, P.k, "DARK")
    r_ring(cv, W, P.cx, P.cy, inner, P.k, "DARK")
    for i in range(8):
        deg = 45 * i - turn
        colour = "LIGHT" if i % 2 == 0 else "DARK"
        r_stroke_line(cv, W, *P.polar(tick, deg), *P.polar(R, deg), P.k, colour)
    for text, deg, colour in COMPASS_LETTERS:
        r_text(cv, W, text, *number_cell(P, text, deg - turn, letters), P.k, colour)
    if h is not None:
        r_triangle(cv, W, pointer_points(P, -h, needle, half), c)
        r_triangle(cv, W, pointer_points(P, 180 - h, needle, half), "DARK")
    r_disc(cv, W, P.cx, P.cy, pivot, "GRAY")
    return cv


def compass_layout(W, heading):
    """R, the inner radius, the tick ends, the letter centres and cells, the needle tip and base,
    and the WHITE ink box of N of compass(heading) on a W panel."""
    P = Panel(W)
    R, inner, tick, letters, needle, half, _ = compass_radii(W)
    h = compass_heading(heading)
    turn = 0 if h is None else h
    return {
        "R": R,
        "inner": inner,
        "ticks": [(P.polar(tick, 45 * i - turn), P.polar(R, 45 * i - turn)) for i in range(8)],
        "centres": {text: P.polar(letters, deg - turn) for text, deg, _ in COMPASS_LETTERS},
        "cells": {text: number_cell(P, text, deg - turn, letters)
                  for text, deg, _ in COMPASS_LETTERS},
        "tip": None if h is None else P.polar(needle, -h),
        "base": None if h is None else pointer_points(P, -h, needle, half)[1:],
        "white": colour_box(compass_render(W, heading), "WHITE"),
    }


def device_compass_points(W):
    """device.py's compass() at heading 0 (927e96b l.303-341), int() of float trigonometry: the two
    radii, the eight ticks, the letter cells (lx - CHAR_W // 2, ly - CHAR_H // 2), the needle tip
    and base. device.py keeps the rose fixed and turns the needle by +heading (decision 2 turns the
    whole rose by -heading), so only heading 0 compares."""
    cx = cy = W // 2
    r = W // 2 - 12

    def at(d, deg):
        rad = math.radians(deg)
        return cx + int(d * math.sin(rad)), cy - int(d * math.cos(rad))

    cells = {}
    for text, deg, _ in COMPASS_LETTERS:
        lx, ly = at(r + 5, deg)
        cells[text] = (lx - 4, ly - 4)
    half = 3
    return {
        "R": r,
        "inner": int(r * 0.7),
        "ticks": [(at(r - 6, a), at(r, a)) for a in range(0, 360, 45)],
        "cells": cells,
        "tip": at(int(r * 0.85), 0),
        "base": {(cx - int(half * math.cos(0)), cy - int(half * math.sin(0))),
                 (cx + int(half * math.cos(0)), cy + int(half * math.sin(0)))},
    }


# Mockup 07 (micropython-steami-tutorials 83d8e2c docs/mockups/07_compass.svg): a 200 px drawing of
# the 128 px screen, centre (100, 100). Its numbers, held here as literals: rose circles r 81.2 and
# 56.2, the north tip at y 31.2, the needle base x 95.3 .. 104.7, the pivot r 4.7.
SVG07_SCALE = 200 / 128
SVG07 = {"R": 81.2, "inner": 56.2, "tip": 100 - 31.2, "half": 104.7 - 100, "pivot": 4.7}

# ---- Self-check of 4b Task 3 ----
assert compass_radii(128) == (52, 36, 46, 57, 44, 3, 3)
assert compass_radii(240) == (98, 68, 87, 107, 83, 5, 5)
# The needle tip and the N centre at 0, 90, 180 and 270 (spec 7: "compass() at 0, 90, 180, 270")
COMPASS_POINTS = {
    128: {0: ((64, 20), (64, 7)), 90: ((20, 64), (7, 64)), 180: ((64, 108), (64, 121)),
          270: ((108, 64), (121, 64))},
    240: {0: ((120, 37), (120, 13)), 90: ((37, 120), (13, 120)), 180: ((120, 203), (120, 227)),
          270: ((203, 120), (227, 120))},
}
for W, points in COMPASS_POINTS.items():
    for heading, (tip, north) in points.items():
        layout = compass_layout(W, heading)
        assert (layout["tip"], layout["centres"]["N"]) == (tip, north), (W, heading)
# The N cell's top-left is its centre - 4k: (60, 3) on 128 and (112, 5) on 240 at heading 0
assert compass_layout(128, 0)["cells"]["N"] == (60, 3)
assert compass_layout(240, 0)["cells"]["N"] == (112, 5)
# Spec 5.9: at 90 the N letter is on the left
for W in (128, 240):
    assert compass_layout(W, 90)["white"][2] < Panel(W).cx
# device.py's int() equals polar() at 128, heading 0, wherever sin and cos are exact: the circles,
# the four cardinal ticks, the letters, the needle tip and base. The four diagonal ticks differ:
# Q14 rounds to nearest where int() truncates (inner end x 97 here against 96 in device.py at 45
# degrees, outer offset 37 against 36), so they are not compared.
DEV07, OURS07 = device_compass_points(128), compass_layout(128, 0)
assert (DEV07["R"], DEV07["inner"]) == (OURS07["R"], OURS07["inner"])
assert DEV07["ticks"][0::2] == OURS07["ticks"][0::2]
assert DEV07["ticks"][1][0] == (96, 32) and OURS07["ticks"][1][0] == (97, 31)
assert DEV07["cells"] == OURS07["cells"] and DEV07["tip"] == OURS07["tip"]
assert DEV07["base"] == set(OURS07["base"])
# Mockup 07's SVG agrees at 128: circles 52 and 36, tip 44, half width 3, pivot 3
R7, I7, _, _, N7, H7, PV7 = compass_radii(128)
SVG07_PX = [round(SVG07[key] / SVG07_SCALE) for key in ("R", "inner", "tip", "half", "pivot")]
assert SVG07_PX == [R7, I7, N7, H7, PV7]
# roundedHeading (Review Focus 2): NAN and +-INF draw no needle; halves and wraps
assert [compass_heading(v) for v in (NAN, INF, -INF)] == [None, None, None]
assert ([compass_heading(v) for v in (359.6, 0.4, -90.4, 450, 270, 0.5, -0.5)]
        == [360, 0, -90, 90, 270, 1, -1])
assert compass_heading(1e30) is not None
# Drawings that the rounding makes equal (test_compass_heading_rounding)
for W in (128, 240):
    assert compass_render(W, 359.6) == compass_render(W, 0) == compass_render(W, 0.4)
    assert compass_render(W, -90.4) == compass_render(W, 270)
    assert compass_render(W, 450) == compass_render(W, 90)
    assert "RED" in compass_render(W, 1e30, "RED").values()
# compass(NAN): no needle, the rose and the letters of heading 0, the pivot
for W in (128, 240):
    P = Panel(W)
    for v in (NAN, INF, -INF):
        cv = compass_render(W, v, "RED")
        assert "RED" not in cv.values() and cv[(P.cx, P.cy)] == "GRAY"
        north = {p for p, c in compass_render(W, 0).items() if c == "WHITE"}
        assert {p for p, c in cv.items() if c == "WHITE"} == north
# compass(h, RED): the tip is RED and the RED box reaches it on its side; the WHITE box is the N box
# (nothing is drawn over the letter); the pivot GRAY
SIDE = {0: 1, 90: 0, 180: 3, 270: 2}  # index into (x0, y0, x1, y1)
for W in (128, 240):
    P = Panel(W)
    for heading in (0, 90, 180, 270):
        cv, layout = compass_render(W, heading, "RED"), compass_layout(W, heading)
        tip = layout["tip"]
        assert cv[tip] == "RED" and colour_box(cv, "RED")[SIDE[heading]] == tip[SIDE[heading] % 2]
        n_only = {}
        r_text(n_only, W, "N", *layout["cells"]["N"], P.k, "WHITE")
        assert all(cv[p] == "WHITE" for p in n_only)
        assert colour_box(cv, "WHITE") == box(list(n_only))
        assert cv[(P.cx, P.cy)] == "GRAY"
# compass(45): the N tick turned to 315 is LIGHT, the NE diagonal tick turned to 0 DARK, at the
# tick start R - u(6); the needle (44 / 83) reaches neither
for W in (128, 240):
    P = Panel(W)
    R, _, tick, _, needle, _, _ = compass_radii(W)
    cv = compass_render(W, 45, "RED")
    assert cv[P.polar(tick, 315)] == "LIGHT" and cv[P.polar(tick, 0)] == "DARK" and needle < tick
# Review Focus 3: nothing outside radius() + 1 at any heading
for W in (128, 240):
    P = Panel(W)
    for heading in range(360):
        assert not outside_circle(compass_render(W, heading), P), (W, heading)

# ---- Values pinned by test_steami_screen (4b Task 3) ----
print("compass (R, inner, tick start, letters, needle, half width, pivot)")
for W in (128, 240):
    print(f"  {W}: {compass_radii(W)}")
for W in (128, 240):
    P = Panel(W)
    R, _, tick, _, _, _, _ = compass_radii(W)
    for heading in (0, 90, 180, 270):
        layout = compass_layout(W, heading)
        print(f"  {W} heading {heading}: tip {layout['tip']}, N centre {layout['centres']['N']}, "
              f"N cell {layout['cells']['N']}, WHITE box {layout['white']}, "
              f"RED box {colour_box(compass_render(W, heading, 'RED'), 'RED')}")
    print(f"  {W} compass(45): N tick start {P.polar(tick, 315)}, "
          f"NE tick start {P.polar(tick, 0)}")

# ---- Widget of 4b Task 4: gauge() (spec 5.8 table, 5.9 "gauge", the decisions "gauge" and
# "gaugeLabel") ----
# Track and fill are arc()s of the same outer radius and width from 225 degrees (noon, clockwise;
# device.py's 135 in screen-math angles), drawn in device.py's order: track, fill, min label, max
# label. A label is the integer part of minV or maxV ("--" past 1e9) in 5x7 x k GRAY, its top row
# on the point at 233 / 127 degrees, centred on it by its cell width, then slid into the chord at
# its inked rows shrunk by k (the 4a corner rule B10, ruling 3). device.py truncates its label
# points with int(): its min point on 128 is (29, 89) where Q14 rounding gives (30, 90). The cut in
# drawGaugeLabel (fitGlyphs) is a guard that no label of at most 10 glyphs reaches: the chord at
# the label rows leaves about 109 / 201 px.
GAUGE_START, GAUGE_SWEEP, GAUGE_MIN_ANGLE, GAUGE_MAX_ANGLE = 225, 270, 233, 127
FLT_MAX = struct.unpack("<f", b"\xff\xff\x7f\x7f")[0]


def f32(x):
    """x rounded to a float32, as the C++ holds it (+-INF past FLT_MAX)."""
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def gauge_sweep(v, min_v, max_v):
    """gaugeSweep(): lroundf(270.0f * ratio), ratio = (v - minV) / (maxV - minV) in float32,
    clamped to [0, 1]; 0 when maxV == minV or the ratio is NAN (NAN in any argument, INF - INF).
    Halves away from zero, never Python's round() (ruling 5: 270 * 0.75 = 202.5 gives 203)."""
    v, min_v, max_v = f32(v), f32(min_v), f32(max_v)
    if max_v == min_v:
        return 0
    ratio = f32(f32(v - min_v) / f32(max_v - min_v))
    if math.isnan(ratio) or ratio <= 0:
        return 0
    if ratio >= 1:
        return GAUGE_SWEEP
    return math.floor(f32(GAUGE_SWEEP * ratio) + 0.5)


def gauge_label_text(v):
    """gaugeLabel()'s text: the integer part of the float32 v, truncated toward zero as
    device.py's int(), when |v| < 1e9; otherwise "--" (NAN, +-INF, huge)."""
    v = f32(v)
    return str(int(v)) if abs(v) < 1e9 else "--"


def gauge_layout(W, min_v, max_v, arc_width=0):
    """The outer radius, the ring width, the label distance and, per label (min then max), its
    text, its point, the glyphs kept, its left column, its lit pixels and their box."""
    P = Panel(W)
    outer = P.r - P.u(4)
    w = arc_width if arc_width else min(max(5, P.r // 9), 255)
    d = max(0, outer - w - P.u(10))
    panel = (0, P.W - 1)
    labels = []
    for v, deg in ((min_v, GAUGE_MIN_ANGLE), (max_v, GAUGE_MAX_ANGLE)):
        text = gauge_label_text(v)
        px, py = P.polar(d, deg)
        n = P.fit_glyphs(text, LABEL, P.k, py, panel, P.k, 0)
        x = clamp_into(P, text, n, LABEL, py, P.k, panel, px - cell_width(text, n, LABEL, P.k) // 2)
        lit = draw_glyphs(text, n, LABEL, x, py, P.k)
        labels.append({"text": text, "point": (px, py), "kept": n, "x": x, "lit": lit,
                       "box": box(lit) if lit else None})
    return {"outer": outer, "width": w, "d": d, "labels": labels}


def r_arc(cv, W, cx, cy, r, start, sweep, w, c):
    """arc(cx, cy, r, start, sweep, c, w): the ring [ri - 1/2, r + 1/2] of r_ring, under a full
    turn cut to the wedges [s, s + 180] and [s + 180, s + sweep] of SteamiDisplay.cpp's Sector
    (Q14 directions)."""
    if sweep <= 0 or w == 0 or r < 0:
        return
    if sweep >= 360:
        r_ring(cv, W, cx, cy, r, w, c)
        return
    end = start + sweep
    mid = start + 180 if sweep > 180 else end
    wedges = [(start, mid)] + ([(mid, end)] if mid != end else [])

    def holds(dx, dy):
        for s, e in wedges:
            sx, sy, ex, ey = sin_q14(s), -cos_q14(s), sin_q14(e), -cos_q14(e)
            if sx * dy - sy * dx >= 0 and dx * ey - dy * ex >= 0:
                return True
        return False

    ring = {}
    r_ring(ring, W, cx, cy, r, w, c)
    for (x, y) in ring:
        if holds(x - cx, y - cy):
            cv[(x, y)] = c


def gauge_render(W, v, min_v=0, max_v=100, fill="LIGHT", arc_width=0):
    """The canvas of gauge(v, minV, maxV, c, arcWidth) on a cleared W panel; fill names the
    colour the panel shows for c (lifted on the grey panel)."""
    P = Panel(W)
    layout = gauge_layout(W, min_v, max_v, arc_width)
    cv = {}
    r_arc(cv, W, P.cx, P.cy, layout["outer"], GAUGE_START, GAUGE_SWEEP, layout["width"], "DARK")
    r_arc(cv, W, P.cx, P.cy, layout["outer"], GAUGE_START, gauge_sweep(v, min_v, max_v),
          layout["width"], fill)
    for label in layout["labels"]:
        for p in label["lit"]:
            cv[p] = "GRAY"
    return cv


def colour_set(cv, c):
    return {p for p, v in cv.items() if v == c}


def gauge_gray(W, min_v, max_v):
    """(GRAY box, GRAY count) of a gauge with these labels: the union of the two labels' ink."""
    lit = set()
    for label in gauge_layout(W, min_v, max_v)["labels"]:
        lit |= label["lit"]
    return box(lit), len(lit)


def device_gauge_points(W):
    """device.py's label points (927e96b l.200-223): int() of float trigonometry on r_label =
    radius - arc_w // 2 - 1 - arc_w - 10, at 143 and 37 degrees in screen-math angles."""
    cx = cy = W // 2
    arc_w = max(5, (W // 2) // 9)
    r_label = (W // 2 - arc_w // 2 - 1) - arc_w - 10
    return tuple((int(cx + r_label * math.cos(math.radians(a))),
                  int(cy + r_label * math.sin(math.radians(a))))
                 for a in (143, 37))


# ---- Self-check of 4b Task 4 ----
G128, G240 = gauge_layout(128, 0, 100), gauge_layout(240, 0, 100)
assert (G128["outer"], G128["width"], G128["d"]) == (60, 7, 43)  # ring 54..60, spec 5.8 table
assert (G240["outer"], G240["width"], G240["d"]) == (113, 13, 82)  # ring 101..113
assert [G128["outer"] - G128["width"] + 1, G240["outer"] - G240["width"] + 1] == [54, 101]
assert [label["point"] for label in G128["labels"]] == [(30, 90), (98, 90)]
assert [label["point"] for label in G240["labels"]] == [(55, 169), (185, 169)]
assert device_gauge_points(128) == ((29, 89), (98, 89))  # int() against Q14's (30, 90) and (98, 90)
assert gauge_sweep(342, 0, 500) == 185 and gauge_sweep(50, 0, 100) == 135
assert gauge_sweep(2.5, 10, 0) == gauge_sweep(7.5, 0, 10) == 203  # ratio 0.75: 202.5 rounds away
assert round(270 * 0.75) == 202  # why the script never uses round() (ruling 5)
# Review Focus 2: an empty or a full fill, never a NAN sweep
for v, lo, hi in ((NAN, 0, 500), (-INF, 0, 500), (-1, 0, 500), (5, 7, 7), (5, NAN, 10),
                  (INF, 0, INF), (5, 0, NAN), (INF, INF, INF), (-INF, -INF, 10)):
    assert gauge_sweep(v, lo, hi) == 0, (v, lo, hi)
for v, lo, hi in ((INF, 0, 500), (600, 0, 500), (100, 0, 100), (-20, 10, -10)):
    assert gauge_sweep(v, lo, hi) == GAUGE_SWEEP, (v, lo, hi)
assert [gauge_label_text(v)
        for v in (-0.5, 99.9, 1e9, -1e9, NAN, INF, -INF, 999999999, -100000000.0)] == \
    ["0", "99", "--", "--", "--", "--", "--", "--", "-100000000"]  # f32(999999999) is 1e9
assert gauge_label_text(999999900) == "999999872"  # the float32 below 1e9 keeps its digits
# The ring on row centerY(), left of the centre: x 4..10 / 7..19 for the default width,
# 4..6 / 7..9 for 3
for W, ring, thin in ((128, (4, 10), (4, 6)), (240, (7, 19), (7, 9))):
    P = Panel(W)
    for width_, span in ((0, ring), (3, thin)):
        row = sorted(x for (x, y), c in gauge_render(W, 0, arc_width=width_).items()
                     if y == P.cy and x < P.cx and c == "DARK")
        assert row == list(range(span[0], span[1] + 1)), (W, width_, row)
    # the gap from 135 to 225: the bottom of the ring is not drawn
    outer = gauge_layout(W, 0, 100)["outer"]
    assert (P.cx, P.cy + outer - 2) not in gauge_render(W, 0)
# Spec 7: gauge(50) lights half of the track within 2 %; track and fill of equal thickness
for W in (128, 240):
    half, full = colour_set(gauge_render(W, 50), "LIGHT"), colour_set(gauge_render(W, 100), "LIGHT")
    assert 0.48 <= len(half) / len(full) <= 0.52, (W, len(half), len(full))
    assert full == colour_set(gauge_render(W, 0), "DARK")
# The labels of the examples stay centred on their point (the slide never moves them)
for W in (128, 240):
    P = Panel(W)
    for lo, hi in ((0, 100), (0, 500), (0, 1050)):
        for label in gauge_layout(W, lo, hi)["labels"]:
            text, (px, _) = label["text"], label["point"]
            assert label["kept"] == count(text)
            assert label["x"] == px - cell_width(text, count(text), LABEL, P.k) // 2
# A long label slides inward and stays whole (10 glyphs at most): on 128 and 240 the two slid labels
# of (-1e8, 1e8) overlap (128: min cell 10..69, max cell 65..118; 240: min cell 20..139, max cell
# 113..220), a known difference of the PR; no example draws such labels
for W in (128, 240):
    for lo, hi in ((-100000000.0, 100000000.0), (-999999872, 999999872), (1e9, -1e9)):
        for label in gauge_layout(W, lo, hi)["labels"]:
            assert label["kept"] == count(label["text"]), (W, lo, hi, label["text"])
L128 = gauge_layout(128, -100000000.0, 100000000.0)["labels"]
assert [(label["x"], label["x"] + cell_width(label["text"], label["kept"], LABEL, 1) - 1)
        for label in L128] == [(10, 69), (65, 118)]
L240 = gauge_layout(240, -100000000.0, 100000000.0)["labels"]
assert [(label["x"], label["x"] + cell_width(label["text"], label["kept"], LABEL, 2) - 1)
        for label in L240] == [(20, 139), (113, 220)]
# On 240 a min label of 3 digits such as "100" paints over the ring ends by a few pixels
# (known difference)
P = Panel(240)
touch = sorted(gauge_layout(240, 100, 500)["labels"][0]["lit"]
               & colour_set(gauge_render(240, 0, 0, 500), "DARK"))
assert touch == [(39, 181), (39, 182), (40, 181), (40, 182)], touch
assert not (gauge_layout(240, 0, 500)["labels"][0]["lit"]
            & colour_set(gauge_render(240, 0, 0, 500), "DARK"))
# Review Focus 3: nothing outside radius() + 1, whatever the fill, the labels or the width
for W in (128, 240):
    P = Panel(W)
    for lo, hi in ((0, 100), (-999, 99999), (-100000000.0, 100000000.0)):
        for v in (-10, 50, 110):
            assert not outside_circle(gauge_render(W, v, lo, hi), P), (W, v, lo, hi)
    for width_ in (3, 64, 255):
        assert not outside_circle(gauge_render(W, 100, 0, 100, arc_width=width_), P), (W, width_)

# ---- Values pinned by test_steami_screen (4b Task 4) ----
print("gauge (outer, width, label distance; label points)")
for W, G in ((128, G128), (240, G240)):
    min_point, max_point = (label["point"] for label in G["labels"])
    print(f"  {W}: {G['outer']} {G['width']} {G['d']}; {min_point} {max_point}")
print("gauge GRAY labels (box, count): (minV, maxV) -> texts")
for W in (128, 240):
    for lo, hi in ((0, 100), (0, 500), (-0.5, 99.9), (1e9, -1e9), (-100000000.0, 100000000.0)):
        texts = [label["text"] for label in gauge_layout(W, lo, hi)["labels"]]
        gray_box, gray_count = gauge_gray(W, lo, hi)
        print(f"  {W} ({lo!r}, {hi!r}) {texts}: {gray_box} {gray_count}")
for W in (128, 240):
    print(f"  {W} gauge(50) LIGHT {len(colour_set(gauge_render(W, 50), 'LIGHT'))}, "
          f"gauge(100) LIGHT {len(colour_set(gauge_render(W, 100), 'LIGHT'))}, "
          f"gauge(342, 0, 500) LIGHT {len(colour_set(gauge_render(W, 342, 0, 500), 'LIGHT'))}")

# ---- Mockups of spec section 6, as SteamiMockups.h draws them (4b Task 5) ----
# Each mockup is composed from the widget goldens above in its program's drawing order, later calls
# painting over earlier ones. Colours are names; on the grey panel each name is one grey level and
# the names used here are distinct levels (GREY_LEVELS), so a box per name is the suite's inkBox per
# colour.
GREY_LEVELS = {"GRID_DARK": 3, "DARK": 6, "GRAY": 9, "LIGHT": 11, "GREEN": 12, "YELLOW": 14,
               "WHITE": 15}
MOOD_FACES = ("happy", "sad", "surprised", "sleeping", "angry", "love")  # SteamiFace order


def paint(cv, lit, c):
    for p in lit:
        cv[p] = c


def m_title(cv, W, s, c="GRAY"):
    P = Panel(W)
    x, y, n = text_layout(P, s, "N", 1)
    paint(cv, draw_glyphs(s, n, TEXT, x, y, glyph_factor(P, 1)), c)


def m_subtitle(cv, W, lines, c="DARK"):
    P = Panel(W)
    for line, (x, y, n) in zip(lines, subtitle_layout(P, lines), strict=False):
        paint(cv, draw_glyphs(line, n, TEXT, x, y, P.k), c)


def m_value(cv, W, t, unit=None, at="CENTER", label=None, scale=2, y_offset=0):
    got = value_layout(W, t, at, unit, label, scale, y_offset)
    paint(cv, got["value_lit"], "WHITE")
    paint(cv, got["label_lit"], "GRAY")
    paint(cv, got["unit_lit"], "LIGHT")


def m_bar(cv, W, v, max_v, y_offset, c):
    for y, ((t0, t1), f) in bar_spans(W, v, max_v, y_offset).items():
        paint(cv, {(x, y) for x in range(t0, t1 + 1)}, "GRID_DARK")
        if f:
            paint(cv, {(x, y) for x in range(f[0], f[1] + 1)}, c)


def mockup_01(W, t, chip):
    cv = {}
    m_title(cv, W, "Temperature")
    m_value(cv, W, t, DEG_C)
    m_subtitle(cv, W, (chip,))
    return cv


def mockup_02(W, percent, volts):
    cv = {}
    m_title(cv, W, "Battery")
    m_value(cv, W, str(percent), "%", y_offset=-12)
    m_bar(cv, W, percent, 100, -12, "GREEN")
    m_subtitle(cv, W, (volts, "BQ27441"))
    return cv


def mockup_03(W, t, h, comfort, chip):
    P = Panel(W)
    cv = {}
    m_title(cv, W, "Comfort")
    r_span(cv, W, P.cx, P.cy - P.r // 2, 1, P.r, "DARK")  # vline(centerX(), centerY() - r / 2, r)
    m_value(cv, W, t, DEG_C, "W", "TEMP")
    m_value(cv, W, h, "%", "E", "HUM")
    m_subtitle(cv, W, (comfort, chip), "GREEN")
    return cv


def mockup_04(W, d):
    """mockupGauge(d): gauge(d, 0, 500) first, then the text paints over the ring."""
    cv = gauge_render(W, d, 0, 500)
    m_title(cv, W, "Distance")
    m_value(cv, W, str(d), "mm")
    m_subtitle(cv, W, ("VL53L1X ToF",))
    return cv


def mockup_08(W, shown, d):
    """mockupFaces(shown, d): FACES[shown] alone for 0..5; 6 is the reactive mood of distance d."""
    cv = {}
    if shown < 6:
        mood, line = MOOD_FACES[shown], None
    else:
        m_title(cv, W, "Mood")
        mood, line = (("surprised", ("SURPRISED", "YELLOW")) if d < 50 else
                      ("happy", ("HAPPY", "GREEN")) if d < 150 else
                      ("sleeping", ("SLEEPING", "LIGHT")) if d < 300 else ("sad", ("SAD", "RED")))
    for x, y, w, h in face_cells(W, FACES[mood], False):
        r_span(cv, W, x, y, w, h, "LIGHT")
    if line:
        m_subtitle(cv, W, (line[0],), line[1])
    return cv


def mockup_09(W, s):
    return watch_render(W, (s // 3600) % 24, (s // 60) % 60, s % 60)


def band_box(cv, c, row_from=0, row_to=10 ** 6):
    lit = [p for p, v in cv.items() if v == c and row_from <= p[1] <= row_to]
    return box(lit) if lit else None


WATCH_START = 10 * 3600 + 10 * 60 + 30  # mockup 09: 10:10:30
# The suite's own literals for mockups 01 to 03 (test_steami_screen_mockups): name, W, colour, rows,
# box. The composed renders must give them back.
PINNED_4A = (
    ("01", 128, "GRAY", None, (21, 20, 106, 27)), ("01", 128, "WHITE", None, (38, 50, 89, 63)),
    ("01", 128, "LIGHT", None, (57, 66, 70, 72)), ("01", 128, "DARK", None, (29, 108, 98, 114)),
    ("01", 240, "GRAY", None, (34, 41, 205, 56)), ("01", 240, "WHITE", None, (68, 91, 171, 118)),
    ("01", 240, "LIGHT", None, (106, 123, 133, 136)),
    ("01", 240, "DARK", None, (74, 202, 165, 215)),
    ("02", 128, "WHITE", None, (50, 38, 77, 51)), ("02", 128, "LIGHT", None, (61, 54, 66, 60)),
    ("02", 128, "GRID_DARK", None, (83, 72, 107, 79)), ("02", 128, "GREEN", None, (20, 72, 82, 79)),
    ("02", 128, "DARK", (95, 105), (37, 95, 90, 101)),
    ("02", 128, "DARK", (106, 127), (37, 106, 90, 112)),
    ("02", 240, "WHITE", None, (92, 69, 147, 96)), ("02", 240, "LIGHT", None, (114, 101, 125, 114)),
    ("02", 240, "GRID_DARK", None, (156, 135, 202, 149)),
    ("02", 240, "GREEN", None, (38, 135, 155, 149)),
    ("02", 240, "DARK", (177, 196), (66, 177, 173, 190)),
    ("02", 240, "DARK", (197, 239), (66, 197, 173, 210)),
    ("03", 128, "DARK", None, (64, 32, 64, 95)), ("03", 128, "WHITE", None, (6, 50, 122, 63)),
    ("03", 128, "LIGHT", None, (24, 66, 99, 72)), ("03", 128, "GRAY", (0, 30), (37, 20, 90, 26)),
    ("03", 128, "GRAY", (31, 127), (22, 39, 102, 45)),
    ("03", 128, "GREEN", (95, 105), (21, 95, 106, 101)),
    ("03", 128, "GREEN", (106, 127), (29, 106, 98, 112)),
    ("03", 240, "DARK", None, (120, 60, 120, 179)), ("03", 240, "WHITE", None, (21, 96, 219, 116)),
    ("03", 240, "LIGHT", None, (45, 120, 187, 133)),
    ("03", 240, "GRAY", (0, 60), (66, 37, 173, 50)),
    ("03", 240, "GRAY", (61, 239), (39, 75, 193, 88)),
    ("03", 240, "GREEN", (173, 192), (34, 173, 205, 186)),
    ("03", 240, "GREEN", (193, 239), (74, 193, 165, 206)),
    # test_mockups_without_sensors: 02 with no gauge, 03 with no sensor
    ("02 none", 128, "GRID_DARK", None, (20, 72, 107, 79)),
    ("02 none", 128, "DARK", (95, 105), (57, 98, 70, 98)),
    ("02 none", 128, "DARK", (106, 127), (37, 106, 90, 112)),
    ("02 none", 240, "GRID_DARK", None, (38, 135, 202, 149)),
    ("02 none", 240, "DARK", (177, 196), (106, 183, 133, 184)),
    ("02 none", 240, "DARK", (197, 239), (66, 197, 173, 210)),
    ("03 none", 128, "GREEN", None, (13, 86, 114, 92)),
    ("03 none", 240, "GREEN", None, (18, 151, 221, 164)),
)
CHIP_OF = {128: "WSEN-HIDS", 240: "BME280"}


def mockup_4a(name, W):
    return {"01": lambda: mockup_01(W, "23.5", CHIP_OF[W]),
            "02": lambda: mockup_02(W, 72, "3842 mV"),
            "03": lambda: mockup_03(W, "23.5", "45.3", "Comfortable", CHIP_OF[W]),
            "02 none": lambda: mockup_02(W, 0, "--"),
            "03 none": lambda: mockup_03(W, "--", "--", "Uncomfortable", "")}[name]()


# ---- Self-check of 4b Task 5 ----
assert len(set(GREY_LEVELS.values())) == len(GREY_LEVELS)
for name, W, c, rows, expected in PINNED_4A:
    got = band_box(mockup_4a(name, W), c, *(rows or ()))
    assert got == expected, (name, W, c, rows, got)
for W in (128, 240):
    assert not outside_circle(mockup_4a("01", W), Panel(W))
# 04: the fill ends at 50 degrees (sweep 185 from 225), in the middle of the ring; the text paints
# over the ring but leaves the two points clear
FILL_END = {128: ((104, 24), (111, 31)), 240: ((196, 44), (208, 59))}
for W in (128, 240):
    P = Panel(W)
    G = gauge_layout(W, 0, 500)
    mid = G["outer"] - G["width"] // 2  # 57 / 107
    # the sweep starts at 225 degrees, so 185 degrees of sweep end at (225 + 185) % 360 = 50
    assert mid == {128: 57, 240: 107}[W] and gauge_sweep(342, 0, 500) == 185
    assert (P.polar(mid, 45), P.polar(mid, 55)) == FILL_END[W]
    cv = mockup_04(W, 342)
    assert cv[P.polar(mid, 45)] == "LIGHT" and cv[P.polar(mid, 55)] == "DARK", W
    assert not outside_circle(cv, P)
    # the GRAY band holds the two labels "0" and "500" only
    band = (85, 100) if W == 128 else (160, 190)
    assert band_box(cv, "GRAY", *band) == gauge_gray(W, 0, 500)[0]
    assert band_box(cv, "WHITE") == value_layout(W, "342", "CENTER", "mm")["value"]
    # Review Focus 1, d = -1: no fill, and the WHITE "-1" of value(-1, "mm") alone
    none = mockup_04(W, -1)
    assert "LIGHT" not in {c for p, c in none.items() if (G["outer"] - G["width"]) ** 2 <=
                           (p[0] - P.cx) ** 2 + (p[1] - P.cy) ** 2 <= (G["outer"] + 1) ** 2}
    assert colour_set(none, "WHITE") == value_layout(W, "-1", "CENTER", "mm")["value_lit"]
# 07 at heading 0: the needle tip LIGHT, the N WHITE. Review Focus 1, NAN: the same N, and no LIGHT
# within 45 / 84 px of the centre (the needle is gone; the LIGHT ticks start at 46 / 87)
for W, tip, near in ((128, (64, 20), 45), (240, (120, 37), 84)):
    P = Panel(W)
    cv, none = compass_render(W, 0.0), compass_render(W, NAN)
    assert cv[tip] == "LIGHT" and colour_set(none, "WHITE") == colour_set(cv, "WHITE")
    assert all((x - P.cx) ** 2 + (y - P.cy) ** 2 > near * near
               for (x, y) in colour_set(none, "LIGHT"))
    assert any((x - P.cx) ** 2 + (y - P.cy) ** 2 <= near * near
               for (x, y) in colour_set(cv, "LIGHT"))
    assert compass_radii(W)[2] == {128: 46, 240: 87}[W]
# 08: each face alone is LIGHT only; the reactive mood at 100 mm is HAPPY under "Mood", "HAPPY" in
# GREEN; at -1 mm SURPRISED in YELLOW
for W in (128, 240):
    P = Panel(W)
    for i, _mood in enumerate(MOOD_FACES):
        cv = mockup_08(W, i, 0)
        assert set(cv.values()) == {"LIGHT"} and not outside_circle(cv, P)
    cv = mockup_08(W, 6, 100)
    assert subtitle_rows(W, ("HAPPY",))[0][0] == {128: 108, 240: 202}[W]
    assert title_row(W, "Mood")[0] == {128: 20, 240: 37}[W]
    assert colour_box(cv, "GREEN") == subtitle_rows(W, ("HAPPY",))[0][2]
    assert colour_box(cv, "GRAY") == text_box(W, "Mood", "N")
    # the title is clear of the face
    assert colour_set(cv, "LIGHT") == colour_set(mockup_08(W, 0, 0), "LIGHT")
    none = mockup_08(W, 6, -1)
    assert colour_box(none, "YELLOW") == subtitle_rows(W, ("SURPRISED",))[0][2]
    assert colour_set(none, "LIGHT") == colour_set(mockup_08(W, 2, 0), "LIGHT")
    assert not outside_circle(cv, P) and not outside_circle(none, P)
# 09 at 10:10:30: the "12" is the WHITE ink in the rows above the "3" and "9" cells (rows 0..59 /
# 0..111; the rows above centerY() would also hold the top of the "3" and the "9"); hour and
# minute tips LIGHT
TWELVE_ROWS = {128: (0, 59), 240: (0, 111)}
for W in (128, 240):
    P = Panel(W)
    cv = mockup_09(W, WATCH_START)
    cells = watch_layout(W, 10, 10, 30)["cells"]
    assert TWELVE_ROWS[W][1] == cells["3"][1] - 1 == cells["9"][1] - 1
    twelve = {}
    r_text(twelve, W, "12", *cells["12"], P.k, "WHITE")
    assert all(cv[p] == "WHITE" for p in twelve)
    assert band_box(cv, "WHITE", *TWELVE_ROWS[W]) == box(list(twelve))
    assert band_box(cv, "WHITE", 0, P.cy - 1) != box(list(twelve))
    hour, minute, _ = watch_layout(W, 10, 10, 30)["tips"]
    assert (cv[hour], cv[minute]) == ("LIGHT", "LIGHT")

# ---- Values pinned by test_steami_screen_mockups (4b Task 5) ----
print("mockups 01 to 03 (suite literals regenerated: name W colour rows box)")
for name, W, c, rows, _ in PINNED_4A:
    print(f"  {name:<7} {W} {c:<9} {rows or ''!s:<10} "
          f"{band_box(mockup_4a(name, W), c, *(rows or ()))}")
print("mockup 04 gauge(342, 0, 500): WHITE box, GRAY labels box (rows), "
      "fill end polar(mid, 45) LIGHT, (mid, 55) DARK")
for W in (128, 240):
    P = Panel(W)
    cv = mockup_04(W, 342)
    mid = gauge_layout(W, 0, 500)["outer"] - gauge_layout(W, 0, 500)["width"] // 2
    band = (85, 100) if W == 128 else (160, 190)
    print(f"  {W}: WHITE {colour_box(cv, 'WHITE')}, GRAY {band} {band_box(cv, 'GRAY', *band)}, "
          f"polar({mid}, 45) {P.polar(mid, 45)}, polar({mid}, 55) {P.polar(mid, 55)}; "
          f"d -1 WHITE {colour_box(mockup_04(W, -1), 'WHITE')}")
print("mockup 07 compass(0): WHITE N box, needle tip")
for W in (128, 240):
    print(f"  {W}: WHITE {colour_box(compass_render(W, 0.0), 'WHITE')}, "
          f"tip {compass_layout(W, 0)['tip']}")
print("mockup 08 reactive at 100 mm: GRAY Mood, LIGHT face, GREEN HAPPY; at -1 mm YELLOW SURPRISED")
for W in (128, 240):
    cv = mockup_08(W, 6, 100)
    print(f"  {W}: GRAY {colour_box(cv, 'GRAY')}, LIGHT {colour_box(cv, 'LIGHT')}, "
          f"GREEN {colour_box(cv, 'GREEN')}; YELLOW {colour_box(mockup_08(W, 6, -1), 'YELLOW')}")
print("mockup 09 watch at 10:10:30: WHITE 12 (rows above the 3 and 9 cells), hour and minute tips")
for W in (128, 240):
    hour, minute = watch_layout(W, 10, 10, 30)["tips"][:2]
    print(f"  {W}: WHITE {TWELVE_ROWS[W]} "
          f"{band_box(mockup_09(W, WATCH_START), 'WHITE', *TWELVE_ROWS[W])}, tips {hour} {minute}")
