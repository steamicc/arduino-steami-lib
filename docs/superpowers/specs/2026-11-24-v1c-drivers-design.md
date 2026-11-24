# V1c drivers (BME280, GC9A01) — design

- Date: 2026-11-24
- Issues: #14 (BME280), #15 (GC9A01)
- Status: approach chosen by the maintainer (GC9A01 option 1)

## Intent

The STeaMi V1c replaces the WSEN-PADS / WSEN-HIDS / APDS-9960 sensors by a BME280 and the SSD1327 OLED by a GC9A01 round colour TFT. Today V1c users need third-party libraries (Adafruit BME280, GFX Library for Arduino, with a MISO-on-P10 workaround). Both components get STeaMi drivers in this repository, shipped in the `STeaMi` Library Manager package.

## BME280

- Port of the MicroPython driver (`steamicc/micropython-steami-lib`, `lib/bme280`), following the Arduino conventions of this repository (see `lib/wsen-hids`, `lib/wsen-pads`, CONTRIBUTING.md).
- Class `SteamiBME280`, headers `SteamiBME280.h` / `SteamiBME280_const.h`, constructor `SteamiBME280(TwoWire& wire = Wire, uint8_t address = 0x76)`. Register constants keep the `BME280_` prefix.
- Why the prefix: the "BME280" Library Manager library (finitespace/BME280) installs a folder `BME280` with `src/BME280.h` and its own abstract `class BME280`. The Arduino include resolver prefers a library whose name and folder match the header, so with both installed `#include <BME280.h>` would pick that library over `STeaMi` and the sketch would fail with errors that never mention STeaMi. Many classroom PCs have it from earlier V1c workarounds.
- API: `begin()`, `deviceId()`, `softReset()`, `powerOn()` / `powerOff()`, `temperature()`, `humidity()`, `pressureHpa()`, `read()` (struct with temperature, humidity, pressure), `dataReady()` and per-quantity `*Ready()`, `setContinuous(standby)`, `triggerOneShot()`, `readOneShot()`, oversampling and IIR filter setters, `altitude(seaLevelHpa)`, `dewPoint()`, `setTemperatureOffset()`, `calibrateTemperature()`.
- Factory compensation formulas from the datasheet (as in the MicroPython driver).
- Native unit tests with the existing I²C mock, hardware test, at least three examples, README, keywords.

## GC9A01

- Same drawing API as the SSD1327 driver, so that a sketch moves from the V1b OLED to the V1c TFT by changing the class and the colours: `begin()`, `show()`, `fill(color)`, `pixel(x, y, color)`, `line(...)`, `fillRect(...)`, `text(str, x, y, color)`, `scroll(dx, dy)`, `rotate(bool)`, `invert(bool)`, `powerOn()` / `powerOff()`.
- RGB565 frame buffer of 240 × 240 × 2 bytes (115 200 bytes), allocated in `begin()` (never as a static array, so sketches that only include the header pay nothing); `begin()` returns `false` if the allocation fails. `show()` sends the whole buffer.
- Colours are `uint16_t` RGB565, with `color565(r, g, b)` and named constants.
- Class layout mirrors SSD1327: a generic SPI class taking `(SPIClass&, dc, rst, cs)` and a STeaMi class with no constructor argument that uses the internal SPI bus and the variant pins (`SPI_INT_*`, `CS_DISPLAY`, `RST_DISPLAY`).
- D/C sits on the internal SPI MISO pin (PB4). Same technique as SSD1327: keep MISO = PB4 in the `SPIClass` and call `pinMode(dc, OUTPUT)` after each `beginTransaction()`. The P10 edge pin stays free.
- Backlight is wired to +3V3 on the V1c: no brightness control.
- Native unit tests with an SPI mock where the existing helpers allow it, hardware test, examples mirroring the SSD1327 ones (pixels, lines, rects, text, colours), README, keywords.

## Shared

- Naming rule (decided by the maintainer): every STeaMi driver's public header is `Steami<Chip>.h` (constants header `Steami<Chip>_const.h`), and the main class bound to the STeaMi board is `Steami<Chip>` (`SteamiBME280`, `SteamiGC9A01`, later `SteamiISM330DL`, `SteamiSSD1327`...). Generic classes meant to drive the chip on another board (`GC9A01`, `GC9A01_SPI`, `SSD1327_SPI`) keep the chip name. Register constants, enums and structs keep their chip prefix. The library folder names under `lib/` do not change.
- Why one rule for all drivers: 5 of the 12 drivers have an exact homonym in the Arduino Library Manager: BME280 ("BME280", "Bme280"), APDS9960 ("APDS9960"), MCP23009E ("MCP23009E", the former `steamicc/MCP23009E` library), SSD1327 ("ssd1327") and VL53L1X ("VL53L1X", Pololu). The Arduino resolver prefers a library whose name matches the header, so where the homonym is installed the sketch compiles the wrong library, with errors that never mention STeaMi. A single prefix removes every conflict without checking each chip.
- Rollout: the BME280 and GC9A01 PRs apply the rule to their own drivers. The remaining drivers (including `WS_OLED_128X128_STEAMI`, which becomes `SteamiSSD1327`) are renamed in a follow-up PR (#242), then the wiki is updated.
- Versioning: the package stays at 0.x until the last driver lands; v1.0.0 is released then. No commit carries a breaking-change marker before that.
- One PR per driver. Hardware validation is done on a V1c by the maintainer's session after both build: each example is flashed by mass storage and checked (serial values for the BME280, visual check of the screen).
- The package paragraph in `scripts/build-arduino-package.py` and the README component table are updated by each PR for its own component only.
- Follow-up, not in scope: a high-level screen layer shared by SSD1327 and GC9A01 (like MicroPython `steami_screen`).
