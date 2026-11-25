# SteamiBoard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `lib/steami_board`, a `SteamiBoard` class and its global `Steami` that detect the STeaMi release once (forced, then config zone, then I2C probing), own the internal I2C bus and one lazily built instance of every I2C chip driver, and report problems with coded messages.

**Architecture:** One new library folder with `SteamiTypes.h` (enums, revision decoding, routing), `SteamiLazy.h` (placement-new slot), `SteamiBoard.h/.cpp`. All logic lives in `SteamiBoard.cpp`; the header only carries the compile-time override registrar. Native Unity suites run detection against the Wire mock and `FakeDaplinkBridge`.

**Tech Stack:** C++ (gnu++17 on target, `-std=c++17` native), STM32duino 3.1.0 through PlatformIO `env:steami` and arduino-cli, Unity native tests, Python unittest for tooling checks.

**Spec:** `docs/superpowers/specs/2026-11-25-steami-functional-layer-design.md` (sections 5.1, 5.2, 5.2.1, 5.4, 5.5, 5.6, 7 and 9; sub-project 1 of section 8). Issue: #243.

## Global Constraints

- Branch `feat/steami-board`, base `main` (v0.30.0). Commit scope `board` (already in `commitlint.config.js`); subjects in sentence case, ending with a period, header at most 78 characters; never write a breaking-change marker (`!` or `BREAKING CHANGE`) and never the string that skips CI.
- Every `.h/.cpp/.ino` starts with `// SPDX-License-Identifier: GPL-3.0-or-later`; headers use `#pragma once`; clang-format (Google, indent 4, 100 columns) and clang-tidy (`WarningsAsErrors: '*'`) must pass (`make lint`).
- File names unique across `lib/*/src` (the package flattens them); no file named `Steami.h`.
- Only `SteamiBoard.h` reads `STEAMI_BOARD_V1A/V1B/V1C`; no `STEAMI_BOARD_` in any `lib/*/src/*.cpp`.
- No `SteamiConfig` method is called before `SteamiDaplinkBridge::hasConfigZone()` returned true.
- Constructors do no I/O and no heap allocation; slots are built on first use (placement new in static storage).
- `sizeof(SteamiBoard)` below 2560 bytes on native.
- Library code prints only through the logger (section 5.5); the default logger on target writes to `Serial` only when `Serial` is started (`if (Serial)`), and is `nullptr` on native.
- The library version stays 0.x.

## Decisions taken while planning (differ from or refine the spec)

- **Missing revision is silent.** The spec prints E02 for a missing *or* unknown revision. No board carries a factory revision yet (maintainer, 2026-11-25), so E02 on a missing value would greet every student at every boot. Missing `rev` -> probe without message; an invalid `rev` -> E02 then probe.
- **`steamiDecodeRevision()` returns by value** (`constexpr SteamiRevisionInfo steamiDecodeRevision(int32_t)`) instead of filling an out-parameter, so it can be checked with `static_assert`.
- **Two constructors.** `constexpr SteamiBoard()` (used by the global `Steami`: internal bus resolved on first use, pins from the variant) and `SteamiBoard(TwoWire&, const SteamiBoardPins&)` (tests and advanced use). A reference member initialised from a function-local static would make `Steami` dynamically initialised; the default constructor keeps it constant-initialised.
- **No display slot in this PR.** `display()`/`setDisplayBackend()` arrive with `SteamiDisplay` (sub-project 3). `claim()` returns `SteamiStatus::NOT_STARTED` for `SSD1327` and `GC9A01`.
- **`flashSizeKb()` is left out** (optional in the spec, no consumer yet).
- **The `bme` calibration key and automatic calibration** belong to sub-project 2 (function classes); this PR only exposes `config()`.
- **Routing is a free function** `steamiRoute(release, function, candidate)` in `SteamiTypes.h` rather than the member `route(function, candidate)`: it is constexpr, testable without a board, and the function classes call it with `Steami.release()`.

## Review Focus

- A V1c whose config holds an old example value (`{"rev":3}`) must still come out V1C (by probing), with E02 printed once, not UNKNOWN. Test in Task 4.
- A board with a legacy DAPLink (no config zone) must never receive a config command and must stay responsive, even when `Steami.begin()` is called twice. Test in Task 4.
- A sketch that calls `Steami.release()`, `has()` or a chip accessor without calling `begin()` first must get the same answer as after `begin()`. Test in Task 4.
- Calling `Steami.begin(SteamiRelease::V1C)` after an automatic detection must switch to the forced release without I2C traffic. Test in Task 4.
- A message reported twice (same class, same code) must reach the logger once, but `lastError()` must still hold it. Test in Task 3.

---

### Task 1: Native mocks for the board layer

**Files:**
- Modify: `tests/native/helpers/Wire.h`
- Modify: `tests/native/helpers/Arduino.h`
- Create: `tests/native/helpers/Print.h`
- Test: `tests/native/test_wire/test_main.cpp`

**Interfaces:**
- Produces (Wire mock): `TwoWire(uint32_t sda, uint32_t scl)` (stores nothing, like the default one); `void setAbsent(uint8_t address)` (from then on `endTransmission()` returns 2 and `requestFrom()` returns 0 for that address); `size_t transactions(uint8_t address) const` and `size_t totalTransactions() const` (one per `endTransmission()` and one per `requestFrom()`, attached devices included).
- Produces (Arduino.h): macros `I2C_INT_SDA 200`, `I2C_INT_SCL 201`, `A_BUTTON 202`, `B_BUTTON 203`, `MENU_BUTTON 204`, `RST_EXPANDER 205`, `INT_EXPANDER 206`, `CS_DISPLAY 207`, `RST_DISPLAY 208`, `SPI_INT_MOSI 209`, `SPI_INT_MISO 210`, `SPI_INT_SCK 211`; `#include "Print.h"`.
- Produces (Print.h): `class Print { public: virtual ~Print() = default; virtual size_t write(uint8_t c) = 0; size_t write(const char* s); size_t print(const char* s); size_t print(int32_t v); size_t print(uint32_t v, int base = 10); size_t println(const char* s = ""); size_t println(int32_t v); };` and `class StringPrint : public Print { public: std::string text; size_t write(uint8_t c) override; };`.

- [ ] **Step 1: Write the failing tests** in `tests/native/test_wire/test_main.cpp`:
  - `test_absent_address_nacks`: `wire.setAbsent(0x76)`; `beginTransmission(0x76); write(0xD0)` -> `endTransmission()` == 2; `requestFrom(0x76, 1)` == 0; address 0x5F still returns 0 and 1.
  - `test_transactions_are_counted_per_address`: one register read at 0x76 (`endTransmission` + `requestFrom`) -> `transactions(0x76)` == 2, `transactions(0x5F)` == 0, `totalTransactions()` == 2.
  - `test_pin_constructor_exists`: `TwoWire w(I2C_INT_SDA, I2C_INT_SCL); w.begin();` compiles and `w.totalTransactions()` == 0.
  - `test_string_print_collects_text`: `StringPrint p; p.print("a"); p.print(int32_t(-3)); p.print(uint32_t(0x3B), 16); p.println("z");` -> `p.text == "a-33bz\n"` (match the line ending you choose for `println`, and use the same one in Task 3).
- [ ] **Step 2: Run** `make test-native/wire` — Expected: compile errors (`setAbsent`, `transactions`, `StringPrint`, pin macros missing).
- [ ] **Step 3: Implement** the Interfaces above. Keep the existing behaviour for every address not marked absent; `Wire = TwoWire()` must clear the absent set and the counters.
- [ ] **Step 4: Run** `make test-native` — Expected: every existing suite still passes, plus the four new tests.
- [ ] **Step 5: Commit** `git add tests/native && git commit -m "test(board): Add absent addresses, transaction counts and Print to the mocks."`

### Task 2: Types, revision code and routing table

**Files:**
- Create: `lib/steami_board/library.properties`, `lib/steami_board/keywords.txt`
- Create: `lib/steami_board/src/SteamiTypes.h`, `lib/steami_board/src/SteamiLazy.h`
- Test: `tests/native/test_steami_board/test_main.cpp` (first tests of the suite)

**Interfaces:**
- Produces (`SteamiTypes.h`), verbatim from spec 5.2:
  `enum class SteamiRelease : uint8_t { UNKNOWN, V1A, V1B, V1AB, V1C };`
  `enum class SteamiOrigin : uint8_t { NONE, FORCED_RUNTIME, FORCED_DEFINE, CONFIG, PROBE };`
  `enum class SteamiStatus : uint8_t { OK, NOT_STARTED, NOT_ON_THIS_BOARD, NOT_RESPONDING, NO_MEMORY, UNKNOWN_BOARD };`
  `enum class SteamiChip : uint8_t { NONE, BME280, WSEN_HIDS, WSEN_PADS, APDS9960, ISM330DL, LIS2MDL, VL53L1X, BQ27441, MCP23009E, SSD1327, GC9A01, COUNT };`
  `enum class SteamiFunction : uint8_t { TEMPERATURE, HUMIDITY, PRESSURE, DISTANCE, ACCELEROMETER, GYROSCOPE, MAGNETOMETER, COMPASS, LIGHT, COLOR, GESTURE, BATTERY, BUTTONS, DISPLAY, COUNT };`
  `struct SteamiRevisionInfo { bool valid; uint8_t generation; uint8_t release; uint8_t index; SteamiRelease board; bool prototype; };`
  `constexpr SteamiRevisionInfo steamiDecodeRevision(int32_t rev);`
  `constexpr SteamiChip steamiRoute(SteamiRelease release, SteamiFunction f, uint8_t candidate);` (returns `SteamiChip::NONE` past the last candidate)
  `const char* steamiChipName(SteamiChip c);` (`"BME280"`, `"WSEN-HIDS"`, `"WSEN-PADS"`, `"APDS-9960"`, `"ISM330DL"`, `"LIS2MDL"`, `"VL53L1X"`, `"BQ27441"`, `"MCP23009E"`, `"SSD1327"`, `"GC9A01"`, `""` for NONE) and `uint8_t steamiChipAddress(SteamiChip c);` (0x76, 0x5F, 0x5D, 0x39, 0x6B, 0x1E, 0x29, 0x55, 0x20, 0 for the displays and NONE) — declared here, defined in `SteamiBoard.cpp` (Task 3).
- Produces (`SteamiLazy.h`): `template <typename T> class SteamiLazy { public: constexpr SteamiLazy() = default; template <typename... A> T& emplace(A&&... args); T* get(); bool built() const; void reset(); private: alignas(T) unsigned char _storage[sizeof(T)] = {}; bool _built = false; };` — `emplace` builds once (a second call returns the existing object), `reset` calls `~T()`.
- Revision code (spec 5.2.1): valid when `100 <= rev <= 999`, generation `rev / 100 == 1`, release `(rev / 10) % 10` in 1..3 (1 = V1A, 2 = V1B, 3 = V1C); `index = rev % 10`, `prototype = index == 0`. Anything else: `valid = false`, `board = UNKNOWN`.
- Routing (spec 5.3 table), candidates in order:

| Function | V1A / V1B / V1AB | V1C | UNKNOWN |
|---|---|---|---|
| TEMPERATURE | WSEN_HIDS, WSEN_PADS | BME280 | BME280, WSEN_HIDS, WSEN_PADS |
| HUMIDITY | WSEN_HIDS | BME280 | BME280, WSEN_HIDS |
| PRESSURE | WSEN_PADS | BME280 | BME280, WSEN_PADS |
| DISTANCE | VL53L1X | VL53L1X | VL53L1X |
| ACCELEROMETER, GYROSCOPE | ISM330DL | ISM330DL | ISM330DL |
| MAGNETOMETER, COMPASS | LIS2MDL | LIS2MDL | LIS2MDL |
| LIGHT, COLOR, GESTURE | APDS9960 | none | APDS9960 |
| BATTERY | BQ27441 | BQ27441 | BQ27441 |
| BUTTONS | MCP23009E | MCP23009E | MCP23009E |
| DISPLAY | SSD1327 | GC9A01 | none (E30, sub-project 3) |

- `library.properties`: `name=STeaMi Board`, `version=0.0.0`, `author=STeaMi contributors`, `maintainer=STeaMi contributors`, `sentence=Board detection and shared drivers for the STeaMi.`, `paragraph=Detects the STeaMi release (V1a, V1b or V1c) from the DAPLink config zone or the on-board sensors, and shares one instance of each sensor driver on the internal I2C bus.`, `category=Sensors`, `url=https://github.com/steamicc/arduino-steami-lib`, `architectures=*`, `depends=STeaMi DAPLink Bridge, STeaMi Config, STeaMi BME280, STeaMi WSEN-HIDS, STeaMi WSEN-PADS, STeaMi APDS9960, STeaMi ISM330DL, STeaMi LIS2MDL, STeaMi VL53L1X, STeaMi BQ27441, STeaMi MCP23009E` (use each dependency's exact `name=` from its `library.properties`), `includes=SteamiBoard.h`.

- [ ] **Step 1: Write the failing tests** (`tests/native/test_steami_board/test_main.cpp`, includes `"SteamiTypes.h"` and `"SteamiLazy.h"`):
  - `test_revision_codes`: `static_assert(steamiDecodeRevision(131).board == SteamiRelease::V1C, "")`; runtime asserts for 110 (V1A, prototype), 111 (V1A), 120 (V1B, prototype), 121 (V1B), 130 (V1C, prototype, index 0), 131 (V1C, index 1, not prototype); invalid: 3, 99, 100, 140, 210, 230, 1000, -131.
  - `test_routing_v1c_has_no_apds_and_uses_bme280`: `steamiRoute(V1C, TEMPERATURE, 0) == BME280`, `steamiRoute(V1C, TEMPERATURE, 1) == NONE`, `steamiRoute(V1C, GESTURE, 0) == NONE`, `steamiRoute(V1C, DISPLAY, 0) == GC9A01`.
  - `test_routing_v1ab_and_unknown_candidates`: `steamiRoute(V1AB, TEMPERATURE, 1) == WSEN_PADS`, `steamiRoute(V1B, DISPLAY, 0) == SSD1327`, `steamiRoute(UNKNOWN, HUMIDITY, 0) == BME280`, `steamiRoute(UNKNOWN, HUMIDITY, 1) == WSEN_HIDS`, `steamiRoute(UNKNOWN, DISPLAY, 0) == NONE`.
  - `test_lazy_slot_builds_once_and_resets`: a counting type `struct Probe { static int alive; int v; explicit Probe(int x) : v(x) { ++alive; } ~Probe() { --alive; } };` — `emplace(4)` then `emplace(9)` keeps `get()->v == 4` and `alive == 1`; `reset()` -> `alive == 0`, `built() == false`.
- [ ] **Step 2: Run** `make test-native/steami_board` — Expected: FAIL, headers missing.
- [ ] **Step 3: Implement** `SteamiTypes.h` and `SteamiLazy.h` per Interfaces (routing as one constexpr switch; no tables in RAM). Add `library.properties` and a `keywords.txt` listing every type and enum (`KEYWORD1`) and `steamiDecodeRevision` (`KEYWORD2`).
- [ ] **Step 4: Run** `make test-native/steami_board` — Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add the board types, the revision code and the routing table.`

### Task 3: SteamiBoard core: construction, logger, messages

**Files:**
- Create: `lib/steami_board/src/SteamiBoard.h`, `lib/steami_board/src/SteamiBoard.cpp`
- Test: `tests/native/test_steami_board/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 mocks, Task 2 types and `SteamiLazy`.
- Produces (`SteamiBoard.h`):
  ```cpp
  struct SteamiBoardPins { uint32_t sda, scl, expanderReset, expanderIrq, buttonA, buttonB, buttonMenu;
                           static SteamiBoardPins steami(); };   // I2C_INT_SDA, I2C_INT_SCL, RST_EXPANDER, INT_EXPANDER, A_BUTTON, B_BUTTON, MENU_BUTTON
  class SteamiBoard {
   public:
    constexpr SteamiBoard();                                   // internal bus resolved on first use
    SteamiBoard(TwoWire& wire, const SteamiBoardPins& pins);   // stores refs/values only
    void setLogger(void (*log)(const char* line));             // nullptr = silent
    const char* lastError() const;                             // "" when nothing was reported
    void report(const char* who, SteamiStatus s, const char* detail, uint8_t code);
    TwoWire& wire();                                           // begins the bus once
    void end();                                                // destroys slots, forgets release, reported pairs and runtime override
    // Task 4 adds detection, Task 5 the chip slots.
  };
  extern SteamiBoard Steami;
  ```
- Message format (spec 5.5): `[STeaMi] <who>: <detail> (E<code, 2 digits>)`, for example `[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on V1a/V1b only). gesture.read() will return NONE. (E20)`. `report()` stores the line in `lastError()` (fixed buffer of 192 chars, truncated) every time, and sends it to the logger only the first time a `(who, code)` pair is seen (remember up to 16 pairs; compare `who` with `strcmp`).
- Default logger: on `ARDUINO`, a function that does `if (Serial) Serial.println(line);`; otherwise `nullptr`.
- `wire()` on the default-constructed global: on `ARDUINO`, a function-local `static TwoWire internal(I2C_INT_SDA, I2C_INT_SCL)`; on native, the mock global `Wire`. Calls `begin()` on the bus the first time.
- Global: `SteamiBoard Steami;` defined in `SteamiBoard.cpp`.

- [ ] **Step 1: Write the failing tests:**
  - `test_constructor_touches_nothing`: `Wire = TwoWire(); gpioPinMode().clear(); SteamiBoard board(Wire, SteamiBoardPins::steami());` -> `Wire.totalTransactions() == 0`, `gpioPinMode().empty()`.
  - `test_report_formats_and_logs_once`: install a capturing logger (static `std::vector<std::string>`); `board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, "this STeaMi is a V1c, it has no gesture sensor.", 20)` twice -> logger got exactly one line equal to `"[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor. (E20)"`; `lastError()` equals the same text.
  - `test_report_same_code_other_class_logs_again`: same code, `who` = `"SteamiLight"` -> two lines.
  - `test_end_forgets_reported_pairs`: report, `end()`, report again -> two lines.
  - `test_sizeof_board`: `TEST_ASSERT_LESS_THAN(2560, sizeof(SteamiBoard));` (keep this test in the suite through Task 5 so the slots are counted).
- [ ] **Step 2: Run** `make test-native/steami_board` — Expected: FAIL (no `SteamiBoard.h`).
- [ ] **Step 3: Implement** per Interfaces; every test's `tearDown` calls `board.end()` and `setLogger(nullptr)`.
- [ ] **Step 4: Run** `make test-native/steami_board` — Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Add SteamiBoard with its logger and coded messages.`

### Task 4: Release detection

**Files:**
- Modify: `lib/steami_board/src/SteamiBoard.h`, `lib/steami_board/src/SteamiBoard.cpp`
- Test: `tests/native/test_steami_board/test_main.cpp`

**Interfaces:**
- Consumes: `steamiDecodeRevision`, `SteamiDaplinkBridge(TwoWire&)`, `bridge.begin()`, `bridge.hasConfigZone()`, `SteamiConfig(SteamiDaplinkBridge&)`, `config.load()`, `config.boardRevision(int32_t&)`.
- Produces:
  ```cpp
  bool begin();                       // detection, once; true unless the release stays UNKNOWN
  bool begin(SteamiRelease forced);   // runtime override, no I2C traffic, highest precedence
  SteamiRelease release();            // detects on first call
  bool isV1c();  bool isV1ab();       // V1A, V1B and V1AB answer true to isV1ab()
  SteamiOrigin origin() const;
  int32_t revision() const;           // -1 when not read from the config
  bool isPrototype() const;           // valid revision with index 0
  uint8_t schematicIndex() const;     // 0xFF when no valid revision
  const char* releaseName();          // "V1a", "V1b", "V1a/V1b", "V1c", "unknown"
  bool has(SteamiFunction f);         // steamiRoute(release(), f, 0) != NONE
  SteamiConfig* config();             // non-null only after a successful load during detection
  void printInfo(Print& out);         // banner line, then one line per I2C chip (Task 5 fills the chip states)
  void printInfo();                   // same, through the logger
  ```
  and in the header, outside the class: `void steamiForceRelease(SteamiRelease r);` plus the registrar block of spec 5.6, copied verbatim (the `#error` for two macros, `STEAMI_FORCED_RELEASE_`, the anonymous-namespace registrar object).
- Detection order (spec 5.2, steps 0 to 4): bus begin; forced (runtime value, else registrar value; E03 if the registrar saw two different values); config (bridge slot `begin()`, then `hasConfigZone()`, then config slot `load()`, then `boardRevision()`; a valid code gives origin `CONFIG`; a present but invalid code gives E02 then step 3; a missing code goes to step 3 silently); probe by chip ID: `0x76` reg `0xD0` == `0x60` -> V1C; else `0x5F` reg `0x0F` == `0xBC` -> V1AB; else `0x5D` reg `0x0F` == `0xB3` -> V1AB; else `0x39` reg `0x92` in {`0xAB`, `0x9C`, `0xA8`} -> V1AB; origin `PROBE`. Nothing: UNKNOWN, E01, `begin()` returns false. One probe = `beginTransmission(addr); write(reg); endTransmission(false) == 0 && requestFrom(addr, 1) == 1`. Never probe 0x3B.
- Messages (who = `"SteamiBoard"`):
  - E01: `cannot tell which STeaMi this is: no V1a/V1b or V1c sensor answered. Add #define STEAMI_BOARD_V1C (or V1B, V1A) before #include <STeaMi.h>.`
  - E02: `unknown board revision <rev> in the board config. Update the STeaMi library; the board is detected from its sensors instead.`
  - E03: `two different STEAMI_BOARD_ defines in this sketch. Keep only one, before #include <STeaMi.h>.`
- Banner (sent once through the logger at the end of detection, and first line of `printInfo`), for example:
  `[STeaMi] Board V1c (read from the board config, revision 131)`,
  `[STeaMi] Board V1c (read from the board config, revision 130, prototype)`,
  `[STeaMi] Board V1c (detected: BME280 at 0x76)`, `[STeaMi] Board V1a/V1b (detected: WSEN-HIDS at 0x5F)`,
  `[STeaMi] Board V1c (forced by STEAMI_BOARD_V1C)`, `[STeaMi] Board V1c (forced by Steami.begin())`,
  `[STeaMi] Board unknown (nothing detected)`.

- [ ] **Step 1: Write the failing tests.** Helpers in the suite: `attachFirmware()` (static `FakeDaplinkBridge`, reset, `Wire.attachDevice(DAPLINK_BRIDGE_DEFAULT_ADDR, &fw)`), `presentBme280()` (`Wire.setRegister(0x76, 0xD0, 0x60)`), `presentHids()` (`0x5F`, `0x0F`, `0xBC`), `presentPads()` (`0x5D`, `0x0F`, `0xB3`), `presentApds()` (`0x39`, `0x92`, `0xAB`). Each test builds `SteamiBoard board(Wire, SteamiBoardPins::steami())`.
  - `test_config_131_gives_v1c_without_probing`: `fw.store("{\"rev\":131}")` -> `release() == V1C`, `origin() == CONFIG`, `revision() == 131`, `!isPrototype()`, `schematicIndex() == 1`, `Wire.transactions(0x76) == 0 && Wire.transactions(0x5F) == 0`.
  - `test_config_130_is_a_prototype`: `{"rev":130}` -> V1C, `isPrototype()`.
  - `test_invalid_revision_reports_e02_and_probes`: `{"rev":3}` plus `presentBme280()` -> V1C, origin PROBE, one logged line ends with `(E02)`; same for `{"rev":230}` with `presentHids()` -> V1AB.
  - `test_missing_revision_probes_silently`: `{"name":"x"}` plus `presentBme280()` -> V1C/PROBE, no line ending with `(E02)`.
  - `test_legacy_firmware_never_gets_a_config_command`: `fw.legacyFirmware = true` plus `presentHids()` -> V1AB/PROBE; `fw.configCommandsOnLegacy == 0`; `!fw.hung`; call `board.begin()` a second time, still true and still `configCommandsOnLegacy == 0`.
  - `test_bridge_absent_probes`: nothing attached at 0x3B, `Wire.setAbsent(0x3B)`, `presentBme280()` -> V1C/PROBE, `config() == nullptr`.
  - `test_probe_matrix`: for each of {BME280 -> V1C}, {HIDS -> V1AB}, {PADS only -> V1AB}, {APDS only -> V1AB}, {nothing -> UNKNOWN, `begin()` false, a line ending with `(E01)`}; reset `Wire` and `board.end()` between cases.
  - `test_second_begin_makes_no_traffic`: after a probe detection, `n = Wire.totalTransactions()`; `begin()` again -> unchanged.
  - `test_runtime_override_wins_without_traffic`: detect V1AB by probing, then `begin(SteamiRelease::V1C)` -> V1C, origin `FORCED_RUNTIME`, `totalTransactions()` unchanged.
  - `test_queries_before_begin_detect`: `presentBme280()`; without `begin()`: `has(SteamiFunction::GESTURE) == false`, `has(SteamiFunction::TEMPERATURE) == true`, `releaseName()` is `"V1c"`.
  - `test_banner_and_print_info`: `{"rev":131}` -> the logger's first line is `"[STeaMi] Board V1c (read from the board config, revision 131)"`; `StringPrint p; board.printInfo(p);` -> `p.text` starts with the same line.
- [ ] **Step 2: Run** `make test-native/steami_board` — Expected: FAIL (no `begin`, `release`, ...).
- [ ] **Step 3: Implement** per Interfaces. The bridge and config objects live in `SteamiLazy` slots of the board (they count in `sizeof`). Do not call any `SteamiConfig` method when `hasConfigZone()` is false.
- [ ] **Step 4: Run** `make test-native/steami_board` and `make test-native` — Expected: PASS, other suites unchanged.
- [ ] **Step 5: Commit** `feat(board): Detect the release from the config zone, then by probing.`

### Task 5: Shared chip slots and `claim()`

**Files:**
- Modify: `lib/steami_board/src/SteamiBoard.h`, `lib/steami_board/src/SteamiBoard.cpp`
- Test: `tests/native/test_steami_board/test_main.cpp`

**Interfaces:**
- Consumes: the driver constructors (no I/O): `SteamiBME280(TwoWire&)`, `SteamiWsenHids(TwoWire&)`, `SteamiWsenPads(TwoWire&)`, `SteamiAPDS9960(TwoWire&)`, `SteamiISM330DL(TwoWire&)`, `SteamiLIS2MDL(TwoWire&)`, `SteamiVL53L1X(TwoWire&)`, `SteamiBQ27441(TwoWire&)`, `SteamiMCP23009E(TwoWire&, uint8_t resetPin, uint8_t address = MCP23009_I2C_ADDR, int interruptPin = -1)` built with `pins.expanderReset`, `MCP23009_I2C_ADDR`, `pins.expanderIrq`; each `begin()`.
- Produces:
  ```cpp
  SteamiStatus claim(SteamiChip c);   // build + begin once; cache OK / NOT_RESPONDING; NOT_STARTED for SSD1327, GC9A01, NONE
  bool built(SteamiChip c) const;     // slot constructed (tests and printInfo)
  SteamiBME280* bme280();  SteamiWsenHids* wsenHids();  SteamiWsenPads* wsenPads();
  SteamiAPDS9960* apds9960();  SteamiISM330DL* ism330dl();  SteamiLIS2MDL* lis2mdl();
  SteamiVL53L1X* vl53l1x();  SteamiBQ27441* bq27441();  SteamiMCP23009E* mcp23009e();
  ```
  Each accessor calls `claim()` and returns the instance when the status is `OK`, `nullptr` otherwise. `claim()` triggers detection first if it has not run. `printInfo()` lists every I2C chip as `<name> 0x<addr> OK|not responding|not tried`.

- [ ] **Step 1: Write the failing tests:**
  - `test_claim_builds_and_begins_once`: V1c by probing, BME280 registers for a successful `begin()` (copy the register preload used by `tests/native/test_bme280` for its `begin()` test); `claim(BME280)` twice -> `OK` both times, and the second call makes no transaction at 0x76 (`Wire.transactions(0x76)` unchanged).
  - `test_accessor_returns_same_instance`: `bme280() == bme280()`, non-null.
  - `test_not_responding_is_cached`: V1AB, nothing at 0x6B -> `claim(ISM330DL) == NOT_RESPONDING`, second call makes no transaction at 0x6B, `ism330dl() == nullptr`.
  - `test_v1ab_never_builds_bme280`: V1AB by probing; for every `SteamiFunction` except DISPLAY, `claim(steamiRoute(V1AB, f, 0))`; then `!built(SteamiChip::BME280)` and `!built(SteamiChip::GC9A01)`.
  - `test_print_info_lists_chips`: after the previous scenario, `printInfo(p)` contains `"BME280 0x76 not tried"` and `"WSEN-HIDS 0x5F"`.
  - `test_end_destroys_slots`: after claims, `end()` -> `built(SteamiChip::WSEN_HIDS) == false`, `release()` detects again.
  - keep `test_sizeof_board` passing (below 2560 bytes with every slot).
- [ ] **Step 2: Run** `make test-native/steami_board` — Expected: FAIL.
- [ ] **Step 3: Implement** one `SteamiLazy<T>` member per chip, a `uint8_t` state per chip (`NOT_TRIED`, `OK`, `NOT_RESPONDING`), `claim()` as one switch.
- [ ] **Step 4: Run** `make test-native` — Expected: PASS.
- [ ] **Step 5: Commit** `feat(board): Share one instance of each chip driver through claim().`

### Task 6: Compile-time override

**Files:**
- Create: `tests/native/test_steami_forced/test_main.cpp`, `tests/native/test_steami_forced/second_unit.cpp`
- Create: `tests/native/test_steami_forced_conflict/test_main.cpp`, `tests/native/test_steami_forced_conflict/other_unit.cpp`
- Create: `scripts/tests/test_steami_board_rules.py`
- Test: the three files above

**Interfaces:**
- Consumes: the registrar block (Task 4), the global `Steami`, `Steami.origin()`.

- [ ] **Step 1: Write the failing tests:**
  - `test_steami_forced/test_main.cpp` starts with `#define STEAMI_BOARD_V1C` before `#include "SteamiBoard.h"`; `second_unit.cpp` does the same and defines `int steamiSecondUnit() { return 1; }` (called from the test so the unit is linked). Test `test_define_forces_v1c_without_traffic`: `Wire = TwoWire();` then `Steami.begin()` -> `Steami.release() == V1C`, `Steami.origin() == FORCED_DEFINE`, `Wire.totalTransactions() == 0`; capturing logger shows `"[STeaMi] Board V1c (forced by STEAMI_BOARD_V1C)"`.
  - `test_steami_forced_conflict`: `test_main.cpp` defines `STEAMI_BOARD_V1C`, `other_unit.cpp` defines `STEAMI_BOARD_V1B`; test `test_two_defines_report_e03`: `Steami.begin()` -> a logged line ends with `(E03)`.
  - `scripts/tests/test_steami_board_rules.py` (unittest, skipped with `self.skipTest` when no `c++` is on PATH):
    - `test_two_macros_in_one_unit_fail_to_compile`: write a temp `.cpp` with `#define STEAMI_BOARD_V1B`, `#define STEAMI_BOARD_V1C`, `#include "SteamiBoard.h"`; run `c++ -std=c++17 -fsyntax-only -I tests/native/helpers -I <every lib/*/src>`; assert non-zero exit and `Define only one of STEAMI_BOARD_V1A` in stderr.
    - `test_no_board_macro_in_library_sources`: no `lib/*/src/*.cpp` contains `STEAMI_BOARD_`.
- [ ] **Step 2: Run** `make test-native/steami_forced test-native/steami_forced_conflict test-tooling` — Expected: PASS if Task 4 copied the registrar correctly; otherwise fix the registrar until they pass. (These tests pin behaviour Task 4 already wrote; a first run that passes is expected.)
- [ ] **Step 2b: Build with the define as a compiler flag:** `PLATFORMIO_BUILD_FLAGS=-DSTEAMI_BOARD_V1C make build` — Expected: SUCCESS (every unit gets a registrar, all write the same value, no duplicate symbol).
- [ ] **Step 3: Commit** `test(board): Check the STEAMI_BOARD_ defines and the library rules.`

### Task 7: Example, smoke sketch, package and docs

**Files:**
- Create: `lib/steami_board/examples/BoardInfo/BoardInfo.ino`, `lib/steami_board/README.md`
- Modify: `src/main.cpp`, `README.md` (component table), `CONTRIBUTING.md` (one exception line under "No `Serial.print()` in library code": the board logger), `scripts/build-arduino-package.py` (package `paragraph`: mention board detection)
- Modify: `lib/steami_config/README.md` (board information section), `lib/steami_config/examples/show_config/show_config.ino`
- Test: `make build`, `make test-examples/steami_board`, `make package`, `make lint`, `make test-tooling`

- [ ] **Step 1:** `BoardInfo.ino`: `Serial.begin(115200); while (!Serial && millis() < 2000) {}` then `Steami.begin(); Steami.printInfo();` and in `loop()` nothing. Header comment explains the three detection sources and how to force a release (`#define STEAMI_BOARD_V1C` before the include).
- [ ] **Step 2:** `src/main.cpp`: include `SteamiBoard.h` and call `Steami.begin(); Steami.printInfo(Serial);` after `Serial.begin`, keeping the existing driver checks (they still build their own `TwoWire`; moving examples to `Steami.wire()` is sub-project 2).
- [ ] **Step 3:** README of the library (sections: Hardware, Quick start with `BoardInfo`, Detection order, Revision code table copied from spec 5.2.1, Forcing a release, Messages table E01/E02/E03 with their text, API table, Testing). `steami_config` README: the board-information section documents the code (`setBoardRevision(131)` for a series V1c, decoding with `steamiDecodeRevision()` from the STeaMi Board library); `show_config.ino` prints the decoded value (`131 (V1c, index 1)`), decoding inline (`rev / 100`, `(rev / 10) % 10`, `rev % 10`) so that the example does not depend on `SteamiBoard`.
- [ ] **Step 4: Run** `make lint && make build && make test-examples/steami_board && make test-examples/steami_config && make package && make test-tooling` — Expected: all pass; `build/arduino-package/src/STeaMi.h` includes `SteamiBoard.h`, `SteamiLazy.h`, `SteamiTypes.h`; `build/arduino-package/examples/STEAMI_BOARD/BoardInfo/` exists.
- [ ] **Step 5: Commit** `docs(board): Add the BoardInfo example and document the revision code.`

### Task 8: Hardware test

**Files:**
- Create: `tests/hardware/test_steami_board/test_main.cpp`

**Interfaces:**
- Consumes: `Steami`, `ConfigZoneBackup` (`tests/shared/config_zone_backup.h`), `SteamiDaplinkBridge`, `SteamiConfig`.

- [ ] **Step 1: Write the test** (Unity, `env:steami`):
  - `test_detects_a_known_release`: `Steami.begin()` true, `release()` is V1C or V1AB, `printInfo(Serial)`.
  - `test_factory_revision_round_trip`: only when the loaded config has no `rev`: take a `ConfigZoneBackup`, write `{"rev":131}` through `SteamiConfig` (`setBoardRevision(131)`, `save()`), `Steami.end()`, `Steami.begin()` -> `origin() == CONFIG`, `revision() == 131`; restore the backup in all cases (also on failure, before the asserts); when the board already has a `rev`, assert it decodes and skip writing.
  - `test_logger_before_serial_begin_does_not_block`: `Serial.end()`, `Steami.report("SteamiBoard", SteamiStatus::OK, "<200-character text>", 99)` twice, `Serial.begin(115200)` -> the test reaches its next line (a hang fails the run by timeout) and `lastError()` holds the text.
- [ ] **Step 2: Run** on the shared V1c after asking the other session for a slot (`make test-hardware/steami_board`) — Expected: PASS; paste the transcript in the PR.
- [ ] **Step 3: Commit** `test(board): Add the hardware test of the board detection.`

### Task 9: Pull request

- [ ] **Step 1:** `make ci` — Expected: lint, build, native tests, examples, tooling all pass.
- [ ] **Step 2:** Push `feat/steami-board`, open the PR titled `feat(board): Add SteamiBoard, board detection and shared drivers.` with `Closes #243`, the decisions section of this plan, and the hardware transcript.

### Task 10: Revision code outside the library (separate PRs)

- [ ] **Step 1: Wiki** (`steamicc/wiki_steami`, issue + branch `docs/...`): page `docs/hardware/revisions.md`, section *Identifier sa version*: the decimal code table (110 to 131) and the MicroPython/Arduino reads; page `docs/software/arduino/` gets the French table of messages E01, E02, E03 (title, cause, what to do). Build, prettier, markdownlint, cspell.
- [ ] **Step 2: MicroPython** (`steamicc/micropython-steami-lib`, issue + PR): `lib/steami_config/README.md` and the examples that write `board_revision = 3` use the code (131 for a series V1c) and document it; no code change.
