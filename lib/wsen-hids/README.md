# WSEN-HIDS

Arduino/C++ driver for the Würth Elektronik WSEN-HIDS digital humidity
and temperature sensor on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x5F`.
* Factory-calibrated humidity and temperature MEMS environmental sensor.
* Mounted on the **internal** STeaMi I2C bus.

## Quick start

On the STeaMi board, the WSEN-HIDS is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the board variant, not the
default global `Wire`). Spin up a dedicated `TwoWire` and hand it to the
driver:

```cpp
#include <Wire.h>
#include <WsenHids.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-HIDS not detected");
        while (true) delay(1000);
    }

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
}

void loop() {
    if (sensor.dataReady()) {
        float t = sensor.temperature();
        float h = sensor.humidity();

        Serial.print(t);
        Serial.print(" C / ");
        Serial.print(h);
        Serial.println(" %");
    }

    delay(100);
}
```

See [examples/BasicRead/](examples/BasicRead/) for the full sketch.

## Examples

| Example                            | What it does                                                  |
| ---------------------------------- | ------------------------------------------------------------- |
| `BasicReader` | Baseline sketch: print temperature and humidity every second. |
| `HumidityAlert/HumidityAlert.ino` | Trigger buzzer when humidity exceeds a threshold (mold prevention) |
| `DewPointMonitor/DewPointMonitor.ino` | Calculate and display dew point, warn when condensation risk is high |
| `ComfortZone/ComfortZone.ino` | Classify environment as comfortable/dry/humid using temperature-humidity chart |

### Building an example

List available examples (each line is a runnable Make target):

```bash
make list-examples
```

Then flash one — copy a line from the listing:

```bash
make flash-wsen-hids/BasicReader
```

This builds, uploads, and opens the serial monitor at 115200 baud.

To reliably capture the first lines printed at boot (which the interactive monitor often misses), swap `flash-` for `capture-`:

```bash
make capture-wsen-hids/BasicReader
make capture-wsen-hids/BasicReader DURATION=30
```

## API

All methods follow the collection conventions: `camelCase`, minimal
surface, and explicit environmental units.

### Lifecycle

| Method                                                                        | Description                                                                                                    |
| ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `WsenHids(TwoWire& wire = Wire, uint8_t address = WSEN_HIDS_DEFAULT_ADDRESS)` | Construct. Defaults to the global `Wire` and address `0x5F`.                                                   |
| `bool begin()`                                                                | Probe `WHO_AM_I`, initialize the sensor, leave it powered down. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()`                                                          | Reads `WHO_AM_I` (always `0xBC`).                                                                              |
| `void softReset()`                                                            | Software reset of the device.                                                                                  |
| `void powerOn()` / `void powerOff()`                                          | Toggle measurement engine power state.                                                                         |

### Reading

The WSEN-HIDS can operate either in one-shot mode or in continuous
background conversion mode.

| Method                                             | Description                                   |
| -------------------------------------------------- | --------------------------------------------- |
| `float temperature()`                              | Celsius.                                      |
| `float humidity()`                                 | Relative humidity in `%RH`.                   |
| `uint8_t status()`                                 | Raw status register.                          |
| `bool dataReady()`                                 | Both humidity and temperature data available. |
| `bool temperatureReady()` / `bool humidityReady()` | Per-channel readiness.                        |

### Modes

| Method                                   | Description                                                         |
| ---------------------------------------- | ------------------------------------------------------------------- |
| `void setContinuous(uint8_t odr)`        | Continuous mode. Pass `WSEN_HIDS_ODR_1_HZ`, `_7_HZ`, or `_12_5_HZ`. |
| `void triggerOneShot()`                  | Non-blocking: start a single conversion.                            |
| `std::tuple<float, float> readOneShot()` | Trigger + wait + return `{temperature, humidity}`.                  |

## Register constants

`WsenHids_const.h` exports register addresses (`WSEN_HIDS_REG_*`), bit
masks, and ODR values (`WSEN_HIDS_ODR_*`) so applications can poke the
part directly if they need functionality outside the driver's API
surface.

## Testing

The driver should be validated both in build CI and on real STeaMi
hardware.

Build-only verification:

```bash
make build
```

Formatting:

```bash
make lint
```

Manual hardware validation can be done with the provided `BasicRead`
example to confirm:

* successful `begin()`
* correct `deviceId()`
* plausible temperature values
* plausible humidity values
* continuous acquisition readiness

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
