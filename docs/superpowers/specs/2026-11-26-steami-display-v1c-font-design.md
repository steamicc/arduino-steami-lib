# STeaMi display layer, amendment: the V1c fonts

Status: Draft for the maintainer's review, 2026-11-26. The maintainer approved the look of the spike ("Oui")
and asked for the same fonts in MicroPython; every other choice below is marked as the controller's or the
designer's and can be corrected. Section 13 holds the only open questions.
Repository: `steamicc/arduino-steami-lib`, base `main` after PR #277 (delivery 4b) is merged.
Amends: `2026-11-25-steami-display-design.md` (below: **the spec**). The delivery that builds this amendment
edits the spec as section 9 lists, so that the spec stays the single authority; this file then keeps the
reasons and the figures.

Sources: the spike of 2026-11-26 (mockups 01 and 04 drawn at 240 px with Spleen 8x16 text and 16x32 values,
compared with today's doubled 8x8 and with the `_ref.png`), Spleen 2.1.0 (<https://github.com/fcambus/spleen>,
BSD-2-Clause, BDF files 6x12, 8x16, 12x24, 16x32 measured for this document), the kernel of
`lib/steami_board/src/SteamiScreen.cpp` (`Run`, `inkRows`, `cellWidth`, `valueWidth`, `fitGlyphs`,
`marginNs`, `drawGlyphs`, `glyphsAt`, `gaugeLabel`), `SteamiFont.h/.cpp`,
`tests/native/test_steami_screen/golden_layout.py`, the 4b plan (`docs/superpowers/plans/2026-11-26-steami-display-4b.md`),
`scripts/build-arduino-package.py`, `scripts/check-footprint.py`, and the builder and fidelity checks of this
document (2026-11-26).

## 1. Intent and why

On the V1c (GC9A01, 240x240 colour, the only release that will be deployed, decision 6 of the spec) every
widget draws MicroPython's 8x8 framebuf font doubled (k = 2): 16x16 blocks, 15 characters per line, a look
far from the mockups' thin vector text. The width also breaks layouts: in program 04, "VL53L1X ToF" is 176 px
wide, rises to row 188 and paints over both ends of the gauge ring, and "Distance" (128 px) touches the
ring's top. MicroPython has the same problem as soon as it scales text on the GC9A01 (spec section 12).

The fix is a bitmap font drawn for this pixel size, used only by the widgets on the 240 px panel. The spike
shows that Spleen 8x16 for text and 16x32 for values brings titles, units and subtitles close to the mockups
and removes the 04 overlap on the V1c without touching the example or the gauge. Both languages get the same
tables from one generator, so a course looks the same in Arduino and MicroPython on the V1c (layout parity,
section 8).

## 2. Goals, non-goals, anti-goals

Goals
- On the 240 px panel, every `SteamiScreen` widget draws Spleen glyphs: text 8x16, values up to 16x32,
  secondary labels 6x12.
- Program 04 draws with no text over the gauge ring **on 240**, unchanged. On 128 (V1a/V1b, rendered "at
  best" under decision 6) the overlap stays a known difference, because 128 must stay bit-identical
  *(designer's call; section 9 rewrites 4b's entry to "on 128")*.
- The 128 px rendering stays **bit-identical**: same goldens, same PPM bytes.
- French accents are drawn by widgets on the V1c ("Température", "Été").
- One generator, pinned to Spleen 2.1.0, writes the C++ tables and the MicroPython module (5.6).

Non-goals
- Anti-aliasing, vector or proportional fonts (unchanged from the spec).
- Spleen on the 128 px OLED, or in `SteamiDisplay` primitives: `text()`, `textWidth()`, `textSmall()`,
  `CHAR_W`/`CHAR_H` and the `Drawing` example keep the 8x8 and 5x7 fonts *(controller's choice; its visible
  consequence is in 5.1 and question 1 of section 13)*.
- A value face bigger than 16x32 (Spleen has no 24x48; 32x64 is too big for a 240 px disc and costs 32 KB).
- Full ASCII in the 12x24 and 16x32 faces (5.5).
- Pixel-identical rendering with MicroPython: still a non-goal of the spec (its L58); section 8 defines the
  parity this delivery aims at.

Anti-goals
- Never change a pixel on 128 px. Never fetch a font at build time: the tables are committed.
- No student-visible font choice, no new public API except the value of `maxChars()` on 240.
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
  12 KB. The library cannot know the release at compile time (no `STEAMI_BOARD_*` in a `.cpp`), so the V1c
  tables are linked into every sketch that uses `SteamiScreen`, V1a/V1b included. They are referenced only from
  `SteamiScreen`, so `--gc-sections` drops them from a sketch that uses only `SteamiDisplay` (such as
  `Drawing`, today's only footprint sketch).
- Stack: at most 64 B per widget frame (spec 5.11), measured on the target. `value()`, `subtitle()` and
  `compass()` sit at exactly 64 B today; `gauge()` reached 72 to 80 B in 4b prototypes with one extra local (4b
  plan, Global Constraints and Task 3).
- Geometry: polar points stay Q14 (`polarX/polarY`); only the cell placed around a point changes.
- No heap, no `String`, both cores (STM32duino 2.12 and 3.1.0), native tests without a board.
- Scope (spec B4): the delivery changes only this repository; MicroPython is a separate PR.
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

## 5. Design

### 5.1 Who uses which font

| Ink (kernel `Ink`) | 128 px (k = 1), unchanged | 240 px (k = 2) |
|---|---|---|
| TEXT at f = s·k: title, subtitle, unit, `text(s, at)`, menu items, compass and watch letters, graph current value | 8x8 × f | Spleen 8x16 × s (native at s = 1) |
| LABEL (f = k): value label, gauge min/max, graph axis labels | 5x7 × k | Spleen 6x12 × 1 |
| VALUE at f, text made only of value glyphs | 8x8 × f, narrow '.' ':' | f 2: 8x16; f 3: 12x24; f 4: 16x32; ladder above f 4 below; bold on every 12x24 and 16x32 face, replicated or not (F5) |
| VALUE at f, a text with any other glyph ("Uncomfortable", "Été") | 8x8 × f, narrow '.' ':' | as TEXT: 8x16 × f/2 (odd f has no face) |

Value glyphs: `' '`, `'+'`, `'-'`, `'.'`, `':'`, `'0'..'9'` (15 glyphs; everything `SteamiValue::format()`
prints for a number, and "--").

Factors without a native Spleen size *(designer's call)*: a face is a Spleen size replicated n times (each
font pixel drawn n×n). On 240 the VALUE ladder for f > 4 is 16x32 × f/4 when 4 divides f, else 12x24 × f/3
when 3 divides f, else 8x16 × f/2 when f is even, else no face (f 6: 12x24 × 2; f 8: 16x32 × 2; f 10: 8x16 × 5).
A factor with no face is skipped by the value fit. Reasons: scale-n text keeps the meaning "n times bigger"
(the student's `text(s, at, c, 2)` is 8x16 doubled), every face keeps a cell height of exactly 8f (5.2), and
no table bigger than 16x32 is needed.

f = 1 never occurs on 240 *(designer's call)*: `value()` now lowers f from `max(scale, 1)·k` **down to k**,
not to 1 (on 128, k = 1: unchanged), **at every anchor, corners included** (section 6 gives the new NE
figures). Reasons: 8x16 at f 2 already has the 8 px advance of the old f 1, so the W/E and CENTER capacity at
the last step is unchanged (15 glyphs in 120 px), and no 5x8 table is needed. LABEL is fixed at 6x12, so no
widget reaches a 1-px-per-unit face.

Two fonts on the V1c (consequence of the controller's choice, stated for the maintainer): the spec's "8x8 is
the only font the student sees" (spec L174-175) no longer holds on 240. `screen.text(s, x, y)`, `textWidth()`
and the `Drawing` example draw the 8x8 at its physical size (8 px tall), while `screen.text(s, at)` and every
widget draw Spleen 8x16; a sketch that mixes them shows both. On 128 nothing changes. Section 9 rewrites
L174-175; section 13 asks the maintainer to confirm.

### 5.2 Metrics

- **Cell height stays 8f on both panels** (8x16 at f 2, 12x24 at 3, 16x32 at 4, replicated faces likewise):
  every vertical formula of the spec in `8f`, `8k` or `ch` (floor of `margin_ns`, `margin_ew` = device.py's
  `ch + 4`, S row `− 8f`, value block `8f + 8f/3 + 8k`, unit at `value top + 8f`, `glyphsAt` half height)
  keeps its numbers. The builder names this "cell height" in the code instead of `CHAR_W`/`CHAR_H`.
- **Advance**: the face's cell width: 8f on 128, **4f on 240** (Spleen cells are h/2 wide; 6x12 advances 6).
  Every `8·f` used as a *width* in spec 5.8/5.9 becomes the face advance: `cellWidth`, `valueWidth`,
  `textWidth`-as-measured-by-widgets, the 13-cell cap, the menu's 12 cells, `maxChars()`.
- **Narrow '.' and ':' (B6), generalised** *(designer's call)*: in VALUE ink, drawn from the column window
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
- **Bold values** *(controller's choice: double strike; the maintainer did not pick between plain and
  bold, question 2 of section 13)*: a VALUE glyph from a 12x24 or 16x32 face, replicated or not, is drawn
  twice, at x and at x + n (one font column). The value glyphs ink columns 1..10 of 12 and 2..13 of 16, so the
  strike stays inside the cell: **advance, widths, fits and ink rows are unchanged**, and neighbours keep 1
  (12x24) or 3 (16x32) free columns. 8x16 values (f 2, and 8x16 × 5 or × 7 at f 10 or 14) are not bold
  *(designer's call)*: its digits ink columns 0..6, and a strike would close the only gap between them. The 128
  panel is never bold.

### 5.3 Layout rules restated (what changes on 240; 128 identical)

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
  many characters fit on a line", which doubled; it also equals device.py's `width // CHAR_W`)*.
- `menu()` visible rows (B3): "the marker plus 10 cells" is 12 TEXT cells, 96 px on both panels. On 240 the
  chord of row 6 (the spec's rows `u(35 + 14·i)`: cells 196..211) is 157 px, so **6 rows are visible on 240**
  (5 on 128), as mockup 06 *(designer's call: V1c first; the MP mark "6" disappears on 240)*. 4c builds it.
- Compass and watch letters (`glyphsAt`): drawn through the kernel's TEXT face (not `SteamiDisplay::text`),
  the cell centred on the same Q14 point, as device.py. The cap ink of 8x16 (rows 2..11, centre 6.5) sits 1 px
  above the cell centre (row 7.5), 1.5 px above the Q14 point (row 8); accepted *(designer's call: parity with
  MicroPython's `ly − CHAR_H // 2`)*.
- Value fit floor k at every anchor (5.1). B9's "a scale of 0 counts as 1" stays.

### 5.4 Accents

Spleen 6x12, 8x16, 12x24 and 16x32 have every letter of the spec's folding table (`à â ä ç é è ê ë î ï ô ö ù
û ü ÿ` and capitals); only U+0178 (Ÿ) is missing from 6x12. Decision *(designer's call)*: **drawn on 240**
by TEXT and LABEL faces, **folded on 128** as today, and still folded by the primitives on both panels.
Reasons: French courses, no layout rule (accented capitals ink from row 0 of the cell, which the inked-rows
chord already handles), 32 glyphs × 28 B = 896 B. The generator fills the 6x12 Ÿ slot with its Y glyph. Value
faces need no accents: a text with an accent is not made of value glyphs and uses the TEXT ladder.

Glyph codes: `steamiNextGlyph()` keeps its contract (32..127, folded) for `SteamiDisplay`. A new decoder
returns the extended code space 32..159: 32..126 ASCII, 127 degree, 128..158 the 31 Latin-1 letters in the
order of `kFolds` (À Â Ä Ç È É Ê Ë Î Ï Ô Ö Ù Û Ü à â ä ç è é ê ë î ï ô ö ù û ü ÿ), 159 Ÿ; a fold function maps
128..159 to the base letter for 8x8 and 5x7.

### 5.5 Storage, files, flash

Format *(designer's call)*: rows top to bottom, each row `ceil(w/8)` bytes, most significant bit = leftmost
column (MicroPython `framebuf.MONO_HLSB`). Full sets are 128 glyphs indexed by code − 32; value sets are 15
glyphs in the order of the 15-character key `" +-.:0123456789"`. A glyph of face (w, h) is the
`h·ceil(w/8)` bytes at `index·h·ceil(w/8)` of its face's blob. MicroPython can wrap one glyph of a native-size
face (n = 1) in a `FrameBuffer` and `blit` it (with a palette argument on the RGB565 backend); replicated
faces and the bold strike need a row drawer, as in Arduino (section 8 says so in the issue).

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
in the MicroPython firmware, and the only big text in the mockups is a number; text at scale 2 uses 8x16 × 2.
Row alignment costs 564 B over bit-packing and keeps the drawing loop and the MicroPython blit simple.
With the code (decoder, face lookup, row drawer), the layer grows by about 6 KB; the spec's 8 to 12 KB
estimate becomes 14 to 18 KB.

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
    decoder.
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
    change to `build-arduino-package.py`: the README is already copied;
  - `scripts/tests/test_build_arduino_package.py` gains a test that reads the repository's real
    `scripts/arduino-package/README.md` and asserts both notices are present (the existing fixture README stays
    for the copy tests);
  - `lib/steami_board/README.md` gains the same "Third-party fonts" section for readers of the repository;
  - MicroPython: the same notice in the module (above) and in its README (the follow-up issue).

### 5.7 Stack *(designer's call, answering the fidelity check)*

`value()`, `subtitle()` and `compass()` have no headroom (64 B), and `gauge()` grows past 64 B with one extra
live local. This delivery changes code they call or may inline: the f loop of `value()` (stop at k, skip f
with no face), the label height in `drawValueParts`, `glyphsAt` (now builds a `Run` instead of calling
`SteamiDisplay::text`), 32-bit row masks and the face lookup. Rules:
- No new local in a public widget frame. The face and ladder logic sits in helpers that hold no buffer: for
  example `placeValue()` returns false for an f with no face, so `value()`'s loop stays `t.f--`.
- `Run` stores a one-byte face id in its padding byte (`s`, `glyphs`, `y`, `ink`, `f`, `shrink` use 15 of
  its 16 B on ARM), so `Run`, `Region` and the 4a signatures stay as they are.
- Face helpers are marked `__attribute__((noinline))` when the compiler inlines them into a bound frame.
- Fallback when a bound frame still goes over 64 B: the 4b plan's rule (the task shrinks its locals; if that
  is impossible without breaking another spec rule, the builder stops and reports a spec deviation; a helper
  is never exempted).

## 6. Effect on the mockups at 240

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

## 7. Tests

- 128: every assertion of `test_steami_screen`, `test_steami_screen_mockups` and `test_steami_display`
  unchanged; the delivery records the sha256 of every 128 px PPM on `main` before the change and a test or CI
  step checks them after (bit-identical goal).
- 240 goldens: `golden_layout.py` parses `SteamiFontSpleen.cpp` (as it parses `SteamiFont.cpp` today) and
  applies 5.1 to 5.4; every 240 number of the spec's section 7 is regenerated (titles at 37, subtitles at
  202 and 177/197, W/E f 4, label capacities, `yOffset` truncation thresholds, `text("ABCDEFGH", N, c, 2)`
  now whole on 240, compass and watch letter cells, NE value at f 2 and label "TEMP" whole at x 159, the 4b
  gauge label boxes of (0, 100), (−1e8, 1e8) and the other printed pairs).
- New native cases: 'é' drawn by a widget on 240, folded on 128 and by `SteamiDisplay::text` on 240; Ÿ in a
  label on 240 draws Y; a value with a letter uses the TEXT ladder; on 240 the value fit stops at f 2 (CENTER,
  W/E and corners) and skips f 5 and 7; bold adds ink only inside the cells (no pixel past `x + valueWidth`),
  also on a replicated 12x24 × 2 face; '.' ink centred in its narrow window on every face; no pixel outside
  `radius() + 1` for every widget with long strings, as today.
- Program 04 at 240, geometrically *(designer's call: the DARK subtitle on the DARK track cannot be told apart
  on the final picture)*: render the title alone and the subtitle alone, and take the GRAY label pixels of a
  gauge-alone render; no such pixel satisfies the ring's own distance and angle test (radius 101..113, sweep
  225° to 135° clockwise).
- Stack: every widget frame and bound helper re-measured on env:steami (`-fstack-usage`), at most 64 B
  (5.7); the PR lists them next to 4b's.
- `scripts/mockup-review.py`: the 240 SSIM of every mockup is expected to rise; the PR gives before/after.
- Footprint gate *(designer's call, answering the fidelity check)*: `SKETCHES` in `scripts/check-footprint.py`
  gains `steami_board/ScreenGauge` (title, value, unit, subtitle and gauge labels: every face is linked),
  since `Drawing` uses only `SteamiDisplay` and its build drops the tables. The PR reports ScreenGauge's flash
  before (on `main`) and after; expected about 6 KB more. WeatherStation still joins in 4c.
- Generator and package tests of 5.6.

## 8. MicroPython follow-up (separate issue, micropython-steami-lib)

An issue in `steamicc/micropython-steami-lib` (filed by the controller when this delivery's PR opens; not
part of its "done", spec B4): commit `steami_screen/spleen.py` produced by this repository's generator
(`--micropython`, contract of 5.6), and on the 240 px backend draw the faces of 5.1 with the rules of 5.2 to
5.4. The issue says that a `FrameBuffer` blit works only for native-size faces (with a palette on RGB565),
and that replicated faces and the bold strike need a row drawer.

**Parity** means *(designer's call)*: on 240, for the same widget calls, the text uses the same glyphs (same
tables), the same faces, cells and positions as the Arduino goldens. Pixel parity stays a non-goal (spec
L58): it would also need MicroPython to copy the integer arc scanline, the triangle edges, Q14 trigonometry and
`isqrt`. MicroPython on 128 keeps framebuf's 8x8. The issue completes the GC9A01 issue of spec section 12
(#368) and replaces its "k = 2, a real `draw_scaled_text`" for text.

## 9. Edits to the spec and the README made by the delivery

Line numbers are those of the spec on branch `feat/steami-display-4b` at fc9a7e9; the builder rebases them.

Spec:
- L3-4 (status): add "Amended on 2026-11-26 by `2026-11-26-steami-display-v1c-font-design.md` (V1c fonts)".
- L40-44 (section 1): the 240 px panel's widgets use Spleen bitmap fonts; 128 and the primitives keep 8x8
  and 5x7.
- L58 (non-goals): unchanged in substance; add "on 240 the text of widgets uses the same glyph tables, faces
  and cells as MicroPython (decision 7)".
- L59 (non-goals): drop "a big-digits font"; add "Spleen on 128, in primitives, or above 16x32".
- L77-78 (anti-goals): the Spleen faces are internal, like 5x7 and k.
- L89-90 (constraints, round screens): "173 to 181 px at rows 37 to 41 on 240" becomes the chord at row 37
  (titles stay there on 240).
- L91-92 (constraints, Font): add Spleen 2.1.0, BSD-2, generated tables.
- L108-109 (approaches): "Text on 240 px" and "Fonts" rows point to section 4 here.
- L127 (5.1 files): add `SteamiFontSpleen.cpp` (generated) and the extended decoder in `SteamiFont.h`.
- L171-185 (5.3): add the Spleen faces, the 240 ladders (5.1), drawn accents and code space (5.4), the
  generalised narrow window, bold (5.2). L174-175: "It is the only font the student sees" becomes "It is the
  font of the primitives on both panels and of the widgets on 128; on 240 the widgets draw Spleen, so a
  sketch that mixes `text(s, x, y)` and widgets shows both".
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
- L524-534 (5.9 rows): title (row 37 on 240, cap 14), subtitle (one-line rows 202, blocks 177/197), value
  (label face height, the W/E and truncation figures regenerated, the NE example of section 6 replacing "x
  180, row 22" and "cut to TE at x 110" in L526), gauge and graph labels 6x12, menu 6 rows on 240,
  compass/watch letters 8x16.
- L568 (5.12 flash): add Spleen tables 5 264 B; code 14 to 18 KB.
- L796-799 (known differences, fonts): text on 240 is Spleen 8x16; values 16x32 bold, shorter than the
  mockups.
- L800-803 (known differences, labels): 6x12 on 240, close to the mockups' size; the 5x7 figures stay for 128.
- L804-807 (known differences, value block): the 240 figures recomputed from the goldens (04: "342" inks rows
  97..116 against about 101..131 in the mockup at 240; the unit inks 128..134 against about 142..156).
- L812-815 (known differences, 06): "5 rows instead of 6" on 128 only; 6 rows on 240.
- The program 04 entry that 4b adds if the maintainer accepts the overlap ("on 240 and 128") becomes "on 128
  (V1a/V1b)".
- L831, L849-892, L896, L905-911 (section 7): font cases and 240 numbers as section 7 here; L873 "cut to TE at
  x 110" becomes "TEMP whole at x 159"; L896 "5 visible rows on both panels" becomes "5 on 128, 6 on 240".
- L949-955 (section 8): add the row of section 10 here between 4b and 4c.
- L967-968 (decision 4): "no big-digits font in v1" is revisited by decision 7.
- L969-970 (decision 5): "k = 1 or 2 for glyphs" stays for strokes and the 128 glyphs; on 240 glyphs follow
  decision 7.
- After L978 (section 9): decision 7, maintainer 2026-11-26: V1c widgets use Spleen (option 2), same
  tables in MicroPython; and the designer's calls F1 to F12 of section 11 here.
- L990-991 (B1): graph labels 5x7 on 128, 6x12 on 240.
- L995-998 (B3): "Temperature" at row 37 on V1c; 5 visible rows on 128, 6 on 240.
- L1006-1010 (B6): window `[w/4, 3w/4)` with a `w/2` advance (columns 2 to 5 and 4·f for 8x8); "×3 on 240"
  becomes "f 4 on 240".
- L1016-1021 (B8): cap 14 on 240; title band row 37 on 240.
- L1058 (section 11, item 8): `menu()` shows 5 rows on 128, 6 on 240.
- L1083-1088 (section 12): the MicroPython font issue of section 8 here; L1085 "5 visible rows" becomes
  "5 on 128, 6 on 240".
- L1104 (section 13): add section 12 here.

`lib/steami_board/README.md`:
- L430 (`text()`): accents are folded by the primitives; widgets on 240 draw them.
- L508 (`maxChars()`): "15 on 240" becomes 30.
- L534-536: "glyphs are twice as large on 240" becomes "widgets on 240 use thinner fonts drawn for that
  panel (8 px per character for text)".
- The 240 known differences of 4b's gauge labels (min label touching the ring, slid labels overlapping) are
  removed wherever 4b's PR placed them (README or PR text); the 128 ones stay.
- New "Third-party fonts" section (5.6).

## 10. Delivery and "done when"

Order *(controller's choice)*: **4b** (PR #277, merged with program 04's overlap accepted provisionally) →
**4b-font** (this amendment) → **4c** (graph, menu, `SteamiHistory`, WeatherStation, built on the final 240
metrics so that their positions are computed once).

| # | Delivery | Content | Done when |
|---|---|---|---|
| 4b-font | V1c fonts | Generator and its tests, `SteamiFontSpleen.cpp`, extended decoder, faces in the kernel (`cellWidth`, `inkRows`, `drawGlyphs`, `glyphsAt`, label height, cap, fit floor, bold), `maxChars()`, README edits and "Third-party fonts", spec edits of section 9. Outside `lib/steami_board`: `scripts/gen-spleen-fonts.py`, `scripts/tests/` (generator and package tests), `scripts/arduino-package/README.md`, `scripts/check-footprint.py` (`SKETCHES`), `.gitignore` (font cache), the two screen suites and `golden_layout.py`, `.github/workflows/` (regeneration check) | 128 goldens and PPM hashes unchanged; 240 goldens regenerated and passing, natively and under ASan/UBSan; widget frames and bound helpers ≤ 64 B; footprint gate passes with ScreenGauge's delta reported; the package README carries both font notices (test); examples 01 to 04 and 07 to 09 reviewed on V1c under the webcam, 04 with no text on the ring; SSIM before/after in the PR. |

## 11. Decisions

Maintainer (2026-11-26): option 2, bitmap fonts drawn for 240 px on the V1c, internal; the Spleen 8x16 /
16x32 look of the spike; the same fonts in MicroPython.

Controller's choices: only `SteamiScreen` widgets use the V1c fonts (primitives unchanged); V1a/V1b unchanged;
values bold by a 1-px double strike; one generator for both languages; order 4b, 4b-font, 4c.

Designer's calls (reversible), numbered for the spec:
- **F1** Faces and ladders of 5.1 (TEXT 8x16 × s, LABEL 6x12, VALUE 8x16/12x24/16x32 with replication above
  f 4, odd f skipped); no 5x8, no 32x64.
- **F2** Value fit down to k, not 1, at every anchor (corners included).
- **F3** 12x24 and 16x32 carry the 15 value glyphs only; other value text uses the TEXT ladder.
- **F4** Cell height 8f on both panels, advance from the face; narrow window `[w/4, 3w/4)`, advance `w/2`.
- **F5** Bold on every 12x24 and 16x32 face, replicated or not, inside the cell; never on 8x16 faces; advance
  unchanged.
- **F6** Accents drawn by widgets on 240, folded on 128 and by primitives; extended code space 32..159.
- **F7** Cap from 13 TEXT cells (titles fixed at 37 on 240); label height from its face; `maxChars()` 30 on
  240; menu 6 rows on 240; letters cell-centred.
- **F8** Row-aligned MSB-left tables in a generated `SteamiFontSpleen.cpp`; tarball fetched by sha256,
  outputs committed, CI regeneration check.
- **F9** Licence notices (MIT 8x8 and BSD-2 Spleen) in the shipped package README
  (`scripts/arduino-package/README.md`), tested; also in the generated file and `lib/steami_board/README.md`.
- **F10** The `spleen.py` contract of 5.6, frozen and tested here; the generator lives here; `spleen.py` is
  committed only by the MicroPython PR.
- **F11** Program 04's goal is on 240; the 128 overlap stays a known difference; 4b's 240 gauge-label known
  differences are removed after the regenerated goldens confirm them clear.
- **F12** Stack placement of 5.7 (face id in `Run`'s padding, no new local in a 64 B frame); ScreenGauge joins
  the footprint gate.

## 12. Left to the builder

- Names and layout of the face type (`SteamiFontFace` or similar; not `SteamiFace`, taken by the smileys).
- Whether the faces switch on `k == 2` or on width 240 (the tests use 128, 240 and 2x1 panels only).
- The row drawer (runs per font row, `fillRect(run·n, n)`), and whether 8x8 goes through it or keeps
  `glyph()`, as long as 128 stays bit-identical.
- The generator's command line, cache directory, Makefile target and CI wiring; the exact tarball URL
  (checked against the sha256 above).
- How the 128 PPM hashes are recorded and checked.
- Every regenerated 240 number of the spec, from `golden_layout.py`.
- The release-please commit type for the change of `maxChars()` on 240.
- Plain values instead of bold, if the maintainer answers so (question 2): drop the strike, nothing else moves.

## 13. Questions for the maintainer

1. **Two fonts on the V1c: is that acceptable?** With the controller's choice (primitives unchanged),
   `screen.text("Hi", x, y)`, `textWidth()` and the `Drawing` example draw the 8x8 at 8 px tall on the V1c,
   while titles, values and `screen.text(s, at)` draw Spleen 8x16. A sketch that mixes them shows both, and
   the spec's "only font the student sees" no longer holds on 240.
   - (a) Accept: primitives stay pixel-exact 8x8 (`CHAR_W = CHAR_H = 8`, as MicroPython's `framebuf.text`),
     widgets use Spleen. **Designer's recommendation.** Primitives are the pixel-coordinate layer, the
     Drawing example and its 240 rendering stay as published, and MicroPython's primitives behave alike.
   - (b) Primitives also draw Spleen 8x16 on 240 at scale 1: one look on the V1c, but `CHAR_W`/`CHAR_H` and
     `textWidth()` change on 240, the Drawing example and its tests are redone, and MicroPython's
     `framebuf.text` would differ from Arduino's `text()`.
2. **Values in bold or plain?** The spike showed both; the maintainer approved the look without choosing.
   - (a) Bold by a 1-px double strike on 12x24 and 16x32 (this document). **Designer's recommendation:**
     closer to the mockups' heavy digits, no change to widths or layout, a few lines of code.
   - (b) Plain: thinner, a little further from the mockups; removes the strike and its tests.
   - (c) One size up is not available in Spleen (no 24x48); 16x32 × 2 (64 px cells) or 32x64 (+32 KB) are too
     big for the 240 px disc, so not offered.
