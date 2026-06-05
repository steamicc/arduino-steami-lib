# SSD1327

Arduino/C++ driver for the SSD1327 128x128 grayscale OLED display on the STeaMi board.

## Hardware

* SPI or I2C display controller, default I2C address `0x3C`.
* 128x128 pixels, 16 levels of grayscale (GS4 format: 4 bits per pixel).
* The STeaMi board exposes the display on the internal SPI bus via
  `SPI_INT_MOSI` / `SPI_INT_SCK` / `SPI_INT_MISO` and chip-select
  `CS_DISPLAY`.

## Quick start

### I2C

```cpp
#include <Wire.h>
#include <SSD1327.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WS_OLED_128X128_I2C display(internalI2C);

void setup() {
    internalI2C.begin();
    display.begin();
    display.fill(0);
    display.pixel(64, 64, 15);
    display.show();
}

void loop() {}
```

### SPI

```cpp
#include <SPI.h>
#include <SSD1327.h>

WS_OLED_128X128_SPI display(SPI, DC_DISPLAY, RST_DISPLAY, CS_DISPLAY);

void setup() {
    SPI.begin();
    display.begin();
    display.fill(7);
    display.show();
}

void loop() {}
```

See [examples/](examples/) for full sketches.

## Examples

| Example | What it does |
|---------|--------------|
| [`fill_and_show`](examples/fill_and_show/) | Fill the screen with a uniform grayscale level and display it. |
| [`draw_shapes`](examples/draw_shapes/) | Draw pixels, lines, and rectangles at various grayscale levels. |
| [`scroll`](examples/scroll/) | Software-scroll the framebuffer vertically. |
| [`contrast`](examples/contrast/) | Cycle through contrast levels to demonstrate brightness control. |

### Building an example

```bash
make list-examples
make flash-ssd1327/fill_and_show
```

To capture serial output from boot:

```bash
make capture-ssd1327/fill_and_show
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `WS_OLED_128X128_I2C(TwoWire& wire = Wire, uint8_t address = 0x3C)` | Construct an I2C display instance. |
| `WS_OLED_128X128_SPI(SPIClass& spi, uint8_t dc, uint8_t res, uint8_t cs)` | Construct a SPI display instance. |
| `bool begin()` | Initialise the controller and clear the screen. Always returns `true`. |
| `void show()` | Flush the framebuffer to the display. |
| `void powerOn()` / `void powerOff()` | Toggle the display power state. |

### Drawing

All drawing methods operate on the internal framebuffer. Call `show()` to push changes to the screen.
Colors are 4-bit grayscale values (0 = black, 15 = white).

| Method | Description |
|--------|-------------|
| `void fill(uint8_t color)` | Fill the entire framebuffer with a single color. |
| `void pixel(uint8_t x, uint8_t y, uint8_t color)` | Set a single pixel. |
| `void line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)` | Draw a line using Bresenham's algorithm. |
| `void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)` | Fill a rectangle. |
| `void scroll(int16_t dx, int16_t dy)` | Scroll the framebuffer vertically (dx ignored). |
| `void text(const char* str, uint8_t x, uint8_t y, uint8_t color)` | Draw text (not yet implemented). |

### Display settings

| Method | Description |
|--------|-------------|
| `void contrast(uint8_t contrast)` | Set display contrast (0–255). |
| `void invert(uint8_t invert)` | Invert display colors (`1` = inverted, `0` = normal). |
| `void rotate(uint8_t rotate)` | Rotate display (`0` = normal, `1` = rotated 180°). |

## Register constants

`SSD1327_const.h` exports all command bytes (`SET_COL_ADDR`, `SET_CONTRAST`,
`SET_DISP`, …) and control register values (`REG_CMD`, `REG_DATA`) for
applications that need to send raw commands directly.

## Testing

Host-side unit tests under [`tests/native/test_ssd1327/`](../../tests/native/test_ssd1327/)
exercise the I2C driver against the `TwoWire` mock. Run without hardware:

```bash
make test-native
```

Hardware validation tests under [`tests/hardware/test_ssd1327/`](../../tests/hardware/test_ssd1327/)
verify that all drawing calls complete without crashing on real silicon:

```bash
make test-hardware
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).