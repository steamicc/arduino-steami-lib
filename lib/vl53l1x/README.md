# VL53L1X

Arduino/C++ driver for the ST `VL53L1X` Time-of-Flight distance sensor.

The driver provides a lightweight API for initializing the sensor,
starting ranging operations, and reading distance measurements over I2C.

## Hardware

* I2C Time-of-Flight distance sensor
* Default 7-bit I2C address: `0x29`
* Distance measurements returned in millimeters
* Based on the ST default configuration sequence

## Quick start

On the STeaMi board, the VL53L1X is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the variant, not the default
global `Wire`). Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <Wire.h>
#include <SteamiVL53L1X.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiVL53L1X sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected");
        while (true) delay(1000);
    }

    sensor.startRanging();
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t distance = sensor.read();

        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
    }

    delay(50);
}
```

## Examples

No example sketch has been merged for this driver yet; practical examples
are tracked in [#194](https://github.com/steamicc/arduino-steami-lib/issues/194).
Once they land, `make list-examples DRIVER=vl53l1x` lists the ready-to-run
`flash-vl53l1x/<example>` targets.

## API

All methods follow the collection conventions: `camelCase`, explicit
units where relevant, and minimal redundant prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiVL53L1X(TwoWire& wire = Wire, uint8_t address = VL53L1X_I2C_DEFAULT_ADDR)` | Construct a driver instance. |
| `bool begin()` | Reset the sensor, verify the device ID, and upload the default configuration. Returns `false` if the sensor is not detected. |
| `uint16_t deviceId()` | Read the device model ID register. |
| `void reset()` | Perform a software reset. |
| `void powerOff()` | Put the sensor into reset state. |
| `void powerOn()` | Release reset and power the device back on. |

### Ranging

| Method | Description |
|--------|-------------|
| `void startRanging()` | Start ranging measurements. |
| `void stopRanging()` | Stop ranging measurements. |
| `bool dataReady()` | Check whether a new measurement is available. |
| `uint16_t distanceMm()` | Read the measured distance in millimeters. |
| `uint16_t read()` | Alias for `distanceMm()`. |

### Internal behavior

When `distanceMm()` is called and no data is available yet, the driver:

1. Starts ranging automatically
2. Polls the sensor until data becomes available
3. Reads the result block
4. Clears the interrupt flag
5. Returns the measured distance

The polling timeout is approximately 1 second.

## Register constants

`SteamiVL53L1X_const.h` exports all public register definitions and bit masks,
including:

* Device identification registers
* Ranging control registers
* Interrupt status bits
* Result block offsets
* Default I2C address

Applications can directly access low-level registers if needed.

## Sensor configuration

The driver uploads the ST recommended default configuration block during
`begin()` using the `DEFAULT_CONFIG` table embedded in the driver.

This includes:

* Interrupt configuration
* ROI configuration
* Sigma thresholds
* Intermeasurement periods
* VCSEL timing parameters

## Testing

The driver is designed to work with mocked `TwoWire` implementations for
host-side testing.

Example unit tests can be added under:

```text
tests/native/test_vl53l1x/
```

and executed with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).