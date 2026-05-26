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
        auto r = sensor.read();
        Serial.print(r.temperature);
        Serial.print(" C / ");
        Serial.print(r.humidity);
        Serial.println(" %");
    }
    delay(100);
}
```

See [examples/BasicReader/](examples/BasicReader/) for the full sketch.

## Examples

| Example                                        | What it does                                                                                |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------- |
| [`BasicReader`](examples/BasicReader/)         | Baseline sketch: print temperature and humidity every second.                               |
| [`HumidityAlert`](examples/HumidityAlert/)     | Sound the on-board buzzer when humidity climbs past a mold-risk threshold.                  |
| [`DewPointMonitor`](examples/DewPointMonitor/) | Compute the dew point from T and RH, warn when condensation risk is high (T − Td < 2 °C).   |
| [`ComfortZone`](examples/ComfortZone/)         | Classify the room as comfortable / too dry / too humid / too cold / too hot.                |

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
make capture-wsen-hids/BasicReader             # 10 seconds, OpenOCD reset, stdout
make capture-wsen-hids/BasicReader DURATION=30 # longer window
```

## API

All methods follow the collection conventions: `camelCase`, include
units in method names only when they carry ambiguity, and skip
redundant `read` / `get` prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `WsenHids(TwoWire& wire = Wire, uint8_t address = WSEN_HIDS_DEFAULT_ADDRESS)` | Construct. Defaults to the global `Wire` and address `0x5F`. |
| `bool begin()` | Probe `WHO_AM_I`, load factory calibration, leave the part powered down. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Reads `WHO_AM_I` (always `0xBC`). |
| `void softReset()` / `void reboot()` | Reload factory trimming via `CTRL2.BOOT`. |
| `void powerOn()` / `void powerOff()` | Toggle `CTRL1.PD`. |

### Reading

If the part is powered down when a read is requested, the driver
switches it to continuous mode at 12.5 Hz, polls `dataReady()` with a
timeout, and returns the first sample. The caller doesn't have to
manage modes manually — but be aware that the chip stays powered on in
continuous mode afterwards (call `powerOff()` if you want it back in
low-power sleep). The auto-bring-up uses the continuous path rather
than `CTRL2.ONE_SHOT` because the on-board silicon does not honour the
one-shot trigger reliably (see also the matching MicroPython issue in
`steamicc/micropython-steami-lib`).

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
| `void setContinuous(uint8_t odr)` | Continuous mode. Pass `WSEN_HIDS_ODR_1_HZ`, `_7_HZ`, or `_12_5_HZ`. |
| `void triggerOneShot()` | Non-blocking: start a single conversion. |
| `ReadResult readOneShot()` | Trigger + wait + return. |
| `void setAveraging(uint8_t humAvg, uint8_t tempAvg)` | Configure `AV_CONF` averaging (datasheet table 18). |

### Calibration

| Method | Description |
|--------|-------------|
| `void setTemperatureOffset(float offset)` | Additive Celsius offset on top of the factory calibration. |
| `void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration. Applied after the factory curve. |

## Register constants

`WsenHids_const.h` exports register addresses (`WSEN_HIDS_REG_*`), bit
masks (`WSEN_HIDS_CTRL1_*`, `WSEN_HIDS_STATUS_*`), and ODR values
(`WSEN_HIDS_ODR_*`) so applications can poke the part directly if they
need something outside the driver's API surface.

## Testing

Build-only verification:

```bash
make build
```

Formatting:

```bash
make lint
```

Manual hardware validation can be done with the provided `BasicReader`
example to confirm:

* successful `begin()`
* correct `deviceId()`
* plausible temperature values
* plausible humidity values
* continuous acquisition readiness

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
