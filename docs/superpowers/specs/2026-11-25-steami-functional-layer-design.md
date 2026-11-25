# STeaMi board-independent layer

Status: approved by the maintainer on 2026-11-25 (decisions in section 9). The library
version stays 0.x.
Repository: `steamicc/arduino-steami-lib`, base `main` at v0.30.0: #234 GC9A01, #235 BME280, #239 hardened
`hasConfigZone()`, #241 STM32duino 3.1.0, #167 `SteamiDaplinkBridge`/`SteamiDaplinkFlash` and #242 Steami
prefix rename are all merged. Each sub-project branches from `main`.

## 1. Intent and why

Today a student who wants the temperature on a STeaMi has to know which chip the board carries, build a
`TwoWire(I2C_INT_SDA, I2C_INT_SCL)`, and pick `SteamiWsenHids` on V1a/V1b or `SteamiBME280` on V1c. The screen
is worse: `SteamiSSD1327` takes grey levels 0..15 at 128x128, `SteamiGC9A01` takes RGB565 at 240x240.
A sketch written for one release does not build, or draws garbage, on another.

This layer gives teachers and students one class per *function* (temperature, distance, screen...). The
same sketch runs unchanged on V1a, V1b and V1c. It also gives Arduino the widget toolkit that MicroPython
already has in `steami_screen`, so a course can move between the two languages with the same vocabulary.

## 2. Goals, non-goals, anti-goals

Goals
- One class per function, English names, `Steami` prefix: the student never sees an I2C bus, an address or a chip.
- Same sketch on V1a/V1b/V1c. A function missing on the board makes `begin()` return false and print one clear message.
- Two functions on one chip share one driver instance, started once.
- Runtime board detection (config zone, then I2C probing), with a compile-time override that works with
  Arduino IDE separate compilation and with build flags.
- `SteamiDisplay` (primitives) and `SteamiScreen` (widgets) that work on the grey OLED and the colour TFT.
- Chip drivers stay public for advanced use and share the layer's bus and instances.

Non-goals (may come later)
- Microphone (no driver yet), RGB LED, buzzer, BLE: identical on all releases, can join later without
  touching detection. Pluggable external sensors (only the display backend is pluggable).
- Pixel-identical rendering with MicroPython (layout parity is a goal), telling V1a from V1b by probing,
  `scroll()` in `SteamiDisplay` (SSD1327 scroll allocates a second 8 KB buffer; use the driver).

Anti-goals (things this layer must never do)
- Never send a SteamiConfig command to a DAPLink before `hasConfigZone()` returned true.
- Never construct the SSD1327 (8 KB in its constructor) or begin the GC9A01 (115 200 B in `begin()`) on a
  release that does not carry it, or in a sketch that does not use the screen.
- Never test `STEAMI_BOARD_*` inside a library `.cpp`: a sketch `#define` does not reach it.
- No RTTI, no exceptions, no heap allocation in the layer itself (drivers keep their own buffers).

## 3. Constraints

- MCU STM32WB55: about 192 KB RAM, flash capped at 512 KB by the board entry (`upload.maximum_size`), the rest
  holds the BLE stack.
- Two cores: STM32duino 2.12 (PlatformIO) and 3.1.0 (Arduino IDE). Both compile with `-std=gnu++17 -fno-rtti
  -fno-exceptions -fno-threadsafe-statics -fno-use-cxa-atexit`. The layer only uses C++11/14 features plus
  `constexpr`, so it does not depend on the dialect.
- Packaging: one Library Manager library, `src/` flattened by `scripts/build-arduino-package.py`, generated
  umbrella `STeaMi.h` that includes every public header. File names must be unique, and no file may be named
  `Steami.h` (clash with `STeaMi.h` on case-insensitive file systems).
- Arduino IDE compiles library `.cpp` files without the sketch's `#define`s. Only headers included by the
  sketch see them.
- Old DAPLink 9513ae0 (V1a/V1b and test boards) has no config zone and locks up on config commands.
- Native tests (`tests/native`, Unity, mocked Wire/SPI) must cover the layer without hardware.

## 4. Approaches considered

Three proposals were scored 1 (poor) to 5 (best) on each criterion.

| Criterion | P1 runtime adapters | P2 compile-time templates | P3 student-first |
|---|---|---|---|
| Student simplicity | 4 | 3 | 5 |
| Correctness (IDE separate compilation, both cores) | 5 | 4 | 4 |
| RAM safety (GC9A01 buffer) | 5 | 5 | 4 |
| Detection safety (old DAPLink) | 5 | 5 | 5 |
| Testability | 5 | 4 | 4 |
| Maintainability | 3 | 2 | 4 |
| Consistency with MicroPython `steami_screen` | 4 | 4 | 4 |
| **Total / 35** | **31** | **27** | **30** |

**P1, runtime-polymorphic board layer (spine).** A global `SteamiBoard Steami` detects the release once
(forced > config > probe) and owns one lazily built slot per chip (`SteamiLazy<T>`, placement new in static
storage). Student handles hold a board pointer and a state byte. A header-side registrar with internal
linkage carries a sketch `#define` to the library at static-init time. Thorough error model (status enum,
once-per-function messages, `lastError()`, `printReport()`), and the best test plan (detection matrix,
legacy-firmware regression, forced-define TU, ASan clipping). Weak point: a pure-virtual "source" interface
plus an adapter class per chip plus a handle per function is three layers for what is mostly a two-way switch.
*Taken:* almost everything: global `Steami`, `SteamiLazy` slots for every chip, registrar override and
`begin(SteamiRelease)` runtime override, status and logger model, `SteamiAt`/`SteamiButton` scoped enums,
`SteamiHistory<N>`, mandatory clipping before the uint8_t SSD1327 API, test plan.

**P2, compile-time family templates.** `using SteamiTemperature = steami::TemperatureT<STEAMI_FAMILY>` with
`if constexpr` so that a forced build links only one release's drivers and drops the DAPLink/config code via
`--gc-sections`. Clever and the smallest forced binaries, but student errors show template names, the
`#define` placement changes types, multi-TU sketches risk ODR and `extern` mismatches, and every contributor
must keep a "no namespace-scope constructor in any .cpp" rule. *Taken:* the 512 KB flash cap and a footprint
CI step; the rule "no `STEAMI_BOARD_*` in any library .cpp" enforced by CI; a type-safe colour value type
(`SteamiRgb`, since `SteamiColor` is the colour-sensor class); probing fallbacks (WSEN-PADS, APDS-9960) and
"unknown release: each sensor tries its candidates"; `arduino-cli --build-property` forced builds in CI;
`textWidth()`; overload-ambiguity tests for `value()`.

**P3, student-first.** Simple handles that hold a `SteamiBoard&`, read through a switch on the chosen chip, no
virtual function except the display backend. `SteamiScreen` inherits `SteamiDisplay`, so one object gives
widgets and primitives, as in MicroPython. Teacher-oriented message format with codes that point to a French
table on the wiki. Weak points: renames the maintainer's `SteamiColor` sensor, unprefixed globals (`WHITE`,
`BUTTON_A`, `At`) that collide with other libraries, driver objects as plain members (relies on every driver
constructor staying inert). *Taken:* no per-sensor interfaces (switch on chip tag), `SteamiScreen : public
SteamiDisplay`, message format `[STeaMi] <Function>: <what happened>. <what to do>. (Exx)`, display refuses
to guess on an unknown release, UTF-8 degree sign, `menu()` that deduces the array size, release values that
keep V1a/V1b apart when the config says so, the "Serial before `Serial.begin()`" hardware check.

## 5. Design

### 5.1 Files

One new folder `lib/steami_board/src`: `SteamiLazy.h`, `SteamiTypes.h`, `SteamiBoard.h/.cpp` (the only
macro reader), one `.h/.cpp` pair per function class, `SteamiRgb.h`, `SteamiFont8x8.h/.cpp`,
`SteamiDisplayBackend.h`, `SteamiSsd1327Backend.h/.cpp`, `SteamiGc9a01Backend.h/.cpp`, `SteamiDisplay.h/.cpp`,
`SteamiScreen.h/.cpp`, `SteamiScreenConst.h`, `SteamiHistory.h`; examples `BoardInfo`, `WeatherStation`,
`ForcedV1c`, `ScreenWidgets`. All logic lives in `.cpp` files. Headers contain no inline function whose body
depends on a macro, so a sketch TU and a library TU can never disagree (no ODR risk).

### 5.2 SteamiBoard

```cpp
enum class SteamiRelease : uint8_t { UNKNOWN, V1A, V1B, V1AB, V1C };   // V1AB = "V1a or V1b", from probing
enum class SteamiOrigin  : uint8_t { NONE, FORCED_RUNTIME, FORCED_DEFINE, CONFIG, PROBE };
enum class SteamiStatus  : uint8_t { OK, NOT_STARTED, NOT_ON_THIS_BOARD, NOT_RESPONDING, NO_MEMORY, UNKNOWN_BOARD };
enum class SteamiChip    : uint8_t { NONE, BME280, WSEN_HIDS, WSEN_PADS, APDS9960, ISM330DL, LIS2MDL,
                                     VL53L1X, BQ27441, MCP23009E, SSD1327, GC9A01, COUNT };
enum class SteamiFunction : uint8_t { TEMPERATURE, HUMIDITY, PRESSURE, DISTANCE, ACCELEROMETER, GYROSCOPE,
                                      MAGNETOMETER, COMPASS, LIGHT, COLOR, GESTURE, BATTERY, BUTTONS, DISPLAY, COUNT };
struct SteamiBoardPins { uint32_t sda, scl, expanderReset, expanderIrq, buttonA, buttonB, buttonMenu;
                         static SteamiBoardPins steami(); };   // defaults from the STeaMi variant

class SteamiBoard {
 public:
  SteamiBoard(TwoWire& wire, const SteamiBoardPins& pins);   // stores refs only: no I/O, no heap
  bool begin();                         // bus + detection, idempotent; every function's begin() calls it
  bool begin(SteamiRelease forced);     // runtime override, highest precedence, no probing
  SteamiRelease release();              // detects on first call
  bool isV1c();  bool isV1ab();         // V1A, V1B and V1AB all answer true to isV1ab()
  SteamiOrigin origin() const;
  int32_t revision() const;             // board_revision from the config zone, -1 if not read
  bool isPrototype() const;             // true only when a valid revision has schematic index 0
  uint8_t schematicIndex() const;       // units digit of a valid revision (0 = prototype), 0xFF otherwise
  const char* releaseName();            // "V1a", "V1b", "V1a/V1b", "V1c", "unknown"
  bool has(SteamiFunction f);           // availability table, usable before any begin()
  void printInfo(Print& out = Serial);  // release, origin, revision, chip table (present/absent/not tried)
  const char* lastError() const;        // last message, for sketches that start Serial late
  void setLogger(void (*log)(const char* line));   // nullptr = silent; default prints on Serial

  TwoWire& wire();                      // THE internal I2C bus; advanced drivers must use it
  // Shared chip drivers, built and begun on first call; nullptr if absent on this board.
  SteamiBME280* bme280();      SteamiWsenHids* wsenHids();  SteamiWsenPads* wsenPads();
  SteamiAPDS9960* apds9960();  SteamiISM330DL* ism330dl();  SteamiLIS2MDL* lis2mdl();
  SteamiVL53L1X* vl53l1x();    SteamiBQ27441* bq27441();    SteamiMCP23009E* mcp23009e();
  SteamiConfig* config();      // nullptr unless the config zone exists and was loaded
  SteamiDisplayBackend* display();                 // built for the release, not begun
  void setDisplayBackend(SteamiDisplayBackend& b); // tests or an external panel

  // Used by the function classes (public, documented as internal)
  SteamiStatus claim(SteamiChip c);                // begin() the chip once, cache OK / NOT_RESPONDING
  SteamiChip route(SteamiFunction f, uint8_t candidate);   // routing table, see 5.3
  void report(const char* who, SteamiStatus s, const char* detail, uint8_t code); // once per (who, code)
  void end();                                      // UNIT_TEST and advanced: destroy slots, forget release
};
extern SteamiBoard Steami;
```

Storage. `SteamiBoard Steami` is defined in `SteamiBoard.cpp` next to a function-local `TwoWire` built on
`I2C_INT_SDA/I2C_INT_SCL` (the mock `Wire` on native). Every chip lives in a `SteamiLazy<T>` member:
`alignas(T) unsigned char _storage[sizeof(T)]; bool _built;` with `T& emplace(args...)` (placement new, once),
`T* get()`, `bool built()` and `void reset()` (calls `~T()`, used by `end()`). The board's state is
constant-initialised POD, so global function objects and the registrar are safe whatever the static-init
order. The whole board (all slots, MCP23009E's `std::function` array included) is about 1.5 to 2 KB of .bss;
a native test fails above 2.5 KB. Nothing is constructed until a function asks for it.

Detection, `SteamiBoard::begin()`, runs once:

0. `wire().begin()` once.
1. Forced: a runtime `begin(SteamiRelease)` argument, else the value written by the registrar (5.6). No I2C
   traffic at all. Origin `FORCED_*`.
2. Config:
   - build the `SteamiDaplinkBridge` slot, `bridge.begin()` (WHO_AM_I only, safe on 9513ae0). If it fails, go to 3.
   - `bridge.hasConfigZone()` (the #239 probe: READ_SECTOR, ERROR read before STATUS, Unknown counts as
     absent). If false, go to 3. **No SteamiConfig method is called before this returns true.**
   - build the `SteamiConfig` slot, `load()`, `boardRevision(rev)`. Decode `rev` with the board revision
     code of 5.2.1. Missing or unknown value: message E02 and go to 3. Origin `CONFIG`; the loaded config
     stays available for calibrations (5.3.1).
3. Probe, by chip ID (one register write + one-byte read each, never at 0x3B):
   - 0x76 reg 0xD0 == 0x60 (BME280) -> V1C;
   - else 0x5F reg 0x0F == 0xBC (WSEN-HIDS) -> V1AB;
   - else 0x5D reg 0x0F == 0xB3 (WSEN-PADS) -> V1AB;
   - else 0x39 ID in {0xAB, 0x9C, 0xA8} (APDS-9960) -> V1AB. Origin `PROBE`.
   Chip IDs, not ACKs, so a stray ACK cannot mislead and the native Wire mock (which ACKs everything) works.
4. Nothing: `UNKNOWN`, `begin()` returns false, message E01. Functions whose chip is the same on every
   release keep working, sensor functions try their candidates one by one, the display refuses (E30).

The banner is printed once: `[STeaMi] Board V1c (read from the board config, revision 131)`,
`(detected: BME280 at 0x76)` or `(forced by STEAMI_BOARD_V1C)`. It is how a teacher sees that a `#define`
was placed after the `#include`.

Cross-check: if a forced or config release is wrong, the first function whose chip is missing says so
(E11: `BME280 not found at 0x76, but the board is forced to V1c. Is this really a V1c?`).

### 5.2.1 Board revision code (decided 2026-11-25)

`board_revision` (`"rev"` in the config zone JSON) is a three-digit **decimal** integer, written at the factory:

| Digit | Meaning | Values |
| --- | --- | --- |
| hundreds | generation | 1 = V1, 2 = V2... |
| tens | release | 1 = a, 2 = b, 3 = c... |
| units | schematic index | 0 = prototype (index A, B, C), 1 = first series index (A1, B1, C1)... |

So A = 110, A1 = 111, B = 120, B1 = 121, C = 130, C1 = 131 (indices of the wiki page *Révisions*).
Decimal rather than hexadecimal so that the raw JSON (`{"rev":131}`) and a MicroPython `print()` read the same
as the board name. Decoding: `generation = rev / 100`, `release = (rev / 10) % 10`, `index = rev % 10`.

- Generation 1, release 1/2/3 -> V1A/V1B/V1C, origin `CONFIG`. `isPrototype()` is `index == 0`.
- Any other value (below 100, 1000 or more, generation other than 1, release 0 or above 3): E02
  (`unknown board revision 230 in the board config: update the STeaMi library`), then probing. The example
  value 3 written by older examples falls here, which is the wanted behaviour (no board carries a factory
  value yet).
- The table lives in one constexpr function `steamiDecodeRevision(int32_t, SteamiRevisionInfo&)` in
  `SteamiTypes.h`, unit-tested on every value above and the boundaries.
- Same code in MicroPython `steami_config` and on the wiki: sub-project 1 changes the Arduino `SteamiConfig`
  README and `show_config` example (`setBoardRevision(131)`, decoded print); separate PRs update the MicroPython
  `steami_config` README/examples and the wiki page *Révisions* (section *Identifier sa version*), so the
  factory tool and both languages share one table.
- Builder's call, optional: `SteamiBoard::flashSizeKb()` from the STM32WB55 flash size register (RM0434,
  verify the address), useful to tell prototypes with a different flash size without the config zone.

### 5.3 Per-function classes

Every class has the same shape (shown for temperature):

```cpp
class SteamiTemperature {
 public:
  explicit SteamiTemperature(SteamiBoard& board = Steami);   // stores the address only
  bool begin();                  // false + one message if absent or not answering; idempotent
  bool isAvailable() const;
  SteamiStatus status() const;
  const char* error() const;     // "" when OK, short enough for screen.text()
  const char* chip() const;      // "BME280", "WSEN-HIDS", ""
  float read();                  // degrees C; NAN when unavailable
  void setOffset(float celsius);
 private:
  SteamiBoard* _board;  SteamiChip _chip = SteamiChip::NONE;  SteamiStatus _status = SteamiStatus::NOT_STARTED;
};
```

Behaviour common to all: a read before `begin()` calls `begin()` itself. After a failure, reads return NAN,
-1, `false` or `NONE` without bus traffic and without printing again. `read()` is a switch on `_chip`
(two cases at most), not a virtual call: there is no interface layer for sensors.

Routing table (`SteamiBoard::route`, constexpr, candidates in order; UNKNOWN tries all candidates):

| Class | V1a / V1b | V1c | Public API (beyond the common shape) |
|---|---|---|---|
| `SteamiTemperature` | WSEN-HIDS, then WSEN-PADS | BME280 | `float read()` degC, `setOffset(float)` |
| `SteamiHumidity` | WSEN-HIDS | BME280 | `float read()` %RH |
| `SteamiPressure` | WSEN-PADS | BME280 | `float read()` hPa, `float altitude(float seaLevelHpa = 1013.25f)` (uses `SteamiBME280::altitudeFromPressure` on both) |
| `SteamiDistance` | VL53L1X | VL53L1X | `int read()` mm (-1 if none), `float cm()` |
| `SteamiAccelerometer` | ISM330DL | ISM330DL | `SteamiVector read()` g, `float x() y() z()`, `float strength()`, `bool isShaken(float g = 2.0f)` |
| `SteamiGyroscope` | ISM330DL | ISM330DL | `SteamiVector read()` deg/s, `float x() y() z()` |
| `SteamiMagnetometer` | LIS2MDL | LIS2MDL | `SteamiVector read()` uT, `float x() y() z()`, `float strength()`, `void calibrate(uint16_t seconds = 12)` |
| `SteamiCompass` | LIS2MDL | LIS2MDL | `float heading()` 0..360, `const char* direction()` ("N", "NE"...), `void calibrate(uint16_t seconds = 12)`, `void setDeclination(float deg)` |
| `SteamiLight` | APDS-9960 | absent | `uint16_t read()` raw ambient count |
| `SteamiColor` | APDS-9960 | absent | `bool read(uint16_t& r, uint16_t& g, uint16_t& b)`, `red() green() blue()`, `SteamiRgb rgb()` |
| `SteamiGesture` | APDS-9960 | absent | `enum Direction : uint8_t { NONE, LEFT, RIGHT, UP, DOWN, NEAR, FAR }`, `Direction read(uint32_t timeoutMs = 0)`, `static const char* name(Direction)` |
| `SteamiBattery` | BQ27441 | BQ27441 | `uint8_t percent()`, `float voltage()`, `int16_t current()` mA, `bool isCharging()` |
| `SteamiButtons` | GPIO + MCP23009E | GPIO + MCP23009E | `bool isPressed(SteamiButton)`, `bool wasPressed(SteamiButton)` (debounced edge), `bool wasReleased(SteamiButton)`, `SteamiButton waitForPress(uint32_t timeoutMs = 0)` |

```cpp
struct SteamiVector { float x, y, z; };
enum class SteamiButton : uint8_t { NONE, A, B, MENU, UP, DOWN, LEFT, RIGHT };  // A/B/MENU = MCU pins, D-PAD = MCP23009E
```

`SteamiColor` keeps the maintainer's name for the colour *sensor*; the drawing colour type is `SteamiRgb`.
`SteamiMagnetometer` (raw field) and `SteamiCompass` (heading) are two classes on the same LIS2MDL.

### 5.4 Sharing driver instances

- One `TwoWire` for the internal bus, owned by the board and exposed as `Steami.wire()`. Driver examples
  move from `TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL)` to `Steami.wire()`: two TwoWire objects on the same
  I2C peripheral fight over its interrupt.
- `claim(chip)` builds the slot (placement new with `wire()` and the board pins), calls the driver's
  `begin()` once, caches the result in a per-chip state (`NOT_TRIED / OK / NOT_RESPONDING`). Temperature,
  humidity and pressure on V1c therefore share one `SteamiBME280`; accelerometer and gyroscope share one
  `SteamiISM330DL`; light, colour and gesture share one `SteamiAPDS9960` (its slot enables the light and
  gesture engines together and documents the interference).
- Several objects of the same class are allowed (copies share the chip).
- `Steami.bme280()` etc. return the same instance for advanced code, documented as "shared with the
  SteamiXxx classes: changing oversampling also changes `SteamiPressure`".

### 5.5 Absent-function behaviour

`begin()` returns false, `status()` says why, and the board prints one line per (class, code):

```
[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on V1a/V1b only). gesture.read() will return NONE. (E20)
[STeaMi] SteamiHumidity: the WSEN-HIDS sensor (I2C 0x5F) did not answer. Check the board or try another one. (E10)
[STeaMi] SteamiDisplay: not enough RAM for the colour screen (115200 bytes in one block). Call screen.begin() first in setup(). (E31)
[STeaMi] SteamiDisplay: cannot tell if this board is a V1a/V1b or a V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)
```

Codes: E01 unknown board, E02 unknown config revision, E03 conflicting defines, E10 chip not answering,
E11 forced/config release contradicted by a missing chip, E20 not on this board, E30 display on unknown
board, E31 no memory. The codes index a French table on the wiki. `Steami.has(SteamiFunction::GESTURE)`
lets a sketch adapt before calling `begin()`.

Output goes through the logger pointer (default: `Serial.println`). Messages printed before USB/UART is ready
are kept in `lastError()` and replayed by `printInfo()`. The hardware tests check that writing to `Serial`
before `Serial.begin()` does not block on either core (STeaMi's `Serial` is the LPUART to the DAPLink VCP);
if it does, the default logger skips output while the UART is disabled (see open question 5).

### 5.6 Static override

Problem: the Arduino IDE compiles library `.cpp` files without the sketch's `#define`. Only code in the
sketch's own TU sees it. So the macro must create an object in the sketch TU that writes a value the library
reads at run time.

In `SteamiBoard.cpp` (constant-initialised, exists before any dynamic initialiser):

```cpp
namespace { SteamiRelease gForced = SteamiRelease::UNKNOWN; bool gForcedConflict = false; }
void steamiForceRelease(SteamiRelease r) {
  if (gForced != SteamiRelease::UNKNOWN && gForced != r) gForcedConflict = true;   // E03 in begin()
  gForced = r;
}
```

In `SteamiBoard.h`, the only place that reads the macros:

```cpp
#if (defined(STEAMI_BOARD_V1A) + defined(STEAMI_BOARD_V1B) + defined(STEAMI_BOARD_V1C)) > 1
#error "Define only one of STEAMI_BOARD_V1A, STEAMI_BOARD_V1B, STEAMI_BOARD_V1C."
#endif
#if defined(STEAMI_BOARD_V1C)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1C
#elif defined(STEAMI_BOARD_V1B)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1B
#elif defined(STEAMI_BOARD_V1A)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1A
#endif
#ifdef STEAMI_FORCED_RELEASE_
namespace {
struct SteamiForcedReleaseRegistrar { SteamiForcedReleaseRegistrar() { steamiForceRelease(STEAMI_FORCED_RELEASE_); } };
const SteamiForcedReleaseRegistrar steamiForcedReleaseRegistrar_;
}
#endif
```

- Sketch `#define` before `#include <STeaMi.h>`: one internal-linkage object in the sketch TU, run before
  `setup()`. Library TUs see no macro and contain nothing.
- PlatformIO `build_flags = -DSTEAMI_BOARD_V1C`, `arduino-cli --build-property compiler.cpp.extra_flags=...`,
  or a future board menu (`menu.steami_release.v1c.build.extra_flags`): every TU gets a registrar, all write the
  same value. No library change is needed when the menu arrives.
- No weak symbols, no external-linkage definitions, no macro-dependent inline code: no link error, no ODR issue,
  same result with `.o` linking (IDE) and `.a` archives (PlatformIO).
- Precedence: `Steami.begin(SteamiRelease)` > define/flag > config > probe. CI rule: no `STEAMI_BOARD_` in
  any `lib/*/src/*.cpp`.
- Pitfalls: a define after the include (or misspelt) is silently ignored; the banner then says "detected"
  instead of "forced". Forcing keeps the other release's drivers in flash but never builds their slots.

### 5.7 SteamiDisplay

Colour model. One value type, RGB565 inside, used on both panels:

```cpp
class SteamiRgb {
 public:
  constexpr SteamiRgb(uint8_t r, uint8_t g, uint8_t b);   // allows {255, 128, 0} at call sites
  static constexpr SteamiRgb gray(uint8_t level0to15);    // level * 17 per channel
  static constexpr SteamiRgb fromRgb565(uint16_t v);
  constexpr uint16_t rgb565() const;
  constexpr uint8_t gray4() const;   // expand 565 to 8 bits, ((r*77 + g*150 + b*29) >> 8) >> 4, as colors.py
 private:
  uint16_t _v;
};
constexpr SteamiRgb STEAMI_BLACK{0,0,0}, STEAMI_DARK{102,102,102}, STEAMI_GRAY{153,153,153},
  STEAMI_LIGHT{187,187,187}, STEAMI_WHITE{255,255,255}, STEAMI_GREEN{119,255,119}, STEAMI_RED{255,85,85},
  STEAMI_BLUE{85,85,255}, STEAMI_YELLOW{255,255,85}, STEAMI_GRID_DARK{51,51,51};
```

- No implicit conversion from an integer: `fill(15)` does not compile, so an old grey level cannot silently
  become dark blue on V1c. `SteamiRgb::gray(15)` is the explicit form.
- Greys land on the same SSD1327 levels as MicroPython (BLACK 0, GRID_DARK 3, DARK 6, GRAY 9, LIGHT 11,
  WHITE 15); accents degrade to grey (GREEN 12). The GC9A01 backend passes `rgb565()` through.
- Prefixed names avoid `WHITE`/`BLACK` macros of other libraries.

Coordinates. `int16_t` physical pixels, origin top-left, x right, y down. 128x128 on V1a/V1b, 240x240 on
V1c. `width() height() centerX() centerY() radius()` (min/2) let sketches stay portable. All clipping is
done by `SteamiDisplay` in `int16_t` before calling the backend: `SSD1327::pixel(uint8_t, uint8_t, ...)` has
no bounds check, so `pixel(-1, 5)` would wrap to x=255 and an off-screen `text()` row writes past the 8 KB
buffer. Lines, circles, triangles, arcs and text are drawn by `SteamiDisplay` through clipped `pixel` and
`fillRect`, never through the drivers' `line()`/`text()`.

Backend interface (the only virtual interface in the layer):

```cpp
class SteamiDisplayBackend {
 protected: ~SteamiDisplayBackend() = default;   // non-virtual: no deleting destructor needed
 public:
  virtual bool begin() = 0;  virtual const char* chip() const = 0;   // begin(): false on NO_MEMORY
  virtual int16_t width() const = 0;  virtual int16_t height() const = 0;  virtual bool isColor() const = 0;
  virtual void fill(uint16_t rgb565) = 0;  virtual void show() = 0;
  virtual void pixel(int16_t x, int16_t y, uint16_t rgb565) = 0;                         // already clipped
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t rgb565) = 0; // already clipped
  virtual void rotate(bool upsideDown) = 0;  virtual void invert(bool on) = 0;
  virtual void powerOn() = 0;  virtual void powerOff() = 0;
};
class SteamiSsd1327Backend : public SteamiDisplayBackend { SteamiSSD1327 _oled; ... };  // converts to gray4
class SteamiGc9a01Backend  : public SteamiDisplayBackend { SteamiGC9A01 _tft;  ... };  // pass-through
```

Buffer ownership. Drivers own their frame buffers; `SteamiDisplay` and `SteamiScreen` own none.
- The board holds two `SteamiLazy` slots, one per backend, and builds only the one routed for the release, in
  the first `SteamiDisplay::begin()`. `SteamiSSD1327`'s constructor (`new uint8_t[8192]`) therefore runs only
  on V1a/V1b, and `SteamiGC9A01::begin()` (115 200 B `malloc`) only on V1c.
- A sketch without a screen never builds either slot.
- Every `SteamiDisplay`/`SteamiScreen` object shares the board's backend and draws into the same buffer.
- If the GC9A01 allocation fails: `begin()` false, status `NO_MEMORY`, message E31. Examples and docs start
  the screen first in `setup()`.
- On UNKNOWN release the display does not guess (panels are write-only SPI): E30.

```cpp
class SteamiDisplay {
 public:
  explicit SteamiDisplay(SteamiBoard& board = Steami);
  bool begin();  bool isAvailable() const;  const char* error() const;  bool isColor();
  int16_t width(); int16_t height(); int16_t centerX(); int16_t centerY(); int16_t radius();
  void clear(SteamiRgb c = STEAMI_BLACK);
  void pixel(int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE);
  void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c = STEAMI_WHITE);
  void hline(int16_t x, int16_t y, int16_t w, SteamiRgb c = STEAMI_WHITE);
  void vline(int16_t x, int16_t y, int16_t h, SteamiRgb c = STEAMI_WHITE);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, SteamiRgb c = STEAMI_WHITE, bool fill = false);
  void circle(int16_t cx, int16_t cy, int16_t r, SteamiRgb c = STEAMI_WHITE, bool fill = false);
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                SteamiRgb c = STEAMI_WHITE, bool fill = false);
  void arc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t sweepDeg, SteamiRgb c, uint8_t width = 3);
  void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);
  int16_t textWidth(const char* s, uint8_t scale = 1) const;
  void show();
  void rotate(bool upsideDown);  void invert(bool on);  void powerOn();  void powerOff();
  SteamiDisplayBackend* backend();
  static constexpr uint8_t CHAR_W = 8, CHAR_H = 8;
};
```

Text. `SteamiDisplay` renders its own font: a port of MicroPython's 8x8 framebuf font (MIT, about 760 B of
flash) plus a degree glyph. Same font as MicroPython means the `steami_screen` layout constants port
verbatim. Integer scale gives a real pixel zoom. Strings are UTF-8 aware: `°` maps to the degree glyph and
`é è ê à ç ù ô` fold to their base letters, so "Température" and "°C" typed by French students render.

### 5.8 SteamiScreen

`class SteamiScreen : public SteamiDisplay`. One object gives widgets and primitives, as the MicroPython
`Screen` does. Immediate mode: `clear()`, draw in call order, `show()`; later widgets paint over earlier ones
(gauge before title).

```cpp
enum class SteamiAt   : uint8_t { N, NE, E, SE, S, SW, W, NW, CENTER };   // same names as MicroPython strings
enum class SteamiFace : uint8_t { HAPPY, SAD, SURPRISED, SLEEPING, ANGRY, LOVE };
template <size_t N> class SteamiHistory {      // ring buffer, no heap
 public: void add(float v); size_t size() const; float operator[](size_t i) const; float last() const; void clear();
};

class SteamiScreen : public SteamiDisplay {
 public:
  explicit SteamiScreen(SteamiBoard& board = Steami);
  uint8_t maxChars();
  // Level 1: widgets (port of steami_screen/device.py, same argument order)
  void title(const char* text, SteamiRgb c = STEAMI_GRAY);
  void subtitle(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, SteamiRgb c = STEAMI_DARK);
  void value(float v,  const char* unit = nullptr, SteamiAt at = SteamiAt::CENTER, const char* label = nullptr,
             SteamiRgb c = STEAMI_WHITE, uint8_t scale = 2, int16_t yOffset = 0);
  void value(double v, ...same...);  void value(int v, ...);  void value(long v, ...);
  void value(unsigned long v, ...);  void value(const char* text, ...);
  void setDecimals(uint8_t d);       // decimals for float/double values, default 1
  void bar(float v, float maxV = 100, int16_t yOffset = 0, SteamiRgb c = STEAMI_LIGHT);
  void gauge(float v, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT, uint8_t arcWidth = 0);
  template <size_t N> void graph(const SteamiHistory<N>& h, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT);
  void graph(const float* data, size_t n, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT);
  template <size_t N> void menu(const char* const (&items)[N], size_t selected = 0, SteamiRgb c = STEAMI_WHITE);
  void menu(const char* const* items, size_t n, size_t selected = 0, SteamiRgb c = STEAMI_WHITE);
  void compass(float headingDeg, SteamiRgb c = STEAMI_LIGHT);
  void watch(uint8_t h, uint8_t m, uint8_t s = 0, SteamiRgb c = STEAMI_LIGHT);
  void face(SteamiFace f, bool compact = false, SteamiRgb c = STEAMI_LIGHT);
  void face(const uint8_t bitmap[8], bool compact = false, SteamiRgb c = STEAMI_LIGHT);
  // Level 2: text at a cardinal position (text at x,y, line, circle, rect, pixel, clear, show are inherited)
  void text(const char* s, SteamiAt at = SteamiAt::CENTER, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);
  using SteamiDisplay::text;
};
```

Layout. `SteamiScreenConst.h` ports `const.py` (`STEAMI_GAUGE_START_ANGLE 135`, `STEAMI_GAUGE_SWEEP 270`,
`STEAMI_GRAPH_MARGIN 15`, `X_OFFSET 6`, `Y 38`, `HEIGHT 52`, `VALUE_Y 31`, `DASH 3`, `GAP 3`) and the face
bitmaps. `_resolve(at, len, scale)` and `_safeMargin(tw, fromEdge)` port the round-screen formula
(`r - sqrt(r^2 - (tw/2)^2) + 2`, floor `ch*2+4`). `value()` keeps the W/E special cases (quarter widths).
Radius-based widgets (gauge, compass, watch, face) adapt to 240 px by themselves; graph and menu use the
128-px constants as MicroPython does, scaled on the 240 px panel as decided in section 9 (text size).

Formatting uses `snprintf`/`dtostrf` into stack buffers; there is no `String` and no heap in widgets.
Overload sets are tested with `0`, `23`, `23.5`, `23.5f`, `millis()` and `analogRead()` so that no student call
is ambiguous.

## 6. Student sketch examples

Smallest sketch:

```cpp
#include <STeaMi.h>

SteamiTemperature temperature;
SteamiScreen screen;

void setup() {
  screen.begin();          // first: the V1c colour screen needs 115 KB of RAM in one block
  temperature.begin();
}

void loop() {
  screen.clear();
  screen.title("Temperature");
  screen.value(temperature.read(), "°C");
  screen.show();
  delay(500);
}
```

Weather station, the same file on V1a, V1b and V1c:

```cpp
// Optional: to skip detection, uncomment ONE line. It must come BEFORE the #include.
// #define STEAMI_BOARD_V1C
#include <STeaMi.h>

SteamiScreen screen;
SteamiTemperature temperature;   // BME280 on V1c, WSEN-HIDS on V1a/V1b
SteamiHumidity humidity;         // same chip as temperature on every release
SteamiPressure pressure;
SteamiCompass compass;
SteamiGesture gesture;           // V1a/V1b only
SteamiButtons buttons;           // A, B, MENU and the D-PAD

SteamiHistory<40> history;       // last 40 pressures, no heap
const char* const PAGES[] = {"Temperature", "Humidity", "Pressure", "Compass"};
uint8_t page = 0;
bool hasGesture = false;

void setup() {
  Serial.begin(115200);
  screen.begin();
  temperature.begin();
  humidity.begin();
  pressure.begin();
  compass.begin();
  buttons.begin();
  hasGesture = gesture.begin();  // on V1c: prints E20 and returns false; the sketch goes on
  Steami.printInfo();            // "STeaMi V1c (read from the board config, revision 131)"
}

void loop() {
  if (buttons.wasPressed(SteamiButton::RIGHT) || buttons.wasPressed(SteamiButton::A)) page = (page + 1) % 4;
  if (buttons.wasPressed(SteamiButton::LEFT)  || buttons.wasPressed(SteamiButton::B)) page = (page + 3) % 4;
  if (hasGesture && gesture.read() == SteamiGesture::RIGHT) page = (page + 1) % 4;

  history.add(pressure.read());

  screen.clear();
  if (page == 0) {
    screen.title(PAGES[page]);
    screen.value(temperature.read(), "°C");
  } else if (page == 1) {
    float h = humidity.read();
    screen.gauge(h, 0, 100, STEAMI_BLUE);    // gauge first, text on top
    screen.title(PAGES[page]);
    screen.value(h, "%");
    if (h > 70) screen.face(SteamiFace::SAD, true);
  } else if (page == 2) {
    screen.title(PAGES[page]);
    screen.graph(history, 950, 1050, STEAMI_RED);
    screen.subtitle("hPa");
  } else {
    screen.compass(compass.heading());
    screen.text(compass.direction(), SteamiAt::S);
  }
  screen.show();
  delay(200);
}
```

Advanced use: `SteamiLIS2MDL* mag = Steami.lis2mdl();` (shared with `SteamiCompass`, already begun, nullptr
if absent) or `SteamiVL53L1X tof(Steami.wire());` (own instance on the board's single TwoWire).

## 7. Testing strategy

Mock additions (`tests/native/helpers`): `TwoWire(uint32_t sda, uint32_t scl)` constructor; the STeaMi pin
names (`I2C_INT_SDA/SCL`, `A_BUTTON`, `B_BUTTON`, `MENU_BUTTON`, `RST_EXPANDER`, `INT_EXPANDER`, display pins);
per-address NACK `setAbsent(addr)` in the Wire mock; a capturing logger installed with `setLogger`; reuse of
`FakeDaplinkBridge` from #239 (config present, legacy 9513ae0, `nackAfterReadSector`). Each test builds its own
`SteamiBoard board(Wire, pins)` and calls `board.end()` in `tearDown`.

Native suites:
1. `test_steami_board`: **legacy firmware -> no config command ever reaches the fake, the fake is not hung,
   result from probe**; config `{"rev":131}` -> V1C/CONFIG with no probe traffic, `{"rev":130}` -> V1C prototype, `{"rev":3}` and `{"rev":230}` -> E02 then probe; bridge absent, probe Unknown,
   revision missing or unknown (E02) -> probe; BME280 / HIDS / PADS-only / APDS-only / nothing -> V1C / V1AB /
   V1AB / V1AB / UNKNOWN + E01; second `begin()` and `begin(SteamiRelease::V1C)` make no I2C transaction; the
   constructor touches neither Wire nor pins; on V1AB, using every function leaves the BME280 and GC9A01 slots
   unbuilt; `sizeof(SteamiBoard)` under 2.5 KB.
2. `test_steami_forced` (separate binary because the registrar runs at static init): `test_main.cpp` starts
   with `#define STEAMI_BOARD_V1C`; asserts V1C/FORCED_DEFINE and no bus traffic. A second `.cpp` in the same
   binary includes the header with the same define (proves no duplicate symbol); a variant with `V1B` in the
   second TU asserts E03. A compile-fail script checks the `#error` for two macros in one TU.
3. `test_steami_functions`, per release over the existing driver register maps (`driver_checks.h`):
   routing; temperature + humidity + pressure on V1c begin the BME280 once (count chip-ID reads);
   accelerometer + gyroscope share one ISM330DL; gesture/light/colour on V1c return false with E20 and never
   address 0x39; reads before `begin()` auto-begin; failed functions print once in ten calls; buttons edge and
   debounce with the MCP23009E GPIO register and a mocked `millis()`.
4. `test_steami_display`: `SteamiRgb` table (gray4 and rgb565 of every const.py colour, golden values from a
   10-line Python script); no implicit int conversion (compile-fail check); clipping of `pixel(-1,0)`,
   `pixel(200,5)`, text at y=125, line from (-50,-50) to (300,300) on `SteamiSsd1327Backend` over the SPI mock,
   with canary bytes around the 8 KB buffer and `-fsanitize=address,undefined` in env:native; a test-only
   `SteamiMemoryBackend` (RGB565 array, 128 and 240) for shape symmetry, scale-2 glyph size, UTF-8 degree;
   no SSD1327 constructed when the release is V1C.
5. `test_steami_screen` on `SteamiMemoryBackend` at 128 and 240: `_resolve()` golden positions for every
   `SteamiAt` computed from device.py with the same 8x8 font; every widget stays inside the circle of radius
   r+1; title in the top band, subtitle in the bottom band; gauge(50) lights about half the arc; graph with
   0, 1 and N points; menu window scrolling; face bitmaps equal const.py; `value()` overload smoke test.
   PPM dumps are uploaded as a CI artifact for review against MicroPython SimBackend screenshots.

Build and package CI: compile every example with arduino-cli (core 3.1.0) and PlatformIO (core 2.12), plus
`ForcedV1c` (define in the .ino), a build-flag variant (`--build-property compiler.cpp.extra_flags=-DSTEAMI_BOARD_V1C`
and PlatformIO `build_flags`) and a sketch with the define *after* the include (must build). Footprint gate
with `arm-none-eabi-size`: flash under the 512 KB cap with margin, WeatherStation `.data + .bss` leaving at
least 140 KB for heap and stack. Rule checks: no `STEAMI_BOARD_` in `lib/*/src/*.cpp`; a case-insensitive
duplicate-name check in `build-arduino-package.py` and a test that `STeaMi.h` includes the new headers.

Hardware (`tests/hardware/test_steami_board`, PlatformIO env:steami), never rewriting the factory
`board_revision`:
- V1b with old DAPLink 9513ae0: release from probe, board responsive over several resets (first real run of
  the #239 legacy path); V1b with new DAPLink and no revision; V1c with factory revision; V1c with the config
  erased and restored through `tests/shared/config_zone_backup.h`.
- WeatherStation flashed unchanged on V1a/V1b/V1c (`scripts/qualify.py`); transcript shows the banner and only
  E20 on V1c. On V1c, `screen.begin()` succeeds with STM32duinoBLE initialised. Messages printed before
  `Serial.begin()` do not block on either core.

## 8. Sub-projects, in the agreed order

Each is one PR (two if large), with tests and an example, branched from `main`.

1. **SteamiBoard** (all prerequisites are on `main` since v0.30.0). Delivers `steamiDecodeRevision()` (5.2.1), `SteamiLazy.h`, `SteamiTypes.h`, `SteamiBoard.h/.cpp` (detection, registrar,
   `begin(SteamiRelease)`, `wire()`, chip slots, accessors, `claim()`, routing and availability tables, logger,
   coded messages, `printInfo()`), mock additions, `test_steami_board`, `test_steami_forced`, `BoardInfo`, the
   CI rule check, the French error table on the wiki. Done when the detection matrix passes natively and on
   the three hardware configurations.
2. **Per-function classes.** The 13 classes of 5.3 (may split: environment/distance/battery, then
   motion/APDS/buttons), `test_steami_functions`, one example per function, keywords.txt, driver examples moved
   to `Steami.wire()`. Done when one sketch using every class runs on V1b and V1c with only E20 on V1c.
3. **SteamiDisplay.** `SteamiRgb.h`, `SteamiFont8x8`, `SteamiDisplayBackend`, both backends and their board
   slots, primitives with clipping and UTF-8 text, `test_steami_display` with ASan, the footprint gate. Done
   when one drawing example looks the same on both panels.
4. **SteamiScreen.** `SteamiScreen`, `SteamiScreenConst.h`, `SteamiHistory`, faces, `SteamiAt`,
   `test_steami_screen` with PPM artifacts, `WeatherStation`, `ScreenWidgets`, wiki page. Done when every
   device.py widget has a C++ counterpart reviewed against the MicroPython screenshots.

## 9. Maintainer decisions (2026-11-25)

- **Calibrations**: applied automatically. A function class whose chip has calibration data in the loaded
  config (temperature gain/offset `tc`, magnetometer `cm`, accelerometer `ca`, with the same keys as MicroPython
  `steami_config`) applies it in `begin()`; without a config zone nothing changes. `printInfo()` lists the
  calibrations applied.
- **Board revision**: decimal code of 5.2.1.
- **Axes**: the ISM330DL and LIS2MDL orientation is the same on V1a, V1b and V1c: no per-release axis remap.
- **Units and temperature source**: follow MicroPython where it costs little (units, WSEN-HIDS as the
  temperature source on V1a/V1b); not a blocking requirement.
- **Text size** (maintainer unsure, designer's call): `SteamiDisplay::text()` keeps its explicit `scale`
  (default 1, pixel-exact on both panels). `SteamiScreen` adapts to the panel so that a widget sketch looks
  alike on V1b and V1c: positions and sizes written for 128 px are multiplied by `width() / 128.0` (1 on the
  OLED, 1.875 on the TFT), and text uses an integer glyph scale of 1 on 128 px and 2 on 240 px.
- **Serial before `Serial.begin()`** (designer's call): the default logger never calls `Serial.begin()`
  itself. If the hardware check shows that writing before `begin()` blocks, it skips output until `Serial` is
  started and the message stays in `lastError()`; `printInfo()` replays it.
- **BME280 only on V1c** (maintainer, 2026-11-25): no board before release C carries a BME280, whatever the
  A bill of materials says. The probe rule "BME280 (chip ID 0x60) at 0x76 -> V1C" stands as written in 5.2.
  A BME680 (chip ID 0x61, mentioned on the C schematic) is not supported: it does not match the probe, so the
  board falls back to the next probe rules and the environment functions report "not responding"
  (designer's call; no C1 board with a BME680 is known).
- **BME280 temperature calibration key** (designer's call): new `tc` key `bme`, added to the Arduino
  `SteamiConfig` constants and, in a coordinated PR, to MicroPython `steami_config` `_SENSOR_KEYS`. Applied
  only when present; how the factory measures it (no WSEN-HIDS reference on V1c) is outside this layer.
- **Hardware case "V1c with factory revision"** (designer's call): until a factory-programmed V1c exists, the
  hardware test writes `{"rev":131}` only on a board whose config has no `rev`, after saving the zone with
  `tests/shared/config_zone_backup.h`, and restores it at the end. A board that already carries a `rev` is
  read, never written.

Former open questions, kept for the record:

1. **board_revision encoding.** The routing table assumes `1 -> V1a, 2 -> V1b, 3 -> V1c` (MicroPython's
   example writes 3). What does the factory tool write, and is the same table used by MicroPython
   `steami_config`? Unknown values fall back to probing either way.
2. **Text size on the 240 px screen.** With the MicroPython 8x8 font at scale 1, titles and menus are small
   on V1c. Follow MicroPython exactly (scale 1, course screenshots match), or scale text and the graph/menu
   constants by 2 on 240 px (sketches look alike on V1b and V1c)? Proposed: follow MicroPython unless its V1c
   backend already scales.
3. **Temperature source on V1a/V1b.** WSEN-HIDS (proposed) or WSEN-PADS? Pick the one MicroPython courses use,
   so readings match between languages.
4. **Units.** Accelerometer in g, gyroscope in deg/s, magnetometer in uT, distance in mm, as proposed, or the
   units of the MicroPython drivers if they differ?
5. **Serial before `Serial.begin()`.** If the hardware check shows that writing to the LPUART before
   `begin()` blocks, may the default logger call `Serial.begin(115200)` itself (a sketch that wanted 9600 then
   gets 115200), or should it stay silent and only keep `lastError()`?
