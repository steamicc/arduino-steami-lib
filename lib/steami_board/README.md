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

## Examples

The examples use the function classes and the screen, never a driver directly
(except `Pads`, which drives the expander through `Steami.mcp23009e()`): the
same sketch runs on a V1a, a V1b or a V1c.

| Example | What it does | Try it |
|---------|--------------|--------|
| [`Temperature`](examples/Temperature/) | Print the temperature once per second: BME280 on a V1c, WSEN-HIDS (then WSEN-PADS) on a V1a/V1b, with the board calibration applied. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#temperature) |
| [`Humidity`](examples/Humidity/) | Print the relative humidity once per second: BME280 on a V1c, WSEN-HIDS on a V1a/V1b. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#humidity) |
| [`Pressure`](examples/Pressure/) | Print the pressure in hPa and the altitude in metres once per second: BME280 on a V1c, WSEN-PADS on a V1a/V1b. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#pressure) |
| [`Distance`](examples/Distance/) | Print the distance measured by the VL53L1X once per second, in millimetres. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#distance) |
| [`Battery`](examples/Battery/) | Print the battery charge, voltage and current of the BQ27441 gauge once per second. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#battery) |
| [`Accelerometer`](examples/Accelerometer/) | Print the ISM330DL acceleration in g five times per second, with its norm and the shake flag. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#accelerometer) |
| [`Gyroscope`](examples/Gyroscope/) | Print the ISM330DL angular speed in degrees per second five times per second. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#gyroscope) |
| [`Buttons`](examples/Buttons/) | Print each press and release of the seven buttons (A, B, MENU and the D-PAD). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#buttons) |
| [`Magnetometer`](examples/Magnetometer/) | Print the LIS2MDL magnetic field in microtesla twice per second. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#magnetometer) |
| [`Compass`](examples/Compass/) | Print the LIS2MDL heading and direction twice per second, board flat and screen up. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#compass) |
| [`Light`](examples/Light/) | Print the APDS-9960 light level twice per second (V1a/V1b; a V1c reports E20). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#light) |
| [`Color`](examples/Color/) | Print the colour measured by the APDS-9960 twice per second (V1a/V1b; a V1c reports E20). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#color) |
| [`Gesture`](examples/Gesture/) | Print each hand gesture seen by the APDS-9960 (V1a/V1b; a V1c reports E20). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#gesture) |
| [`Drawing`](examples/Drawing/) | Draw the primitives gallery of `SteamiDisplay` on the screen: arcs, accented text, rectangles, a disc, a triangle and a line. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#drawing) |
| [`ScreenTemperature`](examples/ScreenTemperature/) | Show the temperature on the screen (mockup 01 of the tutorials). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screentemperature) |
| [`ScreenBattery`](examples/ScreenBattery/) | Show the battery charge, a bar and the voltage on the screen (mockup 02). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screenbattery) |
| [`ScreenComfort`](examples/ScreenComfort/) | Show temperature and humidity side by side, with a comfort verdict (mockup 03). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screencomfort) |
| [`ScreenGauge`](examples/ScreenGauge/) | Show the VL53L1X distance on a round gauge (mockup 04). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screengauge) |
| [`ScreenCompass`](examples/ScreenCompass/) | Show a compass rose with its needle (mockup 07). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screencompass) |
| [`ScreenWatch`](examples/ScreenWatch/) | Show an analog watch (mockup 09). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screenwatch) |
| [`ScreenFaces`](examples/ScreenFaces/) | Show a smiley whose mood follows the distance in front of the board; button A shows the six faces (mockup 08). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screenfaces) |
| [`ScreenGraph`](examples/ScreenGraph/) | Show a graph of the last 20 readings of a sensor (mockup 05). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screengraph) |
| [`ScreenMenu`](examples/ScreenMenu/) | Show a menu moved with the D-PAD (mockup 06). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#screenmenu) |
| [`WeatherStation`](examples/WeatherStation/) | A weather station on four screen pages: temperature, humidity, pressure graph and compass. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#weatherstation) |
| [`BoardInfo`](examples/BoardInfo/) | Print the detected release, where the answer comes from, and whether each I2C chip of the release answers. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board#boardinfo) |
| [`Pads`](examples/Pads/) | Read the crocodile pad P24 and drive P28 through the MCP23009E expander: an LED on P28 lights and the buzzer beeps while P24 is grounded. | — |

Each example but `Pads` is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board).

### Building an example

```bash
make list-examples DRIVER=steami_board
make flash-steami_board/BoardInfo
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-steami_board/BoardInfo DURATION=30
```

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

### Pads on the expander

The crocodile pads P28, P29, P24 and P25 are GPIO 0 to 3 of the MCP23009E, not
pins of the MCU. `STEAMI_PAD_P28`, `STEAMI_PAD_P29`, `STEAMI_PAD_P24` and
`STEAMI_PAD_P25` (in `SteamiTypes.h`) give their GPIO number, for the shared
expander driver. The [`Pads`](examples/Pads/) example reads one pad and drives
another:

```cpp
SteamiMCP23009E* expander = Steami.mcp23009e();  // nullptr when it does not answer
expander->setup(STEAMI_PAD_P24, MCP23009_DIR_INPUT, MCP23009_PULLUP);
expander->getLevel(STEAMI_PAD_P24);  // MCP23009_LOGIC_LOW when grounded
```

The expander outputs are open-drain: they sink current (25 mA at most per pin)
but do not source it. Wire a load such as an LED from 3V3 to the pad and drive
the pad `MCP23009_LOGIC_LOW` to turn it on.

The MicroPython names are `steami.PAD_P24`...

## Function classes

Thirteen classes read a function without you choosing a chip:
`SteamiTemperature`, `SteamiHumidity`, `SteamiPressure`, `SteamiDistance`,
`SteamiBattery`, `SteamiAccelerometer`, `SteamiGyroscope`,
`SteamiMagnetometer`, `SteamiCompass`, `SteamiLight`, `SteamiColor`,
`SteamiGesture` and `SteamiButtons`. They pick the chip of the release
(V1a/V1b or V1c) and share the driver the board already built. Each has a
ready-to-run example in [`examples/`](examples/).

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/steami-board).

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
| `SteamiCompass` | `heading()` in degrees, `headingTiltCompensated()`, `direction()`, `setDeclination(deg)`, `calibrate(seconds = 12)` | `NAN`, `""` |
| `SteamiLight` | `read()`, the raw clear count of the sensor | `0` |
| `SteamiColor` | `read(r, g, b)` (raw counts, returns whether it worked), `red()`, `green()`, `blue()`, `rgb()` (hue of the object, largest channel 255) | `false`, `0`, `STEAMI_BLACK` |
| `SteamiGesture` | `read(timeoutMs = 0)` gives `NONE`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `NEAR` or `FAR`; `SteamiGesture::name(d)` | `NONE` |
| `SteamiButtons` | `isPressed(b)`, `wasPressed(b)`, `wasReleased(b)`, `getPresses(b)`, `waitForPress(timeoutMs = 0)` with `SteamiButton::A`, `B`, `MENU`, `UP`, `DOWN`, `LEFT`, `RIGHT` | `false`, `NONE` |

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
  A failed I2C read of the field returns `NAN` on every axis, and
  `SteamiCompass::heading()` returns `NAN` too (`direction()` returns `""`).
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
  released. A change is taken at once and the bounces of the next 20 ms are
  ignored. A tap made between two calls is caught whatever the time between
  them ([issue #251](https://github.com/steamicc/arduino-steami-lib/issues/251),
  [issue #341](https://github.com/steamicc/arduino-steami-lib/issues/341)):
  * A, B and MENU: `begin()` attaches an interrupt (`CHANGE`) to each pin. A tap
    made between two calls gives, at the next call, `wasPressed()` and
    `wasReleased()` true and `isPressed()` false.
  * D-PAD: each call reads the latch of the expander (`interruptEvent()`), which
    keeps the first change since the previous read, and takes that change at
    once. The latch does not keep the time of the change: once a read changed
    the D-PAD, the call reads it again 20 ms later and only the level then
    counts, so a contact bounce made just after a read is not a new tap. That
    call takes 20 ms more (40 ms at most). A tap made between two calls then
    gives the same as on A, and a release is reported by the call that reads
    it. When that read found a whole tap (pressed, then released), a press
    found 20 ms later is a second tap if the button was still released 5 ms
    after the read; pressed again within those 5 ms, it is taken for a bounce
    of the first press. The pad callbacks a sketch registers on the expander
    still run.
  * The D-PAD is one lever: released from one direction, it can touch the
    opposite one for a moment. A press found in the read that releases another
    direction counts only if it is still there 20 ms later.
  * `wasPressed()` and `wasReleased()` are flags: several presses (or
    releases) made between two calls give one, and presses nobody reads are
    not reported later more than once. `getPresses()` counts every press since
    its last call for that button and sets the count to zero, like
    `get_presses()` on the micro:bit (it stops at 255 and does not depend on
    `wasPressed()`): use it to count taps with a slow loop. Two D-PAD taps made
    between two reads of the expander still give one press: the chip latches
    only the first change.
  * The board leaves EXTI line 0 to MENU: `INT_EXPANDER` (PB0) shares it with
    `MENU_BUTTON` (PA0, `USER_BTN`), so the expander is used without its
    interrupt pin. An interrupt the sketch attaches to `A_BUTTON`,
    `B_BUTTON`, `MENU_BUTTON` or another pin of their EXTI line (0, 7, 8)
    replaces the one of `SteamiButtons`: that button is then only sampled at
    each call. On STM32duino 3.1, `detachInterrupt()` on any pin of EXTI lines
    5 to 9 (P1, P2, P7, P10, P12, the Jacdac pins) also turns off the
    interrupts of A (line 7) and B (line 8), until the next `attachInterrupt()`.

  A button held at start is not a press. `waitForPress()` drops the presses
  made before the call, the latched taps included, and returns `NONE` after
  the timeout.
* A function that failed is not tried again until the board resets (or
  `Steami.end()`). After plugging in a battery, reset the board.

### Heading convention

`SteamiCompass::heading()` is in degrees clockwise from north, in [0, 360),
with the board held flat and the screen up. 0 means that the top of the screen
(the USB connector side) points north, 90 east, 180 south and
270 west. It is `atan2(-x, y)` of the corrected X and Y fields, measured on a
V1c: the convention of the `SteamiLIS2MDL` driver (`headingFlatOnly()`).
Without tilt compensation, so keep the board flat. With the screen up, the
accelerometer reads `z` about -1 g.

`SteamiCompass::headingTiltCompensated()` gives the same heading for a tilted
board, up to about 60 degrees: it reads the ISM330DL accelerometer (started as
by `SteamiAccelerometer::begin()`, +-8 g at 104 Hz) and calls
`SteamiLIS2MDL::headingWithTiltCompensation()`. Flat, it equals `heading()`.
Its first call sets the ISM330DL to +-8 g at 104 Hz, as `SteamiAccelerometer`
does: it replaces a range that the sketch set on the ISM330DL before.
It needs a 3D calibration: `calibrate()`, the `cm` calibration of the board
config, or `calibrateMinmax3d()`. After `calibrateMinmax2d()` the `z` field is
not calibrated and the result is meaningless. It returns `NAN` when the compass
or the accelerometer is unavailable or a read fails. A missing accelerometer is
reported under the `SteamiAccelerometer` name (`E10`), not under
`SteamiCompass`. The sign of the LIS2MDL Z axis was measured on a V1c on
2026-11-28
([micropython-steami-lib#344](https://github.com/steamicc/micropython-steami-lib/issues/344)).

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
`setTemperatureOffset()` or `calibrateTemperature()` that your sketch made
earlier is overwritten. Call `setTemperatureOffset(0)` on the shared driver
to read it without calibration.

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
| `void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1)` | Text in 8x8 glyphs on the 128 px screen, Spleen 8x16 on the 240 px screen, `scale` times larger; the same width on both (`CHAR_W`, 8 px per glyph at scale 1), height `textHeight(scale)` (`CHAR_H`, 8, is the glyph height on the 128 px screen only). UTF-8: `°` is drawn; accents are drawn on the 240 px screen and folded to their plain letter on the 128 px screen. |
| `int16_t textWidth(const char* s, uint8_t scale = 1) const` | Width in pixels; counts glyphs, not bytes. |
| `int16_t textHeight(uint8_t scale = 1)` | Line height in pixels: 8 per scale on 128, 16 on 240. |
| `void show()` | Send the drawing to the panel. |
| `void rotate(bool upsideDown)` | Turn the picture by 180 degrees. |
| `void invert(bool on)` | Invert the panel. |
| `void powerOn()` / `void powerOff()` | Switch the panel on or off. |
| `SteamiDisplayBackend* backend()` | The backend of the board, built but not begun; `nullptr` without one. |

Since this release, `text()` is 16 px tall at scale 1 on the V1c, and stacked lines use `textHeight()`.

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

## Screen

`SteamiScreen` is a `SteamiDisplay` (everything above works on it) that adds
widgets placed for the round screen, on both panels. Include `SteamiScreen.h`
(or `STeaMi.h`) and call `begin()` first in `setup()`.

```cpp
#include <Arduino.h>
#include <SteamiScreen.h>
#include <SteamiTemperature.h>

SteamiScreen screen;
SteamiTemperature temperature;

void setup() {
    Serial.begin(115200);  // the screen and the sensor report their errors here
    screen.begin();
    temperature.begin();
}

void loop() {
    screen.clear();
    screen.title("Temperature");
    screen.value(temperature.read(), "°C");
    screen.subtitle(temperature.chip());  // "WSEN-HIDS" on V1a/V1b, "BME280" on V1c
    screen.show();
    delay(500);
}
```

Immediate mode, as in MicroPython: `clear()`, the widgets in call order (a later
widget paints over an earlier one), then `show()`.

| Method | Default colour | Description |
|--------|----------------|-------------|
| `explicit constexpr SteamiScreen(SteamiBoard& board = Steami)` | | Same as `SteamiDisplay`; touches no hardware. |
| `void title(const char* text, SteamiRgb c = STEAMI_GRAY)` | `STEAMI_GRAY` | One line at the top. |
| `void subtitle(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, SteamiRgb c = STEAMI_DARK)` | `STEAMI_DARK` | One to three lines at the bottom (3 lines at most). |
| `void subtitle(const char* l1, SteamiRgb c)` | | One line, with a colour. |
| `void subtitle(const char* l1, const char* l2, SteamiRgb c)` | | Two lines, with a colour. |
| `void value(SteamiValue v, const char* unit = nullptr, SteamiAt at = SteamiAt::CENTER, const char* label = nullptr, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 2, int16_t yOffset = 0)` | `STEAMI_WHITE` | A number or a text in large digits, with its unit (`STEAMI_LIGHT`) and its label (`STEAMI_GRAY`). |
| `void value(SteamiValue v, const char* unit, SteamiRgb c)` | | The value at the centre, with a colour. |
| `void setDecimals(uint8_t d)` | | Digits after the point for float values: 1 by default, 4 at most. Per object. |
| `void bar(float v, float maxV = 100, int16_t yOffset = 0, SteamiRgb c = STEAMI_LIGHT)` | `STEAMI_LIGHT` | A horizontal bar filled from 0 to `maxV`. |
| `void bar(float v, float maxV, SteamiRgb c)` | | The bar at the centre, with a colour. |
| `void text(const char* s, SteamiAt at = SteamiAt::CENTER, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1)` | `STEAMI_WHITE` | Text at a position; `text(s, x, y, ...)` of `SteamiDisplay` is still there, in pixels. |
| `uint8_t maxChars()` | | Characters of scale 1 across the panel: 16 on 128, 30 on 240. |
| `void gauge(float v, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT, uint8_t arcWidth = 0)` | track `STEAMI_DARK`, labels `STEAMI_GRAY`, fill `STEAMI_LIGHT` | A ring of 270 degrees near the edge, gap at the bottom, filled clockwise from the lower left; the integer parts of `minV` and `maxV` sit inside its two ends. No value: draw it with `value()`. |
| `template <size_t N> void graph(const SteamiHistory<N>& h, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT)` and `void graph(const float* data, size_t n, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT)` | axes and labels `STEAMI_DARK`, dashed middle line `STEAMI_GRID_DARK`, newest value `STEAMI_WHITE`, curve `STEAMI_LIGHT` | A curve of the last values, oldest on the left, between the min and max labels, with a dashed middle line; the integer part of the newest value is written above the box (`--` when there is none or it is `NAN`). |
| `template <size_t N> int menu(const char* const (&items)[N], int selected = 0, SteamiRgb c = STEAMI_WHITE)` and `int menu(const char* const* items, size_t n, int selected = 0, SteamiRgb c = STEAMI_WHITE)` | bar `STEAMI_DARK`, selected `STEAMI_WHITE`, others `STEAMI_GRAY` | A list of items with the selected one marked; returns the selected index, wrapped into the list. |
| `void compass(float headingDeg, SteamiRgb c = STEAMI_LIGHT)` | rose `STEAMI_DARK`, N `STEAMI_WHITE`, E S W `STEAMI_GRAY`, needle `c` | A compass rose whose needle points to real north. |
| `void watch(uint8_t h, uint8_t m, uint8_t s = 0, SteamiRgb c = STEAMI_LIGHT)` | dial `STEAMI_DARK`, 12 3 6 9 `STEAMI_WHITE`, seconds and pivot `STEAMI_GRAY`, hour and minute hands `c` | An analog watch. |
| `void face(SteamiFace f, bool compact = false, SteamiRgb c = STEAMI_LIGHT)` | `STEAMI_LIGHT` | A pixel-art face, centred. |
| `void face(SteamiFace f, SteamiRgb c)` | | The same, with a colour. |
| `void face(const uint8_t bitmap[8], bool compact = false, SteamiRgb c = STEAMI_LIGHT)` | `STEAMI_LIGHT` | Your own 8x8 face. |

`SteamiFace` names the six faces: `HAPPY`, `SAD`, `SURPRISED`, `SLEEPING`,
`ANGRY` and `LOVE`. `compact` draws them with smaller squares (8 px instead of
11 on the 128 px screen), half a square higher, to leave room for a title and
a subtitle. `face(bitmap)` takes your own picture: 8 bytes, one per row from top
to bottom, the leftmost pixel in the high bit (`0x80`); `nullptr` draws nothing.

`SteamiHistory<N>` (in `SteamiHistory.h`) keeps the last `N` values for `graph()`:
`add(v)`, `size()`, `capacity()`, `operator[](i)` (0 is the oldest; `NAN` past
`size()`), `last()` (`NAN` when empty) and `clear()`. It takes 4N + 4 bytes, no
heap, with `N` from 1 to 65535. Draw it with `screen.graph(history, min, max)`.

`SteamiAt` names the nine positions: `N`, `NE`, `E`, `SE`, `S`, `SW`, `W`, `NW`
and `CENTER` (as `"N"` ... `"CENTER"` in MicroPython).

`SteamiValue` is what `value()` shows. It accepts `int`, `unsigned`, `long`,
`unsigned long`, `float`, `double` and `const char*`; `uint8_t`, `int16_t`,
`uint16_t` and `bool` become `int`, and `0` is the number 0, not a text. A
`long long` is a compile error. A float is rounded to `setDecimals()` digits.
`NAN`, `INF`, a float beyond 999999999 and a `nullptr` text show `--`. `format()`
is the formatter `value()` uses, in `SteamiValue.h`.

Layout rules:

* The same sketch runs on 128 and 240 px: positions and `yOffset` are in the
  units of the 128 px screen, also on 240 (an offset of 8 moves the value by
  15 px there), and text on 240 uses fonts drawn for that panel (8 px per
  character, 16 px tall), for widgets and `text()` alike.
* Text never leaves the circle: it is cut (no ellipsis), or `value()` drops to
  a smaller size first.
* A title of up to 11 characters stays at the top; the examples keep to that.
* The values at `W` and `E` share one size, so two values side by side always
  match.
* A bar fill or a gauge fill is lifted to at least 4 grey levels above its
  track on the V1a/V1b grey screen (`STEAMI_BLUE` over the `STEAMI_DARK` gauge
  track becomes grey 10; the bar's track is `STEAMI_GRID_DARK`). The primitives
  of `SteamiDisplay` never adjust a colour.
* `gauge()` draws the ring and the min/max labels only: call `value()` after it
  to show the number (as in MicroPython). Its `arcWidth` is in pixels of the
  panel, not in 128 px units; 0 keeps the default.
* The needle of `compass()` points to real north and the rose turns, so the
  letters stay upright (see [Heading convention](#heading-convention)): at
  heading 90 the N is on the left. `NAN` (no sensor) draws the rose without a
  needle.
* `graph()` writes the newest value above the box itself: do not add a `value()`
  over it. It writes only two axis labels (min and max), in full inside the box;
  a `NAN` leaves a gap in the curve; values outside the range stay on the edge.
* `menu()` shows 5 rows on 128 px and 6 on 240 px; the bar follows the circle;
  `-1` wraps to the last item, and the index is returned. Keep it in an `int`:
  with a `const char* const` array, a `long`, `unsigned` or `size_t` index calls
  the `(items, n)` form, with `n` set to the index; with a `const char*` array a
  `size_t` index does too (and `unsigned` on the STeaMi, where `size_t` is
  `unsigned int`), with GCC's "ISO C++ says that these are ambiguous" warning.
* The selected text of `menu()` is lifted on the grey screen, as the fills are.
* `watch()` takes hours 0 to 23 and minutes and seconds 0 to 59; the hands move
  in whole degrees (the hour hand with the minutes, the minute hand with the
  seconds).

| MicroPython | Arduino |
|-------------|---------|
| `screen = Screen(display)` | `SteamiScreen screen;` then `screen.begin();` first in `setup()` |
| `screen.value(v, unit="C", at="W", label="TEMP", color=C)` | `screen.value(v, "°C", SteamiAt::W, "TEMP", C)` |
| `screen.subtitle("a", "b", color=C)` | `screen.subtitle("a", "b", C)` (3 lines at most) |
| `screen.bar(v, max_val=100, color=C)` | `screen.bar(v, 100, C)` |
| `screen.text("x", at="NE", scale=2)` | `screen.text("x", SteamiAt::NE, STEAMI_WHITE, 2)` |
| `screen.gauge(v, min_val, max_val, color=C)` | `screen.gauge(v, min, max, C)` |
| `screen.graph(data, min_val, max_val)` | `screen.graph(history, min, max)` with `SteamiHistory<N>` |
| `screen.menu(items, selected=i)` | `i = screen.menu(ITEMS, i);` (array size deduced, index wrapped) |
| `screen.face("happy", color=C)` | `screen.face(SteamiFace::HAPPY, C)` |
| `"N"` `"NE"` ... `"CENTER"`, `BLACK` ... `YELLOW` | `SteamiAt::N` ... `SteamiAt::CENTER`, `STEAMI_BLACK` ... `STEAMI_YELLOW` |
| `screen.center`, `screen.radius`, `screen.width` | `centerX()`, `centerY()`, `radius()`, `width()` |

Examples, each reproducing a mockup of the tutorials:

| Example | Mockup |
|---------|--------|
| `ScreenTemperature` | `01_temperature_ref.png`: title, value with its unit, sensor name |
| `ScreenBattery` | `02_battery_ref.png`: charge in percent, green bar, voltage and gauge name |
| `ScreenComfort` | `03_comfort_dual_ref.png`: temperature and humidity side by side, comfort verdict |
| `ScreenGauge` | `04_circular_gauge_ref.png`: distance of the VL53L1X in a ring |
| `ScreenCompass` | `07_compass_ref.png`: the rose turns with the compass heading |
| `ScreenGraph` | `05_scrolling_graph_ref.png`: light, or pressure on the V1c, over the last 20 s |
| `ScreenMenu` | `06_dpad_menu_ref.png`: six items, UP and DOWN |
| `WeatherStation` | no mockup of its own: four pages, as mockups 01, 04, 05 and 07 |
| `ScreenFaces` | `08_smiley_*_ref.png`: the six faces (button A shows the next) and the reactive smiley |
| `ScreenWatch` | `09_watch_ref.png`: an analog watch |

Messages of the screen are those of the display, with `who` = `SteamiScreen`:

| Code | Text | `error()` |
|------|------|-----------|
| `E30` | `[STeaMi] SteamiScreen: cannot tell if this board is a V1a/V1b or a V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)` | `"unknown board"` |
| `E31` | `[STeaMi] SteamiScreen: not enough RAM for the colour screen (115200 bytes in one block). Call screen.begin() first in setup(). (E31)` | `"no memory"` |

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
make test-native/steami_screen   # widgets, layout rules, SteamiValue, E30, E31
make test-native/steami_screen_mockups   # mockups 01 to 09 at 128 and 240 px
# score the mockup renders against the tutorials' images (needs Pillow and NumPy)
mkdir -p /tmp/mockups   # writePpm does not create the folder
STEAMI_PPM_DIR=/tmp/mockups .venv/bin/pio test -e native -f native/test_steami_screen_mockups
python3 scripts/mockup-review.py --ppm-dir /tmp/mockups --out /tmp/mockup-review
.venv/bin/pio test -e native_asan
.venv/bin/python3 scripts/check-footprint.py
make test-examples/steami_board
make build
make lint
```

## Third-party fonts

The 8x8 font of the 128 px screen is MicroPython's `extmod/font_petme128_8x8.h`
(commit b0a1b60a9b425cdb24e36d71974e902338ffb059), under the MIT license:

```text
This file is part of the MicroPython project, http://micropython.org/

The MIT License (MIT)

Copyright (c) 2013, 2014 Damien P. George

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

The fonts of the 240 px screen are generated from Spleen 2.1.0
(<https://github.com/fcambus/spleen>), under the BSD-2-Clause license:

```text
Copyright (c) 2018-2024, Frederic Cambus
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

## References

* STeaMi wiki: [Board revisions](https://wiki.steami.cc/docs/hardware/revisions).
* MicroPython layer: [`steami`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/steami) (function classes) and [`steami_screen`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/steami_screen) (screen widgets).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
