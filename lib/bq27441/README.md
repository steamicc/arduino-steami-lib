# BQ27441

Arduino/C++ driver for the Texas Instruments BQ27441-G1A single-cell
Li-Po fuel gauge on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x55`.
* Autonomous fuel gauge — the IC tracks battery state of charge,
  voltage, current, temperature, and health without host intervention.
* Optional GPOUT pin for low-battery or SOC-change interrupts.

## Quick start

On the STeaMi board, the BQ27441 is routed to the **internal** I2C bus.
The board owns that bus: build the driver on `Steami.wire()`, which starts it on first use:

```cpp
#include <SteamiBoard.h>
#include <SteamiBQ27441.h>

SteamiBQ27441 fuelGauge(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);

    if (!fuelGauge.begin()) {
        Serial.println("BQ27441 not detected");
        while (true) delay(1000);
    }
}

void loop() {
    Serial.print(fuelGauge.stateOfCharge());
    Serial.print(" % / ");
    Serial.print(fuelGauge.voltageMv());
    Serial.print(" mV / ");
    Serial.print(fuelGauge.currentAverage());
    Serial.println(" mA");
    delay(1000);
}
```

## Examples

| Example | What it does | Try it |
|---------|--------------|--------|
| [`battery_status`](examples/battery_status/) | Print voltage, state of charge, remaining capacity and state of health every 3 seconds. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/bq27441#battery_status) |
| [`low_battery_alert`](examples/low_battery_alert/) | Blink the red LED below 20 % charge and sound the on-board `SPEAKER` below 5 %. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/bq27441#low_battery_alert) |
| [`battery_logger`](examples/battery_logger/) | Log voltage, charge, current and remaining capacity every minute to `BATTERY.CSV` on the STeaMi USB drive (DAPLink flash), to measure how long a charge lasts. | — |
| [`charging_monitor`](examples/charging_monitor/) | Tell charging from discharging by the sign of the current, and show the charge, a progress bar, the current and the time to full or to empty on the screen (V1a, V1b and V1c). | — |

Each example with a **Try it** link is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/bq27441).

Flash one with `make flash-bq27441/<example>` (list them with
`make list-examples DRIVER=bq27441`).

## API

All methods follow the collection conventions: `camelCase`, units
included in method names only where they carry ambiguity.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiBQ27441(TwoWire& wire = Wire, uint16_t capacity_mAh = 650, uint8_t address = 0x55, int gpout_pin = -1)` | Construct. Defaults to global `Wire`, 650 mAh capacity, address `0x55`, no GPOUT pin. |
| `bool begin()` | Verify device ID, configure GPOUT, set design capacity. Returns `false` if the IC is not detected. |
| `void powerOn()` | Wake the IC from shutdown and apply design capacity. |
| `void powerOff()` | Put the IC into shutdown mode. |
| `uint16_t deviceId()` | Returns the device type word (always `0x0421`). |
| `bool dataReady()` | Returns `true` when the IC has completed initialisation (`INITCOMP` bit set). |

### Battery readings

| Method | Description |
|--------|-------------|
| `uint16_t voltageMv()` | Battery voltage in millivolts. |
| `int16_t currentAverage()` | Average current in mA (negative = discharging). |
| `uint8_t stateOfCharge()` | Remaining charge as a percentage (0–100). |
| `uint8_t stateOfHealth()` | Battery health as a percentage (0–100). |
| `uint16_t capacityRemaining()` | Remaining capacity in mAh. |
| `uint16_t capacityFull()` | Full charge capacity in mAh. |
| `int16_t power()` | Average power in mW (negative = discharging). |
| `int16_t current(CurrentMeasureType type = AVG)` | Current in mA: `AVG` (average), `STBY` (standby) or `MAX` (max load). |
| `int16_t capacity(CapacityMeasureType type = REMAIN)` | Capacity in mAh: `REMAIN`, `FULL`, `AVAIL`, `AVAIL_FULL`, the filtered / unfiltered variants or `DESIGN`. |
| `uint16_t soc(SocMeasureType type = FILTERED)` | State of charge in %, filtered or `UNFILTERED`. |
| `uint16_t soh(SohMeasureType type = PERCENT)` | State of health in %, or the status byte with `SOH_STAT`. |
| `uint16_t flags()` | Raw `Flags()` word (`BQ27441_FLAG_*` bits). |

### Temperature

| Method | Description |
|--------|-------------|
| `float temperature(TempMeasureType type = BATTERY)` | Temperature in °C. Pass `INTERNAL_TEMP` to read the IC die temperature instead. |
| `float temperatureK(TempMeasureType type = BATTERY)` | Temperature in Kelvin. |
| `uint16_t temperatureDk(TempMeasureType type = BATTERY)` | Raw temperature in deci-Kelvin (value × 10). |

### Configuration

| Method | Description |
|--------|-------------|
| `uint16_t setCapacity(uint16_t capacity_mAh)` | Write the design capacity to the IC's extended memory. |
| `bool setGpoutPolarity(bool active_high)` | Set GPOUT pin polarity. |
| `uint16_t gpoutPolarity()` | Read current GPOUT polarity setting. |
| `uint16_t gpoutFunction()` | Read current GPOUT function (SOC_INT or BAT_LOW). |
| `bool setGpoutFunction(bool gpout_function)` | `true` drives GPOUT as BAT_LOW, `false` as SOC_INT. |
| `bool sealed()` | Returns `true` if the IC is in sealed access mode. |
| `bool seal()` / `bool unseal()` | Seal or unseal the data flash. |
| `bool enterConfig(bool user_control)` / `bool exitConfig(bool resim = true)` | Enter or leave the configuration mode. With `user_control = true`, the setters below write without entering or leaving it themselves until `exitConfig()`, so several writes share one session. |
| `bool reset()` / `bool softReset()` | Full reset, or soft reset (resimulation). |

### State-of-charge alerts

The thresholds are percentages; the setters clamp them to 0–100 and return `false` when the gauge refuses the write.

| Method | Description |
|--------|-------------|
| `uint16_t soc1SetThreshold()` / `uint16_t soc1ClearThreshold()` | SOC1 thresholds that set and clear the SOC1 flag. |
| `bool setSoc1Thresholds(uint16_t set, uint16_t clear)` | Write both SOC1 thresholds. |
| `uint16_t socfSetThreshold()` / `uint16_t socfClearThreshold()` | SOCF (final) thresholds. |
| `bool setSocfThresholds(uint16_t set, uint16_t clear)` | Write both SOCF thresholds. |
| `bool socFlag()` / `bool socfFlag()` | `true` while the SOC1 / SOCF flag is set. |
| `uint16_t sociDelta()` / `bool setSociDelta(uint16_t delta)` | SOC_INT step in % between two GPOUT pulses. |
| `bool pulseGpout()` | Pulse GPOUT (SOC_INT mode). |

## Enumerations

```cpp
// Select which current measurement to read
SteamiBQ27441::CurrentMeasureType::AVG   // Average current (default)
SteamiBQ27441::CurrentMeasureType::STBY  // Standby current
SteamiBQ27441::CurrentMeasureType::MAX   // Max load current

// Select which capacity measurement to read
SteamiBQ27441::CapacityMeasureType::REMAIN     // Remaining capacity (default)
SteamiBQ27441::CapacityMeasureType::FULL       // Full charge capacity
SteamiBQ27441::CapacityMeasureType::AVAIL      // Nominal available capacity
SteamiBQ27441::CapacityMeasureType::DESIGN     // Design capacity

// Select which temperature source to read
SteamiBQ27441::TempMeasureType::BATTERY       // Battery temperature (default)
SteamiBQ27441::TempMeasureType::INTERNAL_TEMP // IC internal temperature
```

## Register constants

`SteamiBQ27441_const.h` exports all register addresses (`BQ27441_COMMAND_*`),
control sub-commands (`BQ27441_CONTROL_*`), status bit masks
(`BQ27441_STATUS_*`, `BQ27441_FLAG_*`), and configuration class IDs
(`BQ27441_ID_*`) so applications can access the IC directly if they need
something outside the driver's API surface.

## Testing

Host-side unit tests under [`tests/native/test_bq27441/`](../../tests/native/test_bq27441/)
exercise the driver against the `TwoWire` mock from
`tests/native/helpers/Wire.h`. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).