# SSD1327

Arduino/C++ driver for the SSD1327 128x128 grayscale OLED display used on the STeaMi board.

## Hardware

* SSD1327 OLED controller, usable over SPI or I2C.
* Default I2C address: `0x3C`.
* 128x128 pixels, 16 levels of grayscale.
* Framebuffer format: GS4, 4 bits per pixel, 2 pixels per byte.
* On STeaMi, the OLED is connected to the internal SPI bus:
  * MOSI: `SPI_INT_MOSI`
  * SCK: `SPI_INT_SCK`
  * data/command: `SPI_INT_MISO`
  * reset: `RST_DISPLAY`
  * chip select: `CS_DISPLAY`

## Quick start

### STeaMi OLED

Use this preset when targeting the built-in STeaMi OLED. It creates and starts the internal SPI bus automatically.

```cpp
#include <SSD1327.h>

WS_OLED_128X128_STEAMI display;

void setup() {
    display.begin();
    display.fill(0);
    display.text("STeaMi", 42, 60, 15);
    display.show();
}

void loop() {}
```

### I2C

```cpp
#include <Wire.h>
#include <SSD1327.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WS_OLED_128X128_I2C display(internalI2C);

void setup() {
    internalI2C.begin();

    if (!display.begin()) {
        return;
    }

    display.fill(0);
    display.pixel(64, 64, 15);
    display.show();
}

void loop() {}
```

### Custom SPI

For custom SPI wiring, pass the SPI bus and display control pins explicitly.

```cpp
#include <SPI.h>
#include <SSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

SPIClass spiDisplay(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
WS_OLED_128X128_SPI display(spiDisplay, DATA_COMMAND_DISPLAY, RST_DISPLAY, CS_DISPLAY);

void setup() {
    spiDisplay.begin();

    if (!display.begin()) {
        return;
    }

    display.fill(7);
    display.show();
}

void loop() {}
```

See [examples/](examples/) for full sketches.

## Examples

| Example | What it does |
|---------|--------------|
| [`framebuf_pixels`](examples/framebuf_pixels/) | Draw individual pixel at various positions and grayscale levels. |
| [`framebuf_lines`](examples/framebuf_lines/) | Draw diagonal and intersecting lines across the display. |
| [`framebuf_rects`](examples/framebuf_rects/) | Draw filled and outlined rectangles using different grayscale levels. |
| [`framebuf_text`](examples/framebuf_text/) | Render text strings at different positions on the display. |
| [`framebuf_scroll`](examples/framebuf_scroll/) | Demonstrate software scrolling of the framebuffer with text. |
| [`invert`](examples/invert/) | Toggle the display between normal and inverted modes. |
| [`rotation`](examples/rotation/) | Demonstrate 180° display rotation. |
| [`shades`](examples/shades/) | Display vertical bands covering all 16 shades of gray. |
| [`random_pixels`](examples/random_pixels/) | Display one randomly positioned grayscale pixels on each iteration. |
| [`illusion`](examples/illusion/) | Render an optical illusion pattern using alternating squares and horizontal lines. |
| [`rotating_3d_cube`](examples/rotating_3d_cube/) | Animate a rotating wireframe 3D cube. |

### Building an example

```bash
make list-examples
make flash-ssd1327/rotating_3d_cube
```

To capture serial output from boot:

```bash
make capture-ssd1327/rotating_3d_cube
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `SSD1327(uint8_t width = 128, uint8_t height = 128)` | Base framebuffer/display class. Usually used through one of the concrete subclasses below. |
| `WS_OLED_128X128_STEAMI()` | Construct the built-in STeaMi OLED using the internal SPI bus and board pin mapping. |
| `WS_OLED_128X128_I2C(TwoWire& wire = Wire, uint8_t address = 0x3C)` | Construct a 128x128 I2C display instance. |
| `WS_OLED_128X128_SPI(SPIClass& spi, uint8_t dc, uint8_t res, uint8_t cs)` | Construct a 128x128 SPI display instance. |
| `bool begin()` | Initialise the controller, clear the framebuffer, and power on the display. |
| `void show()` | Flush the framebuffer to the display. |
| `void powerOn()` / `void powerOff()` | Toggle the display power state. |

### Drawing

All drawing methods operate on the internal framebuffer. Call `show()` to push changes to the screen.
Colors are 4-bit grayscale values: `0` = black, `15` = white.

| Method | Description |
|--------|-------------|
| `void fill(uint8_t color)` | Fill the entire framebuffer with a single grayscale value. |
| `void pixel(uint8_t x, uint8_t y, uint8_t color)` | Set a single pixel. Out-of-range coordinates are ignored. |
| `void line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)` | Draw a line using Bresenham's algorithm. |
| `void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)` | Fill a rectangle, clipped to the framebuffer boundaries. |
| `void scroll(int16_t dx, int16_t dy)` | Scroll the framebuffer by `dx` pixels horizontally and `dy` pixels vertically. Empty areas are filled with black. |
| `void text(const char* str, int16_t x, uint8_t y, uint8_t color)` | Draw ASCII text using the built-in 5x7 font. |
| `uint8_t width() const` | Return the framebuffer width in pixels. |
| `uint8_t height() const` | Return the framebuffer height in pixels. |

### Display settings

| Method | Description |
|--------|-------------|
| `void contrast(uint8_t contrast)` | Set display contrast from `0` to `255`. |
| `void invert(bool invert)` | Use SSD1327 normal or inverse display mode. `true` = inverted, `false` = normal. |
| `void rotate(bool rotate)` | Rotate display orientation. `true` = 180°, `false` = normal. |

## Register constants

`SSD1327_const.h` exports command bytes such as `SET_COL_ADDR`, `SET_CONTRAST`,
`SET_DISP`, `SET_DISP_MODE_NORMAL`, `SET_DISP_MODE_INVERSE`, and control register
values such as `REG_CMD` and `REG_DATA` for low-level use.

## Testing

Host-side unit tests under [`tests/native/test_ssd1327/`](../../tests/native/test_ssd1327/)
exercise the I2C driver against the `TwoWire` mock. Run without hardware:

```bash
make test-native
```

Hardware validation tests under [`tests/hardware/test_ssd1327/`](../../tests/hardware/test_ssd1327/)
verify that drawing calls complete without crashing on real silicon:

```bash
make test-hardware
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
