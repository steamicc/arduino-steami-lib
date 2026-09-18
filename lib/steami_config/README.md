# STeaMi Config

Arduino/C++ persistent configuration module for the STeaMi board.

Configuration data is stored as compact JSON in the 1 KB STM32F103 DAPLink
config zone. It is intended for board information, sensor calibration values,
and small persistent counters.

# /!\ current 255-byte read limitation /!\

## Dependency

- `DAPLink Bridge` — config-zone access over the STeaMi internal I2C bus.

## Quick start

```cpp
#include <Wire.h>
#include <DaplinkBridge.h>
#include <SteamiConfig.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
SteamiConfig config(bridge);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!config.begin()) {
        Serial.println("DAPLink bridge not detected");
        return;
    }

    config.setBoardRevision(3);
    config.setBoardName("STeaMi-01");
    config.save();
}

void loop() {}
```

`begin()` probes the DAPLink bridge and loads the stored configuration.
Calling setters only changes the in-memory copy; call `save()` to persist it.

## Persistence

| Method | Description |
|---|---|
| `bool begin()` | Probe DAPLink and load configuration. |
| `bool load()` | Reload the JSON configuration from the config zone. |
| `bool save()` | Erase the config zone and write the current compact JSON. |
| `void clear()` | Clear only the in-memory configuration. |

Invalid JSON is discarded and leaves an empty in-memory configuration.

## Board information

```cpp
config.setBoardRevision(3);
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

## Temperature calibration

Supported sensor names match the MicroPython library:

- `hts221`
- `lis2mdl`
- `ism330dl`
- `wsen_hids`
- `wsen_pads`

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
{"rev":3,"name":"STeaMi-01","tc":{"hts":{"g":1.0,"o":-0.5}},"cm":{"hx":12.3,"hy":-5.1,"hz":0.8,"sx":1.01,"sy":0.98,"sz":1.0},"ca":{"ox":0.01,"oy":-0.02,"oz":0.03},"bc":1}
```

| Key | Content |
|---|---|
| `rev` | Board revision |
| `name` | Board name |
| `tc` | Temperature calibration map |
| `cm` | Magnetometer hard-/soft-iron calibration |
| `ca` | Accelerometer offsets |
| `bc` | Boot counter |

Temperature sensor short keys are `hts`, `mag`, `ism`, `hid`, and `pad`.

## Example

| Example | Description |
|---|---|
| `show_config` | Load and print the stored board information and boot count. |

## License

GPL-3.0-or-later — see the repository `LICENSE`.
