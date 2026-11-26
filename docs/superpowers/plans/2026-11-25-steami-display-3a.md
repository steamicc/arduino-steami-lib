# STeaMi display, delivery 3a (colours and drawing in memory) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the colour type `SteamiRgb` (and `SteamiColor::rgb()`), the 8x8 and 5x7 fonts with UTF-8, integer trigonometry, the display backend interface and `SteamiDisplay`, drawn on an injected in-memory backend, clipped and hole-free, tested natively and under ASan/UBSan, with a PPM gallery in the CI artefacts. No panel, no hardware.

**Architecture:**
- Header-only `SteamiRgb` (RGB565 inside, grey level on demand). Pure functions in `SteamiFont` and `SteamiTrig`. One virtual interface, `SteamiDisplayBackend`.
- `SteamiDisplay` holds only a board pointer, a message name and a status. It draws through `SteamiBoard::display()`, which in 3a returns the backend injected with `setDisplayBackend()` (3b adds the two panel slots behind the same three board methods).
- Every primitive clips in integers before calling the backend. Filled shapes, arcs and text are horizontal spans (`fillRect` of height 1 or a glyph cell), never per-pixel.
- Tests draw into `SteamiMemoryBackend` (test-only, `tests/native/helpers/`), at 128 and 240 px, colour and grey. Its out-of-bounds counter must stay 0.

**Tech Stack:** C++17 (`-std=c++17` native, gnu++17 on target), PlatformIO `env:native` + new `env:native_asan`, Unity, Python 3 (`unittest`) for the compile-fail check, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-25-steami-display-design.md`: sections 5.1 to 5.6, 5.10 to 5.13 and 7 (suite 1), as they apply to row 3a of section 8; decision 6 of section 9. `SteamiColor::rgb()` was promised by the functional-layer spec, section 5.3.

## Global Constraints

Copied from the design document (verbatim):

- "No heap allocation in the layer itself (drivers keep their own buffers), no `String`, no RTTI, no exceptions."
- "Never hand an unclipped coordinate to a driver: `SSD1327::pixel(uint8_t, uint8_t, ...)` has no bounds check."
- "Never call the drivers' `line()` or `text()`: they do not clip and use another font."
- "Never use `double` arithmetic or `sin()`/`cos()` in drawing code (single-precision FPU; integer tables)."
- "No student-visible second font, scale factor or unit system: the 5x7 font, `k` and 128-px units are internal."
- "Integer drawing: Bresenham, scanlines, integer square root, a quarter-wave sine table. `float` is accepted at widget inputs (values, ratios) on the single-precision FPU; literals carry the `f` suffix."
- "Both cores: STM32duino 2.12 (PlatformIO) and 3.1.0 (Arduino IDE), `-std=gnu++17 -fno-rtti -fno-exceptions -fno-threadsafe-statics`. Do not rely on `printf("%f")` (newlib-nano does not link float formatting by default) nor on `dtostrf` (core-specific, absent on native)." (The PlatformIO env now builds on core 3.1.0 too, see the decisions.)
- "Packaging: flattened `src/`, generated umbrella `STeaMi.h`, unique file names (case-insensitive), no `Steami.h`; no `STEAMI_BOARD_*` in any library `.cpp`; headers carry no macro-dependent inline code."
- "Native tests (`tests/native`, Unity, SPI/Wire mocks) must cover everything except the look on the real panel and the refresh time."
- "**Coordinates are physical pixels** of the panel (decision 1), `int16_t`, origin top-left, x right, y down. `centerX() = width()/2`, `centerY() = height()/2`, `radius() = min(width, height)/2` (64 and 120)."
- "**Clipping** is done in `int16_t` (with `int32_t` intermediates) before any backend call: rectangles are intersected with the panel; `pixel()` off-panel is dropped. No circular mask."
- "Primitives never adjust colours: `rect(…, STEAMI_BLUE)` on a `STEAMI_DARK` background stays invisible on the OLED; the documentation says so."
- Decision 6: "**The V1c comes first.** [...] the 128 px OLED of V1a/V1b is best effort. [...] when a rule cannot serve both panels equally, the V1c wins, and a V1a/V1b defect does not block a delivery. V1a/V1b are not checked on hardware."
- Scope (section 8, 3a): only `lib/steami_board` (code, README, `keywords.txt`), `tests/native/test_steami_display/`, `tests/native/helpers/SteamiMemoryBackend.h`, `platformio.ini`, `.github/workflows/`, and the compile-fail check under `scripts/`. "No hardware."

Repository conventions (CONTRIBUTING.md, commitlint, `.clang-format`, `.clang-tidy`):

- Branch `feat/steami-display-memory` from `main` (v0.34.0). One issue and one PR for the delivery.
- Commits: `type(scope): Sentence starting with a capital and ending with a period.`, header at most 72 characters (commitlint allows 78; this delivery keeps 72). Scopes used: `board`, `tests`, `tooling`, `ci`. No `!`, no `BREAKING CHANGE`, no `[skip ci]`, no attribution lines.
- Every `.h/.cpp` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`; `#pragma once`; Google style, indent 4, 100 columns (`make format-fix`); clang-tidy `WarningsAsErrors: '*'` (`make lint`).
- Run `make setup` in the worktree first: it creates `.venv/bin/pio`, clang-format and clang-tidy, and installs the husky hooks, whose pre-commit step runs clang-format and the SPDX check on every staged file (so no task repeats them).
- Every test builds its display objects on the file-local `board` (`SteamiDisplay d(board)`, `TestDisplay d(board)`), never on the default `Steami`, except the one test that covers the default argument.
- A task that adds a protected member of `SteamiDisplay` exposes it in the test fixture `TestDisplay` with a `using` line in the same task.
- Never call unqualified `min`/`max`/`abs` in library code: the STM32duino core defines global `min`/`max` templates that the native mock does not, so code would build on one side only. Use local helpers in an anonymous namespace.

## Decisions taken while planning (the design doc leaves them open)

- **Board hook in 3a, slots in 3b.** Row 3a says "`SteamiDisplay` drawn on an injected backend", but `setDisplayBackend()`, `display()`, `beginDisplay()` and E30/E31 are listed in row 3b. 3a adds them in their final signatures, minus the panel slots: `display()` returns the injected backend or `nullptr`; `beginDisplay()` gives `UNKNOWN_BOARD` without a backend (E30), `NO_MEMORY` when its `begin()` fails (E31), `OK` otherwise. 3b adds the two `SteamiLazy` slots and the routing on `SteamiFunction::SCREEN` inside `display()`. Until then a sketch on a real board gets E30; 3a documents no primitive in the README (row 3b does).
- `setDisplayBackend()` is ignored once `beginDisplay()` has run, until `end()`. `end()` forgets the injected backend and the cached display status (tests inject a stack object per test).
- `SteamiDisplay` re-checks `_board->beginDisplay()` (cached, no cost) on every drawing call, so drawing after `Steami.end()` does nothing, and a backend injected after `end()` and before any draw or `beginDisplay()` is begun before use. A draw with no backend caches `UNKNOWN_BOARD` and so locks injection until the next `end()`.
- `fillRect` with `w <= 0` or `h <= 0` counts as out of bounds in the memory backend: the layer drops empty spans itself.
- `hline`/`vline`/`rect` with a length `<= 0`, `circle` with `r < 0`, `arc` per its pseudo-code: draw nothing. `circle(…, 0)` is one pixel. A text `scale` of 0 counts as 1 (as B9 does for `value()`). `nullptr` text draws nothing, `textWidth(nullptr) == 0`, widths saturate at 32767.
- Squared distances in `arc()` and the edge interpolation product in `triangle()` use `int64_t` (radii and coordinates up to 32767 overflow `int32_t` there); everything else stays `int32_t`.
- UTF-8 (refines 5.3's "any invalid byte sequence draws `?`" to one `?` per byte that cannot start a valid sequence):
  - Lead bytes `C2..DF`, `E0..EF` and `F0..F4` start 2-, 3- and 4-byte sequences. The second byte must be `A0..BF` after `E0`, `80..9F` after `ED`, `90..BF` after `F0`, `80..8F` after `F4`, and `80..BF` after any other lead byte; every later byte must be `80..BF` (Unicode Table 3-7). These ranges reject overlong forms, surrogates (U+D800..DFFF) and values above U+10FFFF.
  - The decoder checks the needed bytes one at a time and stops at the first that breaks the rule; the terminating NUL breaks it, so it never reads past the end. The lead byte then gives one `?` and only the lead byte is consumed; decoding resumes at the next byte, so each stray continuation byte gives its own `?`.
  - Any other non-ASCII byte (`80..C1`, `F5..FF`) gives one `?` and consumes one byte.
  - A valid sequence for a code point outside the folding table gives one `?` and consumes the whole sequence.
  - Control characters (< 32, and 127) give `?`.
- Folding table: U+00E0 à, E2 â, E4 ä → a; E7 ç → c; E8 è, E9 é, EA ê, EB ë → e; EE î, EF ï → i; F4 ô, F6 ö → o; F9 ù, FB û, FC ü → u; FF ÿ → y; the capitals C0, C2, C4 → A; C7 → C; C8, C9, CA, CB → E; CE, CF → I; D4, D6 → O; D9, DB, DC → U; **U+0178 Ÿ** (not Latin-1, `C5 B8`) → Y; U+00B0 → degree glyph.
- Degree glyph, code 127 in both tables (replaces MicroPython's checkerboard 127): 8x8 `{0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00}`, 5x7 `{0x00, 0x06, 0x09, 0x09, 0x06}`.
- 8x8 source: MicroPython `extmod/font_petme128_8x8.h` at commit `b0a1b60a9b425cdb24e36d71974e902338ffb059` (MIT, "Copyright (c) 2013, 2014 Damien P. George"), glyphs 32..126 copied unchanged. 5x7 source: `kFont5x7` of `lib/gc9a01/src/SteamiGC9A01.cpp`, glyphs 32..126.
- Stroke brush of width `w`: the `w×w` square with its top-left corner at `(x − (w − 1)/2, y − (w − 1)/2)` for each Bresenham point (even widths extend right and down).
- Filled triangles are spans only (no outline added). Filled circles take, per row, the extent of the midpoint outline, so a filled circle covers its outline.
- `SteamiColor::rgb()` rounds each scaled channel to nearest: `(c·255 + max/2) / max`.
- The ASan/UBSan build is a separate env `native_asan` (extends `native`, filter `native/test_steami_display*`, so 3b's suite joins it), added in Task 1 and run by every task from then on. The compile-fail check is a `unittest` in `scripts/tests/`, run by `make test-tooling` and the existing "Test the packaging tooling" CI step.
- PPM files are written only when the environment variable `STEAMI_PPM_DIR` is set (CI sets it), so local runs leave no files.
- Interfaces for later deliveries, added now: a protected `glyph()` window renderer (4a draws '.' and ':' from columns 2 to 5 with it), `steamiRoundQ14()` (4b), the gallery scene (3b's `Drawing` example copies it).

## Review Focus

1. **Coordinates at the `int16_t` limits** (−32768, 32767), huge radii, and rectangles whose `x + w` overflows `int16_t`: the visible part is drawn exactly (an `int16_t` wrap of `x + w` must fail a test, not only a sanitizer), nothing reaches the backend outside the panel, UBSan stays silent under `native_asan`, and the call returns in bounded time. Tests: `test_extreme_spans` (Task 6), `test_extreme_circles_and_triangles` (Task 7), `test_extreme_arcs` (Task 8).
2. **Truncated or invalid UTF-8** (`"A\xC3"`, `"\xE2\x82"`, overlong `"\xE0\x81\x81"`, surrogates): the decoder never reads past the NUL, never turns an overlong form into a letter, and draws one `?` per bad byte. Tests: `test_utf8_invalid_bytes`, `test_utf8_truncated_sequences` (Task 3), `test_text_invalid_utf8_draws_question_marks` (Task 9).
3. **Drawing after `Steami.end()`, before any backend, or from two objects**: no crash, nothing drawn, one `begin()` of the backend, one message per (class, code). Tests: `test_drawing_after_board_end_does_nothing`, `test_two_objects_share_one_backend` (Task 5).
4. **Arc angles outside 0..359 and sweeps ≥ 360 or ≤ 0**: normalised modulo 360, a full ring from 360 up, nothing at 0 or below. Test: `test_arc_angles_normalised` (Task 8).
5. **Degenerate shapes**: zero or negative lengths, collinear or single-point triangles, `r = 0`, text scale 0: the documented minimum or nothing, never an empty `fillRect`. Tests: `test_degenerate_spans` (Task 6), `test_degenerate_circles_and_triangles` (Task 7), `test_text_scale_zero_is_one` (Task 9).

## File map

| File | Responsibility | Task |
|---|---|---|
| `lib/steami_board/src/SteamiRgb.h` | `SteamiRgb`, the ten `STEAMI_*` colours | 1 |
| `lib/steami_board/src/SteamiColor.h/.cpp` | + `SteamiRgb rgb()` | 1 |
| `lib/steami_board/src/SteamiTrig.h/.cpp` | Q14 sine/cosine table, rounding, integer square root | 2 |
| `lib/steami_board/src/SteamiFont.h/.cpp` | 8x8 and 5x7 tables, UTF-8 decoding, folding | 3 |
| `lib/steami_board/src/SteamiDisplayBackend.h` | the backend interface | 4 |
| `lib/steami_board/src/SteamiTypes.h` | + E30, E31 constants | 4 |
| `lib/steami_board/src/SteamiBoard.h/.cpp` | + `display()`, `setDisplayBackend()`, `beginDisplay()`, `end()` | 4 |
| `lib/steami_board/src/SteamiDisplay.h/.cpp` | primitives, clipping, text, messages | 5 to 9 |
| `lib/steami_board/README.md`, `keywords.txt` | `SteamiRgb`, colours, `rgb()` | 1 |
| `tests/native/helpers/SteamiMemoryBackend.h` | test backend, counters, PPM | 4 |
| `tests/native/test_steami_display/test_main.cpp` | suite 1 of section 7 | 1 to 10 |
| `tests/native/test_steami_display/golden_colors.py` | the 10-line golden script | 1 |
| `scripts/tests/test_steami_display_rules.py` | compile-fail check (no integer → colour) | 1, 5 |
| `platformio.ini` | `[env:native_asan]` | 1 |
| `.github/workflows/tests.yml` | sanitizer job, PPM artefact | 10 |

---

### Task 1: Colour type `SteamiRgb` and `SteamiColor::rgb()`

**Files:**
- Create: `lib/steami_board/src/SteamiRgb.h`
- Modify: `lib/steami_board/src/SteamiColor.h`, `lib/steami_board/src/SteamiColor.cpp`
- Modify: `lib/steami_board/README.md` (new "Colours" section after "Function classes"; `SteamiColor` row of the reads table), `lib/steami_board/keywords.txt`
- Create: `tests/native/test_steami_display/test_main.cpp`, `tests/native/test_steami_display/golden_colors.py`
- Create: `scripts/tests/test_steami_display_rules.py`
- Modify: `platformio.ini` (new `[env:native_asan]` after `[env:native]`)

**Interfaces:**
- Consumes: `SteamiColor::read(uint16_t&, uint16_t&, uint16_t&)`; `steamiFakeApds9960(TwoWire&, clear, red, green, blue)` from `tests/shared/steami_fake_chips.h`.
- Produces (exactly the design's 5.2 class, 4-space indent):
  - `class SteamiRgb` with `constexpr SteamiRgb(uint8_t r, uint8_t g, uint8_t b)` (565 packing `((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`, as `GC9A01::color565`), `static constexpr SteamiRgb gray(uint8_t level0to15)`, `static constexpr SteamiRgb fromRgb565(uint16_t v)`, `constexpr uint16_t rgb565() const`, `constexpr uint8_t gray4() const`, `constexpr bool operator==/!=(SteamiRgb) const`. No default constructor and no one-argument constructor reachable from an integer (`fromRgb565` uses a private tag-type constructor).
  - `constexpr SteamiRgb STEAMI_BLACK, STEAMI_DARK, STEAMI_GRAY, STEAMI_LIGHT, STEAMI_WHITE, STEAMI_GREEN, STEAMI_RED, STEAMI_BLUE, STEAMI_YELLOW, STEAMI_GRID_DARK` with the 5.2 values.
  - `SteamiRgb SteamiColor::rgb();` (`SteamiColor.h` includes `SteamiRgb.h` only, no display header).
  - Test scaffold reused by every later task: `board`, `g_lines`, `capture`, `countLogged(suffix)`, `assertStatus`, `setUp`/`tearDown` as in `tests/native/test_steami_functions/test_main.cpp`.
  - Later users: 3b's SSD1327 backend converts with `SteamiRgb::fromRgb565(v).gray4()`.

- [ ] **Step 1: Write the golden script and the failing tests**

`golden_colors.py` (SPDX comment on line 1, prints the table the test pins):

```python
# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden RGB565 and grey levels of the STeaMi colours (colors.py rule). Run: python3 golden_colors.py"""
C = {"BLACK": (0, 0, 0), "DARK": (102,) * 3, "GRAY": (153,) * 3, "LIGHT": (187,) * 3, "WHITE": (255,) * 3,
     "GREEN": (119, 255, 119), "RED": (255, 85, 85), "BLUE": (85, 85, 255), "YELLOW": (255, 255, 85), "GRID_DARK": (51,) * 3}
for name, (r, g, b) in C.items():
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    r5, g6, b5 = v >> 11, (v >> 5) & 63, v & 31
    R, G, B = (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)
    print(f"{name:10} 0x{v:04X} {((R * 77 + G * 150 + B * 29) >> 8) >> 4}")
```

Tests in `test_main.cpp` (names and assertions):

```cpp
static void test_rgb565_of_every_colour(void) {   // values printed by golden_colors.py
    TEST_ASSERT_EQUAL_HEX16(0x0000, STEAMI_BLACK.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x632C, STEAMI_DARK.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x9CD3, STEAMI_GRAY.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xBDD7, STEAMI_LIGHT.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, STEAMI_WHITE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x77EE, STEAMI_GREEN.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFAAA, STEAMI_RED.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x52BF, STEAMI_BLUE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFFEA, STEAMI_YELLOW.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x3186, STEAMI_GRID_DARK.rgb565());
}
static void test_gray4_of_every_colour(void) {
    // BLACK 0, GRID_DARK 3, DARK 6, GRAY 9, LIGHT 11, WHITE 15, GREEN 12, RED 8, YELLOW 14, BLUE 6
}
static void test_gray_levels_round_trip(void) {
    // for level 0..15: SteamiRgb::gray(level).gray4() == level; gray(16) == gray(15) == STEAMI_WHITE;
    // gray(3) == STEAMI_GRID_DARK; gray(6) == STEAMI_DARK (102 = 6·17)
}
static void test_from_rgb565_and_equality(void) {
    // fromRgb565(0xFAAA) == STEAMI_RED; fromRgb565(v).rgb565() == v for v in {0, 0x1234, 0xFFFF};
    // STEAMI_BLUE != STEAMI_DARK although both are grey 6
}
static_assert(STEAMI_RED.gray4() == 8, "gray4 is constexpr");
static_assert(sizeof(SteamiRgb) == 2, "one RGB565 word");

static void test_color_rgb_scales_largest_channel(void) {
    steamiFakeApds9960(Wire, 1234, 100, 200, 300);
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == SteamiRgb(85, 170, 255));
}
static void test_color_rgb_rounds_to_nearest(void) {
    steamiFakeApds9960(Wire, 1, 100, 50, 0);
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == SteamiRgb(255, 128, 0));   // 127.5 rounds up (g6 32, not 31)
}
static void test_color_rgb_zero_is_black(void) {   // counts 0, 0, 0
}
static void test_color_rgb_unavailable_is_black(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == STEAMI_BLACK);
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E20)"));
}
```

`scripts/tests/test_steami_display_rules.py`, modelled on `scripts/tests/test_steami_board_rules.py` (`c++ -std=c++17 -fsyntax-only`, `-I tests/native/helpers` and every `lib/*/src`, skip when no `c++`). Class `SteamiDisplayRulesTest`, helper `_compiles(body: str) -> bool` that wraps `body` in `#include "SteamiRgb.h"` and `void f(SteamiRgb) {} int main() { ... }`:
- `test_integer_does_not_convert_to_a_colour`: `SteamiRgb c = 15;`, `f(15);`, `f(0);`, `SteamiRgb c{15};` each fail.
- `test_explicit_forms_compile` (positive control): `f(SteamiRgb::gray(15)); f({255, 128, 0}); f(STEAMI_BLUE);` compiles.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `.venv/bin/pio test -e native -f native/test_steami_display`
Expected: build error, `SteamiRgb.h: No such file or directory`.
Run: `python3 -m unittest discover -s scripts/tests -p 'test_steami_display_rules.py'`
Expected: FAIL in `test_explicit_forms_compile` (header missing).

- [ ] **Step 3: Implement `SteamiRgb.h` and `SteamiRgb SteamiColor::rgb()`**

`gray4()` expands 565 to 8 bits (`r8 = r5 << 3 | r5 >> 2`, `g8 = g6 << 2 | g6 >> 4`, `b8` like `r8`), then `((r8*77 + g8*150 + b8*29) >> 8) >> 4`. `rgb()` reads through `read()`; false or all zero gives `STEAMI_BLACK`, otherwise each channel is `(c·255 + max/2) / max` in `uint32_t`.

- [ ] **Step 4: Document and register keywords**

README: a "Colours" section with the table name / RGB / grey level from 5.2, `SteamiRgb(r, g, b)`, `{255, 128, 0}`, `SteamiRgb::gray(level)`, `rgb565()`, `gray4()`, "an integer is not a colour (`SteamiRgb::gray(15)`, not `15`)", and the rule that primitives never adjust a colour (BLUE on DARK is invisible on the OLED, 5.10). Reads table: `SteamiColor` gains `rgb()` ("hue of the object, largest channel 255"), unavailable value `STEAMI_BLACK`. `keywords.txt`: `SteamiRgb` KEYWORD1; `rgb`, `gray`, `fromRgb565`, `rgb565`, `gray4` KEYWORD2; a new "Constants (LITERAL1)" section with the ten `STEAMI_*` colours.

- [ ] **Step 5: Add the sanitizer environment**

```ini
[env:native_asan]
; The display suites under AddressSanitizer and UndefinedBehaviorSanitizer. SCons passes
; -fsanitize to both the compiler and the linker.
extends = env:native
test_filter = native/test_steami_display*
build_flags =
  ${env:native.build_flags}
  -fsanitize=address,undefined
  -fno-sanitize-recover=all
  -fno-omit-frame-pointer
```

If UBSan flags code outside the display files (a driver reached by the `rgb()` tests), stop and report it instead of fixing it in this delivery.

- [ ] **Step 6: Run the tests to verify they pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: all tests PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report.
Run: `make test-native/steami_functions` — Expected: PASS (`SteamiColor` changed).
Run: `python3 -m unittest discover -s scripts/tests -p 'test_steami_display_rules.py'` — Expected: `OK`.
Run: `python3 tests/native/test_steami_display/golden_colors.py` — Expected: the ten values of the test.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 7: Commit**

```bash
git add lib/steami_board tests/native/test_steami_display scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the SteamiRgb colour type and SteamiColor::rgb()."
git add platformio.ini
git commit -m "build(tooling): Add a native ASan and UBSan environment."
```

---

### Task 2: Integer trigonometry `SteamiTrig`

**Files:**
- Create: `lib/steami_board/src/SteamiTrig.h`, `lib/steami_board/src/SteamiTrig.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Produces (used by Task 8 here, by 4a for chords and 4b for compass and watch):
  - `int16_t steamiSinQ14(int32_t deg);` `round(16384·sin(deg°))` from a 91-entry `static const int16_t` table, any integer degree normalised modulo 360.
  - `int16_t steamiCosQ14(int32_t deg);` = `steamiSinQ14(deg + 90)` (normalise first, no overflow at `INT32_MAX`).
  - `int32_t steamiRoundQ14(int32_t v);` `v / 16384` rounded to nearest, halves away from zero (no shift of a negative number).
  - `uint32_t steamiIsqrt(uint32_t v);` `floor(sqrt(v))`, integer only.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_sin_table_matches_rounded_sine(void) {
    for (int32_t d = 0; d <= 90; d++)   // test code may use double
        TEST_ASSERT_EQUAL_INT16((int16_t)lround(16384.0 * sin(d * M_PI / 180.0)), steamiSinQ14(d));
}
static void test_sin_symmetries_and_normalisation(void) {
    // for d in -720..720: sin(-d) == -sin(d), sin(180 - d) == sin(d), sin(d + 360) == sin(d),
    // cos(d) == sin(d + 90); sin(90) == 16384, sin(270) == -16384, sin(-450) == -16384,
    // sin(INT32_MIN) and cos(INT32_MAX) return without a UBSan report (checked by native_asan)
}
static void test_round_q14(void) {
    // 8192 -> 1, 8191 -> 0, -8192 -> -1, -8191 -> 0, 16384*100 -> 100, -(16384*3 + 8192) -> -4
}
static void test_isqrt(void) {
    // exact floor for v in 0..70000 (r*r <= v < (r+1)*(r+1)); 4294967295 -> 65535; 4294836225 -> 65535
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: build error, `SteamiTrig.h` not found.

- [ ] **Step 3: Implement the four functions in `SteamiTrig.cpp`** (table generated once with `round(16384*sin(radians(i)))`, i = 0..90; quadrant folding on the normalised degree; `steamiIsqrt` by the bitwise method).

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report. This run is what checks the "UBSan silent" comments of the tests.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiTrig.* tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Add integer sine, cosine and square root for drawing."
```

---

### Task 3: Fonts and UTF-8 `SteamiFont`

**Files:**
- Create: `lib/steami_board/src/SteamiFont.h`, `lib/steami_board/src/SteamiFont.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Produces (used by Task 9; 4a reads glyph bitmaps for inked rows and the narrow '.'/':'):
  - `constexpr uint8_t STEAMI_GLYPH_DEGREE = 127;`
  - `uint8_t steamiNextGlyph(const char*& s);` decodes one glyph, advances `s`, returns a code 32..127, 0 at the end of the string (and for `nullptr`, without advancing).
  - `size_t steamiGlyphCount(const char* s);` glyphs, not bytes; 0 for `nullptr`.
  - `const uint8_t* steamiGlyph8x8(uint8_t code);` 8 column bytes, bit 0 = top row; codes outside 32..127 give the `'?'` glyph.
  - `const uint8_t* steamiGlyph5x7(uint8_t code);` 5 column bytes, 7 rows, same rule.
- `SteamiFont.cpp` header comment: the MicroPython attribution and MIT notice (copied from the source file), after the SPDX line.

- [ ] **Step 1: Write the failing tests**

```cpp
static uint32_t weightedSum(const uint8_t* (*glyph)(uint8_t), uint8_t cols) {
    uint32_t sum = 0, i = 0;
    for (uint8_t c = 32; c <= 126; c++)
        for (uint8_t k = 0; k < cols; k++) sum += ++i * glyph(c)[k];
    return sum;
}
static void test_8x8_table_is_micropython_font(void) {
    TEST_ASSERT_EQUAL_UINT32(16797326, weightedSum(steamiGlyph8x8, 8));   // petme128 @ b0a1b60, 32..126
    const uint8_t a[8] = {0x00, 0x7c, 0x7e, 0x0b, 0x0b, 0x7e, 0x7c, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(a, steamiGlyph8x8('A'), 8);
}
static void test_5x7_table_is_driver_font(void) {
    TEST_ASSERT_EQUAL_UINT32(5520505, weightedSum(steamiGlyph5x7, 5));    // kFont5x7, 32..126
}
static void test_degree_glyphs(void) {   // the two arrays of the decisions, at STEAMI_GLYPH_DEGREE
}
static void test_utf8_degree_and_folding(void) {
    // "°C" -> 127, 'C', 0; "é" -> 'e'; "É" -> 'E'; "\xC5\xB8" (Ÿ) -> 'Y'; "ÿ" -> 'y';
    // every letter of the folding table maps to its base letter; "Température" counts 11 glyphs
}
static void test_utf8_invalid_bytes(void) {
    // "\xFF" -> '?'; "\x80" "A" -> '?', 'A' (split literal: "\x80A" is one escape); "\xC0\xAF" -> '?', '?';
    // overlong "\xE0\x81\x81" -> '?', '?', '?'; overlong "\xF0\x80\x81\x81" -> four '?';
    // surrogate "\xED\xA0\x80" -> three '?'; above U+10FFFF "\xF4\x90\x80\x80" -> four '?';
    // none of these yields 'A', and steamiGlyphCount equals the number of '?';
    // valid but unsupported: "€" (E2 82 AC) -> one '?'; "\xF0\x9F\x98\x80" (U+1F600) -> one '?';
    // "\n" -> '?'; "\x7F" -> '?'
}
static void test_utf8_truncated_sequences(void) {   // Review Focus 2
    // "A\xC3" -> 'A', '?', 0 and the pointer stops on the NUL; "\xE2\x82" -> '?', '?', 0;
    // "\xF0\x9F" -> '?', '?', 0; steamiGlyphCount of each: 2, 2, 2;
    // mid-string "\xE2\x82" "A" -> '?', '?', 'A', 0 (split literal)
}
static void test_glyph_count_nullptr_and_empty(void) {   // 0 and 0
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: build error, `SteamiFont.h` not found.

- [ ] **Step 3: Implement** the two `static const uint8_t` tables (96 glyphs each, 32..126 + degree), the folding lookup, and the decoder of the decisions section.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiFont.* tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Add the 8x8 and 5x7 fonts with UTF-8 decoding."
```

---

### Task 4: Backend interface, memory backend and board hook

**Files:**
- Create: `lib/steami_board/src/SteamiDisplayBackend.h` (the 5.4 interface verbatim, 4-space indent)
- Create: `tests/native/helpers/SteamiMemoryBackend.h`
- Modify: `lib/steami_board/src/SteamiTypes.h` (two constants after `STEAMI_E_NOT_ON_THIS_BOARD`)
- Modify: `lib/steami_board/src/SteamiBoard.h` (include `SteamiDisplayBackend.h`; three public methods; two members at the end of the private block), `lib/steami_board/src/SteamiBoard.cpp` (`end()` and the new methods)
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Produces:
  - `constexpr uint8_t STEAMI_E_DISPLAY_UNKNOWN_BOARD = 30;` `constexpr uint8_t STEAMI_E_NO_MEMORY = 31;`
  - `class SteamiDisplayBackend` exactly as 5.4 (protected non-virtual destructor; `begin`, `chip`, `width`, `height`, `isColor`, `fill`, `show`, `pixel`, `fillRect`, `rotate`, `invert`, `powerOn`, `powerOff`, all pure virtual).
  - `SteamiDisplayBackend* SteamiBoard::display();` the injected backend or `nullptr` (3b: else the slot of this release, `nullptr` on UNKNOWN).
  - `void SteamiBoard::setDisplayBackend(SteamiDisplayBackend& b);` ignored once `beginDisplay()` ran, until `end()`.
  - `SteamiStatus SteamiBoard::beginDisplay();` begins `display()` once and caches `OK`, `NO_MEMORY` (begin false) or `UNKNOWN_BOARD` (no backend). Reports nothing (the caller does, with its own name).
  - Members `SteamiDisplayBackend* _displayInjected = nullptr;` and `SteamiStatus _displayStatus = SteamiStatus::NOT_STARTED;` (3b adds `SteamiLazy<SteamiSsd1327Backend>` and `SteamiLazy<SteamiGc9a01Backend>` beside them). `end()` resets both.
  - `class SteamiMemoryBackend final : public SteamiDisplayBackend` (test-only, `std::vector<uint16_t>` storage):
    - `SteamiMemoryBackend(int16_t width, int16_t height, bool color = true, bool beginResult = true);` `chip()` is `"memory"`.
    - Grey mode stores `SteamiRgb::gray(SteamiRgb::fromRgb565(v).gray4()).rgb565()` in `fill`, `pixel`, `fillRect`.
    - Any `pixel` outside the panel, or `fillRect` with `w <= 0`, `h <= 0` or any part outside, increments `outOfBounds()` and draws nothing.
    - Inspection: `uint16_t at(int16_t x, int16_t y) const` (0 outside), `bool lit(int16_t x, int16_t y) const` (`at != 0`), `size_t litCount() const`, `const std::vector<uint16_t>& pixels() const`, `uint32_t outOfBounds() const`, `uint32_t pixelCalls() const`, `uint32_t fillRectCalls() const`, `uint64_t areaDrawn() const` (pixel calls plus `w·h` of each accepted `fillRect`), `uint32_t fillCalls() const`, `uint32_t beginCalls() const`, `uint32_t showCalls() const`, `bool upsideDown() const`, `bool inverted() const`, `bool poweredOn() const`, `void clearCounters()`.
    - `bool writePpm(const char* path) const;` binary P6, `"P6\n<w> <h>\n255\n"`, each pixel expanded from 565 to 8 bits per channel as `gray4()` does.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_memory_backend_counts_out_of_bounds(void) {
    SteamiMemoryBackend mem(128, 128);
    mem.pixel(-1, 0, 0xFFFF); mem.pixel(128, 5, 0xFFFF);
    mem.fillRect(120, 0, 9, 1, 0xFFFF); mem.fillRect(0, 0, 0, 1, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(4, mem.outOfBounds());
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
}
static void test_memory_backend_grey_mode_stores_oled_levels(void) {
    SteamiMemoryBackend mem(128, 128, false);
    mem.pixel(1, 1, STEAMI_BLUE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(SteamiRgb::gray(6).rgb565(), mem.at(1, 1));
    TEST_ASSERT_FALSE(mem.isColor());
}
static void test_memory_backend_writes_ppm(void) {
    // 2x1 backend, pixel (0,0) STEAMI_RED; writePpm("steami_display_test.ppm") in the current
    // directory; read it back: header "P6\n2 1\n255\n", then bytes 255, 85, 82 (RED expanded) and
    // 0, 0, 0; remove the file
}
static void test_board_without_backend_is_unknown_board(void) {
    TEST_ASSERT_NULL(board.display());
    assertStatus(SteamiStatus::UNKNOWN_BOARD, board.beginDisplay());
    TEST_ASSERT_EQUAL_size_t(0, g_lines.size());   // the board itself reports nothing
}
static void test_board_begins_injected_backend_once(void) {
    SteamiMemoryBackend mem(240, 240);
    board.setDisplayBackend(mem);
    TEST_ASSERT_EQUAL_PTR(&mem, board.display());
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
}
static void test_board_failed_begin_is_no_memory(void) {
    SteamiMemoryBackend mem(128, 128, true, false);
    board.setDisplayBackend(mem);
    assertStatus(SteamiStatus::NO_MEMORY, board.beginDisplay());
    assertStatus(SteamiStatus::NO_MEMORY, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());   // a failed begin is cached, not retried
    TEST_ASSERT_EQUAL_size_t(0, g_lines.size());     // the board itself reports nothing
}
static void test_set_backend_after_begin_display_is_ignored(void) {
    // inject a, beginDisplay, inject b: display() is still &a, b.beginCalls() == 0
}
static void test_unknown_board_locks_injection_until_end(void) {
    // beginDisplay() == UNKNOWN_BOARD; inject a: display() still nullptr, a.beginCalls() 0;
    // board.end(); inject a again: beginDisplay() == OK, a.beginCalls() 1
}
static void test_board_end_forgets_backend_and_status(void) {
    // inject, beginDisplay, board.end(): display() == nullptr; inject again: begun again (beginCalls 2)
}
```

Also keep `tests/native/test_steami_board` green: its `sizeof(SteamiBoard) < 2560` gate and the constant-initialised `kConstantBoard`.

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: build error (`SteamiMemoryBackend.h`, `display()` unknown).

- [ ] **Step 3: Implement** the interface, the constants, the three board methods, the `end()` reset and the memory backend.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `make test-native` — Expected: every native suite PASSES (test_steami_board size gate and constant init, and the other suites that build a `SteamiBoard`).
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src tests/native/helpers/SteamiMemoryBackend.h tests/native/test_steami_display
git commit -m "feat(board): Add the display backend interface and the board hook."
```

---

### Task 5: `SteamiDisplay` core (begin, messages, geometry, pixel, control)

**Files:**
- Create: `lib/steami_board/src/SteamiDisplay.h`, `lib/steami_board/src/SteamiDisplay.cpp`
- Modify: `scripts/tests/test_steami_display_rules.py`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 `SteamiRgb`; Task 4 `SteamiBoard::display()`, `beginDisplay()`, `report()`, E30/E31.
- Produces: the whole 5.6 class declaration (all public and protected members, the later ones implemented by Tasks 6 to 9), with in this task:
  - `explicit constexpr SteamiDisplay(SteamiBoard& board = Steami) noexcept;` (who `"SteamiDisplay"`) and protected `constexpr SteamiDisplay(SteamiBoard& board, const char* who) noexcept;` (4a passes `"SteamiScreen"`).
  - `bool begin();` idempotent per object: `_status = _board->beginDisplay()`; `UNKNOWN_BOARD` reports E30, `NO_MEMORY` reports E31 through `_board->report(_who, status, detail, code)`. Details, verbatim from 5.13:
    - E30: `cannot tell if this board is a V1a/V1b or a V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>.`
    - E31: `not enough RAM for the colour screen (115200 bytes in one block). Call screen.begin() first in setup().`
  - `bool isAvailable() const; SteamiStatus status() const; const char* error() const;` (`""`, `"not started"`, `"unknown board"`, `"no memory"`).
  - `bool isColor(); int16_t width(); int16_t height(); int16_t centerX(); int16_t centerY(); int16_t radius();` from `display()` without beginning; 0 / false without a backend.
  - `void clear(SteamiRgb c = STEAMI_BLACK); void pixel(int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE); void show(); void rotate(bool upsideDown); void invert(bool on); void powerOn(); void powerOff();`
  - `SteamiDisplayBackend* backend();` returns `_board->display()` without beginning: `nullptr` without a backend or after `Steami.end()`, the backend otherwise (even after E31).
  - `static constexpr uint8_t CHAR_W = 8, CHAR_H = 8;`
  - Private gate `SteamiDisplayBackend* ready();`: `begin()` when `NOT_STARTED`; `nullptr` unless `_status == OK` and `_board->beginDisplay() == OK`; else `_board->display()`. Every drawing call, `show()` and the four controls go through it.
  - Private clipped helpers used by Tasks 6 to 9: `void dot(SteamiDisplayBackend* b, int32_t x, int32_t y, uint16_t c);` (drops off-panel) and `void span(SteamiDisplayBackend* b, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);` (intersects with the panel, drops empty results, calls `fillRect` with in-range `int16_t`).
  - Test fixture for Tasks 6 to 10: `class TestDisplay : public SteamiDisplay` with `explicit TestDisplay(SteamiBoard& b) : SteamiDisplay(b, "SteamiTestDisplay") {}` and `using SteamiDisplay::textSmall; using SteamiDisplay::textSmallWidth; using SteamiDisplay::strokeLine; using SteamiDisplay::strokeCircle;` in its public part; and `forEachPanel(void (*check)(SteamiMemoryBackend&, TestDisplay&))`, which runs `check` on 128 colour, 128 grey, 240 colour and 240 grey (each: `board.end()`, `board.setLogger(&capture)`, a fresh backend injected, a fresh `TestDisplay d(board)`) and then asserts `outOfBounds() == 0`. Helper `std::vector<uint16_t> snapshot(const SteamiMemoryBackend&)`.

- [ ] **Step 1: Write the failing tests**

```cpp
[[maybe_unused]] static constexpr SteamiDisplay kConstInitDisplay(board);   // constant-initialised
static_assert(sizeof(SteamiDisplay) <= 3 * sizeof(void*), "12 B on the 32-bit target");
static_assert(SteamiDisplay::CHAR_W == 8 && SteamiDisplay::CHAR_H == 8, "5.3");

static void test_geometry_before_begin(void) {
    // 128: width 128, height 128, centerX 64, centerY 64, radius 64; 240: width 240, height 240,
    // centerX 120, centerY 120, radius 120; 200x100: centerX 100, centerY 50, radius 50; 100x200:
    // centerX 50, centerY 100, radius 50 (board.end() and a new injection before each);
    // begin not called (beginCalls 0); a fresh object: status NOT_STARTED, error "not started",
    // isAvailable false; without a backend: width 0, radius 0, isColor false
}
static void test_no_backend_reports_e30_once(void) {
    SteamiDisplay a(board), b(board);
    TEST_ASSERT_FALSE(a.begin());
    TEST_ASSERT_FALSE(b.begin());
    assertStatus(SteamiStatus::UNKNOWN_BOARD, a.status());
    TEST_ASSERT_EQUAL_STRING("unknown board", a.error());
    TEST_ASSERT_EQUAL_size_t(1, g_lines.size());
    TEST_ASSERT_EQUAL_STRING("[STeaMi] SteamiDisplay: cannot tell if this board is a V1a/V1b or a V1c. "
                             "Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)",
                             g_lines[0].c_str());
}
static void test_failed_begin_reports_e31_and_draws_nothing(void) {
    // backend with beginResult false: begin() false, status NO_MEMORY, error "no memory", the E31
    // line verbatim; then clear(STEAMI_WHITE), pixel(1, 1), show(): fillCalls, pixelCalls,
    // fillRectCalls and showCalls stay 0, litCount() == 0
}
static void test_auto_begin_on_first_draw(void) {
    // pixel(3, 4, STEAMI_RED) without begin(): beginCalls 1, at(3, 4) == RED (colour backend);
    // then isAvailable(), status OK, error ""
}
static void test_backend_accessor(void) {
    // without a backend: backend() == nullptr; with mem injected: backend() == &mem and
    // mem.beginCalls() == 0 (no auto-begin)
}
static void test_default_board_is_steami(void) {
    // Steami.end(); Steami.setLogger(&capture); Steami.setDisplayBackend(mem); SteamiDisplay d;
    // d.pixel(1, 1): mem.lit(1, 1); then Steami.end(); Steami.setLogger(nullptr)
}
static void test_pixel_clipping(void) {   // pixel(-1, 0), pixel(200, 5) on 128: dropped, outOfBounds 0
}
static void test_primitives_never_lift_colours(void) {   // 5.10
    // 128 grey: clear(STEAMI_DARK); pixel(1, 1, STEAMI_BLUE): at(1, 1) == STEAMI_DARK.rgb565() (grey 6 on 6)
}
static void test_clear_show_and_controls_forward(void) {
    // clear(STEAMI_DARK) fills every pixel; show() -> showCalls 1; rotate(true), invert(true),
    // powerOff() then powerOn() reach the backend
}
static void test_two_objects_share_one_backend(void) {   // Review Focus 3
    // SteamiDisplay a(board); TestDisplay b(board): a.pixel(1, 1); b.pixel(2, 2); both lit on the
    // backend injected into board, beginCalls 1
}
static void test_drawing_after_board_end_does_nothing(void) {   // Review Focus 3
    // begin OK on backend A, then board.end(): pixel(), clear(), show() do not crash, width() == 0,
    // and A's fill/pixel/fillRect/show counters do not change. Those draws ran beginDisplay() with
    // no backend, which cached UNKNOWN_BOARD and locked injection: so board.end() again, inject a
    // new backend B, pixel(1, 1): B.beginCalls() == 1 and B.lit(1, 1)
}
static void test_protected_name_in_messages(void) {   // TestDisplay without backend: "[STeaMi] SteamiTestDisplay: ..."
}
```

Add to `scripts/tests/test_steami_display_rules.py`: `test_display_takes_no_integer_colour` with a helper that includes `SteamiDisplay.h`; `SteamiDisplay d; d.clear(15);` and `d.pixel(1, 1, 15);` fail, `d.clear(SteamiRgb::gray(15));` compiles.

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: build error, `SteamiDisplay.h` not found.

- [ ] **Step 3: Implement** the members listed above. Declare every public and protected member of 5.6 now and define each in its own task (6 to 9): a declared member that nothing calls yet links fine, and no empty body is written.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report.
Run: `python3 -m unittest discover -s scripts/tests -p 'test_steami_display_rules.py'` — Expected: `OK`.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiDisplay.* scripts/tests/test_steami_display_rules.py tests/native/test_steami_display
git commit -m "feat(board): Add SteamiDisplay with auto-begin, E30 and E31."
```

---

### Task 6: Spans, rectangles, lines and strokes

**Files:**
- Modify: `lib/steami_board/src/SteamiDisplay.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Consumes: Task 5 `ready()`, `dot()`, `span()`, `forEachPanel`.
- Produces: `hline(x, y, w, c)` = `span(x, y, w, 1)`; `vline(x, y, h, c)` = `span(x, y, 1, h)`; `rect(x, y, w, h, c, fill)` (outline: top and bottom `w`, sides `h − 2` between them, no pixel twice; `fill`: one span); `line(x1, y1, x2, y2, c)` (Bresenham, both ends inclusive, `int32_t` error terms, through `dot()`; endpoints put in a canonical order first, lower x then lower y, so a reversed line lights the same pixels); protected `strokeLine(x1, y1, x2, y2, uint8_t w, SteamiRgb c)` (`w == 0` nothing, `w == 1` same pixels as `line()`, else the brush of the decisions through `span()`). 4a/4b use `strokeLine` for `k`-wide strokes.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_hline_vline_rect(void) {
    // hline(10, 5, 4) lights (10..13, 5) with one fillRect and no pixel call; vline likewise;
    // rect(10, 10, 5, 4) outline: 14 pixels, areaDrawn() == litCount() (no overdraw), <= 4 fillRects;
    // rect(…, true): 20 pixels, one fillRect
}
static void test_line_endpoints_and_symmetry(void) {
    // line(0, 0, 10, 4) and line(10, 4, 0, 0) light the same 11 pixels, ends included;
    // line(5, 5, 5, 5) is one pixel
}
static void test_lines_crossing_the_panel(void) {   // forEachPanel
    // line(-50, -50, 300, 300) lights the whole diagonal inside the panel; outOfBounds 0
}
static void test_stroke_widths(void) {
    // strokeLine(…, 1, c) == line(…, c) (snapshot compare); width 2: every pixel of the width-1
    // line has (x+1, y), (x, y+1), (x+1, y+1) lit; width 0 draws nothing
}
static void test_degenerate_spans(void) {   // Review Focus 5
    // hline(…, 0), hline(…, -3), vline(…, 0), rect(…, 0, 5), rect(…, 5, -1): nothing drawn, no
    // fillRect call; rect 1x1 one pixel, areaDrawn() == 1; rect 2x2 four pixels, no overdraw;
    // rect(10, 10, 1, 5) and rect(10, 10, 5, 1): litCount() == 5 and areaDrawn() == 5
}
static void test_extreme_spans(void) {   // Review Focus 1, forEachPanel
    // each case after clear(), W and H the panel size:
    // hline(100, 5, 32767): exactly x 100..W-1 on row 5, fillRectCalls 1;
    // vline(5, 100, 32767): exactly y 100..H-1 in column 5;
    // hline(-32700, 5, 32767): exactly x 0..66 on row 5 (67 pixels);
    // hline(32000, 5, 32767): nothing (x is right of the panel; x + w is computed in int32_t, so it does not wrap);
    // rect(-100, -100, 32767, 32767, c, true): litCount == W*H, one fillRect;
    // rect(10, 10, 32767, 32767): top row x 10..W-1 and left column y 11..H-1 only,
    //   litCount == (W - 10) + (H - 11);
    // off the panel, nothing: hline(-32768, 5, 32767), vline(5, 32767, 32767),
    //   rect(30000, 30000, 10000, 10000), rect(-32768, -32768, 32767, 32767, c, true);
    // line(-32768, -32768, 32767, 32767) lights exactly the visible diagonal; UBSan silent
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: FAIL (nothing drawn, or undefined references).

- [ ] **Step 3: Implement** the five members.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report. This run is what checks the "UBSan silent" comments of the tests.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiDisplay.cpp tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Draw clipped spans, rectangles, lines and strokes."
```

---

### Task 7: Circles and triangles

**Files:**
- Modify: `lib/steami_board/src/SteamiDisplay.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Consumes: Task 5 `dot()`, `span()`; Task 6 `line()`.
- Produces: `circle(cx, cy, r, c, fill)`: midpoint, 8-way symmetric; outline through `dot()`; `fill` as one span per row covering the outline's extent on that row (no `pixel()` call). `triangle(x0, y0, x1, y1, x2, y2, c, fill)`: outline = three `line()`; fill = for each panel row in `[ymin, ymax]`, `x` on each edge spanning that row, computed on the edge oriented from its lower-y end (ties: lower x) as `xa + round((xb − xa)·(y − ya) / (yb − ya))` with the product in `int64_t` and rounding to nearest, halves away from zero; a horizontal edge contributes both ends; one span from min to max. Same pixels for any vertex order.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_circle_is_8_way_symmetric(void) {
    // on 240, circle(120, 120, r) for r in {0, 1, 2, 5, 37, 100}: every lit (120+a, 120+b) has
    // (120±a, 120±b) and (120±b, 120±a) lit; r = 0 is one pixel
}
static void test_filled_circle_covers_outline_with_spans(void) {
    // on 240 colour, for r in {3, 20, 64}: circle(120, 120, r, WHITE) into one fresh backend and
    // circle(120, 120, r, WHITE, true) into another; every pixel lit in the outline is lit in the
    // filled one; filled: pixelCalls() == 0, fillRectCalls() == 2r + 1, areaDrawn() == litCount();
    // clipped: on 128 at (64, 64) with r 64, fillRectCalls() == 128 (visible rows only)
}
static void test_circles_crossing_every_edge(void) {   // forEachPanel, outline and filled
    // centres (0, cy), (W-1, cy), (cx, 0), (cx, H-1), radius 30; and (cx, cy, 100) on 128
}
static void test_filled_triangle_same_for_six_orders(void) {
    // (10, 5), (60, 40), (25, 90) on 128 and (−20, 10), (200, 120), (100, 300) on 240: the six
    // permutations give identical snapshots; pixelCalls 0
}
static void test_degenerate_circles_and_triangles(void) {   // Review Focus 5
    // circle r -1: nothing; triangle with three equal points: one pixel; collinear (0,0) (5,5) (10,10)
    // filled: one span per row from row 0 to 10; no empty fillRect
}
static void test_extreme_circles_and_triangles(void) {   // Review Focus 1, forEachPanel
    // each case after clear(): circle(cx, cy, 32767, c, true): litCount == W*H, pixelCalls 0;
    // circle(0, 0, 32767): litCount 0; circle(-32768, 64, 32767, c, true): litCount 0 (a wrapped
    // cx - r would land at x 1); triangle(-32768, -32768, 32767, 0, -32768, 32767, c, true):
    // litCount == W*H; outOfBounds 0, UBSan silent
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: FAIL.

- [ ] **Step 3: Implement** `circle()` and `triangle()`; filled shapes walk only the panel rows.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report. This run is what checks the "UBSan silent" comments of the tests.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiDisplay.cpp tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Draw clipped circles and triangles as spans."
```

---

### Task 8: Integer arc and `strokeCircle`

**Files:**
- Modify: `lib/steami_board/src/SteamiDisplay.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp`

**Interfaces:**
- Consumes: Task 2 `steamiSinQ14`, `steamiCosQ14`, `steamiIsqrt`; Task 5 `span()`.
- Produces: `arc(cx, cy, r, startDeg, sweepDeg, c, uint8_t width = 1)`, the 5.6 pseudo-code exactly: rows clipped to the panel; `xo = isqrt(floor(((2r+1)² − 4dy²)/4))`; `xi` the smallest `x ≥ 0` with `4(x² + dy²) ≥ (2ri − 1)²` (0 when `ri == 0`); squares in `int64_t`; each span walked **only over its part inside the panel**; sectors of at most 180°, `S = (sin s, −cos s)`, `E = (sin e, −cos e)` in Q14, cross products in `int32_t`; each run emitted as one `span(…, run, 1)`. Protected `strokeCircle(cx, cy, r, uint8_t w, SteamiRgb c)` = `arc(cx, cy, r, 0, 360, c, w)` (4b: compass and watch dials).

- [ ] **Step 1: Write the failing tests**

```cpp
static bool inRing(int32_t dx, int32_t dy, int32_t r, int32_t w) {   // the distance test of 5.6
    const int64_t d4 = 4 * ((int64_t)dx * dx + (int64_t)dy * dy);
    const int32_t ri = r - w + 1 > 0 ? r - w + 1 : 0;
    return d4 <= (int64_t)(2 * r + 1) * (2 * r + 1) && (ri == 0 || d4 >= (int64_t)(2 * ri - 1) * (2 * ri - 1));
}
static void test_full_arc_equals_distance_ring(void) {   // forEachPanel
    // for (r, w) in {(50, 1), (50, 7), (5, 10), (60, 7)} and centres (cx, cy), (0, 0), (W-1, cy),
    // (cx, 0), (cx, H-1):
    // after arc(cx, cy, r, 0, 360, WHITE, w), lit(x, y) == inRing(x - cx, y - cy, r, w) for every
    // panel pixel; pixelCalls 0; areaDrawn() == litCount() (each pixel once)
}
static void test_quarter_arc_is_noon_to_three(void) {
    // arc(120, 120, 50, 0, 90, WHITE, 7) on 240: every lit pixel has dx >= 0 and dy <= 0, and every
    // ring pixel with dx > 0 and dy < 0 is lit
}
static void test_adjacent_arcs_leave_no_gap(void) {
    // snapshot of arc(0, 90) then arc(90, 90) == snapshot of arc(0, 180) (r 60, w 7, same colour);
    // gauge split: arc(225, 135) then arc(0, 135) == arc(225, 270)
}
static void test_arc_angles_normalised(void) {   // Review Focus 4
    // arc(-90, 90) == arc(270, 90); arc(450, 90) == arc(90, 90); arc(0, 400) == arc(0, 360);
    // sweep 0 and sweep -10: nothing; width 0: nothing; r -1: nothing; r 0, w 1: centre pixel only
}
static void test_stroke_circle_is_full_arc(void) {   // strokeCircle(cx, cy, 40, 2, c) == arc(cx, cy, 40, 0, 360, c, 2)
}
static void test_extreme_arcs(void) {   // Review Focus 1, forEachPanel
    // after clear(): arc(cx, cy, 32767, 0, 360, c, 255) lights nothing (every panel pixel fails
    // inRing(x - cx, y - cy, 32767, 255)); arc(-32768, -32768, 32767, 45, 90, c, 3) lights nothing;
    // arc(cx, cy, 300, 0, 360, c, 255) lights exactly the panel pixels where inRing holds;
    // outOfBounds 0, UBSan silent, test finishes (span walk clipped to the panel)
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: FAIL.

- [ ] **Step 3: Implement** `arc()` and `strokeCircle()`.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report. This run is what checks the "UBSan silent" comments of the tests.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiDisplay.cpp tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Draw rings and arcs with an integer scanline test."
```

---

### Task 9: Text (8x8 public, 5x7 protected)

**Files:**
- Modify: `lib/steami_board/src/SteamiDisplay.h` (protected `glyph()`), `lib/steami_board/src/SteamiDisplay.cpp`
- Test: `tests/native/test_steami_display/test_main.cpp` (also add `using SteamiDisplay::glyph;` to `TestDisplay`, since `test_glyph_window` calls it)

**Interfaces:**
- Consumes: Task 3 `steamiNextGlyph`, `steamiGlyphCount`, `steamiGlyph8x8`, `steamiGlyph5x7`; Task 5 `span()`.
- Produces:
  - `void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);` advance `8·s`; `nullptr` or `""` draws nothing.
  - `int16_t textWidth(const char* s, uint8_t scale = 1) const;` `glyphs·8·s`, saturating at 32767.
  - protected `void textSmall(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale = 1);` 5x7, advance `6·s`; `int16_t textSmallWidth(const char* s, uint8_t scale = 1) const;` `glyphs·6·s`.
  - protected `void glyph(const uint8_t* columns, uint8_t firstColumn, uint8_t columnCount, uint8_t rows, int32_t x, int32_t y, SteamiRgb c, uint8_t scale);` draws the column window `[firstColumn, firstColumn + columnCount)` with its left edge at `x`: for each row, each horizontal run of set bits is one `span(x + col·s, y + row·s, run·s, s)`. `text()` uses `(g, 0, 8, 8)`, `textSmall()` `(g, 0, 5, 7)`; 4a's value text uses `(g, 2, 4, 8)` for '.' and ':'.
  - Scale 0 counts as 1 in all four; glyph positions in `int32_t`.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_glyph_size_at_scales_1_to_4(void) {
    // for s in 1..4 on 240: text("A", 10, 10, WHITE, s): lit bounding box x 10+s .. 10+7s-1,
    // y 10 .. 10+7s-1; litCount == 28·s·s; pixelCalls 0
}
static void test_text_width_counts_glyphs(void) {
    // textWidth("°C") == 16; textWidth("°C", 2) == 32; textWidth("Température") == 88;
    // textWidth("") == 0; textWidth(nullptr) == 0; textWidth with 5000 'W' at scale 255 == 32767
}
static void test_accents_fold_when_drawn(void) {
    // snapshot(text("é", …)) == snapshot(text("e", …)); "É" vs "E"; "Ÿ" vs "Y"; "°" draws the degree
    // glyph, a ring in rows 0..3 and columns 1..4
}
static void test_text_invalid_utf8_draws_question_marks(void) {   // Review Focus 2
    // "\xFF" vs "?"; "A\xC3" vs "A?" (textWidth 16); "€" vs "?"
}
static void test_text_clipping(void) {   // forEachPanel
    // text("Hello", 10, H - 3), text("Hello", -20, 10), text("WWWW", -100, -100, c, 255),
    // text("X", 32767, 32767): outOfBounds 0; the visible top rows of "Hello" at H - 3 are lit
}
static void test_text_nullptr_and_empty_draw_nothing(void) {
    // text(nullptr, 10, 10) and text("", 10, 10): litCount 0, fillRectCalls 0, outOfBounds 0
}
static void test_text_scale_zero_is_one(void) {   // Review Focus 5: scale 0 == scale 1, textWidth("AB", 0) == 16
}
static void test_small_text(void) {
    // textSmall("A", 10, 10, WHITE): bounding box x 10..14, y 10..16 (5x7 'A');
    // textSmallWidth("950") == 18, scale 2 == 36; textSmall(nullptr, …) draws nothing
}
static void test_glyph_window(void) {
    // glyph(steamiGlyph8x8('.'), 2, 4, 8, 10, 10, WHITE, 1) lights (11..12, 15..16), the '.' ink of
    // columns 3 and 4 shifted by the window start (4a's narrow '.')
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: FAIL.

- [ ] **Step 3: Implement** the five members.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:` and no `AddressSanitizer` report.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` for the `steami` env.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiDisplay.* tests/native/test_steami_display/test_main.cpp
git commit -m "feat(board): Draw 8x8 and 5x7 text from UTF-8 strings."
```

---

### Task 10: Gallery PPM files and the sanitizer CI job

**Files:**
- Modify: `tests/native/test_steami_display/test_main.cpp`
- Modify: `.github/workflows/tests.yml` (new job beside `native`)

**Interfaces:**
- Consumes: every primitive of Tasks 5 to 9; `SteamiMemoryBackend::writePpm`.
- Produces for 3b: `static void drawGallery(SteamiDisplay& d)` below, the scene 3b's `Drawing` example copies line for line (the 240 px dump is the reference for the V1c photo). The env `native_asan` (Task 1) is the one 3b's `test_steami_display_board` joins through its filter.

```cpp
static void drawGallery(SteamiDisplay& d) {
    const int16_t cx = d.centerX(), cy = d.centerY(), r = d.radius();
    const uint8_t s = d.width() >= 240 ? 2 : 1;
    d.clear();
    d.circle(cx, cy, r - 1, STEAMI_DARK);                              // panel edge
    d.arc(cx, cy, r - 4 * s, 225, 270, STEAMI_GRID_DARK, 6 * s);       // gauge track
    d.arc(cx, cy, r - 4 * s, 225, 135, STEAMI_GREEN, 6 * s);           // half fill
    d.text("STeaMi", cx - d.textWidth("STeaMi", s) / 2, cy - r / 2, STEAMI_WHITE, s);
    d.text("20°C é", cx - d.textWidth("20°C é", s) / 2, cy - r / 2 + 12 * s, STEAMI_YELLOW, s);
    d.rect(cx - r / 2, cy - r / 8, r / 3, r / 4, STEAMI_LIGHT);
    d.rect(cx - r / 2 + 2, cy - r / 8 + 2, r / 3 - 4, r / 4 - 4, STEAMI_BLUE, true);
    d.circle(cx + r / 3, cy, r / 8, STEAMI_RED, true);
    d.triangle(cx - r / 4, cy + r / 2, cx + r / 4, cy + r / 2, cx, cy + r / 4, STEAMI_YELLOW, true);
    d.line(cx - r / 2, cy + r / 6, cx + r / 2, cy + r / 6, STEAMI_GRAY);
    d.show();
}
```

- [ ] **Step 1: Add `drawGallery` and `test_gallery_ppm`** (registered with `RUN_TEST`)

```cpp
static void test_gallery_ppm(void) {
    // 128 grey and 240 colour: drawGallery; outOfBounds 0; litCount > 0; showCalls 1; when getenv("STEAMI_PPM_DIR") is set, writePpm to
    // "<dir>/steami-display-gallery-128-gray.ppm" and "<dir>/steami-display-gallery-240-color.ppm"
    // and assert both return true
}
```

- [ ] **Step 2: Run the gallery**

Run: `.venv/bin/pio test -e native -f native/test_steami_display` — Expected: PASS; on the 128 px grey and the 240 px colour backend: outOfBounds 0, litCount > 0, showCalls 1.
Run: `mkdir -p /tmp/steami-ppm && STEAMI_PPM_DIR=/tmp/steami-ppm .venv/bin/pio test -e native_asan`
Expected: PASS, no `runtime error:` and no `AddressSanitizer` report; two `.ppm` files in `/tmp/steami-ppm`.

- [ ] **Step 3: Commit**

```bash
git add tests/native/test_steami_display/test_main.cpp
git commit -m "test(tests): Render the primitives gallery to PPM files."
```

- [ ] **Step 4: Add the CI job**

In `.github/workflows/tests.yml`, a job `display-sanitizers` with the same checkout, Python and PlatformIO cache steps as `native`, then:
- `Run the display suites under ASan and UBSan`, env `STEAMI_PPM_DIR: ${{ runner.temp }}/steami-display-ppm`, run: `mkdir -p "$STEAMI_PPM_DIR"`, `make install-pio`, `.venv/bin/pio test -e native_asan`;
- `Upload the primitives gallery`, `actions/upload-artifact@v4`, `name: steami-display-gallery`, `path: ${{ runner.temp }}/steami-display-ppm`, `if-no-files-found: error`.

```bash
git add .github/workflows/tests.yml
git commit -m "ci(ci): Run the display suite under ASan and upload its gallery."
```

- [ ] **Step 5: Full verification before the PR**

Run each; all must succeed:
- `make test-native` — every native suite PASSED (including `test_steami_board`, `test_steami_functions`).
- `.venv/bin/pio test -e native_asan` — PASSED, no sanitizer report.
- `make test-tooling` — `OK` (compile-fail rules and packaging tests).
- `make build` — the `steami` env compiles the new sources for the STM32WB55.
- `make lint` — clang-format, SPDX and clang-tidy clean.
- `make package` then `grep -c 'Steami\(Rgb\|Font\|Trig\|Display\|DisplayBackend\)\.h' build/arduino-package/src/STeaMi.h` — 5 (the umbrella includes the new headers; no name clash).
- `git log --format=%s main..` — every header ≤ 72 characters, no `!`, no attribution.

On the PR: the CI artefact `steami-display-gallery` holds the two PPM files; the description states that no hardware check applies to 3a and that V1a/V1b were not checked (decision 6).
