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
| `SteamiStatus claim(SteamiChip c)` | Build the driver of a chip on the first call and begin it once. `OK` or `NOT_RESPONDING` for the nine I2C chips, `NOT_STARTED` for the displays. |
| `bool built(SteamiChip c)` | True when the driver of the chip is constructed. |
| `bme280()`, `wsenHids()`, `wsenPads()`, `apds9960()`, `ism330dl()`, `lis2mdl()`, `vl53l1x()`, `bq27441()`, `mcp23009e()` | The shared driver of a chip, `nullptr` unless `claim()` gives `OK`. |
| `void printInfo()` | Through the logger: the banner (unless the logger already delivered it), the I2C chips of the release, then the last message (unless the logger already delivered it). |
| `void printInfo(Print& out)` | A full report to `out`: always the banner, the I2C chips of the release, then the last message unless the logger already delivered it. |
| `void setLogger(void (*log)(const char*))` | Where messages go; `nullptr` keeps the library silent. |
| `const char* lastError()` | The last message reported, `""` when none. |
| `void report(const char* who, SteamiStatus s, const char* detail, uint8_t code)` | Store and log a coded message (once per pair). `who` must be a literal. |
| `TwoWire& wire()` | The internal I2C bus; begins it the first time. |
| `const SteamiBoardPins& pins()` | The pins the board layer uses. |
| `void end()` | Destroy the shared drivers and forget reported messages. The logger is kept. |

Free functions: `steamiDecodeRevision(int32_t)`, `steamiRoute(release,
function, candidate)`, `steamiChipName(chip)`, `steamiChipAddress(chip)` and
`steamiForceRelease(release)` (used by the `#define` mechanism, do not call
it).

## Using your own drivers

Some drivers are not shared by `Steami` yet. Build them on the board's bus,
from `setup()`:

```cpp
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>

void setup() {
    Steami.begin();
    static SteamiDaplinkBridge bridge(Steami.wire());
    bridge.begin();
}
```

* Pass `Steami.wire()` to every driver you build yourself, and call it from
  `setup()` (or later), never from a global initialiser: the bus is built and
  begun on first use.
* Never create a second `TwoWire` on `I2C_INT_SDA` / `I2C_INT_SCL`. On
  STM32duino 3.1.0 the last `TwoWire` to call `begin()` owns the I2C1
  interrupt (`i2c_handles[I2C1_INDEX]` in `twi.c`); the transfers of the other
  one time out without any error.
* The examples of the other drivers still create their own bus. Do not mix
  them with `Steami` in one sketch until they move to `Steami.wire()` (a later
  sub-project).

## Testing

```bash
make test-native/steami_board   # detection, routing, messages, lazy slots
make test-examples/steami_board
make build
make lint
```

## License

GPL-3.0-or-later - see [LICENSE](../../LICENSE).
