# V1c drivers (BME280, GC9A01) — design

- Date: 2026-11-24
- Issues: #14 (BME280), #15 (GC9A01)
- Status: approach chosen by the maintainer (GC9A01 option 1)

## Intent

The STeaMi V1c replaces the WSEN-PADS / WSEN-HIDS / APDS-9960 sensors by a BME280 and the SSD1327 OLED by a GC9A01 round colour TFT. Today V1c users need third-party libraries (Adafruit BME280, GFX Library for Arduino, with a MISO-on-P10 workaround). Both components get STeaMi drivers in this repository, shipped in the `STeaMi` Library Manager package.

## BME280

- Port of the MicroPython driver (`steamicc/micropython-steami-lib`, `lib/bme280`), following the Arduino conventions of this repository (see `lib/wsen-hids`, `lib/wsen-pads`, CONTRIBUTING.md).
- Class `BME280`, headers `BME280.h` / `BME280_const.h`, constructor `BME280(TwoWire& wire = Wire, uint8_t address = 0x76)`.
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

- One PR per driver. Hardware validation is done on a V1c by the maintainer's session after both build: each example is flashed by mass storage and checked (serial values for the BME280, visual check of the screen).
- The package paragraph in `scripts/build-arduino-package.py` and the README component table are updated by each PR for its own component only.
- Follow-up, not in scope: a high-level screen layer shared by SSD1327 and GC9A01 (like MicroPython `steami_screen`).
