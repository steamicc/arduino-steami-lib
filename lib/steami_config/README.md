# STeaMi Config

Arduino/C++ persistent configuration module for the STeaMi board.

Configuration data is stored as compact JSON in the 1 KB STM32F103 DAPLink
config zone. It is intended for board information, sensor calibration values,
and small persistent counters.

## Hardware

* No chip of its own: the configuration lives in the 1 KB config zone of the
  STM32F103 DAPLink chip, reached over the STeaMi **internal** I2C bus
  (address `0x3B`) through the
  [STeaMi DAPLink Bridge](../daplink_bridge/README.md) library.
* The config zone needs a recent DAPLink firmware on the board. Older
  firmwares lock up when they receive a config command, so the driver checks
  for the zone first (`SteamiDaplinkBridge::hasConfigZone()`): on such a board,
  `begin()`, `load()` and `save()` return `false` without touching it.

## Quick start

```cpp
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiConfig.h>

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiConfig config(bridge);

void setup() {
    Serial.begin(115200);

    if (!config.begin()) {
        Serial.println("DAPLink bridge not detected");
        return;
    }
    config.load();  // keep what is already stored (calibrations...)

    config.setBoardRevision(131);  // series V1c
    config.setBoardName("STeaMi-01");
    config.save();
}

void loop() {}
```

`begin()` probes the DAPLink bridge; `load()` reads the stored
configuration. Setters only change the in-memory copy, and `save()`
replaces the whole zone with it: call `load()` before changing a value, or
the other stored settings are lost.

## Examples

| Example | What it does | Try it |
|---------|--------------|--------|
| [`show_config`](examples/show_config/) | Load and print the stored board information, calibrations and boot count, without writing anything. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#show_config) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config).

### Building an example

```bash
make list-examples DRIVER=steami_config
make flash-steami_config/show_config
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-steami_config/show_config DURATION=30
```

## API

### Persistence

| Method | Description |
|---|---|
| `bool begin()` | Probe the DAPLink bridge. Returns `false` if it is missing or if its firmware has no config zone. |
| `bool load()` | Load the JSON configuration from the config zone. An empty zone gives an empty configuration. |
| `bool save()` | Erase the config zone and write the current compact JSON. Returns `false` if it does not fit. |
| `void clear()` | Clear only the in-memory configuration. |

Invalid JSON is discarded: `load()` returns `false` and leaves an empty
in-memory configuration.

The serialized JSON must stay under 1 KB (1023 bytes at most, the size of
the DAPLink config zone minus its last byte).

### Board information

```cpp
config.setBoardRevision(131);  // series V1c, schematic index 1
config.setBoardName("STeaMi-01");

int32_t revision;
String name;

if (config.boardRevision(revision)) {
    // revision is valid
}

if (config.boardName(name)) {
    // name is valid
}
```

Setters and `clear*()` only change the in-memory copy; getters return
`false` when the value is not set.

| Method | Description |
|---|---|
| `void setBoardRevision(int32_t revision)` / `bool boardRevision(int32_t& revision) const` / `void clearBoardRevision()` | Board revision code (see below). |
| `void setBoardName(const String& name)` / `bool boardName(String& name) const` / `void clearBoardName()` | Board name. |

#### Revision code

The board revision is a decimal code: the hundreds are the generation, the
tens the release (1 = a, 2 = b, 3 = c) and the units the schematic index
(0 = prototype).

| Code | Board |
|---|---|
| 110 | V1a prototype |
| 111 | V1a, schematic index 1 (A1) |
| 120 | V1b prototype |
| 121 | V1b, schematic index 1 (B1) |
| 130 | V1c prototype |
| 131 | V1c, schematic index 1 (C1) |

The code is decoded by `steamiDecodeRevision()` in the STeaMi Board library.
Any other value (for example the old example value `3`) is not a valid code.

### Temperature calibration

Supported sensor names match the MicroPython library:

- `hts221`
- `lis2mdl`
- `ism330dl`
- `wsen_hids`
- `wsen_pads`
- `bme280`

```cpp
config.setTemperatureCalibration("hts221", 1.0f, -0.5f);

TemperatureCalibration cal;
if (config.getTemperatureCalibration("hts221", cal)) {
    Serial.println(cal.gain);
    Serial.println(cal.offset);
}
```

For drivers that implement the project-standard
`calibrateTemperature(refLow, measLow, refHigh, measHigh)` API:

```cpp
config.applyTemperatureCalibration("hts221", hts);
```

The stored `gain` and `offset` reproduce:

```text
corrected = measured * gain + offset
```

| Method | Description |
|---|---|
| `bool setTemperatureCalibration(const char* sensor, float gain = 1.0f, float offset = 0.0f)` | Store the correction of `sensor`. `false` for an unknown sensor name. |
| `bool getTemperatureCalibration(const char* sensor, TemperatureCalibration& calibration) const` | Copy the stored `{gain, offset}`. `false` for an unknown name or when nothing is stored. |
| `bool clearTemperatureCalibration(const char* sensor)` | Forget the correction of `sensor`. `false` for an unknown sensor name. |
| `bool applyTemperatureCalibration(const char* sensor, Sensor& instance) const` | Pass the stored correction to `instance.calibrateTemperature()`. `false` when nothing is stored. |

### Magnetometer calibration

```cpp
config.setMagnetometerCalibration(
    12.3f, -5.1f, 0.8f,
    1.01f, 0.98f, 1.0f
);

MagnetometerCalibration cal;
if (config.getMagnetometerCalibration(cal)) {
    // cal.hardIronX / Y / Z
    // cal.softIronX / Y / Z
}
```

The storage format matches the MicroPython `LIS2MDL` calibration data.
Application is intentionally left to the LIS2MDL public API so this library
does not depend directly on the magnetometer driver.

| Method | Description |
|---|---|
| `void setMagnetometerCalibration(float hardIronX = 0, float hardIronY = 0, float hardIronZ = 0, float softIronX = 1, float softIronY = 1, float softIronZ = 1)` | Store the hard-iron offsets and soft-iron scales. |
| `bool getMagnetometerCalibration(MagnetometerCalibration& calibration) const` | `false` when nothing is stored. |
| `void clearMagnetometerCalibration()` | Forget the magnetometer calibration. |

### Accelerometer calibration

```cpp
config.setAccelerometerCalibration(0.01f, -0.02f, 0.03f);

AccelerometerCalibration cal;
if (config.getAccelerometerCalibration(cal)) {
    // cal.offsetX / offsetY / offsetZ
}
```

For an IMU exposing `setAccelOffset(x, y, z)`:

```cpp
config.applyAccelerometerCalibration(imu);
```

| Method | Description |
|---|---|
| `void setAccelerometerCalibration(float offsetX = 0, float offsetY = 0, float offsetZ = 0)` | Store the accelerometer offsets, in g. |
| `bool getAccelerometerCalibration(AccelerometerCalibration& calibration) const` | `false` when nothing is stored. |
| `void clearAccelerometerCalibration()` | Forget the accelerometer offsets. |
| `bool applyAccelerometerCalibration(Sensor& instance) const` | Pass the stored offsets to `instance.setAccelOffset()`. `false` when nothing is stored. |

### Boot counter

```cpp
config.setBootCount(0);
config.incrementBootCount();
config.save();

uint32_t count;
if (config.bootCount(count)) {
    Serial.println(count);
}
```

| Method | Description |
|---|---|
| `void setBootCount(uint32_t count)` / `bool bootCount(uint32_t& count) const` | Boot counter; `bootCount()` is `false` when it is not set. |
| `uint32_t incrementBootCount()` | Add one (starting from 0 when not set) and return the new value. Call `save()` to keep it. |

## JSON format

The format is compatible with the MicroPython sister library:

```json
{"rev":131,"name":"STeaMi-01","tc":{"hts":{"g":1.0,"o":-0.5}},"cm":{"hx":12.3,"hy":-5.1,"hz":0.8,"sx":1.01,"sy":0.98,"sz":1.0},"ca":{"ox":0.01,"oy":-0.02,"oz":0.03},"bc":1}
```

| Key | Content |
|---|---|
| `rev` | Board revision code (for example `131` for a V1c) |
| `name` | Board name |
| `tc` | Temperature calibration map |
| `cm` | Magnetometer hard-/soft-iron calibration |
| `ca` | Accelerometer offsets |
| `bc` | Boot counter |

Temperature sensor short keys are `hts`, `mag`, `ism`, `hid`, `pad`, and `bme`.

## Testing

Host-side unit tests under
[`tests/native/test_steami_config/`](../../tests/native/test_steami_config/)
run the module against a model of the DAPLink config zone. Run them without
hardware with:

```bash
make test-native/steami_config
```

On a connected board, `make test-hardware/steami_config` and
`make test-integration/steami_config` write the real config zone. Both save
its content first and write it back at the end, so the board keeps its own
configuration.

## References

* Firmware: [steamicc/DAPLink](https://github.com/steamicc/DAPLink), which holds the config zone.
* MicroPython module: [`steami_config`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/steami_config), same JSON format.

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
