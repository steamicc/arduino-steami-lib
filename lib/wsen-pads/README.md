# WSEN-PADS

Arduino/C++ driver for the Würth Elektronik WSEN-PADS barometric pressure
and temperature sensor on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x5D` (SAO pin tied high). The alternate
  `0x5C` address is exposed as `WSEN_PADS_I2C_ADDR_SAO_LOW`.
* No factory calibration block — conversions use the fixed sensitivity
  constants from the datasheet (1 LSB = 1/4096 hPa for pressure, 0.01 °C for
  temperature).

## Quick start

On the STeaMi board, the WSEN-PADS is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the variant, not the default global
`Wire`). Spin up a dedicated `TwoWire` and hand it to the driver:

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
    auto r = sensor.read();
    Serial.print(r.pressure);
    Serial.print(" hPa / ");
    Serial.print(r.temperature);
    Serial.println(" C");
    delay(1000);
}
```

## API

All methods follow the library conventions: `camelCase`, units in method
names only when ambiguous, no `read` / `get` prefix, `read()` for the
combined struct reading.

### Lifecycle

| Method | Description |
|--------|-------------|
| `WSEN_PADS(TwoWire& wire = Wire, uint8_t address = WSEN_PADS_I2C_DEFAULT_ADDR)` | Construct. Defaults to the global `Wire` and address `0x5D`. |
| `bool begin()` | Wait for boot, probe `DEVICE_ID`, apply default config (BDU + auto-increment, ODR=power-down). Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads `DEVICE_ID` (always `0xB3`). |
| `void softReset()` / `void reboot()` | Software reset or memory reboot via `CTRL2.SWRESET` / `CTRL2.BOOT`. Both re-apply the default config afterwards. |
| `void powerOn()` / `void powerOff()` | Start or stop continuous measurement. `powerOn()` enters continuous mode at `ODR_1_HZ` — use `setContinuous(...)` for any other rate. |

### Reading

If the part is in power-down when a read is requested, the driver auto-
triggers a one-shot, polls `dataReady()` with a timeout, and returns the
result. The caller doesn't have to manage modes manually. On timeout the
read returns `NaN` so silent stale readings can't propagate; callers can
check with `isnan()`.

| Method | Description |
|--------|-------------|
| `float pressureHpa()` | Pressure in hPa. |
| `float temperature()` | Temperature in °C, with two-point calibration and additive offset applied. |
| `ReadResult read()` | Both channels — `{pressure, temperature}`. |
| `bool dataReady()` | Both `P_DA` and `T_DA` set in `STATUS`. |
| `bool pressureReady()` / `bool temperatureReady()` | Per-channel readiness. |

### Modes

| Method | Description |
|--------|-------------|
| `void setContinuous(uint8_t odr)` | Continuous mode. Pass one of the `ODR_*` constants (1 / 10 / 25 / 50 / 75 / 100 / 200 Hz). |
| `void triggerOneShot()` | Non-blocking: power-down + arm `CTRL2.ONE_SHOT`. Pair with `dataReady()` polling, or just call `readOneShot()`. |
| `ReadResult readOneShot()` | Trigger + wait for `dataReady` + read + return. Returns `{NaN, NaN}` on timeout. |

### Calibration

`setTemperatureOffset()` and `calibrateTemperature()` are independent — the
offset stacks on top of the two-point user slope/intercept, so calling one
does not reset the other.

| Method | Description |
|--------|-------------|
| `void setTemperatureOffset(float offset)` | Additive °C offset on the final temperature reading. |
| `void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration. Sets the slope/intercept used after the factory conversion. A degenerate two-point input (`measLow == measHigh`) leaves the calibration at identity. |

### Driver-specific tuning

| Method | Description |
|--------|-------------|
| `void enableLowPass(bool strong = false)` | Enable the LPF2 pressure filter. `strong = true` selects ODR/20 bandwidth, otherwise ODR/9. |
| `void disableLowPass()` | Disable the LPF2 pressure filter. |
| `void enableLowNoise()` / `void disableLowNoise()` | Toggle `CTRL2.LOW_NOISE_EN`. Per the datasheet, must only be modified while in power-down (call `powerOff()` first if continuous mode is running). Not allowed at 100 Hz / 200 Hz. |

## Examples

| Example | What it does |
|---------|--------------|
| [`read_pressure_temperature`](examples/read_pressure_temperature/) | Baseline sketch: print pressure (hPa) and temperature (°C) every second. |
| [`altitude`](examples/altitude/) | Estimate altitude from pressure using the international standard atmosphere model. Tunable sea-level reference for local QNH calibration. |
| [`floor_detector`](examples/floor_detector/) | Calibrate a reference altitude over 10 samples, then report floor changes as the carrier moves. Hysteresis margin avoids oscillating on a boundary. |

Flash one with:

```bash
make flash-wsen-pads/altitude
```

## Register constants

`WSEN_PADS_const.h` exports register addresses (`REG_*`), bit masks
(`CTRL1_*`, `CTRL2_*`, `STATUS_*`, `INT_SOURCE_*`), ODR values (`ODR_*`),
and conversion constants (`PRESSURE_HPA_PER_DIGIT`,
`TEMPERATURE_C_PER_DIGIT`, etc.) so applications can poke the part
directly if they need something outside the driver's API surface.

## Testing

Host-side unit tests under
[`tests/native/test_wsen_pads/`](../../tests/native/test_wsen_pads/) exercise
the driver against the `TwoWire` mock and the shared
[`driver_checks.h`](../../tests/shared/driver_checks.h) helpers. Run them
without hardware with:

```bash
make test-native/wsen_pads
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
