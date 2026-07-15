# ISM330DL

Arduino/C++ driver for the ST ISM330DL 6-axis inertial measurement unit
(accelerometer + gyroscope) on the STeaMi board.

## Hardware

* I2C sensor
* Default 7-bit address: `0x6B`
* Alternate address: `0x6A`
* 3-axis accelerometer:
  * ±2 g
  * ±4 g
  * ±8 g
  * ±16 g
* 3-axis gyroscope:
  * ±125 dps
  * ±250 dps
  * ±500 dps
  * ±1000 dps
  * ±2000 dps
* Integrated temperature sensor

## Quick start

On the STeaMi board, the ISM330DL is connected to the **internal**
I2C bus (`I2C_INT_SDA` / `I2C_INT_SCL`).

```cpp
#include <Wire.h>
#include <ISM330DL.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL imu(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!imu.begin()) {
        Serial.println("ISM330DL not detected");
        while (true) delay(1000);
    }
}

void loop() {
    ISM330DL::Vector3 accel;
    ISM330DL::Vector3 gyro;
    float temperature;

    imu.accelerationG(accel);
    imu.gyroscopeDps(gyro);
    imu.temperature(temperature);

    Serial.print(accel.x);
    Serial.print(", ");
    Serial.print(accel.y);
    Serial.print(", ");
    Serial.println(accel.z);

    delay(100);
}
```

See
[`examples/read_imu/`](examples/read_imu/)
for the complete example.

---

# Examples

| Example | What it does |
|---------|--------------|
| [`BasicReader`](examples/BasicReader/) | Read acceleration, angular velocity and temperature values from the ISM330DL. |
| [`Orientation`](examples/Orientation/) | Detect the board orientation using the accelerometer. |
| [`Motion`](examples/Motion/) | Detect tilt and rotation direction using the gyroscope. |


## Building an example

```bash
make list-examples
```

Then flash one:

```bash
make flash-ism330dl/Orientation
```

To capture the complete serial output:

```bash
make capture-ism330dl/Orientation
```

---

# API

The driver follows the common STeaMi API conventions.

## Lifecycle

| Method | Description |
|---------|-------------|
| `ISM330DL(TwoWire&, uint8_t)` | Construct the driver. |
| `bool begin()` | Initialize the sensor and verify `WHO_AM_I`. |
| `uint8_t deviceId()` | Read the `WHO_AM_I` register. |
| `bool isConnected()` | Return true if the sensor responds correctly. |
| `bool softReset()` | Reset the device and reload configuration. |

---

## Configuration

### Accelerometer

| Method | Description |
|---------|-------------|
| `configureAccel(AccelOdr, AccelScale)` | Configure output data rate and full scale. |

Supported ODR values

- POWER_DOWN
- 12.5 Hz
- 26 Hz
- 52 Hz
- 104 Hz
- 208 Hz
- 416 Hz
- 833 Hz
- 1660 Hz

Supported full scales

- ±2 g
- ±4 g
- ±8 g
- ±16 g

---

### Gyroscope

| Method | Description |
|---------|-------------|
| `configureGyro(GyroOdr, GyroScale)` | Configure output data rate and full scale. |

Supported ODR values

- POWER_DOWN
- 12.5 Hz
- 26 Hz
- 52 Hz
- 104 Hz
- 208 Hz
- 416 Hz
- 833 Hz
- 1660 Hz

Supported ranges

- ±125 dps
- ±250 dps
- ±500 dps
- ±1000 dps
- ±2000 dps

---

# Measurements

## Raw values

| Method | Description |
|---------|-------------|
| `accelerationRaw()` | Raw accelerometer counts. |
| `gyroscopeRaw()` | Raw gyroscope counts. |
| `temperatureRaw()` | Raw temperature ADC value. |

---

## Converted values

| Method | Unit |
|---------|------|
| `accelerationG()` | g |
| `accelerationMs2()` | m/s² |
| `gyroscopeDps()` | °/s |
| `gyroscopeRads()` | rad/s |
| `temperature()` | °C |

---

## Calibration

| Method | Description |
|---------|-------------|
| `setAccelOffset()` | Apply accelerometer offsets. |
| `accelOffset()` | Read current offsets. |
| `setTemperatureOffset()` | Apply temperature offset. |
| `calibrateTemperature()` | Two-point temperature calibration. |

---

## Orientation

Orientation is estimated from the gravity vector.

Possible values

- SCREEN_UP
- SCREEN_DOWN
- TOP_EDGE_DOWN
- BOTTOM_EDGE_DOWN
- LEFT_EDGE_DOWN
- RIGHT_EDGE_DOWN
- MOVING

Helper

```cpp
orientationToString()
```

returns a printable string.

---

## Motion

Motion detection uses the gyroscope.

Possible values

- STABLE
- TURNING_RIGHT
- TURNING_LEFT
- TILTING_LEFT
- TILTING_RIGHT
- TILTING_UP
- TILTING_DOWN

Helper

```cpp
motionToString()
```

returns a printable string.

---

## Status

| Method | Description |
|---------|-------------|
| `status()` | Raw STATUS register. |
| `dataReady()` | All sensors have fresh data. |
| `accelReady()` | Accelerometer data ready. |
| `gyroReady()` | Gyroscope data ready. |
| `temperatureReady()` | Temperature ready. |

---

## Power management

| Method | Description |
|---------|-------------|
| `powerOn()` | Restore previous configuration. |
| `powerOff()` | Put accelerometer and gyroscope into power-down mode. |

---

## Constants

`ISM330DL_const.h` exports

- register addresses
- bit masks
- conversion constants
- I²C addresses
- WHO_AM_I value

for applications requiring direct register access.

---

# Testing

Hardware tests can be executed with

```bash
make test-hardware
```

Formatting

```bash
make lint
```

Build

```bash
make build
```

---

# License

GPL-3.0-or-later — see `LICENSE`.
