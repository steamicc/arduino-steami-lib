# STeaMi display, delivery 3b (backends and board) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put `SteamiDisplay` on the real panels: a GC9A01 backend (V1c) and an SSD1327 backend (V1a/V1b) in two lazy board slots routed by the release, with the `Drawing` example, a flash and RAM footprint gate, package checks, README and keywords, tested natively over the SPI and Wire mocks and checked on a V1c under the webcam.

**Architecture:**
- Two `final` backends implement the 3a interface `SteamiDisplayBackend` and hold their driver by value: `SteamiGC9A01` / `SteamiSSD1327` on target, `GC9A01_SPI` / `SSD1327_SPI` on the internal pins over the SPI mock natively (the `Steami*` driver classes exist only under `#ifdef ARDUINO`).
- `SteamiBoard` gains `SteamiLazy<SteamiSsd1327Backend>` and `SteamiLazy<SteamiGc9a01Backend>`. `display()` returns the injected backend if any, otherwise builds (never begins) the slot that `steamiRoute(release(), SteamiFunction::SCREEN, 0)` names, and `nullptr` on UNKNOWN. `beginDisplay()` (3a, unchanged) begins it once.
- Native tests capture every SPI byte through a new static hook of the SPI mock and compare the frame each panel receives with the 3a memory backend drawing the same gallery. So the bytes sent to both panels are pinned without a board. The V1c photo, the `show()` time and the BLE check are done by the controller on hardware.

**Tech Stack:** C++17 (`-std=c++17` native, gnu++17 on target), PlatformIO `env:native`, `env:native_asan`, `env:steami` (STM32duino 3.1.0), Unity, Python 3 `unittest`, `arm-none-eabi-size`, GitHub Actions, arduino-cli (CI).

**Spec:** `docs/superpowers/specs/2026-11-25-steami-display-design.md`: row 3b of section 8; sections 3, 5.1, 5.4, 5.5, 5.12, 5.13, 7 (suite 2, build and package CI, hardware checks), 9 (decision 6, B4, B5), 11, 12, 13. Branch `feat/steami-display-3b` from `main` at v0.35.0 (7946c86, which contains 3a, PR #258). Issue #260; the PR says "Closes #260".

## Global Constraints

Copied from the design document (verbatim):

- "Never construct the SSD1327 (8 KB `new` in its constructor) on a release without it, or in a sketch that does not use the screen."
- "Never begin the GC9A01 (115 200 B `malloc` in `begin()`) unless the sketch uses the screen on a V1c."
- "No heap allocation in the layer itself (drivers keep their own buffers), no `String`, no RTTI, no exceptions."
- "Never hand an unclipped coordinate to a driver: `SSD1327::pixel(uint8_t, uint8_t, ...)` has no bounds check."
- "Never call the drivers' `line()` or `text()`: they do not clip and use another font."
- "Never guess the panel on an unknown release (both panels are write-only SPI): E30."
- "RAM: about 192 KB on the STM32WB55. The GC9A01 frame buffer takes 115 200 B in one block, so the screen starts first in `setup()`; sketches like WeatherStation must keep at least 140 KB free for heap and stack (footprint gate of the functional-layer spec)."
- "Flash: 512 KB cap (`upload.maximum_size`); the layer adds about 8 to 12 KB."
- "Two panels: SSD1327 128x128, 4-bit grey, SPI at 10 MHz, byte-wise `show()`; GC9A01 240x240, RGB565 big-endian buffer, SPI at 16 MHz, block `show()`."
- "Packaging: flattened `src/`, generated umbrella `STeaMi.h`, unique file names (case-insensitive), no `Steami.h`; no `STEAMI_BOARD_*` in any library `.cpp`; headers carry no macro-dependent inline code."
- "Native tests (`tests/native`, Unity, SPI/Wire mocks) must cover everything except the look on the real panel and the refresh time."
- 5.4: "`SteamiSsd1327Backend`: holds the driver (`SteamiSSD1327` on target; an `SSD1327_SPI` on the internal SPI pins over the SPI mock under `UNIT_TEST`, since `SteamiSSD1327` is `#ifdef ARDUINO`). Converts each colour with `SteamiRgb::fromRgb565(v).gray4()` (a one-entry cache of the last colour is enough), not colour, 128x128. Its constructor allocates the 8 KB buffer, which is why its slot is built only on V1a/V1b."
- 5.4: "`SteamiGc9a01Backend`: holds the driver, passes RGB565 through, colour, 240x240; `begin()` returns the driver's `begin()` (false when the 115 200 B `malloc` fails)."
- 5.5: "`display()` routes `SteamiFunction::SCREEN` with `steamiRoute()` and builds only that slot; a sketch without a screen builds neither." "`built(SteamiChip::SSD1327)` and `built(SteamiChip::GC9A01)` report the slots; `printInfo()` lists the screen chip and its state; `end()` destroys the slots (the drivers free their buffers) and forgets the status. `claim()` keeps answering `NOT_STARTED` for the display chips." "`sizeof(SteamiBoard)` stays under the 2.5 KB native gate."
- Decision 6: "**The V1c comes first.** [...] the 128 px OLED of V1a/V1b is best effort. [...] when a rule cannot serve both panels equally, the V1c wins, and a V1a/V1b defect does not block a delivery. V1a/V1b are not checked on hardware."
- Scope (row 3b, B4): `lib/steami_board` (code, examples, README, `keywords.txt`); native tests and helpers under `tests/native`; `scripts/`; `platformio.ini` and `.github/workflows/` as needed. Never `lib/ssd1327`, `lib/gc9a01`, MicroPython, tutorials or the wiki.

Repository conventions (CONTRIBUTING.md, commitlint, `.clang-format`, `.clang-tidy`, the 3a plan):

- Run `make setup` in the worktree first (`.venv/bin/pio`, clang-format, clang-tidy, husky hooks; pre-commit runs clang-format and the SPDX check on staged files, commit-msg runs commitlint).
- Commits: `type(scope): Sentence starting with a capital and ending with a period.`, header at most 72 characters. Scopes allowed by `commitlint.config.js` and used here: `board`, `tests`, `tooling`, `ci` (**not** `display`, **not** `steami_board`). No `!`, no `BREAKING CHANGE`, never `[skip ci]`, no attribution lines.
- Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`; headers use `#pragma once`; Google style, indent 4, 100 columns; clang-tidy `WarningsAsErrors: '*'` with `modernize-use-override` (every backend override says `override`) and `modernize-use-default-member-init`. clang-tidy runs on the native compile database (ARDUINO undefined), so the `#ifdef ARDUINO` branches are checked by `make build` only.
- Tests build display objects on the file-local `board`, never on `Steami`. Every test that builds a panel slot leaves it to `tearDown`'s `board.end()`: `SteamiLazy` has no destructor, so a slot that is never reset leaks its frame buffer under LeakSanitizer.
- Never call unqualified `min`/`max`/`abs` in library code. Do not reuse the unprefixed names of `SteamiSSD1327_const.h` (`REG_CMD`, `REG_DATA`, `SET_*`): `SteamiBoard.h` now makes them visible everywhere.
- A library `.cpp` defines no namespace-scope object with a constructor (arduino-cli links every library object file); use function-local statics.
- Never flash, never open a serial port, never touch `/dev/ttyACM*`, never run `make test-hardware`: only Task 7 uses the board, and only the controller runs it.

## Decisions taken while planning (the design doc leaves them open)

- **Driver type by macro, bodies in the `.cpp`.** Each backend header declares `using SteamiGc9a01Driver = SteamiGC9A01;` (`SteamiSsd1327Driver = SteamiSSD1327;`) under `#ifdef ARDUINO`, else `GC9A01_SPI` (`SSD1327_SPI`). This is a member type, not inline code. The constructor and `begin()` bodies live in the `.cpp`. Natively the backend `.cpp` builds the driver on a function-local `static SPIClass spi(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK)` with D/C `SPI_INT_MISO`, reset `RST_DISPLAY` and CS `CS_DISPLAY`, the same pins as `SteamiGC9A01()`/`SteamiSSD1327()`, and calls `spi.begin()` before the driver's `begin()`, as those classes do.
- **SPI mock additions** (`tests/native/helpers/SPI.h`): a pin constructor `SPIClass(uint32_t mosi, uint32_t miso, uint32_t sclk)` storing `mosiPin`, `misoPin`, `sclkPin`; `begin()` records the current `static inline uint32_t testEpoch` and `bool begun() const` is true only if `begin()` ran in the current epoch (each `setUp` does `++SPIClass::testEpoch`, so a function-local bus begun by an earlier test does not hide a missing `begin()`); and `static inline std::function<void(const SPIClass& bus, uint8_t data)> onAnyTransfer`, called for every byte of every instance after `onTransfer`. This lets a test see the backend's function-local bus. The default constructor and `onTransfer` stay, so `test_gc9a01` is unchanged. `extern SPIClass SPI;` stays undefined and unused.
- **`display()` order:** injected backend first, with no release detection, so the 3a memory-backend tests make no I2C traffic. Otherwise the routed slot, built and not begun; while the status is `NOT_STARTED` it first resets the slot that is not routed, so a release forced after a geometry call never leaves both buffers alive. `nullptr` when the route is `NONE` (UNKNOWN).
- **`setDisplayBackend()`** still applies only while the status is `NOT_STARTED`. It now also destroys a panel slot that `display()` built but nobody began, so injecting after a geometry call leaves no 8 KB or 115 KB behind.
- **`printInfo()` screen line**, after the I2C chips and before the last error: `"<chip> screen <state>"`. `<chip>` is the injected backend's `chip()`, otherwise `steamiChipName()` of the routed panel. `<state>` is `OK`, `not started`, `no memory` or `unknown board` from the cached status. No line on UNKNOWN without injection. It reads the cached status and never builds a slot.
- **`end()`** destroys both panel slots (the drivers free their buffers) besides the 3a resets. It does not power the panel off.
- **Link cost in screenless sketches (accepted by the maintainer on 2026-11-26):** the slot code in `display()`, `setDisplayBackend()` and `end()` references the out-of-line `GC9A01::~GC9A01`, which pulls `SteamiGC9A01.cpp.o` and through it the core's `SPI.cpp.o` (global `SPIClass SPI`, `spi_init`, `HAL_SPI_Init`). gc-sections drops every driver function, so a sketch without a screen grows by about 2.2 KB of flash and 212 B of RAM (measured on `Temperature`: 65 728 → 67 920 B flash, 5 044 → 5 256 B RAM). The spec only requires that such a sketch build no slot at runtime (5.5), and this is negligible against the 512 KB cap and the 140 KB rule. A zero-cost variant (slot code in its own TU behind a destroy function pointer) is a follow-up only if the owner asks.
- **clang-tidy and the SSD1327 header:** `SteamiSSD1327.h` declared its `writeCmd`/`writeData` overrides without `override` (lines 61, 62, 77, 78), which the root `HeaderFilterRegex: '^(lib|src|tests)/'` reports in every `lib/steami_board` TU that includes it. The maintainer chose to fix the driver header in its own PR (issue #263, PR #264) rather than narrow the filter. The branch carries that commit, cherry-picked as its first commit after the plan (`fix(ssd1327): Mark the writeCmd and writeData overrides.`); once #264 is squash-merged, `git rebase origin/main` drops it. No `.clang-tidy` is added and `make tidy` keeps its full header filter. #264 is merged (v0.36.1). Task 1 then showed that the native compile database also brings in both driver sources, so clang-tidy `modernize-use-default-member-init` reports GC9A01 `_rate` and SSD1327 `_cmdArr`/`_dataList`. These are fixed the same way, in their own PR (issue #266, PR #267), and the commit `fix(gc9a01,ssd1327): Initialise the panel driver members in place.` is cherry-picked on the branch until #267 is merged.
- **SSD1327 and E31:** the SSD1327 allocates with a throwing `new[]` in its constructor and its `begin()` always returns true (driver, out of scope), so V1a/V1b never report E31. The README says so.
- **SSD1327 colour cache:** members `uint16_t _lastColor = 0; uint8_t _lastGray = 0;` (black is level 0, so the initial pair is consistent). `fillRect` goes through the driver's `fillRect` (it loops on `pixel`); no faster span code in 3b (section 13 leaves speed to the builder).
- **Gallery shared by tests and example:** the 3a `drawGallery()` moves, unchanged, to `tests/native/helpers/SteamiGallery.h`. `Drawing.ino` copies its body line for line, and a `unittest` fails if the two bodies differ, so the 240 px dump stays the reference for the V1c photo.
- **Drawing measures `show()`:** `setup()` begins the screen first and draws the gallery (which ends with its own `show()`). `loop()` times one more `show()` of the same frame with `micros()`, prints `show(): <n> us` every 2 s, so the controller can read it whenever it opens the port. On a failed `begin()` it prints `No screen: <error()>` once and `loop()` returns at once. The object is named `screen`, as in the E31 hint.
- **Footprint gate:** `scripts/check-footprint.py` builds each sketch of `SKETCHES = ("steami_board/Drawing",)` with `pio run -e steami` and sizes `.pio/build/steami/firmware.elf` with `arm-none-eabi-size -A -d`. Flash is every section at `0x08000000` and up within the cap (`.isr_vector`, `.text`, `.rodata`, `.ARM`, `.init_array`, `.fini_array`), plus `.data`: about 360 B above PlatformIO's `Flash: … used` figure, which counts only `.text + .rodata + .data` (66 088 B against 65 728 B for v0.35.0 `Temperature`); the PR quotes the gate's figure and says so. RAM is `.data + .bss + .noinit`, as PlatformIO reports it. The gate covers only the cap and the 140 KB rule; the "no panel cost in screenless sketches" budget stays a manual Task 6 check (a tight per-sketch ceiling in CI would trip on any later legitimate growth of `SteamiBoard.cpp`). The limits come from `boards/steami.json`: flash ≤ `maximum_size − 65536` (458 752 B, a 64 KB margin) and RAM ≤ `maximum_ram_size − 143360` (53 248 B, so 140 KB stay free). WeatherStation joins `SKETCHES` in 4c. CI runs it in `build.yml` after "Compile examples". There is no Makefile target: the Makefile is outside the 3b path list.
- **Package:** `_copy_sources()` rejects a source named `steami.h` in any case (it would overwrite the generated umbrella or clash with it on a case-insensitive disk). The `STeaMi.h` content test gains the display headers. `lib/steami_board/library.properties` `depends=` gains `STeaMi SSD1327, STeaMi GC9A01`.
- **"Two `SteamiScreen` objects share one backend"** (suite 2): `SteamiScreen` arrives in 4a, so 3b tests two `SteamiDisplay` objects on the real V1c slot. The `SteamiScreen` form is carried to 4a (see Out of scope).
- **"Both cores"** means PlatformIO (`make test-examples`, `pio run -e steami`) and arduino-cli (`arduino-package-check.yml`), both on STM32duino 3.1.0; the spec's "2.12" predates the move of `env:steami` to 3.1.0 (`platformio.ini` lines 7-11, 3a plan). Both already discover a new example, so no workflow edit is needed for the example builds.
- **BLE and late-begin checks** use a throw-away sketch in the controller's scratchpad (Task 7), not a committed example: `tests/hardware` is not in the 3b path list, and `Drawing` stays a primitives demo.

## Review Focus

1. **The screen on a board whose release stays UNKNOWN** (no sensor answers, no define): `width()`, `height()`, `radius()` give 0, every drawing call does nothing, one E30 line, no slot built, no SPI byte, no display pin touched. Test: `test_unknown_gives_e30_and_no_slot` (Task 3).
2. **Geometry asked before `begin()`** (`width()` at the top of `setup()`; never from a global initialiser, which would probe I2C before the core's `init()`, and the README says so): on V1c the GC9A01 slot is built with no pin touched and no SPI byte; on V1a/V1b the SSD1327 slot is built (8 KB, reset pulse) with no SPI byte; the later `begin()` initialises the panel once. Tests: `test_v1c_geometry_before_begin`, `test_v1ab_geometry_before_begin` (Task 3).
3. **A backend injected after `display()` built a panel slot**, and an injection on a board never detected: the injected backend wins, the unbegun slot is destroyed, and injecting first makes no I2C traffic. Tests: `test_injection_frees_an_unbegun_slot`, `test_injected_backend_skips_detection` (Task 3).
4. **`Steami.end()` then drawing again**: the slot and its buffer are freed (LeakSanitizer silent), the next draw rebuilds and begins the panel exactly once more. Test: `test_end_destroys_panel_slots_and_restarts` (Task 3).
5. **Alternating colours on the OLED**, including two colours with the same grey level (BLUE and DARK) and a colour following itself: the one-entry cache never returns a stale level. Test: `test_ssd1327_colour_cache_follows_every_colour` (Task 2).

## File map

| File | Responsibility | Task |
|---|---|---|
| `tests/native/helpers/SPI.h` | pin constructor, `begun()` per test epoch, static `onAnyTransfer` | 1 |
| `tests/native/helpers/SteamiGallery.h` | `drawGallery()`, moved from the 3a suite | 1 |
| `lib/steami_board/src/SteamiGc9a01Backend.h/.cpp` | V1c backend | 1 |
| `lib/steami_board/src/SteamiSsd1327Backend.h/.cpp` | V1a/V1b backend | 2 |
| `lib/steami_board/library.properties` | `depends=` + the two panel libraries | 1, 2 |
| `lib/steami_board/src/SteamiBoard.h/.cpp` | slots, routing, `built()`, `printInfo()`, `end()`, `setDisplayBackend()` | 3 |
| `tests/native/test_steami_display_board/test_main.cpp` | suite 2 of section 7 | 1 to 3 |
| `tests/native/test_steami_display/test_main.cpp` | uses the gallery helper; E30 tests count by suffix | 1, 3 |
| `tests/native/test_steami_board/test_main.cpp` | printInfo tests list the screen | 3 |
| `lib/steami_board/examples/Drawing/Drawing.ino` | the gallery on the panel, `show()` time | 4 |
| `scripts/tests/test_steami_display_rules.py` | gallery guard; backends never call driver `line`/`text`/`scroll` | 2, 4 |
| `scripts/check-footprint.py`, `scripts/tests/test_check_footprint.py` | footprint gate | 5 |
| `.github/workflows/build.yml` | footprint step | 5 |
| `scripts/build-arduino-package.py`, `scripts/tests/test_build_arduino_package.py` | reserved umbrella name, `STeaMi.h` content | 6 |
| `lib/steami_board/README.md`, `keywords.txt`, `examples/BoardInfo/BoardInfo.ino` | display section, E30/E31, board API, keywords | 6 |

---

### Task 1: SPI mock hook, shared gallery and the GC9A01 backend (V1c)

**Files:**
- Modify: `tests/native/helpers/SPI.h`
- Create: `tests/native/helpers/SteamiGallery.h`
- Modify: `tests/native/test_steami_display/test_main.cpp` (delete the static `drawGallery`, `#include "SteamiGallery.h"`)
- Create: `lib/steami_board/src/SteamiGc9a01Backend.h`, `lib/steami_board/src/SteamiGc9a01Backend.cpp`
- Modify: `lib/steami_board/library.properties` (`depends=` gains `STeaMi GC9A01` at the end)
- Create: `tests/native/test_steami_display_board/test_main.cpp`

**Interfaces:**
- Consumes (3a): `class SteamiDisplayBackend` (`SteamiDisplayBackend.h`: protected non-virtual destructor; pure virtual `bool begin()`, `const char* chip() const`, `int16_t width() const`, `int16_t height() const`, `bool isColor() const`, `void fill(uint16_t)`, `void show()`, `void pixel(int16_t, int16_t, uint16_t)`, `void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t)`, `void rotate(bool upsideDown)`, `void invert(bool on)`, `void powerOn()`, `void powerOff()`); `SteamiBoard::setDisplayBackend(SteamiDisplayBackend&)`; `SteamiDisplay(SteamiBoard&)` and its primitives; `SteamiMemoryBackend(int16_t w, int16_t h, bool color = true, bool beginResult = true)` with `at(x, y)`, `outOfBounds()`; driver `GC9A01_SPI(SPIClass& spi, uint8_t dc, uint8_t rst, uint8_t cs)` and, on target, `SteamiGC9A01()`; constants `GC9A01_CMD_*`, `GC9A01_ORIENT_NORMAL`, `GC9A01_ORIENT_ROTATED`.
- Produces:
  - Mock: `SPIClass()`, `SPIClass(uint32_t mosi, uint32_t miso, uint32_t sclk)`, members `uint32_t mosiPin = 0, misoPin = 0, sclkPin = 0; uint32_t begunEpoch = 0;`, `static inline uint32_t testEpoch = 1;`, `bool begun() const { return begunEpoch == testEpoch; }` (`begin()` sets `begunEpoch = testEpoch`), `static inline std::function<void(const SPIClass& bus, uint8_t data)> onAnyTransfer;`.
  - `inline void drawGallery(SteamiDisplay& d)` in `SteamiGallery.h`, the 3a body unchanged. Its doc comment says the `Drawing` example copies it line for line and a script checks it.
  - `class SteamiGc9a01Backend final : public SteamiDisplayBackend` with `SteamiGc9a01Backend();` (builds the driver, touches no pin), `~SteamiGc9a01Backend() = default;` (public; the driver frees its buffer), copy and move deleted, every interface method `override`. `begin()` returns the driver's `begin()`. `chip()` is `"GC9A01"`. `width()`/`height()` are `240` as `int16_t`. `isColor()` is true. The rest passes RGB565 and flags straight to the driver. Private member `SteamiGc9a01Driver _tft;`.
  - Test scaffold reused by Tasks 2 and 3: fixture `board`, `g_lines`, `capture`, `countLogged(suffix)`, `assertStatus`, and the SPI recorder below.

- [ ] **Step 1: Extend the SPI mock and move the gallery**

Add the mock members of the Interfaces block. `record()` calls `onTransfer` then `onAnyTransfer(*this, data)`. Move `drawGallery` to `SteamiGallery.h` (SPDX, `#pragma once`, includes `SteamiDisplay.h` and `SteamiRgb.h`, `inline`). The 3a suite includes it.

Run: `.venv/bin/pio test -e native -f native/test_gc9a01 -f native/test_steami_display` — Expected: PASS (nothing else changed).

- [ ] **Step 2: Write the failing tests**

Suite scaffold (same fixture as `tests/native/test_steami_display/test_main.cpp` lines 28-58, plus):

```cpp
struct SpiByte { bool data; uint8_t value; bool begun; uint32_t mosi, miso, sclk, clock; };
static std::vector<SpiByte> g_spi;
static void recordSpi(const SPIClass& bus, uint8_t v) {   // D/C is SPI_INT_MISO on both panels
    g_spi.push_back({digitalRead(SPI_INT_MISO) == HIGH, v, bus.begun(), bus.mosiPin, bus.misoPin,
                     bus.sclkPin, bus.lastClock});
}
// setUp also: g_spi.clear(); gpioPinState().clear(); gpioPinMode().clear(); ++SPIClass::testEpoch;
//             SPIClass::onAnyTransfer = recordSpi;   tearDown: SPIClass::onAnyTransfer = nullptr;
static std::vector<uint8_t> commandsSent();          // bytes with D/C low, in order
static std::vector<uint8_t> lastData(size_t n);      // the last n bytes with D/C high; starts with
                                                     // TEST_ASSERT_TRUE_MESSAGE(count >= n, ...) so a
                                                     // short capture fails cleanly, never UB
// Every test that reads commandsSent()[0] first asserts !commandsSent().empty().
static int paramAfter(uint8_t cmd);                  // first D/C-high byte after the first cmd, -1 if none
static void assertPanelBus(uint32_t clock);          // every byte: begun, pins 209/210/211, clock
```

```cpp
static void test_gc9a01_backend_identity_and_quiet_constructor(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_EQUAL_STRING("GC9A01", tft.chip());
    TEST_ASSERT_EQUAL_INT16(240, tft.width());
    TEST_ASSERT_EQUAL_INT16(240, tft.height());
    TEST_ASSERT_TRUE(tft.isColor());
    TEST_ASSERT_TRUE(gpioPinMode().empty());   // no pin before begin()
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}
static void test_gc9a01_backend_begin_initialises_the_panel(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_TRUE(tft.begin());
    assertPanelBus(16000000);                  // internal pins, SPI begun before the first byte
    // exactly one GC9A01_CMD_DISPON among commandsSent(); lastData(115200) is all 0x00 (black)
}
static void test_gc9a01_backend_passes_rgb565_through(void) {
    // after begin(), g_spi.clear(): fill(0x1234); show() -> lastData(115200) is 0x12, 0x34 repeated;
    // pixel(239, 239, 0xF800); show() -> the last two bytes are 0xF8, 0x00;
    // fillRect(0, 0, 240, 1, 0x07E0); show() -> bytes 0..479 are 0x07, 0xE0 repeated
}
static void test_gc9a01_backend_controls_forward(void) {
    // after begin(), each with g_spi cleared first:
    // invert(true) -> commandsSent() == {GC9A01_CMD_INVOFF}; invert(false) -> {GC9A01_CMD_INVON}
    // rotate(true) -> commandsSent() not empty, commandsSent()[0] == GC9A01_CMD_MADCTL, paramAfter(MADCTL) == ORIENT_ROTATED,
    //                 and a full frame follows (lastData(115200) exists)
    // rotate(false) -> paramAfter(MADCTL) == GC9A01_ORIENT_NORMAL
    // powerOff() -> {GC9A01_CMD_DISPOFF, GC9A01_CMD_SLPIN}; powerOn() -> {SLPOUT, DISPON}
}
static void test_gc9a01_frame_matches_the_memory_gallery(void) {
    // SteamiGc9a01Backend tft; board.setDisplayBackend(tft); SteamiDisplay d(board); drawGallery(d);
    // frame = lastData(115200); board.end(); board.setLogger(&capture);
    // SteamiMemoryBackend mem(240, 240, true); board.setDisplayBackend(mem); SteamiDisplay m(board);
    // drawGallery(m); TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
    // for every (x, y): (frame[2*(y*240+x)] << 8 | frame[2*(y*240+x)+1]) == mem.at(x, y)
}
```

- [ ] **Step 3: Run to verify failure**

Run: `.venv/bin/pio test -e native -f native/test_steami_display_board` — Expected: build error, `SteamiGc9a01Backend.h` not found.

- [ ] **Step 4: Implement `SteamiGc9a01Backend`** as the Interfaces block and the decisions ("Driver type by macro") say. In the `.cpp`, the native constructor is `_tft(panelSpi(), SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY)`, where `panelSpi()` holds the function-local static bus in an anonymous namespace. `begin()` calls `panelSpi().begin()` first natively; on target it is `return _tft.begin();`. Add `STeaMi GC9A01` to `depends=`.

- [ ] **Step 5: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display_board -f native/test_steami_display -f native/test_gc9a01` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no `runtime error:`, no `AddressSanitizer` or `LeakSanitizer` report.
Run: `make tidy` — Expected: no diagnostic. Run: `make build` — Expected: `SUCCESS` (target branch of the backend compiles against `SteamiGC9A01`).

- [ ] **Step 6: Commit**

```bash
git add tests/native/helpers/SPI.h tests/native/helpers/SteamiGallery.h \
  tests/native/test_steami_display/test_main.cpp
git commit -m "test(tests): Let the SPI mock report bytes from every bus."
git add lib/steami_board/src/SteamiGc9a01Backend.* lib/steami_board/library.properties \
  tests/native/test_steami_display_board/test_main.cpp
git commit -m "feat(board): Add the GC9A01 display backend of the V1c."
```

---

### Task 2: SSD1327 backend (V1a/V1b)

**Files:**
- Create: `lib/steami_board/src/SteamiSsd1327Backend.h`, `lib/steami_board/src/SteamiSsd1327Backend.cpp`
- Modify: `lib/steami_board/library.properties` (`depends=` gains `STeaMi SSD1327`)
- Modify: `tests/native/test_steami_display_board/test_main.cpp`
- Modify: `scripts/tests/test_steami_display_rules.py`

**Interfaces:**
- Consumes: Task 1's recorder (`g_spi`, `commandsSent()`, `lastData(n)`, `assertPanelBus(clock)`), `drawGallery(SteamiDisplay&)`; driver `SSD1327_SPI(uint8_t width, uint8_t height, SPIClass& spi, uint8_t dc, uint8_t res, uint8_t cs)` and, on target, `SteamiSSD1327()` (its `begin()` hides the non-virtual `SSD1327::begin()`: call it on the concrete member); `SteamiRgb::fromRgb565(uint16_t).gray4()`; constants `SET_DISP_MODE_INVERSE` (0xA7), `SET_DISP_MODE_NORMAL` (0xA4), `SET_DISP_OFFSET`, `SET_SEG_REMAP`, `SET_FN_SELECT_A`, `SET_DISP`.
- Produces:
  - `class SteamiSsd1327Backend final : public SteamiDisplayBackend` with `SteamiSsd1327Backend();` (builds the driver: 8 KB `new[]`, pins set, reset pulse, no SPI byte), public `~SteamiSsd1327Backend() = default;`, copy and move deleted, every method `override`. `chip()` is `"SSD1327"`. `width()`/`height()` are `128`. `isColor()` is false. `fill`, `pixel` and `fillRect` convert the colour through `uint8_t gray(uint16_t rgb565)` (private, one-entry cache `_lastColor`/`_lastGray`). Coordinates arrive clipped to [0, 128), so the `uint8_t` casts are exact. `begin()` returns the driver's `begin()` after `panelSpi().begin()` natively. Private member `SteamiSsd1327Driver _oled;`.
  - `static void drawClippingCases(SteamiDisplay& d)` in the suite, reused by Task 3: `pixel(-1, 0)`, `pixel(200, 5)`, `text("Clip", 10, 125)`, `text("Clip", -20, 10)`, `line(-50, -50, 300, 300)`, `rect(-10, -10, 300, 300)`, `circle(0, 0, 20)`, `circle(127, 127, 20, STEAMI_WHITE, true)`, `circle(64, -5, 30)`, `circle(-5, 64, 30, STEAMI_GRAY, true)`, `triangle(-50, -50, 300, 60, 60, 300, STEAMI_LIGHT, true)`, `arc(0, 0, 40, 0, 360, STEAMI_WHITE, 3)`, `arc(127, 0, 40, 90, 270, STEAMI_GREEN, 5)`, `arc(64, 127, 70, 180, 180, STEAMI_RED, 2)`, `arc(127, 127, 200, 0, 360, STEAMI_BLUE, 8)`, then `show()`.
  - `static void assertOledFrameEquals(const SteamiMemoryBackend& grey128, const std::vector<uint8_t>& frame)`: for every (x, y), the nibble `frame[y*64 + x/2]` (high nibble for even x) equals `SteamiRgb::fromRgb565(grey128.at(x, y)).gray4()`.

- [ ] **Step 1: Write the failing tests**

```cpp
static void test_ssd1327_backend_identity(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_EQUAL_STRING("SSD1327", oled.chip());
    TEST_ASSERT_EQUAL_INT16(128, oled.width());
    TEST_ASSERT_EQUAL_INT16(128, oled.height());
    TEST_ASSERT_FALSE(oled.isColor());
}
static void test_ssd1327_constructor_resets_without_spi(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_EQUAL_INT(OUTPUT, gpioPinMode()[RST_DISPLAY]);
    TEST_ASSERT_EQUAL_INT(HIGH, gpioPinState()[RST_DISPLAY]);   // pulse ended high
    TEST_ASSERT_EQUAL_INT(HIGH, gpioPinState()[CS_DISPLAY]);
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}
static void test_ssd1327_backend_begin(void) {
    // begin() true; assertPanelBus(10000000); lastData(8192) all 0x00 (initDisplay clears)
}
static void test_ssd1327_converts_colours_to_grey(void) {
    // after begin(), g_spi cleared: fill(STEAMI_GRAY.rgb565()); show() -> lastData(8192) all 0x99;
    // fill(0); pixel(0, 0, STEAMI_WHITE.rgb565()); pixel(127, 127, STEAMI_BLUE.rgb565());
    // fillRect(126, 0, 2, 1, STEAMI_RED.rgb565()); show() -> byte 0 == 0xF0, byte 63 == 0x88,
    // byte 8191 == 0x06
}
static void test_ssd1327_colour_cache_follows_every_colour(void) {   // Review Focus 5
    // fill(0); pixels (0..5, 0) in BLUE, WHITE, BLUE, DARK, DARK, gray(1); show()
    // -> bytes 0, 1, 2 == 0x6F, 0x66, 0x61
}
static void test_ssd1327_controls_forward(void) {
    // invert(true) -> commandsSent() == {SET_DISP_MODE_INVERSE}; invert(false) -> {SET_DISP_MODE_NORMAL}
    // rotate(true): in commandsSent(), SET_DISP_OFFSET is followed by 128 and SET_SEG_REMAP by 0x42;
    // rotate(false): by 0 and 0x51
    // powerOff() -> {SET_FN_SELECT_A, 0x00, SET_DISP}; powerOn() -> {SET_FN_SELECT_A, 0x01, SET_DISP | 1}
}
static void test_ssd1327_frame_matches_the_grey_gallery(void) {
    // as test_gc9a01_frame_matches_the_memory_gallery, with SteamiSsd1327Backend, lastData(8192),
    // SteamiMemoryBackend mem(128, 128, false) and assertOledFrameEquals(mem, frame)
}
static void test_ssd1327_clipping_cases_stay_in_the_buffer(void) {   // run under native_asan
    // the same pair with drawClippingCases instead of drawGallery; mem.outOfBounds() == 0;
    // assertOledFrameEquals(mem, frame)
}
static_assert(sizeof(SteamiSsd1327Backend) < 128 && sizeof(SteamiGc9a01Backend) < 128,
              "the slots hold drivers, not frame buffers");
```

In `scripts/tests/test_steami_display_rules.py`:

```python
def test_backends_never_call_driver_drawing(self) -> None:
    # for the .h and the .cpp of SteamiSsd1327Backend and SteamiGc9a01Backend under
    # lib/steami_board/src (inline forwarding overrides live in the headers):
    # assertNotRegex(text, r"\.(line|text|scroll)\s*\(")
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display_board` — Expected: build error, `SteamiSsd1327Backend.h` not found.

- [ ] **Step 3: Implement `SteamiSsd1327Backend`**, mirroring Task 1: the native constructor is `_oled(128, 128, panelSpi(), SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY)`; `gray()` recomputes only when the colour differs from `_lastColor`. Add `STeaMi SSD1327` to `depends=`.

- [ ] **Step 4: Run to verify pass**

Run: `.venv/bin/pio test -e native -f native/test_steami_display_board` — Expected: PASS.
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report.
Run: `make test-tooling` — Expected: `OK`. Run: `make build` — `SUCCESS`.
Run the Makefile's exact clang-tidy invocation on the four TUs that see the SSD1327 header now or after Task 3:

```bash
make compile_commands.json
.venv/bin/clang-tidy -p . --quiet $(for d in lib/*/src; do printf -- '--extra-arg-before=-I%s ' "$d"; done) \
  lib/steami_board/src/{SteamiSsd1327Backend,SteamiGc9a01Backend,SteamiDisplay,SteamiBoard}.cpp
```

Expected: exit 0, no `SteamiSSD1327.h` diagnostic (the cherry-picked #264 commit adds its `override`s). Then `make tidy` — no diagnostic.

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiSsd1327Backend.* \
  lib/steami_board/library.properties \
  tests/native/test_steami_display_board/test_main.cpp scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the SSD1327 display backend of the V1a and V1b."
```

---

### Task 3: Board slots, routing and the screen line of `printInfo()`

**Files:**
- Modify: `lib/steami_board/src/SteamiBoard.h` (include both backend headers; two slots beside `_displayInjected`; doc comments of `display()`, `setDisplayBackend()`, `printInfo()` and `end()`)
- Modify: `lib/steami_board/src/SteamiBoard.cpp` (`display()`, `setDisplayBackend()`, `built()`, `printInfoTo()`, `end()`)
- Modify: `tests/native/test_steami_display_board/test_main.cpp`
- Modify: `tests/native/test_steami_display/test_main.cpp` (three no-backend tests, see Step 1)
- Modify: `tests/native/test_steami_board/test_main.cpp` (`test_print_info_lists_chips`, `test_print_info_lists_the_chips_of_the_release`)

**Interfaces:**
- Consumes: `SteamiSsd1327Backend`, `SteamiGc9a01Backend` (Tasks 1, 2); `SteamiLazy<T>` (`emplace()`, `get()`, `built()`, `reset()`); `constexpr SteamiChip steamiRoute(SteamiRelease, SteamiFunction, uint8_t)` (SCREEN: NONE on UNKNOWN, GC9A01 on V1C, SSD1327 on V1A/V1B/V1AB); `steamiChipName(SteamiChip)`; `SteamiBoard::release()`; Task 1/2 recorder and `drawClippingCases`.
- Produces:
  - Members `SteamiLazy<SteamiSsd1327Backend> _ssd1327;` and `SteamiLazy<SteamiGc9a01Backend> _gc9a01;` (the constructor stays `constexpr`; `kConstantBoard` in `test_steami_board` still compiles).
  - `SteamiDisplayBackend* SteamiBoard::display();` the injected backend (no detection), else the routed slot built and not begun (while `NOT_STARTED`, the other slot is reset first), else `nullptr`.
  - `void SteamiBoard::setDisplayBackend(SteamiDisplayBackend& b);` while `NOT_STARTED`: stores `b` and resets both slots.
  - `bool SteamiBoard::built(SteamiChip c) const;` + `SSD1327` and `GC9A01` cases.
  - `printInfo()` line `"<chip> screen <state>"` (see the decisions); `end()` resets both slots.
  - `claim()` unchanged: `NOT_STARTED` for both panels.

- [ ] **Step 1: Write the failing tests and adapt the 3a assertions**

In `test_steami_display/test_main.cpp`, the fresh mock bus now goes through release detection, which logs E01 and the banner before E30. Add `static std::string loggedLine(const char* suffix)` (first line ending with `suffix`, `""` if none) and change:
- `test_board_without_backend_is_unknown_board`: `TEST_ASSERT_TRUE(board.release() == SteamiRelease::UNKNOWN)`; replace the 0-lines assertion with `countLogged("(E30)") == 0` and `countLogged("(E31)") == 0`.
- `test_no_backend_reports_e30_once`: `countLogged("(E30)") == 1` and `loggedLine("(E30)")` equals the exact E30 text already in the test.
- `test_protected_name_in_messages`: `loggedLine("(E30)").rfind("[STeaMi] SteamiTestDisplay: ", 0) == 0`.

In `test_steami_board/test_main.cpp`: `test_print_info_lists_chips` (V1AB) asserts `"SSD1327 screen not started\n"` is present and `"GC9A01"` absent. `test_print_info_lists_the_chips_of_the_release` asserts, on V1C, `"GC9A01 screen not started\n"` present, `"SSD1327"` absent and `!board.built(SteamiChip::GC9A01)` after `printInfo`. On UNKNOWN it asserts `" screen "` absent.

New tests in `test_steami_display_board` (`presentBme280()` = `Wire.setRegister(0x76, 0xD0, 0x60)` gives V1C, `presentHids()` = `Wire.setRegister(0x5F, 0x0F, 0xBC)` gives V1AB, as in `test_steami_board`):

```cpp
static void test_v1c_builds_only_the_gc9a01_slot(void) {
    presentBme280();
    SteamiDisplayBackend* b = board.display();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING("GC9A01", b->chip());
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_EQUAL_PTR(b, board.display());
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());   // built, not begun
}
static void test_v1ab_builds_only_the_ssd1327_slot(void) {
    // presentHids(): chip "SSD1327", built(SSD1327), !built(GC9A01), no SPI byte;
    // then board.end(), board.begin(SteamiRelease::V1A) and V1B: the same
}
static void test_sketch_without_screen_builds_neither(void) {
    // V1C (presentBme280) then, after restart, V1AB (presentHids): claim the candidate 0 chip of every
    // non-SCREEN function, has(SteamiFunction::SCREEN) is true, printInfo(StringPrint) runs;
    // neither panel is built and g_spi is empty
}
static void test_unknown_gives_e30_and_no_slot(void) {   // Review Focus 1
    SteamiDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(0, d.width());
    TEST_ASSERT_EQUAL_INT16(0, d.radius());
    TEST_ASSERT_FALSE(d.begin());
    d.clear(STEAMI_WHITE);
    d.show();
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E30)"));
    TEST_ASSERT_EQUAL_STRING("unknown board", d.error());
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
    TEST_ASSERT_EQUAL_size_t(0, gpioPinMode().count(CS_DISPLAY) + gpioPinMode().count(RST_DISPLAY));
}
static void test_v1c_geometry_before_begin(void) {   // Review Focus 2
    // board.begin(SteamiRelease::V1C); SteamiDisplay d(board): width 240, height 240, radius 120,
    // isColor true; built(GC9A01); no SPI byte; no mode set on CS_DISPLAY, RST_DISPLAY, SPI_INT_MISO;
    // d.begin() true; exactly one GC9A01_CMD_DISPON; board.beginDisplay() again sends nothing
}
static void test_release_forced_after_geometry_keeps_one_slot(void) {
    // board.begin(SteamiRelease::V1B); board.display() -> built(SSD1327);
    // board.begin(SteamiRelease::V1C); board.display() -> chip "GC9A01", built(GC9A01), !built(SSD1327)
}
static void test_v1ab_geometry_before_begin(void) {   // Review Focus 2
    // board.begin(SteamiRelease::V1B): width 128, radius 64, isColor false; built(SSD1327); no SPI byte;
    // RST_DISPLAY is OUTPUT and HIGH; d.begin() true, then bytes were sent; beginDisplay() again sends none
}
static void test_injected_backend_skips_detection(void) {   // Review Focus 3
    SteamiMemoryBackend mem(240, 240);
    board.setDisplayBackend(mem);
    TEST_ASSERT_EQUAL_PTR(&mem, board.display());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
}
static void test_injection_frees_an_unbegun_slot(void) {   // Review Focus 3
    // board.begin(V1B); board.display() builds SSD1327; setDisplayBackend(mem) -> !built(SSD1327),
    // display() == &mem. Then end(); begin(V1C); SteamiDisplay d(board); d.begin();
    // setDisplayBackend(mem) is ignored: display() is still the GC9A01 slot, built(GC9A01)
}
static void test_injected_failing_backend_gives_e31(void) {
    // board.begin(V1C); SteamiMemoryBackend mem(240, 240, true, false); setDisplayBackend(mem);
    // SteamiDisplay d(board); d.clear(STEAMI_WHITE); d.pixel(1, 1); d.show();
    // status NO_MEMORY, error "no memory", countLogged("(E31)") == 1, mem.fillCalls() == 0,
    // mem.pixelCalls() == 0, mem.showCalls() == 0, !built(GC9A01), g_spi empty
}
static void test_two_displays_share_the_gc9a01_slot(void) {   // suite 2; SteamiScreen comes in 4a
    // board.begin(V1C); SteamiDisplay a(board), b(board); a.pixel(1, 1); b.pixel(2, 2); b.show();
    // a.backend() == b.backend() == board.display(); one GC9A01_CMD_DISPON in total;
    // in lastData(115200) pixels (1, 1) and (2, 2) are 0xFF, 0xFF
}
static void test_v1ab_slot_clipping_cases(void) {   // the SSD1327 under ASan through the real slot
    // board.begin(V1B); SteamiDisplay d(board); drawClippingCases(d); frame = lastData(8192);
    // then on SteamiMemoryBackend(128, 128, false) injected after end(): same calls, outOfBounds 0,
    // assertOledFrameEquals(mem, frame)
}
static void test_end_destroys_panel_slots_and_restarts(void) {   // Review Focus 4
    // begin(V1C); d.begin() true; built(GC9A01); board.end(); !built(GC9A01);
    // board.setLogger(&capture); board.begin(V1C); d.pixel(1, 1) -> built again, DISPON count 2
}
static void test_print_info_lists_the_screen_state(void) {
    // V1C before any display call: "GC9A01 screen not started\n" and !built(GC9A01);
    // after SteamiDisplay(board).begin(): "GC9A01 screen OK\n";
    // end(); injected SteamiMemoryBackend(128, 128, true, false), d.begin(): "memory screen no memory\n";
    // end(); fresh bus (UNKNOWN): no " screen " in the text
}
static void test_claim_stays_not_started_for_panels(void) {
    // begin(V1C); display(); claim(GC9A01) and claim(SSD1327) are NOT_STARTED; built(GC9A01) stays true
}
static void test_sizeof_board_with_panel_slots(void) {
    TEST_ASSERT_LESS_THAN(2560, sizeof(SteamiBoard));
}
```

- [ ] **Step 2: Run to verify failure** — `.venv/bin/pio test -e native -f native/test_steami_display_board` — Expected: FAIL (`display()` returns `nullptr` on V1C).

- [ ] **Step 3: Implement** the Interfaces block in `SteamiBoard.h/.cpp`. `display()`: `if (_displayInjected) return _displayInjected;` then switch on the route (`&_gc9a01.emplace()`, `&_ssd1327.emplace()`, `nullptr`), resetting the other slot first while `_displayStatus == NOT_STARTED`. `printInfoTo()` prints the screen line through `emit` with the existing `line` buffer. In `end()`, reset `_gc9a01` and `_ssd1327` next to the 3a display resets.

- [ ] **Step 4: Run to verify pass**

Run: `make test-native` — Expected: every suite PASSED (`test_steami_board`, `test_steami_display`, `test_steami_functions`, `test_steami_forced*` included).
Run: `.venv/bin/pio test -e native_asan` — Expected: PASS, no sanitizer report (LeakSanitizer included).
Run: `make tidy`, `make build` — clean, `SUCCESS` (every `steami_board` TU now includes `SteamiSSD1327.h`, clean since the #264 `override` fix).

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/src/SteamiBoard.* tests/native/test_steami_display_board/test_main.cpp \
  tests/native/test_steami_display/test_main.cpp tests/native/test_steami_board/test_main.cpp
git commit -m "feat(board): Route the screen to the panel slot of the release."
```

---

### Task 4: `Drawing` example

**Files:**
- Create: `lib/steami_board/examples/Drawing/Drawing.ino`
- Modify: `scripts/tests/test_steami_display_rules.py`

**Interfaces:**
- Consumes: `SteamiDisplay` (default board `Steami`), `begin()`, `error()`, `isAvailable()`, `show()`; `drawGallery` body in `tests/native/helpers/SteamiGallery.h` (Task 1).
- Produces: the sketch printing `show(): <n> us` (Task 7 reads it), the global `SteamiDisplay screen;`, and the guard test `test_drawing_example_draws_the_gallery`.

- [ ] **Step 1: Write the failing guard**

```python
def test_drawing_example_draws_the_gallery(self) -> None:
    # body(path): the lines between "drawGallery(SteamiDisplay& d) {" and the next line that is "}",
    # each stripped, "//" comments and blank lines dropped.
    # assertEqual(body(Drawing.ino), body(tests/native/helpers/SteamiGallery.h)), both non-empty,
    # and assertIn("drawGallery(screen);", Drawing.ino text)
```

Run: `make test-tooling` — Expected: FAIL (`Drawing.ino` missing).

- [ ] **Step 2: Write `Drawing.ino`**

- Header comment: what it draws (the primitives gallery, same scene as the native gallery whose 240 px dump is the V1c reference), that it works on V1a/V1b (128 px grey) and V1c (240 px colour) through `centerX()`, `centerY()` and `radius()`, that `screen.begin()` comes first in `setup()` (the V1c frame buffer is 115 200 bytes in one block), and that the Serial monitor (115200 baud) shows how long `show()` takes.
- `#include <Arduino.h>`, `#include <SteamiDisplay.h>` (not `STeaMi.h`, which exists only in the package).
- `static void drawGallery(SteamiDisplay& d)`: the helper's body, line for line.
- `setup()`: `Serial.begin(115200); while (!Serial && millis() < 2000) {}`; if `!screen.begin()`, print `No screen: ` + `screen.error()` and return; else `drawGallery(screen);`.
- `loop()`: return at once unless `screen.isAvailable()`; time `screen.show()` with `micros()`, print `show(): ` + the microseconds + ` us`, `delay(2000)`. No `printf("%f")`.

- [ ] **Step 3: Verify**

Run: `make test-tooling` — Expected: `OK`.
Run: `make test-example/steami_board/Drawing` — Expected: `[PASS] steami_board/Drawing`.
Run: `make format-check` and `scripts/check-spdx.sh` — Expected: clean (the `.ino` is formatted too).

- [ ] **Step 4: Commit**

```bash
git add lib/steami_board/examples/Drawing/Drawing.ino scripts/tests/test_steami_display_rules.py
git commit -m "feat(board): Add the Drawing example of the display primitives."
```

---

### Task 5: Footprint gate

**Files:**
- Create: `scripts/check-footprint.py` (SPDX line 2, executable, style of `scripts/test-examples.py`)
- Create: `scripts/tests/test_check_footprint.py`
- Modify: `.github/workflows/build.yml` (step after "Compile examples")

**Interfaces:**
- Consumes: `lib/steami_board/examples/Drawing` (Task 4), `boards/steami.json` (`upload.maximum_size` 524288, `upload.maximum_ram_size` 196608), `.venv/bin/pio`, `arm-none-eabi-size` from `${PLATFORMIO_CORE_DIR:-~/.platformio}/packages/toolchain-gccarmnoneeabi/bin/`, else `PATH`.
- Test loader: the script starts with `from __future__ import annotations` like every script in `scripts/`, so its `@dataclass` types need the module in `sys.modules` while it executes. Do not copy `load_module()` of `test_build_arduino_package.py` as it stands (it skips that line and `dataclasses` raises `AttributeError` at import); use `spec = importlib.util.spec_from_file_location("check_footprint", SCRIPT); module = importlib.util.module_from_spec(spec); sys.modules[spec.name] = module; spec.loader.exec_module(module)`.
- Produces:
  - `SKETCHES: tuple[str, ...] = ("steami_board/Drawing",)`; `FLASH_START = 0x08000000`; `FLASH_MARGIN = 65536`; `MIN_FREE_RAM = 143360`.
  - `class FootprintError(Exception)`.
  - `@dataclass(frozen=True) class Footprint: flash: int; ram: int` and `class Limits` with the same fields.
  - `parse_sections(text: str) -> dict[str, tuple[int, int]]`: name to (size, addr) from `arm-none-eabi-size -A -d`; header, blank and `Total` lines skipped.
  - `measure(sections, flash_size: int) -> Footprint`: flash = sizes of the sections with `FLASH_START <= addr < FLASH_START + flash_size`, plus `.data` (every flash-resident byte, about 360 B above PlatformIO's `.text + .rodata + .data`); RAM = `.data + .bss + .noinit` (`.noinit` counted when present); `FootprintError` if `.data` or `.bss` is missing.
  - `limits(board_json: Path) -> Limits`: `maximum_size − FLASH_MARGIN`, `maximum_ram_size − MIN_FREE_RAM`.
  - `violations(name: str, fp: Footprint, lim: Limits) -> list[str]`, one message per exceeded limit, naming the sketch, `flash` or `RAM`, the value and the limit.
  - `main() -> int`: for each sketch, `PLATFORMIO_SRC_DIR=<example> .venv/bin/pio run -e steami` (print the output and return 1 on failure), size `.pio/build/steami/firmware.elf`, print `<sketch>: flash <n> B (limit <l>; all flash sections, PlatformIO counts .text+.rodata+.data), RAM <n> B (limit <l>, <free> B free)`, return 1 if any violation.

- [ ] **Step 1: Write the failing tests**

```python
# Section sizes of the v0.35.0 Temperature build (PlatformIO: Flash 65 728 B, RAM 5 044 B);
# .noinit and MAPPING_TABLE addresses as measured, the other flash addresses laid end to end.
SIZE_A = """\
.pio/build/steami/firmware.elf  :
section               size         addr
.isr_vector            316    134217728
.text                58824    134218044
.rodata               4632    134276868
.ARM                     8    134281500
.init_array             24    134281508
.fini_array             12    134281532
.data                 2272    536870916
.bss                  2772    536873188
.noinit                  0    536875960
._user_heap_stack     1536    536875960
MAPPING_TABLE           40    537067520
.ARM.attributes         48            0
.debug_info         123456            0
Total               193940
"""
# test_parse_sections: ".text" -> (58824, 134218044); "Total" and the header absent
# test_measure: Footprint(flash=66088, ram=5044) with flash_size 524288
#   (MAPPING_TABLE, ._user_heap_stack and the debug sections are not counted)
# test_measure_counts_noinit: SIZE_A with ".noinit 16" -> ram == 5060
# test_measure_without_bss_raises: FootprintError
# test_limits_from_the_board: limits(<repo>/boards/steami.json) == Limits(458752, 53248)
# test_violations: (458753, 53248) -> one message containing "flash"; (458752, 53249) -> one
#   containing "RAM"; (458752, 53248) -> []
```

Run: `make test-tooling` — Expected: FAIL (module missing).

- [ ] **Step 2: Implement `scripts/check-footprint.py`** per the Interfaces block (`argparse` with no option today, `sys.exit(main())`).

- [ ] **Step 3: Verify**

Run: `make test-tooling` — Expected: `OK`.
Run: `.venv/bin/python3 scripts/check-footprint.py` — Expected: exit 0 and one line for Drawing, RAM well under 53 248 B. Note the two figures for the PR.

- [ ] **Step 4: Add the CI step** in `.github/workflows/build.yml`, after "Compile examples": `- name: Check the flash and RAM footprint` / `run: .venv/bin/python3 scripts/check-footprint.py`.

- [ ] **Step 5: Commit**

```bash
git add scripts/check-footprint.py scripts/tests/test_check_footprint.py
git commit -m "test(tooling): Check the flash and RAM footprint of the Drawing sketch."
git add .github/workflows/build.yml
git commit -m "ci(ci): Run the footprint gate after the example builds."
```

---

### Task 6: Package checks, README, keywords and full verification

**Files:**
- Modify: `scripts/build-arduino-package.py` (`_copy_sources`), `scripts/tests/test_build_arduino_package.py`
- Modify: `lib/steami_board/README.md`, `lib/steami_board/keywords.txt`, `lib/steami_board/examples/BoardInfo/BoardInfo.ino` (header comment lines 15-19)

**Interfaces:**
- Consumes: everything above. The exact E30/E31 texts are in `lib/steami_board/src/SteamiDisplay.cpp` lines 10-15. The public `SteamiDisplay` API is in `SteamiDisplay.h`.
- Produces: `PackageError` for a source whose lower-case name is `steami.h`; the README display section; keywords.

- [ ] **Step 1: Write the failing package tests**

```python
def test_umbrella_name_is_reserved(self) -> None:
    write(self.lib / "aaa" / "src" / "Steami.h")
    with self.assertRaises(self.mod.PackageError) as ctx:
        self.build()
    self.assertIn("STeaMi.h", str(ctx.exception))

def test_umbrella_includes_display_headers(self) -> None:
    # build_package(real repo) as test_umbrella_includes_function_classes; '#include "<h>"' present for
    # SteamiDisplay.h, SteamiDisplayBackend.h, SteamiRgb.h, SteamiFont.h, SteamiTrig.h,
    # SteamiSsd1327Backend.h, SteamiGc9a01Backend.h, SteamiSSD1327.h, SteamiGC9A01.h
```

Run: `make test-tooling` — Expected: `test_umbrella_name_is_reserved` FAILS.

- [ ] **Step 2: Implement** the check in `_copy_sources()` (before the duplicate test; message names the file, its driver and the generated `STeaMi.h`). Run `make test-tooling` — Expected: `OK`.

- [ ] **Step 3: Commit**

```bash
git add scripts/build-arduino-package.py scripts/tests/test_build_arduino_package.py
git commit -m "test(tooling): Reserve the STeaMi.h name and check the display headers."
```

- [ ] **Step 4: Write the docs**

README (`lib/steami_board/README.md`):
- New `## Display` section after `## Colours`:
  - A short sketch: `SteamiDisplay screen;`, `screen.begin()` first in `setup()`, a few primitives, `screen.show()`.
  - A panel table: V1a/V1b SSD1327 128x128, 16 greys, 8 192 B buffer allocated when the screen is first used, `show()` 10 to 15 ms; V1c GC9A01 240x240 RGB565, 115 200 B allocated by `begin()`, `show()` at least 57.6 ms.
  - The primitives table, one row per public method of `SteamiDisplay.h` with its signature: physical pixels; `arc()` angles 0 = noon, clockwise, outer radius plus thickness; `text()` 8x8, UTF-8 with folded accents and `°`; `textWidth()` counts glyphs.
  - Rules: clipping; auto-begin; after a failure nothing is drawn and `error()` says why; every object shares the board's screen; the geometry is valid before `begin()` (on V1a/V1b this already allocates the 8 KB buffer), from `setup()` onwards only, never from a global initialiser (it detects the release over I2C before the core is initialised); V1a/V1b never report E31; force the release before the first drawing call; checked on a V1c, V1a/V1b by the native tests only.
  - Messages table with E30 and E31 (full texts, `who` is `SteamiDisplay`, short forms `"unknown board"` and `"no memory"`).
- API table: rows for `SteamiDisplayBackend* display()`, `void setDisplayBackend(SteamiDisplayBackend& b)`, `SteamiStatus beginDisplay()`. The `claim()` row: the displays go through `beginDisplay()`. The `printInfo()` rows: the screen line `<chip> screen <state>`. The `end()` row: frees the screen buffer.
- "Using your own drivers": a bullet saying not to build a `SteamiGC9A01` or `SteamiSSD1327` next to `SteamiDisplay`: a second frame buffer (115 200 B on V1c, 8 KB on V1a/V1b) and two drivers drawing to the same panel; the screen shows whichever called `show()` last.
- Testing block: `make test-native/steami_display_board`, `.venv/bin/pio test -e native_asan`, `.venv/bin/python3 scripts/check-footprint.py`.

`keywords.txt` (tab-separated, in the existing blocks): KEYWORD1 `SteamiDisplay`, `SteamiDisplayBackend`, `SteamiSsd1327Backend`, `SteamiGc9a01Backend`. KEYWORD2 `display`, `setDisplayBackend`, `beginDisplay`, `backend`, `isColor`, `width`, `height`, `centerX`, `centerY`, `radius`, `clear`, `pixel`, `line`, `hline`, `vline`, `rect`, `circle`, `triangle`, `arc`, `text`, `textWidth`, `show`, `rotate`, `invert`, `powerOn`, `powerOff`. LITERAL1 `CHAR_W`, `CHAR_H`. Skip names already in the file.

`BoardInfo.ino`: the comment says `printInfo()` also lists the screen of the release with `not started` (this sketch does not use it).

- [ ] **Step 5: Commit**

```bash
git add lib/steami_board/README.md lib/steami_board/keywords.txt \
  lib/steami_board/examples/BoardInfo/BoardInfo.ino
git commit -m "docs(board): Document the display primitives, E30 and E31."
```

- [ ] **Step 6: Full verification before the PR**

Run each; all must succeed:
- `make test-native`: every native suite PASSED.
- `.venv/bin/pio test -e native_asan`: PASSED, no `runtime error:`, no ASan/LSan report.
- `make lint`: clang-format, SPDX and clang-tidy (`make tidy`) clean.
- `make build`: `SUCCESS` for `steami`.
- `make test-examples`: every example PASS, `steami_board/Drawing` and the `ble` examples (now pulling the backends through `SteamiBoard.h`) included.
- `make test-tooling`: `OK`.
- `make package`, then `grep -c 'Steami\(Ssd1327Backend\|Gc9a01Backend\|Display\|DisplayBackend\)\.h' build/arduino-package/src/STeaMi.h`: `4`; `ls build/arduino-package/examples/STEAMI_BOARD/Drawing/Drawing.ino` exists.
- `.venv/bin/python3 scripts/check-footprint.py`: exit 0. Record Drawing's flash and RAM.
- Layer cost: `PLATFORMIO_SRC_DIR=lib/steami_board/examples/BoardInfo .venv/bin/pio run -e steami`; the layer cost is Drawing's PlatformIO flash minus BoardInfo's, plus the Temperature flash delta of the next bullet (BoardInfo already carries that SPI link cost, so the plain difference is about 2.2 KB low). Compare that sum with the spec's "about 8 to 12 KB" in the PR.
- Sensor-only sketch: `PLATFORMIO_SRC_DIR=lib/steami_board/examples/Temperature .venv/bin/pio run -e steami` and its `Flash: … used N bytes` and `RAM: … used N bytes`. Compare with 65 728 B and 5 044 B on v0.35.0. Expected: about +2.2 KB flash and +212 B RAM, the core SPI library pulled by the out-of-line `~GC9A01` that the slot code references (decision "Link cost in screenless sketches"). Growth above 2.5 KB of flash means driver code is linked: `arm-none-eabi-nm -C .pio/build/steami/firmware.elf | grep -E 'GC9A01|SSD1327'` names it; fix before the PR. Record the delta in the PR.
- Optional, when network access allows: arduino-cli in a scratch data directory (`ARDUINO_DIRECTORIES_DATA`, `ARDUINO_DIRECTORIES_USER`, `ARDUINO_DIRECTORIES_DOWNLOADS` under the scratchpad), core `STMicroelectronics:stm32@3.1.0`, `arduino-cli lib install STM32duinoBLE`, then `scripts/compile-package-examples.sh build/arduino-package`: every example PASS. Otherwise the `arduino-package` CI job is the check.
- `npx commitlint --from origin/main --to HEAD`: no error. `git log --format=%s origin/main..`: every header ≤ 72 characters, scopes `board|tests|tooling|ci`, no `!`, no `[skip ci]`, no attribution lines.

---

### Task 7: Hardware acceptance on the V1c

**Controller-run on hardware, needs a board slot; subagents never flash or open the serial port.**

**Files:** none committed. A throw-away sketch lives in the controller's scratchpad.

**Interfaces:**
- Consumes: `Drawing` (Task 4, prints `show(): <n> us`), the 240 px gallery `steami-display-gallery-240-color.ppm`, the webcam that films the V1c.
- Produces: for the PR, the photo, the `show()` time, the BLE and late-begin results.

- [ ] **Step 1: Reference image.** `mkdir -p <scratch>/ppm && STEAMI_PPM_DIR=<scratch>/ppm .venv/bin/pio test -e native -f native/test_steami_display` gives `steami-display-gallery-240-color.ppm` (the same file as the CI artefact `steami-display-gallery`). Convert it to PNG for the PR.

- [ ] **Step 2: Photo of Drawing.** Flash `lib/steami_board/examples/Drawing` on the V1c with PlatformIO `env:steami`. Take a webcam photo. Expected: the same scene as the reference (grey track with green half, "STeaMi", "20°C e", framed blue rectangle, red disc, yellow triangle, grey line), same places and colours, allowing for the camera. The dark-grey edge circle (radius 119, at the bezel) counts only if visible; its absence in the photo is not a mismatch. A mismatch is a V1c defect: it blocks the PR (decision 6) and goes back to Tasks 1 to 4.

- [ ] **Step 3: `show()` time.** Read the serial port: `show(): <n> us` lines. Expected: n of at least 57 600 (115 200 B at 16 MHz). Record the median of five lines. Sanity bound: 57 600 ≤ median ≤ 75 000 µs (spec: about 15 frames/s); outside it, record the figure and investigate the SPI clock and the transfer path (block `SPI_TRANSMITONLY` versus byte-wise) before the PR.

- [ ] **Step 4: BLE first, then the screen; late begin.** The scratch sketch: `#include <STM32duinoBLE.h>` and `<SteamiDisplay.h>`, `SteamiDisplay screen;` and, at namespace scope, `uint8_t* volatile hog = nullptr;` (the pointer itself is volatile, so `-Os` cannot drop the allocation; `volatile uint8_t* hog` in a local would be elided). `setup()` starts in both variants with `Serial.begin(115200); while (!Serial && millis() < 2000) {}` (the default logger drops lines while `!Serial`, `SteamiBoard.cpp:180`), then `BLE.begin()`, `BLE.setLocalName("STeaMi-3b")`, `BLE.advertise()`, then print `screen.begin()` and `screen.error()`. Expected: `begin()` true. Variant with `#define LATE_BEGIN`: before `screen.begin()`, `hog = (uint8_t*)malloc(100000); Serial.println(hog != nullptr);`, never freed. Expected: that line prints `1` (accept the run only then: the 100 000 B are really held), then `begin()` false, `error()` `"no memory"`, one `[STeaMi] SteamiDisplay: … (E31)` line, and `loop()` keeps printing a counter (the sketch goes on).

- [ ] **Step 5: PR text.** "Closes #260". The photo next to the reference PNG, the `show()` time, the BLE and late-begin results, the footprint figures (gate convention, see the decision "Footprint gate"), the layer cost against the spec's 8 to 12 KB, and the Temperature flash and RAM delta from Task 6. It also says:
- the V1a/V1b check was not run on hardware (decision 6; covered by the SSD1327 backend over the SPI mock and the grey gallery);
- "both cores" means PlatformIO and arduino-cli, both on STM32duino 3.1.0: no 2.12 environment exists since `env:steami` moved to 3.1.0, so the spec's "PlatformIO (core 2.12)" is read that way (the maintainer may correct the spec in a separate docs PR);
- the branch depends on #264 (the `override` fix in `SteamiSSD1327.h`, merged) and #267 (driver members initialised in place), merged first;
- carried to 4a: two `SteamiScreen` objects sharing one backend (spec section 7, suite 2).

---

## Out of scope

- The E30/E31 rows of the wiki's French error table and the wiki screen page (section 12: separate PR).
- Everything of 4a, 4b and 4c: `SteamiScreen`, `SteamiAt`, `SteamiValue`, widgets, `SteamiHistory`, mockup examples, `WeatherStation` (added to `SKETCHES` in 4c), `scripts/mockup-review.py`.
- Carried to 4a: in `test_steami_display_board`, two `SteamiScreen` objects on the V1c slot share one backend (spec section 7, suite 2).
- A zero-cost link of screenless sketches (decision "Link cost in screenless sketches"), unless the owner asks.
- MicroPython and tutorial follow-ups (section 12), changes to `lib/ssd1327` or `lib/gc9a01` (block `show()` for the SSD1327, E31 from the SSD1327), `showRect()`, DMA.
- V1a/V1b hardware checks (decision 6).
