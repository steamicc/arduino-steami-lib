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
Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <Wire.h>
#include <BQ27441.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 fuelGauge(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

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

## API

All methods follow the collection conventions: `camelCase`, units
included in method names only where they carry ambiguity.

### Lifecycle

| Method | Description |
|--------|-------------|
| `BQ27441(TwoWire& wire = Wire, uint16_t capacity_mAh = 650, uint8_t address = 0x55, int gpout_pin = -1)` | Construct. Defaults to global `Wire`, 650 mAh capacity, address `0x55`, no GPOUT pin. |
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
| `bool sealed()` | Returns `true` if the IC is in sealed access mode. |

## Enumerations

```cpp
// Select which current measurement to read
BQ27441::CurrentMeasureType::AVG   // Average current (default)
BQ27441::CurrentMeasureType::STBY  // Standby current
BQ27441::CurrentMeasureType::MAX   // Max load current

// Select which capacity measurement to read
BQ27441::CapacityMeasureType::REMAIN     // Remaining capacity (default)
BQ27441::CapacityMeasureType::FULL       // Full charge capacity
BQ27441::CapacityMeasureType::AVAIL      // Nominal available capacity
BQ27441::CapacityMeasureType::DESIGN     // Design capacity

// Select which temperature source to read
BQ27441::TempMeasureType::BATTERY       // Battery temperature (default)
BQ27441::TempMeasureType::INTERNAL_TEMP // IC internal temperature
```

## Register constants

`BQ27441_const.h` exports all register addresses (`BQ27441_COMMAND_*`),
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