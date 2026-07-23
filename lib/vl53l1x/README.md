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

```cpp
#include <Wire.h>
#include <VL53L1X.h>

VL53L1X sensor;

void setup() {
    Serial.begin(115200);
    Wire.begin();

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

See [examples/basic_distance/](examples/basic_distance/) for the complete
example sketch.

## Examples

| Example | What it does |
|---------|--------------|
| [`BasicDistance`](examples/basic_distance/) | Continuously measures and prints the distance in millimeters to the serial monitor. |
| [`ProximityAlarm`](examples/proximity_alarm/) | Buzzes the on-board `SPEAKER` when an object comes closer than a configurable distance threshold. |
| [`ParkingSensor`](examples/parking_sensor/) | Simulates a parking sensor by increasing the beep rate as an object gets closer. |

### Building an example

List available examples:

```bash
make list-examples
```

Flash an example:

```bash
make flash-vl53l1x/BasicDistance
```

Capture boot logs reliably:

```bash
make capture-vl53l1x/BasicDistance
```

Longer capture session:

```bash
make capture-vl53l1x/BasicDistance DURATION=30
```

## API

All methods follow the collection conventions: `camelCase`, explicit
units where relevant, and minimal redundant prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `VL53L1X(TwoWire& wire = Wire, uint8_t address = VL53L1X_I2C_DEFAULT_ADDR)` | Construct a driver instance. |
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

`VL53L1X_const.h` exports all public register definitions and bit masks,
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
