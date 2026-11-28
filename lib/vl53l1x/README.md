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
global `Wire`).
The board owns that bus: build the driver on `Steami.wire()`, which starts it on first use:

```cpp
#include <SteamiBoard.h>
#include <SteamiVL53L1X.h>

SteamiVL53L1X sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);

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

| Example | What it does | Try it |
|---------|--------------|--------|
| [`BasicDistance`](examples/BasicDistance/) | Print the measured distance in millimetres to the Serial Monitor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x#basicdistance) |
| [`ProximityAlarm`](examples/ProximityAlarm/) | Sound the buzzer while an object is closer than a configurable threshold. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x#proximityalarm) |
| [`ParkingSensor`](examples/ParkingSensor/) | Beep faster as an object gets closer, like a car parking sensor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x#parkingsensor) |
| [`ObjectCounter`](examples/ObjectCounter/) | Measure the background distance, then count each object passing in front of the sensor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x#objectcounter) |
| [`GestureDetection`](examples/GestureDetection/) | Toggle the LED when a hand waves quickly in front of the sensor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x#gesturedetection) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/vl53l1x).

### Building an example

```bash
make list-examples DRIVER=vl53l1x
make flash-vl53l1x/BasicDistance
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-vl53l1x/BasicDistance DURATION=30
```

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

### Default configuration

The driver uploads the ST recommended default configuration block during
`begin()` using the `DEFAULT_CONFIG` table embedded in the driver.

This includes:

* Interrupt configuration
* ROI configuration
* Sigma thresholds
* Intermeasurement periods
* VCSEL timing parameters

## Register constants

`SteamiVL53L1X_const.h` exports all public register definitions and bit masks,
including:

* Device identification registers
* Ranging control registers
* Interrupt status bits
* Result block offsets
* Default I2C address

Applications can directly access low-level registers if needed.

## Testing

Host-side unit tests under
[`tests/native/test_vl53l1x/`](../../tests/native/test_vl53l1x/) exercise the
driver against the `TwoWire` mock. Run them without hardware with:

```bash
make test-native/vl53l1x
```

On a connected board, `make test-hardware/vl53l1x` checks the real sensor.

## References

* Datasheet: [ST VL53L1X](https://www.st.com/resource/en/datasheet/vl53l1x.pdf).
* STeaMi wiki: [VL53L1X hardware page](https://wiki.steami.cc/docs/hardware/main-components/sensors/vl53l1x).
* MicroPython driver: [`vl53l1x`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/vl53l1x).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
