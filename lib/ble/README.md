# BLE examples

Bluetooth Low Energy examples for the STeaMi board (STM32WB55). This folder
is not a driver: it only holds example sketches built on the
[STM32duinoBLE](https://github.com/stm32duino/STM32duinoBLE) library.

## Dependency

* **Arduino IDE**: install **STM32duinoBLE** from the Library Manager
  (*Tools › Manage Libraries…*, search for `STM32duinoBLE`).
* **PlatformIO**: nothing to do. `platformio.ini` declares
  `stm32duino/STM32duinoBLE` in `lib_deps` for the `steami` environment.

## Examples

| Example | What it does |
|---------|--------------|
| [`BleBeacon`](examples/BleBeacon/) | Advertise the board name and a live temperature reading. Uses the WSEN-HIDS sensor (STeaMi V1a/V1b only). |
| [`BleHotCold`](examples/BleHotCold/) | BLE treasure hunt game based on the received signal strength. |
| [`BleRssiProximity`](examples/BleRssiProximity/) | Scan for a named BLE beacon and show a proximity gauge on the serial monitor. |
| [`BleRssiMapper`](examples/BleRssiMapper/) | Record RSSI measurements at different positions in a room. |
| [`BleTrilateration`](examples/BleTrilateration/) | Estimate a 2D position from the RSSI of several beacons. |
| [`BleIndoorMap`](examples/BleIndoorMap/) | Real-time indoor position on an ASCII map, with a classroom [activity sheet](examples/BleIndoorMap/BleIndoorMap.md). |

Flash an example with PlatformIO:

```bash
make flash-ble/BleBeacon
```
