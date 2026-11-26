# STeaMi Board

Arduino/C++ board layer for the STeaMi: it detects which release you hold
(**V1a**, **V1b** or **V1c**), builds each sensor driver once for everybody,
and explains problems with short coded messages.

## Hardware

* No chip of its own: it reads the board revision from the DAPLink config
  zone (`0x3B`) and probes the sensors on the **internal** I2C bus.
* Chips probed by ID: BME280 (`0x76`, V1c), WSEN-HIDS (`0x5F`),
  WSEN-PADS (`0x5D`) and APDS-9960 (`0x39`) (V1a/V1b).
* The global object is called `Steami`. It touches no hardware until
  `begin()` (or the first accessor) is called.

## Quick start

With the `STeaMi` Library Manager package, `#include <SteamiBoard.h>` (or
`#include <STeaMi.h>`) is enough:

```cpp
#include <SteamiBoard.h>

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}

    Steami.begin();
    Steami.printInfo();
}

void loop() {}
```

It prints the release, where the answer comes from, and the I2C chips of that
release, each one `OK`, `not responding` or `not tried`. "Not tried" means that
no function used that chip yet. The [`BoardInfo`](examples/BoardInfo/) example
claims every candidate chip of every function first
(`Steami.claim(steamiRoute(release, function, i))` until it returns `NONE`), so
each line shows whether the chip really answers, even on an unknown release.

## Detection order

`begin()` detects the release once; later calls make no I2C traffic.

1. **Forced**: the runtime call `Steami.begin(SteamiRelease::V1C)` wins over
   `#define STEAMI_BOARD_V1C` (before the `#include`, or as a build flag).
2. **Board revision** stored in the DAPLink config zone, only when the
   firmware has a config zone (see the `steami_config` library).
3. **I2C probing** by chip ID: BME280 (`0x76`) means V1c; WSEN-HIDS (`0x5F`),
   WSEN-PADS (`0x5D`) or APDS-9960 (`0x39`) means V1a/V1b.

`origin()` tells which source gave the answer (`FORCED_RUNTIME`,
`FORCED_DEFINE`, `CONFIG` or `PROBE`, `NONE` when the release is unknown).

## Revision code

The revision stored in the config zone is a decimal code: the hundreds are
the generation, the tens the release (1 = a, 2 = b, 3 = c) and the units the
schematic index (0 = prototype).

| Code | Board |
|------|-------|
| 110  | V1a prototype |
| 111  | V1a, index 1 (A1) |
| 120  | V1b prototype |
| 121  | V1b, index 1 (B1) |
| 130  | V1c prototype |
| 131  | V1c, index 1 (C1) |

`steamiDecodeRevision()` decodes a code (valid codes are 100 to 999 with
generation 1 and release 1 to 3). A missing revision is silent: the sensors
are probed instead. An invalid one (for example the old example value `3`)
gives `E02`, then the sensors are probed.

## Forcing a release

Without a config zone and without the sensors answering, the release stays
`UNKNOWN` (message `E01`). Force it:

```cpp
#define STEAMI_BOARD_V1C   // or STEAMI_BOARD_V1B, STEAMI_BOARD_V1A
#include <SteamiBoard.h>
```

or at run time, which has the highest precedence:

```cpp
Steami.begin(SteamiRelease::V1C);
```

A forced release makes no I2C traffic.

## Messages

Messages look like `[STeaMi] <who>: <detail> (Exx)`. Each one is logged once
per (class, code); `lastError()` keeps the last one. A message too long for
`lastError()` loses the end of its detail, never its code. `printInfo()` shows
the last message again only when the logger did not deliver it (no logger, or
`Serial` not started yet).

| Code | Text |
|------|------|
| `E01` | `cannot tell which STeaMi this is: no V1a/V1b or V1c sensor answered. Add #define STEAMI_BOARD_V1C (or V1B, V1A) before #include <STeaMi.h>.` |
| `E02` | `unknown board revision <n> in the board config. Update the STeaMi library; the board is detected from its sensors instead.` |
| `E03` | `two different STEAMI_BOARD_ defines in this sketch. Keep only one, before #include <STeaMi.h>.` |

The default logger prints on `Serial` only once `Serial` is started
(`if (Serial)`). `Steami.setLogger(nullptr)` silences the library, and
`Steami.setLogger(myFunction)` sends the lines elsewhere.

## API

| Method | Description |
|--------|-------------|
| `bool begin()` | Detect the release once. `false` when it stays `UNKNOWN`. |
| `bool begin(SteamiRelease forced)` | Force the release (highest precedence, no I2C traffic). `UNKNOWN` is the same as `begin()`. |
| `SteamiRelease release()` | The release (`UNKNOWN`, `V1A`, `V1B`, `V1AB`, `V1C`); detects it on the first call. |
| `const char* releaseName()` | `"V1a"`, `"V1b"`, `"V1a/V1b"`, `"V1c"` or `"unknown"`. |
| `bool isV1c()` / `bool isV1ab()` | Release tests. `isV1ab()` is true for V1a, V1b and V1a/V1b. |
| `SteamiOrigin origin()` | Where the release comes from. |
| `int32_t revision()` | Revision code read from the config zone, `-1` when none. |
| `bool isPrototype()` | Valid revision with schematic index 0. |
| `uint8_t schematicIndex()` | Units digit of a valid revision, `0xFF` when there is none. |
| `bool has(SteamiFunction f)` | True when a chip on this release serves the function (`TEMPERATURE`, `HUMIDITY`, `PRESSURE`, `DISTANCE`, `ACCELEROMETER`, `GYROSCOPE`, `MAGNETOMETER`, `COMPASS`, `LIGHT`, `COLOR`, `GESTURE`, `BATTERY`, `BUTTONS`, `SCREEN`). |
| `SteamiConfig* config()` | The board config, non-null only after it was loaded during detection. |
| `SteamiStatus claim(SteamiChip c)` | Build the driver of a chip on the first call and begin it once. `OK` or `NOT_RESPONDING` for the nine I2C chips, `NOT_STARTED` for the displays: they go through `beginDisplay()`. |
| `bool built(SteamiChip c)` | True when the driver of the chip is constructed. |
| `bme280()`, `wsenHids()`, `wsenPads()`, `apds9960()`, `ism330dl()`, `lis2mdl()`, `vl53l1x()`, `bq27441()`, `mcp23009e()` | The shared driver of a chip, `nullptr` unless `claim()` gives `OK`. |
| `void printInfo()` | Through the logger: the banner (unless the logger already delivered it), the I2C chips of the release, the screen line `<chip> screen <state>`, then the last message (unless the logger already delivered it). |
| `void printInfo(Print& out)` | A full report to `out`: always the banner, the I2C chips of the release, the screen line `<chip> screen <state>`, then the last message unless the logger already delivered it. |
| `void setLogger(void (*log)(const char*))` | Where messages go; `nullptr` keeps the library silent. |
| `const char* lastError()` | The last message reported, `""` when none. |
| `void report(const char* who, SteamiStatus s, const char* detail, uint8_t code)` | Store and log a coded message (once per pair). `who` must be a literal. |
| `TwoWire& wire()` | The internal I2C bus; begins it the first time. |
| `const SteamiBoardPins& pins()` | The pins the board layer uses. |
| `void end()` | Destroy the shared drivers, free the screen buffer and forget reported messages. The logger is kept. |
| `SteamiDisplayBackend* display()` | The backend of the screen of the release, built but not begun; `nullptr` on an unknown board. |
| `void setDisplayBackend(SteamiDisplayBackend& b)` | Use `b` as the screen (tests, other panels). Ignored once `beginDisplay()` ran, until `end()`. |
| `SteamiStatus beginDisplay()` | Begin the screen once and keep the result: `OK`, `UNKNOWN_BOARD` or `NO_MEMORY`. |

Free functions: `steamiDecodeRevision(int32_t)`, `steamiRoute(release,
function, candidate)`, `steamiChipName(chip)`, `steamiChipAddress(chip)`,
`steamiDisplayStateText(status)` (`"OK"`, `"not started"`, `"unknown board"`,
`"no memory"`) and `steamiForceRelease(release)` (used by the `#define`
mechanism, do not call it).

## Function classes

Thirteen classes read a function without you choosing a chip:
`SteamiTemperature`, `SteamiHumidity`, `SteamiPressure`, `SteamiDistance`,
`SteamiBattery`, `SteamiAccelerometer`, `SteamiGyroscope`,
`SteamiMagnetometer`, `SteamiCompass`, `SteamiLight`, `SteamiColor`,
`SteamiGesture` and `SteamiButtons`. They pick the chip of the release
(V1a/V1b or V1c) and share the driver the board already built. Each has a
ready-to-run example in [`examples/`](examples/).

```cpp
#include <SteamiTemperature.h>

SteamiTemperature temperature;

void setup() {
    Serial.begin(115200);
    temperature.begin();
}

void loop() {
    Serial.println(temperature.read());
    delay(1000);
}
```

| Class | V1a / V1b | V1c |
|-------|-----------|-----|
| `SteamiTemperature` | WSEN-HIDS, then WSEN-PADS | BME280 |
| `SteamiHumidity` | WSEN-HIDS | BME280 |
| `SteamiPressure` | WSEN-PADS | BME280 |
| `SteamiDistance` | VL53L1X | VL53L1X |
| `SteamiBattery` | BQ27441 | BQ27441 |
| `SteamiAccelerometer` | ISM330DL | ISM330DL |
| `SteamiGyroscope` | ISM330DL | ISM330DL |
| `SteamiMagnetometer` | LIS2MDL | LIS2MDL |
| `SteamiCompass` | LIS2MDL | LIS2MDL |
| `SteamiLight` | APDS-9960 | none (`E20`) |
| `SteamiColor` | APDS-9960 | none (`E20`) |
| `SteamiGesture` | APDS-9960 | none (`E20`) |
| `SteamiButtons` | MCP23009E (D-PAD) and MCU pins (A, B, MENU) | same |

When the release is unknown, every candidate is tried (for the temperature:
BME280, WSEN-HIDS, then WSEN-PADS). The altitude of `SteamiPressure` comes from
`SteamiBME280::altitudeFromPressure` on every release.

### Common shape

Every class has the same methods:

| Method | Description |
|--------|-------------|
| `explicit SteamiXxx(SteamiBoard& board = Steami)` | Stores the address of the board only; touches no hardware. |
| `bool begin()` | Claims the chip; `false` with one message when it does not answer. Idempotent. |
| `bool isAvailable() const` | True once `begin()` succeeded. |
| `SteamiStatus status() const` | `OK`, `NOT_STARTED`, `NOT_RESPONDING`, `NOT_ON_THIS_BOARD`, `NO_MEMORY` or `UNKNOWN_BOARD`. |
| `const char* error() const` | A short text for the status (`"no answer"`, `"not on this board"`, ...), `""` when `OK`. |
| `const char* chip() const` | The chip in use (`"BME280"`, `"WSEN-HIDS"`, ...), `""` when none. |

A read before `begin()` calls `begin()`. After a failure, the reads return
their "unavailable" value without any bus traffic and without printing again.

| Class | Reads | Value when unavailable |
|-------|-------|------------------------|
| `SteamiTemperature` | `read()` in degrees Celsius, `setOffset(c)` | `NAN` |
| `SteamiHumidity` | `read()` in %RH | `NAN` |
| `SteamiPressure` | `read()` in hPa, `altitude(seaLevelHpa = 1013.25f)` in metres | `NAN` |
| `SteamiDistance` | `read()` in mm, `cm()` | `-1` (`NAN` for `cm()`) |
| `SteamiBattery` | `percent()`, `voltage()` in V, `current()` in mA, `isCharging()` | `0`, `NAN`, `0`, `false` |
| `SteamiAccelerometer` | `read()` as a `SteamiVector` in g, `x()`, `y()`, `z()`, `strength()`, `isShaken(g = 2.0f)` | `NAN` (all three axes and `strength()`), `false` |
| `SteamiGyroscope` | `read()` as a `SteamiVector` in degrees per second, `x()`, `y()`, `z()` | `NAN` |
| `SteamiMagnetometer` | `read()` as a `SteamiVector` in uT, `x()`, `y()`, `z()`, `strength()`, `calibrate(seconds = 12)` | `NAN` |
| `SteamiCompass` | `heading()` in degrees, `direction()`, `setDeclination(deg)`, `calibrate(seconds = 12)` | `NAN`, `""` |
| `SteamiLight` | `read()`, the raw clear count of the sensor | `0` |
| `SteamiColor` | `read(r, g, b)` (raw counts, returns whether it worked), `red()`, `green()`, `blue()`, `rgb()` (hue of the object, largest channel 255) | `false`, `0`, `STEAMI_BLACK` |
| `SteamiGesture` | `read(timeoutMs = 0)` gives `NONE`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `NEAR` or `FAR`; `SteamiGesture::name(d)` | `NONE` |
| `SteamiButtons` | `isPressed(b)`, `wasPressed(b)`, `wasReleased(b)`, `waitForPress(timeoutMs = 0)` with `SteamiButton::A`, `B`, `MENU`, `UP`, `DOWN`, `LEFT`, `RIGHT` | `false`, `NONE` |

* `SteamiDistance`: `-1` means that the sensor timed out or is unavailable.
  The driver ignores the range status, so a target out of range is not told
  apart, and a true 0 mm reading reads as `-1` too.
* `SteamiBattery`: `isCharging()` is `current() > 0`.
* `SteamiAccelerometer`: `begin()` sets the shared ISM330DL to +-8 g, so a
  sketch that reads the chip itself through `Steami.ism330dl()` sees that
  range once the class began. `strength()` is the norm of the three axes.
* `SteamiGyroscope` and `SteamiAccelerometer` share the ISM330DL;
  `SteamiMagnetometer` and `SteamiCompass` share the LIS2MDL. Each shared
  driver is built once.
* `SteamiMagnetometer`: the values are in uT. The hard-iron offset is removed
  only after a calibration (the `cm` key in the board config, or
  `calibrate()`); before that the values include the board's own field and can
  be much larger than the Earth's 25 to 65 uT. Hard iron only: the `sx`, `sy`
  and `sz` scales are not applied to these values. `SteamiCompass::heading()`
  uses the `sx`, `sy` and `sz` scales as well as the offsets.
  `calibrate(seconds)` is a min/max calibration done in RAM: turn the board in
  every direction meanwhile. It is lost on reset; the `cm` key of the board
  config (see Calibration) is applied automatically. `calibrate(0)` does
  nothing.
* `SteamiCompass`: see Heading convention below. `calibrate()` is the same as
  the magnetometer one, `setDeclination(deg)` adds the magnetic declination
  and may be called before `begin()`.
* `SteamiLight` and `SteamiColor` give raw counts of the APDS-9960, not lux.
  `SteamiColor::rgb()` scales them so that the largest channel is 255: it gives
  the hue of the object, not its brightness (see Colours).
* `SteamiLight`, `SteamiColor` and `SteamiGesture` interfere: `SteamiGesture`
  `begin()` turns the gesture engine on, and the chip serves the gesture
  first. While a hand is over the sensor, the light and colour readings pause
  and resume afterwards.
* `SteamiButtons`: A, B and MENU are read on MCU pins and always work. The four
  D-PAD arrows are on the MCP23009E: when it does not answer, `begin()` returns
  `false` with one message but A, B and MENU still work and the arrows read as
  released. Every call samples the buttons once: a press is seen as soon as
  one call happens while the button is down, and the bounces of the next
  20 ms are ignored. Call the buttons at least every 50 ms (a quick tap lasts
  about 100 ms); a tap shorter than the time between two calls can be missed
  (see [issue #251](https://github.com/steamicc/arduino-steami-lib/issues/251)).
  A button held at start is not a press. `waitForPress()` drops
  the presses latched before the call and returns `NONE` after the timeout.
* A function that failed is not tried again until the board resets (or
  `Steami.end()`). After plugging in a battery, reset the board.

### Heading convention

`SteamiCompass::heading()` is in degrees clockwise from north, in [0, 360),
with the board held flat and the screen up. 0 means that the top of the screen
(the side opposite the USB connector) points north, 90 east, 180 south and
270 west. It is `atan2(-x, y)` of the corrected X and Y fields, measured on a
V1c: the convention of the `SteamiLIS2MDL` driver (`headingFlatOnly()`).
Without tilt compensation, so keep the board flat. With the screen up, the
accelerometer reads `z` about -1 g.

`direction()` returns the nearest of eight sectors, each 45 degrees wide and
centred on its name: `"N"`, `"NE"`, `"E"`, `"SE"`, `"S"`, `"SW"`, `"W"` or
`"NW"`.

### Messages of the function classes

| Code | Text |
|------|------|
| `E10` | `[STeaMi] SteamiTemperature: the BME280 temperature sensor (I2C 0x76) did not answer. Check the board or try another one. (E10)` |
| `E10` | `[STeaMi] SteamiTemperature: no temperature sensor answered (WSEN-HIDS, WSEN-PADS). Check the board or try another one. (E10)` |
| `E10` | `[STeaMi] SteamiAccelerometer: the ISM330DL accelerometer (I2C 0x6B) did not answer. Check the board or try another one. (E10)` |
| `E10` | `[STeaMi] SteamiCompass: the LIS2MDL compass (I2C 0x1E) did not answer. Check the board or try another one. (E10)` |
| `E10` | `[STeaMi] SteamiButtons: the MCP23009E D-PAD expander (I2C 0x20) did not answer. The A, B and MENU buttons still work. (E10)` |
| `E11` | `[STeaMi] SteamiTemperature: this STeaMi is set as a V1c (forced by Steami.begin()), but its BME280 temperature sensor (I2C 0x76) did not answer. Check the #define, Steami.begin() or the board config. (E11)` |
| `E20` | `[STeaMi] SteamiLight: this STeaMi is a V1c, it has no light sensor (APDS-9960 is on V1a/V1b only). read() will return 0. (E20)` |
| `E20` | `[STeaMi] SteamiColor: this STeaMi is a V1c, it has no colour sensor (APDS-9960 is on V1a/V1b only). read() will return false. (E20)` |
| `E20` | `[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on V1a/V1b only). read() will return NONE. (E20)` |
| `E20` | `[STeaMi] <Class>: this STeaMi is a <release>, it has no <noun> (<chip> is on <other release> only). <fallback> (E20)` |

`E10` means that the chip of the release did not answer: with one candidate the
text names the chip and its I2C address, with several it lists them. `E11`
replaces it only for the sensors that differ between releases (temperature,
humidity, pressure, light, colour and gesture) when the release was forced
(`Steami.begin(...)`, `#define`) or read from the board config: the text then
tells you to check that setting. Distance, battery, accelerometer, gyroscope,
magnetometer, compass and buttons are on every release, so they always use
`E10`.
`E20` means that the function does not exist on this release. Only
`SteamiLight`, `SteamiColor` and `SteamiGesture` can print it, on a V1c (the
APDS-9960 is on V1a/V1b only); the other classes exist on every release. The
class name, the sensor and the end of the text change from one class to
another; the code and the shape do not. The classes that share a chip with
another one (the ISM330DL, the LIS2MDL, the APDS-9960) print their own message
once each.

### Calibration

When the board config holds a temperature calibration (`tc`), it is applied
automatically in `begin()`, once per chip, also when the release is forced.
The keys are the same as in the MicroPython `steami_config`:

| Key | Chip |
|-----|------|
| `bme` | BME280 |
| `hid` | WSEN-HIDS |
| `pad` | WSEN-PADS |

`printInfo()` adds `, calibrated` to the line of a chip that got its
calibration. `Steami.calibrate(chip)` applies it on demand and
`Steami.calibrated(chip)` tells whether it was applied. Without a config
zone, nothing changes.

Two more keys are applied automatically when the config holds them, with the
same names as in the MicroPython `steami_config`:

| Key | Fields | Goes to |
|-----|--------|---------|
| `ca` | `ox`, `oy`, `oz` offsets in g | the ISM330DL (accelerometer) |
| `cm` | `hx`, `hy`, `hz` hard iron in LSB, `sx`, `sy`, `sz` scales | the LIS2MDL (magnetometer and compass) |

The classes share their driver with `Steami.bme280()` and the others: once a
class has begun, the shared driver returns calibrated temperatures, and a
`calibrateTemperature()` that your sketch made earlier is overwritten.

## Colours

`SteamiRgb` is a colour, stored as one RGB565 word. `SteamiColor::rgb()` returns
one, and the display classes take one. Include `SteamiRgb.h` (or `SteamiColor.h`).

| Name | RGB | Grey level |
|------|-----|-----------|
| `STEAMI_BLACK` | 0, 0, 0 | 0 |
| `STEAMI_GRID_DARK` | 51, 51, 51 | 3 |
| `STEAMI_DARK` | 102, 102, 102 | 6 |
| `STEAMI_BLUE` | 85, 85, 255 | 6 |
| `STEAMI_RED` | 255, 85, 85 | 8 |
| `STEAMI_GRAY` | 153, 153, 153 | 9 |
| `STEAMI_LIGHT` | 187, 187, 187 | 11 |
| `STEAMI_GREEN` | 119, 255, 119 | 12 |
| `STEAMI_YELLOW` | 255, 255, 85 | 14 |
| `STEAMI_WHITE` | 255, 255, 255 | 15 |

```cpp
SteamiRgb orange(255, 128, 0);        // red, green, blue from 0 to 255
SteamiRgb same = {255, 128, 0};       // the braces work wherever a colour is expected
SteamiRgb mid = SteamiRgb::gray(8);   // 16 grey levels, 0 black to 15 white
uint16_t word = orange.rgb565();      // the RGB565 word
uint8_t level = orange.gray4();       // grey level 0..15, what an OLED shows
```

* An integer is not a colour: write `SteamiRgb::gray(15)`, not `15`. A grey
  level cannot turn into dark blue by mistake.
* The primitives never adjust a colour. `STEAMI_BLUE` on `STEAMI_DARK` is
  invisible on the OLED, where both are grey level 6.

## Display

`SteamiDisplay` draws on the screen of the board, V1a/V1b or V1c, with the
same calls. Include `SteamiDisplay.h` (or `STeaMi.h`), call `begin()` first in
`setup()`, draw, then `show()`.

```cpp
#include <SteamiBoard.h>
#include <SteamiDisplay.h>

SteamiDisplay screen;

void setup() {
    screen.begin();                       // first in setup(), never from a global initialiser
}

void loop() {
    screen.clear();
    screen.circle(screen.centerX(), screen.centerY(), 30, STEAMI_GREEN);
    screen.text("Hello", 40, 60, STEAMI_WHITE);
    screen.show();                        // nothing reaches the panel before this
    delay(1000);
}
```

| | V1a / V1b | V1c |
|-|-----------|-----|
| Panel | SSD1327, 128x128, 16 greys | GC9A01, 240x240, RGB565 |
| Frame buffer | 8 192 B, allocated when the screen is first used | 115 200 B, allocated by `begin()` |
| `show()` | 10 to 15 ms | at least 57.6 ms |

All coordinates are physical pixels of the panel: origin top-left, `x` to the
right, `y` down. On the OLED a colour becomes its grey level.

| Method | Description |
|--------|-------------|
| `explicit constexpr SteamiDisplay(SteamiBoard& board = Steami)` | Stores the address of the board only; touches no hardware. |
| `bool begin()` | Begin the screen once and keep the result. `false` with one message (`E30`, `E31`). Idempotent. |
| `bool isAvailable() const` | True once the screen began. |
| `SteamiStatus status() const` | `OK`, `NOT_STARTED`, `UNKNOWN_BOARD` or `NO_MEMORY`. |
| `const char* error() const` | `""` when `OK`, else `"not started"`, `"unknown board"` or `"no memory"`. |
| `bool isColor()` | True on a V1c. |
| `int16_t width()` / `int16_t height()` | Size of the panel: 128 or 240. |
| `int16_t centerX()` / `int16_t centerY()` | Centre of the panel. |
| `int16_t radius()` | Half the width. |
| `void clear(SteamiRgb c = STEAMI_BLACK)` | Fill the screen. |
| `void pixel(int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE)` | One pixel. |
| `void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c = STEAMI_WHITE)` | A line, ends included. |
| `void hline(int16_t x, int16_t y, int16_t w, SteamiRgb c = STEAMI_WHITE)` | A horizontal line of `w` pixels. |
| `void vline(int16_t x, int16_t y, int16_t h, SteamiRgb c = STEAMI_WHITE)` | A vertical line of `h` pixels. |
| `void rect(int16_t x, int16_t y, int16_t w, int16_t h, SteamiRgb c = STEAMI_WHITE, bool fill = false)` | A rectangle, outline or filled. |
| `void circle(int16_t cx, int16_t cy, int16_t r, SteamiRgb c = STEAMI_WHITE, bool fill = false)` | A circle, outline or filled. |
| `void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c = STEAMI_WHITE, bool fill = false)` | A triangle, outline or filled. |
| `void arc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t sweepDeg, SteamiRgb c = STEAMI_WHITE, uint8_t width = 1)` | A ring segment. Degrees, 0 = noon, clockwise; `r` is the outer radius and the ring is `width` thick inward. |
| `void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1)` | Text in 8x8 glyphs (`CHAR_W`, `CHAR_H`). UTF-8: accents are folded to their plain letter, and `°` is drawn. |
| `int16_t textWidth(const char* s, uint8_t scale = 1) const` | Width in pixels; counts glyphs, not bytes. |
| `void show()` | Send the drawing to the panel. |
| `void rotate(bool upsideDown)` | Turn the picture by 180 degrees. |
| `void invert(bool on)` | Invert the panel. |
| `void powerOn()` / `void powerOff()` | Switch the panel on or off. |
| `SteamiDisplayBackend* backend()` | The backend of the board, built but not begun; `nullptr` without one. |

Rules:

* Everything is clipped to the panel: a shape that goes off the edge is cut,
  never wrapped.
* The first drawing call begins the screen if your sketch did not.
* After a failure nothing is drawn and `show()` does nothing; `error()` says
  why.
* Every `SteamiDisplay` shares the one screen of the board.
* The geometry (`width()`, `height()`, ...) is valid before `begin()`. On a
  V1a/V1b this already allocates the 8 KB buffer.
* Use it from `setup()` onwards only, never from a global initialiser: it
  detects the release over I2C before the core is initialised.
* V1a/V1b never report `E31`.
* To avoid the detection, force the release before the first drawing call
  (`Steami.begin(SteamiRelease::V1C)` or `#define STEAMI_BOARD_V1C`).
* Checked on a V1c; V1a/V1b only by the native tests.

Messages of the display (`who` is `SteamiDisplay`):

| Code | Text | `error()` |
|------|------|-----------|
| `E30` | `[STeaMi] SteamiDisplay: cannot tell if this board is a V1a/V1b or a V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)` | `"unknown board"` |
| `E31` | `[STeaMi] SteamiDisplay: not enough RAM for the colour screen (115200 bytes in one block). Call screen.begin() first in setup(). (E31)` | `"no memory"` |

## Using your own drivers

Some drivers are not shared by `Steami` yet. Build them on the board's bus:

```cpp
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>

SteamiDaplinkBridge bridge(Steami.wire());

void setup() {
    Steami.begin();
    bridge.begin();
}
```

* Pass `Steami.wire()` to every driver you build yourself. It can be called
  from a global initialiser (`SteamiBME280 bme(Steami.wire());`): the bus
  object is built and begun on first use. Checked on a V1c with STM32duino
  3.1.0.
* Do not build a `SteamiGC9A01` or a `SteamiSSD1327` next to `SteamiDisplay`:
  that is a second frame buffer (115 200 B on a V1c, 8 KB on a V1a/V1b) and two
  drivers drawing to the same panel. The screen shows whichever called `show()`
  last.
* Never create a second `TwoWire` on `I2C_INT_SDA` / `I2C_INT_SCL`. On
  STM32duino 3.1.0 the last `TwoWire` to call `begin()` owns the I2C1
  interrupt (`i2c_handles[I2C1_INDEX]` in `twi.c`); the transfers of the other
  one time out without any error.

## Testing

```bash
make test-native/steami_board   # detection, routing, messages, lazy slots
make test-native/steami_display_board   # display primitives, E30, E31
.venv/bin/pio test -e native_asan
.venv/bin/python3 scripts/check-footprint.py
make test-examples/steami_board
make build
make lint
```

## License

GPL-3.0-or-later - see [LICENSE](../../LICENSE).
