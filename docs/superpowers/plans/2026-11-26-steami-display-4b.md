# STeaMi display, delivery 4b (round widgets) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the round widgets of `SteamiScreen`: `face()` with `SteamiFace` and its 8x8 bitmaps, `watch()`, `compass()` (a real rose turned to north), `gauge()` (arc and min/max labels only). Also add the examples of mockups 04, 07, 08 (six faces and the reactive smiley) and 09, rendered natively at 128 and 240 px, scored by the review script and checked on a V1c, with the compass needle checked against a known north.

**Architecture:**
- The four widgets are public members of `SteamiScreen`. Each computes in `int32_t` from `width()` and `radius()` and draws through the inherited primitives only: `arc()`, `triangle(…, true)`, `circle(…, true)`, `rect(…, true)`, the protected `strokeLine()` and `strokeCircle()`, `SteamiDisplay::text()`, and the 4a kernel (`Run`, `fitGlyphs`, `drawGlyphs`).
- Angles are integer degrees, 0 = noon, clockwise (spec 5.6). Two new protected kernel members turn a distance and an angle into a point with the Q14 table. Three private helpers draw the elements the widgets repeat: a ray (ticks, second hand), a pointer (needle halves, hour and minute hands) and a short text centred on a point (rose letters, watch numbers). Three more private helpers serve the gauge: one draws a ring arc (track, fill), two format and draw one label. They keep each frame within the 64 B budget.
- `golden_layout.py` grows a Q14 section parsed from `SteamiTrig.cpp`, one section per widget, and a mockup section. It asserts the spec's numbers and prints what the suites pin.
- The mockups suite and `SteamiMockups.h` grow by four helpers and ten images. The examples guard learns `SteamiFace::` tokens, and `mockup-review.py` lists the ten new images.

**Tech Stack:** C++17 (`-std=c++17` native, gnu++17 on target), PlatformIO `env:native`, `env:native_asan`, `env:steami` (STM32duino 3.1.0), Unity, Python 3 (stdlib golden script, `unittest`; Pillow and NumPy for the review script), arduino-cli in CI.

**Spec:** docs/superpowers/specs/2026-11-25-steami-display-design.md

The plan covers row 4b of section 8 and what binds the round widgets in sections 5.1, 5.2, 5.6, 5.7, 5.8, 5.9, 5.10, 5.11, 5.12 and 5.13. It also covers section 6 (programs 04, 07, 08, 09), section 7 (suites 3 and 4 as far as 4b goes, build CI, hardware), section 9 (decisions 2, 3, 6 and the designer's calls), and sections 10 to 13.

Branch `feat/steami-display-4b`, from `origin/main` at b960407 (contains 3a #258, 3b #270, 4a #274). Issue #275; the PR says "Closes #275".

## Global Constraints

Copied from the design document (verbatim):

- "No heap allocation in the layer itself (drivers keep their own buffers), no `String`, no RTTI, no exceptions."
- "Never use `double` arithmetic or `sin()`/`cos()` in drawing code (single-precision FPU; integer tables)."
- "No text drawn outside the visible circle by a widget: it is truncated or scaled down."
- "No student-visible second font, scale factor or unit system: the 5x7 font, `k` and 128-px units are internal."
- "`float` is accepted at widget inputs (values, ratios) on the single-precision FPU; literals carry the `f` suffix." "Do not rely on `printf("%f")` [...] nor on `dtostrf`."
- "Packaging: flattened `src/`, generated umbrella `STeaMi.h`, unique file names (case-insensitive), no `Steami.h`; no `STEAMI_BOARD_*` in any library `.cpp`; headers carry no macro-dependent inline code."
- "A point at angle a and distance d from the centre is `(cx + rnd(d·sin a), cy − rnd(d·cos a))`, where `rnd` rounds Q14 to nearest, halves away from zero, so shapes stay symmetric. Float angles from widgets are rounded to the nearest degree first."
- Rule 1: "Every widget constant (margins, offsets, bar size, graph box, menu rows, radius offsets) [...] is in 128-px units and goes through `u()`." Rule 2: "a widget line, circle, tick or curve is `k` px wide." Rule 5: "Round widgets scale from `radius()`: `radius() − u(c)` and fractions of it." Rule 7: "Nothing a widget draws leaves the circle of radius `radius() + 1`."
- "NAN [...] never crashes a widget: [...] `bar()` and `gauge()` stay empty, [...] `compass()` draws no needle. `int` -1 from `SteamiDistance` simply clamps to the minimum." "Every widget accepts empty or `nullptr` strings (draws nothing for that part)." "Widgets use at most 64 B of stack (formatting buffers of 16 B, triangle vertices)."
- "On a grey panel (`!isColor()`), a widget foreground drawn over the widget's own background (bar fill over its track, gauge fill over its track, ...) is lifted to at least **background + 4 grey levels** [...]: BLUE (6) over the DARK gauge track becomes 10, RED (8) becomes 10 [...] No effect on the TFT."
- Default colours: "min/max GRAY, [...] gauge track DARK, fills LIGHT."
- Decision 6: "**The V1c comes first.** [...] when a rule cannot serve both panels equally, the V1c wins, and a V1a/V1b defect does not block a delivery. V1a/V1b are not checked on hardware." In this plan the V1a/V1b paths (128 px, grey) are covered by native tests only, and a V1a/V1b-only defect never blocks a task or the PR.
- Scope (row 4b, B4): `lib/steami_board` (code, examples, README, `keywords.txt`); `tests/native` (the two screen suites, `golden_layout.py`, `tests/native/helpers/SteamiMockups.h`); `scripts/` (`mockup-review.py` and the tooling tests, which the 4a pattern ties to the suites). No change to `platformio.ini` or `.github/workflows/`: the `native_asan` filter already covers `native/test_steami_screen*`, and the review job's sparse checkout already fetches every ref. Never `lib/ssd1327`, `lib/gc9a01`, `requirements.txt`, the Makefile, MicroPython, the tutorials or the wiki. The design doc changes only through the commits this plan names: the plan itself (Before Task 1) and, if the maintainer accepts it, the one `docs(docs)` known-difference entry of the decision "Program 04's text over the ring" (4a Ruling 1 precedent).
  - The PR names the paths outside row 4b's list (`tests/native/helpers/SteamiMockups.h`, `scripts/mockup-review.py`, `scripts/tests/test_mockup_review.py`, `scripts/tests/test_steami_display_rules.py`) and why: spec 7's suite 4 "grows with 4b and 4c", and the guard and the review list follow the suites (B4: "each delivery names its paths").

Carried from 4a (plan Global Constraints and the ledger's rulings):

- The worktree is set up (husky hooks, `node_modules`). `.venv` is a symlink to another checkout's venv: run its tools, never `make setup`, never `pip install`.
- Commits: `type(scope): Sentence starting with a capital and ending with a period.`, header at most 72 characters.
  - Scopes from `commitlint.config.js`: `board`, `tests`, `tooling`, `ci`, `docs` (**not** `display`, `screen` or `steami_board`).
  - No `!`, no `BREAKING CHANGE`, never `[skip ci]`, no attribution lines.
- Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`. Google style, indent 4, 100 columns.
- clang-tidy uses `WarningsAsErrors: '*'`:
  - use `lroundf`, never `(int)(x + 0.5f)`;
  - use `fabsf`, `fmodf`, `isnan`, `isinf` from `<math.h>`;
  - cast one operand to `int32_t` before multiplying two `int16_t`;
  - no `uint8_t` loop variable against a wider bound;
  - no unqualified `min`/`max`/`abs`: use the anonymous-namespace `smaller`/`larger` of `SteamiScreen.cpp`;
  - no namespace-scope object with a constructor in a library `.cpp`.
- Tests build screens on the file-local `board`, never on `Steami`, and register every new test with `RUN_TEST` in `main()`. "A == B" between two drawings means: render each alone on a fresh, cleared panel of the same size and mode with `rendered()`, then compare.
- **Stack (4a Ruling 3, binding):** the own `-fstack-usage` frame of every public widget is at most 64 B, and so is the frame of every private helper that holds a widget's 16 B buffer, `Run` or triangle vertices (spec 5.11 counts them in the budget; see "Helpers and the stack" below). A frame over 64 B goes back to its task to shrink its locals. If that is impossible without breaking another spec rule, stop and report to the user as a spec deviation; never exempt a helper. Callee frames are not summed. Do not change `Run`, `Region` or any 4a kernel signature: `subtitle` and `value` sit at exactly 64 B.
  - Measure with `SU="$SCRATCH/su"; rm -rf "$SU"; PLATFORMIO_BUILD_DIR="$SU" PLATFORMIO_BUILD_FLAGS=-fstack-usage PLATFORMIO_SRC_DIR=lib/steami_board/examples/ScreenTemperature .venv/bin/pio run -e steami`, then `grep -h 'SteamiScreen::' $(find "$SU" -name 'SteamiScreen.cpp.su')`. `$SCRATCH` is your scratchpad directory; the repository tree is not touched.
- Never flash, never open a serial port, never touch `/dev/ttyACM*` or `/run/media`, never run `make test-hardware`. Only Task 7 uses the board, and only the controller runs it.
- Read-only references: device.py and const.py are read with `git -C /home/nedjar/sandbox/micropython-steami-lib show 927e96b:lib/steami_screen/steami_screen/<file>`, and the refs from `/home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups` (HEAD 83d8e2c = `TUTORIALS_COMMIT`). Never check out or pull either clone.

## The 4a kernel as it exists (consumed by every task)

`lib/steami_board/src/SteamiScreen.h`. Protected:
- `struct Region { int32_t left; int32_t right; int32_t width() const; };` (inclusive, empty when `right < left`); `struct Point { int32_t x; int32_t y; };`
- `enum class Ink : uint8_t { TEXT, LABEL, VALUE };`
- `struct Run { const char* s; size_t glyphs; int32_t y; Ink ink; uint8_t f; uint8_t shrink = 0; };`
- `int32_t u(int32_t v128);` `uint8_t k();` `Region chord(int32_t top, int32_t bottom);`
- `static uint8_t inkRows(const char* s, size_t glyphs, Ink ink);` `Region inkChord(const Run& t);`
- `static int32_t cellWidth(const char* s, size_t glyphs, Ink ink, uint8_t f);` `static int32_t valueWidth(const char* s, uint8_t f);`
- `size_t fitGlyphs(const Run& t, Region* room = nullptr, int32_t pad = 0);`: on return `*room` holds the kept prefix's ink chord, shrunk by `t.shrink` and intersected with the room it was given.
- `int32_t marginNs(int32_t tw, uint8_t f);` `int32_t marginEw(uint8_t f);` `int32_t resolveX(SteamiAt, int32_t tw, uint8_t f);` `int32_t resolveY(SteamiAt, int32_t tw, uint8_t f);` `Point resolve(...)`. Widgets avoid `resolve()`: a returned `Point` goes through the stack.
- `void drawGlyphs(const Run& t, int32_t x, SteamiRgb c);` `int32_t blockHeight(uint8_t f, bool hasUnit);` `Region valueRegion(const Run&, SteamiAt);` `void drawValueParts(Run& value, SteamiRgb c, const char* unit, const char* label, SteamiAt at);`
- `SteamiRgb lifted(SteamiRgb fg, SteamiRgb bg);` `static size_t glyphCount(const char* s);`

Private: `bool widgetCanDraw();` (auto-begin, then `isAvailable() && width() > 0`), `rowsChord`, `linesFit`, `placeValue`, `fitValue`, `drawValuePart`, `barRows`, `uint8_t _decimals = 1;`. Anonymous namespace of the `.cpp`: `smaller`, `larger`, `rootOf`, `isNarrow`, `isSide`, `isCorner`, `glyphFactor(uint8_t scale, uint8_t k)`, `filledColumns(int32_t w, float v, float maxV)`.

Inherited:
- public `arc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t sweepDeg, SteamiRgb c, uint8_t width = 1)`: `r` is the outer radius, the ring is `[max(r − width + 1, 0) − ½, r + ½]`, and sweep ≤ 0 draws nothing;
- public `triangle(x0, y0, x1, y1, x2, y2, SteamiRgb c, bool fill)`, `circle(cx, cy, r, SteamiRgb c, bool fill)`, `rect(x, y, w, h, SteamiRgb c, bool fill)` and `text(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale)`;
- protected `strokeLine(x1, y1, x2, y2, uint8_t w, SteamiRgb c)` (w×w brush, top-left at `p − (w−1)/2`) and `strokeCircle(cx, cy, r, uint8_t w, SteamiRgb c)` (= `arc(…, 0, 360, c, w)`).

`SteamiTrig.h`: `int16_t steamiSinQ14(int32_t deg)`, `int16_t steamiCosQ14(int32_t deg)` (any degree, normalised), `int32_t steamiRoundQ14(int32_t v)`.

Test fixture (`tests/native/test_steami_screen/test_main.cpp`):
- `class TestScreen` with `using` lines for the kernel; `onPanel(size, color, check)`, which asserts `outOfBounds() == 0` and `outsideCircle() == 0` after the check; `forEachPanel(check)`: 128 grey, 128 colour, 240 colour, 240 grey; `rendered(size, color, draw)`;
- `SteamiScreenCheck.h`: `inkBox(mem, c, rowFrom, rowTo)`, `inkBoxAll`, `expectBox`. `inkBox` matches `mem.stored(c)`, so on the grey panel BLUE and DARK (both level 6) are one colour.

## Decisions taken while planning (the design doc leaves them open)

- **Rounding (spec 13).** Points use the 5.6 rule (`steamiRoundQ14`), never device.py's `int()`. Fractions of a radius truncate, as device.py and `u()` do: `R·7/10`, `R·85/100`, `R/2`, `3R/4`. The face cells are `u(11)` and `u(8)`, which equal `W·11/128` and `W/16` exactly. Where the two roundings differ, the goldens follow this plan, and `golden_layout.py` says so in a comment. Example: the min label point at 233° is x 30 here, 29 in device.py.
- **New protected kernel** (`SteamiScreen.h`; tests reach it through `TestScreen`):
  - `int32_t polarX(int32_t d, int32_t deg);` = `centerX() + steamiRoundQ14(d · steamiSinQ14(deg))`
  - `int32_t polarY(int32_t d, int32_t deg);` = `centerY() − steamiRoundQ14(d · steamiCosQ14(deg))`
  - Two scalars, not a `Point`, for the same stack reason as `resolveX`/`resolveY`. `|d| ≤ 32767` keeps the product in `int32_t`.
- **Private helpers** (`SteamiScreen.h`, private):
  - `void ray(int32_t deg, int32_t from, int32_t to, SteamiRgb c);`: `strokeLine` of width `k()` from distance `from` to `to` on `deg`.
  - `void pointer(int32_t deg, int32_t length, int32_t halfWidth, SteamiRgb c);`: a filled triangle with its tip at `(length, deg)` and its base from `(halfWidth, deg + 90)` to `(halfWidth, deg − 90)`, all through `polarX`/`polarY`. The base ends are mirror images about the centre, because `rnd` is odd-symmetric.
  - `void glyphsAt(const char* s, int32_t deg, int32_t d, SteamiRgb c);`: the 8x8 ×k cell of `s` centred on `(d, deg)`. Its top-left is `(polarX − cellWidth(s, n, TEXT, k)/2, polarY − 4·k)`, as device.py's `lx − tw//2, ly − CHAR_H//2`. It draws with `SteamiDisplay::text(s, x, y, c, k())`, without a cut: Tasks 2 and 3 prove every letter and number inside the circle at every angle.
  - `void gaugeArc(int32_t outer, int32_t sweep, SteamiRgb c, uint8_t w);` = `arc(centerX(), centerY(), outer, STEAMI_GAUGE_START, sweep, c, w)`: Task 4.
  - `void gaugeLabel(float v, int16_t deg, int32_t d);` and `void drawGaugeLabel(Run& t, int32_t px);`: Task 4.
- **Helpers and the stack.** Ruling 3 says "moving the 16 B buffer or the work into a helper to dodge the figure does not count", and spec 5.11 counts "formatting buffers of 16 B, triangle vertices" in the 64 B. So every helper that holds one of them is bound to 64 B like a widget; the others are measured and recorded. A helper here is a unit of the drawing that its widget calls more than once:
  - `pointer` (holds the triangle vertices, **bound**): twice in `compass` and in `watch`;
  - `ray`: 8 ticks; 12 ticks plus the second hand;
  - `glyphsAt`: four letters; four numbers;
  - `gaugeArc`: the track and the fill. Two direct `arc()` calls (4 stack arguments and a spilled `SteamiRgb` each, three live floats) put `gauge()` at 80 B in a prototype; with `gaugeArc` it measured 56 B (`gaugeArc` 40). Computing the sweep before `widgetCanDraw()`, `long` labels or a two-pass loop each left it at 72 B;
  - `gaugeLabel` (holds the 16 B buffer and the `Run`, **bound**): the min and the max label. Shaped as 4a's `value()`/`drawValueParts()`: `gaugeLabel` formats into its buffer and fills the `Run`, then `drawGaugeLabel(t, polarX(d, deg))` (**bound**) owns the `Region`, the fit, the clamp and `drawGlyphs`. A prototype that did all of it in one frame measured 72 B. `gauge()` holds no text.

  Shapes for the widgets themselves:
  - `face(bitmap, …)` is one flat loop `for (int32_t i = 0; i < 64; i++)`: row `i / 8`, column `i % 8`, bit `0x80 >> (i % 8)`. A nested row/column loop around `rect()` (3 stack arguments) measured 72 B; the flat loop 64 B.
  - `compass()` measured exactly 64 B with `roundedHeading`: it gains no live local beyond the decision "compass(headingDeg, c)".
  - `watch()` measured 48 B.

  Pre-measure each widget and helper with the Global Constraints command at the end of its task (Tasks 1 to 4, Step 4), not only in Task 6. Recorded helper frames go in the PR next to 4a's (`drawGlyphs` 88, `fitGlyphs` 64): `polarX`, `polarY`, `ray`, `glyphsAt`, `gaugeArc` (recorded); `pointer`, `gaugeLabel`, `drawGaugeLabel` (bound, ≤ 64).
- **Strokes:**
  - The rose and dial circles are `strokeCircle(cx, cy, R, k(), STEAMI_DARK)`, a ring of outer radius R and width k (5.6: "strokeCircle = arc(…, 0, 360, c, w)"). They are not `circle()`'s midpoint outline: that one has no width for k = 2.
  - Ticks and the second hand are `ray`s.
  - The pivot is `circle(cx, cy, u(3), STEAMI_GRAY, true)`.
- **Draw orders:**
  - compass: circles, ticks, letters, north half, south half, pivot (the spec's listing order, so letters paint over ticks at diagonal headings);
  - watch: dial, ticks, numbers, hour hand, minute hand, second hand, pivot (device.py);
  - gauge: track, fill, min label, max label (device.py).
- **compass(headingDeg, c):**
  - `R = radius() − u(12)`. Inner circle `R·7/10`. Ticks `i·45` from `R − u(6)` to R, LIGHT when i is even, DARK otherwise. Letters N (WHITE), E, S, W (GRAY) at `R + u(5)`. Needle `R·85/100`, half width `u(3)`: north `pointer(−h, len, hw, c)`, south `pointer(180 − h, len, hw, STEAMI_DARK)`. Pivot `u(3)` GRAY.
  - Every element at angle a is drawn at `a − h`. For a finite heading, `h = lroundf(fmodf(headingDeg, 360.0f))` (fmodf first, so `lroundf` never sees a huge value). A NAN or ±INF heading draws the rose at 0 and the pivot, with no needle. The spec names NAN only; INF is treated like it, because no reading points anywhere.
- **watch(h, m, s, c):**
  - `R = radius() − u(8)`. Dial `strokeCircle` DARK. Tick i (0..11) at `30·i` from `R − u(8)`, LIGHT when `i % 3 == 0`; otherwise from `R − u(5)`, GRAY. Numbers "12", "3", "6", "9" at 0, 90, 180, 270, distance `R − u(15)`, WHITE.
  - Hour `pointer((h % 12)·30 + m/2, R/2, u(3), c)`; minute `pointer(m·6 + s/10, 3R/4, u(2), c)`; second `ray(s·6, 0, R·85/100, STEAMI_GRAY)`; pivot.
  - Integer division, no clamping: an out-of-range h, m or s still gives an integer angle, which the trig normalises.
- **gauge(v, minV, maxV, c, arcWidth):**
  - Outer radius `radius() − u(4)`. Thickness `arcWidth`, or `max(5, radius()/9)` when it is 0: physical, 7 on 128 and 13 on 240.
  - Track `gaugeArc(outer, 270, STEAMI_DARK, w)`. Fill `gaugeArc(outer, sweep, lifted(c, STEAMI_DARK), w)` (both start at 225) with `sweep = lroundf(270.0f · ratio)`. `ratio = (v − minV)/(maxV − minV)` clamped to [0, 1]; it is 0 when `maxV == minV` or when the division gives NAN (NAN in any argument, INF − INF).
  - Labels `gaugeLabel(minV, 233, d)` then `gaugeLabel(maxV, 127, d)` with `d = max(0, outer − w − u(10))`: 43 on 128 and 82 on 240.
- **gaugeLabel(v, deg, d)**, the label rule the spec leaves open (C5):
  - The text is v's integer part, truncated toward zero as device.py's `int()`: `SteamiValue(static_cast<long>(v)).format(text, 16, 0)` when `fabsf(v) < 1e9f`, otherwise "--" (NAN, ±INF, huge). This avoids a float-to-long cast that would be undefined, and −0.5 shows "0".
  - `Run t{text, n, polarY(d, deg), Ink::LABEL, k(), k()}` (5x7 ×k, its top row on the point, as device.py). It is centred on `polarX(d, deg)` by its cell width `6·n·k` (x = point − w/2).
  - Then, in `drawGaugeLabel(t, px)`, the 4a corner rule (B10): `fitGlyphs(t, &room)` with the whole panel as room keeps the longest prefix that fits the chord at its inked rows, shrunk by k. x is clamped into `[room.left, room.right − w + 1]`. It is drawn with `drawGlyphs(t, x, STEAMI_GRAY)`.
  - Labels of the examples ("0", "100", "500", "1050") never move. A long label such as "-100000000" slides inward and stays whole: a label has at most 10 glyphs (60 px on 128, 120 px on 240), and the chord at the label rows leaves about 109 / 201 px at every reachable d (d only shrinks as `arcWidth` grows). The cut in `fitGlyphs` is a guard no label reaches; if it ever did, rule 7 would win over "never a cut number", which 4a applied to `format()`, not to the display.
  - Accepted: on 128 the two slid labels of (−1e8, 1e8) overlap at about x 65..69 on rows 90..96 (min label cell at 10..69, max label cell at 65..118). The golden prints the box; no example comes near it.
  - Found in the final review, for the PR's known differences: on 240 the same two labels also overlap, at x 113..139 on rows 169..182 (min label cell at 20..139, max label cell at 113..220; golden box (20, 169, 218, 182), 1254 GRAY pixels). Both labels stay inside the circle, and no example draws such labels.
  - Accepted, for the PR's known differences: on 240 a min label of 3 or more digits whose first glyph inks its bottom-left (such as "100" as a minimum) touches the ring by a few pixels (4 GRAY pixels at (39..40, 181..182) for "100"). The examples' labels (0 as min; 100, 500, 1050 as max) are clear.
- **face:**
  - `enum class SteamiFace : uint8_t { HAPPY, SAD, SURPRISED, SLEEPING, ANGRY, LOVE };` in `SteamiScreen.h` (5.1, 5.7).
  - The bitmaps are `inline constexpr uint8_t STEAMI_FACES[6][8]` in `SteamiScreenConst.h`: one 48 B copy (C++17 `inline` variable; no macro). Rows top to bottom, MSB = left column, const.py order.
  - `face(f, compact, c)` draws `STEAMI_FACES[f]`. A value of f ≥ 6 (a cast) draws nothing, as device.py's `FACES.get`.
  - `face(f, c)` = `face(f, false, c)`.
  - `face(bitmap, compact, c)`: nothing for `nullptr`. Otherwise the cell is `u(11)`, or `u(8)` when compact; `ox = centerX() − 4·cell`, `oy = centerY() − 4·cell − (compact ? cell/2 : 0)`; one flat loop `i = 0..63` (row `i / 8`, col `i % 8`, bit `0x80 >> (i % 8)`; decision "Helpers and the stack") draws one `rect(ox + col·cell, oy + row·cell, cell, cell, c, true)` per set bit.
  - No `face(bitmap, SteamiRgb)` shortcut: the spec has none.
- **Entry.** Every widget starts with `if (!widgetCanDraw()) return;`, before any other work. `face(bitmap)` tests `nullptr` after it, so a no-screen call still reports E30.
- **Constants** (`SteamiScreenConst.h`, 128-px units unless stated):
  - `STEAMI_PIVOT = 3` (pivot radius and the needle and hour half width);
  - `STEAMI_FACE_CELL = 11`, `STEAMI_FACE_COMPACT_CELL = 8`;
  - `STEAMI_WATCH_INSET = 8`, `STEAMI_WATCH_MAJOR_TICK = 8`, `STEAMI_WATCH_MINOR_TICK = 5`, `STEAMI_WATCH_NUMBERS = 15`, `STEAMI_MINUTE_HALF_WIDTH = 2`;
  - `STEAMI_COMPASS_INSET = 12`, `STEAMI_COMPASS_TICK = 6`, `STEAMI_COMPASS_LETTERS = 5`;
  - `STEAMI_GAUGE_INSET = 4`, `STEAMI_GAUGE_LABEL_GAP = 10`;
  - in degrees, not scaled: `STEAMI_GAUGE_START = 225`, `STEAMI_GAUGE_SWEEP = 270`, `STEAMI_GAUGE_MIN_ANGLE = 233`, `STEAMI_GAUGE_MAX_ANGLE = 127`;
  - physical: `STEAMI_GAUGE_MIN_WIDTH = 5`, `STEAMI_GAUGE_WIDTH_DIVISOR = 9`.

  The fractions 7/10, 85/100, 1/2 and 3/4 stay as literals in the code, each with a comment naming the spec row.
- **Golden script** (`tests/native/test_steami_screen/golden_layout.py`):
  - It parses `kSinQ14[91]` from `lib/steami_board/src/SteamiTrig.cpp` (the numbers between `kSinQ14[91] = {` and `};`, asserting 91 of them). It mirrors `steamiSinQ14`, `steamiCosQ14`, `steamiRoundQ14` and `Panel.polar(d, deg)`.
  - Each widget gets a section in the 4a pattern, prefixed with the delivery because the file already holds 4a's `(Task 2)` to `(Task 6)` sections: `# ---- Widget of 4b Task N: <widget>()`, `# ---- Self-check of 4b Task N` (asserts of the spec's numbers), `# ---- Values pinned by test_steami_screen (4b Task N)` (prints).
  - It ties the cardinal points to device.py: at 128, device.py's `int()` points for the compass at heading 0 (circles, the four cardinal ticks, letters, needle tip and base) and for `watch(3, 0, 0)` equal `polar()`, since sin and cos are exact there (at other headings device.py turns only its needle, decision 2). The four diagonal ticks differ (Q14 against `int()`: x 97 against 96 for the inner end at 45°, 37 against 36 for the outer offset), and the script says so in a comment.
  - Its face table holds const.py's literals itself; it never parses `SteamiScreenConst.h`.
  - Task 5 adds `# ---- Values pinned by test_steami_screen_mockups (4b Task 5)`, so the mockup boxes can be regenerated (4a deferred minor).
  - It runs by hand: `python3 tests/native/test_steami_screen/golden_layout.py`, exit 0.
- **Mockup inputs:**
  - 04: 342 mm;
  - 07: heading 0;
  - 08: each `SteamiFace` alone (names happy, sad, surprised, sleeping, angry, love), and the reactive image at d = 100 mm (HAPPY, "HAPPY" in GREEN);
  - 09: `s = 10·3600 + 10·60 + 30`.

  Render names: `04_circular_gauge`, `07_compass`, `08_smiley_<name>`, `08_smiley_reactive`, `09_watch`.
- **ScreenFaces** (spec 13 leaves "the example that cycles the six faces" to the builder):
  - One sketch with `int shown = 6;`: 6 is the reactive mood (section 6's program), 0..5 show `FACES[shown]` alone, full screen. Button A does `shown = (shown + 1) % 7`.
  - `delay(50)` instead of section 6's 200, so that the buttons are read often (`SteamiButtons.h`: "Call the buttons at least every 50 ms"). This is a departure from program 08, which the guard does not see (it compares `screen.*` statements only): the PR names it (Task 7, Step 7). On V1c `show()` takes at least 58 ms (spec 5.12: 57.6 ms), so a loop lasts over 100 ms and a short tap can be missed: the header says "hold A until the face changes".
  - The helper `mockupFaces(screen, shown, d)` holds the same branches, which the guard compares statement by statement.
- **ScreenCompass** keeps section 6's program, with no calibration branch. Its header comment points to the `Compass` example (MENU calibrates) and to the board's `cm` calibration (README "### Calibration"). ScreenGauge and ScreenWatch are section 6's programs.
- **Examples** follow 4a Ruling 2: `#include <Arduino.h>`, `<SteamiScreen.h>` and the sensor headers, not `<STeaMi.h>`; `Serial.begin(115200);` then `screen.begin();` then the sensors' `begin()`.
- **Missing sensor**, as the verbatim programs give it:
  - 04 with d = −1: an empty gauge and "-1 mm";
  - 07 with NAN: the rose, no needle;
  - 08 with d = −1: SURPRISED.

  Each example's header says so; the mockups suite pins it.
- **No new header**: `STeaMi.h` is unchanged and so is `test_umbrella_includes_display_headers`.
- **Footprint gate:** `SKETCHES` is unchanged (WeatherStation joins in 4c). Task 6 reports flash figures.
- **Program 04's text over the ring: asked to the maintainer when the plan is approved (no default); it must be settled before the PR leaves draft.**
  - On the V1c, program 04's text crosses the gauge ring, which mockup 04 does not show. Subtitle "VL53L1X ToF" (spec: rises to row 188, 176 px, x 32..207) covers both bottom ends of the ring (radius 101..113 at about x 40..49 and 191..200, rows 191..200). It is DARK on a DARK track.
  - Title "Distance" (row 37) touches the top of the ring with its 'D'.
  - On 128 the overlap is a few pixels.
  - Section 6 says "first: text paints over the arc", which fixes the draw order, but its list of known differences ("The golden script and the reviewers expect them") does not name this. Under decision 6 a V1c difference from `_ref.png` that is not on that list fails the hardware review (B5).
  - If the maintainer accepts the overlap, the controller adds one commit `docs(docs): Accept program 04's text over the gauge ring ends.` that adds to section 6's known differences "04: the subtitle and the title paint over the ends of the ring, on 240 and 128", and the PR body says so (4a Ruling 1 precedent).
  - If the maintainer wants a layout change (for example a shorter subtitle), stop: a spec edit comes first, and this plan's authority ends there.
  - Tasks 1 to 6 go ahead while the question is open: the native render follows the verbatim program either way. The gate is on leaving draft or merging (Task 7, Step 2).
- **Flash estimate, for the maintainer with the question above (a finding, not a gate).** A prototype of the four widgets as decided here gave `SteamiScreen.cpp.o` 6220 B and about 12.5 KB of `.text` + `.rodata` for `SteamiScreen`, `SteamiValue`, `SteamiDisplay`, `SteamiFont` and `SteamiTrig` (10279 B at b960407), above spec 5.12's "code ≈ 8 to 12 KB" before 4c adds `graph` and `menu`. Task 6 measures the real figure.

## Review Focus

1. **A sensor is missing in examples 04, 07 and 08** (`read()` −1, `heading()` NAN). Expected: an empty gauge with "-1" and "mm" (verbatim program), the rose with no needle, the SURPRISED mood in YELLOW, nothing outside the circle, no crash. Test: `test_mockups_without_sensors` grows (Task 5).
2. **Non-finite and huge numbers**: gauge v, minV or maxV NAN or ±INF, `minV == maxV`, `minV > maxV`, labels at ±1e9 and a long label; compass ±INF, 1e30f, −90.4f, 359.6f. Expected: an empty or full fill, "--" labels, a long label slid inward and whole inside the circle (no label of at most 10 glyphs reaches the cut), no needle for a non-finite heading, a wrapped heading otherwise, no undefined float cast. Tests: `test_gauge_extremes` (Task 4), `test_compass_heading_rounding` and `test_compass_without_a_heading` (Task 3).
3. **Every angle on the V1c's even strokes** (k = 2: the brush extends right and down, so rings and ticks are not symmetric). Expected: no pixel outside radius + 1 at any heading, time or ring width. Tests: `test_compass_stays_inside`, `test_watch_stays_inside`, `test_gauge_stays_inside`, `test_faces_stay_inside` (Tasks 1 to 4).
4. **Out-of-range times** from a sketch that counts time by hand (h ≥ 24, m or s ≥ 60, up to 255). Expected: integer angles, no crash, inside the circle; `watch(25, 61, 61)` equals `watch(1, 61, 61)`. Test: `test_watch_out_of_range` (Task 2).
5. **No screen, or a failed begin**, and bad face inputs. Expected:
   - each round widget begins the display on its own and draws nothing, with one E30 line for all of them and E31 drawing nothing;
   - `face(nullptr)` and `face(static_cast<SteamiFace>(6))` draw nothing.

   Tests: `test_widgets_without_a_screen`, `test_screen_e31_message` (Task 4), `test_face_overloads_and_bad_input` (Task 1).

## File map

| File | Responsibility | Task |
|---|---|---|
| `lib/steami_board/src/SteamiScreenConst.h` | 4b constants, `STEAMI_FACES` | 1 to 4 |
| `lib/steami_board/src/SteamiScreen.h/.cpp` | `SteamiFace`, `face`, `watch`, `compass`, `gauge`, `polarX/Y`, helpers | 1 to 4 |
| `tests/native/test_steami_screen/test_main.cpp` | suite 3, 4b items | 1 to 4 |
| `tests/native/test_steami_screen/golden_layout.py` | Q14, widget and mockup goldens | 1 to 5 |
| `docs/superpowers/plans/2026-11-26-steami-display-4b.md` | this plan | Before Task 1 |
| `docs/superpowers/specs/2026-11-25-steami-display-design.md` | section 6 known difference for 04, only if the maintainer accepts it | 7 |
| `tests/native/helpers/SteamiMockups.h` | `FACES`, `mockupGauge`, `mockupCompass`, `mockupFaces`, `mockupWatch` | 5 |
| `tests/native/test_steami_screen_mockups/test_main.cpp` | suite 4: 04, 07, 08 ×7, 09 | 5 |
| `lib/steami_board/examples/Screen{Gauge,Compass,Faces,Watch}/*.ino` | the four examples | 5 |
| `scripts/tests/test_steami_display_rules.py` | `SteamiFace::` tokens, four new pairs | 5 |
| `scripts/mockup-review.py`, `scripts/tests/test_mockup_review.py` | ten new images; counts from `MOCKUPS` | 5 |
| `lib/steami_board/README.md`, `keywords.txt` | Screen section, keywords | 6 |

---

### Before Task 1: commit the plan

- [ ] **Step 0:** As 4a did, commit this plan alone once the maintainer has approved it:

```bash
git add docs/superpowers/plans/2026-11-26-steami-display-4b.md
git commit -m "docs(docs): Plan display delivery 4b, the round widgets."
```

---

### Task 1: `SteamiFace`, its bitmaps and `face()`

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `lib/steami_board/src/SteamiScreen.h`, `lib/steami_board/src/SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`

**Interfaces:**
- Consumes: the 4a kernel above (`widgetCanDraw`, `u`, `centerX`, `centerY`, `rect`), `rendered`, `forEachPanel`, `onPanel`.
- Produces:
  - `enum class SteamiFace : uint8_t { HAPPY, SAD, SURPRISED, SLEEPING, ANGRY, LOVE };` (in `SteamiScreen.h`, after `SteamiAt`);
  - `inline constexpr uint8_t STEAMI_FACES[6][8]`, `STEAMI_FACE_CELL = 11`, `STEAMI_FACE_COMPACT_CELL = 8`;
  - public `void face(SteamiFace f, bool compact = false, SteamiRgb c = STEAMI_LIGHT);`, `void face(SteamiFace f, SteamiRgb c);`, `void face(const uint8_t bitmap[8], bool compact = false, SteamiRgb c = STEAMI_LIGHT);`;
  - script: `FACES` (const.py's dict, in enum order) and `face_layout(W, compact) -> (cell, ox, oy)`.

- [ ] **Step 1: Golden section.** Add the face section (`# ---- Widget of 4b Task 1: face()` and its self-check): `FACES` copied from const.py (`git show 927e96b:…/const.py`, lines 23-30), MSB = left. Assert `face_layout(128, False) == (11, 20, 20)`, `face_layout(240, False) == (20, 40, 40)`, `face_layout(128, True) == (8, 32, 28)`, `face_layout(240, True) == (15, 60, 53)`. Run: `python3 tests/native/test_steami_screen/golden_layout.py` — Expected: exit 0.

- [ ] **Step 2: Write the failing tests**

```cpp
// The FACES of micropython-steami-lib 927e96b const.py, rows top to bottom, MSB = left column.
static const uint8_t kConstPyFaces[6][8] = {
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x3C, 0x00},   // happy
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x3C, 0x42, 0x00},   // sad
    {0x00, 0x24, 0x24, 0x00, 0x18, 0x24, 0x24, 0x18},   // surprised
    {0x00, 0x00, 0x66, 0x00, 0x00, 0x18, 0x18, 0x00},   // sleeping
    {0x00, 0x42, 0x24, 0x24, 0x00, 0x3C, 0x42, 0x00},   // angry
    {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00}};  // love
static_assert(sizeof(STEAMI_FACES) == 48, "spec 5.12: faces 48 B");

static void test_faces_equal_const_py(void) {
    // memcmp(STEAMI_FACES, kConstPyFaces, 48) == 0;
    // for each panel of forEachPanel and each f in 0..5, face(SteamiFace(f)): the LIGHT mask equals
    // the mask built from kConstPyFaces[f] with cells (11 at origin 20, 20) on 128 and (20 at
    // 40, 40) on 240; nothing else lit
}
static void test_face_compact(void) {
    // face(SteamiFace::HAPPY, true): the same mask built with cell 8 at (32, 28) on 128 and 15 at
    // (60, 53) on 240
}
static void test_face_overloads_and_bad_input(void) {   // Review Focus 5
    // face(SteamiFace::SAD, STEAMI_GREEN) == face(SteamiFace::SAD, false, STEAMI_GREEN);
    // face(STEAMI_FACES[4]) == face(SteamiFace::ANGRY);
    // face(nullptr), face(static_cast<SteamiFace>(6)), face(static_cast<SteamiFace>(255)):
    //   fillRectCalls() == 0
}
static void test_faces_stay_inside(void) {   // Review Focus 3
    // a bitmap of eight 0xFF, full and compact, on every panel of forEachPanel (onPanel asserts
    // outsideCircle() == 0); 64 fillRect calls each
}
```

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`SteamiFace` not declared).

- [ ] **Step 3: Implement** the enum, the table, the constants and the three overloads as the decision "face" says.

- [ ] **Step 4: Verify**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan`, `make tidy`, `make build` — Expected: PASS, clean, `SUCCESS`.
Run the stack measurement (Global Constraints) — Expected: each `SteamiScreen::face` frame ≤ 64 (the bitmap overload's flat loop measured 64 in a prototype, a nested loop 72).

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add face() and the six SteamiFace bitmaps."
```

---

### Task 2: Polar kernel, drawing helpers and `watch()`

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `SteamiScreen.h`, `SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp` (add `using SteamiScreen::polarX;` and `using SteamiScreen::polarY;` to `TestScreen`), `golden_layout.py`

**Interfaces:**
- Consumes: the 4a kernel (`u`, `k`, `cellWidth`, `widgetCanDraw`); `strokeLine`, `strokeCircle`, `triangle`, `circle`, `SteamiDisplay::text`; `steamiSinQ14`, `steamiCosQ14`, `steamiRoundQ14`.
- Produces:
  - protected `int32_t polarX(int32_t d, int32_t deg);`, `int32_t polarY(int32_t d, int32_t deg);`;
  - private `void ray(int32_t deg, int32_t from, int32_t to, SteamiRgb c);`, `void pointer(int32_t deg, int32_t length, int32_t halfWidth, SteamiRgb c);`, `void glyphsAt(const char* s, int32_t deg, int32_t d, SteamiRgb c);`;
  - public `void watch(uint8_t h, uint8_t m, uint8_t s = 0, SteamiRgb c = STEAMI_LIGHT);`;
  - constants `STEAMI_PIVOT`, `STEAMI_WATCH_*`, `STEAMI_MINUTE_HALF_WIDTH`;
  - script: `sin_q14`, `cos_q14`, `round_q14`, `Panel.polar(d, deg)`, `watch_layout(W, h, m, s)`, which returns the angles, hand tips and number cells and the WHITE ink box of the numbers.

- [ ] **Step 1: Golden sections** (Q14, then `# ---- Widget of 4b Task 2: watch()`).
  - Assert `P128.polar(43, 233) == (30, 90)`, `P128.polar(43, 127) == (98, 90)`, `P240.polar(82, 233) == (55, 169)`, `P240.polar(82, 127) == (185, 169)`.
  - Watch on 128: R 56, major ticks from 48, minor from 51, numbers at 41, hour 28 / 3, minute 42 / 2, second 47. On 240: 105, 90, 96, 77, 52 / 5, 78 / 3, 89.
  - Angles: (10, 10, 30) → 305, 63, 180; (3, 0, 0) → 90, 0, 0; (3, 0, 30) → 90, 3, 180; (10, 11, 59) → 305, 71, 354.
  - Tips of `watch(3, 0, 0)`: hour (92, 64), minute (64, 22), second (64, 17) on 128; (172, 120), (120, 42), (120, 31) on 240. These are `polar()` points; on screen the GRAY second hand (same angle, drawn after) covers the minute tip and the one-pixel top of the minute triangle, so the test does not read RED there.
  - Minute tip of `watch(3, 0, 30)` (minute at 3°, second hand pointing down): (66, 22) on 128, (124, 42) on 240. Assert and print it: nothing is drawn over it, and no other RED pixel lies above it (the hour hand's RED starts at row 61 / 115), so the RED ink box starts on its row.
  - Tips at 10:10:30: hour (41, 48), minute (101, 45), second (64, 111) on 128; (77, 90), (189, 85), (120, 209) on 240.
  - Number cells on 128: "12" (56, 19), "3" (101, 60), "6" (60, 101), "9" (19, 60). On 240: (104, 35), (189, 112), (112, 189), (35, 112).
  - device.py's `int()` tips equal these for `watch(3, 0, 0)` at 128.
  - Print the WHITE box of the four numbers for `watch(1, 37, 5)` (no hand crosses a number) on both panels, and the 10:11:59 tips.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_polar_points(void) {
    // 128: polarX/Y(43, 233) (30, 90); (43, 127) (98, 90); (44, 0) (64, 20); (0, 77) (64, 64);
    //      (44, -90) == (44, 270) (20, 64)
    // 240: (82, 233) (55, 169); (83, 0) (120, 37)
    // for d in {3, 5, 44} and deg in 0..359: polarX(d, deg + 180) - cx == -(polarX(d, deg) - cx), and
    // the same for y (rnd is odd-symmetric)
}
static void test_watch_hands_at_three(void) {
    // watch(3, 0, 0, STEAMI_RED) on 128 grey and 240 colour (RED is unique on both: level 8), spec 7's
    // "watch(3, 0, 0) hand directions": at(hour tip) RED and inkBox(RED).x1 == 92 / 172;
    // at(minute tip) (64, 22) / (120, 42) GRAY: the second hand, at the same 0 degrees and drawn
    // after it (device.py order), covers the minute tip; at(second tip) GRAY: (64, 17) / (120, 31);
    // at(cx, cy) GRAY (pivot)
    // watch(3, 0, 30, STEAMI_RED), values printed by golden_layout.py: the second hand points down,
    // so at(minute tip) (66, 22) / (124, 42) is RED and inkBox(RED).y0 == 22 / 42
}
static void test_watch_integer_angles(void) {
    // Values printed by golden_layout.py: at 10:10:30 and 10:11:59 the hour and minute tips are LIGHT
    // and the second tip GRAY on both panels (m/2 and s/10 are integer divisions: 10:11:59 puts the
    // hour at 305 and the minute at 71)
}
static void test_watch_numbers_golden(void) {
    // Values printed by golden_layout.py: watch(1, 37, 5): inkBox(WHITE) on 128 grey and 240 colour
}
static void test_watch_out_of_range(void) {   // Review Focus 4
    // watch(25, 61, 61) == watch(1, 61, 61); watch(255, 255, 255) and watch(0, 0, 0, STEAMI_BLUE)
    // draw on every panel (onPanel: nothing outside the circle)
}
static void test_watch_stays_inside(void) {   // Review Focus 3
    // forEachPanel: h 0..11, m 0..59 step 7, s 0..59 step 13: outsideCircle() == 0
}
```

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`watch` not a member).

- [ ] **Step 3: Implement** `polarX`, `polarY`, `ray`, `pointer`, `glyphsAt` and `watch` as the decisions say (orders, angles, constants). The number strings are literals in the `.cpp`.

- [ ] **Step 4: Verify**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen`, then `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.
Run: `make tidy`, `make build` — Expected: clean, `SUCCESS`.
Run the stack measurement — Expected: `SteamiScreen::watch` ≤ 64 (48 in a prototype) and `SteamiScreen::pointer` ≤ 64 (bound: it holds the triangle vertices; 64 in a prototype). Note `ray`, `glyphsAt`, `polarX` and `polarY` for the PR.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add watch() and the polar drawing helpers."
```

---

### Task 3: `compass()`, a real rose

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `SteamiScreen.h`, `SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `golden_layout.py`

**Interfaces:**
- Consumes: Task 2's `polarX`, `polarY`, `ray`, `pointer`, `glyphsAt`, `STEAMI_PIVOT`; `strokeCircle`, `circle`.
- Produces:
  - public `void compass(float headingDeg, SteamiRgb c = STEAMI_LIGHT);`;
  - constants `STEAMI_COMPASS_INSET`, `STEAMI_COMPASS_TICK`, `STEAMI_COMPASS_LETTERS`;
  - script `compass_layout(W, heading)`, which returns R, the inner radius, the tick ends, the letter cells, the needle tip and the WHITE box of N.

- [ ] **Step 1: Golden section** (`# ---- Widget of 4b Task 3: compass()`).
  - Assert on 128: R 52, inner circle 36, ticks 46..52, letters at 57, needle 44 with half width 3, pivot 3. On 240: 98, 68, 87..98, 107, 83 / 5, 5.
  - Assert tip and N centre per heading, 128 then 240:
    - 0: (64, 20), (64, 7); (120, 37), (120, 13);
    - 90: (20, 64), (7, 64); (37, 120), (13, 120);
    - 180: (64, 108), (64, 121); (120, 203), (120, 227);
    - 270: (108, 64), (121, 64); (203, 120), (227, 120).
  - N cell top-left = centre − 4k: (60, 3) on 128 and (112, 5) on 240 at heading 0.
  - device.py's `int()` points for the cardinal elements (circles, the four cardinal ticks, letters, needle tip and base) equal these at 128 at heading 0 only: device.py keeps the rose fixed and turns the needle by +heading, where decision 2 turns the whole rose by −heading. The four diagonal ticks are a Q14-versus-`int()` difference (inner end x 97 against 96 at 45°), noted in a comment, not asserted equal.
  - Mockup 07's SVG agrees at 128 (circles 52 and 36, tip 44, half width 3, pivot 3).
  - Print the WHITE box of N for the four headings on both panels.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_compass_needle_and_north(void) {
    // Values printed by golden_layout.py. For h in {0, 90, 180, 270}, compass(h, STEAMI_RED) on 128
    // grey and 240 colour: inkBox(WHITE) is the N box; at(tip) is RED and the RED box reaches the
    // tip on its side (y0 at 0, x0 at 90, y1 at 180, x1 at 270); at 90 the N box lies left of
    // centerX() (spec: "at 90 the N letter is on the left"); at(cx, cy) GRAY
}
static void test_compass_without_a_heading(void) {   // Review Focus 2
    // compass(NAN, STEAMI_RED), compass(INFINITY, ...), compass(-INFINITY, ...): no RED pixel; the
    // WHITE mask equals compass(0)'s; at(cx, cy) GRAY (pivot kept)
}
static void test_compass_heading_rounding(void) {   // Review Focus 2
    // compass(359.6f) == compass(0); compass(0.4f) == compass(0); compass(-90.4f) == compass(270);
    // compass(450) == compass(90); compass(1e30f) draws a needle, no crash
}
static void test_compass_rose_turns(void) {
    // compass(45) on 128 grey and 240 colour: at(polarX/Y(R - u(6), 315)) is LIGHT (the N tick
    // turned to 315) and at(polarX/Y(R - u(6), 0)) is DARK (the NE diagonal tick turned to the
    // top); the needle (length 44 / 83) does not reach either point
}
static void test_compass_stays_inside(void) {   // Review Focus 3
    // forEachPanel, headings 0..359 step 1: outsideCircle() == 0
}
```

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`compass` not a member).

- [ ] **Step 3: Implement** `compass` as the decision says, with an anonymous-namespace `int32_t roundedHeading(float h)` (0 when not finite; the caller skips the needle then).

- [ ] **Step 4: Verify** as Task 2, Step 4. Expected: `SteamiScreen::compass` ≤ 64 (exactly 64 in a prototype with `roundedHeading`: add no live local), `pointer` still ≤ 64.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add compass(), a rose turned to real north."
```

---

### Task 4: `gauge()`, its contrast, and the round widgets without a screen

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `SteamiScreen.h`, `SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `golden_layout.py`

**Interfaces:**
- Consumes:
  - the 4a kernel: `Run`, `fitGlyphs(const Run&, Region*, int32_t)`, `cellWidth`, `drawGlyphs(const Run&, int32_t, SteamiRgb)`, `glyphCount`, `lifted`, `u`, `k`, `radius`;
  - `SteamiValue(long)` and `size_t format(char* out, size_t size, uint8_t decimals) const`;
  - Task 2's `polarX`, `polarY`; `arc()`;
  - Tasks 1 to 3's widgets, for the message tests.
- Produces:
  - public `void gauge(float v, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT, uint8_t arcWidth = 0);`;
  - private `void gaugeArc(int32_t outer, int32_t sweep, SteamiRgb c, uint8_t w);`, `void gaugeLabel(float v, int16_t deg, int32_t d);`, `void drawGaugeLabel(Run& t, int32_t px);` (decision "Helpers and the stack");
  - an anonymous-namespace `int32_t gaugeSweep(float v, float minV, float maxV)` (0..270);
  - constants `STEAMI_GAUGE_*`;
  - script `gauge_layout(W, min_v, max_v, arc_width=0)`, which returns the outer radius, width, label distance, label points and label ink boxes.

- [ ] **Step 1: Golden section** (`# ---- Widget of 4b Task 4: gauge()`).
  - Assert outer 60 and width 7 (ring 54..60) on 128; 113 and 13 (101..113) on 240.
  - Assert label distance 43 / 82 and points (30, 90), (98, 90) / (55, 169), (185, 169).
  - Assert sweep 185 for 342 of 0..500 and 135 for 50 of 0..100.
  - Assert the label texts: −0.5 gives "0", 99.9 gives "99", 1e9 gives "--".
  - Print the GRAY label boxes for (0, 100), (0, 500), (−0.5, 99.9), (1e9, −1e9) (two "--") and (−1e8, 1e8) (slid inward, whole; the two labels overlap at about x 65..69 on 128 and x 113..139 on 240) on both panels. Assert that gauge(2.5, 10, 0) and gauge(7.5, 0, 10) both give sweep 203. Note in a comment that device.py's truncated min point is x 29, and that the cut in `drawGaugeLabel` is a guard no label of at most 10 glyphs reaches.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_gauge_ring(void) {
    // gauge(0) on each panel of forEachPanel: on row centerY() the DARK pixels left of the centre are
    // exactly x 4..10 (128) / 7..19 (240) (ring 54..60 / 101..113, spec 5.8 table); the bottom
    // (cx, cy + outer - 2) is not DARK (the gap from 135 to 225)
}
static void test_gauge_half_and_equal_thickness(void) {
    // spec 7: gauge(50, 0, 100) lights half of the track within 2 %: LIGHT count over the LIGHT count
    // of gauge(100) in [0.48, 0.52]; track and fill of equal thickness: the LIGHT mask of
    // gauge(100) equals the DARK mask of gauge(0) (labels are GRAY)
}
static void test_gauge_labels_golden(void) {
    // Values printed by golden_layout.py: GRAY boxes of gauge(0, 0, 100), gauge(342, 0, 500) and
    // gauge(0, -0.5f, 99.9f) on 128 grey and 240 colour (no value, no unit drawn: no WHITE pixel)
}
static void test_gauge_extremes(void) {   // Review Focus 2
    // no LIGHT pixel for: v NAN, -INFINITY, -1 (0..500), minV == maxV, minV NAN, maxV INFINITY with
    // v INFINITY; the full LIGHT mask (== gauge(100)'s) for v INFINITY, 600 (0..500);
    // gauge(2.5f, 10, 0) and gauge(7.5f, 0, 10) have equal LIGHT masks (ratio 0.75, 203 degrees;
    // short labels: a 3-digit min label such as "100" touches the ring on 240);
    // Values printed by golden_layout.py: the GRAY boxes of gauge(0, 1e9f, -1e9f) (two "--") and of
    // gauge(0, -100000000.0f, 100000000.0f) (slid inward, whole), inside the circle (onPanel)
}
static void test_gauge_arc_width(void) {
    // gauge(0, 0, 100, STEAMI_LIGHT, 3): DARK x 4..6 on row cy of 128 and 7..9 on 240 (physical);
    // arcWidth 64 and 255 on every panel: no crash, inside the circle
}
static void test_gauge_contrast(void) {
    // spec 7: 128 grey, gauge(50, 0, 100, STEAMI_BLUE): every fill pixel has gray4 >= 10, and RED
    // gives 10; 240 colour: the fill is STEAMI_BLUE exactly
}
static void test_gauge_stays_inside(void) {   // Review Focus 3
    // forEachPanel: v -10..110 step 5 with labels (0, 100), (-999, 99999), (-100000000, 1e8f)
}
```

Extend the message tests (Review Focus 5):
- `test_widgets_without_a_screen`: four more fresh objects `c`, `d`, `e`, `f`, each with one call: `c.gauge(50)`, `d.compass(0)`, `e.watch(1, 2, 3)`, `f.face(SteamiFace::HAPPY)`. Each one's `status()` is `UNKNOWN_BOARD` right after its own call (auto-begin isolated per widget, 4a deferred minor), and `countLogged("(E30)")` stays 1.
- `test_screen_e31_message`: also `s.gauge(50)`, `s.compass(90)`, `s.watch(10, 10, 30)`, `s.face(SteamiFace::LOVE, true)`, `s.face(nullptr)`. The counters stay 0.

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`gauge` not a member).

- [ ] **Step 3: Implement** `gaugeSweep`, `gaugeArc`, `gaugeLabel`, `drawGaugeLabel` and `gauge` as the decisions "gauge", "gaugeLabel" and "Helpers and the stack" say.

- [ ] **Step 4: Verify**

Run: `make test-native` — Expected: every suite PASSED.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:`, no ASan or LeakSanitizer report.
Run: `make tidy`, `make build`, `python3 tests/native/test_steami_screen/golden_layout.py` — Expected: clean, `SUCCESS`, exit 0.
Run the stack measurement — Expected: `SteamiScreen::gauge` ≤ 64 (56 in a prototype with `gaugeArc`), `gaugeLabel` ≤ 64 and `drawGaugeLabel` ≤ 64 (bound: buffer, `Run`, `Region`); `subtitle` and `value` still 64. Note `gaugeArc` for the PR. A bound frame over 64 B that cannot shrink without breaking another spec rule: stop and report to the user (Ruling 3).

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add gauge() with its min and max labels."
```

---

### Task 5: Mockups 04, 07, 08 and 09, examples and review list

**Files:**
- Modify: `tests/native/helpers/SteamiMockups.h`, `tests/native/test_steami_screen_mockups/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`. Their header comments say "mockups 01 to 03" (`SteamiMockups.h` line 10, the suite's line 20): they become "mockups 01 to 04 and 07 to 09".
- Create: `lib/steami_board/examples/ScreenGauge/ScreenGauge.ino`, `ScreenCompass/ScreenCompass.ino`, `ScreenFaces/ScreenFaces.ino`, `ScreenWatch/ScreenWatch.ino`
- Modify: `scripts/tests/test_steami_display_rules.py`, `scripts/mockup-review.py`, `scripts/tests/test_mockup_review.py`

**Interfaces:**
- Consumes:
  - Tasks 1 to 4's widgets and 4a's `title`, `value`, `subtitle`;
  - the suite's `render(name, size, color, draw)`, `onPanel`, `maskOf`, `maskAlone`, `any`, `expectShownOnce`, `inkBox`, `expectBox`;
  - `SteamiDistance`: `bool begin()`, `int read()` (−1 when missing). `SteamiCompass`: `bool begin()`, `float heading()` (NAN when missing). `SteamiButtons`: `bool begin()`, `bool wasPressed(SteamiButton)`, `SteamiButton::A`.
- Produces:
  - in `SteamiMockups.h` (each starts with `screen.clear();`, ends with `screen.show();`, closing `}` at column 0):
    - `inline void mockupGauge(SteamiScreen& screen, int d)`
    - `inline void mockupCompass(SteamiScreen& screen, float heading)`
    - `constexpr SteamiFace FACES[6]` in enum order, at namespace scope before `mockupFaces()`, so the suite's `face(FACES[i])` sees it
    - `inline void mockupFaces(SteamiScreen& screen, int shown, int d)`
    - `inline void mockupWatch(SteamiScreen& screen, uint32_t s)`
  - PPM names `steami-mockup-<name>-<128-gray|240-color>.ppm` for the ten names of the decision "Mockup inputs";
  - `MOCKUPS` with 13 names, in this order: `"01_temperature", "02_battery", "03_comfort_dual", "04_circular_gauge", "07_compass", "08_smiley_happy", "08_smiley_sad", "08_smiley_surprised", "08_smiley_angry", "08_smiley_sleeping", "08_smiley_love", "08_smiley_reactive", "09_watch"`.

- [ ] **Step 1: Golden mockup section.** Print the boxes and points Step 2 pins, under `# ---- Values pinned by test_steami_screen_mockups (4b Task 5)`, using the script's widget functions. The mockups suite has no `polar()`: it pins the printed points as literals (for example (104, 24) and (111, 31) on 128 for the 04 fill end).
  - 04: WHITE "342", GRAY labels in rows 85..100 (128) / 160..190 (240), the fill-end points `polar(57, 45)` and `polar(57, 55)` on 128 and `polar(107, 45)` and `polar(107, 55)` on 240 (fill ends at 50°);
  - 07: WHITE N;
  - 08 reactive: GRAY "Mood" (row 20 / 37), LIGHT face, GREEN "HAPPY" (row 108 / 202);
  - 09: WHITE "12" in rows above `centerY()`; hour and minute tips LIGHT.

  Also print the boxes of the 4a mockups 01 to 03 that the suite already pins, so that every pinned number can be regenerated; they must equal the pinned ones.

- [ ] **Step 2: Write the guard and the failing suite**

In `test_steami_display_rules.py`:
- `_TOKEN` gains `SteamiFace::\w+` next to `SteamiAt::\w+`;
- `test_calls_tokens` gains `screen.face(SteamiFace::HAPPY);` → `("face", ["SteamiFace::HAPPY"])`;
- `test_screen_examples_draw_the_mockups` gains the pairs `("ScreenGauge", "mockupGauge")`, `("ScreenCompass", "mockupCompass")`, `("ScreenFaces", "mockupFaces")`, `("ScreenWatch", "mockupWatch")`.

In the mockups suite:

```cpp
static void test_mockup_04_gauge(void) {
    // render("04_circular_gauge", …): mockupGauge(s, 342) on 128 grey and 240 colour; the golden
    // boxes; the fill ends at 50 degrees: the pixel at the golden's polar(57, 45) = (104, 24) is LIGHT
    // and at polar(57, 55) = (111, 31) DARK on 128; the same at radius 107 on 240 (literals printed
    // by golden_layout.py)
}
static void test_mockup_07_compass(void) {
    // render("07_compass", …): mockupCompass(s, 0.0f); WHITE N box; the pixel at the needle tip
    // (64, 20) / (120, 37) LIGHT
}
static void test_mockup_08_faces(void) {
    // const char* const kFaceNames[6] = {"08_smiley_happy", ..., "08_smiley_love"} (full render
    // names, enum order; render() takes a const char*); for i in 0..5: render(kFaceNames[i], …)
    // with mockupFaces(s, i, 0); the LIGHT mask equals face(FACES[i]) alone (maskAlone); no GRAY
    // pixel (no title)
}
static void test_mockup_08_reactive(void) {
    // render("08_smiley_reactive", …): mockupFaces(s, 6, 100); the golden boxes
}
static void test_mockup_09_watch(void) {
    // render("09_watch", …): mockupWatch(s, 10 * 3600UL + 10 * 60 + 30); the golden boxes and tips
}
```

`test_mockups_without_sensors` grows (Review Focus 1), on both panels:
- `mockupGauge(s, -1)`: no LIGHT pixel on the ring, and the WHITE mask equals `value(-1, "mm")` alone;
- `mockupCompass(s, NAN)`: the WHITE mask equals that of `mockupCompass(s, 0.0f)`, and no LIGHT pixel lies within distance 45 (128) / 84 (240) of the centre (the needle is gone; the ticks start at 46 / 87);
- `mockupFaces(s, 6, -1)`: a YELLOW "SURPRISED" subtitle.

Run: `make test-tooling` — Expected: FAIL (examples missing). Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: build error (`mockupGauge` not declared).

- [ ] **Step 3: Write the helpers and the examples**
  - Helper bodies: section 6's `loop()` bodies up to `show()`, each sensor call replaced by its parameter. `mockupFaces` uses the branches of the decision "ScreenFaces". `mockupWatch` repeats `screen.watch((s / 3600) % 24, (s / 60) % 60, s % 60);`.
  - Each example has a header comment in the 4a style:
    - the mockup it reproduces (`<name>_ref.png`);
    - the same sketch on V1a/V1b (128 px grey) and V1c (240 px colour);
    - `screen.begin()` first;
    - what a missing sensor shows (decision "Missing sensor");
    - ScreenGauge: `gauge()` draws the arc only, so `value()` shows the number;
    - ScreenCompass: the needle points to real north, at heading 90 the N letter is on the left, and the pointers on calibration;
    - ScreenFaces: hold A until the face changes;
    - ScreenWatch: the start time is set by hand; `millis()`, no clock.
  - Globals: `SteamiScreen screen;` and the sensors (ScreenFaces also `SteamiButtons buttons;` and `const SteamiFace FACES[] = {…};`).
  - `loop()` is section 6's body. ScreenFaces adds the A press and the `shown` branch; its delay is 50 ms. The other delays are 200, 50 and 500 ms.

- [ ] **Step 4: The review list.**
  - In `scripts/mockup-review.py`, set `MOCKUPS` to the 13 names (`TUTORIALS_COMMIT` and `PANELS` unchanged). The docstring states no count: leave it.
  - In `test_review_writes_the_page_and_never_fails_on_scores`, replace `7` with `2 * len(mr.MOCKUPS) + 1` and `6` with `2 * len(mr.MOCKUPS)`.

- [ ] **Step 5: Verify**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: PASS.
Run, with `SCRATCH` set to your scratchpad: `mkdir -p "$SCRATCH/mockups" && STEAMI_PPM_DIR="$SCRATCH/mockups" .venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: PASS, 26 PPM files.
Run: `python3 scripts/mockup-review.py --ppm-dir "$SCRATCH/mockups" --out "$SCRATCH/mockup-review" --refs /home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups` — Expected: exit 0, 26 scores. Note the 4b scores for the PR (indicative, about 0.7).
Run: `make test-tooling` — Expected: `OK`.
Run: `make test-example/steami_board/ScreenGauge`, `make test-example/steami_board/ScreenCompass`, `make test-example/steami_board/ScreenFaces`, `make test-example/steami_board/ScreenWatch` — Expected: `[PASS]` each.
Run: `make format-check`, `scripts/check-spdx.sh`, `.venv/bin/pio test -e native_asan` — Expected: clean, PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/native/helpers/SteamiMockups.h tests/native/test_steami_screen_mockups/ \
  tests/native/test_steami_screen/golden_layout.py
git commit -m "test(tests): Render mockups 04, 07, 08 and 09 at 128 and 240 px."
git add lib/steami_board/examples/Screen*/ scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the screen examples of mockups 04, 07, 08 and 09."
git add scripts/mockup-review.py scripts/tests/test_mockup_review.py
git commit -m "feat(tooling): Score mockups 04, 07, 08 and 09 in the review."
```

---

### Task 6: README, keywords and full verification

**Files:**
- Modify: `lib/steami_board/README.md` (`## Screen`, `## Testing`), `lib/steami_board/keywords.txt`

**Interfaces:**
- Consumes: the public signatures of Tasks 1 to 4, `SteamiFace`, the four examples.
- Produces: the docs and the PR figures.

- [ ] **Step 1: Write the docs.** In `## Screen`:
  - Widget-table rows: `gauge`, `compass`, `watch` and the three `face` forms, with their signatures and default colours (track DARK, labels GRAY, fill LIGHT; rose and dial DARK, letters and numbers WHITE and GRAY, needle and hands in `c`).
  - A `SteamiFace` paragraph with the six names, `compact`, and the bitmap overload (rows top to bottom, leftmost pixel in the high bit).
  - Rules a student meets:
    - `gauge()` draws the arc and the min/max labels only: call `value()` after it (MicroPython parity, decision 3);
    - a `gauge()` `arcWidth` is in pixels of the panel;
    - the needle of `compass()` points to real north, and the rose turns (link to "### Heading convention"); at heading 90 the N letter is on the left; NAN draws the rose without a needle;
    - `watch()` takes hours 0..23 and minutes and seconds 0..59, and its angles are whole degrees.
  - The contrast bullet covers the gauge fill too (BLUE becomes grey 10 over the DARK track) and names the bar's GRID_DARK track (4a deferred minor).
  - Merge the two `yOffset` bullets (4a deferred minor).
  - Vocabulary rows from spec 5.7: `screen.gauge(v, min_val, max_val, color=C)` → `screen.gauge(v, min, max, C)` and `screen.face("happy", color=C)` → `screen.face(SteamiFace::HAPPY, C)`.
  - Examples table: ScreenGauge (`04_circular_gauge_ref.png`), ScreenCompass (`07_compass_ref.png`), ScreenFaces (`08_smiley_*_ref.png`, the six faces and the reactive smiley), ScreenWatch (`09_watch_ref.png`).
  - `## Testing`: the mockups comment becomes "mockups 01 to 04 and 07 to 09 at 128 and 240 px".

  `keywords.txt` (tab-separated, in the existing blocks): KEYWORD1 `SteamiFace`; KEYWORD2 `gauge`, `compass`, `watch`, `face`. No LITERAL1 for the enumerators (4a precedent for `SteamiAt`).

- [ ] **Step 2: Commit**

```bash
git add lib/steami_board/README.md lib/steami_board/keywords.txt
git commit -m "docs(board): Document gauge, compass, watch and face."
```

- [ ] **Step 3: Full verification before the PR.** Run each; all must succeed:
  - `make test-native`: every suite PASSED.
  - `.venv/bin/pio test -e native_asan`: PASSED for the four display and screen suites, with no `runtime error:` and no ASan or LSan report.
  - `python3 tests/native/test_steami_screen/golden_layout.py`: exit 0. Spot-check five printed values against the two suites.
  - `make lint` (format, SPDX, clang-tidy): clean. `make build`: `SUCCESS`.
  - `make test-examples`: every example PASS, the seven `Screen*` included.
  - `make test-tooling`: `OK`.
  - `make package`, then `ls build/arduino-package/examples/STEAMI_BOARD/Screen{Gauge,Compass,Faces,Watch}/*.ino`: four files. `STeaMi.h` is unchanged (no new header).
  - `.venv/bin/python3 scripts/check-footprint.py`: exit 0.
  - **Stack, binding:** the measurement of the Global Constraints. Every public widget frame ≤ 64 B: `face` (×3), `watch`, `compass`, `gauge`, and 4a's `title`, `subtitle`, `value`, `text`, `bar` again. The bound helpers too: `pointer`, `gaugeLabel`, `drawGaugeLabel` ≤ 64. A larger frame goes back to its task (Ruling 3). Record the other helpers' frames (`polarX`, `polarY`, `ray`, `glyphsAt`, `gaugeArc`).
  - **Flash, a figure for the PR:**
    - build ScreenCompass and ScreenGauge with `PLATFORMIO_BUILD_DIR="$SCRATCH/flash-<Name>" PLATFORMIO_SRC_DIR=lib/steami_board/examples/<Name> .venv/bin/pio run -e steami`, and record `Flash` and `RAM`;
    - with `SIZE` the tool `find_size_tool()` of `check-footprint.py` finds (`arm-none-eabi-size`), run `find "$SCRATCH/flash-ScreenGauge" \( -name SteamiScreen.cpp.o -o -name SteamiValue.cpp.o -o -name SteamiDisplay.cpp.o -o -name SteamiFont.cpp.o -o -name SteamiTrig.cpp.o \) | xargs "$SIZE" -A` and sum `.text` + `.rodata` (`-A`: the Berkeley format folds `.rodata` into `text`);
    - the baseline is measured the same way on b960407, in the scratchpad and read-only for the repository: `mkdir -p "$SCRATCH/base" && git archive b960407 | tar -x -C "$SCRATCH/base"`, then `PLATFORMIO_BUILD_DIR="$SCRATCH/flash-base" PLATFORMIO_SRC_DIR="$SCRATCH/base/lib/steami_board/examples/ScreenTemperature" .venv/bin/pio run -d "$SCRATCH/base" -e steami` (the plan's planning-time figure, 10279 B, is not recorded in the repository);
    - report the 4b sum against that baseline and the spec's "about 8 to 12 KB"; above 12 KB (the decision "Flash estimate" expects about 12.5 KB) is a finding for the maintainer, not a gate.
  - Optional, when network access allows: arduino-cli in a scratch data directory. Otherwise the `arduino-package` CI job is the check.
  - `npx commitlint --from origin/main --to HEAD`: no error. `git log --format=%s origin/main..`: every header ≤ 72 characters, scopes `board|tests|tooling|docs` (`docs` for the plan and the optional section 6 entry), no `!`, no `[skip ci]`, no attribution lines.

---

### Task 7: Hardware acceptance on the V1c

**Controller-run on hardware, needs a board slot; subagents never flash or open the serial port.**

**Files:** none committed, except the `docs(docs)` section 6 entry if the maintainer accepted the 04 overlap (decision "Program 04's text over the ring"; the controller commits it before the PR leaves draft). Scratch sketches live in the controller's scratchpad. Every sketch is built with `PLATFORMIO_SRC_DIR=<dir> .venv/bin/pio run -e steami`, converted with `arm-none-eabi-objcopy -O binary`, and flashed by copying the `.bin` to the STeaMi drive (DAPLink mass storage), never with `pio run -t upload` or OpenOCD, as in 3b and 4a. After the `docs(docs)` commit, rerun `npx commitlint --from origin/main --to HEAD` and the header check of Task 6 Step 3 before the PR leaves draft.

**Interfaces:**
- Consumes:
  - the four examples (Task 5) and the 240 px renders and the review page (Task 5, Step 5);
  - the ten `_ref.png`;
  - a serial capture that resets the target with a break (pyserial `send_break(0.3)` on /dev/ttyACM0 at 115200, then read for N s), as in 3b and 4a: never `scripts/capture-serial.py`, which resets through OpenOCD;
  - the webcam that films the V1c.
- Produces, for the PR: photos next to their `_ref.png` and 240 px render, the north measurements, and the findings.

- [ ] **Step 1: References.** Convert the ten 240 px PPMs to PNG; keep the review page.

- [ ] **Step 2: Photos the controller takes alone.** Flash each example and photograph the screen with the known-good capture of this V1c's webcam: auto exposure (`uvcdynctrl -d /dev/video4 -s "Auto Exposure" 3`) and 60 warm-up frames (`ffmpeg -hide_banner -loglevel error -f v4l2 -input_format mjpeg -i /dev/video4 -vf "select=gte(n\,60),scale=1280:-1,format=yuvj420p" -frames:v 1 -y <file>.jpg`). Manual exposure gives images too dark or too bright. Judge the colours of the fill and of the mood subtitle by hue against the 240 px render.
  - **ScreenGauge:** the live distance, so compare the layout, not the number: the ring 270° open at the bottom, track `#666`, LIGHT fill from the lower left, labels "0" and "500", title, value, "mm", subtitle. Record how the subtitle and title meet the ring ends (decision "Program 04's text over the ring").
  - **ScreenCompass**, unmodified: the rose, the letters upright at the live heading, the needle LIGHT and the south half `#666`, compared with `07_compass_ref.png` and the 240 px render for the layout (the angle is checked in Step 3).
  - **ScreenWatch**, within 10 s of reset: hands at about 10:10:(30 + t), numbers upright, the second hand GRAY.
  - **ScreenFaces:** the mood the camera's distance gives, as found.
  - **The six faces:** a scratch sketch that draws `mockupFaces(screen, i, 0)`'s calls for i = 0..5, 3 s each. Each photo must equal its `08_smiley_<name>_ref.png`: LIGHT pixels, 20 px cells.
  - A V1c mismatch with the 240 px render or with `_ref.png` that is not in section 6's known differences blocks the PR and goes back to the owning task (decision 6, B5). The 04 overlap blocks leaving draft unless the maintainer has accepted it and the `docs(docs)` entry is in section 6.

- [ ] **Step 3: Needle rendering against the measured heading** (the controller, no user).
  - The scratch sketch is ScreenCompass with `SteamiButtons buttons;` added (`buttons.begin()` in `setup()`) and this `loop()`: `float h = compass.heading();`, then section 6's screen calls with `screen.compass(h)`, then every 500 ms `Serial.println(lroundf(h))` (the value drawn, not a second reading), and `if (buttons.wasPressed(SteamiButton::MENU)) compass.calibrate(12);`. `heading()` is not filtered (`SteamiCompass.cpp` returns the magnetometer's reading), so no settling wait is needed.
  - Capture 20 s with the break-reset serial capture and take one photo after 10 s. H is the median of the headings printed in the 4 s around the photo.
  - Expected needle direction on the photo: φ = (360 − H) mod 360, clockwise from screen-up. Screen-up is the side opposite the USB connector; the upright letters confirm it.
  - Measure φ from the pivot (screen centre) to the LIGHT north tip; the south half is `#666`, and N is the only WHITE glyph.
  - Pass: |φ_photo − φ| ≤ 5°, and the N letter lies on the same ray, beyond the tip. (No native render at an arbitrary heading is made; the φ measurement is the check.)

- [ ] **Step 4: Known north (needs the user once; row 4b's "done when").**
  1. Calibrate first: the readings of 2026-11-25 (N 14°, E 116°, S 185°, W 262°) already miss 20° at E. With the Step 3 sketch running, ask the user to press MENU and turn the board in every direction for 12 s, then put it back (`calibrate()` is lost on reset: do not reflash before step 3 below).
  2. Ask the user to lay a phone compass along the board's screen-up edge and read the **magnetic** bearing B (not true north: turn the phone's true-north setting off), then take the phone away (it disturbs the field).
  3. Capture H and a photo.
     - Pass: |H − B| ≤ 20°. That tolerance is for the sensor, not the rendering.
     - The needle then points to magnetic north within the same tolerance.
     - Outside 20°: ask the user to calibrate again (MENU, 12 s), then measure again.
  4. Ask the user to turn the board so that its top points east (B ≈ 90). Photo: the N letter left of the centre and the needle pointing left (spec section 7).

  If the user is not available, the PR stays a draft with this check pending. Steps 3 and the native `test_compass_needle_and_north` cover the rendering, not the absolute north.

- [ ] **Step 5 (optional, user-assisted):**
  - the user presses A in ScreenFaces to walk the six faces and back to the mood;
  - with a hand at about 3, 10, 20 and 40 cm, the four moods appear (SURPRISED YELLOW, HAPPY GREEN, SLEEPING LIGHT, SAD RED);
  - ScreenGauge with an object at about 34 cm gives the mockup's 342 mm.

  Skipped steps are named in the PR; the native suites cover them.

- [ ] **Step 6: Restore the board.** Ask the user to put the board back in its usual place if it was moved, and reflash the program it ran before Task 7 (as 4a's hardware task did). Release the board slot.

- [ ] **Step 7: PR text.** It contains:
  - "Closes #275";
  - the photos with their `_ref.png` and 240 px renders, and the SSIM scores (indicative, not a gate);
  - the north measurements (H, B, φ, the photos), and whether Step 4 ran or is pending;
  - the stack frames (widgets, bound helpers and recorded helpers) and the flash figures of Task 6, with the baseline at b960407;
  - the maintainer's decision on program 04's text over the ring, with the photo, and the `docs(docs)` section 6 entry if it was accepted;
  - the known difference of a 3-digit gauge min label touching the ring on 240 (decision "gaugeLabel"), and the overlap of two slid long labels such as (−1e8, 1e8) on 128 and 240 (x 113..139 on rows 169..182 on the V1c; no example draws such labels);
  - the rounding of points (Q14, not device.py's `int()`), the gauge label rule (integer part, "--", slid inward and whole inside the circle; the cut is a guard no label reaches), and ScreenFaces' design;
  - the departure from section 6's program 08: its delay goes from 200 to 50 ms because of the A-button face cycle that spec 13 leaves to the builder;
  - the paths outside row 4b's list and why (Global Constraints, "Scope");
  - that the V1a/V1b check was not run on hardware (decision 6; covered by the grey 128 px renders and native tests);
  - "both cores" read as PlatformIO and arduino-cli, both on STM32duino 3.1.0, as in 3b and 4a.

---

## Out of scope

- 4c: `graph` (both overloads), `menu` (both overloads), `SteamiHistory`, examples 05 ScreenGraph and 06 ScreenMenu, `WeatherStation` and its place in `SKETCHES`, mockups 05 and 06, `scripts/qualify.py` cases, the selected-menu contrast case.
- The wiki pages (Arduino screen page, E30/E31 in the French error table).
- MicroPython and tutorials follow-ups of spec section 12 (gauge thickness and radius, contrast, real compass rose #434/#371, tutorials 04, 07, 08-reactive).
- Issue #269.
- V1a/V1b hardware checks (decision 6); an RTC source for `watch()` and the STM32RTC example; a big-digits font, partial refresh, DMA.
- Changes to `lib/ssd1327`, `lib/gc9a01`, `requirements.txt`, the Makefile, `platformio.ini`, `.github/workflows/`, and the design doc beyond the one section 6 entry the maintainer may accept (decision "Program 04's text over the ring").
