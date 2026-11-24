# STeaMi

Arduino library for the [STeaMi](https://wiki.steami.cc) board: drivers for its
sensors, D-PAD, battery gauge and display, plus BLE examples.

This repository is generated from
[`steamicc/arduino-steami-lib`](https://github.com/steamicc/arduino-steami-lib)
v{version}. Do not edit it here: issues and pull requests go to the upstream
repository.

## Installation

Install **STeaMi** from the Arduino IDE Library Manager (search "STeaMi").

In the Boards Manager, install the STM32 MCU based boards package, then select
**STeaMi Board** in *STM32 MCU based boards*.

## Usage

The sensors are on the internal I²C bus:

```cpp
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
```

Examples are available in *File > Examples > STeaMi*.

## Documentation

See the wiki: <https://wiki.steami.cc>.

## License

GPL-3.0-or-later.
