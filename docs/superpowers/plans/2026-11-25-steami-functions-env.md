# Per-function classes, part A (environment, distance, battery) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first five per-function classes (`SteamiTemperature`, `SteamiHumidity`, `SteamiPressure`, `SteamiDistance`, `SteamiBattery`) on top of `SteamiBoard`, with shared drivers, automatic temperature calibration from the board config and the coded messages E10, E11 and E20. Then move every driver example onto `Steami.wire()`.

**Architecture:**
- A small non-virtual helper, `SteamiHandle`, carries the common shape: routing through `steamiRoute`, `claim()`, the messages, the status and `error()`. Each function class owns one `SteamiHandle` plus a few lines of reading code: a switch on the chosen chip, with no virtual call.
- `SteamiBoard` gains `calibrate(SteamiChip)`, which applies the `tc` calibration of the loaded config once per chip and remembers it for `printInfo()`.
- Native Unity suites run everything against the Wire mock, with fake chips moved to `tests/shared`.

**Tech Stack:**
- C++ (gnu++17 on target, `-std=c++17` native), STM32duino 3.1.0 through PlatformIO `env:steami` and arduino-cli.
- Unity native tests, Python unittest for tooling checks.

**Spec:** `docs/superpowers/specs/2026-11-25-steami-functional-layer-design.md`: sections 5.3, 5.4, 5.5, 7 (suite 3), 8 (item 2) and 9. Issue: #247. Part B (accelerometer, gyroscope, magnetometer, compass, light, colour, gesture, buttons) gets its own plan once this one is merged.

## Global Constraints

- **Branch and commits:**
  - Branch `feat/steami-functions`, base `main` (v0.31.0). Commit scope `board`, or the driver's own scope for a driver or its examples.
  - Subjects in sentence case, ending with a period, header at most 78 characters.
  - Never write a breaking-change marker (`!` or `BREAKING CHANGE`), and never the string that skips CI.
- **Code style:**
  - Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`, and headers use `#pragma once`.
  - `make lint` must pass: clang-format (Google, indent 4, 100 columns) and clang-tidy with `WarningsAsErrors: '*'`.
- **Packaging:** file names unique across `lib/*/src` (the package flattens them), and no file named `Steami.h`.
- **Hard rules of the layer:**
  - No `STEAMI_BOARD_` in any `lib/*/src/*.cpp`.
  - No `SteamiConfig` method before `SteamiDaplinkBridge::hasConfigZone()` returned true.
  - No RTTI, no exceptions, no heap allocation in the layer.
  - Constructors only store addresses.
- **Class shape:** every class has the common shape of spec 5.3:
  - `explicit SteamiXxx(SteamiBoard& board = Steami)`, `bool begin()`, `bool isAvailable() const`, `SteamiStatus status() const`, `const char* error() const` and `const char* chip() const`.
  - A read before `begin()` calls `begin()`. After a failure, reads return NAN, -1, 0 or `false` without bus traffic and without printing again.
- **Messages** follow `[STeaMi] <Class>: <what happened>. <what to do>. (Exx)` and are logged once per (class, code) through `SteamiBoard::report()`. The codes are E10 (chip not answering), E11 (forced or config release contradicted by a missing chip) and E20 (function not on this board).
- **Calibration:**
  - It is applied automatically in `begin()`.
  - The `tc` keys are the same as in MicroPython `steami_config`: `hid` (WSEN-HIDS), `pad` (WSEN-PADS) and the new `bme` (BME280).
  - Without a config zone, nothing changes.
- **Sizes:** `sizeof(SteamiBoard)` stays below 2560 bytes on native.
- **Version:** the library stays 0.x.

## Decisions taken while planning (refine the spec)

- **Two parts.** Section 8 allows the split. Part A (this plan) covers environment, distance and battery. Part B covers motion, APDS-9960 and buttons, which need decisions still open: compass convention (Arduino `atan2(y,x)` vs MicroPython `atan2(x,y)`), APDS engine activation, button debounce, and an ISM330DL fake whose reset bit clears.
- **`SteamiHandle` instead of a base class.** It is composed, not inherited: no virtual functions, and each class keeps its own public API in its header. The spec writes `SteamiBoard::route`; the code has the free function `steamiRoute`, and the handle uses it.
- **`claim()` stays as it is.** It never reports and never checks routing. The handle consults `steamiRoute` before `claim()` and emits E10, E11 and E20 itself.
- **Candidates and messages:**
  - The handle tries the candidates in order (two on V1a/V1b for temperature, up to three on an unknown release) and stops at the first that answers, without a message.
  - When every candidate fails, it prints one message: E11 if `origin()` is `FORCED_RUNTIME`, `FORCED_DEFINE` or `CONFIG`, otherwise E10.
  - After a failure, `chip()` is `""` and `status()` is `NOT_RESPONDING`.
  - On an unknown release, a function with no candidate gets `UNKNOWN_BOARD` and no message, because E01 already said it.
- **`MAX_REPORTED` goes from 16 to 24.** Thirteen classes with one code each, plus E01 to E03, must fit; past the cap, `report()` re-delivers every call.
- **Calibration lives in the board.** `bool SteamiBoard::calibrate(SteamiChip c)` applies once per chip and keeps two bitmasks: tried and applied.
  - When the release was forced, `config()` was never loaded. `calibrate()` then loads it on demand through the same guarded sequence as detection (`bridge.begin()`, `hasConfigZone()`, `load()`), once.
  - `printInfo()` appends `, calibrated` to the line of every chip whose calibration was applied.
- **`SteamiTemperature::setOffset()` stays in the class.** It is added to the reading and does not touch the shared driver, so `Steami.bme280()` and the other classes are unaffected.
- **Distance.** `int read()` returns -1 when unavailable and also when the driver returns 0, since `SteamiVL53L1X` returns 0 on timeout. A true 0 mm reading therefore reads as -1, which is documented. `float cm()` returns NAN when `read()` is -1.
- **Battery.**
  - `percent()` returns 0 when unavailable.
  - `voltage()` returns volts, and NAN when unavailable or when the gauge reads 0 mV.
  - `current()` returns mA, positive while charging, and 0 when unavailable.
  - `isCharging()` is `current() > 0`, so the driver's private `flags()` stays untouched.
- **`Steami.wire()` in a global initialiser is allowed.**
  - Verified on a V1c on 2026-11-25 (STM32duino 3.1.0): `SteamiBME280 bme(Steami.wire());` as a global reads correctly next to `Steami`'s shared instance, also after a reset.
  - The driver examples become `SteamiXxx driver(Steami.wire());` with no bus line in `setup()`.
  - The README rule "never from a global initialiser" is replaced by "one bus: always `Steami.wire()`".
- **The MicroPython `bme` key** is a separate PR in `steamicc/micropython-steami-lib` (Task 8).

## Review Focus

- **A sketch that only reads, with no `begin()` call,** must work: `SteamiTemperature t; void loop(){ t.read(); }` auto-begins, detects the board and claims the chip. Test in Task 3 (`test_read_before_begin_auto_begins`).
- **A missing chip must not slow every loop down.** After the first failure, ten `read()` calls make no I2C transaction and print no new line: the VL53L1X timeout is about 1 s and the BQ27441 can block for seconds. Test in Task 3 (`test_failed_function_is_silent_and_quiet`) and Task 4 (`test_distance_absent`, `test_battery_absent`).
- **A forced release on a board with a legacy DAPLink** must never send a config command when a class asks for calibration. Test in Task 2 (`test_calibrate_forced_legacy_no_config_command`).
- **Two objects of one class, or two classes on one chip,** must begin the chip once and print at most one message per class. Test in Task 3 (`test_shared_bme280_begins_once`, `test_two_objects_one_message`).
- **A config that holds a `bme` calibration** must change `SteamiTemperature::read()` on a V1c, even when the release is forced. Test in Task 3 (`test_temperature_calibrated_from_config`, `test_temperature_calibrated_when_forced`).

---

### Task 1: `bme` temperature calibration key

**Files:**
- Modify: `lib/steami_config/src/SteamiConfig_const.h:8-14`
- Modify: `lib/steami_config/src/SteamiConfig.cpp:10-17`
- Modify: `lib/steami_config/README.md` (supported sensors list)
- Test: `tests/native/test_steami_config/test_main.cpp:90-106`

**Interfaces:**
- Produces: `constexpr const char* STEAMI_CONFIG_SENSOR_BME280 = "bme280";` with JSON short key `"bme"`, and `STEAMI_CONFIG_SENSOR_COUNT = 6`.

- [ ] **Step 1: Write the failing test.** Add `"bme280"` to the `sensors[]` array of `test_temperature_calibration_supported_sensors`, and add `test_bme280_calibration_uses_short_key_bme`. It calls `config.setTemperatureCalibration("bme280", 1.0f, -2.0f)`, saves through the fake bridge, and asserts that the stored JSON contains `"tc":{"bme":{"g":1.0,"o":-2.0}}`, using the same JSON assertion helpers the suite already uses for `hid`.
- [ ] **Step 2: Run** `make test-native/steami_config`. Expected: FAIL (`setTemperatureCalibration` returns false for `"bme280"`).
- [ ] **Step 3: Implement.**
  - Add the constant and set `STEAMI_CONFIG_SENSOR_COUNT` to 6.
  - Append `STEAMI_CONFIG_SENSOR_BME280` at the end of `kSensorNames`, and `"bme"` at the end of `kSensorShortKeys`. Appending keeps the existing indices.
  - Add the sensor to the README list.
- [ ] **Step 4: Run** `make test-native/steami_config`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(steami_config): Add the bme temperature calibration key.`

### Task 2: Board calibration, message codes and report capacity

**Files:**
- Modify: `lib/steami_board/src/SteamiTypes.h` (message codes)
- Modify: `lib/steami_board/src/SteamiBoard.h`, `lib/steami_board/src/SteamiBoard.cpp`
- Test: `tests/native/test_steami_board/test_main.cpp`
- Create: `tests/shared/steami_fake_chips.h`
- Modify: `tests/native/test_bme280/test_main.cpp`, `test_wsen_hids`, `test_wsen_pads`, `test_vl53l1x`, `test_bq27441` (use the shared fakes)

**Interfaces:**
- Produces, in `SteamiTypes.h`:
  ```cpp
  constexpr uint8_t STEAMI_E_UNKNOWN_BOARD = 1;
  constexpr uint8_t STEAMI_E_UNKNOWN_REVISION = 2;
  constexpr uint8_t STEAMI_E_CONFLICTING_DEFINES = 3;
  constexpr uint8_t STEAMI_E_NOT_ANSWERING = 10;
  constexpr uint8_t STEAMI_E_RELEASE_CONTRADICTED = 11;
  constexpr uint8_t STEAMI_E_NOT_ON_THIS_BOARD = 20;
  ```
  `SteamiBoard.cpp` uses these in place of its local `E_*` constants.
- Produces, in `SteamiBoard`:
  - `bool calibrate(SteamiChip c);` applies the config calibration of chip `c` once.
    - It returns true when a calibration was applied, now or on an earlier call.
    - It calls `claim(c)` first and does nothing when that is not OK.
    - In part A, only BME280 (`STEAMI_CONFIG_SENSOR_BME280`), WSEN-HIDS (`STEAMI_CONFIG_SENSOR_WSEN_HIDS`) and WSEN-PADS (`STEAMI_CONFIG_SENSOR_WSEN_PADS`) have a calibration, all through `SteamiConfig::applyTemperatureCalibration`. Other chips return false.
  - `bool calibrated(SteamiChip c) const;` is true when a calibration was applied to `c`.
  - `MAX_REPORTED = 24`.
  - Private: `bool loadConfig();` holds the guarded sequence now inside `detectFromConfig()` (bridge, `hasConfigZone()`, `load()`), run at most once per `begin()`/`end()` cycle; `detectFromConfig()` calls it. Also `uint16_t _calibrationTried`, `uint16_t _calibrationApplied` and `bool _configTried`. `end()` clears all three.
  - `printInfo()` chip line: `"%s 0x%02X %s"` becomes `"%s 0x%02X %s, calibrated"` when `calibrated(c)`.
- Produces, in `tests/shared/steami_fake_chips.h` (inline functions taking `TwoWire&`, moved from the driver suites unchanged in behaviour):
  - `steamiFakeBme280(TwoWire&)`: chip ID, Bosch example calibration and raw sample. Reads 25.08 °C, 1006.5325 hPa and 55.3535 %RH.
  - `steamiFakeWsenHids(TwoWire&, float tempC, float humidity)`: ID, factory calibration, measurement and STATUS H_DA|T_DA.
  - `steamiFakeWsenPads(TwoWire&, float tempC, float hPa)`: ID, measurement and STATUS P_DA|T_DA.
  - `steamiFakeVl53l1x(TwoWire&)`: who-am-I. `steamiFakeVl53l1xDistance(TwoWire&, uint16_t mm)` must be called after the driver's `begin()`.
  - `steamiFakeBq27441(TwoWire&)`: the control responses and the `CFGUPMODE` flag that the `test_bq27441` setUp uses. `steamiFakeBq27441Measurement(TwoWire&, uint16_t mV, int16_t mA, uint8_t soc)`.

- [ ] **Step 1: Move the fakes.** Move the `preload*` helpers into `tests/shared/steami_fake_chips.h` under the names above, and make the five driver suites call them.
- [ ] **Step 2: Run** `make test-native`. Expected: PASS, with the same counts as before.
- [ ] **Step 3: Write the failing tests** in `test_steami_board`. Every test uses `attachFirmware()` with the JSON shown, `steamiFakeBme280(Wire)` and `board.claim(SteamiChip::BME280)` unless it says otherwise.
  ```cpp
  // {"rev":131,"tc":{"bme":{"g":1.0,"o":-2.0}}}
  static void test_calibrate_applies_bme_key(void) {
      TEST_ASSERT_TRUE(board.calibrate(SteamiChip::BME280));
      TEST_ASSERT_TRUE(board.calibrated(SteamiChip::BME280));
      TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, board.bme280()->temperature());
  }
  // {"rev":131} (no tc): returns false, temperature 25.08, calibrated() false
  static void test_calibrate_without_key_changes_nothing(void);
  // calling twice reads the config once: fw.readConfigCommands unchanged by the 2nd call
  static void test_calibrate_is_once_per_chip(void);
  // board.begin(SteamiRelease::V1C) with the zone of the first test;
  // calibrate(BME280) loads the config on demand and returns true
  static void test_calibrate_forced_loads_config_on_demand(void);
  // fw.legacyFirmware = true, begin(V1C), calibrate(BME280): false and
  // fw.readConfigCommands == 0 and !fw.hung
  static void test_calibrate_forced_legacy_no_config_command(void);
  // printInfo(StringPrint) after test 1's setup contains "BME280 0x76 OK, calibrated\n"
  static void test_print_info_marks_calibrated_chip(void);
  // 20 distinct (who, code) pairs are each logged exactly once (cap now 24)
  static void test_report_keeps_twenty_pairs(void);
  ```
  Keep `test_sizeof_board` unchanged (below 2560).
- [ ] **Step 4: Run** `make test-native/steami_board`. Expected: FAIL to compile (`calibrate` undeclared).
- [ ] **Step 5: Implement** the interfaces above. `calibrate()` sets its "tried" bit before any bus traffic, so a failing config read is not retried.
- [ ] **Step 6: Run** `make test-native/steami_board test-native/steami_forced test-native/steami_forced_conflict`. Expected: PASS, `test_sizeof_board` included.
- [ ] **Step 7: Commit** `feat(board): Apply config calibrations once per chip.` (plus a separate `test(tests): Share the chip fakes between suites.` commit for Step 1).

### Task 3: `SteamiHandle`, `SteamiTemperature`, `SteamiHumidity`, `SteamiPressure`

**Files:**
- Create: `lib/steami_board/src/SteamiHandle.h`, `SteamiHandle.cpp`
- Create: `lib/steami_board/src/SteamiTemperature.h/.cpp`, `SteamiHumidity.h/.cpp`, `SteamiPressure.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp`

**Interfaces:**
- Consumes: `steamiRoute`, `steamiChipName`, `steamiChipAddress`, `SteamiBoard::claim/calibrate/report/release/origin/releaseName`, and the `STEAMI_E_*` codes (Task 2).
- Produces:
  ```cpp
  struct SteamiFunctionInfo {
      SteamiFunction function;
      const char* who;       // class name literal, e.g. "SteamiTemperature"
      const char* noun;      // "temperature sensor", "battery gauge"...
      const char* hint;      // E10 advice, e.g. "Check the board or try another one."
      const char* fallback;  // e.g. "read() will return NAN."
  };

  class SteamiHandle {
     public:
      constexpr SteamiHandle(SteamiBoard& board, const SteamiFunctionInfo& info) noexcept;
      bool begin();                          // idempotent; true when a candidate answered
      bool ready();                          // begin() on first use; true when OK
      bool isAvailable() const;              // status() == OK
      SteamiStatus status() const;
      const char* error() const;             // short text, see below
      const char* chip() const;              // steamiChipName(chipId()), "" when none
      SteamiChip chipId() const;
      SteamiBoard& board() const;
  };
  ```
  - `error()` returns `""` for OK, `"not started"`, `"not on this board"`, `"no answer"` or `"unknown board"`.
  - Each class has the common shape plus:
    - `SteamiTemperature`: `float read()` (°C, `setOffset` added) and `void setOffset(float celsius)`.
    - `SteamiHumidity`: `float read()` (%RH).
    - `SteamiPressure`: `float read()` (hPa) and `float altitude(float seaLevelHpa = 1013.25f)` (metres, through `SteamiBME280::altitudeFromPressure(read(), seaLevelHpa)` on every release).
  - Each class has one file-scope `constexpr SteamiFunctionInfo` in its `.cpp`. The nouns are `"temperature sensor"`, `"humidity sensor"` and `"pressure sensor"`; the hint is `"Check the board or try another one."` and the fallback `"read() will return NAN."`.
- Message texts. The `detail` passed to `report()` is copied exactly; `report()` adds the prefix and `(Exx)`.
  - E10, one candidate tried: `"the %s %s (I2C 0x%02X) did not answer. %s"`, with chip name, noun, address and hint.
  - E10, several candidates tried: `"no %s answered (%s). %s"`, with noun, comma-separated chip names in candidate order, and hint.
  - E11: `"this STeaMi is set as a %s (%s), but its %s %s (I2C 0x%02X) did not answer. Check the #define, Steami.begin() or the board config."`
    - It is filled with `releaseName()`, the origin text, the first candidate's name, the noun and its address.
    - The origin texts are `"forced by Steami.begin()"`, `"forced by #define"` and `"read from the board config"`.
  - E20: `"this STeaMi is a %s, it has no %s (%s is on %s only). %s"`.
    - It is filled with `releaseName()`, the noun, `steamiRoute(other, f, 0)`'s name, the other release's name and the fallback.
    - The other release is `V1AB` (`"V1a/V1b"`) when the board is V1c, and `V1C` (`"V1c"`) otherwise.
    - Not reachable by the part A functions. The test drives it through the handle with `SteamiFunction::GESTURE` on V1c.

- [ ] **Step 1: Write the failing tests** in `tests/native/test_steami_functions/test_main.cpp`.
  - Setup, like `test_steami_board`: `static SteamiBoard board(Wire, SteamiBoardPins::steami());`. `setUp` resets `Wire`, calls `board.end()` and installs a capturing logger. Each class is built on `board`.
  ```cpp
  test_temperature_on_v1c_reads_bme280            // fakeBme280: begin() true, chip()=="BME280", read()≈25.08
  test_humidity_and_pressure_on_v1c               // ≈55.35 %RH, ≈1006.53 hPa, altitude(1013.25f)≈ altitudeFromPressure value
  test_shared_bme280_begins_once                  // 3 classes begin(); Wire.transactions(0x76) after the 3rd begin()
                                                  // equals the count after the 1st begin()
  test_temperature_on_v1ab_reads_wsen_hids        // fakeWsenHids(21.5, 40): chip()=="WSEN-HIDS", read()≈21.5
  test_temperature_falls_back_to_wsen_pads        // HIDS absent, fakeWsenPads(22.0, 990): chip()=="WSEN-PADS", no line logged
  test_unknown_release_tries_every_candidate      // board.begin() with no fake (every ID register reads 0): UNKNOWN + E01;
                                                  // then steamiFakeWsenPads(Wire, 22.0f, 990.0f): SteamiTemperature
                                                  // skips BME280 and WSEN-HIDS (wrong ID), picks WSEN-PADS, no E10
  test_absent_chip_reports_e10_once               // fakeBme280, board.begin() (V1c by probe), then Wire.setAbsent(0x76)
                                                  // before temperature.begin(): status()==NOT_RESPONDING,
                                                  // chip()=="", error()=="no answer", one line ending
                                                  // "the BME280 temperature sensor (I2C 0x76) did not answer. Check the board or try another one. (E10)"
  test_unknown_release_lists_candidates_in_e10    // no fake at all (UNKNOWN): "no temperature sensor answered (BME280, WSEN-HIDS, WSEN-PADS)."
  test_forced_release_contradicted_reports_e11    // board.begin(SteamiRelease::V1C), BME280 absent:
                                                  // "this STeaMi is set as a V1c (forced by Steami.begin()), but its BME280 temperature sensor (I2C 0x76) did not answer." ... "(E11)"
  test_failed_function_is_silent_and_quiet        // after the failure, 10 read(): isnan, Wire.totalTransactions() unchanged, no new line
  test_two_objects_one_message                    // two SteamiTemperature on an absent chip: exactly one E10 line
  test_read_before_begin_auto_begins              // no begin(): read()≈25.08 and status()==OK
  test_set_offset_does_not_touch_shared_driver    // setOffset(1.5f): read()≈26.58, board.bme280()->temperature()≈25.08
  test_temperature_calibrated_from_config         // zone {"rev":131,"tc":{"bme":{"g":1.0,"o":-2.0}}}: read()≈23.08,
                                                  // printInfo shows "BME280 0x76 OK, calibrated"
  test_temperature_calibrated_when_forced         // same zone, board.begin(SteamiRelease::V1C): read()≈23.08
  test_hids_calibration_key_hid                   // V1AB, zone {"tc":{"hid":{"g":1.0,"o":0.5}}}: read()≈22.0 for a 21.5 fake
  test_handle_e20_on_v1c                          // SteamiHandle with {GESTURE,"SteamiGesture","gesture sensor",...} on V1c:
                                                  // begin() false, status NOT_ON_THIS_BOARD, line
                                                  // "this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on V1a/V1b only). read() will return NONE. (E20)",
                                                  // and Wire.transactions(0x39)==0
  ```
- [ ] **Step 2: Run** `make test-native/steami_functions`. Expected: FAIL to compile (missing headers).
- [ ] **Step 3: Implement** `SteamiHandle` and the three classes with the interfaces above.
- [ ] **Step 4: Run** `make test-native/steami_functions test-native/steami_board`. Expected: PASS.
- [ ] **Step 5: Run** `make lint`. Expected: no finding.
- [ ] **Step 6: Commit** `feat(board): Add temperature, humidity and pressure classes.`

### Task 4: `SteamiDistance` and `SteamiBattery`

**Files:**
- Create: `lib/steami_board/src/SteamiDistance.h/.cpp`, `SteamiBattery.h/.cpp`
- Test: `tests/native/test_steami_functions/test_main.cpp` (same suite)

**Interfaces:**
- Consumes: `SteamiHandle` and `SteamiFunctionInfo` (Task 3).
- Produces:
  - `SteamiDistance`: `int read()` (mm, -1 when unavailable or when the driver returns 0) and `float cm()` (NAN when `read()` is -1). Noun `"distance sensor"`, fallback `"read() will return -1."`.
  - `SteamiBattery`: `uint8_t percent()` (0 when unavailable), `float voltage()` (V, NAN when unavailable or 0 mV), `int16_t current()` (mA, positive while charging, 0 when unavailable) and `bool isCharging()` (`current() > 0`).
    - Noun `"battery gauge"`, hint `"Plug in a battery or check the board."`, fallback `"percent() will return 0."`.

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  test_distance_reads_mm               // fakeVl53l1x, begin(), then fakeVl53l1xDistance(Wire, 123): read()==123, cm()≈12.3
  test_distance_zero_is_minus_one      // distance 0: read()==-1, isnan(cm())
  test_distance_absent                 // 0x29 absent: begin() false, one E10 line, 10 read()==-1 with no new transaction
  test_battery_reads_gauge             // fakeBq27441 + measurement (3900 mV, 250 mA, 87 %): percent()==87,
                                       // voltage()≈3.9, current()==250, isCharging()
  test_battery_discharging             // -120 mA: isCharging()==false
  test_battery_absent                  // 0x55 absent: E10 ending "the BQ27441 battery gauge (I2C 0x55) did not answer. Plug in a battery or check the board. (E10)",
                                       // percent()==0, isnan(voltage()), current()==0, no transaction on 10 more calls
  ```
- [ ] **Step 2: Run** `make test-native/steami_functions`. Expected: FAIL to compile.
- [ ] **Step 3: Implement** the two classes.
- [ ] **Step 4: Run** `make test-native/steami_functions`. Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add distance and battery classes.`

### Task 5: Examples, keywords, README and packaging checks

**Files:**
- Create: `lib/steami_board/examples/Temperature/Temperature.ino`, `Humidity/Humidity.ino`, `Pressure/Pressure.ino` (prints hPa and altitude), `Distance/Distance.ino`, `Battery/Battery.ino`
- Modify: `lib/steami_board/keywords.txt`, `lib/steami_board/README.md`, `lib/steami_board/library.properties` (`includes=` unchanged; no new dependency)
- Modify: `scripts/build-arduino-package.py` and `scripts/tests/test_build_arduino_package.py`

**Interfaces:**
- Consumes: the five classes.
- Each example:
  - starts with a comment explaining what it prints and on which releases;
  - uses a global object (`SteamiTemperature temperature;`), `Serial.begin(115200)`, `begin()` in `setup()` and one reading per second in `loop()`;
  - prints `error()` when `begin()` returns false.

- [ ] **Step 1: Write the failing tooling tests** in `test_build_arduino_package.py`:
  - `test_case_insensitive_duplicate_is_rejected`: a fixture with `lib/a/src/Foo.h` and `lib/b/src/foo.h` raises `PackageError`.
  - `test_umbrella_includes_function_classes`: building from the real `lib/` gives a `STeaMi.h` containing `#include "SteamiTemperature.h"`, `SteamiHumidity.h`, `SteamiPressure.h`, `SteamiDistance.h`, `SteamiBattery.h` and `SteamiHandle.h`.
- [ ] **Step 2: Run** `make test-tooling`. Expected: the duplicate test FAILS.
- [ ] **Step 3: Implement** the case-insensitive check in `_copy_sources` (compare `name.lower()`).
- [ ] **Step 4: Write the five examples.** Add KEYWORD1 entries for the five classes and `SteamiHandle`, and KEYWORD2 entries for `read`, `setOffset`, `altitude`, `cm`, `percent`, `voltage`, `current`, `isCharging`, `isAvailable`, `status`, `error`, `chip`, `calibrate` and `calibrated`.
- [ ] **Step 5: Update the README.** Add a "Function classes" section with the routing table of the five classes per release, the common shape, the message table (E10, E11, E20 with their exact texts), the calibration keys and the "Distance 0 mm reads -1" note. In "Using your own drivers", replace the global-initialiser warning with the verified rule from the decisions above.
- [ ] **Step 6: Run** `make test-tooling test-examples/steami_board lint`. Expected: PASS, 6 examples.
- [ ] **Step 7: Commit** `feat(board): Add the function class examples.` and `feat(tooling): Reject case-insensitive duplicate file names.`

### Task 6: Driver examples and `src/main.cpp` on `Steami.wire()`

**Files:**
- Modify: the 33 examples listed in the survey. Each one currently creates `TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL)`:
  - apds9960: ambient_light, gesture, light_theremin, proximity
  - ble: BleBeacon
  - bme280: Altitude, BasicReader, ComfortZone, DewPointMonitor, WeatherStation
  - bq27441: battery_status, low_battery_alert
  - daplink_flash: data_logger
  - ism330dl: BasicReader, Motion, Orientation
  - lis2mdl: BasicReader, DigitalCompass, MagneticCalibration, MetalDetector
  - mcp23009e: dpad_reader, MorseCode, MusicPlayer, ReactionTimer, SimonGame
  - steami_config: show_config
  - wsen-hids: BasicReader, ComfortZone, DewPointMonitor, HumidityAlert
  - wsen-pads: altitude, floor_detector, read_pressure_temperature

  `grep -rln "TwoWire internalI2C" lib/*/examples` must list exactly these, and must list nothing afterwards.
- Modify: `src/main.cpp`, `CONTRIBUTING.md:164-174` ("Example folders"), and the driver READMEs whose quick start shows `internalI2C` (`grep -rln internalI2C lib/*/README.md`).

**Interfaces:**
- Consumes: `Steami.wire()`, safe in a global initialiser (decision above).
- Pattern (exact):
  ```cpp
  #include <SteamiBoard.h>
  #include <SteamiBME280.h>

  SteamiBME280 bme(Steami.wire());  // the board's internal I2C bus
  ```
  Remove the `TwoWire internalI2C(...)` line, every `internalI2C.begin()` and any `#include <Wire.h>` that only served it. Keep the rest of each example unchanged.

- [ ] **Step 1: Apply the pattern** to every file listed, one commit per driver scope (`refactor(bme280): Use Steami.wire() in the examples.` etc.). In `src/main.cpp`, build the drivers it lists on `Steami.wire()` the same way, keeping it the tidy entry point.
- [ ] **Step 2: Update** CONTRIBUTING ("Example folders": a driver example uses `Steami.wire()` and `#include <SteamiBoard.h>`) and the driver READMEs.
- [ ] **Step 3: Run** `grep -rn "internalI2C\|TwoWire internal" lib/*/examples lib/*/README.md src/ CONTRIBUTING.md`. Expected: no output.
- [ ] **Step 4: Run** `make ci`. Expected: PASS: lint, build, every native suite, every example (61 or more) and tooling.
- [ ] **Step 5: Compile the package examples with arduino-cli:** build the package (`python3 scripts/build-arduino-package.py`), then `scripts/compile-package-examples.sh`. Expected: every example compiles on STM32duino 3.1.0.

### Task 7: Hardware check on the V1c (controller only)

**Files:**
- Create: `tests/hardware/test_steami_functions/test_main.cpp` (PlatformIO `env:steami`)

The controller runs this task. The board is shared: ask `wiki-steami-71` for a slot through SendMessage, and tell it when done, restoring the sketch it asks for.

- [ ] **Step 1: Write** a Unity hardware test.
  - **V1c:**
    - `SteamiTemperature`, `SteamiHumidity` and `SteamiPressure` begin and read plausible values (0..50 °C, 10..100 %RH, 900..1100 hPa).
    - `Steami.built(SteamiChip::BME280)` is true and the WSEN slots are not built.
    - `SteamiDistance` returns -1 or 1..4000.
    - `SteamiBattery` either begins with 3.0..4.3 V, or reports E10. Both pass: the shared board's battery may be unplugged.
  - Without a config `bme` key, nothing is calibrated.
  - **V1a/V1b:** the same file passes on a V1b board when one is available. Not available to this session; say so in the PR.
- [ ] **Step 2: Run** `pio test -e steami -f hardware/test_steami_functions` on the V1c. Expected: PASS. Paste the result into the PR.
- [ ] **Step 3: Flash** each of the five new examples once and paste three lines of output per example into the PR.
- [ ] **Step 4: Commit** `test(board): Add the function classes hardware test.`

### Task 8: MicroPython `bme` key (separate PR in `steamicc/micropython-steami-lib`)

**Files (in `/home/nedjar/sandbox/micropython-steami-lib`, own branch `feat/steami-config-bme-key`, own issue):**
- Modify: `lib/steami_config/steami_config/device.py` (`_SENSOR_KEYS` gains `"bme280": "bme"`)
- Modify: `lib/bme280/bme280/device.py`: `_temp_gain = 1.0` and `_temp_offset = 0.0` set in `__init__`. `temperature()` returns `self._temp_gain * t + self._temp_offset`, as WSEN-HIDS does at `device.py:324`, so that `apply_temperature_calibration` works.
- Modify: `lib/steami_config/README.md` (key list) and the tests of both libraries, following the repository's test layout.

- [ ] **Step 1: Write the failing tests:**
  - `set_temperature_calibration("bme280", 1.0, -2.0)` stores `"bme"`;
  - `apply_temperature_calibration(bme)` lowers `temperature()` by 2.0 on the fake BME280.
- [ ] **Step 2: Run** the repository's test command. Expected: FAIL.
- [ ] **Step 3: Implement**, run the tests again, expect PASS, and commit with the repository's conventions.
- [ ] **Step 4: Open the PR**, linked to steamicc/arduino-steami-lib#247.

---

## Self-review notes

- **Spec coverage, part A:**
  - Common shape, auto-begin, silent failures, routing and candidates (5.3): Task 3.
  - Sharing and one bus (5.4): Tasks 2, 3 and 6.
  - E10, E11 and E20 (5.5): Task 3.
  - Calibrations with `bme` and listing in `printInfo()` (9): Tasks 1, 2 and 3.
  - Suite 3 for these classes, plus the case-insensitive and umbrella checks (7): Tasks 3, 4 and 5.
  - One example per function and keywords (8.2): Task 5.
  - Driver examples on `Steami.wire()`: Task 6.
  - Hardware: Task 7.
- **Left to part B:**
  - The eight remaining classes.
  - The `ca` and `cm` calibrations: `calibrate()` returns false for ISM330DL and LIS2MDL until then.
  - E20 on a real class.
  - The "one sketch with every class on V1b and V1c" done-criterion.
