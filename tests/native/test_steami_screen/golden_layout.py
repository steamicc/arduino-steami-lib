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
    for name, s, ink, y in (("label", label, LABEL, v["top"] - P.u(4) - 7 * P.k),
                            ("unit", unit, TEXT, v["top"] + 8 * v["f"])):
        out[name] = None
        out[name + "_n"] = 0
        if has_text(s):
            centre = v["x"] + cell_width(t, v["n"], VALUE, v["f"]) // 2
            x, n, room = part_place(P, s, ink, y, at, centre)
            lit = draw_glyphs(s, n, ink, x, y, P.k)
            out[name] = box(lit) if lit else None
            out[name + "_n"], out[name + "_room"], out[name + "_x"] = n, room, x
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
