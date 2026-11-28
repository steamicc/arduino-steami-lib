# GC9A01

Arduino/C++ driver for the GC9A01 240x240 round colour TFT display of the STeaMi V1c board.

## Hardware

* GC9A01 TFT controller, driven over SPI (write only).
* Panel on the STeaMi V1c: Winstar WF0128BTYAA4DNN0, 1.28", 240x240 pixels, round, IPS.
* The STeaMi V1a/V1b boards have an SSD1327 OLED instead: use the [`ssd1327`](../ssd1327/) driver there.
* Colour format: RGB565, 16 bits per pixel (5 bits red, 6 bits green, 5 bits blue).
* Only the pixels inside the 240-pixel circle are visible: the four corners of the frame buffer
  are hidden. The largest fully visible square is 168x168, from `(36, 36)` to `(203, 203)`.
* The backlight is wired to +3V3: it is always on, there is no brightness control.
* On the STeaMi V1c, the display is on the internal SPI bus:
  * MOSI: `SPI_INT_MOSI` (PB5)
  * SCK: `SPI_INT_SCK` (PA1)
  * data/command: `SPI_INT_MISO` (PB4)
  * reset: `RST_DISPLAY` (PA12)
  * chip select: `CS_DISPLAY` (PD0)

The data/command line sits on the internal SPI MISO pin. The display is write-only, so the
driver keeps PB4 as MISO in the `SPIClass` and takes it back as a GPIO output after every
`beginTransaction()` (same technique as the SSD1327 driver). The P10 edge pin stays free.

### RAM cost

The driver keeps a full frame buffer: 240 x 240 x 2 = **115 200 bytes** of RAM, about 59 % of
the STM32WB55 RAM available to sketches. It is allocated on the heap by `begin()`, not when the
object is created: a sketch that only includes the header pays nothing. `begin()` returns
`false` when the allocation fails (for instance when the sketch already uses a lot of RAM).

### SPI speed

`show()` sets the address window once and streams the whole buffer in one transaction. The
driver asks for 16 MHz, the SPI1 maximum on the STeaMi (PCLK2 = 32 MHz divided by 2, the
smallest prescaler). A frame is 115 200 bytes, so its theoretical transfer time at 16 MHz is
about 58 ms (115 200 x 8 bits / 16 MHz); the real `show()` is a bit longer. The hardware test
prints the measured duration (see [Testing](#testing)).

## Quick start

### STeaMi V1c display

Use this preset with the built-in STeaMi V1c display. It creates and starts the internal SPI
bus automatically.

```cpp
#include <SteamiGC9A01.h>

SteamiGC9A01 display;

void setup() {
    Serial.begin(115200);
    if (!display.begin()) {
        Serial.println("GC9A01 begin failed: not enough RAM for the frame buffer");
        return;
    }
    display.fill(GC9A01_BLACK);
    display.text("STeaMi", 102, 116, GC9A01_YELLOW);
    display.show();
}

void loop() {}
```

### Custom SPI

For custom wiring, pass the SPI bus and the display control pins explicitly. Use
`GC9A01_PIN_NONE` as the reset pin when it is not connected: a software reset is sent instead.

```cpp
#include <SPI.h>
#include <SteamiGC9A01.h>

SPIClass spiDisplay(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
GC9A01_SPI display(spiDisplay, SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY);

void setup() {
    spiDisplay.begin();

    if (!display.begin()) {
        return;
    }

    display.fill(GC9A01_BLUE);
    display.show();
}

void loop() {}
```

### Moving a sketch from the SSD1327 OLED

The drawing API is the same as the SSD1327 driver. Change the include and the class, then
replace the 4-bit grey levels (`0` to `15`) by RGB565 colours:

| SSD1327 (V1a/V1b)              | GC9A01 (V1c)                             |
|--------------------------------|------------------------------------------|
| `#include <SteamiSSD1327.h>`   | `#include <SteamiGC9A01.h>`              |
| `SteamiSSD1327 display;`       | `SteamiGC9A01 display;`                  |
| `display.fill(0);`             | `display.fill(GC9A01_BLACK);`            |
| `display.pixel(64, 64, 15);`   | `display.pixel(120, 120, GC9A01_WHITE);` |
| `display.contrast(...)`        | no equivalent (backlight always on)      |

Coordinates go from `0` to `239` instead of `0` to `127`.

See [examples/](examples/) for full sketches.

## Examples

| Example | What it does | Try it |
|---------|--------------|--------|
| [`framebuf_pixels`](examples/framebuf_pixels/) | Draw single pixels in different colours. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#framebuf_pixels) |
| [`framebuf_lines`](examples/framebuf_lines/) | Draw a star pattern of coloured lines. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#framebuf_lines) |
| [`framebuf_rects`](examples/framebuf_rects/) | Draw nested filled squares in different colours. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#framebuf_rects) |
| [`framebuf_text`](examples/framebuf_text/) | Render centred text lines in different colours. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#framebuf_text) |
| [`framebuf_scroll`](examples/framebuf_scroll/) | Scroll text up with `scroll()`. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#framebuf_scroll) |
| [`invert`](examples/invert/) | Toggle the display between normal and inverted colours. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#invert) |
| [`rotation`](examples/rotation/) | Demonstrate 180° display rotation. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#rotation) |
| [`color_palette`](examples/color_palette/) | Show the named colours and red, green, blue and grey gradients built with `color565()`. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#color_palette) |
| [`random_pixels`](examples/random_pixels/) | Add randomly placed pixels of random colours. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#random_pixels) |
| [`rotating_3d_cube`](examples/rotating_3d_cube/) | Animate a rotating wireframe 3D cube with coloured edges. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01#rotating_3d_cube) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/gc9a01).

### Building an example

```bash
make list-examples DRIVER=gc9a01
make flash-gc9a01/rotating_3d_cube
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiGC9A01()` | Construct the built-in STeaMi V1c display using the internal SPI bus and board pin mapping. |
| `GC9A01_SPI(SPIClass& spi, uint8_t dc, uint8_t rst, uint8_t cs)` | Construct a display on any SPI bus. `rst` may be `GC9A01_PIN_NONE`. |
| `bool begin()` | Allocate the frame buffer, reset and initialise the controller, clear the screen, then turn the display on (the uninitialised display RAM is never shown). Returns `false` if the frame buffer cannot be allocated. |
| `void show()` | Send the frame buffer to the display. |
| `void powerOn()` / `void powerOff()` | Leave or enter sleep mode (display off). Each call waits 120 ms, as the controller requires. `powerOff()` does not switch the backlight off: on the STeaMi V1c it is wired to +3V3 and stays lit. |

### Drawing

All drawing methods operate on the frame buffer. Call `show()` to push changes to the screen.
Drawing before `begin()` (or after a failed `begin()`) does nothing.
Colors are 16-bit RGB565 values.

| Method | Description |
|--------|-------------|
| `void fill(uint16_t color)` | Fill the entire frame buffer with one colour. |
| `void pixel(int16_t x, int16_t y, uint16_t color)` | Set a single pixel. Out-of-range coordinates are ignored. |
| `uint16_t pixel(int16_t x, int16_t y) const` | Read a pixel from the frame buffer (`0` when out of range). |
| `void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)` | Draw a line using Bresenham's algorithm, clipped to the screen. |
| `void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)` | Fill a rectangle, clipped to the frame buffer boundaries. |
| `void scroll(int16_t dx, int16_t dy)` | Scroll the frame buffer by `dx` pixels horizontally and `dy` pixels vertically. Empty areas are filled with black. Done in place, without a second buffer. |
| `void text(const char* str, int16_t x, int16_t y, uint16_t color)` | Draw ASCII text with the built-in 5x7 font (6-pixel advance, `(x, y)` is the top-left corner). |
| `uint16_t width() const` | Return the frame buffer width in pixels (240). |
| `uint16_t height() const` | Return the frame buffer height in pixels (240). |

### Colours

| Name | Description |
|------|-------------|
| `static constexpr uint16_t GC9A01::color565(uint8_t r, uint8_t g, uint8_t b)` | Pack 8-bit red, green and blue into RGB565. Also callable as `display.color565(r, g, b)`. |
| `GC9A01_BLACK`, `GC9A01_WHITE`, `GC9A01_RED`, `GC9A01_GREEN`, `GC9A01_BLUE`, `GC9A01_CYAN`, `GC9A01_MAGENTA`, `GC9A01_YELLOW`, `GC9A01_ORANGE`, `GC9A01_PINK`, `GC9A01_PURPLE`, `GC9A01_NAVY`, `GC9A01_DARKGREEN`, `GC9A01_GREY`, `GC9A01_LIGHTGREY`, `GC9A01_DARKGREY` | Named RGB565 colours. |

In the frame buffer and on the wire, each pixel is two bytes, high byte first.

### Display settings

| Method | Description |
|--------|-------------|
| `void invert(bool invert)` | Invert the colours. `true` = inverted, `false` = normal. The V1c panel is IPS: normal colours use the controller inversion (INVON), so `invert(true)` sends INVOFF. |
| `void rotate(bool rotate)` | Rotate the display orientation. `true` = 180°, `false` = normal. The frame is sent again so the picture turns right away. |

## Register constants

`SteamiGC9A01_const.h` exports the command bytes (`GC9A01_CMD_CASET`, `GC9A01_CMD_RAMWR`,
`GC9A01_CMD_MADCTL`, `GC9A01_CMD_INVON`, ...), the MADCTL bits (`GC9A01_MAD_*`), the two
orientations used by `rotate()` (`GC9A01_ORIENT_NORMAL`, `GC9A01_ORIENT_ROTATED`), the panel size
(`GC9A01_WIDTH`, `GC9A01_HEIGHT`) and the colours. Every name starts with `GC9A01_` so it does not
clash with the other STeaMi drivers.

## Testing

Host-side unit tests under [`tests/native/test_gc9a01/`](../../tests/native/test_gc9a01/)
check the frame buffer drawing and the SPI command stream (init sequence, address window,
byte order, D/C and CS levels) against the SPI mock. Run without hardware:

```bash
make test-native/gc9a01
```

Hardware validation tests under [`tests/hardware/test_gc9a01/`](../../tests/hardware/test_gc9a01/)
run on a STeaMi V1c and print the measured `show()` duration:

```bash
make test-hardware/gc9a01
```

## References

* STeaMi wiki: [Display hardware page](https://wiki.steami.cc/docs/hardware/main-components/display).
* MicroPython driver: [`gc9a01`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/gc9a01).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
