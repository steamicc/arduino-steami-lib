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

| Example | What it does | Try it |
|---------|--------------|--------|
| [`BleBeacon`](examples/BleBeacon/) | Advertise the board name and a live temperature reading. Uses the WSEN-HIDS sensor (STeaMi V1a/V1b only). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#blebeacon) |
| [`BleHotCold`](examples/BleHotCold/) | BLE treasure hunt game based on the received signal strength. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#blehotcold) |
| [`BleRssiProximity`](examples/BleRssiProximity/) | Scan for a named BLE beacon and show a proximity gauge on the serial monitor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#blerssiproximity) |
| [`BleRssiMapper`](examples/BleRssiMapper/) | Record RSSI measurements at different positions in a room. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#blerssimapper) |
| [`BleTrilateration`](examples/BleTrilateration/) | Estimate a 2D position from the RSSI of several beacons. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#bletrilateration) |
| [`BleIndoorMap`](examples/BleIndoorMap/) | Real-time indoor position on an ASCII map, with a classroom [activity sheet](examples/BleIndoorMap/BleIndoorMap.md). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/ble#bleindoormap) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/ble).

Flash an example with PlatformIO:

```bash
make flash-ble/BleBeacon
```
