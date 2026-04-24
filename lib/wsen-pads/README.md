# WSEN-PADS

Arduino/C++ driver for the Würth Elektronik WSEN-PADS barometric pressure
and temperature sensor on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x5D` (SAO pin tied high).
* No factory calibration block — conversions use fixed sensitivity constants
  from the datasheet.

## Quick start

```cpp
#include <Wire.h>
#include <WSEN_PADS.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-PADS not detected");
        while (true) delay(1000);
    }

    sensor.setContinuous(ODR_1_HZ);
}

void loop() {
    auto r = sensor.read(false);
    Serial.print(r.pressure);
    Serial.print(" hPa / ");
    Serial.print(r.temperature);
    Serial.println(" C");
    delay(1000);
}
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| | `WSEN_PADS(TwoWire& wire = Wire, uint8_t address = WSEN_PADS_I2C_DEFAULT_ADDR, float temp_gain = 1.0, float temp_offset = 0.0)` | Construct. Defaults to the global `Wire`, address `0x5D`, gain `1.0` and offset `0.0`. | Construct. Defaults to the global `Wire` and address `0x5D`. |
| `bool begin()` | Probe device ID, wait for boot, apply default configuration. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads `DEVICE_ID` (always `0xB3`). |
| `void softReset()` / `void reboot()` | Soft reset or full reboot of internal memory. |
| `void powerOn(uint8_t odr = ODR_1_HZ)` / `void powerOff()` | Start or stop continuous measurement. |

### Reading

If the sensor is in power-down mode when a read is requested, the driver
auto-triggers a one-shot conversion before reading.

| Method | Description |
|--------|-------------|
| `float pressureHpa()` | Pressure in hPa. |
| `float pressurePa()` | Pressure in Pa. |
| `float pressureKpa()` | Pressure in kPa. |
| `float temperature()` | Temperature in °C. |
| `ReadResult read(bool lowNoise = false)` | Both channels — `{pressure, temperature}`. |

### Modes

| Method | Description |
|--------|-------------|
| `bool setContinuous(uint8_t odr, bool lowNoise, bool lowPass, bool lowPassStrong)` | Continuous mode. Pass one of the `ODR_*` constants. |
| `void triggerOneShot(bool lowNoise = false)` | Non-blocking: start a single conversion. |
| `ReadResult readOneShot(bool lowNoise = false)` | Trigger + return result. |

### Filters

| Method | Description |
|--------|-------------|
| `void enableLowPass(bool strong = false)` | Enable LPF2 pressure filter. `strong = true` selects ODR/20 bandwidth instead of ODR/9. |
| `void disableLowPass()` | Disable the LPF2 pressure filter. |

### Calibration

| Method | Description |
|--------|-------------|
| `void setTempOffset(float offset_c)` | Additive °C offset on temperature readings. |
| `bool calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration. Returns `false` if the two measured points are identical. |

## Register constants

`WSEN_PADS_const.h` exports register addresses (`REG_*`), bit masks
(`CTRL1_*`, `CTRL2_*`, `STATUS_*`), ODR values (`ODR_*`), and conversion
constants (`PRESSURE_HPA_PER_DIGIT`, `TEMPERATURE_C_PER_DIGIT`, etc.).

## Testing

Host-side unit tests under `tests/native/test_wsen_pads/` exercise the
driver against the `TwoWire` mock. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).