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
default global `Wire`).
The board owns that bus: build the driver on `Steami.wire()`, which starts it on first use:

```cpp
#include <SteamiBoard.h>
#include <SteamiWsenHids.h>

SteamiWsenHids sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);

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

| Example                                        | What it does                                                                              | Try it                                                                                      |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| [`BasicReader`](examples/BasicReader/)         | Baseline sketch: print temperature and humidity every second.                             | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/wsen-hids#basicreader)     |
| [`HumidityAlert`](examples/HumidityAlert/)     | Sound the on-board buzzer when humidity climbs past a mold-risk threshold.                | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/wsen-hids#humidityalert)   |
| [`DewPointMonitor`](examples/DewPointMonitor/) | Compute the dew point from T and RH, warn when condensation risk is high (T − Td < 2 °C). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/wsen-hids#dewpointmonitor) |
| [`ComfortZone`](examples/ComfortZone/)         | Classify the room as comfortable / too dry / too humid / too cold / too hot.              | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/wsen-hids#comfortzone)     |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/wsen-hids).

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
| `SteamiWsenHids(TwoWire& wire = Wire, uint8_t address = WSEN_HIDS_DEFAULT_ADDRESS)` | Construct. Defaults to the global `Wire` and address `0x5F`. |
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
| `void enableBdu(bool enabled = true)` | Block Data Update, on by default. Applied at once and kept by `powerOn()`, `setContinuous()` and `triggerOneShot()`. |

### Calibration

The user correction is `gain * factory + offset`, applied after the factory
curve. Each call replaces the previous correction, as in the other
temperature drivers and in MicroPython: `setTemperatureOffset()` sets the
gain back to 1.

| Method | Description |
|--------|-------------|
| `void setTemperatureOffset(float offset)` | Celsius offset (resets the gain to 1). |
| `bool calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh)` | Two-point user calibration, from readings taken without calibration. Returns `false` if `measLow == measHigh` and keeps the previous correction. |

## Register constants

`SteamiWsenHids_const.h` exports register addresses (`WSEN_HIDS_REG_*`), bit
masks (`WSEN_HIDS_CTRL1_*`, `WSEN_HIDS_STATUS_*`), and ODR values
(`WSEN_HIDS_ODR_*`) so applications can poke the part directly if they
need something outside the driver's API surface.

## Testing

Host-side unit tests under
[`tests/native/test_wsen_hids/`](../../tests/native/test_wsen_hids/) exercise
the driver against the `TwoWire` mock. Run them without hardware with:

```bash
make test-native/wsen_hids
```

The hardware suite under
[`tests/hardware/test_wsen_hids/`](../../tests/hardware/test_wsen_hids/)
checks the real sensor: `begin()`, `deviceId()`, plausible temperature and
humidity, the one-shot and power-down paths, `reboot()`, averaging and the
temperature offset. It needs a STeaMi V1a or V1b (the V1c has a BME280
instead):

```bash
make test-hardware/wsen_hids
```

Build and formatting checks:

```bash
make build
make lint
```

## References

* Product page and datasheet: [Würth Elektronik WSEN-HIDS 2525020210001](https://www.we-online.com/en/components/products/WSEN-HIDS_1).
* STeaMi wiki: [WSEN-HIDS hardware page](https://wiki.steami.cc/docs/hardware/main-components/sensors/wsen-hids).
* MicroPython driver: [`wsen-hids`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/wsen-hids).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
