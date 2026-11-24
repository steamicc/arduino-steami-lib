# BME280

Arduino/C++ driver for the Bosch BME280 digital pressure, humidity and
temperature sensor of the STeaMi **V1c** board.

On the V1c, the BME280 replaces the WSEN-PADS (pressure) and WSEN-HIDS
(humidity) sensors of the V1a/V1b boards. On a V1a/V1b board the chip is
absent and `begin()` returns `false`.

## Hardware

* I2C sensor, default 7-bit address `0x76` (SDO tied low on the STeaMi V1c,
  `0x77` when SDO is high: `BME280_ALT_ADDRESS`).
* Chip ID `0x60`.
* Pressure 300 to 1100 hPa, humidity 0 to 100 %RH, temperature -40 to +85 C.
* Factory-calibrated: the driver reads the trimming parameters from the
  chip and applies the integer compensation formulas of the datasheet
  (section 4.2.3), like the MicroPython driver.
* Mounted on the **internal** STeaMi I2C bus.

## Quick start

On the STeaMi board, the BME280 is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the board variant, not the default
global `Wire`). Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <SteamiBME280.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiBME280 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("BME280 not detected");
        while (true) delay(1000);
    }
}

void loop() {
    auto r = sensor.read();
    Serial.print(r.temperature);
    Serial.print(" C / ");
    Serial.print(r.humidity);
    Serial.print(" % / ");
    Serial.print(r.pressure);
    Serial.println(" hPa");
    delay(1000);
}
```

With the `STeaMi` Library Manager package, `#include <SteamiBME280.h>` (or
`#include <STeaMi.h>`) is enough.

The header and the class are called `SteamiBME280`, not `BME280`, on purpose:
the "BME280" library by Tyler Glenn (and others) installs its own `BME280.h`
with an unrelated `class BME280`. Many school computers already have it, and
with both installed `#include <BME280.h>` would pick that library and fail with
errors that never mention STeaMi. If you see errors about an abstract
`BME280` class or a `BME280(TwoWire&)` constructor, check that the sketch
includes `<SteamiBME280.h>` and uses `SteamiBME280`.

## Examples

| Example                                        | What it does                                                                                  |
| ---------------------------------------------- | --------------------------------------------------------------------------------------------- |
| [`BasicReader`](examples/BasicReader/)         | Baseline sketch: print temperature, humidity and pressure every second.                       |
| [`Altitude`](examples/Altitude/)               | Estimate the altitude (absolute and since power-on) from the pressure, with filtering.        |
| [`DewPointMonitor`](examples/DewPointMonitor/) | Compute the dew point from T and RH, warn when condensation risk is high (T - Td < 2 C).      |
| [`ComfortZone`](examples/ComfortZone/)         | Classify the room as comfortable / too dry / too humid / too cold / too hot.                  |
| [`WeatherStation`](examples/WeatherStation/)   | Continuous (normal mode) monitoring with pressure trend.                                      |

### Building an example

```bash
make list-examples DRIVER=bme280
make flash-bme280/BasicReader
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-bme280/BasicReader DURATION=30
```

## API

All methods follow the collection conventions: `camelCase`, units in method
names only when they carry ambiguity, no `read` / `get` prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiBME280(TwoWire& wire = Wire, uint8_t address = BME280_DEFAULT_ADDRESS)` | Construct. Defaults to the global `Wire` and address `0x76`. |
| `bool begin()` | Probe the chip ID, wait for the NVM copy, load the factory calibration and apply the default configuration (oversampling x1, filter off, standby 0.5 ms, sleep mode). Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads the chip ID register (always `0x60`). |
| `void softReset()` | Software reset, then reloads the calibration and re-applies the default configuration. |
| `void powerOn()` / `void powerOff()` | Normal mode (continuous conversions) / sleep mode. |

### Reading

If the chip is in sleep mode when a read is requested (the state after
`begin()` and `powerOff()`), the driver triggers one forced conversion, waits
for it (bounded timeout) and returns the result. Callers do not have to
manage modes to read a value. On timeout, on an I2C error (module missing or
bus fault) and when `begin()` has not succeeded, the methods return `NAN`, so
`isnan()` catches every failure. A channel whose oversampling is
`BME280_OSRS_SKIP` also reads as `NAN`.

| Method | Description |
|--------|-------------|
| `float temperature()` | Celsius, with the user calibration applied. |
| `float humidity()` | %RH, clamped to `[0, 100]`. |
| `float pressureHpa()` | hPa. |
| `ReadResult read()` | All channels from a single conversion: `{temperature, humidity, pressure}`. |
| `bool dataReady()` | After `triggerOneShot()`: `true` once that conversion is over (nominal time elapsed, chip back to sleep, `STATUS.measuring` clear). Otherwise `true` when no conversion is in progress. The BME280 has no "new sample" flag, so in normal mode this does not mean a new sample arrived since the last read: pace the loop with `delay()` instead. |
| `bool temperatureReady()` / `pressureReady()` / `humidityReady()` | Same as `dataReady()`: the BME280 has a single flag for all channels. |

### Modes

| Method | Description |
|--------|-------------|
| `void setContinuous(uint8_t standby = BME280_STANDBY_1000_MS)` | Normal mode with the given standby time between conversions (`BME280_STANDBY_0_5_MS` ... `BME280_STANDBY_20_MS`). |
| `void triggerOneShot()` | Non-blocking: start a forced conversion. Poll `dataReady()`, then call `read()`: it uses this conversion instead of starting another one, and waits for it if it is not over yet. |
| `ReadResult readOneShot()` | Trigger + wait + return. |

### Configuration

| Method | Description |
|--------|-------------|
| `void setOversampling(uint8_t t, uint8_t p, uint8_t h)` | Oversampling per channel, `BME280_OSRS_SKIP` ... `BME280_OSRS_X16`; `BME280_OSRS_KEEP` leaves a channel unchanged. |
| `void setIirFilter(uint8_t coefficient)` | `BME280_FILTER_OFF` ... `BME280_FILTER_16`. |
| `void setStandby(uint8_t standby)` | Standby time used in normal mode. |
| `uint32_t measurementTimeMs()` | Worst-case conversion time for the current oversampling (datasheet section 9.1), rounded up. |

Changing the filter or the standby time while the chip is in normal mode is
safe: the driver drops to sleep mode for the write (the chip may ignore
`CONFIG` writes otherwise) and restores normal mode afterwards.

### Derived quantities

| Method | Description |
|--------|-------------|
| `float altitude(float seaLevelHpa = BME280_SEA_LEVEL_HPA)` | Altitude in meters from a fresh pressure reading (international barometric formula). Give the local sea-level pressure for a good estimate. |
| `float dewPoint()` | Dew point in Celsius (Magnus formula) from a fresh reading. |
| `static float altitudeFromPressure(float pressureHpa, float seaLevelHpa = BME280_SEA_LEVEL_HPA)` | Same formulas on a value you already have, without a new reading. |
| `static float dewPointFrom(float temperatureC, float humidityPct)` | Same, from a `read()` result. |

### Calibration

| Method | Description |
|--------|-------------|
| `void setTemperatureOffset(float offset)` | Additive Celsius offset on top of the factory compensation. |
| `void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration. Applied after the factory compensation. Pressure and humidity are not affected. |

## Register constants

`SteamiBME280_const.h` exports register addresses (`BME280_REG_*`), bit masks
(`BME280_STATUS_*`, `BME280_MODE_*`), and the oversampling, filter and
standby values (`BME280_OSRS_*`, `BME280_FILTER_*`, `BME280_STANDBY_*`).

## Testing

```bash
make test-native/bme280   # compensation math, register sequences, mode logic
make build
make lint
```

The native suite checks the compensation formulas against reference vectors
(the Bosch example calibration, with values computed from a Python port of
the MicroPython driver). The hardware suite in `tests/hardware/test_bme280/`
needs a STeaMi V1c.

## License

GPL-3.0-or-later - see [LICENSE](../../LICENSE).
