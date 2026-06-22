# LIS2MDL

Arduino/C++ driver for the ST LIS2MDL 3-axis magnetometer on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x1E`.
* Fixed ±50 gauss full scale, 1.5 mG/LSB (0.15 µT/LSB).
* Output Data Rate: 10, 20, 50, or 100 Hz.

## Quick start

On the STeaMi board, the LIS2MDL is routed to the **internal** I2C bus
(pins `I2C_INT_SDA` / `I2C_INT_SCL` from the variant, not the default
global `Wire`). Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <Wire.h>
#include <LIS2MDL.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("LIS2MDL not detected");
        while (true) delay(1000);
    }

    sensor.setContinuous(10);
}

void loop() {
    if (sensor.dataReady()) {
        MagneticField f = sensor.magneticField();
        Serial.print(f.x); Serial.print(" ");
        Serial.print(f.y); Serial.print(" ");
        Serial.println(f.z);
    }
    delay(100);
}
```

See [examples/read_magnetic_field/](examples/read_magnetic_field/)
for the full sketch.

## Examples

| Example | What it does |
|---------|--------------|
| [`read_magnetic_field`](examples/read_magnetic_field/) | Print raw X/Y/Z magnetic field values at 10 Hz. |
| [`compass`](examples/compass/) | Flat 2D compass heading with min/max calibration and cardinal direction label. |
| [`magnitude`](examples/magnitude/) | Print total field strength in µT — useful to detect nearby magnets or metallic objects. |
| [`tilt_compensated_compass`](examples/tilt_compensated_compass/) | Heading with tilt compensation using an external accelerometer. |

### Building an example

List available examples:

```bash
make list-examples
```

Then flash one:

```bash
make flash-lis2mdl/compass
```

To capture the first lines printed at boot:

```bash
make capture-lis2mdl/compass
make capture-lis2mdl/compass DURATION=30
```

## API

All methods follow the collection conventions: `camelCase`, unit suffix
in method names when they carry ambiguity, no `read` / `get` prefix.

### Lifecycle

| Method | Description |
|--------|-------------|
| `LIS2MDL(TwoWire& wire = Wire, uint8_t address = LIS2MDL_I2C_ADDR, uint8_t odrHz = 10, bool tempComp = true, bool lowPower = false, bool drdyEnable = false)` | Construct. Defaults to global `Wire`, address `0x1E`, 10 Hz ODR, temperature compensation enabled. |
| `bool begin()` | Probe `WHO_AM_I`, configure the sensor, leave it in continuous mode. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads `WHO_AM_I` (always `0x40`). |
| `void softReset(uint16_t waitMs = 10)` | Software reset via `SOFT_RST` bit in `CFG_REG_A`. |
| `void reboot(uint16_t waitMs = 10)` | Reload internal registers via `REBOOT` bit in `CFG_REG_A`. |
| `void powerOn(const char* mode = "continuous")` | Exit idle mode. Pass `"continuous"` or `"single"`. |
| `void powerOff()` | Switch to idle mode (low power). |
| `bool isIdle()` | Returns `true` if `MD1..0 == 0b11`. |
| `const char* getMode()` | Returns `"continuous"`, `"single"`, or `"idle"`. |

### Reading

If the sensor is idle when a read is requested, the driver automatically
triggers a one-shot conversion, polls `dataReady()` with a bounded timeout,
and returns the result. The caller does not have to manage modes manually.

| Method | Return type | Description |
|--------|-------------|-------------|
| `magneticField()` | `MagneticField` (`Vec3i`) | Raw X/Y/Z in LSB (int16). |
| `magneticFieldRaw()` | `MagneticField` | Alias for `magneticField()`. |
| `magneticFieldUt()` | `MagneticFieldUt` (`Vec3f`) | Field in µT (0.15 µT/LSB). |
| `calibratedField()` | `CalibratedField` (`Vec3f`) | Offset/scale corrected, unitless. |
| `magnitudeUt()` | `float` | Total field strength in µT. |
| `readAll()` | `ReadAll` | All channels in one call: `raw`, `ut`, `cal`, `tempC`, `status`. |
| `dataReady()` | `bool` | `true` when a fresh XYZ triplet is available (Zyxda bit). |
| `status()` | `uint8_t` | Raw `STATUS_REG` value. |
| `readIntSource()` | `uint8_t` | Raw `INT_SOURCE_REG` value. |

### Temperature

The LIS2MDL embeds a temperature sensor intended for internal magnetic
compensation. It has **no guaranteed absolute offset** (see datasheet
Table 4) — the driver applies an empirical 25 °C base offset confirmed
by Zephyr RTOS PR #35912. Use `setTempOffset()` or `calibrateTemperature()`
to adjust against a reference thermometer.

| Method | Description |
|--------|-------------|
| `float temperature()` | Silicon temperature in °C (empirical, not ambient). |
| `int16_t readTemperatureRaw()` | Raw temperature LSB (8 LSB/°C). |
| `void setTempOffset(float offset)` | Additive offset in °C (resets gain to 1.0). |
| `bool calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point linear correction. Returns `false` if `measLow == measHigh`. |

### Modes

| Method | Description |
|--------|-------------|
| `void setContinuous(uint8_t hz = 10)` | Continuous mode at the given ODR (10, 20, 50, or 100 Hz). |
| `void triggerOneShot()` | Non-blocking: start a single conversion. |
| `MagneticField readOneShot()` | Trigger + wait + return. Returns `{0, 0, 0}` on timeout. |
| `void setOdr(int hz)` | Set ODR bits only, without changing mode. |

### Calibration

| Method | Description |
|--------|-------------|
| `void setCalibrateStep(float xoff, float yoff, float zoff, float xscale, float yscale, float zscale)` | Set offsets and scales manually. |
| `void calibrateMinmax2d(uint16_t samples = 300, uint16_t delayMs = 20)` | Min/max calibration on XY only — rotate board flat during acquisition. |
| `void calibrateMinmax3d(uint16_t samples = 600, uint16_t delayMs = 20)` | Min/max calibration on all 3 axes — rotate board in all directions. |
| `CalibratedField calibrateApply(float x, float y, float z)` | Apply current offset/scale to a raw triplet. |
| `CalibrationQuality calibrateQuality(uint16_t samplesCheck = 200, uint16_t delayMs = 10)` | Evaluate calibration quality: center, radius dispersion, anisotropy. |
| `void calibrateReset()` | Reset offsets to 0 and scales to 1. |
| `void calibrateStep()` | Alias for `calibrateMinmax3d()`. |
| `HwOffsets readHwOffsets()` | Read hardware offset registers (`OFFSET_*`). |
| `void setHwOffsets(int16_t x, int16_t y, int16_t z)` | Write hardware offset registers. |

### Heading / Compass

| Method | Description |
|--------|-------------|
| `float headingFlatOnly()` | Compass angle (0–360°) assuming the board is flat. Uses XY only. |
| `float headingFromVectors(float x, float y, float z, bool calibrated = true)` | Angle from a raw or calibrated triplet. |
| `float headingWithTiltCompensation(Vec3f (*readAccel)())` | Tilt-compensated heading using an external accelerometer callback. |
| `void setHeadingOffset(float deg)` | Software heading offset (physical 0° alignment). |
| `void setDeclination(float deg)` | Magnetic declination correction for true north. |
| `void setHeadingFilter(float alpha)` | Low-pass filter on heading via cos/sin averaging. `0` = off, `0.1–0.3` = light/medium. |
| `const char* directionLabel(float angle = -1.0f)` | Returns `"N"`, `"NE"`, `"E"`, … . Pass `angle < 0` to read the sensor automatically. |

### Configuration

| Method | Description |
|--------|-------------|
| `void setLowPower(bool enabled)` | Toggle low-power mode (LP bit in `CFG_REG_A`). |
| `void setLowPass(bool enabled)` | Toggle low-pass filter (LPF bit in `CFG_REG_B`). |
| `void setOffsetCancellation(bool enabled, bool oneShot = false)` | Toggle hardware offset cancellation. |
| `void setBdu(bool enabled = true)` | Toggle block data update (BDU bit in `CFG_REG_C`). |
| `void setEndianness(bool bigEndian)` | Toggle byte order (BLE bit in `CFG_REG_C`). |
| `void useSpi4wire(bool enable)` | Toggle 4-wire SPI mode. |

## Register constants

`LIS2MDL_const.h` exports register addresses (`LIS2MDL_*_REG`), the
expected `WHO_AM_I` value (`LIS2MDL_WHO_AM_I_VAL`), and temperature
conversion constants (`LIS2MDL_TEMP_SENSITIVITY`, `LIS2MDL_TEMP_OFFSET`)
so applications can access the part directly when needed.

## Testing

Host-side unit tests under [`tests/native/test_lis2mdl/`](../../tests/native/test_lis2mdl/)
exercise the driver against the `TwoWire` mock from
`tests/native/helpers/Wire.h`. Hardware validation tests under
[`tests/hardware/test_lis2mdl/`](../../tests/hardware/test_lis2mdl/)
run on a real STeaMi and verify device detection, field plausibility,
and sensor stability. Run them with:

```bash
make test-native        # host only, no board required
make test-hardware      # requires a connected STeaMi
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).