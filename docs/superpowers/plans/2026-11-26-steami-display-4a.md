# STeaMi display, delivery 4a (layout) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `SteamiScreen`, the layout layer over `SteamiDisplay`: `SteamiAt`, `SteamiValue` and its own formatter, the 128 → 240 scaling rules (`u()`, `k`, `_resolve`, available width), `title`, `subtitle`, `value`, `setDecimals`, `text(at)`, `bar` and the grey contrast rule. Also add the examples of mockups 01 to 03, rendered natively at 128 and 240 px, scored against the tutorials' `_ref.png` by a review script, and checked on a V1c under the webcam.

**Architecture:**
- `SteamiScreen` derives from `SteamiDisplay` and keeps one byte of state (`_decimals`). Every widget computes its layout from `width()` in integers, then draws through the inherited API only:
  - glyphs through the protected `glyph()`, so value text can draw '.' and ':' narrow;
  - spans through `rect(…, true)`.
- The layout kernel is a set of protected members (`u`, `k`, `chord`, `inkChord`, `fitGlyphs`, `resolve`…). Tests reach them through a `TestScreen` subclass, as 3a did with `TestDisplay`. The student sees none of them.
- A stdlib-only Python script, `golden_layout.py`, reimplements the 5.8 and 5.9 rules on the real glyph tables. It asserts every worked number of the spec and prints the ink boxes that the native tests pin.
- A new mockups suite renders 01 to 03 with the same calls as the examples (a guard test keeps them equal) and writes PPM files. `scripts/mockup-review.py` scores them against the `_ref.png` images in a CI artefact. It is a review tool, never a gate.

**Tech Stack:** C++17 (`-std=c++17` native, gnu++17 on target), PlatformIO `env:native`, `env:native_asan`, `env:steami` (STM32duino 3.1.0), Unity, Python 3 (`unittest`, stdlib golden script; Pillow and NumPy for the review script only), GitHub Actions, arduino-cli (CI).

**Spec:** docs/superpowers/specs/2026-11-25-steami-display-design.md. It covers row 4a of section 8 and sections 5.1, 5.2, 5.3, 5.6 (what `SteamiScreen` relies on), 5.7, 5.8, 5.9 (the 4a widgets), 5.10, 5.11, 5.12, 5.13, 6 (programs 01, 02, 03), 7 (suites 2, 3 and 4 as far as 4a goes, build CI, hardware), 9 (decision 6, B2, B3, B6 to B10), 10, 11, 12 and 13.

Branch `feat/steami-display-4a`, from `main` at v0.37.0 (50a9321, which contains 3a #258 and 3b #270). Issue #271; the PR says "Closes #271".

## Global Constraints

Copied from the design document (verbatim):

- "No heap allocation in the layer itself (drivers keep their own buffers), no `String`, no RTTI, no exceptions."
- "Never call the drivers' `line()` or `text()`: they do not clip and use another font."
- "Never use `double` arithmetic or `sin()`/`cos()` in drawing code (single-precision FPU; integer tables)."
- "No text drawn outside the visible circle by a widget: it is truncated or scaled down."
- "No student-visible second font, scale factor or unit system: the 5x7 font, `k` and 128-px units are internal."
- "`float` is accepted at widget inputs (values, ratios) on the single-precision FPU; literals carry the `f` suffix."
- "Do not rely on `printf("%f")` (newlib-nano does not link float formatting by default) nor on `dtostrf` (core-specific, absent on native)."
- "Packaging: flattened `src/`, generated umbrella `STeaMi.h`, unique file names (case-insensitive), no `Steami.h`; no `STEAMI_BOARD_*` in any library `.cpp`; headers carry no macro-dependent inline code."
- "`u(v) = v·W / 128` (int32, truncation toward zero) [...] `k = (W + 64) / 128`, at least 1: 1 on 128, 2 on 240. Glyph factor **and** stroke width."
- "Chord convention [...]: at pixel row y, `chord(y) = 2·isqrt(r² − dy²) + 1` with `dy = y − centerY()`, from `centerX() − isqrt(r² − dy²)` to `centerX() + isqrt(r² − dy²)` inclusive, and 0 when `|dy| > r`."
- "Nothing a widget draws leaves the circle of radius `radius() + 1`. Every cut keeps the longest fitting prefix (rule 6), with no ellipsis."
- "Every object shares the board's backend: two `SteamiScreen` objects draw into the same buffer. The object holds a board pointer, a message name and a status: under 16 B, no buffer. Constant-initialised."
- "NAN [...] never crashes a widget: `value()` shows `--`, `bar()` [...] stay[s] empty." "Every widget accepts empty or `nullptr` strings (draws nothing for that part)." "Widgets use at most 64 B of stack (formatting buffers of 16 B [...])."
- "Default colours [...]: title GRAY, value WHITE, unit LIGHT, subtitle DARK, value label [...] GRAY, [...] bar track GRID_DARK, [...] fills LIGHT."
- Decision 6: "**The V1c comes first.** [...] when a rule cannot serve both panels equally, the V1c wins, and a V1a/V1b defect does not block a delivery. V1a/V1b are not checked on hardware." In this plan the V1a/V1b paths (128 px, grey) are covered by native tests only, and a V1a/V1b-only defect never blocks a task or the PR.
- Scope (row 4a, B4): `lib/steami_board` (code, examples, README, `keywords.txt`); `tests/native` (suites and helpers); `scripts/`; `platformio.ini` and `.github/workflows/` as needed; plus, in this plan, program 02's voltage line in the design doc (decision "Examples include"). Never `lib/ssd1327`, `lib/gc9a01`, `requirements.txt`, the Makefile, MicroPython, tutorials or the wiki.

Repository conventions (CONTRIBUTING.md, commitlint, `.clang-format`, `.clang-tidy`, the 3a and 3b plans):

- The worktree is already set up (husky hooks, `node_modules`). `.venv` is a symlink to the venv of another checkout: run its tools, but never `make setup` and never `pip install` into it. The review script runs locally with the system `python3`, which has Pillow and NumPy.
- Commits: `type(scope): Sentence starting with a capital and ending with a period.`, header at most 72 characters.
  - Scopes allowed by `commitlint.config.js` and used here: `board`, `tests`, `tooling`, `ci`, `docs` (**not** `display`, **not** `screen`, **not** `steami_board`).
  - No `!`, no `BREAKING CHANGE`, never `[skip ci]`, no attribution lines.
- Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`. Headers use `#pragma once`. Google style, indent 4, 100 columns.
- clang-tidy uses `WarningsAsErrors: '*'`. In layout code that means:
  - no `(int)(x + 0.5f)`: use `lroundf`;
  - cast one operand to `int32_t` before multiplying two `int16_t` values;
  - use `fabsf`/`isnan`/`isinf` on `float`, from `<math.h>`;
  - initialise members in place.
- Never call unqualified `min`/`max`/`abs` in library code; use local helpers in an anonymous namespace. Do not reuse the unprefixed names of `SteamiSSD1327_const.h` (`REG_CMD`, `REG_DATA`, `SET_*`).
- A library `.cpp` defines no namespace-scope object with a constructor.
- Tests build screen objects on the file-local `board`, never on `Steami`. A test that builds a panel slot leaves it to `tearDown`'s `board.end()` (LeakSanitizer). Every new test function is registered with `RUN_TEST` in its suite's `main()`.
- In test sketches below, "A == B" between two drawing calls means: render each alone on a fresh, cleared panel of the same size and mode, then compare `pixels()`.
- Never flash, never open a serial port, never touch `/dev/ttyACM*` or `/run/media`, never run `make test-hardware`. Only Task 10 uses the board, and only the controller runs it.

## Decisions taken while planning (the design doc leaves them open)

- **Internal helpers are protected members** of `SteamiScreen` (Task 2 lists them). Tests expose them in `TestScreen` with `using` lines, as 3a's `TestDisplay`. Neither the README nor `keywords.txt` names them. The spec's `_resolve` is `resolve()`: in this repo a leading underscore marks data members.
- **Regions are inclusive** `[left, right]`, width `right − left + 1`, empty when `right < left`.
  - A W/E half region "[edge + k, `centerX() − u(3)`)" is `[cl + k, cx − u(3) − 1]`. E is its mirror image, `[cx + u(3) + 1, cr − k]`.
  - A corner region is the chord shrunk by k at each end.
  - "x clamped into [region left, region right − width]" is `clamp(x, left, right − w + 1)`.
  - This convention reproduces every worked example: 102, 72 and 95 on 128; 180 and 110 on 240; and the region widths 58, 59, 110, 109, 111 and 112.
- **Narrowest chord over inked rows** is the smaller of the chords at the first and at the last inked pixel row (|dy| is largest at an end). It is empty when either row is outside the disc. A text with no inked row (spaces only) always fits and takes no part in the B7 rise test or in a value's fit.
- **CENTER, N and S values use one region** for the f fit and for the ×1 truncation: the available width minus `u(4)`. This reproduces every yOffset threshold of 5.9:
  - 15-character text with a unit: −7/+19 on 128 and −48/+59 on 240;
  - the same without a unit: −12/+14 and −53/+54;
  - the 116 px numeric value: −14/+26 and −49/+60.
- **Rows of `text(s, at)` at CENTER, W and E** follow rule 3 in 128-px units: `u(64 − 4·s)`. That is 60 and 56 on 128 (s 1, 2), and 112 and 105 on 240. The N and S rows are physical (B3, B8).
- **`yOffset` moves CENTER, W and E values only**, as device.py. N, S and the corners take `resolve()`'s row.
- **Cutting and placing:**
  - The row (`margin_ns`) is always computed from the whole text's width.
  - The kept prefix is re-centred at N, S and CENTER, for titles, for subtitle lines, and for values in their W/E region.
  - At E, `text(s, at)` keeps the prefix right-aligned at the anchor.
  - At a corner, the prefix's x is `resolve()`'s x for the prefix width, then clamped.
- **Label and unit of `value()`:**
  - Label top = value top − `u(4)` − 7k (5x7 ×k, GRAY).
  - Unit top = value top + 8f (8x8 ×k, LIGHT).
  - At CENTER, N and S they are centred on `centerX()`.
  - At W and E they are centred in their half region ∩ chord at their own inked rows (spec: "centred in what remains").
  - At a corner they are centred on the value's centre (x + w/2), then clamped.
  - Drawing order: label, value, unit.
- **`subtitle` lines:**
  - The list ends at the first `nullptr`.
  - `""` counts as a line (the block keeps its shape) but draws nothing and is ignored by the rise test.
  - `l1 == nullptr` draws nothing.
- **`SteamiValue::format`:**
  - Floats round half away from zero at `decimals` digits, computed as integer part plus `lroundf(fraction · 10^d)`, with the carry.
  - The minus sign is written only when the rounded value is not zero ("-0.04" with 1 decimal prints "0.0").
  - `decimals` is clamped to 4, here and in `setDecimals()`. With 0 decimals there is no '.'.
  - An integer whose text needs more than `size − 1` characters writes "--". This only happens natively, with a 64-bit `long`; a number is never shown cut.
  - "--" itself is cut to `size − 1` characters.
  - Text is copied up to `size − 1` bytes without splitting a UTF-8 sequence.
  - The return value is `strlen(out)`; `size == 0` writes nothing and returns 0.
  - `value()` always formats into a 16 B buffer. `format()` uses no second buffer (spec 5.11 and 5.12: at most 64 B of widget stack): it counts an integer's digits first, writes "--" when they do not fit, otherwise writes them backwards straight into `out`.
- **Every public widget** (`title`, `subtitle`, `value`, `text(s, at)`, `bar`) starts with `if (status() == SteamiStatus::NOT_STARTED) begin();` then returns when `!isAvailable() || width() <= 0` (spec 5.6 Auto-begin: "the first drawing call begins the display"). Drawing still goes through `glyph()`/`rect()`, which keep `ready()`'s check. So a widget on a board with no screen reports E30 even when nothing would be drawn.
- **Scale:** every widget computes `f` in `int32_t` as `min(max(scale, 1), 255 / k) · k`, so `f` always fits the kernel's `uint8_t` (scale 128 on 240 would otherwise wrap to 0).
- **`bar()` geometry:**
  - x = `cx − w/2`: 20 on 128 and 38 on 240.
  - Fill = `(int)(w · clamp(v, 0, maxV) / maxV)` in `float`, truncated as device.py. `+INF` fills, `−INF` gives 0, NAN or `maxV` not > 0 (NAN included) gives an empty fill.
  - Every row is clipped to the panel and to the chord at that row, so rule 7 holds for any `yOffset`. At the examples' offsets the whole bar is inside the disc and nothing is cut.
  - Rows are computed in `int32_t`, so an `int16_t` wrap never draws a bar somewhere else.
- **Contrast:** `STEAMI_MIN_CONTRAST = 4` lives in `SteamiScreenConst.h`. A protected `lifted(fg, bg)` returns `fg` on a colour panel. On a grey panel it returns `SteamiRgb::gray(min(bg.gray4() + 4, 15))` when `fg.gray4()` is below that, otherwise `fg`. In 4a only `bar()` uses it (fill over track).
- **"No pixel outside the circle" is counted when the pixel is written:**
  - `SteamiMemoryBackend` gains `outsideCircle()`: every `pixel()` and `fillRect()` call with a pixel where `(x − w/2)² + (y − h/2)² > (min(w, h)/2 + 1)²`. A rectangle is outside when one of its four corners is.
  - A call rejected as off-panel counts in `outOfBounds()` only. The counter is reset by `clearCounters()`.
  - It catches black ink and pixels painted over later, which a scan of the final image would miss. `fill()` (`clear()`) is not counted.
  - `stored(SteamiRgb)` gives the value a pixel of that colour holds (grey mode stores `gray(gray4())`).
- **Golden script:** `tests/native/test_steami_screen/golden_layout.py`, stdlib only.
  - It reads `kFont8x8` and `kFont5x7` from `lib/steami_board/src/SteamiFont.cpp`, so ink rows come from the real tables.
  - It implements the kernel of Task 2 and the widget rules of Tasks 3 to 6.
  - It ties the 128 anchors to device.py, as spec 7 asks ("computed from device.py"): a `device_resolve()` transcribed from `_safe_margin` and `_resolve` (micropython-steami-lib `lib/steami_screen/steami_screen/device.py` l.67-101, commit 927e96b) must equal the kernel's `resolve()` at 128 for every anchor wherever 5.9 changes nothing (f = 1, tw < 2r, row below the 13-cell cap).
  - It `assert`s every number the spec states (the self-check), then prints ink boxes `x0 y0 x1 y1` per case.
  - The tests pin those numbers with the comment `// Values printed by golden_layout.py`.
  - It runs by hand, as `golden_colors.py`; CI does not run it.
- **Mockup inputs:**
  - 01: 23.5 °C.
  - 02: 72 % and 3.842 V, so "3842 mV".
  - 03: 23.5 °C and humidity 45.3 %, which gives "Comfortable". The humidity is the example's float, an accepted difference (spec section 6).
  - The chip names are those of the panel's release: "WSEN-HIDS" at 128 and "BME280" at 240, instead of the SVG's "HTS221" (accepted difference). So the 240 render is the reference for the V1c photo.
- **Examples and mockups share their drawing code:**
  - `tests/native/helpers/SteamiMockups.h` holds one `inline` function per mockup.
  - Each example's `loop()` makes the same `screen.` calls with the same literals.
  - A `unittest` compares, statement by statement, the method name and the literal tokens. This is the 3b `Drawing`/`SteamiGallery.h` guard, adapted to sensor reads.
- **Examples include** `<Arduino.h>`, `<SteamiScreen.h>` and the sensor headers, not `<STeaMi.h>`, which exists only in the generated package (as `Drawing`).
  - `setup()`: `Serial.begin(115200);` (so E30/E31 reach the monitor), `screen.begin();`, then the sensors' `begin()`.
  - Example 02 prints the voltage with `(int)lroundf(v * 1000.0f)` and the spec's `"%d mV"`: the spec's `(int)(v * 1000)` prints 24 of the values from 3000 to 4299 mV one millivolt low (4002 mV gives "4001 mV"). This is a **spec correction for the maintainer**: Task 7 updates that line of program 02 in section 6 of the design doc in the same PR (one `docs(docs)` commit), and the PR body says so.
- **The two-objects test** (carried from 3b, spec suite 2) lives in `test_steami_display_board` on the real V1c slot, with a memory-backend twin and the E30 check in `test_steami_screen`.
- **ASan/UBSan:** `[env:native_asan]` `test_filter` becomes the two patterns `native/test_steami_display*` and `native/test_steami_screen*`. Its CI job and its `steami-display-gallery` artefact are otherwise unchanged; the mockup PPMs land there too.
- **Review script:**
  - Fetches the refs from `https://raw.githubusercontent.com/steamicc/micropython-steami-tutorials/83d8e2ca23abb0eeca7afca2d001cdd95b1df76b/docs/mockups/<name>_ref.png`, the tutorials' current HEAD (it contains the last change of the 01 to 03 refs, 29d86d3). `--refs DIR` reads them from a local folder instead.
  - Resizes 384 → 128 and 384 → 240 with Lanczos.
  - The 128 comparison first maps the ref to the 16 OLED grey levels (the `colors.py` rule), stored as the grey render stores them (RGB565 round trip, see `oled_grey` in Task 8).
  - Masks both images outside radius `0.95 · size / 2`: the `#555` bezel ring of every ref lies at about 0.955 to 0.99 R and is the panel edge, not a widget.
  - Computes SSIM in NumPy with scikit-image's defaults: 7x7 uniform window, K1 0.01, K2 0.03, data range 255, sample covariance, 3 px border cropped, mean over channels. `validate.py` calls `structural_similarity(a, b, channel_axis=-1)`, and the spec names only Pillow and NumPy.
  - Writes side-by-side PNGs, `ssim.csv` and `index.html`. Indicative threshold 0.7; it exits 0 whatever the score.
- **CI review job:** Pillow 12.1.1 and NumPy 2.3.5 are installed inline in a new job on Python 3.12. `requirements.txt` stays untouched: it keys the PlatformIO cache and is outside the 4a paths.
  - The refs come from a second `actions/checkout@v4` of the tutorials at the pinned commit (sparse `docs/mockups`), passed with `--refs`; the urllib path stays for local use. A tooling test checks that the workflow's `ref:` equals `TUTORIALS_COMMIT`.
  - The render step is strict (a broken mockup suite fails the job). The scoring step has `continue-on-error: true`, so network or tooling noise never turns a PR red; the upload runs with `if: always()`.
  - The scores go to `$GITHUB_STEP_SUMMARY`, so a drop is visible on the run page without downloading the artefact.
- **Footprint gate:** `SKETCHES` stays `("steami_board/Drawing",)`; WeatherStation joins in 4c. Task 9 measures ScreenComfort to report the layer's flash against the spec's 8 to 12 KB.
- **README:**
  - a `## Screen` section with the 4a widgets and the 4a rows of the vocabulary table (4c completes the reference and the table);
  - the `SteamiScreen` form of E30/E31;
  - the contrast note.
- **`SteamiScreen.h`** declares only the 4a members: no `SteamiFace`, gauge, compass, watch, face, graph, menu or `SteamiHistory`. `maxChars()` is included.

## Review Focus

1. **A sensor is missing** in the three example screens (NAN temperature or humidity, `chip()` returning `""`, `percent()` 0, NAN voltage). Expected: "--" values, "0" and an empty bar, no subtitle text for the empty line, the 03 block keeping its two-line shape, no crash, nothing outside the circle. Test: `test_mockups_without_sensors` (Task 7).
2. **No screen at all** (UNKNOWN release) or a backend whose `begin()` fails. Expected:
   - every widget begins the display on entry, then draws nothing;
   - nothing divides by zero and `isqrt` never sees a negative number (`width()` 0 gives `k` 1, `maxChars()` 0, empty chords, `marginNs()` 2, the clamped cap `r + 2`), also on an injected panel narrower than 104 px;
   - one E30 line naming `SteamiScreen` for two objects;
   - E31 with the `SteamiScreen` text.

   Tests: `test_widgets_without_a_screen` and `test_screen_e31_message` (Task 6).
3. **Accented UTF-8 in titles, subtitles, labels, units and text values** ("Température", "°C"). Expected: widths count glyphs, a cut never splits a code point, no stray `?`. Tests: `test_format_text_keeps_whole_code_points` (Task 1), `test_title_cut_keeps_whole_code_points` (Task 3).
4. **Offsets at the `int16_t` limits** (`yOffset` ±32767, ±20000) for `value()` and `bar()`. Expected: no `int16_t` wrap that draws elsewhere, nothing drawn when the rows leave the panel, UBSan silent. Tests: `test_value_offsets_at_int16_limits` (Task 4), `test_bar_offsets_at_int16_limits` (Task 6).
5. **Extreme numbers**. Expected: exact text or "--", never a cut number, and an empty or full bar, never a crash. Inputs:
   - `LONG_MIN`, `LONG_MAX`, `ULONG_MAX`;
   - ±INF, 1e9f, −0.0f, `setDecimals(9)`;
   - `bar()` with v < 0, v > maxV, ±INF, and `maxV` 0, negative or NAN.

   Tests: `test_format_extremes` (Task 1), `test_bar_extremes` (Task 6).

## File map

| File | Responsibility | Task |
|---|---|---|
| `lib/steami_board/src/SteamiValue.h/.cpp` | the `value()` argument type and its formatter | 1 |
| `platformio.ini` | `native_asan` also runs `test_steami_screen*` | 1 |
| `scripts/tests/test_steami_display_rules.py` | `long long` rejected; examples equal the mockup helpers | 1, 7 |
| `tests/native/helpers/SteamiMemoryBackend.h` | `outsideCircle()`, `stored()` | 2 |
| `tests/native/helpers/SteamiScreenCheck.h` | `InkBox`, `inkBox()`, `expectBox()` | 2 |
| `lib/steami_board/src/SteamiScreenConst.h` | 128-px layout constants, `STEAMI_MIN_CONTRAST` | 2 |
| `lib/steami_board/src/SteamiScreen.h/.cpp` | `SteamiAt`, `SteamiScreen`, layout kernel, widgets | 2 to 6 |
| `tests/native/test_steami_screen/test_main.cpp` | suite 3 of section 7, 4a items | 1 to 6 |
| `tests/native/test_steami_screen/golden_layout.py` | golden positions at 128 and 240 | 2 to 6 |
| `tests/native/test_steami_display_board/test_main.cpp` | two `SteamiScreen` objects on the V1c slot | 6 |
| `tests/native/helpers/SteamiMockups.h` | the drawing code of mockups 01 to 03 | 7 |
| `tests/native/test_steami_screen_mockups/test_main.cpp` | suite 4 of section 7, 01 to 03, PPM files | 7 |
| `lib/steami_board/examples/Screen{Temperature,Battery,Comfort}/*.ino` | the three examples | 7 |
| `docs/superpowers/specs/2026-11-25-steami-display-design.md` | program 02's voltage line (spec correction) | 7 |
| `scripts/mockup-review.py`, `scripts/tests/test_mockup_review.py` | SSIM review page | 8 |
| `.github/workflows/tests.yml` | job `mockup-review`, artefact `steami-mockup-review` | 8 |
| `scripts/tests/test_build_arduino_package.py` | `STeaMi.h` includes the screen headers | 9 |
| `lib/steami_board/README.md`, `keywords.txt` | Screen section, E30/E31 forms, keywords | 9 |

---

### Task 1: `SteamiValue`, its formatter, and the screen suites under ASan

**Files:**
- Create: `lib/steami_board/src/SteamiValue.h`, `lib/steami_board/src/SteamiValue.cpp`
- Create: `tests/native/test_steami_screen/test_main.cpp`
- Modify: `platformio.ini` (`[env:native_asan]` `test_filter`)
- Modify: `scripts/tests/test_steami_display_rules.py` (`_compiles` preamble, new test)

**Interfaces:**
- Consumes: the native mock `millis()` / `millisClock()` (`tests/native/helpers/Arduino.h`), the Unity scaffold of `tests/native/test_steami_display/test_main.cpp` lines 3-70 (`board`, `g_lines`, `capture`, `countLogged`, `loggedLine`, `setUp`/`tearDown`).
- Produces:
  - `SteamiValue.h`, which includes only `<stddef.h>` and `<stdint.h>`. It declares `class SteamiValue` exactly as spec 5.7, with implicit constructors:
    - `SteamiValue(int)`, `SteamiValue(unsigned)`, `SteamiValue(long)`, `SteamiValue(unsigned long)`, `SteamiValue(float)`, `SteamiValue(double)` (stored as `float`), `SteamiValue(const char*)`;
    - `size_t format(char* out, size_t size, uint8_t decimals) const;`
    - private `enum class Kind : uint8_t { INT, UINT, FLOAT, TEXT } _kind;` and `union { long i; unsigned long u; float f; const char* s; } _v;`.
  - `constexpr uint8_t STEAMI_VALUE_MAX_DECIMALS = 4;` in `SteamiValue.h`.
  - The test scaffold reused by Tasks 2 to 6, plus `static std::string fmt(SteamiValue v, uint8_t d = 1)`, which formats into `char b[16]`.

- [ ] **Step 1: Widen the sanitizer env**

In `platformio.ini`, `[env:native_asan]`:

```ini
test_filter =
  native/test_steami_display*
  native/test_steami_screen*
```

- [ ] **Step 2: Write the failing tests** (in `test_steami_screen/test_main.cpp`; add `#include <math.h>`, `<limits.h>`, `<string>`, and a local stub `static int analogRead(uint32_t) { return 512; }`, the target's return type)

```cpp
static void test_format_integers(void) {
    TEST_ASSERT_EQUAL_STRING("0", fmt(0).c_str());
    TEST_ASSERT_EQUAL_STRING("-5", fmt(-5).c_str());
    TEST_ASSERT_EQUAL_STRING("4000000000", fmt(4000000000u).c_str());
    TEST_ASSERT_EQUAL_STRING("-2147483648", fmt(-2147483647L - 1).c_str());
    TEST_ASSERT_EQUAL_STRING("3000000000", fmt(3000000000UL).c_str());
}
static void test_format_floats(void) {
    // fmt(23.5f) "23.5"; fmt(23.5) "23.5"; fmt(45.0f) "45.0"; fmt(23.5f, 0) "24"; fmt(2.5f, 0) "3";
    // fmt(-2.5f, 0) "-3"; fmt(0.25f) "0.3"; fmt(-0.25f) "-0.3"; fmt(0.125f, 2) "0.13";
    // fmt(9.96f) "10.0"; fmt(1.5f, 4) "1.5000"; fmt(1.5f, 9) "1.5000" (clamped to 4)
}
static void test_format_negative_zero(void) {
    // fmt(-0.0f) "0.0"; fmt(-0.04f) "0.0"; fmt(-0.4f, 0) "0"
}
static void test_format_dashes(void) {
    // "--" for NAN, INFINITY, -INFINITY, 1e9f, -1e9f, (const char*)nullptr;
    // fmt(999999936.0f, 4) "999999936.0000"; fmt(-999999936.0f, 4) "-999999936.0000" (15 chars)
}
static void test_format_extremes(void) {   // Review Focus 5
    // for LONG_MIN, LONG_MAX, ULONG_MAX: std::to_string(x) if its length <= 15, else "--"
    // (natively, with a 64-bit long, all three give "--"); never a cut number
}
static void test_format_respects_size(void) {
    // char b[4]: SteamiValue(12345).format(b, 4, 1) == 2 and b "--";
    // SteamiValue("abcdef").format(b, 4, 1) == 3 and b "abc"; format(b, 0, 1) == 0 and b untouched;
    // char c[2]: SteamiValue(NAN).format(c, 2, 1) == 1 and c "-"
}
static void test_format_text_keeps_whole_code_points(void) {   // Review Focus 3
    // char b[3]: SteamiValue("T\xC3\xA9").format(b, 3, 1) == 1 and b "T";
    // char c[4]: the same into c gives "T\xC3\xA9" (3); char d[2]: SteamiValue("\xC2\xB0" "C") -> "" (0)
}
static void test_value_overloads_compile_and_format(void) {   // spec 7: overload smoke test
    // uint8_t u8 = 200; int16_t i16 = -300; size_t n = 7; millisClock() = 4000000000u;
    // fmt(0) "0", fmt(23) "23", fmt(23.5) "23.5", fmt(23.5f) "23.5", fmt(millis()) "4000000000",
    // fmt(analogRead(0)) "512", fmt(u8) "200", fmt(i16) "-300", fmt(n) "7", fmt("text") "text",
    // fmt(NAN) "--", fmt(true) "1"
}
```

In `scripts/tests/test_steami_display_rules.py`, `_compiles` writes `#include "SteamiRgb.h"` before `#include "{header}"`, so a header without colours can be tested. Then:

```python
def test_value_rejects_long_long(self) -> None:
    self.assertTrue(self._compiles("SteamiValue v(1L); (void)v;", header="SteamiValue.h"))
    self.assertFalse(self._compiles("SteamiValue v(1LL); (void)v;", header="SteamiValue.h"))
```

- [ ] **Step 3: Run to verify failure**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error, `SteamiValue.h` not found.

- [ ] **Step 4: Implement `SteamiValue`** as the Interfaces block and the decision "`SteamiValue::format`" say. Floats are formatted in `float` and `uint32_t` arithmetic only, with no `double`, no `printf`, and a power-of-ten table of 5 entries. Integers are written from their magnitude as `unsigned long` (so `LONG_MIN` does not overflow): count the digits first, write "--" if sign and digits exceed `size − 1`, otherwise write the digits backwards straight into `out` (no second buffer: the widget stack budget is 64 B).

- [ ] **Step 5: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, `test_steami_screen` listed, no `runtime error:`, no sanitizer report.
Run: `make test-tooling` — Expected: `OK`. Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add lib/steami_board/src/SteamiValue.* tests/native/test_steami_screen/test_main.cpp \
  scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add SteamiValue and its own number formatter."
git add platformio.ini
git commit -m "build(tooling): Run the screen suites under ASan and UBSan."
```

---

### Task 2: Layout kernel of `SteamiScreen`

**Files:**
- Modify: `tests/native/helpers/SteamiMemoryBackend.h` (`outsideCircle()`, `stored()`)
- Create: `tests/native/helpers/SteamiScreenCheck.h`
- Create: `lib/steami_board/src/SteamiScreenConst.h`, `lib/steami_board/src/SteamiScreen.h`, `lib/steami_board/src/SteamiScreen.cpp`
- Create: `tests/native/test_steami_screen/golden_layout.py`
- Modify: `tests/native/test_steami_screen/test_main.cpp`

**Interfaces:**
- Consumes:
  - From `SteamiDisplay`: the protected `SteamiDisplay(SteamiBoard&, const char* who)` and `glyph(const uint8_t* columns, uint8_t firstColumn, uint8_t columnCount, uint8_t rows, int32_t x, int32_t y, SteamiRgb c, uint8_t scale)`.
  - Its public non-const `width()`, `height()`, `centerX()`, `centerY()`, `radius()` and `isColor()`, and `rect(int16_t, int16_t, int16_t, int16_t, SteamiRgb, bool)`.
  - From the font: `steamiNextGlyph(const char*&)`, `steamiGlyph8x8(uint8_t)` and `steamiGlyph5x7(uint8_t)` (columns, bit 0 = top row). From `SteamiTrig.h`: `steamiIsqrt(uint32_t)`. Task 1's `SteamiValue`.
- Produces:
  - Memory backend: `uint32_t outsideCircle() const` (reset by `clearCounters()`, see the decisions) and `uint16_t stored(SteamiRgb c) const`.
  - `SteamiScreenCheck.h`:
    - `struct InkBox { int16_t x0, y0, x1, y1; size_t count; };`
    - `InkBox inkBox(const SteamiMemoryBackend& mem, SteamiRgb c, int16_t rowFrom = 0, int16_t rowTo = 32767)`: the bounds of the pixels holding `mem.stored(c)` on rows `rowFrom..rowTo`; count 0 means none. The row band separates the lines of a subtitle, which share one colour.
    - `InkBox inkBoxAll(const SteamiMemoryBackend& mem)`: every non-zero pixel.
    - `void expectBox(const InkBox& b, int x0, int y0, int x1, int y1)`: four `TEST_ASSERT_EQUAL_INT_MESSAGE`s, plus `count > 0`.
  - `SteamiScreenConst.h` (128-px units, all `constexpr int16_t` unless stated):
    - `STEAMI_SCREEN_PAD = 4` (the `+4` of floor and `margin_ew`);
    - `STEAMI_TITLE_CAP_CELLS = 13`;
    - `STEAMI_SUBTITLE_DROP = 8`, `STEAMI_SUBTITLE_PITCH = 11`;
    - `STEAMI_VALUE_LABEL_GAP = 4`, `STEAMI_VALUE_FIT_MARGIN = 4`, `STEAMI_VALUE_SIDE_GAP = 3`;
    - `constexpr const char STEAMI_VALUE_REFERENCE[] = "88.8";`
    - `STEAMI_BAR_SIDE = 40`, `STEAMI_BAR_HEIGHT = 8`, `STEAMI_BAR_DROP = 20`;
    - `constexpr uint8_t STEAMI_MIN_CONTRAST = 4;`.
  - `SteamiScreen.h`, which includes `SteamiDisplay.h`, `SteamiScreenConst.h` and `SteamiValue.h`:
    - `enum class SteamiAt : uint8_t { N, NE, E, SE, S, SW, W, NW, CENTER };`
    - Public `class SteamiScreen : public SteamiDisplay`:
      - `explicit constexpr SteamiScreen(SteamiBoard& board = Steami) noexcept : SteamiDisplay(board, "SteamiScreen") {}`
      - `uint8_t maxChars();` (`width() / (8·k())`)
      - `void setDecimals(uint8_t d);` (clamped to `STEAMI_VALUE_MAX_DECIMALS`)
      - `using SteamiDisplay::text;`
      - private `uint8_t _decimals = 1;`
    - Protected kernel, used by Tasks 3 to 6:
      - `struct Region { int32_t left; int32_t right; int32_t width() const; };` (inclusive; width 0 when empty)
      - `struct Point { int32_t x; int32_t y; };`
      - `enum class Ink : uint8_t { TEXT, LABEL, VALUE };` (8x8; 5x7 advancing 6; 8x8 with '.' and ':' from the window (2, 4) advancing 4)
      - `int32_t u(int32_t v128);`
      - `uint8_t k();`
      - `Region chord(int32_t top, int32_t bottom);`
      - `static uint8_t inkRows(const char* s, size_t glyphs, Ink ink);` (OR of the columns of the first `glyphs` glyphs; LABEL masked to 7 rows)
      - `Region inkChord(const char* s, size_t glyphs, Ink ink, int32_t y, uint8_t f);` (`{0, width() − 1}` when the prefix has no ink: callers treat no ink as fitting)
      - `static int32_t cellWidth(const char* s, size_t glyphs, Ink ink, uint8_t f);`
      - `static int32_t valueWidth(const char* s, uint8_t f);`
      - `size_t fitGlyphs(const char* s, Ink ink, uint8_t f, int32_t y, Region limit, uint8_t shrink, int32_t pad);`
      - `int32_t marginNs(int32_t tw, uint8_t f);`
      - `int32_t marginEw(uint8_t f);`
      - `Point resolve(SteamiAt at, int32_t tw, uint8_t f);` (cell top-left)
      - `void drawGlyphs(const char* s, size_t glyphs, Ink ink, int32_t x, int32_t y, SteamiRgb c, uint8_t f);`
      - `SteamiRgb lifted(SteamiRgb fg, SteamiRgb bg);`
      - `static size_t glyphCount(const char* s);` (`steamiGlyphCount`, 0 for `nullptr`)
  - Test fixture:
    - `class TestScreen : public SteamiScreen` with a `using` line for every protected member above, plus `using SteamiDisplay::textSmall;`;
    - `static void onPanel(int16_t size, bool color, void (*check)(SteamiMemoryBackend&, TestScreen&))`: `board.end()`, `setLogger(&capture)`, inject, `TestScreen s(board)`, `s.clear()`, `mem.clearCounters()`, the check, then `outOfBounds() == 0` and `outsideCircle() == 0`;
    - `static void forEachPanel(...)`: 128 grey, 128 colour, 240 colour, 240 grey.

The kernel, written once (Python syntax; the golden script uses it as is and the C++ mirrors it with `int32_t`):

```python
def u(v): return int(v * W / 128)                     # truncation toward zero: u(-12) is -22 on 240
k = max(1, (W + 64) // 128)
r = min(W, H) // 2; cx, cy = W // 2, H // 2            # radius(), centerX(), centerY() in the C++
def chord(top, bottom):                               # narrowest of the two end rows; None = empty
    hs = []
    for y in (top, bottom):
        dy = y - cy
        if abs(dy) > r: return None
        hs.append(isqrt(r * r - dy * dy))
    return (cx - min(hs), cx + min(hs))
def ink_chord(s, n, ink, y, f):                       # mask == 0 -> "no ink": always fits
    m = ink_rows(s, n, ink); first, last = lowest_bit(m), highest_bit(m)
    return chord(y + first * f, y + (last + 1) * f - 1)
def cap(): return r - isqrt(max(0, r * r - (13 * 8 * k // 2) ** 2)) + 2  # 29 on 128, 63 on 240; r + 2 when r <= 52k
def margin_ns(tw, f):
    m = cap() if tw >= 2 * r else r - isqrt(r * r - (tw // 2) ** 2) + 2   # tw < 2r: argument > 0
    return max((16 * f + 4 * k) * W // (128 * k), min(m, cap()))
def margin_ew(f): return (8 * f + 4 * k) * W // (128 * k)
def resolve(at, tw, f):
    mid = u(64 - 4 * (f // k))                        # rule 3 row of CENTER, W, E (text only)
    n, s = margin_ns(tw, f), H - margin_ns(tw, f) - 8 * f
    x = {"W": margin_ew(f), "NW": margin_ew(f), "SW": margin_ew(f),
         "E": W - margin_ew(f) - tw, "NE": W - margin_ew(f) - tw, "SE": W - margin_ew(f) - tw}.get(at, cx - tw // 2)
    y = {"N": n, "NE": n, "NW": n, "S": s, "SE": s, "SW": s}.get(at, mid)
    return x, y
def fit_glyphs(s, ink, f, y, limit, shrink, pad):   # longest prefix, in glyphs
    for n in range(count(s), 0, -1):
        if ink_rows(s, n, ink) == 0: return n
        c = ink_chord(s, n, ink, y, f)
        if c is None: continue
        left, right = max(c[0] + shrink, limit[0]), min(c[1] - shrink, limit[1])
        if cell_width(s, n, ink, f) + pad <= right - left + 1: return n
    return 0
```

- [ ] **Step 1: Write the golden script's kernel and self-check**

`golden_layout.py` follows the style of `golden_colors.py`: SPDX on line 1, a docstring with `Run: python3 golden_layout.py`, and `class Panel(W, H=W)` holding the kernel above. Glyph tables are parsed from `SteamiFont.cpp`: the script reads each table between `const uint8_t kFont8x8[kGlyphCount][8] = {` (and `kFont5x7[kGlyphCount][5]`) and the next `};`, takes the hex bytes of each row (8 or 5 per row), and asserts 96 rows per table. It does not key on the row comments (row 127 is `// 127 degree`) nor on a bare `{0x` (the `kFolds` lines start with it too). It also holds `device_resolve(W, at, tw)`, transcribed from device.py (see the decision "Golden script"). It asserts, for both panels:

```python
assert (P128.k, P240.k) == (1, 2) and (P128.cap(), P240.cap()) == (29, 63)
assert [P128.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [8, 11, 4, 3, 20, 40, -12]
assert [P240.u(v) for v in (8, 11, 4, 3, 20, 40, -12)] == [15, 20, 7, 5, 37, 75, -22]
# floor_f: 20/37 at scale 1, 36/67 at scale 2, 52 for f = 3 on 240; margin_ew 12/22, 30 for f = 3
assert P128.chord(20, 20) == (18, 110)           # 93 px
assert P128.chord(29, 29)[1] - P128.chord(29, 29)[0] + 1 == 107
assert P128.chord(36, 36)[1] - P128.chord(36, 36)[0] + 1 == 115
assert P240.chord(63, 63)[1] - P240.chord(63, 63)[0] + 1 == 211
assert P240.chord(67, 67)[1] - P240.chord(67, 67)[0] + 1 == 215
assert P240.chord(37, 37)[1] - P240.chord(37, 37)[0] + 1 == 173 and P240.chord(41, 41) == (30, 210)
assert P128.chord(9, 15) == (32, 96) and P240.chord(1, 14) == (105, 135)
assert P128.chord(-1, 0) is None and P240.chord(239, 241) is None
# no screen (W 0) and a narrow injected panel: isqrt never sees a negative number (B9)
# P0 = Panel(0), P96 = Panel(96); ANCHORS = the nine SteamiAt names
assert (P0.k, P0.cap(), P0.margin_ns(8, 1), P0.margin_ns(160, 1)) == (1, 2, 2, 2)
assert (P96.cap(), P96.margin_ns(200, 1), P96.margin_ns(160, 1)) == (50, 50, 50)  # r 48 < 52: cap r + 2
# the 128 anchors are device.py's wherever 5.9 changes nothing (f 1, tw <= 104: below the cap)
assert all(P128.resolve(at, tw, 1) == device_resolve(128, at, tw) for at in ANCHORS for tw in range(0, 105, 8))
```

Run: `python3 tests/native/test_steami_screen/golden_layout.py` — Expected: exit 0.

- [ ] **Step 2: Write the failing tests**

```cpp
[[maybe_unused]] static constexpr SteamiScreen kConstInitScreen(board);
static_assert(sizeof(SteamiScreen) <= 3 * sizeof(void*), "12 B on the 32-bit target, under 16 B");

static void test_memory_backend_counts_outside_circle(void) {
    // SteamiMemoryBackend m(128, 128) (outside: (x - 64)² + (y - 64)² > 65² at a pixel or corner):
    // pixel(64, 64) -> outsideCircle() 0; pixel(0, 0) -> 1; fillRect(0, 63, 128, 1) -> still 1
    // (corners at 4097 and 3970, inside); fillRect(60, 60, 8, 8) -> 1; fillRect(120, 0, 4, 4) -> 2;
    // clearCounters() -> 0; pixel(-1, 64) -> outOfBounds() 1, outsideCircle() 0 (an off-panel call
    // is counted by outOfBounds only);
    // stored(STEAMI_BLUE) == STEAMI_BLUE.rgb565() in colour, SteamiRgb::gray(6).rgb565() in grey
}
static void test_scale_quantities(void) {
    // 128: u(8) 8, u(-12) -12, k() 1, maxChars() 16, marginEw(1) 12, marginNs(8, 1) 20,
    //      marginNs(104, 1) 29, marginNs(112, 1) 29, marginNs(160, 1) 29 (tw >= 2r), marginNs(8, 2) 36
    // 240: u(8) 15, u(-12) -22, u(40) 75, k() 2, maxChars() 15, marginEw(2) 22, marginEw(3) 30,
    //      marginNs(16, 2) 37, marginNs(176, 2) 41, marginNs(208, 2) 63, marginNs(16, 4) 67,
    //      marginNs(16, 3) 52
    // 96 (onPanel(96, false, …), an injected panel narrower than 104 px): k() 1, marginNs(200, 1) 50
    //      and marginNs(160, 1) 50 (the clamped cap r + 2: no isqrt of a negative number)
}
static void test_chord_convention(void) {
    // 128: chord(20, 20) == {18, 110}; chord(9, 15) == {32, 96}; chord(29, 29).width() 107;
    //      chord(-1, 0).width() 0; chord(64, 64) == {0, 128}  (row of the centre: 2r + 1 px)
    // 240: chord(37, 37).width() 173; chord(41, 41) == {30, 210}; chord(63, 63).width() 211;
    //      chord(1, 14) == {105, 135}; chord(239, 241).width() 0
}
static void test_ink_rows_and_widths(void) {
    // inkRows("_", 1, TEXT) == 0xC0 (rows 6 and 7); inkRows("8.", 2, VALUE) == 0x7F;
    // inkRows("   ", 3, TEXT) == 0; inkRows("TEMP", 4, LABEL) == 0x7F;
    // cellWidth("23.5", 4, TEXT, 1) 32 == textWidth("23.5"); valueWidth("23.5", 1) 28;
    // valueWidth("23.5", 2) 56; valueWidth("12:34:56", 1) 56; cellWidth("TEMP", 4, LABEL, 2) 48;
    // cellWidth("Température", 11, TEXT, 1) 88 (glyphs, not bytes)
}
static void test_fit_glyphs_longest_prefix(void) {
    // 128, y 29: fitGlyphs("ABCDEFGHIJKLMNOPQRST", TEXT, 1, 29, {0, 127}, 0, 0) == 13;
    //            fitGlyphs("______________T", ...) == 14; fitGlyphs("              ", ...) == 14
    // 240, y 63, f 2: the same three give 13, 13, 14
    // 128: fitGlyphs("TEMP", LABEL, 1, 9, {0, 127}, 1, 0) == 4 (region 33..95) and with limit
    //      {0, 50} == 3 (region 33..50, 18 px: "TEM")
}
static void test_resolve_every_anchor(void) {
    // 128, tw 16, f 1: N (56, 20), NE (100, 20), E (100, 60), SE (100, 100), S (56, 100),
    //                  SW (12, 100), W (12, 60), NW (12, 20), CENTER (56, 60)
    // 240, tw 32, f 2: N (104, 37), NE (186, 37), E (186, 112), SE (186, 187), S (104, 187),
    //                  SW (22, 187), W (22, 112), NW (22, 37), CENTER (104, 112)
    // 240, f 4: CENTER y 105 (u(56))
}
static void test_draw_glyphs_value_window(void) {
    // 128 colour: drawGlyphs("8.8", 3, VALUE, 10, 10, STEAMI_WHITE, 2):
    //   '.' ink is x 28..31, y 20..23 (window columns 3, 4 at f 2), !lit(27, 20), !lit(32, 20);
    //   the second '8' starts at x 34, its first lit column is 36; inkBoxAll right edge 34 + 16 - 1 - 2 = 47
    //   (the 8x8 '8' leaves column 7 empty); drawGlyphs("ABC", 2, TEXT, ...) draws only "AB";
    // drawGlyphs("8:8", 3, VALUE, 10, 10, STEAMI_WHITE, 2): ':' ink is x 28..31 on y 14..15 and
    //   20..21 (glyph rows 2 and 5), !lit(27, 14), !lit(32, 14); the second '8' starts at x 34
}
static void test_lifted_contrast(void) {
    // grey panel: lifted(STEAMI_BLUE, STEAMI_GRID_DARK).gray4() == 7; lifted(STEAMI_GREEN,
    //   STEAMI_GRID_DARK) == STEAMI_GREEN; lifted(STEAMI_BLACK, STEAMI_WHITE).gray4() == 15 (capped)
    // colour panel: lifted(STEAMI_BLUE, STEAMI_GRID_DARK) == STEAMI_BLUE
}
```

- [ ] **Step 3: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error, `SteamiScreen.h` not found.

- [ ] **Step 4: Implement** the helpers, the constants header, the class and the kernel as the Interfaces block and the kernel block say. `drawGlyphs` decodes with `steamiNextGlyph`, stops after `glyphs` glyphs or at the end, and calls `glyph()`:
- TEXT: `(g8, 0, 8, 8)`, advancing 8f;
- LABEL: `(g5, 0, 5, 7)`, advancing 6f;
- VALUE: `(g8, 2, 4, 8)` for '.' and ':' advancing 4f, `(g8, 0, 8, 8)` advancing 8f otherwise.

`isqrt` is never called on a negative number: in `chord()` only when `|dy| ≤ r`, in `cap()` on a value clamped at 0, in `marginNs()` only when `tw < 2r`. `r`, `cx` and `cy` come from `radius()`, `centerX()` and `centerY()`, never from `width()` alone. Products such as `r * r` and `dy * dy` are `int32_t`.

- [ ] **Step 5: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen -f native/test_steami_display` — Expected: PASS (the 3a suite still passes with the extended backend).
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.
Run: `make tidy`, `make build` — Expected: clean, `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add tests/native/helpers/SteamiMemoryBackend.h tests/native/helpers/SteamiScreenCheck.h
git commit -m "test(tests): Count drawing outside the round panel in memory."
git add lib/steami_board/src/SteamiScreen.* lib/steami_board/src/SteamiScreenConst.h \
  tests/native/test_steami_screen/
git commit -m "feat(board): Add the SteamiScreen layout rules for both panels."
```

---

### Task 3: `text(s, at)`, `title` and `subtitle`

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.h/.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`

**Interfaces:**
- Consumes: Task 2's kernel (`resolve`, `fitGlyphs`, `inkChord`, `cellWidth`, `drawGlyphs`, `u`, `k`, `glyphCount`), `TestScreen`, `onPanel`, `forEachPanel`, `inkBox`, `expectBox`.
- Produces (public, spec 5.7):
  - `void text(const char* s, SteamiAt at = SteamiAt::CENTER, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);`
  - `void title(const char* text, SteamiRgb c = STEAMI_GRAY);`
  - `void subtitle(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, SteamiRgb c = STEAMI_DARK);`
  - `void subtitle(const char* l1, SteamiRgb c);`
  - `void subtitle(const char* l1, const char* l2, SteamiRgb c);`
  - Script functions `text_box(W, s, at, scale)`, `title_row(W, s)`, `subtitle_rows(W, lines)`, used by Tasks 4 to 6.

Rules (spec 5.8 rules 3, 6, 7, 5.9 `text(s, at)`, `title`, `subtitle`, B7, B8, B10, and the decisions):
- Every widget, here and in Tasks 4 to 6, starts with the entry guard of the decision "Every public widget" (auto-begin, then return when `!isAvailable() || width() <= 0`).
- `text`:
  - `f` per the decision "Scale" (`min(max(scale, 1), 255 / k)·k`, in `int32_t`), `tw = cellWidth(s, all, TEXT, f)`, `p = resolve(at, tw, f)`.
  - N, S, CENTER: `n = fitGlyphs(s, TEXT, f, p.y, panel, 0, 0)`, x = `cx − w/2`.
  - W: limit `{p.x, W − 1}`, shrink k, x = `p.x` clamped.
  - E: limit `{0, p.x + tw − 1}`, shrink k, x = `p.x + tw − w` clamped.
  - Corners: limit panel, shrink k, x = `resolve(at, w, f).x` clamped into the region at the prefix's inked rows.
- `title(s, c)` is `text(s, SteamiAt::N, c, 1)`. The header keeps the spec's parameter name `text`; the `.cpp` definition names it `s`, since inside the body `text` would name the `const char*` and `text(text, …)` does not compile.
- `subtitle` follows this block, with `S = resolve(S, max line width, k).y` and `pitch = u(11)`:

```python
start = S + u(8) if n == 1 else S - (pitch * (n - 1)) // 2
lowest = S - (n - 1) * pitch
y = start
while y > lowest and any(line has ink and not fits(line, y + i * pitch) for i, line in enumerate(lines)):
    y -= 1
# then each line: fit_glyphs(line, TEXT, k, y + i * pitch, panel, 0, 0), centred on cx, colour c
```

- [ ] **Step 1: Extend the golden script**

Add the three functions. Then add `assert`s for every spec number:
- titles:
  - "Temperature" row 20 / 41, 11 cells kept;
  - "Comfort" and "Battery" row 20 / 37;
  - "ABCDEFGHIJKLM" row 29 / 63, 13 kept;
  - "ABCDEFGHIJKLMNOPQRST" row 29 / 63, 13 kept;
  - 14 '_' row 29 with 14 kept on 128, 13 on 240;
  - "______________T" 14 / 13 kept;
- `text("ABCDEFGH", N, c, 2)`: row 36 / 67, 7 / 6 cells;
- subtitles (128 / 240):
  - one line: "WSEN-HIDS", "BME280", "BQ27441", "VL53L1X" and "SURPRISED" at 108 / 202; "Comfortable" and "VL53L1X ToF" at 104 / 188; "Uncomfortable" at 95 / 166; "20s window" at 107 / 196;
  - two lines, first/second: ("3842 mV", "BQ27441") at 95/106 and 177/197; ("Comfortable", chip) at 95/106 and 173/193; ("Uncomfortable", chip) at 86/97 and 151/171; (chip, "20s window") at 95/106 and 176/196, for chip "WSEN-HIDS", "BME280" and "APDS-9960";
  - every line whole.

Print, for 128 and 240: the `text_box` of "Hi" at the nine anchors at scales 1 and 2, the title boxes of the cases above, and the ink box of every line of every subtitle case.

Run: `python3 tests/native/test_steami_screen/golden_layout.py` — Expected: exit 0 and the tables.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_text_every_anchor_golden(void) {
    // Values printed by golden_layout.py: for 128 grey and 240 colour, scales 1 and 2,
    // text("Hi", at, STEAMI_WHITE, scale) alone on a cleared panel; expectBox(inkBox(mem, WHITE), …)
}
static void test_title_rows(void) {
    // through inkBox(mem, STEAMI_GRAY); 'T', 'A' and capitals ink their top row, so y0 is the cell row:
    // "Temperature" y0 20 (128) / 41 (240), whole (its box equals the printed golden box);
    // "ABCDEFGHIJKLM" y0 29 / 63, 13 cells; "ABCDEFGHIJKLMNOPQRST" y0 29 / 63, 13 cells, centred on
    // centerX(); 14 '_' on 128: whole, x0 8, x1 119; text("ABCDEFGH", SteamiAt::N, STEAMI_WHITE, 2)
    // (WHITE box): y0 36 / 67, 7 / 6 cells; plus the golden boxes printed by golden_layout.py
}
static void test_title_band(void) {
    // titles at scale 1 of 1..20 characters, uppercase, lowercase only ("acemnorsuvwxz"), with
    // descenders ("gjpqy"), and spaces only: top row of the cell within 20..29 (128) / 37..63 (240),
    // inked rows within 20..36 / 37..78; a lowercase-only title's available width comes from the
    // rows it inks, not its cell row (the row depends on the width only): at the cap,
    // inkChord("acemnorsuvwxz", 13, TEXT, 29, 1).width() 109 against 107 for 13 capitals on 128,
    // 215 against 211 at row 63, f 2 on 240 (golden case; both still keep 13 cells);
    // outsideCircle() == 0 (checked by onPanel)
}
static void test_title_cut_keeps_whole_code_points(void) {   // Review Focus 3
    // "Températureéééé" (15 glyphs, 20 bytes) on 128: 13 glyphs kept (cells x 12..115, 104 px);
    // expectBox(inkBox(mem, STEAMI_GRAY), 13, 29, 114, …): ink 102 px wide, since the 8x8 'T' leaves
    // column 0 empty and the 13th glyph, 'é' folded to 'e', leaves column 7 empty (also printed by
    // golden_layout.py); no '?' glyph drawn: title("Températureéééé") and
    // text("Temperatureeee", SteamiAt::N, STEAMI_GRAY), cut to the same 13 glyphs, give the same pixels
}
static void test_text_cut_at_every_anchor(void) {
    // B10: short strings ("Hi") at NE, NW, SE, SW at scale 1; "ABCDEFGHIJKLMNO" at W and E;
    // on both panels: outsideCircle() == 0, and the W text ends at or before the chord's right
    // edge − k at its rows; the E text starts at or after the left edge + k;
    // text("x", SteamiAt::N, STEAMI_WHITE, 200) on 240 (scale clamped, f never wraps): nothing
    // outside the circle
}
static void test_subtitle_rows(void) {
    // Values printed by golden_layout.py: for every subtitle case of Step 1 on 128 grey and 240
    // colour, each line's box is inkBox(mem, STEAMI_DARK, rowFrom, rowTo) with the band of that
    // line's printed rows, and equals the printed box (the script has asserted the spec's cell rows
    // and that every line is whole)
}
static void test_subtitle_block_limits(void) {
    // n = 1, 2, 3 with lines of 3 to 20 characters on both panels: the last line's top lies
    // between S and the device.py start (S + u(8) for one line, S − u(11)·(n−1)/2 + u(11)·(n−1));
    // the pitch is u(11); a line is cut only when the last line's top equals S
}
static void test_subtitle_colour_and_shortcuts(void) {
    // subtitle("a", STEAMI_GREEN) and subtitle("a", "b", STEAMI_GREEN): every lit pixel is GREEN;
    // subtitle("a", "", "c"): three-line block, the empty line draws nothing;
    // subtitle(nullptr), subtitle(""), title(nullptr), title(""), text(nullptr, SteamiAt::N):
    // nothing lit, no call reaches the backend (fillRectCalls() == 0)
}
```

- [ ] **Step 3: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`title` not a member).

- [ ] **Step 4: Implement** `text`, `title` and the three `subtitle` overloads as the rules above say, each behind the entry guard; `title`'s definition names its parameter `s`. Each line is drawn with `drawGlyphs(…, TEXT, …)`, never with `SteamiDisplay::text`, so that the cut is exact.

- [ ] **Step 5: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` and `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.
Run: `make tidy` — Expected: no diagnostic.

- [ ] **Step 6: Commit**

```bash
git add lib/steami_board/src/SteamiScreen.* tests/native/test_steami_screen/
git commit -m "feat(board): Add title, subtitle and text at a cardinal position."
```

---

### Task 4: `value()` at CENTER, N, S, W and E

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.h/.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`

**Interfaces:**
- Consumes: Task 1's `SteamiValue::format`, `_decimals`; Task 2's kernel; Task 3's script functions.
- Produces (public, spec 5.7):
  - `void value(SteamiValue v, const char* unit = nullptr, SteamiAt at = SteamiAt::CENTER, const char* label = nullptr, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 2, int16_t yOffset = 0);`
  - `void value(SteamiValue v, const char* unit, SteamiRgb c);` (CENTER, no label)
  - Protected, used by Task 5: `int32_t blockHeight(uint8_t f, bool hasUnit);` (`8f + 8f/3 + 8k` with a unit, `8f` without). `hasUnit` is `unit != nullptr && unit[0] != '\0'`, as device.py's `if unit:`; a label counts the same way. `golden_layout.py` uses the same rule.
  - Protected, used by Task 5: `void drawValueParts(const char* text, size_t glyphs, int32_t x, int32_t top, uint8_t f, SteamiRgb c, const char* unit, const char* label, SteamiAt at);`, which draws label, value and unit by the decision "Label and unit of `value()`". At W and E it recomputes the half region at the label's and the unit's own rows.
  - Script function `value_layout(W, text, at, unit, label, scale, yOffset)`, returning the boxes of value, label and unit.

Algorithm (spec 5.9 `value`, B2, B6, B9, and the decisions):

```python
t = format(v, decimals)                     # 16 B buffer; NAN -> "--"
for f in range(min(max(scale, 1), 255 // k) * k, 0, -1):   # decision "Scale"
    vw = value_width(t, f)
    if at in (CENTER, W, E): top = cy + u(yOffset) - block_height(f, unit) // 2
    if at in (N, S):         top = resolve(at, vw, f).y
    if at in (CENTER, N, S): region = ink_chord(t, all, VALUE, top, f) minus u(4) in width; fits = no ink or vw <= region width
    if at in (W, E):         region = half(at, chord(top, top + 8f - 1)); fits = max(vw, value_width("88.8", f)) <= region width
    if fits: break
else: f = 1; keep = fit_glyphs(t, VALUE, 1, top, limit, 0, pad)   # CENTER/N/S: pad u(4); W/E: limit = half region
x = cx - w // 2 at CENTER, N and S; region.left + (region.width - w) // 2 at W and E
```

An empty region (rows outside the disc) draws nothing for that part.

- [ ] **Step 1: Extend the golden script**

Add `value_layout`. Assert:
- `value_width("23.5", f) == 28 * f`;
- CENTER "23.5" with unit, 128: f 2, top 50, x 36, ink rows 50..63, unit "°C" ink rows 66..72;
- CENTER, 240: f 4;
- W "23.5" and E "45": ×2 at top 50 on 128, regions (3, 60) and (68, 125); ×3 at top 96 on 240, regions (5, 114) and (126, 235);
- "1" ×2 / ×3; "8888" ×1 / ×3; "123456" ×1 / ×2;
- the ×1 W region on 128 is (2, 60);
- "12:34:56" whole at W on 128;
- "Uncomfortable" at W: cut on 128, whole on 240; "ABCDEFGHIJKLMNO" at W cut on 240;
- "888888888888888" with a unit at CENTER: cut at yOffset −12 (128) and −50 (240), whole at 0;
- the threshold search of the decision: (−7, +19), (−12, +14), (−14, +26) on 128 and (−48, +59), (−53, +54), (−49, +60) on 240;
- labels "TEMPERATU" (9 characters) whole above a W value on 128, "TEMPERAT" (8) on 240, "TEMPERATURE" cut.

Print the value, label and unit boxes for: 01 (CENTER, "23.5", "°C"); 02 (CENTER, "72", "%", yOffset −12); 03 (W "23.5" "°C" "TEMP", E "45.3" "%" "HUM"); N and S "23.5" with and without a unit.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_value_center_golden(void) {
    // Values printed by golden_layout.py, via inkBox of WHITE (value), LIGHT (unit), GRAY (label),
    // on 128 grey and 240 colour, for the printed cases; 01's "23.5" is 56 px of cells (x 36) on 128
}
static void test_value_glyph_scale_side_by_side(void) {
    // value(23.5, "°C", SteamiAt::W, "TEMP") and value(45, "%", SteamiAt::E, "HUM"): ink height 7f
    // with f 2 on 128 and 3 on 240 for both; "1" against "88.8" the same; "8888" f 1 / 3;
    // "123456" f 1 / 2 and inside its half region; drawing E then W gives the same pixels as W then E
}
static void test_value_truncation(void) {
    // the spec cases of Step 1 on both panels; every truncated text lies inside its region (inkBox
    // within the region bounds) and inside the circle
}
static void test_value_labels_and_units(void) {
    // value(23.5, "°C", W, "TEMPERATURE") and the same at E: label cut to the cells that fit;
    // labels of up to 9 (128) / 8 (240) characters whole; nothing outside the circle
}
static void test_value_nan_and_decimals(void) {
    // value(NAN, "°C") draws "--" (compare pixels with value("--", "°C")); setDecimals(2):
    // value(23.456f) == value("23.46"); setDecimals(0): "23"; setDecimals(9): 4 decimals
}
static void test_value_edge_cases(void) {
    // value(23.5f, "°C", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 0) == the same with scale 1;
    // value(23.5f, "°C", SteamiAt::W, "TEMP", STEAMI_WHITE, 2, 80) (rows out of the disc):
    // no crash, nothing outside the circle, and litCount() == 0 on both panels (spec 7: "draws
    // nothing"); value("") with unit and label: label and unit only;
    // value(0, "", SteamiAt::CENTER, "") draws "0" only, and value(0, "") == value(0, nullptr)
    // pixel for pixel (an empty unit is no unit)
}
static void test_value_offsets_at_int16_limits(void) {   // Review Focus 4
    // yOffset ±32767 and ±20000 at CENTER, W and E on both panels: nothing lit, fillRectCalls() == 0
}
static void test_value_colour_shortcut(void) {
    // value(23.5f, "°C", STEAMI_GREEN) == value(23.5f, "°C", SteamiAt::CENTER, nullptr, STEAMI_GREEN)
}
```

- [ ] **Step 3: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`value` not a member).

- [ ] **Step 4: Implement** `value()`, `blockHeight()` and `drawValueParts()` per the algorithm and the decisions. N and S use `resolve()` with the value's width and f. For now NE, NW, SE and SW take the CENTER branch; Task 5 replaces it.

- [ ] **Step 5: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` and `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.
Run: `make tidy`, `make build` — Expected: clean, `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add lib/steami_board/src/SteamiScreen.* tests/native/test_steami_screen/
git commit -m "feat(board): Add value() with its fit and side-by-side scale."
```

---

### Task 5: `value()` at the corners

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`

**Interfaces:**
- Consumes: Task 4's `value()`, `blockHeight()`, `drawValueParts()`, `value_layout`; Task 2's `resolve`, `chord`, `fitGlyphs`.
- Produces: the NE, NW, SE and SW branch of `value()` (no new signature).

Algorithm (5.9 "Corner anchors", B9):

```python
for f in range(min(max(scale, 1), 255 // k) * k, 0, -1):
    p = resolve(at, value_width(t, f), f)
    region = chord(p.y, p.y + 8f - 1) shrunk by k at each end
    if region not empty and region.left <= p.x and p.x + vw - 1 <= region.right: break
else:                                   # f = 1: p, top and region stay those of the whole text
    keep the longest prefix with value_width <= region width (w = its width)
    x = clamp(resolve(at, w, 1).x, region.left, region.right - w + 1)   # only x follows the prefix
    top = p.y                                                          # the row never moves
# label and unit: region = ink chord at their own rows shrunk by k; cut to the longest prefix;
# x = value centre - w // 2, then clamped; an empty region draws nothing
```

- [ ] **Step 1: Extend the golden script** with the block above as written (the ×1 row stays the whole text's). Assert the worked examples:
- 128, `value(8, "C", NE, "TEMP", WHITE, 1)`: value x 102, row 20; label ink rows 9..15, region (33, 95), x 72; at SE, unit rows 108..114, region (26, 102), x 95.
- 240, the same NE call: value x 180, row 22; label rows 1..14, region (107, 133), "TE" kept at x 110.

Print the value, label and unit boxes for the four corners × scales 1 and 2 × labels "TEMP", "TEMPERATURE" (11 characters) and "TEMPERATURES12" (14) × both panels, and for `value(8, nullptr, NE, nullptr, WHITE, 1)` on 128.

- [ ] **Step 2: Write the failing tests**

```cpp
static void test_value_corner_worked_examples(void) {
    // 128 grey, value(8, "C", SteamiAt::NE, "TEMP", STEAMI_WHITE, 1): inkBox(WHITE) x0 103 (the 8x8
    // '8' leaves column 0 empty) and y0 20; inkBox(GRAY) x0 72 (the 5x7 'T' inks column 0), x1 <= 95,
    // rows 9..15 (spec: "the NE label ends at x <= 95 on rows 9..15"); the same call at SE:
    // inkBox(LIGHT) x0 96 ('C' leaves column 0 empty), rows 108..114
    // 240 colour, the NE call: inkBox(WHITE) x0 181 (f 1), y0 22; inkBox(GRAY) x0 110, x1 <= 133, rows 1..14,
    // and its pixels equal those of textSmall("TE", 110, 1, STEAMI_GRAY, 2) drawn alone
}
static void test_value_corner_golden(void) {
    // Values printed by golden_layout.py for every printed case
}
static void test_value_corners_stay_inside(void) {
    // the printed cases plus value(-12345.678f, "°C", corner, "TEMPERATURES12", c, 2) and the text
    // value "Uncomfortable": onPanel's outsideCircle() == 0 on all four panels
}
```

- [ ] **Step 3: Run to verify failure** — Expected: `test_value_corner_worked_examples` FAILS (the corners still fall back to CENTER).

- [ ] **Step 4: Implement** the corner branch.

- [ ] **Step 5: Run to verify pass** — `.venv/bin/pio test -e native -f native/test_steami_screen`, `.venv/bin/pio test -e native_asan`, `make tidy` — Expected: PASS, clean.

- [ ] **Step 6: Commit**

```bash
git add lib/steami_board/src/SteamiScreen.cpp tests/native/test_steami_screen/
git commit -m "feat(board): Fit value() and its texts into the screen corners."
```

---

### Task 6: `bar()`, contrast, messages and two screens on one backend

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.h/.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`
- Modify: `tests/native/test_steami_display_board/test_main.cpp` (`#include "SteamiScreen.h"`, new tests, `RUN_TEST` lines)

**Interfaces:**
- Consumes: Task 2's `lifted`, `chord`, `u`; Tasks 3 and 4's widgets. In the board suite: `board`, `presentBme280()`, `commandCount(uint8_t)`, `lastData(size_t)`, `kGc9a01Frame` (240·240·2), `countLogged`, `loggedLine`, `g_spi`.
- Produces (public, spec 5.7):
  - `void bar(float v, float maxV = 100, int16_t yOffset = 0, SteamiRgb c = STEAMI_LIGHT);`
  - `void bar(float v, float maxV, SteamiRgb c);`
  - Script function `bar_rect(W, v, maxV, yOffset)`.

Rules: track `GRID_DARK`, width `W − u(40)`, height `u(8)`, x `cx − w/2`, top `cy + u(20) + u(yOffset)`. The fill is drawn in `lifted(c, STEAMI_GRID_DARK)`; its width follows the decision "`bar()` geometry". Each row `y` in [top, top + h) that lies on the panel is drawn as `rect(x0, y, x1 − x0 + 1, 1, …, true)`, with `[x0, x1]` = the bar's span ∩ `chord(y, y)`. An empty intersection draws nothing.

- [ ] **Step 1: Extend the golden script.** Assert: on 128, track x 20..107 and rows 84..91 at yOffset 0, rows 72..79 at −12; on 240, x 38..202, height 15, top 135 at −12. Fill 63 px for 72 % on 128 and 118 on 240. Print the boxes.

- [ ] **Step 2: Write the failing tests**

In `test_steami_screen`:

```cpp
static void test_bar_golden(void) {
    // Values printed by golden_layout.py: bar(72, 100, -12, STEAMI_GREEN) on 128 grey and 240
    // colour: inkBox(GRID_DARK) is the track's unfilled part, inkBox(fill colour) the fill
}
static void test_bar_contrast(void) {
    // 128 grey: bar(50, 100, 0, STEAMI_BLUE): every fill pixel has gray4 >= 7 (spec 7: "a BLUE bar
    // fill at least 7"), the track stays level 3; 240 colour: the fill is STEAMI_BLUE exactly
}
static void test_bar_extremes(void) {   // Review Focus 5
    // v -5 and -INFINITY: track only; v 150 and +INFINITY: full; NAN: track only;
    // maxV 0, -10, NAN: track only; bar(50, 100, STEAMI_GREEN) == bar(50, 100, 0, STEAMI_GREEN)
}
static void test_bar_stays_in_the_circle(void) {
    // yOffset -60..60 step 4 on both panels: outsideCircle() == 0; at yOffset 0 the track is whole
    // (88 / 165 px on every row)
}
static void test_bar_offsets_at_int16_limits(void) {   // Review Focus 4
    // yOffset ±32767 and ±20000: nothing lit, fillRectCalls() == 0
}
static void test_widgets_without_a_screen(void) {   // Review Focus 2
    // fresh board (UNKNOWN, no injection): TestScreen a(board), b(board); width() 0, k() 1,
    // maxChars() 0, a.marginNs(8, 1) 2 and a.marginNs(160, 1) 2 (floor 0, clamped cap r + 2: no isqrt
    // of a negative number, which UBSan could not see through the uint32_t argument);
    // a.title("T"), a.subtitle("s"), a.value(23.5f, "°C", SteamiAt::W, "L"),
    // b.bar(50), b.text("x", SteamiAt::NE), a.show(): no crash; b began through its first widget
    // (decision "Every public widget"), so countLogged("(E30)") == 1 (deduplicated by who and code) and
    // loggedLine("(E30)") == "[STeaMi] SteamiScreen: cannot tell if this board is a V1a/V1b or a
    // V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)";
    // a.error() and b.error() "unknown board"
}
static void test_screen_e31_message(void) {
    // injected SteamiMemoryBackend(240, 240, true, false): every widget draws nothing
    // (fillRectCalls() == 0); loggedLine("(E31)") == "[STeaMi] SteamiScreen: not enough RAM for the
    // colour screen (115200 bytes in one block). Call screen.begin() first in setup(). (E31)";
    // error() "no memory"
}
static void test_two_screens_share_a_memory_backend(void) {
    // injected SteamiMemoryBackend(128, 128, false): SteamiScreen a(board), b(board);
    // a.title("Comfort"); b.value(45.3f, "%", SteamiAt::E, "HUM"); a.show();
    // beginCalls() == 1, a.backend() == b.backend() == &mem, both GRAY title and WHITE value lit
}
```

In `test_steami_display_board` (spec suite 2, carried from 3b):

```cpp
static void test_two_screens_share_the_gc9a01_slot(void) {
    board.begin(SteamiRelease::V1C);
    SteamiScreen a(board), b(board);
    a.setDecimals(2);
    a.title("Comfort");
    a.value(23.456f, "°C", SteamiAt::W, "TEMP");
    b.value(45.3f, "%", SteamiAt::E, "HUM");      // b keeps 1 decimal: _decimals is per object
    b.show();
    // a.backend() == b.backend() == board.display(), not null: both draw into one backend
    // commandCount(GC9A01_CMD_DISPON) == 1: the panel is begun once (no double begin)
    // a.status() == b.status() == SteamiStatus::OK
    // frame = lastData(kGc9a01Frame); board.end() frees the one slot (ASan/LSan: no double free);
    // board.setLogger(&capture); SteamiMemoryBackend mem(240, 240); board.setDisplayBackend(mem);
    // SteamiScreen m(board); m.setDecimals(2); m.title("Comfort");
    // m.value(23.456f, "°C", SteamiAt::W, "TEMP"); m.setDecimals(1);
    // m.value(45.3f, "%", SteamiAt::E, "HUM"); m.show();
    // consistent results: every (x, y) of frame equals mem.at(x, y)
}
```

- [ ] **Step 3: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_screen -f native/test_steami_display_board` — Expected: build error (`bar` not a member).

- [ ] **Step 4: Implement** the two `bar` overloads per the rules.

- [ ] **Step 5: Run to verify pass**

Run: `make test-native` — Expected: every suite PASSED.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:`, no ASan or LeakSanitizer report.
Run: `make tidy`, `make build` — Expected: clean, `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add lib/steami_board/src/SteamiScreen.* tests/native/test_steami_screen/
git commit -m "feat(board): Add bar() with the grey minimum contrast."
git add tests/native/test_steami_display_board/test_main.cpp
git commit -m "test(tests): Check that two SteamiScreen objects share one backend."
```

---

### Task 7: Mockups 01 to 03, native renders and examples

**Files:**
- Create: `tests/native/helpers/SteamiMockups.h`
- Create: `tests/native/test_steami_screen_mockups/test_main.cpp`
- Create: `lib/steami_board/examples/ScreenTemperature/ScreenTemperature.ino`, `lib/steami_board/examples/ScreenBattery/ScreenBattery.ino`, `lib/steami_board/examples/ScreenComfort/ScreenComfort.ino`
- Modify: `scripts/tests/test_steami_display_rules.py`
- Modify: `docs/superpowers/specs/2026-11-25-steami-display-design.md` (program 02's voltage line only)

**Interfaces:**
- Consumes:
  - Every widget of Tasks 3 to 6, `vline`, `centerX()`, `centerY()`, `radius()`, `show()`; `inkBox`, `outsideCircle()`.
  - `SteamiTemperature`/`SteamiHumidity`: `bool begin()`, `float read()` (NAN when missing), `const char* chip() const` ("" when none).
  - `SteamiBattery`: `bool begin()`, `uint8_t percent()` (0 when missing), `float voltage()` (NAN when missing).
- Produces:
  - `SteamiMockups.h` (SPDX, `#pragma once`; includes `SteamiScreen.h`, `<math.h>`, `<stdio.h>`):
    - `inline void mockupTemperature(SteamiScreen& screen, float t, const char* chip)`
    - `inline void mockupBattery(SteamiScreen& screen, uint8_t percent, float v)`
    - `inline void mockupComfort(SteamiScreen& screen, float t, float h, const char* chip)`
    - Their bodies are the `loop()` bodies of section 6 up to `show()`, with each sensor call replaced by its parameter and the voltage line per the decision.
  - PPM files `steami-mockup-<name>-<size>-<mode>.ppm`, where `<name>` is `01_temperature`, `02_battery` or `03_comfort_dual` and `<size>-<mode>` is `128-gray` or `240-color`. They are written to `$STEAMI_PPM_DIR` only when it is set and not empty. Task 8 reads them.

- [ ] **Step 1: Write the guard and the failing suite**

In `scripts/tests/test_steami_display_rules.py`:

```python
def test_screen_examples_draw_the_mockups(self) -> None:
    # calls(text): every statement that starts with "screen." -> (method name, tokens), where tokens
    # are, in order, the string literals, numeric literals, STEAMI_* names, SteamiAt::* names and
    # nullptr of the statement (regex; "//" comments dropped). For (ScreenTemperature,
    # mockupTemperature), (ScreenBattery, mockupBattery), (ScreenComfort, mockupComfort): calls() of
    # the example's loop() body equals calls() of the helper's body, both non-empty; the example's
    # loop() also contains "screen.show();" and the helper ends with "screen.show();"
```

`test_steami_screen_mockups/test_main.cpp` (fixture as Task 2's `onPanel`, plus a `render(name, size, color, draw)` that writes the PPM as 3a's `galleryOn` does):

```cpp
static void test_mockup_01_temperature(void) {
    // 128 grey: mockupTemperature(s, 23.5f, "WSEN-HIDS"); 240 colour: (…, "BME280");
    // outsideCircle() == 0, outOfBounds() == 0, showCalls() == 1, litCount() > 0;
    // title GRAY at row 20 / 41; value WHITE ink rows 50..63 on 128; unit LIGHT rows 66..72 on 128
}
static void test_mockup_02_battery(void) {
    // mockupBattery(s, 72, 3.842f) on both: the same four checks; bar track and GREEN fill rows
    // 72..79 on 128 (spec: "rows 72..79 on 128, as mockup 02"); subtitle "3842 mV" / "BQ27441" at
    // 95/106 (128) and 177/197 (240)
}
static void test_mockup_03_comfort(void) {
    // mockupComfort(s, 23.5f, 45.3f, chip) with "WSEN-HIDS" (128) / "BME280" (240): the four checks;
    // the DARK vline at x centerX(); both values at f 2 (128) / 3 (240); "Comfortable" first line
}
static void test_mockups_without_sensors(void) {   // Review Focus 1
    // mockupTemperature(s, NAN, ""), mockupBattery(s, 0, NAN), mockupComfort(s, NAN, NAN, "") on
    // both panels: no crash, outsideCircle() == 0; the value pixels equal those of value("--", …);
    // 02 shows "0" and a track with no fill; 03's subtitle is "Uncomfortable" plus an empty line
    // at 86 (128) / 151 (240)
}
```

Run: `make test-tooling` — Expected: FAIL (examples missing). Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: build error, `SteamiMockups.h` not found.

- [ ] **Step 2: Write the helpers and the examples**

Each example:
- Header comment: which mockup it reproduces (`<name>_ref.png` of the tutorials), the board differences (V1a/V1b 128 px grey, V1c 240 px colour, same sketch), that `screen.begin()` comes first in `setup()` (115 200 bytes in one block on V1c), and what it shows when the sensor is missing ("--").
- `#include <Arduino.h>`, `<SteamiScreen.h>` and the sensor headers (`<SteamiTemperature.h>`; `<SteamiBattery.h>`; `<SteamiTemperature.h>` + `<SteamiHumidity.h>`).
- Globals `SteamiScreen screen;` and the sensor objects of section 6.
- `setup()`: `Serial.begin(115200); screen.begin();` then the sensors' `begin()`.
- `loop()`: section 6's body verbatim, except 02's voltage line: `if (!isnan(v)) snprintf(volts, sizeof volts, "%d mV", (int)lroundf(v * 1000.0f));`. The delays are 500, 1000 and 1000 ms.
- In the design doc, program 02 of section 6 gets the same voltage line (the spec correction of the decision "Examples include").

- [ ] **Step 3: Verify**

Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: PASS.
Run, with `SCRATCH` set to your scratchpad directory: `mkdir -p "$SCRATCH/mockups" && STEAMI_PPM_DIR="$SCRATCH/mockups" .venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: PASS and six `steami-mockup-*.ppm` files.
Run: `make test-tooling` — Expected: `OK`.
Run: `make test-example/steami_board/ScreenTemperature`, `make test-example/steami_board/ScreenBattery`, `make test-example/steami_board/ScreenComfort` — Expected: `[PASS]` each.
Run: `make format-check` and `scripts/check-spdx.sh` — Expected: clean.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.

- [ ] **Step 4: Commit**

```bash
git add tests/native/helpers/SteamiMockups.h tests/native/test_steami_screen_mockups/
git commit -m "test(tests): Render mockups 01 to 03 at 128 and 240 px."
git add lib/steami_board/examples/Screen*/ scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the screen examples of mockups 01 to 03."
git add docs/superpowers/specs/2026-11-25-steami-display-design.md
git commit -m "docs(docs): Round the battery voltage of program 02 to the mV."
```

---

### Task 8: `scripts/mockup-review.py` and the review artefact

**Files:**
- Create: `scripts/mockup-review.py` (line 1 `#!/usr/bin/env python3`, line 2 SPDX, docstring with Usage, `from __future__ import annotations`, `sys.exit(main())`, mode 755)
- Create: `scripts/tests/test_mockup_review.py`
- Modify: `.github/workflows/tests.yml` (new job)

**Interfaces:**
- Consumes: Task 7's PPM names; the tutorials' refs `01_temperature_ref.png`, `02_battery_ref.png`, `03_comfort_dual_ref.png` (384x384 RGB).
- Produces:
  - Constants:
    - `TUTORIALS_COMMIT = "83d8e2ca23abb0eeca7afca2d001cdd95b1df76b"`
    - `MOCKUPS: tuple[str, ...] = ("01_temperature", "02_battery", "03_comfort_dual")`
    - `PANELS: tuple[tuple[int, str], ...] = ((128, "gray"), (240, "color"))`
    - `MASK_RATIO = 0.95`, `SSIM_HINT = 0.7`
  - `ref_url(name: str) -> str` (pinned raw URL) and `ppm_name(name: str, size: int, mode: str) -> str` (no NumPy needed).
  - `oled_grey(rgb)`, `disc_mask(size: int, ratio: float)`, `ssim(a, b) -> float` (uint8 HxWx3; see the decision "Review script").
    - `oled_grey` reproduces the grey render path of `SteamiMemoryBackend` and `writePpm`: pack the pixel to RGB565 and expand it (`(r5 << 3) | (r5 >> 2)`, `(g6 << 2) | (g6 >> 4)`), level = `SteamiRgb::gray4()` of that (`(r·77 + g·150 + b·29) >> 8 >> 4`), then pack `level · 17` on every channel to RGB565 and expand it again. So the ref's greys are exactly the render's, green tint included.
  - `@dataclass(frozen=True) class Row: name: str; size: int; score: float`.
  - `review(ppm_dir: Path, out_dir: Path, refs_dir: Path | None) -> list[Row]`, which writes:
    - `<out>/<name>-<size>.png`: ref, render and absolute difference side by side, all masked;
    - `<out>/ssim.csv`;
    - `<out>/index.html`: a table with the three images and the score, "look" next to scores under `SSIM_HINT`.
  - `main(argv: list[str] | None = None) -> int`: `--ppm-dir`, `--out`, optional `--refs`. Pillow and NumPy are imported inside the functions. The exit code:
    - 2 with a message if Pillow or NumPy is missing;
    - 1 if a PPM or a ref is missing or cannot be fetched;
    - 0 otherwise, whatever the scores.

- [ ] **Step 1: Write the failing tests** (load the script with `spec_from_file_location("mockup_review", SCRIPT)`, `sys.modules[spec.name] = module` before `exec_module`, as the 3b plan's footprint test)

```python
def test_ref_url_is_pinned(self) -> None:
    # ref_url("01_temperature") == "https://raw.githubusercontent.com/steamicc/micropython-steami-"
    # "tutorials/83d8e2ca23abb0eeca7afca2d001cdd95b1df76b/docs/mockups/01_temperature_ref.png"
def test_ppm_name(self) -> None:
    # ppm_name("02_battery", 240, "color") == "steami-mockup-02_battery-240-color.ppm"
@skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
def test_ssim_identity_symmetry_and_noise(self) -> None:
    # rng = np.random.default_rng(0); a = rng.integers(0, 256, (64, 64, 3), dtype=np.uint8);
    # b = clip(a + rng.normal(0, 40, a.shape), 0, 255).astype(uint8):
    # ssim(a, a) == 1.0 (places 6); ssim(a, b) == ssim(b, a); ssim(a, b) < 0.9
@skipUnless(HAVE_SKIMAGE, "scikit-image")
def test_ssim_matches_scikit_image(self) -> None:
    # structural_similarity(a, b, channel_axis=-1) within 1e-6
@skipUnless(HAVE_NUMPY_PIL, ...)
def test_oled_grey_levels(self) -> None:
    # oled_grey of (85, 85, 255) -> (99, 101, 99) (level 6, 102 through RGB565 as writePpm expands it);
    # (51, 51, 51) -> (49, 48, 49) (level 3); white -> (255, 255, 255)
@skipUnless(HAVE_NUMPY_PIL, ...)
def test_disc_mask(self) -> None:
    # disc_mask(128, 0.95) (indexed [y, x], centre 64, radius 60.8): [64, 64] True, [0, 0] False,
    # [2, 64] False (dy -62), [4, 64] True (dy -60)
@skipUnless(HAVE_NUMPY_PIL, ...)
def test_review_writes_the_page_and_never_fails_on_scores(self) -> None:
    # tmp refs dir with three 384x384 PNGs and tmp PPM dir with six PPMs (synthetic, unrelated
    # images): main([...]) == 0; index.html, ssim.csv with 6 rows, 6 PNGs exist
@skipUnless(HAVE_NUMPY_PIL, ...)
def test_missing_ppm_fails(self) -> None:
    # one PPM removed: main([...]) == 1
```

Run: `make test-tooling` — Expected: FAIL (script missing).

- [ ] **Step 2: Implement `scripts/mockup-review.py`** per the Interfaces block and the decision.

- [ ] **Step 3: Verify locally**

Run: `make test-tooling` — Expected: `OK` (the system `python3` has Pillow and NumPy; skimage is absent, so one test is skipped).
Run (Task 7's `$SCRATCH/mockups`; the tutorials clone is read-only, never checked out or pulled): `python3 scripts/mockup-review.py --ppm-dir "$SCRATCH/mockups" --out "$SCRATCH/mockup-review" --refs /home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups` — Expected: exit 0, six PNGs, `index.html`, six scores. Run it once without `--refs` when network access allows — Expected: the same scores (the clone's HEAD is the pinned commit). Note the scores for the PR: indicative, about 0.6 to 0.8 expected.

- [ ] **Step 4: Add the CI job** in `.github/workflows/tests.yml`. Job `mockup-review` on `ubuntu-latest`, with these steps:
  1. `actions/checkout@v4`, as every other job;
  2. a second `actions/checkout@v4` with `repository: steamicc/micropython-steami-tutorials`, `ref: 83d8e2ca23abb0eeca7afca2d001cdd95b1df76b`, `sparse-checkout: docs/mockups`, `path: tutorials`;
  3. `actions/setup-python@v5` with `python-version: '3.12'`;
  4. the PlatformIO cache step copied from job `native`;
  5. step "Render the mockups" (strict): `env: STEAMI_PPM_DIR: ${{ runner.temp }}/steami-mockups`, running `mkdir -p "$STEAMI_PPM_DIR"`, `make install-pio` and `.venv/bin/pio test -e native -f native/test_steami_screen_mockups`;
  6. step "Score them against the tutorials", with `continue-on-error: true` (a review tool, never a gate): `python -m pip install pillow==12.1.1 numpy==2.3.5`, then `python scripts/mockup-review.py --ppm-dir "${{ runner.temp }}/steami-mockups" --refs tutorials/docs/mockups --out "${{ runner.temp }}/steami-mockup-review"`, then ``{ echo '```'; cat "${{ runner.temp }}/steami-mockup-review/ssim.csv"; echo '```'; } >> "$GITHUB_STEP_SUMMARY"``;
  7. `actions/upload-artifact@v4` with `if: always()`, `name: steami-mockup-review`, `path: ${{ runner.temp }}/steami-mockup-review`, `if-no-files-found: warn`.

  In `scripts/tests/test_mockup_review.py`, add `test_ci_pins_the_tutorials_commit`: `.github/workflows/tests.yml` contains `"ref: " + TUTORIALS_COMMIT` (text search, no YAML parser).

  Run: `make test-tooling` — Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add scripts/mockup-review.py scripts/tests/test_mockup_review.py
git commit -m "feat(tooling): Score the mockup renders against the tutorials."
git add .github/workflows/tests.yml scripts/tests/test_mockup_review.py
git commit -m "ci(ci): Upload the mockup review page as an artefact."
```

---

### Task 9: Package check, README, keywords and full verification

**Files:**
- Modify: `scripts/tests/test_build_arduino_package.py` (`test_umbrella_includes_display_headers`)
- Modify: `lib/steami_board/README.md`, `lib/steami_board/keywords.txt`

**Interfaces:**
- Consumes: the public API of `SteamiScreen.h` and `SteamiValue.h`; the E30/E31 texts (`SteamiDisplay.cpp` lines 10-15) with `who` = `SteamiScreen`.
- Produces: the README `## Screen` section, keywords, and the PR figures.

- [ ] **Step 1: Pin the umbrella content.** Add `"SteamiScreen.h"`, `"SteamiValue.h"` and `"SteamiScreenConst.h"` to the header tuple of `test_umbrella_includes_display_headers`. The umbrella already takes every header, so this test pins the behaviour instead of failing first. Run: `make test-tooling` — Expected: `OK`. Then `make package` and `grep -c 'SteamiScreen.h\|SteamiValue.h\|SteamiScreenConst.h' build/arduino-package/src/STeaMi.h` — Expected: `3`.

- [ ] **Step 2: Commit**

```bash
git add scripts/tests/test_build_arduino_package.py
git commit -m "test(tooling): Check the screen headers in STeaMi.h."
```

- [ ] **Step 3: Write the docs**

README, a new `## Screen` section after `## Display`:
- A short sketch: 01's program.
- What `SteamiScreen` adds to `SteamiDisplay` (it is one, with widgets placed for the round screen on both panels).
- A widget table, one row per public 4a member with its signature and default colour: `title`, the three `subtitle` forms (3 lines at most), the two `value` forms, `setDecimals` (default 1, at most 4), the two `bar` forms, `text(s, at, c, scale)`, `maxChars()`.
- `SteamiAt` (the nine positions).
- `SteamiValue`: the accepted types (overload rules of 5.7, `long long` not accepted), NAN, INF and too-large floats show "--".
- The layout rules a student meets:
  - same sketch on 128 and 240 px;
  - text never leaves the circle: it is cut, no ellipsis, or `value()` drops to a smaller size;
  - titles of up to 11 characters stay at the top; the examples keep to that;
  - W and E values share one size;
  - `yOffset` is in the units of the 128 px screen.
- Immediate mode: `clear()`, widgets in call order, `show()`.
- The 4a rows of the vocabulary table of 5.7: `screen = Screen(display)`, `value`, `subtitle`, `bar`, `text`, the position and colour names, `center`/`radius`/`width`.
- Contrast: on the V1a/V1b grey screen a bar fill is lifted to at least 4 grey levels above its track (BLUE becomes visible); primitives never adjust colours.
- The examples ScreenTemperature, ScreenBattery and ScreenComfort, with the tutorials' mockup each one reproduces.
- Display messages table: add the `SteamiScreen:` form of E30 and E31.
- Testing block: `make test-native/steami_screen`, `make test-native/steami_screen_mockups`, and the `scripts/mockup-review.py` command of Task 8 with `STEAMI_PPM_DIR`.

`keywords.txt` (tab-separated, in the existing blocks; skip names already there): KEYWORD1 `SteamiScreen`, `SteamiAt`, `SteamiValue`; KEYWORD2 `title`, `subtitle`, `value`, `setDecimals`, `bar`, `maxChars`, `format`.

- [ ] **Step 4: Commit**

```bash
git add lib/steami_board/README.md lib/steami_board/keywords.txt
git commit -m "docs(board): Document SteamiScreen and its layout widgets."
```

- [ ] **Step 5: Full verification before the PR**

Run each; all must succeed:
- `make test-native`: every native suite PASSED, `test_steami_screen` and `test_steami_screen_mockups` included.
- `.venv/bin/pio test -e native_asan`: PASSED for the four display and screen suites, no `runtime error:`, no ASan or LSan report.
- `python3 tests/native/test_steami_screen/golden_layout.py`: exit 0. Its printed tables equal the values pinned in `test_main.cpp` (spot-check five).
- `make lint`: clang-format, SPDX and clang-tidy clean.
- `make build`: `SUCCESS`.
- `make test-examples`: every example PASS, the three `Screen*` included.
- `make test-tooling`: `OK`.
- `make package`, then `ls build/arduino-package/examples/STEAMI_BOARD/{ScreenTemperature,ScreenBattery,ScreenComfort}/*.ino`: three files.
- `.venv/bin/python3 scripts/check-footprint.py`: exit 0.
- Layer cost, a figure for the PR, not a gate: build `ScreenComfort` and `Drawing` with `PLATFORMIO_SRC_DIR=lib/steami_board/examples/<Name> .venv/bin/pio run -e steami` and record each `Flash: … used` and `RAM: … used` figure (context only).
  - Measure the layer from the objects: after the ScreenComfort build, run `arm-none-eabi-size` (the tool `find_size_tool()` of `scripts/check-footprint.py` finds) on `SteamiScreen.cpp.o`, `SteamiValue.cpp.o`, `SteamiDisplay.cpp.o`, `SteamiFont.cpp.o` and `SteamiTrig.cpp.o` (`find .pio/build/steami -name '<Name>.cpp.o'`). Report text + rodata, 4a objects apart from 3a's, as an upper bound against the spec's "about 8 to 12 KB" for the whole layer.
  - ScreenComfort's RAM may exceed Drawing's only by the sensor objects: `SteamiScreen` has no buffer.
- Widget stack, a figure for the PR against the spec's 64 B: rebuild ScreenComfort with `PLATFORMIO_BUILD_FLAGS=-fstack-usage` and read the frame sizes of `SteamiScreen::value`, `SteamiScreen::subtitle`, `SteamiScreen::text` and `SteamiValue::format` from the `.su` files next to those objects. Give them in the PR; a frame over 64 B is named there for the maintainer.
- Optional, when network access allows: arduino-cli in a scratch data directory, as the 3b plan's Task 6. Otherwise the `arduino-package` CI job is the check.
- `npx commitlint --from origin/main --to HEAD`: no error. `git log --format=%s origin/main..`: every header ≤ 72 characters, scopes `board|tests|tooling|ci|docs`, no `!`, no `[skip ci]`, no attribution lines.

---

### Task 10: Hardware acceptance on the V1c

**Controller-run on hardware, needs a board slot; subagents never flash or open the serial port.**

**Files:** none committed.

**Interfaces:**
- Consumes:
  - the three examples (Task 7);
  - the 240 px renders `steami-mockup-*-240-color.ppm` (Task 7) and the review page (Task 8);
  - the refs `01_temperature_ref.png`, `02_battery_ref.png`, `03_comfort_dual_ref.png`;
  - the webcam that films the V1c.
- Produces: for the PR, three photos next to their `_ref.png` and their 240 px render, and the findings.

- [ ] **Step 1: References.** Render the PPMs with `STEAMI_PPM_DIR` (Task 7, Step 3) and convert the three 240 px files to PNG. Keep the review page of Task 8.

- [ ] **Step 2: Photos.** Flash `ScreenTemperature`, `ScreenBattery` and `ScreenComfort` in turn on the V1c with PlatformIO `env:steami` and photograph each screen with the webcam. Expected, allowing for the camera, the live sensor values and the accepted differences of spec section 6 (fonts, unit under the value, "45.3", chip names, bar track `#333`):
  - the same layout as the 240 px render: title at the top, value block centred, unit under it, two-line subtitle, bar rows, W/E values with their labels, the separator;
  - the same colours as the `_ref.png`;
  - no text cut by the bezel;
  - every element of the 240 px render is present, whole and inside the visible disc; the DARK subtitle (#666666) is expected to be visible.

  A V1c mismatch blocks the PR and goes back to the owning task (decision 6).

- [ ] **Step 3 (optional, user-assisted): Missing sensor.** The shared V1c normally runs on its battery, which powers the BQ27441, and the controller cannot unplug it. If the user is available: ask them to unplug the battery, restart ScreenBattery on USB only and photograph it. Expected: "0" with an empty bar and "--" on the first subtitle line (spec section 6 and Review Focus 1). Then ask them to plug the battery back, restart the sketch so the gauge answers again, and leave the board in its usual state at the end of the slot (battery fitted, its usual `battery_status` program back on it). If the user is not available, skip this step and say so in the PR: native `test_mockups_without_sensors` covers it, and spec section 7 does not require it on hardware.

- [ ] **Step 4: PR text.** It contains:
  - "Closes #271";
  - the three photos with their `_ref.png` and 240 px renders, and the SSIM scores of the review page (indicative, not a gate);
  - the footprint, layer-cost and widget-stack figures of Task 9;
  - whether Step 3 ran on hardware or was skipped;
  - that the V1a/V1b check was not run on hardware (decision 6; covered by the grey 128 px renders and native tests);
  - the reading of "both cores" as PlatformIO and arduino-cli, both on STM32duino 3.1.0, as in 3b;
  - the examples include `<SteamiScreen.h>`, because `<STeaMi.h>` exists only in the package;
  - example 02 rounds the voltage with `lroundf`: a spec correction for the maintainer (24 values from 3000 to 4299 mV printed 1 mV low), with section 6 of the design doc updated in the same PR.

---

## Out of scope

- 4b and 4c widgets: `gauge`, `compass`, `watch`, `face`, `SteamiFace` and its bitmaps, `graph`, `menu`, `SteamiHistory`; their contrast cases (gauge fill, selected menu item).
- Mockups 04 to 09, their examples and native renders; `WeatherStation` and its place in `SKETCHES`; the full widget reference and vocabulary table of the README (4c).
- The wiki pages (Arduino screen page, E30/E31 in the French error table) and every follow-up of spec section 12 in MicroPython and the tutorials.
- Issue #269 (MicroPython GC9A01 wrapper).
- V1a/V1b hardware checks (decision 6).
- Changes to `lib/ssd1327`, `lib/gc9a01`, `requirements.txt` and the Makefile; a big-digits font, partial refresh, DMA.
