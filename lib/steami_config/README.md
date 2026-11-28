# STeaMi Config

Arduino/C++ persistent configuration module for the STeaMi board.

Configuration data is stored as compact JSON in the 1 KB STM32F103 DAPLink
config zone. It is intended for board information, sensor calibration values,
and small persistent counters.

## DAPLink firmware

The config zone needs a recent DAPLink firmware on the board. Older
firmwares lock up when they receive a config command, so the driver checks
for the zone first (`SteamiDaplinkBridge::hasConfigZone()`): on such a board,
`begin()`, `load()` and `save()` return `false` without touching it.

## Dependency

- `STeaMi DAPLink Bridge` — config-zone access over the STeaMi internal I2C
  bus.

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

## Persistence

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

## Board information

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

### Revision code

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

## Temperature calibration

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

## Magnetometer calibration

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

## Accelerometer calibration

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

## Boot counter

```cpp
config.setBootCount(0);
config.incrementBootCount();
config.save();

uint32_t count;
if (config.bootCount(count)) {
    Serial.println(count);
}
```

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

## Examples

| Example | What it does | Try it |
|---------|--------------|--------|
| [`show_config`](examples/show_config/) | Load and print the stored board information, calibrations and boot count, without writing anything. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#show_config) |
| [`calibrate_temperature`](examples/calibrate_temperature/) | Align every temperature sensor on the reference of the release (BME280 on a V1c, WSEN-HIDS on a V1a/V1b) and save the offsets (`tc`). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_temperature) |
| [`calibrate_accelerometer`](examples/calibrate_accelerometer/) | Measure the ISM330DL bias with the board flat and save it (`ca`). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_accelerometer) |
| [`calibrate_magnetometer`](examples/calibrate_magnetometer/) | Press MENU, turn the board for 12 s, save the LIS2MDL hard and soft iron (`cm`) and check the heading. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config#calibrate_magnetometer) |

The three `calibrate_*` examples write the config zone. They keep the other
stored settings (they `load()` the zone before saving) and mirror the examples of the
same name in the MicroPython `steami_config` library, so a board calibrated
in one language reads the same in the other.

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/steami-config).

## License

GPL-3.0-or-later — see the repository `LICENSE`.
