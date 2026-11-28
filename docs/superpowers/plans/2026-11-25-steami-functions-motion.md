# Per-function classes, part B (motion, compass, APDS-9960, buttons) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the last eight per-function classes. They are `SteamiAccelerometer`, `SteamiGyroscope`, `SteamiMagnetometer`, `SteamiCompass`, `SteamiLight`, `SteamiColor`, `SteamiGesture` and `SteamiButtons`. They sit on the part A `SteamiHandle`, with the `ca` and `cm` calibrations applied automatically. The LIS2MDL heading is fixed to the convention measured on hardware.

**Architecture:**
- Each class owns one `SteamiHandle`, the composed non-virtual helper from part A. It reads through a switch on the chosen chip.
- `SteamiBoard::calibrate()` gains the accelerometer (`ca`) and magnetometer (`cm`) calibrations.
- Two driver fixes come first, because the classes rely on them:
  - the LIS2MDL heading convention (issue #249);
  - an MCP23009E port read that reports I2C failures.
- Native Unity suites use the Wire mock, plus a small behavioural fake for the ISM330DL, whose reset bit must clear.

**Tech Stack:** C++ (gnu++17 on target, `-std=c++17` native), STM32duino 3.1.0 through PlatformIO `env:steami` and arduino-cli, Unity native tests.

**Spec:** `docs/superpowers/specs/2026-11-25-steami-functional-layer-design.md`, sections 5.3, 5.4, 5.5, 7 (suite 3), 8 (item 2) and 9. Part A plan: `docs/superpowers/plans/2026-11-25-steami-functions-env.md` (merged in v0.32.0; read its "Decisions" for the handle rules). Issues: #247 (sub-project 2) and #249 (LIS2MDL heading).

## Global Constraints

- **Branch and commits:**
  - Branch `feat/steami-functions-motion`, base `main` (v0.32.0).
  - Commit scope `board`, or the driver's own scope (`lis2mdl`, `mcp23009e`) for a driver change.
  - Subjects in sentence case, ending with a period, header at most 78 characters.
  - Never write a breaking-change marker (`!` or `BREAKING CHANGE`), and never the string that skips CI. No attribution lines.
- **Code style:**
  - Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`, and headers use `#pragma once`.
  - `make lint` must pass: clang-format (Google, indent 4, 100 columns) and clang-tidy with `WarningsAsErrors: '*'`.
- **Packaging:** file names unique across `lib/*/src` (case-insensitive, checked by the packager), and no file named `Steami.h`.
- **Hard rules of the layer:**
  - No `STEAMI_BOARD_` in any `lib/*/src/*.cpp`.
  - No `SteamiConfig` method before `hasConfigZone()` returned true.
  - No RTTI, no exceptions, no heap in the layer.
  - Constructors only store addresses.
  - The SSD1327 and GC9A01 slots are never touched (sub-project 3).
- **Class shape:** every class has the common shape of part A:
  - `explicit SteamiXxx(SteamiBoard& board = Steami)`, `bool begin()`, `bool isAvailable() const`, `SteamiStatus status() const`, `const char* error() const` and `const char* chip() const`.
  - A read before `begin()` begins.
  - After a failure, reads return NAN, 0, `false` or `NONE` without bus traffic and without printing again.
  - A board accessor that returns `nullptr` (after `Steami.end()`) gives the unavailable value.
- **Messages:**
  - The texts come from `SteamiHandle`: E10 in its one-candidate or list form, E11 only for release-dependent chips, and E20.
  - Each class supplies its `SteamiFunctionInfo`: who, noun, hint and fallback.
  - Logged once per (class, code).
- **Calibration:** the keys are the MicroPython `steami_config` ones.
  - `ca` (`ox`, `oy`, `oz` in g) goes to the ISM330DL through `SteamiConfig::applyAccelerometerCalibration`.
  - `cm` (`hx`, `hy`, `hz` hard iron in LSB, `sx`, `sy`, `sz` scales) goes to the LIS2MDL through `SteamiConfig::getMagnetometerCalibration` and `SteamiLIS2MDL::setCalibrateStep`.
  - Without a config zone, nothing changes.
- **Heading convention:** measured on a V1c on 2026-11-25. The board was flat with the screen up, and the top of the screen is the USB connector side.
  - The flat heading clockwise from north is `atan2(-x, y)` on the hard-iron-corrected X and Y: north 14°, east 116°, south 185°, west 262°.
  - The current driver `atan2(y, x)` is 90° off.
  - The accelerometer reads z ≈ -1 g with the screen up.
- **Sizes:** `sizeof(SteamiBoard)` stays below 2560 bytes on native.

## Decisions taken while planning (refine the spec)

- **Driver fixes first.**
  - `SteamiLIS2MDL::headingFromVectors` becomes `atan2(-x, y)`, and `directionLabel` centres its 8 sectors: `(int)((angle + 22.5) / 45) % 8`. The DigitalCompass example then shows the right heading.
  - Tilt compensation is not changed: its axes are not verified on hardware. `SteamiCompass` uses the flat heading only.
  - MicroPython is tracked in steamicc/micropython-steami-lib#434, outside this plan.
- **MCP23009E port read with a status.** The driver's `getGpio()` returns 0 on an I2C error, which reads as "all D-PAD buttons pressed". A new `bool gpio(uint8_t& value)` returns false on an I2C error, and `SteamiButtons` uses it: a failed read leaves the D-PAD released.
- **`SteamiButtons` keeps A, B and MENU without the expander.**
  - Those three are MCU pins, with no bus involved.
  - If the MCP23009E does not answer, `begin()` returns false and prints E10 ("The A, B and MENU buttons still work."). The D-PAD reads released, while A, B and MENU keep working. This is the one exception to "a failed function returns its unavailable value".
- **Debounce.**
  - Every `SteamiButtons` call samples the seven buttons: one `gpio()` read plus three `digitalRead()`.
  - A button changes state only after 20 ms of stable reading, which matches the MicroPython examples' 20 ms polling.
  - Edges are latched per button until `wasPressed()` or `wasReleased()` consumes them.
  - `waitForPress(0)` waits forever.
- **`SteamiGesture` enables the gesture engine** in `begin()` (`enableGestureSensor(false)`).
  - The README documents that the light and colour readings freeze while a hand is over the sensor.
  - `read(0)` makes one pass of about 30 ms.
  - `name()` returns `"NONE"`, `"LEFT"`, `"RIGHT"`, `"UP"`, `"DOWN"`, `"NEAR"` or `"FAR"`.
  - The direction mapping is the driver's. It is not verified on hardware: there is no V1a or V1b here.
- **`SteamiColor::rgb()` is deferred.** `SteamiRgb` belongs to sub-project 3 (SteamiDisplay), so this plan ships `read(r, g, b)`, `red()`, `green()` and `blue()`.
- **Magnetometer units.**
  - `read()` is in µT, corrected for hard iron only: `(raw - off) × 0.15`, using the driver's public `xOff/yOff/zOff`.
  - `calibrate(seconds)` runs `calibrateMinmax3d(seconds × 50, 20)` and keeps the result in RAM; saving it to the config is left to the `steami_config` examples.
  - `SteamiMagnetometer` and `SteamiCompass` share the LIS2MDL, so a calibration through either affects both.
- **Accelerometer.**
  - `strength()` is the norm including gravity, about 1 g at rest, as in MicroPython's tetris example.
  - `isShaken(g)` is `strength() > g`, with no hold-off.
  - `x()`, `y()` and `z()` each take a fresh reading.
- **E20 with no chip on either release** stays unhandled. No class can reach it: SCREEN is not a class.

## Review Focus

- **A V1c sketch that uses every class** must print exactly three E20 lines, for light, colour and gesture, and nothing else. The other five classes must read real values. Test in Task 8 (hardware) and Task 6 (`test_apds_classes_e20_on_v1c`).
- **A D-PAD read that fails** must not report every arrow pressed, and A, B and MENU must keep working. Test in Task 7 (`test_dpad_bus_error_reads_released`, `test_buttons_without_expander`).
- **A button held down for several loops** must give one `wasPressed()`. A 5 ms bounce must give none. Test in Task 7 (`test_was_pressed_once_per_press`, `test_bounce_shorter_than_debounce_is_ignored`).
- **A calibration in the config** must change the readings automatically, also when the release is forced: `ca` shifts `SteamiAccelerometer::read()`, and `cm` shifts `SteamiCompass::heading()`. Test in Task 3 and Task 5.
- **The compass reads north as about 0° and east as about 90°** with the measured convention. Test in Task 1 (`test_heading_convention_matches_board`) and Task 5.

---

### Task 1: LIS2MDL heading convention and centred directions (#249)

**Files:**
- Modify: `lib/lis2mdl/src/SteamiLIS2MDL.cpp` (`headingFromVectors`, `directionLabel`)
- Modify: `lib/lis2mdl/README.md` (heading convention sentence)
- Test: `tests/native/test_lis2mdl/test_main.cpp`

**Interfaces:**
- Produces: `float SteamiLIS2MDL::headingFromVectors(float x, float y, float z, bool calibrated = true)` returns `atan2(-x', y')` in degrees clockwise from north, in [0, 360), then applies the heading offset, declination and filter as today. Here x' and y' are the calibrated components when `calibrated`. `const char* directionLabel(float angle = -1.0f)` uses centred sectors.

- [ ] **Step 1: Write the failing tests.** Use `headingFromVectors` with `calibrated = false`, offsets 0 and the filter off.
  ```cpp
  test_heading_convention_matches_board   // (x=0, y=+1) -> 0°, (x=-1, y=0) -> 90°, (x=0, y=-1) -> 180°, (x=+1, y=0) -> 270°
  test_direction_sectors_are_centred      // directionLabel(350) == "N", (10) == "N", (30) == "NE", (90) == "E", (200) == "S", (337.4f) == "NW", (337.6f) == "N"
  ```
  Update every existing heading or label assertion that encoded `atan2(y, x)` or uncentred sectors, and say in the report which ones changed.
- [ ] **Step 2: Run** `make test-native/lis2mdl`. Expected: FAIL.
- [ ] **Step 3: Implement** the two changes. Add one README sentence: "Heading: 0° when the top of the screen (the USB connector side) points north, increasing clockwise (`atan2(-x, y)`, measured on a V1c)."
- [ ] **Step 4: Run** `make test-native/lis2mdl` and `make test-examples/lis2mdl`. Expected: PASS.
- [ ] **Step 5: Commit** `fix(lis2mdl): Fix the heading convention and centre the directions.` with body `Closes #249`.

### Task 2: MCP23009E port read with a status

**Files:**
- Modify: `lib/mcp23009e/src/SteamiMCP23009E.h/.cpp`, `lib/mcp23009e/README.md`, `lib/mcp23009e/keywords.txt`
- Test: `tests/native/test_mcp23009e/test_main.cpp`

**Interfaces:**
- Produces: `bool SteamiMCP23009E::gpio(uint8_t& value);` reads the GPIO register in one transaction. On success it returns true with `value` set. If `endTransmission` or `requestFrom` fails, it returns false and leaves `value` unchanged. `getGpio()` keeps its behaviour.

- [ ] **Step 1: Write the failing tests.** `test_gpio_reads_port` (GPIO preloaded 0xB0, so true and 0xB0). `test_gpio_reports_bus_error`, using `Wire.setAbsent(0x20)` and also `Wire.setEndTransmissionResult(2)`: false, and the value is unchanged.
- [ ] **Step 2: Run** `make test-native/mcp23009e`. Expected: FAIL to compile.
- [ ] **Step 3: Implement.** Add the method to the README API table and to keywords.
- [ ] **Step 4: Run** `make test-native/mcp23009e`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(mcp23009e): Add a port read that reports bus errors.`

### Task 3: Shared types, fakes and the `ca`/`cm` calibrations

**Files:**
- Modify: `lib/steami_board/src/SteamiTypes.h`, `SteamiBoard.cpp` (`calibrate`)
- Create: `tests/shared/steami_fake_ism330dl.h`
- Modify: `tests/shared/steami_fake_chips.h` (LIS2MDL, APDS-9960, MCP23009E helpers)
- Test: `tests/native/test_steami_board/test_main.cpp`

**Interfaces:**
- Produces, in `SteamiTypes.h`:
  ```cpp
  struct SteamiVector { float x; float y; float z; };
  enum class SteamiButton : uint8_t { NONE, A, B, MENU, UP, DOWN, LEFT, RIGHT };
  ```
- Produces: `SteamiBoard::calibrate(SteamiChip c)` also handles:
  - `ISM330DL`: `applyAccelerometerCalibration(*ism330dl)`; true when a `ca` entry exists.
  - `LIS2MDL`: when `getMagnetometerCalibration(cal)` succeeds, `setCalibrateStep(cal.hardIronX, cal.hardIronY, cal.hardIronZ, cal.softIronX, cal.softIronY, cal.softIronZ)` and true.
  - Same once-per-chip and guarded config rules as part A.
- Produces, in `tests/shared/steami_fake_ism330dl.h`: `class SteamiFakeIsm330dl : public I2CDevice`. It is a 128-register model with a register pointer and auto-increment. A write to CTRL3_C (0x12) clears the SW_RESET bit (bit 0) after storing. It answers WHO_AM_I 0x6A and reports STATUS ready. It provides `void setAccelRaw(int16_t x, int16_t y, int16_t z)` and `void setGyroRaw(int16_t x, int16_t y, int16_t z)`, which fill OUTX_L_XL… and OUTX_L_G…. A public counter `int resetWrites` counts the CTRL3_C writes with SW_RESET set. Attach it with `Wire.attachDevice(0x6B, &fake)`.
- Produces, in `steami_fake_chips.h`:
  - `steamiFakeLis2mdl(TwoWire&, int16_t x, int16_t y, int16_t z)`: WHO_AM_I 0x40, data ready, OUTX/Y/Z.
  - `steamiFakeApds9960(TwoWire&, uint16_t clear, uint16_t red, uint16_t green, uint16_t blue)`: ID 0xAB, AVALID and the four channels.
  - `steamiFakeMcp23009e(TwoWire&, uint8_t gpio)`: IODIR 0xF0 and GPIO.

- [ ] **Step 1: Write the failing tests** in `test_steami_board`:
  ```cpp
  test_fake_ism330dl_lets_claim_succeed        // attach fake: claim(ISM330DL)==OK
  test_calibrate_applies_ca                    // zone {"rev":131,"ca":{"ox":0.1,"oy":0,"oz":0}}, fake accel raw = 1 g on x:
                                               // calibrate(ISM330DL) true; accelerationG().x ≈ 0.9
  test_calibrate_applies_cm                    // zone {"rev":131,"cm":{"hx":100,"hy":-50,"hz":0,"sx":1,"sy":1,"sz":1}}:
                                               // calibrate(LIS2MDL) true; lis2mdl()->xOff == 100, yOff == -50
  test_print_info_marks_ca_and_cm              // after both: "ISM330DL 0x6B OK, calibrated", "LIS2MDL 0x1E OK, calibrated"
  ```
- [ ] **Step 2: Run** `make test-native/steami_board`. Expected: FAIL.
- [ ] **Step 3: Implement.** Keep `sizeof(SteamiBoard)` below 2560.
- [ ] **Step 4: Run** `make test-native`. Expected: PASS, all suites.
- [ ] **Step 5: Commit** `feat(board): Apply the accelerometer and magnetometer calibrations.` and `test(tests): Add the ISM330DL fake and more chip fakes.`

### Task 4: `SteamiAccelerometer` and `SteamiGyroscope`

**Files:**
- Create: `lib/steami_board/src/SteamiAccelerometer.h/.cpp`, `SteamiGyroscope.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp`

**Interfaces:**
- Consumes: `SteamiHandle`, `SteamiFunctionInfo`, `SteamiVector` and `SteamiFakeIsm330dl`.
- Produces:
  - `SteamiAccelerometer`:
    - `SteamiVector read()` (g, NAN components when unavailable);
    - `float x()`, `float y()`, `float z()`;
    - `float strength()` (norm of `read()`);
    - `bool isShaken(float g = 2.0f)` (`strength() > g`).
    - Noun `"accelerometer"`, hint `"Check the board or try another one."`, fallback `"read() will return NAN."`.
  - `SteamiGyroscope`: `SteamiVector read()` (°/s), `float x()`, `float y()`, `float z()`. Noun `"gyroscope"`.

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  test_accelerometer_reads_g                // fake raw z = -16393 (≈ -1 g at ±2 g, 0.061 mg/LSB): z() ≈ -1.0, strength() ≈ 1.0
  test_accelerometer_is_shaken              // raw x = 2.5 g: isShaken() true, isShaken(3.0f) false
  test_accelerometer_and_gyroscope_share_chip  // both begin(): ISM330DL begun once (fake counts CTRL3_C reset writes == 1)
  test_gyroscope_reads_dps                  // raw x = 1000 at ±250 dps (8.75 mdps/LSB): x() ≈ 8.75
  test_accelerometer_absent                 // 0x6B absent: one E10 line "the ISM330DL accelerometer (I2C 0x6B) did not answer. Check the board or try another one. (E10)",
                                            // read() NAN on all axes, no transaction on 10 more reads
  test_accelerometer_calibrated_from_config // zone ca ox=0.1, raw x = 1 g: x() ≈ 0.9
  ```
- [ ] **Step 2: Run** `make test-native/steami_functions`. Expected: FAIL to compile.
- [ ] **Step 3: Implement** the two classes on the part A pattern, including the `nullptr` guard after `Steami.end()`.
- [ ] **Step 4: Run** `make test-native/steami_functions`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add accelerometer and gyroscope classes.`

### Task 5: `SteamiMagnetometer` and `SteamiCompass`

**Files:**
- Create: `lib/steami_board/src/SteamiMagnetometer.h/.cpp`, `SteamiCompass.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp`

**Interfaces:**
- Consumes: the Task 1 heading convention, `SteamiVector` and `steamiFakeLis2mdl`.
- Produces:
  - `SteamiMagnetometer`:
    - `SteamiVector read()` (µT, `(raw - off) × 0.15`, NAN when unavailable);
    - `float x()`, `float y()`, `float z()`, `float strength()`;
    - `void calibrate(uint16_t seconds = 12)`, which runs `calibrateMinmax3d(seconds × 50, 20)` and does nothing when unavailable.
    - Noun `"magnetometer"`.
  - `SteamiCompass`:
    - `float heading()`: the driver's `headingFlatOnly()` after Task 1, in [0, 360), NAN when unavailable;
    - `const char* direction()`: the driver's `directionLabel(heading())`, `""` when unavailable;
    - `void calibrate(uint16_t seconds = 12)`, as above;
    - `void setDeclination(float deg)`, forwarded to the driver, also when called before `begin()`. It is kept in the class and applied on the first successful begin.
    - Noun `"compass"`.

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  test_magnetometer_reads_ut               // fake raw (200, -100, 50), offsets 0: read() ≈ (30, -15, 7.5)
  test_magnetometer_uses_hard_iron_offsets // zone cm hx=100: x() ≈ 15
  test_compass_north_and_east              // fake (0, +300, 0) -> heading() ≈ 0 and direction()=="N";
                                           // fake (-300, 0, 0) -> heading() ≈ 90 and "E"
  test_compass_declination_before_begin    // setDeclination(10) then heading() for north fake ≈ 10
  test_compass_and_magnetometer_share_chip // both begin(): one LIS2MDL claim
  test_compass_absent                      // 0x1E absent: E10 once, heading() NAN, direction()=="", no transaction on 10 more calls
  ```
- [ ] **Step 2: Run.** Expected: FAIL to compile.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `make test-native/steami_functions`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add magnetometer and compass classes.`

### Task 6: `SteamiLight`, `SteamiColor` and `SteamiGesture`

**Files:**
- Create: `lib/steami_board/src/SteamiLight.h/.cpp`, `SteamiColor.h/.cpp`, `SteamiGesture.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp`

**Interfaces:**
- Consumes: `steamiFakeApds9960`.
- Produces:
  - `SteamiLight`: `uint16_t read()` (raw clear count, 0 when unavailable). Noun `"light sensor"`, fallback `"read() will return 0."`.
  - `SteamiColor`: `bool read(uint16_t& r, uint16_t& g, uint16_t& b)` (false and zeros when unavailable), `uint16_t red()`, `uint16_t green()`, `uint16_t blue()`. Noun `"colour sensor"`, fallback `"read() will return false."`.
  - `SteamiGesture`:
    - `enum Direction : uint8_t { NONE, LEFT, RIGHT, UP, DOWN, NEAR, FAR };`
    - `Direction read(uint32_t timeoutMs = 0)`;
    - `static const char* name(Direction d)`.
    - `begin()` enables the gesture engine once after a successful claim.
    - Noun `"gesture sensor"`, fallback `"read() will return NONE."`.

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  test_light_reads_clear                 // V1AB (APDS fake id): read() == clear value
  test_color_reads_rgb                   // red()/green()/blue() and read(r,g,b) true
  test_light_color_gesture_share_chip    // three begin(): APDS-9960 begun once
  test_gesture_begin_enables_engine      // after begin(): ENABLE register (0x80) has PON|GEN set
  test_gesture_none_without_motion       // read() == SteamiGesture::NONE (GSTATUS not valid)
  test_gesture_names                     // name(LEFT)=="LEFT", name(FAR)=="FAR", name(NONE)=="NONE"
  test_apds_classes_e20_on_v1c           // V1c (BME280 fake): three begin() false, status NOT_ON_THIS_BOARD, exactly three lines:
                                         // "[STeaMi] SteamiLight: this STeaMi is a V1c, it has no light sensor (APDS-9960 is on V1a/V1b only). read() will return 0. (E20)"
                                         // and the SteamiColor / SteamiGesture equivalents; Wire.transactions(0x39) == 0
  test_apds_forced_v1ab_absent_is_e11    // begin(SteamiRelease::V1B), 0x39 absent: SteamiColor E11 line ends with "(E11)" and contains "APDS-9960 colour sensor (I2C 0x39)"
  ```
- [ ] **Step 2: Run.** Expected: FAIL to compile.
- [ ] **Step 3: Implement.** In `SteamiGesture.h`, document that the light and colour readings freeze while the gesture engine tracks a hand.
- [ ] **Step 4: Run** `make test-native/steami_functions`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add light, colour and gesture classes.`

### Task 7: `SteamiButtons`

**Files:**
- Create: `lib/steami_board/src/SteamiButtons.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp`

**Interfaces:**
- Consumes: `SteamiButton`, `SteamiMCP23009E::gpio()` (Task 2), `MCP23009_BTN_UP/DOWN/LEFT/RIGHT`, `pins().buttonA/B/Menu` and `steamiFakeMcp23009e`.
- Produces:
  - Methods:
    - `bool isPressed(SteamiButton b)`: debounced state;
    - `bool wasPressed(SteamiButton b)` and `bool wasReleased(SteamiButton b)`: a latched edge, consumed by the call;
    - `SteamiButton waitForPress(uint32_t timeoutMs = 0)`: the first button newly pressed, `NONE` on timeout, and 0 waits forever.
  - `begin()`:
    - always sets `pinMode(INPUT_PULLUP)` on A, B and MENU;
    - claims the MCP23009E, and on OK runs `setup(pin, MCP23009_DIR_INPUT, MCP23009_PULLUP)` for the four D-PAD pins;
    - returns false and prints E10 when the expander does not answer.
  - All buttons are active low. Debounce is 20 ms of stable reading.
  - Noun `"D-PAD expander"`, hint `"The A, B and MENU buttons still work."`, fallback `"the D-PAD will read as released."`.

- [ ] **Step 1: Write the failing tests.** `setUp` sets `gpioPinState()[A_BUTTON] = gpioPinState()[B_BUTTON] = gpioPinState()[MENU_BUTTON] = HIGH`, and `millisClock()` advances through `delay()`.
  ```cpp
  test_buttons_read_a_and_dpad            // A low for 30 ms -> isPressed(A); GPIO bit 7 low for 30 ms -> isPressed(UP)
  test_was_pressed_once_per_press         // hold A for 200 ms polling every 10 ms: wasPressed(A) true exactly once
  test_was_released_after_release         // release A for 30 ms: wasReleased(A) true once
  test_bounce_shorter_than_debounce_is_ignored  // A low for 5 ms then high: no wasPressed(A)
  test_dpad_bus_error_reads_released      // after begin(), Wire.setAbsent(0x20): isPressed(UP..RIGHT) false; A still works
  test_buttons_without_expander           // 0x20 absent before begin(): begin() false, one line ending
                                          // "the MCP23009E D-PAD expander (I2C 0x20) did not answer. The A, B and MENU buttons still work. (E10)",
                                          // isPressed(A) still follows the pin
  test_wait_for_press_timeout             // nothing pressed: waitForPress(100) == NONE after ~100 ms simulated
  test_wait_for_press_returns_button      // B goes low after 50 ms (use a millis hook or preset sequence): waitForPress(500) == SteamiButton::B
  ```
  If the mock cannot change a pin mid-wait, implement the last test by pressing before the call and asserting the immediate return, and say so in the report.
- [ ] **Step 2: Run.** Expected: FAIL to compile.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `make test-native/steami_functions` and `make test-native`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add the buttons class.`

### Task 8: Examples, keywords, README and hardware check

**Files:**
- Create: `lib/steami_board/examples/Accelerometer`, `Gyroscope`, `Magnetometer`, `Compass`, `Light`, `Color`, `Gesture`, `Buttons` (`<Name>/<Name>.ino`)
- Create: `tests/hardware/test_steami_motion/test_main.cpp`
- Modify: `lib/steami_board/keywords.txt`, `lib/steami_board/README.md`

**Interfaces:**
- Each example:
  - starts with a comment explaining what it prints and on which releases;
  - uses a global object, `Serial.begin(115200)`, `begin()` in `setup()`, and prints `error()` when `begin()` returns false.
- `Compass` calibrates for 12 s when MENU is pressed. `Buttons` prints each `wasPressed()` and `wasReleased()`. `Gesture` prints `name(read())` when it is not NONE.
- The README adds:
  - the eight classes to the function table, with the composant per release;
  - the heading convention;
  - the `ca`/`cm` calibration keys;
  - the light/gesture interference note and the buttons exception (A, B and MENU without the expander).

- [ ] **Step 1: Write the eight examples and the keywords** (KEYWORD1 for the classes, `SteamiVector`, `SteamiButton`; KEYWORD2 for the new methods).
- [ ] **Step 2: Update the README.** Message examples must be the exact texts the tests assert.
- [ ] **Step 3: Write the hardware test** (build only; the controller runs it on the shared V1c):
  - V1c:
    - `SteamiAccelerometer::z()` in [-1.2, -0.8] with the board flat;
    - `SteamiGyroscope` norm < 10 °/s while still;
    - `SteamiMagnetometer::strength()` in [10, 150] µT;
    - `SteamiCompass::heading()` in [0, 360);
    - `SteamiLight`, `SteamiColor` and `SteamiGesture` `begin()` false, with `lastError()` ending in "(E20)";
    - `SteamiButtons::begin()` true.
  - V1a/V1b: the APDS assertions flip and the V1c-only ones are ignored.
- [ ] **Step 4: Run** `make test-examples/steami_board test-tooling lint` and the hardware build-only command `.venv/bin/pio test -e steami -f hardware/test_steami_motion --without-uploading --without-testing`. Expected: PASS, 14 examples in steami_board.
- [ ] **Step 5: Commit** `feat(board): Add the motion, light and button examples.` and `test(board): Add the motion classes hardware test.`
- [ ] **Step 6 (controller, with the user at the board):**
  - Ask `wiki-steami-71` for a V1c slot and run the hardware test.
  - Flash `Compass`, and with the user check north ≈ 0° and east ≈ 90° after a MENU calibration.
  - Flash `Buttons`, and have the user press A, B, MENU and the four arrows.
  - Restore `battery_status` and release the slot.

---

## Self-review notes

- **Spec coverage:** the eight remaining classes of 5.3 (Tasks 4–7), sharing (5.4: Tasks 4–6), absent functions and E20 on V1c (5.5: Task 6), the `ca`/`cm` calibrations (9: Task 3), one example per function and keywords (8.2: Task 8), and the done-criterion on V1c (Task 8). V1b is not available; this is stated in the PR.
- **Deferred:**
  - `SteamiColor::rgb()` (sub-project 3);
  - tilt-compensated heading (axes not verified);
  - MicroPython heading fix (micropython-steami-lib#434);
  - E20 with no chip on either release (unreachable).
