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

This is the [`BoardInfo`](examples/BoardInfo/) example. It prints the release,
where the answer comes from, and the I2C chips that answered.

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
per (class, code); `lastError()` keeps the last one, and `printInfo()` shows
it again if it was reported earlier.

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
| `void printInfo(Print& out)` / `void printInfo()` | Print the banner, the I2C chips and the last message, to `out` or through the logger. |
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

## Testing

```bash
make test-native/steami_board   # detection, routing, messages, lazy slots
make test-examples/steami_board
make build
make lint
```

## License

GPL-3.0-or-later - see [LICENSE](../../LICENSE).
