# STeaMi display layer, amendment: the V1c fonts

Status: Draft for the maintainer's review, 2026-11-26. The maintainer approved the look of the spike ("Oui"),
asked for the same fonts in MicroPython, and answered the two questions of the first draft (section 11,
"Maintainer decisions"): the primitives draw Spleen too, and values are bold. Every other choice below is
marked as the controller's or the designer's and can be corrected. No question is left open (section 13).
Repository: `steamicc/arduino-steami-lib`, base `main` after PR #277 (delivery 4b) is merged.
Amends: `2026-11-25-steami-display-design.md` (below: **the spec**). The delivery that builds this amendment
edits the spec as section 9 lists, so that the spec stays the single authority; this file then keeps the
reasons and the figures.

Sources: the spike of 2026-11-26 (mockups 01 and 04 drawn at 240 px with Spleen 8x16 text and 16x32 values,
compared with today's doubled 8x8 and with the `_ref.png`), Spleen 2.1.0 (<https://github.com/fcambus/spleen>,
BSD-2-Clause, BDF files 6x12, 8x16, 12x24, 16x32 measured for this document), the kernel of
`lib/steami_board/src/SteamiScreen.cpp` (`Run`, `inkRows`, `cellWidth`, `valueWidth`, `fitGlyphs`,
`marginNs`, `drawGlyphs`, `glyphsAt`, `gaugeLabel`), `SteamiDisplay.h/.cpp` (`text`, `textWidth`,
`textSmall`, `drawText`, `CHAR_W`/`CHAR_H`), `SteamiFont.h/.cpp`, the `Drawing` example and
`tests/native/helpers/SteamiGallery.h`, `tests/native/test_steami_display` and `test_steami_display_board`,
`scripts/tests/test_steami_display_rules.py`, `tests/native/test_steami_screen/golden_layout.py`, the 4b plan
(`docs/superpowers/plans/2026-11-26-steami-display-4b.md`), `scripts/build-arduino-package.py`,
`scripts/check-footprint.py`, the wiki page `docs/software/arduino/exemples/steami-board.md` of
`steamicc/wiki_steami`, and the builder and fidelity checks of this document (2026-11-26).

## 1. Intent and why

On the V1c (GC9A01, 240x240 colour, the only release that will be deployed, decision 6 of the spec) every
widget draws MicroPython's 8x8 framebuf font doubled (k = 2): 16x16 blocks, 15 characters per line, a look
far from the mockups' thin vector text. The width also breaks layouts: in program 04, "VL53L1X ToF" is 176 px
wide, rises to row 188 and paints over both ends of the gauge ring, and "Distance" (128 px) touches the
ring's top. MicroPython has the same problem as soon as it scales text on the GC9A01 (spec section 12).

The fix is a bitmap font drawn for this pixel size, used for **all text on the 240 px panel**: the widgets and
the `SteamiDisplay` primitives (`text()`, and so the `Drawing` example), so the V1c shows one font. The spike
shows that Spleen 8x16 for text and 16x32 for values brings titles, units and subtitles close to the mockups
and removes the 04 overlap on the V1c without touching the example or the gauge. Both languages get the same
tables from one generator, so a course looks the same in Arduino and MicroPython on the V1c (parity,
section 8).

## 2. Goals, non-goals, anti-goals

Goals
- **One font per panel.** On the 240 px panel all text is Spleen: every `SteamiScreen` widget (text 8x16,
  values up to 16x32, secondary labels 6x12) and the primitives (`text()` in 8x16 × scale). On 128 all text
  stays 8x8, with 5x7 for secondary labels.
- Program 04 draws with no text over the gauge ring **on 240**, unchanged. On 128 (V1a/V1b, rendered "at
  best" under decision 6) the overlap stays a known difference, because 128 must stay bit-identical
  *(designer's call; section 9 rewrites 4b's entry to "on 128")*.
- The 128 px rendering stays **bit-identical**, primitives included: same goldens, same PPM bytes (the
  primitives gallery too).
- French accents are drawn on the V1c, by widgets and primitives ("Température", "Été").
- The `Drawing` scene keeps its layout on 240; only its two text lines change, to 8x16 (section 6).
- One generator, pinned to Spleen 2.1.0, writes the C++ tables and the MicroPython module (5.6).

Non-goals
- Anti-aliasing, vector or proportional fonts (unchanged from the spec).
- Spleen on the 128 px OLED, for widgets or primitives.
- Value faces (the 12x24 and 16x32 value sets), bold or the narrow '.' and ':' in primitives (5.3).
- A value face bigger than 16x32 (Spleen has no 24x48; 32x64 is too big for a 240 px disc and costs 32 KB).
- Full ASCII in the 12x24 and 16x32 faces (5.5).
- Pixel-identical rendering with MicroPython: still a non-goal of the spec (its L58), except for the text
  primitive on 240; section 8 defines the parity this delivery aims at.

Anti-goals
- Never change a pixel on 128 px. Never fetch a font at build time: the tables are committed.
- No student-visible font choice. The only new public API is `textHeight()` (5.3); the other visible changes
  are values on 240: `maxChars()` and the height of `text()`.
- No RAM copy of a table (all `const`, in flash), no heap, no widget frame above 64 B, and no new local in a
  frame that is already at 64 B (5.7).
- Never hand-edit a generated table; never ship Spleen data without its BSD-2 notice in the source **and** in
  the documentation of the Arduino package (5.6).

## 3. Constraints

- Licence: Spleen is BSD-2-Clause (Copyright (c) 2018-2024, Frederic Cambus). The library is
  GPL-3.0-or-later and carries MIT text (the 8x8 font); BSD-2 is compatible with both. Clause 2 (binary
  redistribution) applies to firmware built from the library, so the notice must also be in the documentation
  that ships with the package, not only in a source file. The package built by `scripts/build-arduino-package.py`
  ships the repository `LICENSE` and `scripts/arduino-package/README.md` as its README; `lib/steami_board/README.md`
  is not shipped (5.6).
- Flash: 512 KB cap with the 64 KB margin of `scripts/check-footprint.py`; the layer was budgeted at 8 to
  12 KB. The library cannot know the release at compile time (no `STEAMI_BOARD_*` in a `.cpp`), so a V1c table
  is linked into every sketch that reaches it, V1a/V1b included. `--gc-sections` keeps only what a sketch
  reaches (5.5): `SteamiScreen` reaches the four faces; a sketch that calls `SteamiDisplay::text()`, such as
  `Drawing` (today's only footprint sketch), reaches the 8x16 full set, the extended decoder and the row
  drawer; a sketch that draws no text reaches none.
- Stack: at most 64 B per widget frame (spec 5.11), measured on the target. `value()`, `subtitle()` and
  `compass()` sit at exactly 64 B today; `gauge()` reached 72 to 80 B in 4b prototypes with one extra local (4b
  plan, Global Constraints and Task 3).
- Geometry: polar points stay Q14 (`polarX/polarY`); only the cell placed around a point changes.
- No heap, no `String`, both cores (STM32duino 2.12 and 3.1.0), native tests without a board.
- Scope (spec B4): the delivery changes only this repository; MicroPython and the wiki are separate PRs
  (section 8).
- MicroPython: `steami_screen` is frozen into the firmware (`tests/test_frozen_manifest.py` of
  micropython-steami-lib), so `bytes` constants of a frozen module stay in flash.

## 4. Approaches considered

| Option | For | Against | Taken |
|---|---|---|---|
| (1) Scale2x smoothing of the doubled 8x8 | No new table | Same 16 px advance: 04 still overlaps; rounded blocks, not thin text | no |
| (2) Bitmap font drawn for 240 px, V1c only, internal | Thin, legible, half the width; integer drawing unchanged; one table set for both languages | ~5 KB of flash; layout numbers on 240 change | **yes** (maintainer) |
| (3) Anti-aliased proportional fonts | Closest to the mockups | Kerning tables, alpha blending on RGB565 and grey, much more flash, breaks the cell model of the kernel | no |

Spleen was chosen for option 2 because it ships every size needed with one design (6x12 to 32x64), Latin-1
and the degree sign, a permissive licence, and cells exactly `h/2` wide from 8x16 up, which keeps the
kernel's cell arithmetic.

For the primitives, the first draft offered (a) keep the 8x8 at its physical size, or (b) draw Spleen too.
The maintainer chose (b) (section 11).

## 5. Design

### 5.1 Who uses which font

| Ink (kernel `Ink`) or primitive | 128 px (k = 1), unchanged | 240 px (k = 2) |
|---|---|---|
| TEXT at f = s·k: title, subtitle, unit, `text(s, at)`, menu items, compass and watch letters, graph current value | 8x8 × f | Spleen 8x16 × s (native at s = 1) |
| LABEL (f = k): value label, gauge min/max, graph axis labels | 5x7 × k | Spleen 6x12 × 1 |
| VALUE at f, text made only of value glyphs | 8x8 × f, narrow '.' ':' | f 2: 8x16; f 3: 12x24; f 4: 16x32; ladder above f 4 below; bold on every 12x24 and 16x32 face, replicated or not (F5) |
| VALUE at f, a text with any other glyph ("Uncomfortable", "Été", "T:23", "12:30 PM") | 8x8 × f, narrow '.' ':' | as TEXT: 8x16 × f/2 (odd f has no face), narrow '.' ':' (5.2), never bold |
| Primitive `text(s, x, y, c, scale)` | 8x8 × scale | Spleen 8x16 × scale, the pixels of TEXT at f = scale·k (5.3) |
| Primitive `textSmall(s, x, y, c, scale)` (protected) | 5x7 × scale | Spleen 6x12 × scale; at scale 1, the pixels of LABEL |

Value glyphs: `' '`, `'+'`, `'-'`, `'.'`, `':'`, `'0'..'9'` (15 glyphs; everything `SteamiValue::format()`
prints for a number, and "--"). The face of a VALUE text is chosen once from the **whole** text (all its
glyphs are value glyphs or not) and the f being tried, never from a prefix being measured, so `fitGlyphs`
measures every prefix in the face of the whole text.

A VALUE text with another glyph keeps the narrow '.' and ':' on 240 *(designer's call, answering the
builder)*: the narrow window and its `w/2` advance are a rule of the VALUE ink (B6), not of a face, so they
apply in every VALUE text on both panels, whichever face draws it ("12:30 PM" at f 2 on 240: 8 px per glyph,
4 px for ':'). Reasons: the 128 rule is unchanged, `cellWidth`/`valueWidth` keep one rule per ink, and 128
and 240 differ only by the face. A TEXT-ink 8x16 glyph (title, `text()`) keeps its full 8-column advance; the
two inks share the 8x16 table and differ only in that window.

Factors without a native Spleen size *(designer's call)*: a face is a Spleen size replicated n times (each
font pixel drawn n×n). On 240 the VALUE ladder for f > 4 is 16x32 × f/4 when 4 divides f, else 12x24 × f/3
when 3 divides f, else 8x16 × f/2 when f is even, else no face (f 6: 12x24 × 2; f 8: 16x32 × 2; f 10: 8x16 × 5).
A factor with no face is skipped by the value fit. Reasons: scale-n text keeps the meaning "n times bigger"
(the student's `text(s, at, c, 2)` and `text(s, x, y, c, 2)` are 8x16 doubled), every TEXT and VALUE face
keeps a cell height of exactly 8f (5.2), and no table bigger than 16x32 is needed. LABEL faces have their own
cell height: 7k on 128, 12 on 240.

f = 1 never occurs on 240 *(designer's call)*: `value()` now lowers f from `max(scale, 1)·k` **down to k**,
not to 1 (on 128, k = 1: unchanged), **at every anchor, corners included** (section 6 gives the new NE
figures). Reasons: 8x16 at f 2 already has the 8 px advance of the old f 1, so the W/E and CENTER capacity at
the last step is unchanged (15 glyphs in 120 px), and no 5x8 table is needed. LABEL is fixed at 6x12, so no
widget reaches a 1-px-per-unit face.

One font per panel (maintainer, 2026-11-26): the spec's "8x8 is the only font the student sees" (spec
L174-175) becomes "one font per panel". On 240, `screen.text(s, x, y)`, `screen.text(s, at)`, the `Drawing`
example and every widget draw Spleen; a sketch that mixes primitives and widgets shows one font. On 128
nothing changes. Section 9 rewrites L174-175.

### 5.2 Metrics

- **Cell height stays 8f on both panels for TEXT and VALUE faces** (8x16 at f 2, 12x24 at 3, 16x32 at 4,
  replicated faces likewise; LABEL faces have their own: 7k on 128, 12 on 240):
  every vertical formula of the spec in `8f`, `8k` or `ch` (floor of `margin_ns`, `margin_ew` = device.py's
  `ch + 4`, S row `− 8f`, value block `8f + 8f/3 + 8k`, unit at `value top + 8f`, `glyphsAt` half height)
  keeps its numbers. The builder names this "cell height" in the code instead of `CHAR_W`/`CHAR_H`.
- **Advance**: the face's cell width: 8f on 128, **4f on 240** (Spleen cells are h/2 wide; 6x12 advances 6).
  Every `8·f` used as a *width* in spec 5.8/5.9 becomes the face advance: `cellWidth`, `valueWidth`,
  `textWidth`-as-measured-by-widgets, the 13-cell cap, the menu's 12 cells, `maxChars()`. For the primitives
  this gives back the old numbers: `text()` at scale s advances 8s on both panels (5.3).
- **Narrow '.' and ':' (B6), generalised** *(designer's call)*: in VALUE ink, on every face (value sets and
  the 8x16 of the TEXT ladder alike, 5.1), drawn from the column window
  `[w/4, 3w/4)` of their glyph with an advance of `w/2` (w = font cell width; 8x8 gives columns 2..5 and 4f,
  as today). Measured: the ink of '.' and ':' is columns 3..4 (8x16), 5..6 (12x24), 7..8 (16x32), inside the
  window with the bold column. B6 still applies on 240 because the monospaced '.' leaves a visible gap
  ("23 . 5" in the spike) where the mockups have a narrow point.
- **Ink rows** come from the bitmaps, as today; the kernel's row mask widens from 8 to 32 bits (font rows),
  and physical row = `y + row·n`. Spleen rows, cell top = row 0:

| Face | ascent / descent | caps | digits | x-height | descenders g p y | degree | accented caps |
|---|---|---|---|---|---|---|---|
| 6x12 | 9 / 3 | 1..8 (Q 9) | 1..8 | 3..8 | to 11 | 1..4 | from 0 |
| 8x16 | 12 / 4 | 2..11 (Q 13) | 2..11 | 5..11 | to 14 | 1..4 | from 0 |
| 12x24 | 19 / 5 | 4..18 (Q 21) | 4..18 | 8..18 | to 23 | 3..8 | from 0 |
| 16x32 | 26 / 6 | 6..25 (Q 29) | 6..25 | 12..25 | to 31 | 5..13 | from 0 |

  Every Spleen 2.1.0 glyph used has the full-cell bounding box and DWIDTH = cell width (checked), so a glyph
  is its cell; descenders stay inside the cell, and rule 6 of the spec (chord at the inked rows) applies as is.
- **Bold values** *(maintainer, 2026-11-26: 1-px double strike)*: a VALUE glyph from a 12x24 or 16x32 face,
  replicated or not, is drawn twice, at x and at x + n (one font column). The value glyphs ink columns 1..10
  of 12 and 2..13 of 16, so the strike stays inside the cell: **advance, widths, fits and ink rows are
  unchanged**, and neighbours keep 1 (12x24) or 3 (16x32) free columns. 8x16 values (f 2, and 8x16 × 5 or × 7
  at f 10 or 14) are not bold *(designer's call)*: its digits ink columns 0..6, and a strike would close the
  only gap between them. The 128 panel and the primitives are never bold.

### 5.3 Primitives and layout rules restated (what changes on 240; 128 identical)

**Primitives** *(designer's calls F13 and F14, applying the maintainer's "Spleen aussi")*: on each panel a
primitive draws that panel's base face replicated `scale` times (scale 0 counts as 1, as today).

| Call | 128 px, unchanged | 240 px |
|---|---|---|
| `text(s, x, y, c, scale)` | 8x8 × scale, cell 8·scale × 8·scale | Spleen 8x16 × scale, cell 8·scale × 16·scale |
| `textWidth(s, scale)` | 8·scale per glyph | 8·scale per glyph (**unchanged**) |
| `textHeight(scale)` (new) | 8·scale | 16·scale |
| `textSmall(s, x, y, c, scale)` (protected) | 5x7 × scale, advance 6·scale | Spleen 6x12 × scale, advance 6·scale |
| `textSmallWidth(s, scale)` (protected) | 6·scale per glyph | 6·scale per glyph (unchanged) |
| `CHAR_W`, `CHAR_H` | 8, 8 | 8, 8 (constants unchanged) |

- `(x, y)` stays the top-left corner of the first glyph's cell. A Spleen glyph is its cell (5.2), so
  descenders and accented capitals stay inside `[y, y + textHeight(scale))`.
- Same face as the widgets: on 240, `text(s, x, y, c, scale)` draws the pixels of the kernel's TEXT ink at
  f = scale·k, and `textSmall(…, 1)` those of LABEL. So `glyphsAt` keeps calling `SteamiDisplay::text`, at
  scale 1 instead of f, with x = point − `textWidth(s)/2` and y = point − `textHeight()/2`: the same cells as
  the kernel computes (F16). Reasons: one font per panel by construction; "scale n is n times bigger" on both
  layers; `textWidth()`, `CHAR_W` and `maxChars()` stay consistent (`width() / CHAR_W` is 16 and 30); no new
  table (the 16x32 face carries digits only).
- Full sets only: primitives never use the 12x24 and 16x32 value sets, bold or the narrow '.' and ':'.
  Reasons: the value sets hold 15 glyphs; picking a face from the string's content would make
  `text("23", x, y, c, 2)` and `text("A23", x, y, c, 2)` differ in height and break "n times bigger"; bold and
  the narrow window are rules of `value()` (B6, F5), which the student does not call through `text()`.
- Accents: drawn on 240 by `text()` and `textSmall()`, folded on 128 (5.4). Unknown code points and invalid
  UTF-8 still draw `?`. `textWidth()` counts glyphs, so no width changes.
- **`int16_t textHeight(uint8_t scale = 1);`** (public, new): the cell height of `text()` on this panel, the
  pitch a sketch needs to stack lines on both panels. Valid before `begin()`, like `width()`, and not `const`
  for the same reason; it follows the same face switch as the kernel (section 12), so a panel without the
  240 faces, or no backend, gives 8·scale. Reason: the line height is the only text metric that now depends
  on the panel, and `CHAR_H` is a `static constexpr` that cannot.
- `CHAR_W` and `CHAR_H` stay, both 8 *(designer's call)*. `CHAR_W` is still the advance at scale 1 on both
  panels. `CHAR_H` keeps meaning the 8x8 height: the 128 px height and the value of device.py's `CHAR_H`, in
  which MicroPython's layout constants are written. Changing it would make it panel-dependent (impossible for
  a constant) and removing it would break sketches; the README says "glyph height on the 128 px screen; use
  `textHeight()` for the screen in use". The kernel stops using either for 240 widths and heights (5.2).
- Visible change on the V1c: `text()` is twice as tall at scale 1 (16 px) and as wide as in v0.39. A V1c
  sketch that stacks lines with a hard-coded 8 to 15 px pitch now overlaps; the changelog says so
  (section 12).

**Layout rules of the widgets**
- Title cap (B8): the safe margin of a 13-cell text **in the panel's TEXT face at scale 1**: 104 px on both
  panels, so 29 on 128 (unchanged) and 14 on 240, below the floor 37 *(designer's call)*. On 240 every title
  sits at row 37 and is never moved down (rule 3's "text that is 7 % wider on V1c moves down" no longer
  applies to titles on 240); it is truncated at the chord of its inked rows, which holds 22 cells for ordinary
  text (rows 39..51: 177 px). The title band on 240 is row 37 only.
- Subtitle (B7), value fit, corners, W/E regions, `text(s, at)` and truncation: the rules are unchanged; they
  read widths from `cellWidth()` and rows from `inkRows()`, which now follow the face.
- Value label: its cell bottom stays `u(4)` above the value's cell top, using the label face's cell height
  (7k on 128 as today, 12 on 240) instead of the literal `7·k`.
- `maxChars()` = `width() / advance(TEXT, k)`: 16 on 128, **30** on 240 *(designer's call: it answers "how
  many characters fit on a line", which doubled)*. device.py on `main` returns `width // CHAR_W` (30), but
  the branch `feat/gc9a01-screen` (micropython-steami-lib#454) changed it to `width // (CHAR_W * k)` (15 on
  240); the MicroPython follow-up sets `max_chars` to `width // advance` of the TEXT face at k (16 / 30), and
  8.1 lists that change.
- `menu()` visible rows (B3): "the marker plus 10 cells" is 12 TEXT cells, 96 px on both panels. On 240 the
  chord of row 6 (the spec's rows `u(35 + 14·i)`: cells 196..211) is 157 px, so **6 rows are visible on 240**
  (5 on 128), as mockup 06 *(designer's call: V1c first; the MP mark "6" disappears on 240)*. 4c builds it.
  Six rows hold only with B3's highlight clipped per scanline to the chord (left + k .. right − k): a fixed
  bar `u(15)..W − u(15)`, as MicroPython draws it today, leaves the disc on row 6 (rows 193..218, about 23 px
  past the chord on each side). The MicroPython follow-up clips its bar the same way (one `fill_rect` per
  scanline, 26 on 240) or keeps 5 rows on 240 as a listed difference (8.1).
- Graph axis labels (B1, 4c; 6x12 on 240) *(designer's call, answering the MicroPython review: ink edges, as
  device.py reads B1)*: the max label's first inked row is `y + u(2)`, the min label's last inked row is
  `y + h − k − u(2)`, both read from `inkRows()` (6x12 digits: rows 1..8). On 240 with the default box: max
  cell top 73 (ink 74..81), min cell top 155 (ink 156..163). On 128 (5x7, ink = cell) the rows are unchanged.
  device.py's `8*k` term in these formulas becomes the LABEL face's metrics.
- Compass and watch letters (`glyphsAt`): `SteamiDisplay::text(s, x, y, c, 1)`, the cell centred on the same
  Q14 point, as device.py (F16). The cap ink of 8x16 (rows 2..11, centre 6.5) sits 1 px above the cell centre
  (row 7.5), 1.5 px above the Q14 point (row 8); accepted *(designer's call: parity with MicroPython's
  `ly − CHAR_H // 2`)*.
- Value fit floor k at every anchor (5.1). B9's "a scale of 0 counts as 1" stays.

### 5.4 Accents

Spleen 6x12, 8x16, 12x24 and 16x32 have every letter of the spec's folding table (`à â ä ç é è ê ë î ï ô ö ù
û ü ÿ` and capitals); only U+0178 (Ÿ) is missing from 6x12. Decision *(designer's call, extended to the
primitives)*: **drawn on 240** by TEXT and LABEL faces and by `text()` and `textSmall()`, **folded on 128**
by everything, as today. Reasons: French courses, one rule per panel, no layout rule (accented capitals ink
from row 0 of the cell, which the inked-rows chord already handles), 32 glyphs × 28 B = 896 B. The generator
fills the 6x12 Ÿ slot with its Y glyph. Value faces need no accents: a text with an accent is not made of
value glyphs and uses the TEXT ladder.

Glyph codes: `steamiNextGlyph()` keeps its contract (32..127, folded) for the 128 path. A new decoder returns
the extended code space 32..159: 32..126 ASCII, 127 degree, 128..158 the 31 Latin-1 letters in the order of
`kFolds` (À Â Ä Ç È É Ê Ë Î Ï Ô Ö Ù Û Ü à â ä ç è é ê ë î ï ô ö ù û ü ÿ), 159 Ÿ; a fold function maps
128..159 to the base letter for 8x8 and 5x7. The 240 path of the primitives and the kernel both use it;
`steamiGlyphCount()` (so `textWidth()`) counts the same glyphs either way.

### 5.5 Storage, files, flash

Format *(designer's call)*: rows top to bottom, each row `ceil(w/8)` bytes, most significant bit = leftmost
column (MicroPython `framebuf.MONO_HLSB`). Full sets are 128 glyphs indexed by code − 32; value sets are 15
glyphs in the order of the 15-character key `" +-.:0123456789"`. A glyph of face (w, h) is the
`h·ceil(w/8)` bytes at `index·h·ceil(w/8)` of its face's blob (the stride `ceil(w/8)` is MicroPython's
MONO_HLSB stride). MicroPython blits one glyph of a native-size face (n = 1) straight from the frozen table,
bold included (a second blit at x + 1); only replicated faces (n ≥ 2) need a row drawer. Section 8.1 gives
the method (read-only blit source, palette and key on the GS4 frame).

Bold at n ≥ 2 *(designer's call, from the MicroPython review, for both languages)*: the union of the strikes
at x and x + n is each run of set bits widened by one font column, physical span `[a·n, (b + 2)·n − 1]`
instead of `[a·n, (b + 1)·n − 1]`, so a row drawer draws bold with no extra call. Arduino may draw the strike
either way; the pixels are the same.

Figures for Spleen 2.1.0 (row-aligned; bit-packed in brackets):

| Face | 128 glyphs (ASCII + degree + accents) | 15 value glyphs |
|---|---|---|
| 6x12 | 1 536 B (1 152) | 180 B |
| 8x16 | 2 048 B | 240 B |
| 12x24 | 6 144 B (4 608) | 720 B |
| 16x32 | 8 192 B | 960 B |
| 32x64 (not carried) | 32 768 B | 3 840 B |

Decision *(designer's call)*: full sets for 6x12 and 8x16, value sets for 12x24 and 16x32: **5 264 B** of
tables, against 17 920 B with full sets everywhere. The 12 656 B saved matter less on Arduino (512 KB) than
in the MicroPython firmware, and the only big text in the mockups is a number. The primitives add no table:
`text()` at any scale is 8x16 × scale and `textSmall()` is 6x12 × scale, so full sets are still needed for
8x16 and 6x12 only. Row alignment costs 564 B over bit-packing and keeps the drawing loop and the
MicroPython blit simple. With the code (decoder, face lookup, row drawer), the layer grows by about 6 KB; the
spec's 8 to 12 KB estimate becomes 14 to 18 KB.

Linking *(designer's call)*: each face blob is its own `const` object, and no table of faces that a primitive
reaches points to the other blobs, so `--gc-sections` (with `-fdata-sections`) keeps per sketch only what it
draws. **`text()` and `textSmall()` name their 240 face themselves** *(designer's call, answering the
fidelity check)*: today both go through `drawText(…, bool small)` (SteamiDisplay.cpp:497-517), a run-time flag
that would link the 6x12 blob into every sketch that calls `text()`. On the 240 path the shared code takes the
face as an argument that each public function passes as a constant (or each keeps its own path); no run-time
flag selects between 8x16 and 6x12. The 128 path may keep its `small` flag (8x8 and 5x7 are linked today and
stay so). `Drawing` (primitives only) then grows by the 8x16 full set (2 048 B), the extended decoder and the row
drawer, about 3 KB; the PR checks with `arm-none-eabi-nm` (or the map file) that `Drawing` holds no 6x12,
12x24 or 16x32 blob. `SteamiScreen` sketches carry all 5 264 B, as before.

Files: the tables and their metrics go to a generated `lib/steami_board/src/SteamiFontSpleen.cpp` (unique
name); the face type, the extended decoder and the lookups are declared by hand in `SteamiFont.h`.
`SteamiFont.cpp` keeps 8x8 and 5x7.

### 5.6 Generator, MicroPython module, licence

- `scripts/gen-spleen-fonts.py` (Python 3, standard library only). Input: the release tarball
  `spleen-2.1.0.tar.gz` of fcambus/spleen, fetched once and checked against sha256
  `8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f`, then per BDF file:
  6x12 `f40e935bdd194976d30ef296920d5ccca29eb84bb7b3459a88fe7932fa8ba872`,
  8x16 `b2b05484ba4380c9c6854cc57637235a9fb039c65b5ba38a65289540f8fab7dc`,
  12x24 `eb690d1dd190e21ae31f800e63fe1188409e3d94a11da222196de0777ed8c9e8`,
  16x32 `3b6ae73ef1dd3c78bad873697c18b15df82d8e5c0258114fda401556e01d0e67`;
  cached in a git-ignored directory (one `.gitignore` line). Fetched, not vendored *(designer's call)*: the
  outputs are committed, so builds and native tests never need the BDF; vendoring would add 700 KB of
  third-party files.
- Both outputs are written from one in-memory model of the faces, so they hold the same bytes.
- Output 1: `lib/steami_board/src/SteamiFontSpleen.cpp`.
- Output 2, with `--micropython PATH`: the module `spleen.py`. Its contract is fixed and unit-tested by this
  delivery *(designer's call, answering the builder)*, so the MicroPython follow-up consumes it as is:
  - `VERSION = "2.1.0"`; `VALUE_KEY = " +-.:0123456789"`; `EXTRA`, the 33 characters of codes 127..159
    (`°`, the 31 letters of 5.4 in order, `Ÿ`), so MicroPython maps a character to a code without the C++
    decoder; `FOLD`, a 33-character `str` parallel to `EXTRA` giving the 8x8 replacement of codes 127..159
    (the base letter of each accent, `Y` for `Ÿ`; `°` maps to itself: MicroPython's petme128 has no degree
    glyph, so MicroPython draws `°` at 128 with the same 8x8 degree glyph as Arduino, see 8.1), so that
    MicroPython can fold on 128 without copying `kFolds` *(designer's call, from the MicroPython review)*.
    The generator tests check the 31 letters of `FOLD` (positions 1..31) against the base letters of
    `kFolds` in `SteamiFont.cpp`, and the two entries outside `kFolds` by fixed assertions: `FOLD[0] == "°"` and
    `FOLD[32] == "Y"` (the replacement the C++ fold function gives code 159).
  - Code of a character `ch` (MicroPython iterates code points): `o = ord(ch)`; `32 ≤ o ≤ 126` → `o`;
    otherwise `i = EXTRA.find(ch)`, `i ≥ 0` → `127 + i`; otherwise `ord('?')`. This covers control
    characters, U+007F and every character outside the table, as Arduino draws `?`. In a value set the index
    is `VALUE_KEY.find(ch)`; a character not in the key sends the text to the TEXT ladder (5.1). Widths count
    one cell per code point.
  - Four faces `F6X12`, `F8X16`, `F12X24`, `F16X32`, each a tuple `(w, h, ascent, key, data)`: `key` is
    `None` for a full set (index = code − 32) and `VALUE_KEY` for a value set (index = position in the key);
    `data` is one `bytes` per face in the format of 5.5. One blob per face, no per-glyph offsets (every glyph of a
    face has the same size).
  - A header comment with the Spleen version, the sha256 values, the generator's path and repository
    commit, and the full BSD-2 notice; `# SPDX-License-Identifier: BSD-2-Clause`.
- Ownership *(designer's call)*: the generator lives in this repository and is the only writer of both
  outputs. This delivery does not commit `spleen.py` anywhere (spec B4); the MicroPython PR commits the file it
  produced with `--micropython`, and its header names the generator commit. A Spleen bump is a PR here first,
  then a MicroPython PR that regenerates. The MicroPython CI cannot regenerate (accepted: the sha256 values in
  the header make a drift visible).
- The generator fails when an assumption breaks: a glyph without the full-cell box or with DWIDTH ≠ w; a value
  glyph inking column 0 or w − 1 in 12x24/16x32 (bold); '.' or ':' ink plus its bold column outside
  `[w/4, 3w/4)`; a missing glyph other than Ÿ in 6x12.
- Tests: `scripts/tests/test_gen_spleen_fonts.py`, on a tiny hand-written BDF: packing, MSB order, value key,
  fold of a missing glyph, each failure above. It also writes `spleen.py` to a temporary directory, imports it
  under CPython and checks its names, tuple shapes, `EXTRA` and that every glyph equals the matching glyph of
  the C++ output. A CI step regenerates `SteamiFontSpleen.cpp` and runs `git diff --exit-code`.
- Licence *(designer's call, answering the builder: the package README carries it)*:
  - the generated `.cpp` starts with `// SPDX-License-Identifier: BSD-2-Clause` (accepted by
    `scripts/check-spdx.sh`, which checks the prefix) and the notice;
  - `scripts/arduino-package/README.md`, which `build-arduino-package.py` ships as the package README, turns
    its "License" section into "GPL-3.0-or-later, with third-party fonts:" followed by the MIT notice of the 8x8
    font (absent from the package today) and the full BSD-2 notice of Spleen. No separate NOTICE file and no
    change to `build-arduino-package.py`: the README is already copied. It is rendered with
    `readme.format(version=version)` (line 161), so a literal `{` or `}` in a notice would have to be doubled;
    the current MIT and BSD-2 texts contain none;
  - `scripts/tests/test_build_arduino_package.py` gains a test that reads the repository's real
    `scripts/arduino-package/README.md`, renders it with `format(version=…)` as the build does, and asserts both
    notices are present in the result (the existing fixture README stays for the copy tests);
  - `lib/steami_board/README.md` gains the same "Third-party fonts" section for readers of the repository;
  - MicroPython: the same notice in the module (above) and in its README (the follow-up issue).

### 5.7 Stack *(designer's call, answering the fidelity check)*

`value()`, `subtitle()` and `compass()` have no headroom (64 B), and `gauge()` grows past 64 B with one extra
live local. This delivery changes code they call or may inline: the f loop of `value()` (stop at k, skip f
with no face), the label height in `drawValueParts`, `glyphsAt` (still one `SteamiDisplay::text` call, now at
scale 1 with `textWidth()` and `textHeight()`), 32-bit row masks and the face lookup. Rules:
- No new local in a public widget frame. The face and ladder logic sits in helpers that hold no buffer: for
  example `placeValue()` returns false for an f with no face, so `value()`'s loop stays `t.f--`.
- `Run` stores a one-byte face id in its padding byte (`s`, `glyphs`, `y`, `ink`, `f`, `shrink` use 15 of
  its 16 B on ARM), so the layout and size of `Run` and `Region` stay as they are.
- What stays and what may change *(designer's call, answering the builder; it replaces the 4b plan's "do not
  change any 4a kernel signature" for this delivery)*: the public widget signatures, `Run` and `Region` stay.
  The private kernel helpers may change their signatures, because the design needs it: the row mask widens to
  32 bits (5.2); `inkRows` needs the face (a VALUE's rows depend on f); `rowsChord` needs the replication n,
  since the physical row is `y + row·n` and n ≠ f for LABEL (f = k = 2, n = 1) and 16x32 at f 4 (n = 1);
  `cellWidth` needs the face of the whole text while it measures a prefix (5.1). The face is resolved by the
  code that sets a `Run`'s text and f (for `value()`, `placeValue()` on each f tried) and stored in the `Run`;
  a helper then reads it from `const Run&` (plus a prefix count where it measures one) or takes the one-byte
  face id where it has no `Run`. In a frame at 64 B (`value`, `subtitle`, `compass`) and in `gauge`'s, a
  changed call must not add a stack argument: under the ARM AAPCS `this` and three more words travel in
  registers, so prefer `const Run&` over extra scalars. Every changed helper's frame is measured; those that
  hold a buffer, `Run`, `Region` or triangle vertices stay bound to 64 B (4b rule), the others are recorded.
- `glyphsAt` builds no `Run` (it calls the primitive), so `compass()` and `watch()` gain no local.
- The primitives' text frames (`text()`, `textSmall()`, `drawText()` and the row drawer, shared or not with
  the kernel) are not widget frames: spec 5.11 does not bind them, and callee frames are not summed (4b plan).
  They hold no buffer and no `Run`; the PR records their frames next to 4b's.
- Face helpers are marked `__attribute__((noinline))` when the compiler inlines them into a bound frame.
- Fallback when a bound frame still goes over 64 B: the 4b plan's rule (the task shrinks its locals; if that
  is impossible without breaking another spec rule, the builder stops and reports a spec deviation; a helper
  is never exempted).

## 6. Effect on the mockups and on Drawing at 240

Program 04, computed with the rules above and the Spleen bitmaps (gauge ring 101..113, ends at 225° and 135°,
the code's arc algorithm):
- Title "Distance": row 37 (unchanged), 64 px, x 88..151, ink rows 39..48; nearest ring pixel 13.6 px away
  (today 128 px wide, touching the ring top).
- Value "342": 16x32 bold, 48 px at x 96, ink columns 98..142, rows 97..116; unit "mm" ink rows 128..134.
- Gauge labels in 6x12: "0" at (52, 169), "500" at (176, 169), ink rows 170..177, 12.7 and 9.4 px from the
  ring (today 5x7 × 2, "500" 36 px wide).
- Subtitle "VL53L1X ToF": 88 px, fits at device.py's row **202** with no rise (today 176 px at row 188), x
  76..163, ink rows 204..213, chord [45, 195], 30.5 px from the ring, inside the gap between its ends.
- On 128 nothing changes: the overlap stays (section 2).

Gauge labels outside the examples (4b's 240 known differences, re-measured):
- A 3-digit min label such as "100", which touched the ring in 4b, is 8.1 px clear.
- The two slid labels of (−1e8, 1e8), which overlapped at x 113..139 in 4b, now have cells 25..84 and
  158..211: no overlap. Both entries are removed for 240 (section 9); the 128 entries stay.

Other mockups (240 only; 128 unchanged):
- 01: "Temperature" 88 px at row 37 (was 176 px at 41; mockup about 103 px); "23.5" 56 px, ink 20 px tall
  (mockup about 32: still smaller, listed in the spec's known differences); "°C" rows 124..134; "BME280" at
  202.
- 02: "72" 16x32 bold at top 69; bar unchanged; subtitle block 177/197 (unchanged).
- 03: both side values at f 4 (was ×3), "23.5" at x 32, "45.3" at x 152 (`88.8` = 56 px in a 109 px region);
  labels "TEMP" and "HUM" 6x12, cell top 72; subtitle blocks at 177/197 for "Comfortable" and
  "Uncomfortable" (were 173/193 and 151/171).
- 05 (4c): axis labels "1050"/"950" 24/18 px (were 48/36), about an eighth of the plot width; 177/197.
- 06 (4c): 6 visible rows; "Temperature" 88 px per item.
- 07, 09: letters N, E, S, W and numerals 12, 3, 6, 9 in 8x16 (8 px per character instead of 16), same points.
- 08: "Mood" 32 px at 37; subtitles at 202; face unchanged.
- The spec's 240 NE example `value(8, "C", NE, "TEMP", STEAMI_WHITE, 1)`: under F2 the value stays at f 2
  (it dropped to f 1 before): row 37 (`margin_ns`), region [36, 204], x clamped from 210 to 197. The label
  cell top is 37 − 7 − 12 = 18, ink rows 19..26, room [58, 182] (125 px): "TEMP" (24 px in 6x12) is whole at
  x 159 (today "TE" at x 110 in a 27 px region, value at x 180, row 22). Corner values on the V1c are thus
  larger and lower than in the spec's current example *(designer's call, answering the builder: intended)*.

**`Drawing` and the primitives gallery** *(designer's call F15)*. Today `s = 2` on 240 sizes both the gauge
strokes (inset `4·s`, width `6·s`) and the text (8x8 × 2: 16x16 glyphs, "STeaMi" 96 px). Kept as is, `s = 2`
would now draw 8x16 × 2 (32 px cells): the two cells would overlap ("STeaMi" rows 60..91, the second line from
row 84; their inks, 64..83 and 86..107, would not touch), and the second line (cells 84..115, ink to row 107,
x 72..167) would run into the rectangle outline (row 105, x 60..99), which is drawn after the text and
paints over it. Rule: `s` keeps sizing the strokes; the text is drawn at scale 1, the
font's native size on both panels, and the second line sits `textHeight() * 3 / 2` below the first instead of
`12 * s`. The two text lines of the shared body (`Drawing.ino` and `SteamiGallery.h`, kept identical by
`test_steami_display_rules.py`) become:

```cpp
    d.text("STeaMi", cx - d.textWidth("STeaMi") / 2, cy - r / 2, STEAMI_WHITE);
    ...
    d.text(temp, cx - d.textWidth(temp) / 2, cy - r / 2 + d.textHeight() * 3 / 2, STEAMI_YELLOW);
```

and the comment on `s` says it scales the gauge strokes, while `text()` follows the panel's font by itself.
- On 128: scale 1 = `s`, and `textHeight() * 3 / 2` = 12 = `12 * s`: the same calls, bit-identical.
- On 240: "STeaMi" 48 px at x 96 (was 96 px at x 72), cell rows 60..75 as before, ink rows 62..71; "20°C é"
  48 px at x 96, cell rows 84..99 as before, ink rows 85..95, the `é` drawn with its accent (was `e`). Every
  pixel outside the two old text cells (x 72..167, rows 60..75 and 84..99) is unchanged.
- Reasons: the layout stays as published (same rows, same shapes); the text now matches the widgets' text
  (title "Temperature" is 8x16 too) and the accent is drawn; the example shows the portable way to stack lines
  (`textHeight()`) instead of a panel test. Rejected: a text scale computed from the font height (it is 1 on
  both panels, so it adds code for nothing).

## 7. Tests

- 128: every assertion of `test_steami_screen`, `test_steami_screen_mockups`, `test_steami_display` and
  `test_steami_display_board` unchanged; the delivery records the sha256 of every 128 px PPM on `main` before
  the change, the primitives gallery `steami-display-gallery-128-gray.ppm` included, and a test or CI step
  checks them after (bit-identical goal).
- 240 widget goldens: `golden_layout.py` parses `SteamiFontSpleen.cpp` (as it parses `SteamiFont.cpp` today)
  and applies 5.1 to 5.4, its model of `SteamiDisplay::text` included (8x16 × scale on 240); every 240 number
  of the spec's section 7 is regenerated (titles at 37, subtitles at 202 and 177/197, W/E f 4, label
  capacities, `yOffset` truncation thresholds, `text("ABCDEFGH", N, c, 2)` now whole on 240, compass and
  watch letter cells, NE value at f 2 and label "TEMP" whole at x 159, the 4b gauge label boxes of (0, 100),
  (−1e8, 1e8) and the other printed pairs).
- Existing 240 screen tests whose reference call changes, not only their numbers (line numbers of
  `tests/native/test_steami_screen/test_main.cpp` at fc9a7e9):
  - L1482-1500, the NE call: the value is checked at f 2 (row 37, x 197) and the GRAY label against
    `textSmall("TEMP", 159, 18, STEAMI_GRAY, 1)` (cell top 18, ink box rows 19..26) instead of
    `textSmall("TE", 110, 1, STEAMI_GRAY, 2)`: LABEL on 240 is 6x12 × 1, and `textSmall(…, 2)` would be
    6x12 × 2.
  - L742-749, the accented title: `title(kAccented)` equals `text("Temperatureeee", N, GRAY)` on 128 only; on
    240 the two renders differ (accents drawn), and the 240 ink box (L738-741) is regenerated.
- 240 primitive expectations (3a, `test_steami_display`, the `forEachPanel` cases): the 240 half of the glyph
  size case (scales 1 to 4), the degree box, the text clipping rows and the `textSmall` box are recomputed from
  the Spleen tables; on 240 `"é"` no longer equals `"e"` and `"Ÿ"` draws Spleen's Ÿ (the fold cases stay on
  128); invalid UTF-8 still draws `?` on both; every `textWidth` and `textSmallWidth` value is unchanged on
  both panels; the `CHAR_W == CHAR_H == 8` static assertion stays; new: `textHeight()` is 8·s on 128 and 16·s
  on 240 for s 0 to 4 (0 counts as 1), and 8 with no backend.
- Primitives gallery (3a/3b): the 240 colour dump changes; the PR shows it before and after and checks once
  that only pixels inside the two old text cells (section 6) differ. `test_gc9a01_frame_matches_the_memory_gallery`
  (`test_steami_display_board`) keeps its code: the SPI frame and the memory backend both draw the new gallery.
  `test_steami_display_rules.py` keeps its code: `Drawing.ino` and `SteamiGallery.h` change together.
- New native cases: one font per panel: on 128 and 240, `SteamiScreen::text(s, at, c, n)` and
  `SteamiDisplay::text(s, x, y, c, n)` at the resolved (x, y) draw the same pixels for an untruncated string
  with accents, n 1 to 3; a compass letter equals `text("N", x, y, c, 1)` at its cell; a LABEL equals
  `textSmall(…, 1)`. 'é' drawn on 240 by a widget and by `text()`, folded on 128 by both; Ÿ in a label on 240
  draws Y; a value with a letter uses the TEXT ladder and keeps the narrow '.' and ':' ("12:30 PM" at f 2 on
240 measures 60 px, 7 × 8 + 4, and its ':' inks inside its 4 px window); a prefix kept by truncation is
measured in the face of the whole text; on 240 the value fit stops at f 2 (CENTER, W/E and
  corners) and skips f 5 and 7; bold adds ink only inside the cells (no pixel past `x + valueWidth`), also on a
  replicated 12x24 × 2 face; `text("88", x, y, c, 2)` on 240 is not bold and uses 8x16 × 2; '.' ink centred in
  its narrow window on every face; no pixel outside `radius() + 1` for every widget with long strings, as
  today.
- Program 04 at 240, geometrically *(designer's call: the DARK subtitle on the DARK track cannot be told apart
  on the final picture)*: render the title alone and the subtitle alone, and take the GRAY label pixels of a
  gauge-alone render; no such pixel satisfies the ring's own distance and angle test (radius 101..113, sweep
  225° to 135° clockwise).
- Stack: every widget frame and bound helper re-measured on env:steami (`-fstack-usage`), at most 64 B
  (5.7); the primitives' text frames recorded; the PR lists them next to 4b's.
- `scripts/mockup-review.py`: the 240 SSIM of every mockup is expected to rise; the PR gives before/after.
- Footprint gate *(designer's call, answering the fidelity check)*: `SKETCHES` in `scripts/check-footprint.py`
  gains `steami_board/ScreenGauge` (title, value, unit, subtitle and gauge labels: every face is linked). The
  PR reports the flash of ScreenGauge and of `Drawing` before (on `main`) and after; expected about 6 KB and
  3 KB more (5.5), and `Drawing` links the 8x16 blob only (no 6x12, 12x24 or 16x32 symbol in its ELF). A
  Drawing delta near 4.5 KB means `text()` reaches the 6x12 blob, against the linking rule of 5.5. WeatherStation still joins in 4c.
- Hardware, V1c under the webcam: the examples of section 10 and `Drawing`, whose photo must match the new
  240 px gallery dump; `Drawing`'s `show()` time recorded (unchanged, text does not touch `show()`).
- Generator and package tests of 5.6.

## 8. Follow-ups in other repositories (separate PRs, not part of "done", spec B4)

### 8.1 MicroPython (issue in `steamicc/micropython-steami-lib`)

Filed by the controller when this delivery's PR opens: commit the `spleen.py` produced by this
repository's generator in `steami_screen/`, frozen with the package (flash option (a), below) (`--micropython`, contract of 5.6), and on the 240 px backend draw the faces of 5.1
with the rules of 5.2 to 5.4, for the widgets and for the pixel text `text(s, (x, y), c, scale)`: Spleen 8x16
× scale with its cell's top-left at (x, y), accents drawn (mapped through `EXTRA`), instead of framebuf's 8x8
or `_draw_scaled_text` on 240. The issue says that native faces, bold included, are blitted from the frozen
tables (bold = a second blit at x + 1) with a 2-entry palette and a key on the GS4_HMSB frame, and that only
replicated faces use the row drawer, with the bold strike folded into each run (5.5). device.py keeps
`CHAR_W = CHAR_H = 8` (its 128-unit layout constants); a public text height mirroring `textHeight()` is the
MicroPython PR's choice. Below `steami_screen`, the GC9A01 and SSD1327 drivers' own `text()` and
`framebuf.text` stay 8x8, as the Arduino drivers' `text()` keep their fonts (spec anti-goal L73).

**Parity** means *(designer's calls)*: on 240, for the same widget calls, the text uses the same glyphs (same
tables), the same faces, cells and positions as the Arduino goldens, except where device.py keeps a listed
MicroPython difference (Layout, below); and the pixel text `text(s, (x, y), c,
scale)` draws **the same pixels** as Arduino's `text(s, x, y, c, scale)` (F17): same tables, integer
position, the same n×n replication, so this costs nothing beyond using the tables, and the issue asks for a
test that renders a string with accents and compares it with the glyph rows of `spleen.py`. Pixel parity of
everything else stays a non-goal (spec L58): it would also need MicroPython to copy the integer arc scanline,
the triangle edges, Q14 trigonometry and `isqrt`. MicroPython on 128 keeps framebuf's 8x8. The issue
completes the GC9A01 issue of spec section 12 (#368) and replaces its "k = 2, a real `draw_scaled_text`" for
text.

**MicroPython constraints** (from the session that writes the MicroPython GC9A01 screen, 2026-11-26, and its
review of this document against its branch, 2026-11-27):

- **Order:** steamicc/micropython-steami-lib#454 (GC9A01 driver and V1c screen) was merged on 2026-11-27
  and released in micropython-steami-lib v0.30.0, with the GC9A01 screen frozen. The Spleen integration
  starts from v0.30.0, with `spleen.py` produced by this repository's generator.
- **Frame buffer:** the MicroPython V1c frame is a `GS4_HMSB` `FrameBuffer` of palette indices (28 800 B),
  converted to RGB565 by bands in `show()`, not a full RGB565 frame (115 200 B would not fit beside BLE).
- **Native faces (n = 1), bold included, by blit:**
  - Source: the read-only tuple `(memoryview(data)[o:o + h·bpr], w, h, framebuf.MONO_HLSB)`, with
    `bpr = (w + 7) // 8` and `o = index·h·bpr`. `framebuf.FrameBuffer()` needs a writable buffer and refuses
    the frozen `bytes` of `spleen.py` (TypeError on the board); `blit` accepts the tuple read-only. A copy into
    one reused `bytearray` per face size, wrapped by a pre-built FrameBuffer, also works. The MicroPython
    framebuf stub must reject a read-only buffer in its constructor so that host tests catch this.
  - Palette and key: `blit` compares the key with the colour **after** the palette. The palette is a 2×1
    `GS4_HMSB` FrameBuffer whose entry 1 is the text colour index c and entry 0 an index b ≠ c (for example
    b = 1 if c == 0 else 0), and `key = b`, built once per string. Only set bits are drawn and the background
    (rings, bars, the selected menu row) stays. A naive palette (0, c) without a key paints black cells; with
    key 0 it makes BLACK text invisible.
  - Bold: the same glyph blitted again at x + 1 (two C-level blits per glyph, the double-strike pixels of 5.2).
  - Narrow '.' and ':' of a native face: the whole glyph blitted at x − w/4; its columns outside the window
    are transparent, so the window of 5.2 is drawn without cropping (the generator checks that the ink and
    the bold column lie inside the window).
  - Where: on the driver's `framebuf`. The GC9A01 driver's `blit` gains the `palette` argument (today
    `blit(fb, x, y, key)` drops it), and the adapter exposes a text or blit method with the colour resolved
    once.
- **Replicated faces (n ≥ 2)** use a row drawer with one `fill_rect` per run of set bits, never one call per
  pixel (per-pixel drawing made a gauge take 5 to 11 s), and the bold strike folded into each run (5.5). The
  measured cost is about 1 ms of Python per row-level operation through the adapter (about 230 rows of fill
  take 270 to 560 ms in the gauge), so the drawer keeps the Python work per row and per glyph minimal.
- **Firmware flash:** after #454 the margin is 5 572 B (text+data 518 716 B, frozen with `mpy-cross -O3`). A
  stand-in `spleen.py` with the contract of 5.6 and the sizes of 5.5 compiles to about 5 535 B of `.mpy`, and
  the drawing code (decoder, face ladder, narrow window, bold, row drawer, ink bounds, adapter method) adds
  about 1.5 to 3 KB. The fonts therefore cannot be frozen in the current flash layout.
  - **Chosen (maintainer): (a).** Move `FLASH_FS` up by 16 KB, from 0x08080000 to 0x08084000, in a
    STeaMi-specific copy of `stm32wb55xg.ld` (the shared file also serves NUCLEO_WB55 and USBDONGLE_WB55),
    named in `LD_FILES` of `STEAM32_WB55RG/mpconfigboard.mk` for both the mboot and plain builds: FLASH 528K,
    FLASH_APP 496K, FLASH_FS at 0x08084000, length 256K (end 0x080C4000, below the radio stack at
    0x080E0000). `spleen.py` is frozen in `steami_screen/` with the drawing code. The file system is
    reformatted on update, so the user's files are lost; the MicroPython release notes say so.
  - Radio stack (maintainer, 2026-11-27): the only BLE stack that works with both Arduino (STM32duinoBLE) and
    MicroPython (NimBLE) is **BLE_HCILayer**; v1.20.0 takes 80 KB at 0x080E0000. The layout is checked
    against that stack only: the file system ends at 0x080C4000, 112 KB below it. Another stack (Full,
    extended) is not a target and does not constrain this layout; a later HCILayer version whose start
    address moves down needs the check again.
  - Set aside: (b), freezing only the drawing code and installing a precompiled `spleen.mpy` on the file
    system. It would cost about 5.3 KB of the roughly 92 KB of heap left with BLE, an installation step lost
    at each reformat, and a fallback to the 8x8 text when the file is missing.
  - The integration PR reports the firmware text+data and the margin before and after.
- **Layout:** MicroPython follows spec 5.8 on 240 (k = 2), with three kept MicroPython differences: the `_at`
  margins use the real glyph height plus the title row floor and cap, the gauge outer radius is
  `radius − u(4)`, and the face cell is `W·11/128`. device.py also keeps the behaviours that spec 5.9 marks
  **MP**:
  - no narrow '.' and ':' (B6): every value glyph is 8f wide, "88.8" is 32f;
  - three graph labels, with "1k" formatting;
  - a fixed menu bar with left-aligned items;
  - a gauge fill 2 px thicker;
  - the value label lowered when no character fits.

  With Spleen these give other positions on 240 (in mockup 03, Arduino draws "23.5" 56 px wide at x 32 and
  152; device.py would draw 64 px at x 28 and 148). The integration therefore (1) adds the narrow window
  `[w/4, 3w/4)` and the `w/2` advance of 5.2 to device.py's `_fit`, `_put` and `_value` ("88.8" = 3 advances
  + advance/2), and (2) either adopts B1 and B3 for the graph and menu or lists them as remaining differences.
  The Arduino goldens are reused only for title, subtitle, value, unit, label, gauge labels, compass and
  watch letters, and `text(s, at)`. The 5.8 rules restated in font metrics in 5.2 and 5.3 (advance, cell
  height, ascent, ink rows, title cap, floor, graph labels) are the ones both languages apply, so `device.py`
  is parameterised by face instead of `8 × k`.
- **device.py changes the issue lists:** `max_chars` = `width // advance` of the TEXT face at k (16 / 30;
  #454 gives 15 on 240, 5.3); the menu's 6 rows on 240 with a bar clipped per scanline, or 5 rows kept as a
  difference (5.3); the graph label rows of 5.3 if B1 is adopted.
- **Visible change and tests:** `text(s, (x, y))` on the V1c is 16 px tall at scale 1 (changelog, like
  Arduino). Tests to rewrite: `test_mock_widget_text_is_scaled_by_k` (routing), the 240 goldens imported from
  Arduino (`kHiBoxes` and the others, regenerated by 4b-font), the menu row count. `gauge_demo.py`: its
  comment "8x8 scaled by k" is updated; its erase box (112 × 18) still covers "1234 mm" (56 × 16) and stays.
- **Timing** (V1c, measured with `time.ticks_us` in the MicroPython PR, reported before and after): title,
  value with unit and label, subtitle and gauge labels of programs 01 and 04, and `text(s, (x, y), c, 2)` of
  15 characters. Acceptance: no widget's text draws slower than with today's `_draw_scaled_text`, and a
  native-face widget's text takes under 10 ms. Estimate for program 04 on 240: about 31 blits, 3 to 10 ms,
  against about 150 ms for today's per-pixel ×2/×4 text. The gauge's 270 to 560 ms and `show()`'s 167 ms are
  not affected.
- **128 px:** for MicroPython, the 8x8 font stays at 128 px. `framebuf.text` draws each UTF-8 byte above
  127 as glyph 127, a checkerboard (modframebuf.c:844-848), so today `é` or `°` (two bytes) is two
  checkerboards, 16 px, while the layout counts one cell. MicroPython folds codes 128..159 through `FOLD`
  before `framebuf.text`, and each folded character reaches it as a single ASCII byte, so an accented letter
  is one 8 px cell; this is an improvement allowed on the SSD1327. `°` (`FOLD[0]`, which maps to itself) is
  drawn by MicroPython with the same 8x8 degree glyph as Arduino, one 8 px cell
  (steamicc/micropython-steami-lib#461), so `°` at 128 matches Arduino *(maintainer's decision)*.
  Its rendering at 128 may change elsewhere where it improves, with the maintainer's agreement: the MicroPython gauge at 128
  is already a ring filled by horizontal rows, closer to Arduino. This document does not require MicroPython's
  128 rendering to stay pixel-identical; the bit-identical 128 requirement applies to the Arduino library.

### 8.2 Wiki (`steamicc/wiki_steami`, after the release that ships this delivery)

An issue and a PR by the controller once the release is tagged (the wiki's example listings follow the
released library), in `docs/software/arduino/exemples/steami-board.md`. Line numbers are those of the wiki
at 05ea52c; the issue lists the sentences, the PR rebases them:
- Drawing:
  - L231 ("Ce qu'on observe") and L235 (the photo's alt text): `20°C e` becomes `20°C é` on the V1c; the photo
    `steami-board-drawing.jpg` is retaken under the webcam.
  - L245: "Le texte suit la même règle : `s` vaut 2 sur l'écran de 240 pixels (des caractères de 16 × 16
    pixels) et 1 sur celui de 128 pixels (8 × 8 pixels)" becomes: `s` sizes the gauge strokes only; the text is
    drawn at its natural size, 8 × 16 pixels on the V1c and 8 × 8 on the V1a/V1b, and `textHeight()` gives the
    height used to place the second line. "La police n'a pas de lettres accentuées : `é` s'affiche `e`" becomes:
    `é` is drawn on the V1c and shown as `e` on the V1a/V1b.
  - L255: the bullet "`text(texte, x, y, couleur, s)` … agrandi `s` fois" becomes `text(texte, x, y,
    couleur)`, with the optional last argument `scale` (1 by default) that enlarges it `scale` times; and a
    bullet for `textHeight()`.
- ScreenTemperature, L285: "16 × 16 sur une V1c" and "32 [pixels de haut] sur celui de 240" become 8 × 16 for
  title, unit and subtitle and 16 × 32 bold digits.
- ScreenComfort: L332, labels "5 × 7 pixels (10 × 14 sur l'écran de 240)" become 6 × 12 on the 240 screen;
  L333, side values "24 sur celui de 240, un peu moins qu'au centre" become 32 px on the 240 screen, the same
  size as at the centre.
- Every other sentence giving a V1c glyph size, and the photos of the Screen examples, which are retaken.

## 9. Edits to the spec and the README made by the delivery

Line numbers are those of the spec **and of `lib/steami_board/README.md`** on branch `feat/steami-display-4b`
at fc9a7e9 (the base of this delivery once PR #277 is merged; on today's `main` the README's "twice as large"
sentence is at L522-523); the builder rebases them.

Spec:
- L3-4 (status): add "Amended on 2026-11-26 by `2026-11-26-steami-display-v1c-font-design.md` (V1c fonts)".
- L40-44 (section 1): on the 240 px panel all text, widgets and primitives, uses Spleen bitmap fonts; 128
  keeps 8x8 and 5x7.
- L58 (non-goals): unchanged in substance; add "on 240 the text of widgets and primitives uses the same glyph
  tables, faces and cells as MicroPython, and the pixel text draws the same pixels (decision 7)".
- L59 (non-goals): drop "a big-digits font"; add "Spleen on 128, or above 16x32".
- L77-78 (anti-goals): the Spleen faces are internal, like 5x7 and k; "no student-visible second font" holds
  on each panel (one font per panel).
- L89-90 (constraints, round screens): "173 to 181 px at rows 37 to 41 on 240" becomes the chord at row 37
  (titles stay there on 240).
- L91-92 (constraints, Font): add Spleen 2.1.0, BSD-2, generated tables.
- L108-109 (approaches): "Text on 240 px" and "Fonts" rows point to section 4 here.
- L127 (5.1 files): add `SteamiFontSpleen.cpp` (generated) and the extended decoder in `SteamiFont.h`.
- L171-185 (5.3): add the Spleen faces, the 240 ladders (5.1), the primitives' faces (5.3 here), drawn
  accents and code space (5.4), the generalised narrow window, bold (5.2). L173-175: "It is the only font the
  student sees (`text()`, `textWidth()`, `CHAR_W = CHAR_H = 8`)" becomes "One font per panel: on 128 the 8x8
  is the font of the primitives and the widgets (5x7 for secondary labels); on 240 both draw Spleen (8x16 ×
  scale, 6x12 for labels). `CHAR_W = CHAR_H = 8` stay the 8x8 metrics; `textHeight()` gives the panel's line
  height". L180-182: accents drawn on 240, folded on 128. L183-185: "every glyph advances `8·s`" holds on both
  panels.
- L265-275 (5.6 API): `text()`'s comment "8x8 × scale on 128, Spleen 8x16 × scale on 240"; add
  `int16_t textHeight(uint8_t scale = 1);   // 8·scale on 128, 16·scale on 240`; `CHAR_W, CHAR_H` commented
  "8x8 metrics (the 128 px screen)"; `textSmall()`'s comment "5x7 on 128, 6x12 on 240".
- L357 (`maxChars()`): 16 on 128, 30 on 240.
- L430-431 (5.8 rule 2): glyph width = face advance; cell height 8f on both panels.
- L436-438 (rule 3): "a text that is 7 % wider on V1c moves down" applies to 128-unit layouts only; on 240
  titles stay at row 37.
- L443-450 (rule 3): cap from 13 TEXT cells (29 / 14); the title band on 240 is row 37.
- L458-461, L471 (rule 6): "full 8-row cell" becomes "full cell"; widths from the face; truncation example.
- L483-485 (rule 7): `value()` lowers f down to k, at every anchor.
- L493-504 (size table): 240 column per 5.1; side-by-side values f 4; '.' and ':' `w/2`; labels 6x12;
  menu 6 rows on 240.
- L506-516 (long titles): 240 at row 37, about 22 cells whole.
- L524-534 (5.9 rows): title (row 37 on 240, cap 14), subtitle (8x8 ×k on 128, 8x16 on 240; one-line rows
  202, blocks 177/197), gauge and graph labels 6x12, menu 6 rows on 240, compass/watch letters 8x16 through
  `text(…, 1)`. In the `value` row (L526):
  - "tried from `max(scale, 1)·k` … down to 1" becomes "down to k" (faceless f skipped on 240); "At f = 1, x
    is clamped" and "**Truncation**: at f = 1" become "f = k";
  - "a 4-column window of their glyph (columns 2 to 5) with an advance of `4·f`; every other glyph advances
    `8·f`" becomes the window `[w/4, 3w/4)` and the `w/2` advance of the face, every other glyph the face's
    advance, in every VALUE text (5.1, 5.2);
  - "Unit: 8x8 ×k" becomes "8x8 ×k on 128, 8x16 on 240"; "Label: **5x7** ×k" becomes "5x7 ×k on 128, 6x12 on
    240", with the label face's cell height (5.3);
  - the 240 figures are regenerated: "×3 on 240 (84 px in 110 px; ×4 would need 112 px in 109)" becomes f 4
    (56 px in 109), "8888" and 6-character values, "Labels of up to … 8 on 240", "the ×1 W/E region is 112
    px" (now the f 2 region), and the `yOffset` truncation thresholds;
  - the NE example of section 6 replaces "x 180, row 22" and "cut to TE at x 110".
- L568 (5.12 flash): add Spleen tables 5 264 B (8x16 also linked by sketches that call `text()`); code 14 to
  18 KB.
- L796-799 (known differences, fonts): text on 240 is Spleen 8x16; values 16x32 bold, shorter than the
  mockups.
- L800-803 (known differences, labels): 6x12 on 240, close to the mockups' size; the 5x7 figures stay for 128.
- L804-807 (known differences, value block): the 240 figures recomputed from the goldens (04: "342" inks rows
  97..116 against about 101..131 in the mockup at 240; the unit inks 128..134 against about 142..156).
- L812-815 (known differences, 06): "5 rows instead of 6" on 128 only; 6 rows on 240.
- The program 04 entry that 4b adds if the maintainer accepts the overlap ("on 240 and 128") becomes "on 128
  (V1a/V1b)".
- L831, L849-892, L896, L905-911 (section 7): font cases and 240 numbers as section 7 here (L831: glyph sizes
  per panel, `textWidth("°C") == 16` on both, `"é"` draws `e` on 128 and `é` on 240, `textHeight()`); L873 "cut
  to TE at x 110" becomes "TEMP whole at x 159"; L896 "5 visible rows on both panels" becomes "5 on 128, 6 on
  240". L837-838 and L920 (the gallery as the V1c reference for `Drawing`) stay; the spec has no `Drawing`
  listing in section 6, so no program edit there.
- L949-955 (section 8): add the row of section 10 here between 4b and 4c.
- L967-968 (decision 4): "no big-digits font in v1" is revisited by decision 7.
- L969-970 (decision 5): "k = 1 or 2 for glyphs" stays for strokes and the 128 glyphs; on 240 glyphs follow
  decision 7.
- After L978 (section 9): decision 7, maintainer 2026-11-26: V1c text uses Spleen (option 2), widgets and
  primitives, one font per panel; values bold by a 1-px double strike; same tables in MicroPython; and the
  designer's calls F1 to F17 of section 11 here.
- L990-991 (B1): graph labels 5x7 on 128, 6x12 on 240.
- L995-998 (B3): "Temperature" at row 37 on V1c; 5 visible rows on 128, 6 on 240.
- L1006-1010 (B6): window `[w/4, 3w/4)` with a `w/2` advance (columns 2 to 5 and 4·f for 8x8), in every VALUE
  text whatever its face; "×3 on 240" becomes "f 4 on 240".
- L1016-1021 (B8): cap 14 on 240; title band row 37 on 240.
- L1058 (section 11, item 8): `menu()` shows 5 rows on 128, 6 on 240.
- L1083-1088 (section 12): the MicroPython font issue of 8.1 here; L1085 "5 visible rows" becomes
  "5 on 128, 6 on 240".
- L1104 (section 13): add section 12 here.

`lib/steami_board/README.md` and `keywords.txt`:
- L430 (`text()`): "8x8 glyphs on the 128 px screen, Spleen 8x16 on the 240 px screen, `scale` times larger;
  the same width on both (`CHAR_W`, 8 px per glyph at scale 1), height `textHeight(scale)` (`CHAR_H`, 8, is
  the glyph height on the 128 px screen only). UTF-8: `°` is drawn; accents are drawn on the 240 px screen and
  folded to their plain letter on the 128 px screen". `CHAR_H` is described in this row, where the README
  names it today *(designer's call, answering the fidelity check: no new row for the constants)*.
- New row after L431 (`textWidth()`), `int16_t textHeight(uint8_t scale = 1)`: "Line height in pixels: 8 per
  scale on 128, 16 on 240". `keywords.txt`: `textHeight KEYWORD2`.
- L508 (`maxChars()`): "15 on 240" becomes 30.
- L534-536: "glyphs are twice as large on 240" becomes "text on 240 uses fonts drawn for that panel (8 px per
  character, 16 px tall), for widgets and `text()` alike".
- The 240 known differences of 4b's gauge labels (min label touching the ring, slid labels overlapping) are
  removed wherever 4b's PR placed them (README or PR text); the 128 ones stay.
- New "Third-party fonts" section (5.6).

## 10. Delivery and "done when"

Order *(controller's choice)*: **4b** (PR #277, merged with program 04's overlap accepted provisionally) →
**4b-font** (this amendment) → **4c** (graph, menu, `SteamiHistory`, WeatherStation, built on the final 240
metrics so that their positions are computed once).

| # | Delivery | Content | Done when |
|---|---|---|---|
| 4b-font | V1c fonts | Generator and its tests, `SteamiFontSpleen.cpp`, extended decoder, faces in the kernel (`cellWidth`, `inkRows`, `drawGlyphs`, `glyphsAt`, label height, cap, fit floor, bold), the primitives on 240 (`text()`, `textSmall()`, `textHeight()`), `maxChars()`, the `Drawing` example, README and `keywords.txt` edits and "Third-party fonts", spec edits of section 9. Outside `lib/steami_board`: `scripts/gen-spleen-fonts.py`, `scripts/tests/` (generator and package tests), `scripts/arduino-package/README.md`, `scripts/check-footprint.py` (`SKETCHES`), `.gitignore` (font cache), `tests/native/helpers/SteamiGallery.h`, `tests/native/test_steami_display/` (240 expectations), the two screen suites and `golden_layout.py`, `.github/workflows/` (regeneration check) | 128 goldens and PPM hashes unchanged (primitives gallery included); 240 goldens and primitive expectations regenerated and passing, natively and under ASan/UBSan; `test_steami_display_board` and `test_steami_display_rules.py` pass unchanged; widget frames and bound helpers ≤ 64 B; footprint gate passes with ScreenGauge's and Drawing's deltas reported, Drawing linking the 8x16 blob only; the package README carries both font notices (test); examples 01 to 04 and 07 to 09 and `Drawing` reviewed on V1c under the webcam, 04 with no text on the ring, `Drawing` matching the new 240 gallery dump; SSIM before/after in the PR. |

## 11. Decisions

**Maintainer decisions (2026-11-26)**
- Option 2: bitmap fonts drawn for 240 px on the V1c, internal; the Spleen 8x16 / 16x32 look of the spike
  ("Oui"); the same fonts in MicroPython.
- Question 1 of the first draft, "Spleen aussi": on the V1c the `SteamiDisplay` primitives (`text()`,
  `textWidth()`, `textSmall()`, the `Drawing` example) also draw Spleen, so the V1c shows one font. This
  reverses the controller's earlier "primitives unchanged" and rejects the first draft's recommendation (a).
- Question 2: values bold by a 1-px double strike on 12x24 and 16x32 faces, confirmed.
- V1a/V1b (128 px) unchanged and bit-identical, primitives included.

Controller's choices: one generator for both languages; order 4b, 4b-font, 4c.

Designer's calls (reversible), numbered for the spec:
- **F1** Faces and ladders of 5.1 (TEXT 8x16 × s, LABEL 6x12, VALUE 8x16/12x24/16x32 with replication above
  f 4, odd f skipped); no 5x8, no 32x64.
- **F2** Value fit down to k, not 1, at every anchor (corners included).
- **F3** 12x24 and 16x32 carry the 15 value glyphs only; other value text uses the TEXT ladder.
- **F4** Cell height 8f on both panels, advance from the face; narrow window `[w/4, 3w/4)`, advance `w/2`, in
  every VALUE text whatever its face (a mixed text on 240 included); the face of a VALUE text is chosen from
  the whole text.
- **F5** Bold (maintainer) on every 12x24 and 16x32 face, replicated or not, inside the cell; never on 8x16
  faces nor in primitives; advance unchanged.
- **F6** Accents drawn on 240 by widgets and primitives, folded on 128 by both; extended code space 32..159.
- **F7** Cap from 13 TEXT cells (titles fixed at 37 on 240); label height from its face; `maxChars()` 30 on
  240; menu 6 rows on 240; letters cell-centred.
- **F8** Row-aligned MSB-left tables in a generated `SteamiFontSpleen.cpp`, one `const` object per face;
  tarball fetched by sha256, outputs committed, CI regeneration check.
- **F9** Licence notices (MIT 8x8 and BSD-2 Spleen) in the shipped package README
  (`scripts/arduino-package/README.md`), tested; also in the generated file and `lib/steami_board/README.md`.
- **F10** The `spleen.py` contract of 5.6, frozen and tested here; the generator lives here; `spleen.py` is
  committed only by the MicroPython PR.
- **F11** Program 04's goal is on 240; the 128 overlap stays a known difference; 4b's 240 gauge-label known
  differences are removed after the regenerated goldens confirm them clear.
- **F12** Stack placement of 5.7 (face id in `Run`'s padding, no new local in a 64 B frame, primitives'
  frames recorded, not bound; public signatures, `Run` and `Region` fixed, private helper signatures free
  with no new stack argument in a 64 B frame); ScreenGauge joins the footprint gate, Drawing's delta reported
  and checked to link the 8x16 blob only (`text()` and `textSmall()` pass their own 240 face, 5.5).
- **F13** Primitives draw the panel's base face × scale: 8x16 × scale for `text()` and 6x12 × scale for
  `textSmall()` on 240, (x, y) the cell's top-left, full sets only (no value sets, bold or narrow '.'/':');
  `textWidth()` and `textSmallWidth()` unchanged. Reasons: one font per panel, "n times bigger" kept, widths
  and `maxChars()` consistent, no new table.
- **F14** New `textHeight(scale)` (8·scale / 16·scale); `CHAR_W = CHAR_H = 8` kept as the 8x8 metrics.
  Reasons: the line height is the only metric that depends on the panel; the constants are released API and
  match device.py.
- **F15** `Drawing`: text at scale 1, second line at `textHeight() * 3 / 2`, `s` for the strokes only. Reasons:
  same layout on 240 as published, bit-identical on 128, text like the widgets', `é` drawn.
- **F16** One drawing path: on each panel `text()` at scale n draws the kernel's TEXT at f = n·k, `textSmall(…,
  1)` its LABEL; `glyphsAt` calls `text(…, 1)` and builds no `Run`. Tested (section 7).
- **F17** MicroPython: the pixel text on 240 draws the same pixels as Arduino's `text()` (a goal); other
  primitives stay layout parity only.
- **F18** (from the MicroPython review, 2026-11-27) `spleen.py` also carries `FOLD`, and its code mapping
  sends any character outside the table to `?` (5.6); bold at n ≥ 2 widens each run by one font column (5.5);
  graph axis labels placed by their ink edges (5.3); six menu rows on 240 require the scanline-clipped bar
  (5.3). The MicroPython method of 8.1 (read-only blit source, palette with key, bold as a second blit,
  freezing only with a moved `FLASH_FS`, timing acceptance) is that session's, recorded for the issue.

## 12. Left to the builder

- Names and layout of the face type (`SteamiFontFace` or similar; not `SteamiFace`, taken by the smileys).
- Whether the faces switch on `k == 2` or on width 240 (the tests use 128, 240 and 2x1 panels only); the
  primitives, `textHeight()` and the kernel use the same predicate.
- The row drawer (runs per font row, `fillRect(run·n, n)`), whether the primitives and the kernel share it
  (the face is then its argument, never picked by a run-time flag, 5.5), and whether 8x8 goes through it or
  keeps `glyph()`, as long as 128 stays bit-identical.
- The exact helper signatures within the limits of 5.7 (`const Run&` or a face id).
- The generator's command line, cache directory, Makefile target and CI wiring; the exact tarball URL
  (checked against the sha256 above).
- How the 128 PPM hashes are recorded and checked.
- Every regenerated 240 number of the spec, from `golden_layout.py`, and the 240 primitive expectations.
- The release-please commit type for the changes visible on 240 (`textHeight()` added, `maxChars()` 30,
  `text()` 16 px tall at scale 1), following the repository's release rules; whatever the type, the commit body
  says that `text()` on the V1c is twice as tall at the same width, and that `textHeight()` gives the pitch.

## 13. Questions for the maintainer

None open: both questions of the first draft are answered (section 11), and the three questions of the second
builder check (narrow '.' and ':' in a mixed VALUE text, kernel signatures, wiki sentences) are settled as
designer's calls in 5.1, 5.7 and 8.2. Every designer's call stays open to correction; F13 to F17 are new in
this revision and follow from the answer "Spleen aussi". F18 records the MicroPython session's review of 2026-11-27 (21 findings, each
cross-checked); every one is applied in 5.1 to 5.6 and 8.1.
