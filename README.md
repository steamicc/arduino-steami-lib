# arduino-steami-lib

Arduino/C++ driver library for the [STeaMi](https://www.steami.cc/) board. Each driver lives under `lib/<component>/` and follows a standard structure.

## Repository contents

* Ready-to-use **Arduino-compatible drivers**
* Consistent **API design across all components**
* CI integration with clang-format and PlatformIO

## Board components

| Component     | Driver                                   | I2C Address | Description                           |
| ------------- | ---------------------------------------- | ----------- | ------------------------------------- |
| BQ27441-G1    | [`bq27441`](lib/bq27441/)               | `0x55`      | Battery fuel gauge                    |
| DAPLink Flash | [`daplink_flash`](lib/daplink_flash/)    | `0x3B`      | I2C-to-SPI flash bridge + config zone |
| SSD1327       | [`ssd1327`](lib/ssd1327/)               | — (SPI)     | 128x128 greyscale OLED display        |
| MCP23009E     | [`mcp23009e`](lib/mcp23009e/)           | `0x20`      | 8-bit I/O expander (D-PAD)            |
| VL53L1X       | [`vl53l1x`](lib/vl53l1x/)               | `0x29`      | Time-of-Flight distance sensor        |
| APDS-9960     | [`apds9960`](lib/apds9960/)             | `0x39`      | Proximity, gesture, color, light      |
| HTS221        | [`hts221`](lib/hts221/)                 | `0x5F`      | Humidity + temperature                |
| WSEN-HIDS     | [`wsen-hids`](lib/wsen-hids/)           | `0x5F`      | Humidity + temperature                |
| WSEN-PADS     | [`wsen-pads`](lib/wsen-pads/)           | `0x5D`      | Pressure + temperature                |
| ISM330DL      | [`ism330dl`](lib/ism330dl/)             | `0x6B`      | 6-axis IMU (accel + gyro)             |
| LIS2MDL       | [`lis2mdl`](lib/lis2mdl/)               | `0x1E`      | 3-axis magnetometer                   |
| IM34DT05      | `im34dt05` *(not yet implemented)*       | — (PDM)     | Digital microphone                    |
| BME280        | `bme280` *(not yet implemented)*         | `0x76`      | Pressure + humidity + temperature     |
| GC9A01        | `gc9a01` *(not yet implemented)*         | — (SPI)     | Round color LCD display               |
| STeaMi Config | [`steami_config`](lib/steami_config/)   | —           | Persistent board configuration        |

## Quick start

### Prerequisites

* [PlatformIO](https://platformio.org/) or Arduino IDE
* A STeaMi board

### Build with PlatformIO

```bash
pio run
```

### Upload to board

```bash
pio run --target upload
```

## Development

### Setup

Requires **Python 3** (with `venv` — on Debian/Ubuntu: `sudo apt install python3-venv`)
and **Node.js** (for the git hooks tooling).

```bash
make setup    # Install npm tooling, git hooks, and PlatformIO (in a local .venv/)
```

`make setup` creates a local Python virtualenv in `.venv/` and installs
PlatformIO there, so no global `pip install` is required. All `make` targets
(`build`, `upload`, `test`) transparently use this local `pio`.

To use `pio` directly outside of `make`, activate the venv first:

```bash
source .venv/bin/activate
pio device monitor -b 115200
```

### Available commands

Run `make help` to see all available targets:

| Command | Description |
|---------|-------------|
| `make lint` | Run clang-format check |
| `make lint-fix` | Auto-fix formatting |
| `make build` | Build with PlatformIO |
| `make test` | Run unit tests |
| `make upload` | Upload to board |
| `make clean` | Remove build artifacts |
| `make deepclean` | Remove everything including node_modules |

### Git hooks

Git hooks are managed by [husky](https://typicode.github.io/husky/) and run automatically on commit:

- **commit-msg** — validates commit message format via [commitlint](https://commitlint.js.org/)
- **pre-commit** — branch name validation, content checks, clang-format on staged files

## Troubleshooting uploads

PlatformIO's default uploader on STeaMi is OpenOCD over CMSIS-DAP. On Linux,
the most common failure mode is `Error: unable to find CMSIS-DAP device`
(or similar) caused by incomplete udev rules.

### Install udev rules

The rules shipped with [pyocd](https://github.com/pyocd/pyOCD) only grant
access to the CMSIS-DAP v1 HID interface. OpenOCD prefers the CMSIS-DAP v2
WinUSB backend (libusb bulk transfers), which needs a separate rule on the
USB subsystem. Install both, and tell ModemManager to stop probing the
virtual serial port:

```bash
sudo tee /etc/udev/rules.d/60-steami.rules > /dev/null <<'EOF'
# STeaMi / ARM mbed DAPLink
# CMSIS-DAP v2 (WinUSB bulk — used by OpenOCD by default)
SUBSYSTEM=="usb", ATTRS{idVendor}=="0d28", ATTRS{idProduct}=="0204", \
    MODE="0666", TAG+="uaccess", ENV{ID_MM_DEVICE_IGNORE}="1"
# CMSIS-DAP v1 (HID fallback — used by pyocd and older OpenOCD)
KERNEL=="hidraw*", ATTRS{idVendor}=="0d28", ATTRS{idProduct}=="0204", \
    MODE="0666", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug and replug the board.

### Fallback: use pyocd instead of OpenOCD

If OpenOCD still fails after the udev rules, pyocd is a drop-in alternative
with broader DAPLink firmware compatibility and bundled target definitions:

```bash
source .venv/bin/activate
pip install pyocd
```

Then edit [`platformio.ini`](platformio.ini):

```ini
upload_protocol = custom
upload_command = pyocd flash --target stm32wb55rgvx $SOURCE
```

The exact pyocd target name may vary by pack version — run
`pyocd list --targets | grep -i wb55` to confirm. You may need to install
the CMSIS pack first: `pyocd pack install stm32wb55`.

### Check OpenOCD version

PlatformIO bundles its own OpenOCD at
`~/.platformio/packages/tool-openocd/bin/openocd`. STM32WB55 support has
improved across versions — if both remedies above fail, verify the bundled
OpenOCD is recent:

```bash
~/.platformio/packages/tool-openocd/bin/openocd --version
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This project is licensed under the [GPL v3](LICENSE) License.

## Additional Resources

- [STeaMi Official Website](https://www.steami.cc/)
- [MicroPython driver library](https://github.com/steamicc/micropython-steami-lib) — sister project
- [Arduino Documentation](https://docs.arduino.cc/)
