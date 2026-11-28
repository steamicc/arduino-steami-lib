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
| [`calibrate_temperature`](examples/calibrate_temperature/) | Align every temperature sensor on the reference of the release (BME280 on a V1c, WSEN-HIDS on a V1a/V1b) and save the offsets (`tc`). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_temperature) |
| [`calibrate_accelerometer`](examples/calibrate_accelerometer/) | Measure the ISM330DL bias with the board flat and save it (`ca`). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_accelerometer) |
| [`calibrate_magnetometer`](examples/calibrate_magnetometer/) | Press MENU, turn the board for 12 s, save the LIS2MDL hard and soft iron (`cm`) and check the heading. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_magnetometer) |

The three `calibrate_*` examples write the config zone. When the zone is read,
they keep the other stored settings (they `load()` the zone before saving). An
empty read is tried again, and refused when the board found settings in the
zone at startup, since a failed read looks like an empty zone. They mirror the
examples of the same name in the MicroPython `steami_config` library, so a
board calibrated in one language reads the same in the other.

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
| `bool isEmpty() const` | True when no value is set (no board information, calibration or boot count). |

Invalid JSON is discarded: `load()` returns `false` and leaves an empty
in-memory configuration.

A failed read of the first block of the zone (DAPLink busy, I2C error) also
gives an empty configuration, with `load()` returning `true`: it looks like an
empty zone, and a `save()` that follows would erase the stored settings. When
`isEmpty()` is true after `load()` on a board that should hold a
configuration, read the zone again before saving.

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

- `lis2mdl`
- `ism330dl`
- `wsen_hids`
- `wsen_pads`
- `bme280`
- `hts221`: legacy entry. The HTS221 and the WSEN-HIDS are the same chip at
  0x5F, and older tools stored the WSEN-HIDS calibration under this name.
  The STeaMi Board library still reads it when `wsen_hids` is missing or is
  the identity, as MicroPython does. Do not write it: the
  `calibrate_temperature` example removes it.

```cpp
config.setTemperatureCalibration("wsen_hids", 1.0f, -0.5f);

TemperatureCalibration cal;
if (config.getTemperatureCalibration("wsen_hids", cal)) {
    Serial.println(cal.gain);
    Serial.println(cal.offset);
}
```

For drivers that implement the project-standard
`calibrateTemperature(refLow, measLow, refHigh, measHigh)` API:

```cpp
config.applyTemperatureCalibration("wsen_hids", hids);
```

This replaces any correction the driver had, `setTemperatureOffset()`
included.

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
{"rev":131,"name":"STeaMi-01","tc":{"hid":{"g":1.0,"o":-0.5}},"cm":{"hx":12.3,"hy":-5.1,"hz":0.8,"sx":1.01,"sy":0.98,"sz":1.0},"ca":{"ox":0.01,"oy":-0.02,"oz":0.03},"bc":1}
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
