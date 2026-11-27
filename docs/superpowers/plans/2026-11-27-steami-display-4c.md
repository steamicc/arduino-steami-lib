# STeaMi display, delivery 4c (graph, menu and history) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `SteamiHistory<N>`, `graph()` and `menu()` to the screen layer, render mockups 05 and 06 natively at 128 and 240 px, and ship the examples ScreenGraph, ScreenMenu and WeatherStation, so that every mockup of sub-project 4 has its Arduino example.

**Architecture:**
- `SteamiHistory<N>` is a header-only ring. It befriends `SteamiScreen`, so that `graph(history)` reads the ring in place, without copying it.
- Both `graph()` overloads forward to one private non-template function. It takes a `Series` view (data, capacity, first, count), so the ring and a plain array share one code path.
- `menu()` wraps the index, chooses its visible rows by the chord rule, and draws a highlight bar clipped per scanline. The template overload forwards to `menu(items, n, ...)`.
- Every number comes from the kernel of 4a to 4b-font (`u()`, `k()`, `chord()`, `inkRows()`, `fitGlyphs()`, faces). `golden_layout.py` mirrors each widget and stays the oracle of every pinned number.

**Tech Stack:** C++17 (PlatformIO `env:native`, `env:native_asan`, `env:steami`, STM32duino 3.1.0), Unity, Python 3 standard library (+ Pillow/NumPy for the review), GitHub Actions (unchanged).

**Spec:** docs/superpowers/specs/2026-11-25-steami-display-design.md (as amended by docs/superpowers/specs/2026-11-26-steami-display-v1c-font-design.md).
- Below, **the spec** is the first file and **the font design** the second. The spec is the binding authority, as amended by the font design.
- Read before your task:
  - spec 5.7 (L358-368 `SteamiHistory`, L391-395 and L429-432 `graph`/`menu`, L422-423 vocabulary);
  - spec 5.8 (rules 1 to 7, and the size table L513-524);
  - spec 5.9 (rows `graph` L550 and `menu` L551), 5.10, 5.11;
  - spec section 6 (programs 05 L685, 06 L709, WeatherStation L768, known differences L814-848);
  - spec section 7 (L873-948) and row 4c (L989);
  - font design 5.1, 5.2, 5.3 (menu L248-254, graph labels L255-259) and 5.7.

Branch `feat/steami-display-4c`, from `main` at v0.40.0 (60dc6b0).
- Issue #288; the PR says "Closes #288".
- PR title (feat, at most 70 characters): `feat(board): Add graph, menu and SteamiHistory to the screen.`

## Global Constraints

From the spec (verbatim):
- "Widgets use at most 64 B of stack (formatting buffers of 16 B, triangle vertices); no copy of menu items or history."
- "NAN (what a function class returns when its sensor is missing) never crashes a widget: [...] `graph()` skips the point".
- "Every widget accepts empty or `nullptr` strings (draws nothing for that part)."
- "Nothing a widget draws leaves the circle of radius `radius() + 1`. Every cut keeps the longest fitting prefix [...], with no ellipsis".
- "`SteamiHistory<N>` | 4N + 4 B in the sketch".
- "Default colours [...]: graph axes and labels DARK".
- "On a grey panel (`!isColor()`), a widget foreground drawn over the widget's own background (bar fill over its track, gauge fill over its track, selected menu text over the highlight bar) is lifted to at least **background + 4 grey levels** (`STEAMI_MIN_CONTRAST = 4`, capped at 15)".
- "No heap, no `String`" (spec 3). Float is allowed at widget inputs. Drawing code uses no `double` and no `sin`/`cos`.
- Font design 5.7:
  - "No new local in a public widget frame [that is already at 64 B]".
  - Fallback: "the task shrinks its locals; if that is impossible without breaking another spec rule, the builder stops and reports a spec deviation; a helper is never exempted".
  - Helpers that hold a buffer, a `Run`, a `Region` or triangle vertices are bound to 64 B; `Run` and `Region` keep their layout; `__attribute__((noinline))` where inlining breaks a bound.
- Public signatures exactly as spec L391-395:
  - `template <size_t N> void graph(const SteamiHistory<N>& h, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT);`
  - `void graph(const float* data, size_t n, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT);`
  - `template <size_t N> int menu(const char* const (&items)[N], int selected = 0, SteamiRgb c = STEAMI_WHITE);`
  - `int menu(const char* const* items, size_t n, int selected = 0, SteamiRgb c = STEAMI_WHITE);`
- `sizeof(SteamiScreen) <= 3 * sizeof(void*)` stays: no new data member, no cached state.

Carried from 4a, 4b and 4b-font (plans and ledgers):
- **Setup.**
  - `.venv` is a symlink to `/home/nedjar/sandbox/arduino-steami-lib/.venv`, and `node_modules` to `/home/nedjar/sandbox/arduino-steami-lib-display-3b/node_modules`. Only run their tools: never `make setup`, `make install`, `make prepare`, `pip install`, `npm install`, or any write under another checkout.
  - The Makefile's tool prerequisites are older than `requirements.txt`, so a plain `make <target>` reinstalls into the other checkout. Always call make as **`mk`** = `make -o .venv/bin/pio -o .venv/bin/clang-format -o .venv/bin/clang-tidy -o .venv/bin/ruff`, or call the tools directly. `make test-tooling` and `make package` have no such prerequisite.
  - The local `main` ref is stale. Every diff, baseline and commit range uses `origin/main` (= 60dc6b0).
- **Commits.**
  - Header: `type(scope): Sentence with a capital and a final period.`, at most 72 characters.
  - Scopes: `board` (lib/steami_board), `tests`, `tooling` (scripts), `docs` (spec and plan).
  - Bodies are wrapped under 100 columns. No line of a body starts with `(` followed by an issue reference (commitlint reads it as a footer).
  - No `!`, no "BREAKING CHANGE", no `[skip ci]`, no attribution lines, and never the phrase blocked by `git-precommit-checks.config.js`.
  - Hooks do not run here (`core.hooksPath` is absent). Every **Commit** step starts with **the commit check**, by hand, for each commit:
    - `printf '%s\n\n%s\n' "<header>" "<body, if any>" | npx --no-install commitlint` passes;
    - `mk format-check` and `scripts/check-spdx.sh` pass when the commit touches a `.h`, `.cpp` or `.ino`;
    - ruff passes on every `.py` it touches (below).
- **Style.** Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`. Google style, indent 4, 100 columns. clang-format splits short `if`s, so examples write braces (the 4b ScreenFaces style).
- **clang-tidy** (`WarningsAsErrors: '*'`):
  - cast to `int32_t` (`size_t` or `uint64_t` for indices) before multiplying;
  - no `uint8_t` loop variable against a wider bound;
  - `smaller`/`larger`, never unqualified `min`/`max`/`abs`; use `fabsf`, `isnan`, `isinf`, `ceilf`;
  - no namespace-scope object with a constructor in a library `.cpp`;
  - no `else` after `return`.
  - `make tidy` checks `SteamiHistory.h` only through `SteamiScreen.cpp`'s include; the native suites exercise its members.
- **Ruff.** Every `.py` a task creates or changes passes `/tmp/claude-1000/-home-nedjar-sandbox-wiki-steami/1346e80f-1de2-4e4c-bae2-38b41b348575/scratchpad/ruff-venv/bin/ruff check --no-cache <files>`, run from the worktree root (ruff 0.17.6, the repository's `ruff.toml`).
- **Tests.**
  - Build screens on the file-local `board` and `RUN_TEST` every new test.
  - "A == B" means each is rendered alone on a fresh, cleared panel of the same size and mode (`rendered()`, `colourAlone()`), then compared.
  - "Values printed by golden_layout.py" means the literal comes from that script's output, never from the C++ under test.
- **Arithmetic.** Rows and columns in `int32_t`, cast to `int16_t` only after clamping. Ring and point indices in `size_t` (or `uint64_t` for products). UBSan (`native_asan`) runs every extreme case.
- **Examples.** Per task, compile only the examples the task touches: `.venv/bin/python3 scripts/test-examples.py --example steami_board/<Name>`. **Never run `make test-examples` (all examples) outside the final verification task** (maintainer's rule). The key is always `steami_board/WeatherStation`: `lib/bme280/examples/WeatherStation` is another example.
- **One PlatformIO build at a time.** `test-examples.py`, `check-footprint.py` and the stack measurement all build. Every task that builds ends with `git status --short lib/`, which lists no `*.ino.cpp`. Delete a stray one: the package would copy it.
- **Hardware.** Never flash, open a serial port, touch `/dev/ttyACM*` or `/run/media`, run `scripts/qualify.py` or `make test-hardware`. Only Task 11 uses the board, run by the controller.
- **Read-only references.** MicroPython `/home/nedjar/sandbox/micropython-steami-lib/lib/steami_screen/steami_screen/device.py` (graph L228-286, menu L288-301; its 240 px form is saved at `/tmp/claude-1000/-home-nedjar-sandbox-wiki-steami/1346e80f-1de2-4e4c-bae2-38b41b348575/scratchpad/mp_device_origin_main.py` L423-515). Mockups `/home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups/` (`05_scrolling_graph_ref.png` and `05_scrolling_graph.svg`, `06_dpad_menu_ref.png` and `06_dpad_menu.svg`). Never touch `/home/nedjar/sandbox/arduino-steami-lib`, `/home/nedjar/sandbox/wiki_steami`, `/home/nedjar/sandbox/arduino-steami-lib-display-3b` or `/home/nedjar/sandbox/micropython-steami-lib` (reading is fine; no checkout or pull).

Checks every task uses (`$SCRATCH` is your scratchpad, `ROOT` the worktree):
- **The 128 px check:**
  ```bash
  PPM="$SCRATCH/ppm"; rm -rf "$PPM"; mkdir -p "$PPM"
  STEAMI_PPM_DIR="$PPM" .venv/bin/pio test -e native -f native/test_steami_display -f native/test_steami_screen_mockups
  (cd "$PPM" && sha256sum -c "$ROOT/tests/native/test_steami_display/ppm-128.sha256")
  ```
  Expected: 14 `OK` (16 from Task 5 on, D13). The 14 lines of 4b-font never change. A failure means a 128 px pixel of an existing widget changed: fix the code.
- **The 128 literal check** (every task that touches `SteamiScreen.cpp` or a suite): `git diff -U0 origin/main -- tests/native/test_steami_screen/ tests/native/test_steami_screen_mockups/ tests/native/test_steami_display/` shows no changed literal of an existing 128 branch. New tests only add lines.
- **The stack measurement:**
  ```bash
  SU="$SCRATCH/su"; rm -rf "$SU"
  PLATFORMIO_BUILD_DIR="$SU" PLATFORMIO_BUILD_FLAGS=-fstack-usage PLATFORMIO_SRC_DIR=lib/steami_board/examples/<Sketch> .venv/bin/pio run -e steami
  grep -h 'SteamiScreen::' $(find "$SU" -name '*.su')
  ```
  - `<Sketch>` is ScreenTemperature until the examples exist (Tasks 2 to 4). It is ScreenGraph, ScreenMenu and WeatherStation from Task 6 on: the `graph<N>` and `menu<N>` templates are instantiated in the sketch.
  - **The template wrappers.** `graph<N>` and `menu<N>` are one-line header templates (D1, D11), so at `-Os` GCC normally inlines them into the caller, and the `.su` then has no line for them.
    - If a `SteamiScreen::graph(...) [with unsigned int N = ...]` or `SteamiScreen::menu(...) [with unsigned int N = ...]` line appears in the sketch's `.ino.cpp.su`, it is bound to 64 B.
    - If none appears, confirm the inlining with `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm -C "$SU/steami/firmware.elf" | grep -E 'graph<|menu<'` (no symbol), and record the caller's frame (`loop()`, or the helper that calls the widget) as "inlined" in the stack table. The `Series` temporary and the arguments then live in that frame.
    - In every case the out-of-line `graph(const float*, ...)`, `graphSeries` and `menu(const char* const*, ...)` frames are bound to 64 B.
    - Never add `noinline` to the public templates only to make their line appear, unless the maintainer asks to measure them.
  - **Bound to 64 B:**
    - 4b-font's list: `face` ×3, `watch`, `compass`, `gauge`, `title`, `subtitle` ×3, `value` ×2, `text`, `bar` ×2, `pointer`, `gaugeLabel`, `drawGaugeLabel`, `fitGlyphs`, `placeValue`, `fitValue`, `drawValuePart`, `drawValueParts`, `linesFit`, `valueRegion`;
    - new in 4c: `graph(const float*, ...)`, `graphSeries`, `graphValue`, `graphLabel`, `drawGraphLabel`, `menu(const char* const*, ...)`, `menuRows`, `menuRow`, `menuBar` (it holds a `Region`, font design 5.7), and the template wrappers when they have a line (above).
  - **Recorded only:** `graphFrame`, `graphCurve`, `Series::at`, and the kernel helpers.
  - 4b-font's frames, for comparison: face(bitmap) 64, watch 48, compass 64, gauge 56, title 16, subtitle 64/16/16, value 64/40, text 56, bar 48/0, pointer 64, gaugeLabel 48, drawGaugeLabel 32, fitGlyphs 56, placeValue 48, fitValue 32, drawValuePart 40, drawValueParts 64, linesFit 48, valueRegion 32.
  - A frame at 64 B gains no new live value. A bound frame over 64 B goes through the fallback above.
- **The golden script:** `python3 tests/native/test_steami_screen/golden_layout.py`, exit 0. It is run by hand; no CI job runs it.
  - New sections are titled `(4c Task N)` in 4b's pattern: model, self-check asserts, then `# ---- Values pinned by test_steami_screen (4c Task N) ----` prints.
  - Floats are mirrored in float32 with the script's `f32()` after every operation.

## Decisions taken while planning (spec 13 and the gaps the spec leaves)

- **D1 Ring access.**
  - `SteamiHistory` declares `friend class SteamiScreen;` (forward-declared) and a private `size_t first() const`, the index in `_data` of the oldest value.
  - `SteamiScreen` gains a protected `struct Series { const float* data; size_t capacity; size_t first; size_t count; float at(size_t i) const; };`. `at(i)` is `data[(first + i) % capacity]`, defined in the `.cpp`.
  - `graph<N>` is `graphSeries(Series{h._data, N, h.first(), h._count}, minV, maxV, c);`.
  - The array overload is `graphSeries(Series{data, n, 0, data == nullptr ? 0 : n}, ...)`. So the template "forwards to a non-template .cpp function" with no copy.
  - Rejected:
    - a shifting array: the spec fixes `_head`;
    - a public raw view: students would meet it.
- **D2 History semantics.**
  - `_head` is the slot the next `add()` writes. `add()` writes there, advances `_head` modulo N, and raises `_count` up to N. Index arithmetic is done in `size_t`, never in `uint16_t`, so N = 65535 never overflows.
  - `add(NAN)` stores the NAN and counts it: `graph()` shows the gap.
  - `float _data[N] = {};`: the spec's `float _data[N];` plus an initializer, so that the class is constexpr-constructible, constant-initialised like `SteamiScreen`, and the same size.
  - `static_assert(N >= 1 && N <= 65535, ...)` sits in the class body.
- **D3 Graph geometry** (spec 5.9 `graph`; device.py where the spec is silent). In 128-px constants through `u()`:
  - **Box.** `gx = u(21)`, `gy = u(38)`, `gw = W − u(15) − u(21)`, `gh = u(52)`. That is 21/38/92/52 on 128 and 39/71/173/97 on 240.
  - **Middle line** (device.py v0.30.2). Row `gy + gh / 2` (64 / 119). Dashes from `x = gx + k`, each `[x, min(x + u(3) − 1, gx + gw − 1)]`, every `u(3) + u(3)`, while `x < gx + gw`. Each dash is drawn as `rect(x, row, len, k, STEAMI_GRID_DARK, true)`: k rows thick, by rule 2. strokeLine is not used, because its brush would widen a dash by one column.
  - **Axes.** `strokeLine(gx, gy, gx, gy + gh, k, DARK)` and `strokeLine(gx, gy + gh, gx + gw − 1, gy + gh, k, DARK)`. These are device.py's `_sline` lines, so on 240 they ink x 39..40 and rows 168..169.
  - **Order.** Value, labels, dashes, axes, curve. The curve paints over the labels (spec).
- **D4 Curve.**
  - n = `count` (the history's `size()`), so a partial history stretches across the box, as device.py does. With n < 2, no curve.
  - Point i:
    - `x = gx + i·gw / (n − 1)` in `uint64_t`, so the last point is at `gx + gw`, as device.py and the mockup;
    - `y = gy + gh − ceilf(ratio · gh)`, which is device.py's `int(gy + gh − ratio·gh)` without its float subtraction;
    - `ratio = (v − minV) / span` in float, with `span = maxV − minV`, and span 0 replaced by 1 (device.py);
    - a NAN ratio skips the point; otherwise the ratio is clamped to [0, 1], INF included (spec 7 "clamped"). With minV > maxV the curve is inverted, as `gauge()`.
  - A segment `strokeLine(..., k, c)` is drawn only between consecutive valid points. A NAN leaves a gap, and an isolated valid point draws nothing, as "1 point draws the frame and the value only".
- **D5 Current value.**
  - The text is the integer part of the series' last value, via the new anonymous-namespace `integerText(float v, char* out, size_t size)`. It is the formatter `gaugeLabel()` uses today: truncated toward zero, `"--"` when `!(fabsf(v) < 1e9f)`. `gaugeLabel()` now calls it.
  - This touches a finished 4b widget, which row 4c does not ask for. The PR lists it under the changes outside row 4c, and the existing gauge tests (the 128 and 240 goldens of 4b and 4b-font) pass with no changed literal.
  - An empty series shows `"--"`, from spec 5.9 ("`--` if NAN" and "0 or 1 point draws the frame and the value only"). This is an **MP** mark, because device.py draws no value for empty data (Task 8 writes it).
  - The value is drawn with `SteamiDisplay::text(s, centerX() − textWidth(s) / 2, u(31), STEAMI_WHITE, 1)`, as `glyphsAt` does (the TEXT face at k, font design F16). It is never cut: 10 glyphs at most, 80 px, inside the chord at rows 31..38 and 58..73.
- **D6 Axis labels** (B1, font design 5.3).
  - Text: `integerText(maxV)` and `integerText(minV)`. LABEL ink at f = k (`faceFor`: 5x7 on 128, 6x12 on 240). DARK.
  - Rows, by ink edges: with `rows = inkRows(...)`, `n = replication(face, k)`, and `first` and `last` the lowest and highest set bits:
    - max cell top = `gy + u(2) − first·n`;
    - min cell top = `gy + gh − k − u(2) − (last + 1)·n + 1`.
    - Digits give 40 and 81 on 128, and 73 and 155 on 240. `"--"` gives 37 and 84 on 128, and 69 and 158 on 240 (the rule as written).
  - Column: `x = larger(chord(ink top, ink bottom).left, gx + k) + u(2)`, which is 24 on 128 and 44 on 240.
  - Cut, as a guard: the longest prefix that fits `[x, smaller(chord right − k, W − 1)]` at its inked rows. Every label of at most 10 glyphs fits whole on both panels.
- **D7 Menu rows, window and index.**
  - Row i has its top at `u(35 + 14·i)`, **never** `u(35) + i·u(14)`. That gives 35, 49, 63, 77, 91 on 128 and 65, 91, 118, 144, 170, 196 on 240.
  - Visible rows: count rows from i = 0 while `chord(top, top + 8k − 1).width() >= 12 · glyphAdvance(TEXT, k)` (96 px), and stop at the first that fails. Then take `min(n, that count)`. This gives 5 on 128 and 6 on 240.
  - n above `INT32_MAX` counts as `INT32_MAX`.
  - Wrapped index: `s = selected % n` in `int32_t`, plus n when negative. So −1 → n − 1, n → 0, 2n + 1 → 1, INT_MIN → 4 and INT_MAX → 1 for n = 6.
  - Window start: `clamp(s − visible / 2, 0, n − visible)`.
  - `menu()` computes and returns the wrapped index **before** `widgetCanDraw()`. A missing screen (E30/E31) still returns it, so navigation does not depend on the panel (like `value()`, which formats first).
  - `n == 0` draws nothing and returns 0 (spec 5.9). `items == nullptr` with n > 0 draws nothing and returns the wrapped index, like a missing screen.
  - Other widths follow the same rule: 200 px gives 6 rows; 96 px gives 0 (row 1's full-cell chord is 85 px < 96), so there `menu()` draws nothing and still returns the wrapped index.
- **D8 Highlight bar.**
  - Rows `[top − u(2), top − u(2) + u(14) − 1]`: 33..46 for row 1 on 128, and 193..218 for row 6 on 240 (font design 5.3).
  - Each scanline y fills `[chord(y, y).left + k, smaller(chord(y, y).right − k, W − 1)]` in DARK, so the ends follow the circle (spec "so its ends follow the circle"). It is not the device.py box `u(15)..W − u(15)` cut to the chord.
  - New private `menuBar(int32_t top)`. `barRows()` and `bar()` stay untouched, so `bar()` keeps its 48 B frame and its 128 px pixels.
- **D9 Marker.**
  - The text is `STEAMI_MENU_MARKER = "> "`, drawn in the selected colour.
  - Its x is `chord.left + u(2)`, where the chord is taken at the item's inked rows (spec). An item that inks no row (`""` or `nullptr`) uses the marker's own inked rows instead, so its marker is still placed.
- **D10 Items.**
  - TEXT ink at k.
  - An unselected item: cut with `fitGlyphs(t, &room, u(2))`, room the whole panel. Drawn at `centerX() − w / 2`, in GRAY.
  - The selected item: room `[marker x + width of "> ", W − 1]`, cut with pad `u(2)`. Drawn at `larger(centerX() − w / 2, room left)`, in `lifted(c, STEAMI_DARK)`; the marker uses the same colour.
  - Order: bar, marker, item.
- **D11 Overloads.**
  - `menu<N>` is `return menu(items, N, selected, c);`.
  - `const char* ITEMS[]` binds to `const char* const (&)[N]`: checked with g++ while planning. A native test and an `env:steami` scratch build confirm it, so no second template is added.
  - **Known limit of the spec's signatures** (checked with g++ while planning): with an index of type `unsigned`, `long` or `size_t` and a `const char* const` array, the non-template `menu(items, n, ...)` wins. It then runs with n = index and selected 0, silently. `int`, `uint8_t`, `int16_t`, `uint16_t` and `bool` pick the template (spec L429). A non-const `const char* ITEMS[]` array still picks the template for every index type.
    - The README says to keep the index in an `int`.
    - A test pins the five good types.
    - The PR reports the limit to the maintainer. No public signature changes (spec 5.7).
- **D12 Example guard** (`scripts/tests/test_steami_display_rules.py`).
  - `_calls()` finds the call with `re.search(r"\bscreen\.(\w+)", statement)` and tokenizes from the match start. Statements such as `selected = screen.menu(ITEMS, selected)` and `} else screen.graph(...)` then count.
  - Examples still write braces.
  - New `test_menu_items_match_the_mockup`: the string literals of `ITEMS[]` in ScreenMenu.ino equal, in order, those of `MENU_ITEMS[6]` in `SteamiMockups.h`. The call tokens never compare array contents.
- **D13 Mockup inputs and renders.**
  - **05.** `SteamiHistory<20>` filled with the SVG polyline's values, `1000·(141 − y)/82` rounded, with the last value set to the mockup's text "847": `354, 378, 415, 390, 451, 524, 561, 537, 573, 610, 585, 622, 598, 634, 610, 622, 585, 610, 646, 847`. Light branch (title "Light", `graph(h, 0, 1000)`), chip "APDS-9960" on both panels.
  - **V1c photo reference.** The pressure branch at 240 is rendered as `05_pressure_v1c`: 20 values of 1013.0, chip "BME280".
  - **06.** `selected = 2`, the six `ITEMS` of program 06.
  - **PPM names.** `steami-mockup-05_scrolling_graph-{128-gray,240-color}.ppm` and `steami-mockup-06_dpad_menu-{128-gray,240-color}.ppm`. The extra `05_pressure_v1c` (240 only) and the four `weather_*` renders (Task 7) are not in `MOCKUPS`; the review ignores them.
  - `ppm-128.sha256` gains the two new 128 px lines (16 lines), so later deliveries pin 05 and 06. The 14 old lines stay.
- **D14 WeatherStation.**
  - The example is spec section 6's program, under 4b Ruling 2: `#include <Arduino.h>`, the sensor headers, `<SteamiBoard.h>` and `<SteamiScreen.h>`; `Serial.begin(115200)` first.
  - Its `loop()` screen calls are mirrored by `weatherPage()` in `SteamiMockups.h`, paired in the guard, and rendered natively per page as the V1c photo references.
  - It joins `SKETCHES` of `check-footprint.py`.
  - **No `scripts/qualify.py` case** (row 4c: "if WeatherStation needs a case"). The runner flashes `tests/qualification/<driver>/` (its own sketch, so the example would not run unchanged) and resets the board through OpenOCD, which this delivery's hardware rules exclude; it is interactive and parses `key=value` lines; and the `.venv` that `make qualify-*` uses has no PyYAML. The spec's check is met by the controller's run of the unchanged example (Task 11): the serial transcript shows the banner and only E20. Task 8 rewrites spec L966 to say so.
- **D15 Constants** (`SteamiScreenConst.h`, 128-px units unless stated, MicroPython's names kept, spec 11 item 8):
  - Graph: `STEAMI_GRAPH_MARGIN = 15`, `STEAMI_GRAPH_X_OFFSET = 6`, `STEAMI_GRAPH_Y = 38`, `STEAMI_GRAPH_HEIGHT = 52`, `STEAMI_GRAPH_VALUE_Y = 31`, `STEAMI_GRAPH_DASH = 3`, `STEAMI_GRAPH_GAP = 3`, `STEAMI_GRAPH_LABEL_INSET = 2`.
  - Menu: `STEAMI_MENU_TOP = 35`, `STEAMI_MENU_PITCH = 14`, `STEAMI_MENU_BAR_RISE = 2`, `STEAMI_MENU_ROW_CELLS = 12` (TEXT cells, not scaled; MicroPython v0.30.2's name), `STEAMI_MENU_MARKER_INSET = 2`, `STEAMI_MENU_ITEM_PAD = 2`, and `constexpr const char STEAMI_MENU_MARKER[] = "> ";`.
  - The box x is `u(STEAMI_GRAPH_MARGIN + STEAMI_GRAPH_X_OFFSET)` = u(21), as in the spec. It is not `u(15) + u(6)`, which differs on other widths.

## Review Focus

1. **A menu index that is not an `int`** (`unsigned`, `long`, `size_t`, `uint8_t`, `bool`).
   - A person expects the same menu whatever the integer type.
   - The spec's fixed signatures make `unsigned`, `long` and `size_t` call `menu(items, n, ...)` with n = the index (D11).
   - Test: `test_menu_index_types` (Task 4) pins `int`, `uint8_t`, `int16_t`, `uint16_t` and `bool` to the template (all six items drawn). The README warns, and the maintainer gets the limit.
2. **Extreme graph bounds and values** (minV == maxV, minV > maxV, NAN or INF bounds, ±FLT_MAX whose span overflows, values of ±INF or FLT_MAX).
   - Expected: no crash, no UBSan report, nothing outside the circle, labels "--" for non-finite bounds, the curve clamped into the box.
   - Test: `test_graph_extremes` (Task 3).
3. **Null and empty inputs** (`graph(nullptr, 5)`, `menu(nullptr, 3, 1)`, items that are `nullptr` or `""`, 40-glyph items with accents).
   - Expected: no crash, no read past a string, nothing drawn for that part, the marker still placed; `menu(nullptr, 3, 1)` returns 1 and draws nothing (D7).
   - Tests: `test_graph_null_data` (Task 2), `test_menu_null_and_empty_items` and the `test_long_strings_stay_inside` extension (Task 4).
4. **Arithmetic at the limits**: `SteamiHistory<65535>` after 65537 adds, a menu index of INT_MIN or INT_MAX, and `graph(data, 70000, ...)`.
   - Expected: the ring order is right, the wrap has no overflow, the x mapping has no overflow (UBSan).
   - Tests: `test_history_at_65535` (Task 1), `test_menu_wraps` (Task 4), `test_graph_many_points` (Task 3).
5. **No screen, or a panel other than 128/240** (E30, E31, 96 px, 200 px).
   - Expected: each widget begins the display on its own; `menu()` still returns the wrapped index; nothing is drawn after E31; on 96 and 200 px nothing leaves the circle, and on 96 px `menu()` draws no row (D7).
   - Tests: the extensions of `test_widgets_without_a_screen` and `test_screen_e31_message` (Tasks 2 and 4), and `test_graph_and_menu_other_panels` (Task 4).

## Paths

Row 4c names only "the two screen suites under `tests/native/`". The PR also names these paths, which the spec's section 8 preamble allows (4b Ruling 7), and why:
- `tests/native/helpers/SteamiMockups.h`: the helpers the guard pairs with the examples;
- `tests/native/test_steami_display/ppm-128.sha256`: two new lines (D13);
- `scripts/mockup-review.py` and `scripts/tests/test_mockup_review.py`: the review list;
- `scripts/check-footprint.py` and `scripts/tests/test_check_footprint.py`: the footprint gate (spec 7);
- `scripts/tests/test_steami_display_rules.py`: the guard, and the `SteamiHistory` compile rules;
- `scripts/tests/test_build_arduino_package.py`: "STeaMi.h includes the new headers" (spec 7);
- `docs/superpowers/specs/2026-11-25-steami-display-design.md` (Task 8).

The PR also lists the one change to a finished widget: `gaugeLabel()` calls `integerText()` (D5).

`scripts/qualify.py`, `platformio.ini`, the workflows, the Makefile, `lib/ssd1327`, `lib/gc9a01` and `CONTRIBUTING.md` stay untouched.

---

### Before Task 1: commit the plan

- [ ] **Step 0:** Once the maintainer has approved the plan, commit it alone (the commit check first):

```bash
git add docs/superpowers/plans/2026-11-27-steami-display-4c.md
git commit -m "docs(docs): Plan display delivery 4c, graph, menu and history."
```

---

### Task 1: `SteamiHistory<N>`

**Files:**
- Create: `lib/steami_board/src/SteamiHistory.h`
- Modify:
  - `lib/steami_board/src/SteamiScreen.h` (add `#include "SteamiHistory.h"` only);
  - `tests/native/test_steami_screen/test_main.cpp` (new section `// ---- SteamiHistory (4c) ----` and its `RUN_TEST`s);
  - `scripts/tests/test_build_arduino_package.py` (`test_umbrella_includes_display_headers` lists `"SteamiHistory.h"`);
  - `scripts/tests/test_steami_display_rules.py` (compile rules).

**Interfaces:**
- Consumes: nothing new. `_compiles(body, header)` of `test_steami_display_rules.py`.
- Produces, in `SteamiHistory.h` (includes `<math.h>`, `<stddef.h>`, `<stdint.h>`; forward-declares `class SteamiScreen;`):
  ```cpp
  template <size_t N> class SteamiHistory {  // ring of the last N values, no heap, 4N + 4 bytes
      static_assert(N >= 1 && N <= 65535, "SteamiHistory holds 1 to 65535 values");
     public:
      void add(float v);
      size_t size() const;                       // values stored, <= N
      static constexpr size_t capacity() { return N; }
      float operator[](size_t i) const;          // 0 = oldest; NAN past size()
      float last() const;                        // NAN when empty
      void clear();
     private:
      friend class SteamiScreen;                 // graph() reads the ring in place (D1)
      size_t first() const;                      // index in _data of the oldest value
      float _data[N] = {};
      uint16_t _head = 0, _count = 0;            // _head: the slot the next add() writes
  };
  ```
  Each member has a `///` comment in the style of `SteamiScreen.h`; the class comment says what a student needs: "keeps the last N readings for `screen.graph()`; a new value pushes out the oldest".

- [ ] **Step 1: Write the failing tests** (screen suite):

```cpp
[[maybe_unused]] static constexpr SteamiHistory<3> kConstHistory{};  // constant-initialised (D2)
static_assert(sizeof(SteamiHistory<1>) == 8 && sizeof(SteamiHistory<20>) == 84 &&
              sizeof(SteamiHistory<40>) == 164, "4N + 4 bytes (spec 5.12)");
static_assert(SteamiHistory<20>::capacity() == 20, "capacity() is a constant expression");

static void test_history_starts_empty(void) {
    // SteamiHistory<20> h: size() 0, isnan(last()), isnan(h[0]); after clear() of a filled one the same
}
static void test_history_keeps_the_last_n(void) {
    // add 1..25 to SteamiHistory<20>: size() 20, h[0] == 6, h[19] == 25, last() == 25, isnan(h[20]);
    // add 1..3 only: size() 3, h[0] == 1, h[2] == 3, isnan(h[3]); SteamiHistory<1>: add 7, 8 -> h[0] == 8
}
static void test_history_counts_nan(void) {
    // add 1, NAN, 3: size() 3, isnan(h[1]), last() == 3; add NAN: isnan(last()), size() 4
}
static void test_history_at_65535(void) {   // Review Focus 4
    // static SteamiHistory<65535> h; add i for i = 0..65536 (65537 values): size() 65535, h[0] == 2,
    // h[65534] == 65536, last() == 65536; under native_asan no runtime error
}
```

In `test_steami_display_rules.py`, `test_history_capacity_bounds`:
- positive control: `_compiles("SteamiHistory<1> a; SteamiHistory<65535> b; (void)a; (void)b;", "SteamiHistory.h")` is true;
- `SteamiHistory<0> a; (void)a;` and `SteamiHistory<65536> a; (void)a;` do not compile.

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`SteamiHistory` not declared). Run: `make test-tooling` — Expected: FAIL (the header is missing).

- [ ] **Step 2: Implement `SteamiHistory.h`** as D2 says. `first()` is `(_head + N − _count) % N`, computed in `size_t`. Add the include to `SteamiScreen.h` (examples include `<SteamiScreen.h>`, not `STeaMi.h`).

- [ ] **Step 3: Verify.**
  - `.venv/bin/pio test -e native -f native/test_steami_screen`: PASS.
  - `.venv/bin/pio test -e native_asan`: PASS, no `runtime error:`, no sanitizer report.
  - `make test-tooling`: OK, `test_umbrella_includes_display_headers` included.
  - `mk tidy`, `mk format-check`, `scripts/check-spdx.sh`: clean.
  - ruff on the two `.py` files: clean.
  - The 128 px check: 14 `OK`.

- [ ] **Step 4: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiHistory.h lib/steami_board/src/SteamiScreen.h \
  tests/native/test_steami_screen/test_main.cpp scripts/tests/test_build_arduino_package.py \
  scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add SteamiHistory, a ring of the last N values."
```

---

### Task 2: `graph()`: box, axes, middle line, value and two axis labels

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `SteamiScreen.h`, `SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `tests/native/test_steami_screen/golden_layout.py`

**Interfaces:**
- Consumes:
  - Task 1's `SteamiHistory<N>` and its private `_data`, `_count`, `first()`;
  - the kernel:
    - `Run`, `Region`;
    - `FaceId faceFor(const char*, Ink, uint8_t)`, `static uint8_t replication(FaceId, uint8_t)`;
    - `static uint32_t inkRows(const char*, size_t, Ink, FaceId)`, `Region chord(int32_t, int32_t)`;
    - `size_t fitGlyphs(const Run&, Region*, int32_t)`, `int32_t cellWidth(const Run&, size_t)`;
    - `void drawGlyphs(const Run&, int32_t, SteamiRgb)`, `static size_t glyphCount(const char*)`;
    - `int32_t u(int32_t)`, `uint8_t k()`, `bool widgetCanDraw()`;
  - `SteamiDisplay::text(const char*, int16_t, int16_t, SteamiRgb, uint8_t)`, `int16_t textWidth(const char*, uint8_t) const`, `rect(...)`, `strokeLine(int16_t, int16_t, int16_t, int16_t, uint8_t, SteamiRgb)`;
  - `SteamiValue(long)`, `size_t SteamiValue::format(char*, size_t, uint8_t) const`;
  - golden: `Panel`, `face_for`, `ink_rows`, `replication`, `lowest_bit`, `highest_bit`, `cell_width`, `draw_glyphs`, `gauge_label_text`, `r_span`, `r_stroke_line`, `r_text`, `box`, `f32`.
- Produces:
  - D15's graph constants;
  - public `template <size_t N> void graph(const SteamiHistory<N>& h, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT)` (inline, D1) and `void graph(const float* data, size_t n, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT)`. Their doc comment tells a student: "a box with the curve of the values from the oldest (left) to the newest (right), the integer part of the newest above it, minV and maxV written inside the box; a NAN leaves a gap; values outside [minV, maxV] stay on the edge".
  - protected `struct Series` (D1);
  - private:
    - `void graphSeries(const Series& s, float minV, float maxV, SteamiRgb c);`, the widget behind both overloads;
    - `void graphValue(float v);`
    - `void graphLabel(float v, bool top);`, which formats into `char[16]` and builds the LABEL `Run` at k;
    - `void drawGraphLabel(Run& t, bool top);`, which sets `t.y` and x by D6, cuts and draws in DARK;
    - `void graphFrame();`, the dashes and axes of D3;
    - a stub `void graphCurve(const Series& s, float minV, float maxV, SteamiRgb c);` that draws nothing until Task 3;
  - anonymous-namespace `void integerText(float v, char* out, size_t size)` (D5), now also used by `gaugeLabel()`;
  - golden: `graph_layout(W, min_v, max_v)`, which returns `{"box": (gx, gy, gw, gh), "value": (x, y), "labels": [(text, x, top), (text, x, top)], "dashes": [(x0, x1)], "dash_rows": (y0, y1)}`, and `graph_frame(cv, W, min_v, max_v, value_text)`, which paints the value, the labels, the dashes and the axes onto a canvas.

- [ ] **Step 1: Golden section** `# ---- Widget of 4c Task 2: graph() frame, value and labels (spec 5.9 "graph", B1; font design 5.3) ----`.
  - Self-check asserts (the spec and the font design):
    - box 21/38/92/52 and 39/71/173/97;
    - value row 31 / 58;
    - label x 24 / 44;
    - for "1050", "950", "1000" and "0": max cell top 40 / 73, min cell top 81 / 155, ink 74..81 and 156..163 on 240;
    - label widths "1050" 24 px and "950" 18 px on 240;
    - dash and gap 3 / 5;
    - dash rows 64..64 / 119..120;
    - the first dash at x 22 / 41, the last cut at `gx + gw − 1` = 112 / 211;
    - nothing outside the circle for `graph_frame` with labels `(950, 1050)`, `(0, 1000)`, `(-1e8, 1e8)` and `(NAN, INF)` (two "--").
  - Prints, for `(950, 1050)`, `(0, 1000)` and `(NAN, INF)` on 128 and 240:
    - the DARK ink box of each label;
    - the label cells (x, top);
    - the axis pixel boxes;
    - the dash spans.
- [ ] **Step 2: Write the failing tests** (section `// ---- graph (4c) ----`):

```cpp
static void test_graph_frame_golden(void) {
    // forEachPanel, SteamiHistory<20> h (empty): graph(h, 950, 1050): the GRID_DARK pixels are
    // exactly the printed dash spans on the printed dash rows; the DARK pixels outside the box
    // interior (x < gx + k or y >= gy + gh) are exactly the two axes (printed boxes); no LIGHT pixel
}
static void test_graph_value(void) {
    // forEachPanel: the WHITE mask of graph(...) equals that of
    // text(t, centerX() - textWidth(t) / 2, u(31), WHITE, 1) alone, for: empty history -> "--";
    // one point 1013.7f -> "1013"; -0.5f -> "0"; -12.9f -> "-12"; 1e9f -> "--"; last value NAN -> "--";
    // the WHITE ink top row is in 31..38 (128) / 58..73 (240); one point draws no LIGHT pixel
}
static void test_graph_two_labels(void) {
    // forEachPanel, empty history: for (950, 1050) and (0, 1000), the DARK pixels of the box interior
    // (gx + k <= x <= gx + gw - 1, gy <= y < gy + gh) equal textSmall(max, 24|44, 40|73, DARK, 1) and
    // textSmall(min, 24|44, 81|155, DARK, 1) drawn alone (a LABEL is textSmall(..., 1), font design 7):
    // exactly two labels, "1050"/"950" and "1000"/"0"; both DARK ink boxes inside the box;
    // labels (NAN, INFINITY): two "--" at the printed cells
}
static void test_graph_null_data(void) {   // Review Focus 3
    // forEachPanel: graph(nullptr, 5, 950, 1050) == graph(empty SteamiHistory<4>, 950, 1050);
    // graph(data, 0, ...) the same
}
```

Extend the message tests (Review Focus 5):
- `test_widgets_without_a_screen`: a fresh `TestScreen g(board)` calls `g.graph(history, 950, 1050)`; its status is `UNKNOWN_BOARD`, and `countLogged("(E30)")` stays 1.
- `test_screen_e31_message`: `s.graph(history)` and `s.graph(data, 3)` leave the counters at 0.

`test_long_strings_stay_inside` gains `graph(h, v, -v)` for v ±1e8 and `graph(h, 1e9f, -1e9f)`: `outsideCircle() == 0` and DARK ink present.

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`graph` not a member).

- [ ] **Step 3: Implement** the constants, `Series::at`, `integerText` (with `gaugeLabel()` calling it), both public overloads, `graphSeries`, `graphValue`, `graphLabel`, `drawGraphLabel` and `graphFrame`, by D1, D3, D5 and D6.
  - `graphSeries`:
    - `if (!widgetCanDraw()) return;`
    - `graphValue(s.count == 0 ? NAN : s.at(s.count − 1))`;
    - `graphLabel(maxV, true)`, then `graphLabel(minV, false)`;
    - `graphFrame()`;
    - `graphCurve(s, minV, maxV, c)`.
  - Helpers recompute the box from the constants; no box struct travels through the frames.
- [ ] **Step 4: Verify.**
  - `mk test-native`: every suite PASSED.
  - `.venv/bin/pio test -e native_asan`: PASS, no `runtime error:`.
  - The golden script: exit 0.
  - `mk tidy`, `mk format-check`: clean. ruff on `golden_layout.py`: clean.
  - The 128 px check: 14 `OK` (`gaugeLabel` refactor included).
  - The 128 literal check: no changed 128 literal; the gauge tests' 240 literals are unchanged too (D5).
  - The stack measurement (ScreenTemperature):
    - `graph(const float*, ...)`, `graphSeries`, `graphValue`, `graphLabel`, `drawGraphLabel` ≤ 64;
    - `gaugeLabel` ≤ 64 (was 48) and `gauge` unchanged;
    - `graphFrame` recorded.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add graph() with its box, value and two axis labels."
```

---

### Task 3: the `graph()` curve

**Files:**
- Modify: `lib/steami_board/src/SteamiScreen.cpp` (and `SteamiScreen.h` if a `noinline` is needed)
- Modify: `tests/native/test_steami_screen/test_main.cpp` (`TestScreen` gains `using SteamiDisplay::strokeLine;`), `golden_layout.py`

**Interfaces:**
- Consumes: Task 2's `Series`, `graphSeries`, the `graphCurve` declaration, the box of D3, `strokeLine(...)`; golden `graph_layout`, `graph_frame`, `r_stroke_line`, `f32`.
- Produces:
  - `void graphCurve(const Series& s, float minV, float maxV, SteamiRgb c);` implemented (D4);
  - anonymous-namespace `bool curveRow(float v, float minV, float maxV, int32_t top, int32_t h, int32_t* row)`, false for a skipped point;
  - golden:
    - `graph_points(W, values, min_v, max_v)` gives `[(x, y) or None]`, mirroring D4 in float32;
    - `graph_render(W, values, min_v, max_v, c="LIGHT")` gives a canvas: `graph_frame` plus the segments through `r_stroke_line` at k.

- [ ] **Step 1: Golden section** `# ---- Widget of 4c Task 3: graph() curve (spec 5.9, 7; D4) ----`.
  - Self-check asserts:
    - with 20 values the x of point i is `gx + i·gw // 19`, and the last point is at 113 / 212;
    - 20 × 950 on (950, 1050) lies on row `gy + gh` and 20 × 1050 on row `gy`;
    - `graph_render` has nothing outside the circle for each history of spec 7 (empty, full at 950, full at 1050, full of 2000 and −5, of ±INF) on both panels.
  - Prints:
    - the LIGHT box of the ramp `[50·i for i in range(20)]` on (0, 1000);
    - the LIGHT box of the mockup 05 values (D13) on (0, 1000);
    - both on 128 and 240.
- [ ] **Step 2: Write the failing tests:**

```cpp
static void test_graph_flat_and_clamped(void) {
    // forEachPanel, SteamiHistory<20> full of v, graph(h, 950, 1050): the LIGHT mask equals
    // strokeLine(gx, row, gx + gw, row, k, LIGHT) alone, row = gy + gh for v 950, -5, -INFINITY and
    // row = gy for v 1050, 2000, INFINITY (clamped, spec 7)
}
static void test_graph_curve_golden(void) {
    // Values printed by golden_layout.py: LIGHT boxes of the ramp and of the mockup 05 values on
    // (0, 1000) on 128 grey and 240 colour; with 2 points the curve runs from x gx to gx + gw
    // (a partial history stretches across the box)
}
static void test_graph_nan_gap(void) {
    // forEachPanel, 20 values of 950 with NAN at index 5: LIGHT mask == strokeLine(x0..x4) and
    // strokeLine(x6..x19) on row gy + gh, drawn alone (x_i = gx + i * gw / 19); NAN at 0 and 19:
    // strokeLine(x1..x18) only; {NAN, 950, NAN}: no LIGHT pixel (an isolated point draws nothing);
    // one value: no LIGHT pixel
}
static void test_graph_ring_equals_array(void) {
    // forEachPanel: a SteamiHistory<20> after 25 adds (ramp) renders equal (all pixels) to
    // graph(last20, 20, ...) of a float array holding its last 20 values in order
}
static void test_graph_extremes(void) {   // Review Focus 2
    // forEachPanel, no crash and nothing outside: (minV, maxV) = (500, 500) with values 499, 500, 501
    // (span 1: bottom, bottom, top); (1050, 950) with 20 x 950 -> row gy (inverted); (NAN, 1050) and
    // (950, NAN): no LIGHT pixel, the NAN label "--"; (-FLT_MAX, FLT_MAX) and values FLT_MAX,
    // -FLT_MAX, INFINITY, -INFINITY, NAN mixed; every case also runs under native_asan
}
static void test_graph_many_points(void) {   // Review Focus 4
    // static float data[70000] (ramp 0..1): graph(data, 70000, 0, 1) on 128 and 240: LIGHT ink only
    // inside [gx, gx + gw + k - 1] x [gy, gy + gh + k - 1], nothing outside the circle
}
```

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: FAIL (no LIGHT pixel).

- [ ] **Step 3: Implement** `curveRow` and `graphCurve` by D4. Points are computed in `int32_t` from `uint64_t` products and cast to `int16_t` only at the `strokeLine` call.
- [ ] **Step 4: Verify** as Task 2 Step 4. Stack: `graphSeries` still ≤ 64; `graphCurve` recorded (report it to the controller if it is above 64 B).
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Draw the graph() curve, with a gap at each NAN."
```

---

### Task 4: `menu()`

**Files:**
- Modify: `lib/steami_board/src/SteamiScreenConst.h`, `SteamiScreen.h`, `SteamiScreen.cpp`
- Modify: `tests/native/test_steami_screen/test_main.cpp`, `golden_layout.py`

**Interfaces:**
- Consumes:
  - the kernel of Task 2's list;
  - `SteamiRgb lifted(SteamiRgb, SteamiRgb)`, `rowsChord(uint32_t, const Run&)` (private), `int32_t glyphAdvance(Ink, uint8_t)`;
  - golden `Panel`, `text_layout` conventions, `draw_glyphs`, `r_span`, `cell_width`, `ink_rows`.
- Produces:
  - D15's menu constants;
  - public `template <size_t N> int menu(const char* const (&items)[N], int selected = 0, SteamiRgb c = STEAMI_WHITE)` (inline, D11) and `int menu(const char* const* items, size_t n, int selected = 0, SteamiRgb c = STEAMI_WHITE)`. The doc comment says:
    - the index wraps (−1 is the last item) and is returned;
    - 5 rows show on the 128 px screen and 6 on the 240 px one, scrolling with the selection;
    - keep the index in an `int` (D11).
  - private:
    - `int32_t menuRows(size_t n);`, the visible rows (D7);
    - `void menuRow(const char* item, int32_t top, bool marked, SteamiRgb c);`, which draws the bar when marked, then the marker and the item (D8 to D10);
    - `void menuBar(int32_t top);`, where `top` is the item row and the bar starts `u(2)` above it;
  - golden `menu_layout(W, items, selected)` returns `{"selected": wrapped, "visible": v, "start": s, "rows": [(index, top, x, glyphs)], "marker": (x, top) or None, "bar": {y: (x0, x1)}}`, and `menu_render(W, items, selected, c="WHITE")` returns a canvas.

- [ ] **Step 1: Golden section** `# ---- Widget of 4c Task 4: menu() (spec 5.9 "menu", B3; font design 5.3) ----`.
  - Self-check asserts:
    - row tops 35, 49, 63, 77, 91 and 65, 91, 118, 144, 170, 196;
    - full-cell chords 115, 125, 127, 121, 109, 85 and 213, 233, 239, 227, 201, 157;
    - visible 5 / 6;
    - the bar of row 6 on 240 is rows 193..218;
    - each bar row spans exactly `[chord left + k, min(chord right − k, W − 1)]`;
    - the marker x equals the chord's left edge at the selected item's inked rows (the marker's own when the item inks none) plus `u(2)`;
    - for the six items of 11 characters `kEleven` = {"Temperature", "Humidity_RH", "Distance_mm", "Light_level", "Battery_pct", "Proximity_1"}, at every selected index and on both panels, every item is whole and nothing is outside the circle;
    - wraps −1 → 5, 6 → 0, 13 → 1, INT_MIN → 4, INT_MAX → 1.
  - Prints, on 128 and 240:
    - `menu_layout` of `kEleven` at selected 0..5, and of `kSix` at selected 3 (the short item "Light");
    - eight items "Item0".."Item7" at selected 0, 4 and 7 (start 0 / 2 / 3 on 128, 0 / 1 / 2 on 240);
    - a 40-glyph item (its kept glyphs);
    - the visible rows on 96 and 200 px (0 and 6).
- [ ] **Step 2: Write the failing tests** (section `// ---- menu (4c) ----`). The suite gains `static const char* const kSix[6] = {"Temperature", "Humidity", "Distance", "Light", "Battery", "Proximity"};` (program 06's items) and `kEleven`:

```cpp
static void test_menu_wraps(void) {   // Review Focus 4
    // on 128 and 240: menu(kSix, -1) == 5, 6 -> 0, 13 -> 1, 0..5 unchanged, INT_MIN -> 4, INT_MAX -> 1;
    // menu(kSix, -1) renders equal to menu(kSix, 5); with const char* const* p = kSix,
    // menu(p, 0, 3) (n == 0) returns 0 and lights nothing
}
static void test_menu_visible_rows_and_window(void) {
    // Values printed by golden_layout.py: eight items, selected 0, 4, 7: the drawn items are exactly
    // the printed (index, top, x) rows (each GRAY or WHITE ink box at its row), 5 rows on 128 and 6
    // on 240; on 128 nothing lit from row 105 down (below the 5th row's bar)
}
static void test_menu_bar_follows_the_circle(void) {
    // forEachPanel, kEleven selected at each index 0..5 (spec 7): for every bar row y the DARK pixels
    // run exactly from chord(y, y).left + k to smaller(chord(y, y).right - k, W - 1) (the text never
    // covers either end); no DARK pixel on the rows just above and below the bar; nothing outside
}
static void test_menu_marker_and_items(void) {
    // Values printed by golden_layout.py, kEleven at each index on 128 grey and 240 colour: the WHITE
    // mask equals text("> ", marker x, top, WHITE, 1) and text(item, item x, top, WHITE, 1) drawn
    // alone; marker x - u(2) is the chord's left edge at the item's inked rows (spec 7); a long
    // selected item ("Temperature", kEleven at 0) starts right after the marker; kSix at 3: the short
    // selected item "Light" is centred at the printed x; unselected items are GRAY at centerX() - w / 2
}
static void test_menu_contrast(void) {
    // spec 5.10: 128 grey, menu(kSix, 2, STEAMI_BLUE): every pixel of the selected line's text is
    // SteamiRgb::gray(10) and the bar stays DARK; default WHITE stays 15; 240 colour: STEAMI_BLUE exactly
}
static void test_menu_array_kinds(void) {
    // const char* const A[6] and const char* B[6] of the same strings: equal returns and renders
    // for every index (spec 7)
}
static void test_menu_index_types(void) {   // Review Focus 1
    // menu(A, int 2), (uint8_t)2, (int16_t)2, (uint16_t)2 and true each draw all six items (the
    // template): the lit mask equals that of menu(A, 2) / menu(A, 1); a comment states that unsigned, long and
    // size_t pick menu(items, n) (D11)
}
static void test_menu_null_and_empty_items(void) {   // Review Focus 3
    // forEachPanel: menu(nullptr, 3, 1) returns 1 and menu(nullptr, 3, -1) returns 2, drawing
    // nothing (D7); items {"A", nullptr, "", "B"}
    // selected 1 and 2: no crash, the marker drawn, nothing for the empty item
}
static void test_graph_and_menu_other_panels(void) {   // Review Focus 5
    // onPanel(96, ...) and onPanel(200, ...), grey and colour: menu(kEleven, i) for i 0..5 and
    // graph(h, 950, 1050) with h full of a ramp: no crash, nothing outside (onPanel); the returns wrap;
    // on 96 menu() lights no pixel (visible 0, printed by golden_layout.py)
}
```

Extend:
- `test_widgets_without_a_screen`: a fresh `TestScreen m(board)` with `m.menu(kSix, -1)` returns 5; status `UNKNOWN_BOARD`; E30 count stays 1.
- `test_screen_e31_message`: `s.menu(kSix, 7)` returns 1; the counters stay 0.
- `test_long_strings_stay_inside`: `menu()` of the two 40-glyph strings with `nullptr`, selected 0 and 1: `outsideCircle() == 0`.

Run: `.venv/bin/pio test -e native -f native/test_steami_screen` — Expected: build error (`menu` not a member).

- [ ] **Step 3: Implement** the constants, both `menu` overloads, `menuRows`, `menuRow` and `menuBar`, by D7 to D11.
  - The public `menu` keeps only the index, visible, start and loop values.
  - Each row is `menuRow(items[i], u(STEAMI_MENU_TOP + STEAMI_MENU_PITCH * row), i == s, i == s ? lifted(c, STEAMI_DARK) : STEAMI_GRAY)`.
- [ ] **Step 4: Verify on the target compiler.** Build a scratch sketch that the repository never sees: `$SCRATCH/menu-bind/menu-bind.ino` holds `SteamiScreen screen; const char* ITEMS[] = {"a", "b"};`, and its `loop()` calls `screen.menu(ITEMS, 1);`. Build it with `PLATFORMIO_BUILD_DIR="$SCRATCH/menu-bind-build" PLATFORMIO_SRC_DIR="$SCRATCH/menu-bind" .venv/bin/pio run -e steami`. Expected: `SUCCESS` (the template binds on STM32duino's GCC; no second template, D11). Delete both directories afterwards.
- [ ] **Step 5: Verify** as Task 2 Step 4. Stack: `menu(const char* const*, ...)`, `menuRows`, `menuRow`, `menuBar` ≤ 64.
- [ ] **Step 6: Commit** (the commit check first)

```bash
git add lib/steami_board/src/SteamiScreen* tests/native/test_steami_screen/
git commit -m "feat(board): Add menu(), wrapped, with a bar cut to the circle."
```

---

### Task 5: Mockups 05 and 06, the review list and the 128 px hashes

**Files:**
- Modify:
  - `tests/native/helpers/SteamiMockups.h`. Its header comment "mockups 01 to 04 and 07 to 09" becomes "mockups 01 to 09".
  - `tests/native/test_steami_screen_mockups/test_main.cpp`. Its comment at L20-23 changes the same way, and the `(4b Task 5)` reference gains `(4c Task 5)`.
  - `tests/native/test_steami_screen/golden_layout.py`.
  - `scripts/mockup-review.py`, `scripts/tests/test_mockup_review.py` (only if a count is literal).
  - `tests/native/test_steami_display/ppm-128.sha256` (two lines appended, D13).

**Interfaces:**
- Consumes:
  - `graph(...)`, `menu(...)`, Task 1's `SteamiHistory`, 4a's `title`, `subtitle`;
  - the suite's `render(name, size, color, draw)`, `onPanel`, `maskOf`, `maskAlone`, `any`, `expectShownOnce`, `inkBox`, `expectBox`, `expectPixel`;
  - golden `graph_render`, `menu_render`, `m_title`, `m_subtitle`, `band_box`, `PINNED_MOCKUPS`, `PINNED_POINTS`, `mockup_render`.
- Produces, in `SteamiMockups.h` (each starts with `screen.clear();`, ends with `screen.show();`, its closing `}` at column 0):
  - `inline void mockupGraph(SteamiScreen& screen, const SteamiHistory<20>& history, bool hasLight, const char* chip)`. Its body is program 05's `loop()` from `screen.clear();` on, with braces:
    - `screen.title(hasLight ? "Light" : "Pressure");`;
    - `if (hasLight) { screen.graph(history, 0, 1000); } else { screen.graph(history, 950, 1050); }`;
    - `screen.subtitle(chip, "20s window");`;
    - with section 6's comments.
  - `constexpr const char* const MENU_ITEMS[6] = {"Temperature", "Humidity", "Distance", "Light", "Battery", "Proximity"};`
  - `inline void mockupMenu(SteamiScreen& screen, int& selected)`: `screen.title("Menu");` then `selected = screen.menu(MENU_ITEMS, selected);  // wraps: -1 becomes 5`.
  - golden `mockup_05(W, values, chip, has_light=True)` and `mockup_06(W, selected)`. `mockup_render` gains "05" and "06".
  - `MOCKUPS` with 15 names: `"05_scrolling_graph", "06_dpad_menu"` inserted after `"04_circular_gauge"`.

- [ ] **Step 1: Golden mockup section** `# ---- Mockups 05 and 06 (4c Task 5) ----`.
  - Add to `PINNED_MOCKUPS`:
    - 05: GRAY title, WHITE "847", LIGHT curve; DARK labels inside the box rows; DARK subtitle on rows 95..113 / 177..213;
    - 06: GRAY title band, GRAY items, WHITE selected line, DARK bar.
  - Add to `PINNED_POINTS`: 05's last curve point, and the bar ends on its middle row.
  - Assert:
    - the 05 subtitle lines at 95/106 (128) and 177/197 (240) (spec L546);
    - the 06 visible rows 5 / 6, with "Distance" selected at row 3;
    - nothing outside the circle in either render.
  - Print the 240 boxes of `05_pressure_v1c` (labels "1050" and "950" inside the box, font design 6: 24 and 18 px).
- [ ] **Step 2: Write the failing suite:**

```cpp
static void test_mockup_05_graph(void) {
    // render("05_scrolling_graph", 128, false / 240, true): history of D13's 20 values,
    // mockupGraph(s, history, true, "APDS-9960"); the golden boxes and points
}
static void test_mockup_05_pressure_v1c(void) {
    // render("05_pressure_v1c", 240, true): 20 x 1013.0f, mockupGraph(s, h, false, "BME280"); the
    // DARK label boxes printed for "1050" and "950", inside the box and the circle; "Pressure" GRAY
}
static void test_mockup_06_menu(void) {
    // render("06_dpad_menu", ...): int selected = 2; mockupMenu(s, selected); selected stays 2;
    // the golden boxes; 5 GRAY/WHITE item rows on 128, 6 on 240
}
```

`test_mockups_without_sensors` grows (Review Focus 2, on both panels):
- `mockupGraph(s, history full of NAN, true, "APDS-9960")`: no LIGHT pixel; the WHITE mask equals that of an empty history ("--").
- `mockupMenu(s, sel)` with `sel = -1`: `sel` becomes 5.

Run: `.venv/bin/pio test -e native -f native/test_steami_screen_mockups` — Expected: build error (`mockupGraph` not declared).

- [ ] **Step 3: Write the helpers**, set `MOCKUPS`, and check `test_mockup_review.py`: its row and PNG counts already follow `len(MOCKUPS)`, so it needs no edit unless a literal count remains.
- [ ] **Step 4: Verify the renders and pin the two new 128 px hashes.**
  - Run `mkdir -p "$SCRATCH/mockups" && STEAMI_PPM_DIR="$SCRATCH/mockups" .venv/bin/pio test -e native -f native/test_steami_screen_mockups`. Expected: PASS, 30 mockup PPMs plus `05_pressure_v1c`.
  - Run `python3 scripts/mockup-review.py --ppm-dir "$SCRATCH/mockups" --out "$SCRATCH/review" --refs /home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups`. Expected:
    - exit 0;
    - `ssim.csv` has 31 lines;
    - 30 PNGs;
    - note the 05 and 06 SSIM at 128 and 240 for the PR (indicative, about 0.7, not a gate).
  - **The 13 old mockups are unchanged, byte for byte** (ppm-128.sha256 pins 128 px only; this also covers 240).
    - Baseline: `mkdir -p "$SCRATCH/base" && git archive 60dc6b0 | tar -x -C "$SCRATCH/base"`, rendered with `mkdir -p "$SCRATCH/mockups-base" && STEAMI_PPM_DIR="$SCRATCH/mockups-base" .venv/bin/pio test -d "$SCRATCH/base" -e native -f native/test_steami_screen_mockups`.
    - `for f in $(cd "$SCRATCH/mockups-base" && ls steami-mockup-*.ppm); do cmp "$SCRATCH/mockups-base/$f" "$SCRATCH/mockups/$f"; done`: silent, and the list holds the 26 PPMs of the 13 old names (128 and 240; count them). The PR reports "26 unchanged renders (byte-identical)".
    - Never run the worktree's `mockup-review.py` on the baseline folder: it lacks 05 and 06, so the script exits 1. If a before SSIM table is wanted, run the baseline's own `python3 "$SCRATCH/base/scripts/mockup-review.py" --ppm-dir "$SCRATCH/mockups-base" --out "$SCRATCH/review-base" --refs /home/nedjar/sandbox/micropython-steami-tutorials/docs/mockups`.
    - Delete the tree, `mockups-base` and its build directory afterwards.
  - Look at `05_scrolling_graph-240.png` and `06_dpad_menu-240.png` against the refs. A difference outside spec section 6's known differences goes back to Tasks 2 to 4.
  - Regenerate `ppm-128.sha256` with 4b-font's command: render as in the 128 px check (`PPM` fresh, both suites), then `(cd "$PPM" && sha256sum *-128-gray.ppm) > tests/native/test_steami_display/ppm-128.sha256`. Expected: `git diff -U0 -- tests/native/test_steami_display/ppm-128.sha256` shows exactly two added lines (`steami-mockup-05_scrolling_graph-128-gray.ppm` and `steami-mockup-06_dpad_menu-128-gray.ppm`) and no removed line.
  - The 128 px check: 16 `OK`, with `native_asan` too (`STEAMI_PPM_DIR` set, then the same `sha256sum -c`).
  - `make test-tooling`: OK. ruff on the changed `.py` files: clean. `mk format-check`: clean.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add tests/native/helpers/SteamiMockups.h tests/native/test_steami_screen_mockups/ \
  tests/native/test_steami_screen/golden_layout.py tests/native/test_steami_display/ppm-128.sha256
git commit -m "test(tests): Render mockups 05 and 06 at 128 and 240 px."
git add scripts/mockup-review.py scripts/tests/test_mockup_review.py
git commit -m "feat(tooling): Score mockups 05 and 06 in the review."
```

---

### Task 6: Examples ScreenGraph and ScreenMenu

**Files:**
- Create: `lib/steami_board/examples/ScreenGraph/ScreenGraph.ino`, `lib/steami_board/examples/ScreenMenu/ScreenMenu.ino`
- Modify: `scripts/tests/test_steami_display_rules.py`

**Interfaces:**
- Consumes:
  - Task 5's `mockupGraph`, `mockupMenu`, `MENU_ITEMS`;
  - `SteamiLight`: `bool begin()`, `uint16_t read()` (0 when missing), `const char* chip()`;
  - `SteamiPressure`: `bool begin()`, `float read()` (hPa, NAN when missing), `const char* chip()`;
  - `Steami.has(SteamiFunction::LIGHT)` (`SteamiBoard.h`);
  - `SteamiButtons`: `bool begin()`, `bool wasPressed(SteamiButton)`, with `SteamiButton::UP` and `DOWN`.
- Produces:
  - the two examples;
  - the guard's widened `_calls()` (D12), `test_menu_items_match_the_mockup`, and the pairs `("ScreenGraph", "mockupGraph")` and `("ScreenMenu", "mockupMenu")`.

- [ ] **Step 1: Widen the guard first** (one commit, green on its own).
  - `_calls()` follows D12.
  - `test_calls_tokens` gains positive controls:
    - `if (c) screen.graph(h, 950, 1050);` gives `("graph", ["950", "1050"])`;
    - `selected = screen.menu(ITEMS, selected);` gives `("menu", [])`;
    - `x = notscreen.foo(1);` gives nothing.
  - Run `make test-tooling`: OK. Every existing pair still passes, so no current example statement changes meaning. Run ruff.
  - Commit (the commit check first):

```bash
git add scripts/tests/test_steami_display_rules.py
git commit -m "test(tests): Read screen calls after an if or an assignment."
```

- [ ] **Step 2: Write the failing guard pairs** (`test_screen_examples_draw_the_mockups` gains the two pairs) and `test_menu_items_match_the_mockup`. Run `make test-tooling`. Expected: FAIL (examples missing).
- [ ] **Step 3: Write the examples.** Each has a header comment in the 4b style:
  - the mockup it reproduces (`05_scrolling_graph_ref.png`, `06_dpad_menu_ref.png`);
  - the same sketch on V1a/V1b (128 px grey) and V1c (240 px colour);
  - `screen.begin()` first;
  - what a missing sensor shows.

  The globals and `loop()` follow program 05 and program 06 of section 6, under 4b Ruling 2:
  - `#include <Arduino.h>`, `<SteamiBoard.h>` (ScreenGraph, for `Steami.has`), the sensor headers and `<SteamiScreen.h>`;
  - `Serial.begin(115200)` before `screen.begin()`;
  - braces on every `if`.

  **ScreenGraph:**
  - `SteamiHistory<20> history;`;
  - `setup()` is program 05's: `Serial.begin(115200)`, `screen.begin()`, `hasLight = ...` (below), then `pressure.begin()`;
  - `hasLight = Steami.has(SteamiFunction::LIGHT) && light.begin();  // no E20 on V1c`;
  - `delay(1000)`;
  - the comment says: on the V1c, which has no light sensor, the graph shows the pressure of the BME280 from 950 to 1050 hPa; with no sensor answering, a gap and "--".

  **ScreenMenu:**
  - `const char* const ITEMS[]` (the six of program 06);
  - `int selected = 0;`;
  - `buttons.begin()` in `setup()`;
  - `selected = screen.menu(ITEMS, selected);  // wraps: -1 becomes 5`;
  - `delay(20)`;
  - the comment says: UP and DOWN move the bar, the index wraps, keep it in an `int`.
- [ ] **Step 4: Verify.**
  - `make test-tooling`: OK. ruff: clean.
  - `.venv/bin/python3 scripts/test-examples.py --example steami_board/ScreenGraph` and `--example steami_board/ScreenMenu`: `[PASS]` each.
  - `mk format-check`, `scripts/check-spdx.sh`: clean.
  - The stack measurement with `<Sketch>` = ScreenGraph, then ScreenMenu, by the template-wrapper rule of the Global Constraints: a `graph`/`menu` template line (`... [with unsigned int N = 20]`), if present, ≤ 64; if absent, the `nm` check shows no symbol and `loop()`'s frame is recorded as "inlined". The out-of-line frames as in Tasks 2 to 4.
  - `git status --short lib/`: no `*.ino.cpp`.
- [ ] **Step 5: Commit** (the commit check first)

```bash
git add lib/steami_board/examples/ScreenGraph/ lib/steami_board/examples/ScreenMenu/ \
  scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the ScreenGraph and ScreenMenu examples."
```

---

### Task 7: WeatherStation and the footprint gate

**Files:**
- Create: `lib/steami_board/examples/WeatherStation/WeatherStation.ino`
- Modify:
  - `tests/native/helpers/SteamiMockups.h`;
  - `tests/native/test_steami_screen_mockups/test_main.cpp`;
  - `scripts/tests/test_steami_display_rules.py` (the pair `("WeatherStation", "weatherPage")`);
  - `scripts/check-footprint.py`, `scripts/tests/test_check_footprint.py`.

**Interfaces:**
- Consumes:
  - the widgets `title`, `value`, `gauge`, `graph`, `subtitle`, `compass`, and 4b's `mockupCompass(SteamiScreen&, float)` (`SteamiMockups.h`);
  - `SteamiTemperature::read()`, `SteamiHumidity::read()`, `SteamiPressure::read()` (float, NAN when missing);
  - `SteamiCompass::heading()`;
  - `SteamiButtons::wasPressed(SteamiButton::RIGHT | LEFT | A | B)`;
  - `SteamiGesture`: `bool begin()`, and `Direction read(uint32_t timeoutMs = 0)`, where `SteamiGesture::RIGHT` is an unscoped enumerator;
  - `Steami.printInfo()`;
  - `check-footprint.py`'s `SKETCHES`.
- Produces:
  - `constexpr const char* const WEATHER_PAGES[4] = {"Temperature", "Humidity", "Pressure", "Compass"};`;
  - `inline void weatherPage(SteamiScreen& screen, int page, float t, float h, const SteamiHistory<40>& history, float heading)`. Its body is the WeatherStation `loop()` from `screen.clear();` to `screen.show();`, with `PAGES` read as `WEATHER_PAGES` and each sensor call a parameter;
  - `SKETCHES = ("steami_board/Drawing", "steami_board/ScreenGauge", "steami_board/WeatherStation")`.

- [ ] **Step 1: Write the failing tests.**
  - `test_check_footprint.py`: `test_sketches` expects the three-entry tuple. A new assertion checks that each entry resolves to `lib/<lib>/examples/<Name>/<Name>.ino` (the deferred 4b-font minor).
  - The guard pair `("WeatherStation", "weatherPage")`.
  - In the mockups suite:

```cpp
static void test_weather_station_pages(void) {
    // pages 0..3 with t 23.5f, h 60.0f, a SteamiHistory<40> ramp 990..1029, heading 0.0f, each
    // render("weather_<temperature|humidity|pressure|compass>", 128, false / 240, true, ...):
    // page 0: WHITE mask == value(23.5f, "°C") alone; page 1: on 128 grey the SteamiRgb::gray(10)
    // mask is non-empty and a subset of that of gauge(60.0f, 0, 100, STEAMI_BLUE) alone (the BLUE
    // fill lifted to 10, spec 5.10 L563-564; the title may paint over part of it), 240 has
    // STEAMI_BLUE pixels; page 2: RED mask == graph(history, 950, 1050, STEAMI_RED) alone, and the
    // DARK mask of the box interior == that of the same graph() drawn alone (the "1050" and "950"
    // labels, spec 11 item 9; textSmall is protected in this suite; the ramp keeps the curve off the
    // label rows); page 3: all pixels == mockupCompass(s, 0.0f) (north up, mockup 07)
}
```

  Run `make test-tooling` and `.venv/bin/pio test -e native -f native/test_steami_screen_mockups`. Expected: FAIL.
- [ ] **Step 2: Write the example and the helper.**
  - The example is spec section 6's WeatherStation under D14: one statement per line (clang-format), braces on every `if`, `delay(200)`.
  - The header comment says:
    - the same file runs on V1a/V1b/V1c;
    - the pages and their mockups (Temperature 01, Humidity 04, Pressure 05, Compass 07);
    - RIGHT or A for the next page, LEFT or B for the previous one, and a RIGHT swipe on V1a/V1b;
    - on the V1c, `gesture.begin()` prints E20 and returns false, and the sketch goes on;
    - `SteamiHistory<40>` keeps the last 40 pressures with no heap;
    - the differences from the functional-layer version (spec L808-809).
  - Set `SKETCHES`.
- [ ] **Step 3: Verify.**
  - `make test-tooling`: OK.
  - `.venv/bin/pio test -e native -f native/test_steami_screen_mockups`: PASS.
  - `.venv/bin/python3 scripts/test-examples.py --example steami_board/WeatherStation`: `[PASS]`.
  - `.venv/bin/python3 scripts/check-footprint.py`, with `PLATFORMIO_BUILD_DIR` and `PLATFORMIO_BUILD_FLAGS` unset: exit 0, three lines, each under its limits (`.data + .bss + .noinit` ≤ 53248 B, so 140 KB stay free). Record flash, RAM and "B free" for the PR.
  - The stack measurement with `<Sketch>` = WeatherStation, by the template-wrapper rule: a `graph` line with `N = 40`, if present, ≤ 64; otherwise `loop()`'s frame recorded as "inlined".
  - The 128 px check: 16 `OK`.
  - ruff, `mk format-check`, `scripts/check-spdx.sh`: clean.
  - `git status --short lib/`: no `*.ino.cpp`.
- [ ] **Step 4: Commit** (the commit check first)

```bash
git add lib/steami_board/examples/WeatherStation/ tests/native/helpers/SteamiMockups.h \
  tests/native/test_steami_screen_mockups/ scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the WeatherStation example, four screen pages."
git add scripts/check-footprint.py scripts/tests/test_check_footprint.py
git commit -m "feat(tooling): Add WeatherStation to the footprint gate."
```

---

### Task 8: Record the 4c choices in the spec

**Files:** Modify `docs/superpowers/specs/2026-11-25-steami-display-design.md`.

**Interfaces:**
- Consumes: D3 to D11 and D14, and the golden prints of Tasks 2 to 5. Match each edit by its quoted text; the line numbers are those of 60dc6b0.
- Produces: the spec as the single authority for 4c.

- [ ] **Step 1: Apply**, each a short clause in the existing sentence style.
  1. **5.9 `graph` row (L550).**
     - After "Current value (integer part of the last value, `--` if NAN)", add: "; an empty series shows `--` (**MP**: device.py draws no value for empty data and raises on a NAN last value)".
     - After "NAN points are skipped (gap in the curve)", add: "(**MP**: device.py plots a NAN at the top of the box); point i of n sits at `x + i·w/(n − 1)` and a segment joins two consecutive valid points only".
  2. **5.9 `menu` row (L551).**
     - After "at the item's inked rows", add: "(the marker's own when the item inks none)".
     - After "and **returned**", add: "(also when no screen draws it or `items` is `nullptr`)".
  3. **Known differences (L824-827).** "takes about a quarter of the plot width and lies under the curve" becomes "takes about a quarter of the plot width on 128 and a seventh on 240 (24 of 173 px), and lies under the curve".
  4. **Section 7, the hardware bullet at L966.** It becomes: "WeatherStation, unchanged, on V1c: its serial transcript shows the banner and only E20, and each page is photographed (above); no `scripts/qualify.py` case, which flashes its own sketch, resets through OpenOCD, needs a person at the keyboard and parses `key=value` lines (V1a/V1b: decision 6)."
- [ ] **Step 2: Check.**
  - With `F=docs/superpowers/specs/2026-11-25-steami-display-design.md`: `grep -c "the marker's own when the item inks none" $F`, `grep -c "or \`items\` is \`nullptr\`" $F`, `grep -c "a seventh on 240" $F` and `grep -c "resets through OpenOCD" $F` each give 1; `grep -c "quarter of the plot width and lies" $F` gives 0.
  - `mk format-check` is not needed (Markdown only).
- [ ] **Step 3: Commit** (the commit check first)

```bash
git add docs/superpowers/specs/2026-11-25-steami-display-design.md
git commit -m "docs(docs): Record the graph and menu choices of delivery 4c."
```

---

### Task 9: README and keywords

**Files:** Modify `lib/steami_board/README.md` (`## Screen` L465-590, `## Testing` L627) and `lib/steami_board/keywords.txt`.

**Interfaces:**
- Consumes: the public signatures of Tasks 1, 2 and 4; the three examples; D11.
- Produces: the docs of row 4c ("widget reference, vocabulary table").

- [ ] **Step 1: Write.**
  - **Widget table**, after `gauge`:
    - `graph` ×2: default colours "axes and labels `STEAMI_DARK`, curve `STEAMI_LIGHT`", with a one-line description;
    - `menu` ×2: default colours "bar `STEAMI_DARK`, selected `STEAMI_WHITE`, others `STEAMI_GRAY`", described as returning the wrapped index.
  - **A `SteamiHistory<N>` paragraph:**
    - `add`, `size`, `capacity`, `operator[]` (0 = oldest, `NAN` past `size()`), `last` (`NAN` when empty), `clear`;
    - 4N + 4 bytes, no heap, N from 1 to 65535;
    - `screen.graph(history, min, max)`.
  - **Layout rules,** three bullets:
    - graph: only the min and max labels, written in full inside the box; a NAN leaves a gap; values outside the range stay on the edge;
    - menu: 5 rows on 128 and 6 on 240; the bar follows the circle; `-1` wraps to the last item; the index is returned; keep it in an `int`: with a `const char* const` array, a `long`, `unsigned` or `size_t` index calls the `(items, n)` form (D11);
    - the selected text is lifted on the grey screen.
  - **Vocabulary table:** spec L422-423 verbatim:
    - `screen.graph(data, min_val, max_val)` → `screen.graph(history, min, max)` with `SteamiHistory<N>`;
    - `screen.menu(items, selected=i)` → `i = screen.menu(ITEMS, i);` (array size deduced, index wrapped).
  - **Examples table:**
    - `ScreenGraph` (`05_scrolling_graph_ref.png`: light, or pressure on the V1c, over the last 20 s);
    - `ScreenMenu` (`06_dpad_menu_ref.png`: six items, UP and DOWN);
    - `WeatherStation` (no mockup of its own: four pages, as mockups 01, 04, 05 and 07).
  - **`## Testing`:** "mockups 01 to 04 and 07 to 09" becomes "mockups 01 to 09".
  - **`keywords.txt`** (TAB-separated, in the existing blocks):
    - KEYWORD1: `SteamiHistory`;
    - KEYWORD2: `graph`, `menu`, `add`, `size`, `capacity`, `last` (`clear` is already there).
- [ ] **Step 2: Check.**
  - `make package`.
  - `grep -cP '^(SteamiHistory\tKEYWORD1|(graph|menu|add|size|capacity|last)\tKEYWORD2)$' lib/steami_board/keywords.txt` gives 7 (TAB-separated, as arduino-lint checks), and `grep -c SteamiHistory build/arduino-package/keywords.txt` gives at least 1.
  - `grep -n "01 to 04 and 07 to 09" lib/steami_board/README.md` gives no hit.
- [ ] **Step 3: Commit** (the commit check first)

```bash
git add lib/steami_board/README.md lib/steami_board/keywords.txt
git commit -m "docs(board): Document graph, menu and SteamiHistory."
```

---

### Task 10: Full verification

**Files:** none committed. Figures for the PR go to the scratchpad.

**Interfaces:**
- Consumes: every earlier task and the baseline tree `mkdir -p "$SCRATCH/base" && git archive 60dc6b0 | tar -x -C "$SCRATCH/base"`. The task deletes it, and every build directory it made, before it reports (4b-font Ruling 2).
- Produces: the PR figures.

- [ ] **Step 1: Run each check once, one at a time.** All must succeed:
  - `mk test-native`: every suite PASSED (record the count; 4b-font: 786).
  - `STEAMI_PPM_DIR="$SCRATCH/ppm-asan" .venv/bin/pio test -e native_asan`, then `(cd "$SCRATCH/ppm-asan" && sha256sum -c "$ROOT/tests/native/test_steami_display/ppm-128.sha256")`. Expected: PASS, no `runtime error:`, no sanitizer report, 16 `OK`.
  - The golden script: exit 0. Spot-check five printed values against the two suites.
  - `mk lint` (format, SPDX, clang-tidy, ruff): clean.
  - `mk build`: `SUCCESS`.
  - **`mk test-examples`, the only full run of the delivery:** every example PASS (93 with the three new ones; count them).
  - `make test-tooling`: OK.
  - `make package`. Then `ls build/arduino-package/examples/STEAMI_BOARD/{ScreenGraph,ScreenMenu,WeatherStation}/*.ino` lists three files, and `grep -c '#include "SteamiHistory.h"' build/arduino-package/src/STeaMi.h` gives 1.
  - `.venv/bin/python3 scripts/check-footprint.py`: exit 0, three lines.
  - Mockup review locally (Task 5 Step 4 commands): `ssim.csv` with 31 lines, 30 PNGs, 05 and 06 present; the 26 old renders byte-identical to the baseline's (Task 5 Step 4 `cmp` loop).
  - ruff on every `.py` changed in the range (`git diff --name-only origin/main -- '*.py'`): clean.
  - `git status --short lib/`: no `*.ino.cpp`.
- [ ] **Step 2: The stack table.**
  - Run the stack measurement with ScreenGraph, ScreenMenu and WeatherStation.
  - Every bound frame of the Global Constraints list is ≤ 64. For the template wrappers (`graph` with `N = 20` and `N = 40`, `menu` with `N = 6`), apply the template-wrapper rule: a line, if present, ≤ 64; otherwise the `nm` check and the caller's frame recorded as "inlined".
  - Record the other frames next to 4b-font's.
- [ ] **Step 3: Flash and RAM** of Drawing and ScreenGauge, before (`$SCRATCH/base`, 60dc6b0) and after, and of WeatherStation, each build in its own `PLATFORMIO_BUILD_DIR`.
  - Before: `PLATFORMIO_BUILD_DIR="$SCRATCH/fp-base-<Name>" PLATFORMIO_SRC_DIR="$SCRATCH/base/lib/steami_board/examples/<Name>" .venv/bin/pio run -d "$SCRATCH/base" -e steami` (4b-font's command).
  - After: the same build from the worktree into `$SCRATCH/fp-<Name>`, WeatherStation included.
  - Measure each `<dir>/steami/firmware.elf` with `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-size -A -d`, counted as `check-footprint.py`'s `measure()` counts them (flash = flash-addressed sections + `.data`, RAM = `.data + .bss + .noinit`). The after figures may be cross-checked with `check-footprint.py`'s output. Report every figure and delta in these units, never in PlatformIO's own report (about 360 B lower).
  - Expected: Drawing unchanged (it calls no new widget, and gc-sections drops them). ScreenGauge changes by at most the `integerText` refactor, a few bytes.
  - Report against the spec's "code ≈ 14 to 18 KB" (an estimate, not a gate; 4b Ruling 6).
- [ ] **Step 4: Commits.**
  - `npx --no-install commitlint --from origin/main --to HEAD` passes.
  - `git log --format=%s origin/main..` gives headers ≤ 72 characters with scopes `board|tests|tooling|docs`, no `!`, no `[skip ci]`, no attribution lines.
  - `git log --format=%b origin/main.. | grep -c BREAKING` gives 0.
- [ ] **Step 5 (optional): arduino-cli on STM32duino 3.1.0**, offline, in scratch only. Copy `build/arduino-package` to `.../scratchpad/acli/userbr/libraries/STeaMi`, then run `ARDUINO_DIRECTORIES_DATA=.../scratchpad/acli/data ARDUINO_DIRECTORIES_USER=.../scratchpad/acli/userbr arduino-cli compile --fqbn STMicroelectronics:stm32:STeaMi build/arduino-package/examples/STEAMI_BOARD/<Name>` for the three examples. Never use `/home/nedjar/Arduino`. Otherwise the `arduino-package` CI job is the check.

---

### Task 11: Hardware acceptance on the V1c

**Controller-run on hardware, needs a board slot (coordinate with wiki-steami-71 and wiki-steami-30); subagents never flash or open the serial port.**

**Files:** none committed. Sketches, binaries, transcripts and photos stay in `$P/hw4c/` (`P` = `/tmp/claude-1000/-home-nedjar-sandbox-wiki-steami/1346e80f-1de2-4e4c-bae2-38b41b348575/scratchpad`). `hw-*` and `hw12/` hold 4b-font's files: never write there.

**Interfaces:**
- Consumes:
  - the three examples;
  - the 240 px renders and the review page of Tasks 5 and 7: `05_scrolling_graph-240`, `05_pressure_v1c`, `06_dpad_menu-240`, and `weather_*-240-color`;
  - the `_ref.png` files and the webcam `/dev/video4`.
- Produces: the photos, serial transcripts and findings for the PR.

- [ ] **Step 1: The slot.**
  - Ask wiki-steami-71 for the board only once the branch is final. It holds the V1c for the MicroPython v0.30 timings and then the Spleen firmware, whose flash moves the file system by 16 KB.
  - Record what is on the board and what it wants back: it said it will put `battery_status` back (`$P/hw12/restore-battery_status.ino.hex`, 89 264 B). Do not assume that file while MicroPython is still on the board: flashing an Arduino binary erases it.
  - Tell wiki-steami-30 when the board is taken and returned.
  - Never touch the DAPLink, the BLE stack or the configuration zone.
  - On "Bad response 8 -> 17" or a stalled drive: stop. Recovery needs the user's consent, and wiki-steami-71 does it.
- [ ] **Step 2: Build and flash.**
  - Build: `PLATFORMIO_BUILD_DIR="$P/hw4c/build-<Name>" PLATFORMIO_SRC_DIR=<dir> .venv/bin/pio run -e steami`.
  - Convert: `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-objcopy -O binary .../firmware.elf "$P/hw4c/<Name>.bin"`.
  - Flash: `cd "$P" && ./flashrun.sh hw4c/<Name>.bin <seconds>`. It copies to the DAPLink drive, checks FAIL.TXT, then reads the serial after a break; `readserial.py` is called by a relative path, so the cwd must be `$P`.
  - Never `pio run -t upload`, OpenOCD, `scripts/capture-serial.py`, `make flash-*` or `make test-hardware`.
- [ ] **Step 3: Photos.**
  - Copy `$P/hw12/shoot.sh` to `$P/hw4c/shoot.sh`; it writes next to itself.
  - Run `uvcdynctrl -d /dev/video4 -s "Auto Exposure" 3` first.
  - Check the full photo before trusting the crop, because other sessions moved the board.
  - Compare the layout, not live values (spec 7: "allowing for the titles and live values").
- [ ] **Step 4: ScreenGraph**, unchanged.
  - Photograph at least 22 s after reset, when the 20 samples fill the plot.
  - On the V1c it shows the pressure branch: "Pressure", labels "1050" and "950" inside the box (24 and 18 px), the subtitle block "BME280" / "20s window" at 177/197, nothing outside the disc.
  - Compare with `05_pressure_v1c` and `05_scrolling_graph_ref.png`.
  - The serial transcript has the banner line (`[STeaMi] Board V1c ...`) only, with no `(E..)` code: the light sensor is not started on V1c, and `pressure.begin()` succeeds.
- [ ] **Step 5: ScreenMenu, without button presses.**
  - (a) The unchanged example: a photo at boot (selected 0, "Temperature" on the bar).
  - (b) The scratch sketch `$P/hw4c/MenuSteps/MenuSteps.ino`:
    - it is ScreenMenu's globals, `setup()` and screen calls verbatim;
    - only the two `wasPressed` lines are replaced by a scripted index from `{2, 5, 6, -1}`, each held 12 s (`millis() / 12000 % 4`);
    - each step prints `step <i> selected=<in> -> <returned>` once on Serial;
    - run `readserial.py` long enough to cover the four steps; then, after it returns, photograph each state: the 48 s cycle (`millis() / 12000 % 4`) repeats without a reset, and the bar's row identifies the step, so no timing is needed.
    - Expected: the returns `2, 5, 0, 5` and the bar on rows 3, 6, 1 and 6. The highlight position identifies each photo.
    - Step 2 is compared with `06_dpad_menu_ref.png` and the 240 render.
    - Check: 6 rows visible, the bar's ends following the circle on every row (row 6 included), the marker `u(2)` = 3 px from the disc's edge, items centred, "Temperature" after the marker.
  - All six items fit on the V1c, so the window never scrolls on hardware. The window is covered natively (5 rows on 128).
  - (c) Optional, with the user: UP and DOWN on the unchanged example. If it does not run, the PR says so.
- [ ] **Step 6: WeatherStation.**
  - (a) Unchanged:
    - the serial transcript shows the banner (`[STeaMi] Board V1c ...`), `printInfo()`, and only E20 (from `gesture.begin()`);
    - a photo of page 0 compared with 01 and `weather_temperature-240`.
  - (b) The scratch sketch `$P/hw4c/WeatherPages/WeatherPages.ino`:
    - it is WeatherStation verbatim, except that the button and gesture lines become `page = (millis() / 12000) % 4;`;
    - it prints `page` on each change, and on page 3 `lroundf(compass.heading())`.
    - Compare: Humidity with 04 and `weather_humidity-240` (the BLUE fill); Pressure with 05 and `weather_pressure-240` (the history is full after 8 s at 200 ms per loop; RED curve; "1050" and "950"); Compass with 07, its needle direction φ = (360 − H) mod 360 against the printed H (4b Task 7 Step 3).
  - (c) Optional, with the user: walking the pages with the buttons.
- [ ] **Step 7: Verdict.** A V1c mismatch with the 240 render or `_ref.png` that is not one of spec section 6's known differences blocks the PR and goes back to its task (decision 6, B5). V1a/V1b are not checked on hardware.
  - After such a fix: rerun the owning task's per-task checks, then Task 10 again in full. The full `mk test-examples` still runs only inside Task 10, the final verification task, and the PR reports the run on the shipped tree. The hardware then rechecks only the affected example.
- [ ] **Step 8: Restore.** Reflash what Step 1 recorded, with `flashrun.sh`. Delete the `$P/hw4c/build-*` directories. Ask the user to put the board back if it moved. Release the slot to wiki-steami-71, and tell wiki-steami-30.
- [ ] **Step 9: PR text.** It contains:
  - "Closes #288";
  - the photos next to `_ref.png` and the 240 renders;
  - the SSIM of 05 and 06 at 128 and 240, and "26 unchanged renders (byte-identical)";
  - the 16 OK 128 px hashes (14 unchanged, 2 new);
  - the flash, RAM and "B free" of WeatherStation, and the deltas of Drawing and ScreenGauge against 60dc6b0;
  - the stack table, with the template wrappers' lines or "inlined" and the caller's frame;
  - what was scripted and what was pressed (MenuSteps and WeatherPages stand in for the buttons);
  - no `qualify.py` case, and why, in D14's words (it flashes its own sketch and resets through OpenOCD; interactive, `key=value`; no PyYAML in the `.venv`);
  - the D11 index-type limit, for the maintainer;
  - the paths outside row 4c and the `gaugeLabel()` refactor ("Paths", D5);
  - V1a/V1b not checked on hardware (decision 6);
  - "both cores" read as PlatformIO plus arduino-cli, both on STM32duino 3.1.0;
  - a note for wiki-steami-30 about the three new examples and the `WeatherStation` name it shares with BME280's (different components in the package and the archive).

---

## Out of scope

- **MicroPython** (steamicc/micropython-steami-lib#461 and spec section 12): `device.py`'s graph labels (B1) and menu (B3, six rows on 240 with a clipped bar), wrap and return. Font design 8.1 lists them as MicroPython's own differences until that PR adopts them.
- **The wiki pages**: the Arduino screen page with the vocabulary table, E30/E31 in the French error table, and the examples robot's pages.
- **Left from 4b**: the compass's known-north check and the photo of the six faces with the user.
- **V1a/V1b hardware checks** (decision 6), `scripts/qualify.py` cases (D14), and button presses by a person (optional steps of Task 11).
- **The spec's non-goals**: partial refresh/`showRect`, `scroll()`, DMA, bitmaps other than faces, touch, double buffering, a circular mask, an RTC for `watch()`, Spleen on 128, new font tables.
- **Changes to existing widgets** (`gauge`, `compass`, `watch`, `face`, `bar`, the value and text kernel) beyond `gaugeLabel()` calling `integerText()`. Any change to a 128 px pixel of them, or to a 240 golden of 4a, 4b or 4b-font, is out of scope.
- **Untouched files**: `platformio.ini`, `.github/workflows/`, the Makefile, `requirements.txt`, `lib/ssd1327`, `lib/gc9a01`, `scripts/qualify.py`, `scripts/build-arduino-package.py`, `CONTRIBUTING.md`, and `SteamiFontSpleen.cpp` (generated).
