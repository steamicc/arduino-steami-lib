# HTS221

Arduino/C++ driver for the ST HTS221 capacitive digital humidity and
temperature sensor on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x5F` (SA0 pulled low on the STeaMi
  board).
* Factory-calibrated — the driver loads the calibration block from the
  part itself and applies it on every reading.

## Quick start

On the STeaMi board, the HTS221 is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the variant, not the default
global `Wire`). Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <Wire.h>
#include <HTS221.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("HTS221 not detected");
        while (true) delay(1000);
    }

    sensor.setContinuous(HTS221_ODR_1_HZ);
}

void loop() {
    if (sensor.dataReady()) {
        auto r = sensor.read();
        Serial.print(r.temperature);
        Serial.print(" C / ");
        Serial.print(r.humidity);
        Serial.println(" %");
    }
    delay(100);
}
```

See [examples/read_temperature_humidity/](examples/read_temperature_humidity/)
for the full sketch.

## Examples

| Example | What it does |
|---------|--------------|
| [`read_temperature_humidity`](examples/read_temperature_humidity/) | Baseline sketch: print temperature and humidity every second. |
| [`comfort_index`](examples/comfort_index/) | Classify ambient conditions as *comfortable*, *too dry*, *too humid*, *cold*, or *hot* using simple thresholds. |
| [`dew_point`](examples/dew_point/) | Compute the dew point from T + %RH via the Magnus formula, and warn when the current temperature comes within 2 °C of it (condensation risk). |
| [`temperature_alarm`](examples/temperature_alarm/) | Monitor temperature continuously and buzz the on-board `SPEAKER` whenever it crosses a configurable high threshold. |

### Building an example

List available examples:

```bash
make list-examples
```

Then flash one:

```bash
make flash EXAMPLE=hts221/dew_point
```

This builds, uploads, and opens the serial monitor at 115200 baud.

## API

All methods follow the collection conventions: `camelCase`, include
units in method names only when they carry ambiguity, and skip
redundant `read` / `get` prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `HTS221(TwoWire& wire = Wire, uint8_t address = HTS221_DEFAULT_ADDRESS)` | Construct. Defaults to the global `Wire` and address `0x5F`. |
| `bool begin()` | Probe `WHO_AM_I`, load factory calibration, leave the part powered down. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads `WHO_AM_I` (always `0xBC`). |
| `void softReset()` / `void reboot()` | Reload factory trimming via `CTRL2.BOOT`. |
| `void powerOn()` / `void powerOff()` | Toggle `CTRL1.PD`. |

### Reading

If the part is powered down when a read is requested, the driver auto-
triggers a one-shot measurement, polls `dataReady()` with a timeout, and
returns the result. The caller doesn't have to manage modes manually.

| Method | Description |
|--------|-------------|
| `float temperature()` | Celsius. |
| `float humidity()` | %RH, clamped to `[0, 100]`. |
| `ReadResult read()` | Both channels — `{temperature, humidity}`. |
| `bool dataReady()` | Both `H_DA` and `T_DA` set in `STATUS_REG`. |
| `bool temperatureReady()` / `bool humidityReady()` | Per-channel readiness. |

### Modes

| Method | Description |
|--------|-------------|
| `void setContinuous(uint8_t odr)` | Continuous mode. Pass `HTS221_ODR_1_HZ`, `_7_HZ`, or `_12_5_HZ`. |
| `void triggerOneShot()` | Non-blocking: start a single conversion. |
| `ReadResult readOneShot()` | Trigger + wait + return. |

### Calibration

| Method | Description |
|--------|-------------|
| `void setTemperatureOffset(float offset)` | Additive Celsius offset on top of the factory calibration. |
| `void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration. Applied after the factory curve. |

## Register constants

`HTS221_const.h` exports register addresses (`HTS221_REG_*`), bit masks
(`HTS221_CTRL1_*`, `HTS221_STATUS_*`), and ODR values (`HTS221_ODR_*`) so
applications can poke the part directly if they need something outside
the driver's API surface.

## Testing

Host-side unit tests under [`tests/native/test_hts221/`](../../tests/native/test_hts221/)
exercise the driver against the `TwoWire` mock from
`tests/native/helpers/Wire.h`. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
