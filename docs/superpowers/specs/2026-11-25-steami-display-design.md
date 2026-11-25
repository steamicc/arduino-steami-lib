# STeaMi display layer: SteamiDisplay and SteamiScreen

Status: draft for maintainer review. The five recommendations of the design study (section 9) were
accepted by the maintainer on 2026-11-25; the rest of this document awaits review.
Repository: `steamicc/arduino-steami-lib`, base `main` at v0.34.0 (`SteamiBoard`, `SteamiHandle`, the 13
per-function classes, `Steami.wire()`, `SteamiSSD1327`, `SteamiGC9A01`). Each delivery branches from `main`.

Relation to `2026-11-25-steami-functional-layer-design.md` (the functional-layer spec): this document
**supersedes its sections 5.7 (SteamiDisplay) and 5.8 (SteamiScreen)** and refines its sections 6 (examples),
7 (tests), 8 items 3 and 4 (sub-projects) and 9 (decisions). Everything else in that spec (board, detection,
per-function classes, message format, packaging) still applies; section 11 lists the exact changes.

Sources: the MicroPython tutorial mockups
(<https://github.com/steamicc/micropython-steami-tutorials/tree/main/docs/mockups>, with `design-constraints.md`,
`validate.py` and `sim/`), the MicroPython module `steamicc/micropython-steami-lib` `lib/steami_screen`
(v0.26.0), its open issues #351 (gauge artefacts, `value()` centring), #368/#269 (GC9A01 wrapper), #349 (never
run on GC9A01), #371 and #434 (compass heading), and the drivers `lib/ssd1327` and `lib/gc9a01` of this
repository.

## 1. Intent and why

The per-function classes let one sketch read the temperature on V1a, V1b and V1c. The screen is the last
release-specific part: `SteamiSSD1327` takes grey levels 0..15 at 128x128, `SteamiGC9A01` takes RGB565 at
240x240, and neither clips its coordinates. A sketch that draws on one release does not build, or draws
garbage, on the other.

This layer gives two classes:

- `SteamiDisplay`: drawing primitives (pixels, lines, circles, triangles, arcs, text) in one colour type, on
  both panels, with clipping, integer arithmetic and no heap.
- `SteamiScreen`: the widgets of MicroPython `steami_screen` (`title`, `value`, `bar`, `gauge`, `graph`,
  `menu`, `compass`, `watch`, `face`...) with **the same names, the same argument order and the same cardinal
  positions**, so a course moves between the two languages with one vocabulary.

The visual target is **"the widgets as imagined in the tutorial mockups"**: the `XX_ref.png` images, not the
current MicroPython rendering. Where the MicroPython module differs from its mockups (off-centre `value()`,
arc artefacts, clipped axis labels, compass direction), the Arduino layer follows the mockups and the
corrections go back to MicroPython in separate PRs (section 12), so that both languages converge.

What the mockups fix and what they do not: they were drawn on 200x200 with anti-aliased vector fonts (value
in 36 pt bold, about 23 px on the OLED). The board has a bitmap font only. The mockups therefore fix the
**layout, hierarchy and colours**, not the glyphs: the mockup's four text sizes become the 8x8 font at
scale 1 or 2, plus a 5x7 font for secondary labels.

## 2. Goals, non-goals, anti-goals

Goals
- The same sketch draws the same screen on V1a/V1b (OLED 128x128, 16 greys) and V1c (TFT 240x240, RGB565).
- One widget = one call, with MicroPython's names and argument order; only keyword arguments become
  positional (`unit="mm"` becomes the second argument).
- Every mockup has a short Arduino program (section 6) that a student can type.
- Primitives are integer, clipped and hole-free (no gauge artefacts, #351).
- Widgets stay legible in grey: a colour that would vanish on the OLED (BLUE on DARK) is lifted.
- Everything is testable without a board, including a rendering of every mockup at 128 and 240 px.

Non-goals (may come later)
- Pixel-identical rendering with MicroPython (layout parity is the goal; MicroPython has no 5x7 font).
- Anti-aliasing, vector or proportional fonts, a big-digits font (revisit after the V1c review on the board).
- Partial refresh (`showRect()` with a dirty rectangle) and DMA transfers.
- `scroll()` in the layer (the SSD1327 scroll allocates a second 8 KB buffer; use the driver).
- Images/bitmaps other than the 8x8 face bitmaps, touch, double buffering, a circular clipping mask (the
  round panel hides the corners already).
- A real-time clock source for `watch()` (the examples use `millis()`; `STM32RTC` in an advanced example).

Anti-goals (things this layer must never do)
- Never construct the SSD1327 (8 KB `new` in its constructor) on a release without it, or in a sketch that
  does not use the screen.
- Never begin the GC9A01 (115 200 B `malloc` in `begin()`) unless the sketch uses the screen on a V1c.
- No heap allocation in the layer itself (drivers keep their own buffers), no `String`, no RTTI, no
  exceptions.
- Never hand an unclipped coordinate to a driver: `SSD1327::pixel(uint8_t, uint8_t, ...)` has no bounds check.
- Never call the drivers' `line()` or `text()`: they do not clip and use another font.
- Never use `double` arithmetic or `sin()`/`cos()` in drawing code (single-precision FPU; integer tables).
- Never guess the panel on an unknown release (both panels are write-only SPI): E30.
- No text drawn outside the visible circle by a widget: it is truncated or scaled down.
- No student-visible second font, scale factor or unit system: the 5x7 font, `k` and 128-px units are
  internal.

## 3. Constraints

- RAM: about 192 KB on the STM32WB55. The GC9A01 frame buffer takes 115 200 B in one block, so the screen
  starts first in `setup()`; sketches like WeatherStation must keep at least 140 KB free for heap and stack
  (footprint gate of the functional-layer spec).
- Flash: 512 KB cap (`upload.maximum_size`); the layer adds about 8 to 12 KB.
- Two panels: SSD1327 128x128, 4-bit grey, SPI at 10 MHz, byte-wise `show()`; GC9A01 240x240, RGB565
  big-endian buffer, SPI at 16 MHz, block `show()`.
- Round screens: the visible area is the disc of radius `radius()` centred on `(centerX(), centerY())`; the
  usable chord shrinks near the top and bottom (93 px at the title row on 128, 174 px on 240).
- Font: MicroPython's 8x8 framebuf font, so `steami_screen` layout constants port verbatim; a 5x7 font for
  secondary labels.
- Integer drawing: Bresenham, scanlines, integer square root, a quarter-wave sine table. `float` is accepted at
  widget inputs (values, ratios) on the single-precision FPU; literals carry the `f` suffix.
- Both cores: STM32duino 2.12 (PlatformIO) and 3.1.0 (Arduino IDE), `-std=gnu++17 -fno-rtti -fno-exceptions
  -fno-threadsafe-statics`. Do not rely on `printf("%f")` (newlib-nano does not link float formatting by
  default) nor on `dtostrf` (core-specific, absent on native).
- Packaging: flattened `src/`, generated umbrella `STeaMi.h`, unique file names (case-insensitive), no
  `Steami.h`; no `STEAMI_BOARD_*` in any library `.cpp`; headers carry no macro-dependent inline code.
- Native tests (`tests/native`, Unity, SPI/Wire mocks) must cover everything except the look on the real
  panel and the refresh time.

## 4. Approaches considered

| Question | Options weighed | Taken, and why |
|---|---|---|
| Units of the primitives called on `screen` (`line`, `circle`, `text(x, y)`) | (a) physical pixels, as MicroPython level 3; (b) 128-px units scaled like the widgets | **(a)**. Same meaning as MicroPython, the 240 px resolution stays reachable, `SteamiScreen` does not contradict `SteamiDisplay`. Portability comes from `centerX()`, `centerY()`, `radius()`. Tutorial 03 is fixed to use them. |
| Text on 240 px | (a) scale 1 as MicroPython today (tiny on V1c); (b) fractional glyph scaling (no anti-aliasing: uneven strokes); (c) integer glyph factor k = 1 or 2, layout ×15/8 | **(c)**, decision 9 of the functional-layer spec, extended to strokes. Pixel-art look consistent with the faces; text is 7 % wider relative to the layout on V1c. |
| Fonts | (a) 8x8 only (parity); (b) 8x8 + internal 5x7 for secondary labels; (c) + big digits 12x16; (d) Scale2x smoothing | **(b)**. Fixes the clipped axis labels and follows the mockup hierarchy for about 480 B. (c) and (d) wait for the V1c review on the board; neither changes the API. |
| `value()` argument type | (a) six overloads × options (old 5.8); (b) a template; (c) one `SteamiValue` with implicit constructors | **(c)**. One function, no ambiguity for `0`, `23.5`, `millis()`, `uint8_t`; short error messages. |
| Arc drawing | (a) float segments (MicroPython, artefacts #351); (b) Bresenham per radius (moiré holes); (c) integer scanline over the ring with an angular test | **(c)**. Every pixel decided once by distance and two cross products: no holes, no overlap, no `sin` per pixel. |
| `gauge()` and the value | (a) gauge draws value and unit (tutorials' copy); (b) arc and min/max only, `value()` separate (MicroPython lib) | **(b)**, decision 3. Composable, same rule as the other widgets; tutorial 04 and the mockups README are updated. |
| `compass(heading)` | (a) needle turned by +heading (MicroPython today: shows the heading); (b) real rose turned by −heading | **(b)**, decision 2. The needle points to real north; heading 0 is exactly the mockup. |
| Parity target | (a) the tutorials' February copy of `steami_screen`; (b) the lib v0.26, corrected | **(b)**, then corrections flow back to MicroPython. |
| Number formatting | (a) `snprintf("%f")`; (b) `dtostrf`; (c) own integer formatter | **(c)**. Same output on native and both cores, no float-printf linkage. |
| Partial refresh | (a) now; (b) later via an optional backend method | **(b)**. Course loops use `delay(200..1000)`; the API does not change. |

## 5. Design

### 5.1 Files

All in `lib/steami_board/src` (flattened into the package):

| File | Content |
|---|---|
| `SteamiRgb.h` | header-only colour type and the `STEAMI_*` colours; no screen dependency |
| `SteamiFont.h/.cpp` | 8x8 and 5x7 glyph tables, UTF-8 decoding and accent folding |
| `SteamiTrig.h/.cpp` | quarter-wave sine table, `steamiSinQ14()`, `steamiCosQ14()`, `steamiIsqrt()` |
| `SteamiDisplayBackend.h` | the backend interface |
| `SteamiSsd1327Backend.h/.cpp`, `SteamiGc9a01Backend.h/.cpp` | the two panel backends |
| `SteamiDisplay.h/.cpp` | primitives, clipping, text |
| `SteamiValue.h/.cpp` | the `value()` argument type and its formatter |
| `SteamiHistory.h` | `SteamiHistory<N>` ring buffer (header-only template) |
| `SteamiScreenConst.h` | 128-px layout constants, face bitmaps |
| `SteamiScreen.h/.cpp` | `SteamiAt`, `SteamiFace`, widgets |

Test-only: `tests/native/helpers/SteamiMemoryBackend.h` (5.4). Examples: `Drawing` (3b), one example per
mockup (section 6: `ScreenTemperature`, `ScreenBattery`, `ScreenComfort`, `ScreenGauge`, `ScreenGraph`,
`ScreenMenu`, `ScreenCompass`, `ScreenFaces`, `ScreenWatch`) and `WeatherStation` (4c). The mockup examples
replace the single `ScreenWidgets` example of the functional-layer spec.

### 5.2 Colour

```cpp
class SteamiRgb {
 public:
  constexpr SteamiRgb(uint8_t r, uint8_t g, uint8_t b);   // allows {255, 128, 0} at call sites
  static constexpr SteamiRgb gray(uint8_t level0to15);    // level * 17 per channel, level clamped to 15
  static constexpr SteamiRgb fromRgb565(uint16_t v);
  constexpr uint16_t rgb565() const;
  constexpr uint8_t gray4() const;   // expand 565 to 8 bits, ((r*77 + g*150 + b*29) >> 8) >> 4, as colors.py
  constexpr bool operator==(SteamiRgb o) const;
  constexpr bool operator!=(SteamiRgb o) const;
 private:
  uint16_t _v;
};
constexpr SteamiRgb STEAMI_BLACK{0,0,0}, STEAMI_DARK{102,102,102}, STEAMI_GRAY{153,153,153},
  STEAMI_LIGHT{187,187,187}, STEAMI_WHITE{255,255,255}, STEAMI_GREEN{119,255,119}, STEAMI_RED{255,85,85},
  STEAMI_BLUE{85,85,255}, STEAMI_YELLOW{255,255,85}, STEAMI_GRID_DARK{51,51,51};
```

- No implicit conversion from an integer: `clear(15)` does not compile, so an old grey level cannot become
  dark blue on V1c. `SteamiRgb::gray(15)` is the explicit form.
- Grey levels (gray4): BLACK 0, GRID_DARK 3, DARK 6, GRAY 9, LIGHT 11, WHITE 15, GREEN 12, RED 8, YELLOW 14,
  **BLUE 6** (same as DARK: see the contrast rule, 5.10).
- `SteamiColor::rgb()` (promised by functional-layer 5.3, absent from v0.34.0) is added in the same delivery:
  `SteamiRgb rgb();` reads the three counts and scales them so that the largest channel becomes 255 (hue of
  the object, not its brightness); all zero or unavailable gives `STEAMI_BLACK`. It includes `SteamiRgb.h`
  only.

### 5.3 Fonts and text

- **8x8**: port of MicroPython's framebuf font (MIT, attribution in the file header), characters 32..126,
  plus a degree glyph: 96 glyphs, about 770 B of flash. It is the only font the student sees
  (`text()`, `textWidth()`, `CHAR_W = CHAR_H = 8`).
- **5x7**: the table already in both drivers (columns, bit 0 on top), characters 32..126 plus a degree glyph,
  about 480 B; advance 6 px, line height 8 px at scale 1. Copied into `SteamiFont.cpp` (the drivers' copies
  are `static` and stay; `--gc-sections` drops them when the drivers' `text()` is unused). Used only by
  `SteamiScreen` for secondary labels: graph axes, gauge min/max, the `label` of `value()`.
- **UTF-8**: strings are decoded; U+00B0 maps to the degree glyph; `à â ä ç é è ê ë î ï ô ö ù û ü ÿ` and their
  capitals fold to the base letter, so "Température" and "°C" render. Any other code point and any invalid
  byte sequence draws `?`. `textWidth()` counts glyphs, not bytes.
- Rendering: each glyph row is drawn as horizontal runs of set pixels, one clipped `fillRect(x, y, run·s, s)`
  per run at scale `s`. Glyphs are never rotated.

### 5.4 Backends

The only virtual interface of the layer (unchanged from functional-layer 5.7):

```cpp
class SteamiDisplayBackend {
 protected: ~SteamiDisplayBackend() = default;   // non-virtual: no deleting destructor needed
 public:
  virtual bool begin() = 0;                       // false = NO_MEMORY
  virtual const char* chip() const = 0;           // "SSD1327", "GC9A01", "memory"
  virtual int16_t width() const = 0;  virtual int16_t height() const = 0;  virtual bool isColor() const = 0;
  virtual void fill(uint16_t rgb565) = 0;  virtual void show() = 0;
  virtual void pixel(int16_t x, int16_t y, uint16_t rgb565) = 0;                         // already clipped
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t rgb565) = 0; // already clipped
  virtual void rotate(bool upsideDown) = 0;  virtual void invert(bool on) = 0;
  virtual void powerOn() = 0;  virtual void powerOff() = 0;
};
```

- `SteamiSsd1327Backend`: holds the driver (`SteamiSSD1327` on target; an `SSD1327_SPI` on the internal SPI
  pins over the SPI mock under `UNIT_TEST`, since `SteamiSSD1327` is `#ifdef ARDUINO`). Converts each colour
  with `SteamiRgb::fromRgb565(v).gray4()` (a one-entry cache of the last colour is enough), not colour,
  128x128. Its constructor allocates the 8 KB buffer, which is why its slot is built only on V1a/V1b.
- `SteamiGc9a01Backend`: holds the driver, passes RGB565 through, colour, 240x240; `begin()` returns the
  driver's `begin()` (false when the 115 200 B `malloc` fails).
- `SteamiMemoryBackend` (tests only): an RGB565 array of any size (128 and 240 used), a colour or grey mode
  (grey mode stores `gray(gray4())` so a dump shows what the OLED shows), a counter of calls outside the
  bounds (must stay 0: proves the layer clipped), counters of `pixel`/`fillRect` calls, and `writePpm(path)`.

A future optional `virtual void showRect(int16_t x, int16_t y, int16_t w, int16_t h)` (partial GC9A01 refresh)
can be added without changing the student API.

### 5.5 Board slots

Added to `SteamiBoard` (functional-layer 5.2):

```cpp
SteamiDisplayBackend* display();                   // backend of this release, built, not begun; the injected
                                                   // one if any; nullptr on an UNKNOWN release
void setDisplayBackend(SteamiDisplayBackend& b);   // tests or an external panel; before the first beginDisplay()
SteamiStatus beginDisplay();                       // internal: begins the backend once and caches the result:
                                                   // OK, NO_MEMORY (E31) or UNKNOWN_BOARD (E30)
```

- Two `SteamiLazy` slots, `SteamiLazy<SteamiSsd1327Backend>` and `SteamiLazy<SteamiGc9a01Backend>`, plus an
  injected-backend pointer and one cached display status. `display()` routes `SteamiFunction::SCREEN` with
  `steamiRoute()` and builds only that slot; a sketch without a screen builds neither.
- `built(SteamiChip::SSD1327)` and `built(SteamiChip::GC9A01)` report the slots; `printInfo()` lists the screen
  chip and its state; `end()` destroys the slots (the drivers free their buffers) and forgets the status.
  `claim()` keeps answering `NOT_STARTED` for the display chips: displays go through `beginDisplay()`.
- New codes in `SteamiTypes.h`: `STEAMI_E_DISPLAY_UNKNOWN_BOARD = 30`, `STEAMI_E_NO_MEMORY = 31`.
- `sizeof(SteamiBoard)` stays under the 2.5 KB native gate (the two slots hold driver objects of a few dozen
  bytes, not their buffers).

### 5.6 SteamiDisplay

```cpp
class SteamiDisplay {
 public:
  explicit constexpr SteamiDisplay(SteamiBoard& board = Steami) noexcept;   // stores the address only
  bool begin();                       // beginDisplay(); false + one message (E30, E31); idempotent
  bool isAvailable() const;  SteamiStatus status() const;  const char* error() const;
  bool isColor();
  int16_t width();  int16_t height();  int16_t centerX();  int16_t centerY();  int16_t radius();

  void clear(SteamiRgb c = STEAMI_BLACK);
  void pixel(int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE);
  void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c = STEAMI_WHITE);
  void hline(int16_t x, int16_t y, int16_t w, SteamiRgb c = STEAMI_WHITE);
  void vline(int16_t x, int16_t y, int16_t h, SteamiRgb c = STEAMI_WHITE);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, SteamiRgb c = STEAMI_WHITE, bool fill = false);
  void circle(int16_t cx, int16_t cy, int16_t r, SteamiRgb c = STEAMI_WHITE, bool fill = false);
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                SteamiRgb c = STEAMI_WHITE, bool fill = false);
  // Ring of outer radius r and radial thickness width; degrees, 0 = noon (12 o'clock), clockwise,
  // the same convention as compass() and watch().
  void arc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t sweepDeg,
           SteamiRgb c = STEAMI_WHITE, uint8_t width = 1);
  void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);
  int16_t textWidth(const char* s, uint8_t scale = 1) const;   // glyphs, not bytes
  void show();
  void rotate(bool upsideDown);  void invert(bool on);  void powerOn();  void powerOff();
  SteamiDisplayBackend* backend();
  static constexpr uint8_t CHAR_W = 8, CHAR_H = 8;

 protected:   // for SteamiScreen, not for the student
  constexpr SteamiDisplay(SteamiBoard& board, const char* who) noexcept;   // "SteamiScreen" in messages
  void textSmall(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale = 1);   // 5x7
  int16_t textSmallWidth(const char* s, uint8_t scale = 1) const;
  void strokeLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t w, SteamiRgb c);
  void strokeCircle(int16_t cx, int16_t cy, int16_t r, uint8_t w, SteamiRgb c);   // arc(…, 0, 360, c, w)
 private:
  SteamiBoard* _board;  const char* _who;  SteamiStatus _status = SteamiStatus::NOT_STARTED;
};
```

Rules:
- **Coordinates are physical pixels** of the panel (decision 1), `int16_t`, origin top-left, x right, y down.
  `centerX() = width()/2`, `centerY() = height()/2`, `radius() = min(width, height)/2` (64 and 120).
  `width()` and the others are valid before `begin()` (from `display()`); 0 on an unknown release.
- **Every object shares the board's backend**: two `SteamiScreen` objects draw into the same buffer. The object
  holds a board pointer, a message name and a status: under 16 B, no buffer. Constant-initialised, so a
  global `SteamiScreen screen;` is safe whatever the static-init order.
- **Auto-begin**: like the function classes, the first drawing call begins the display if the sketch did
  not. After a failure every drawing call and `show()` does nothing; `error()` says why.
- **Clipping** is done in `int16_t` (with `int32_t` intermediates) before any backend call: rectangles are
  intersected with the panel; `pixel()` off-panel is dropped. No circular mask.
- **Algorithms**: Bresenham lines; midpoint circles (8-way symmetric); filled circles and triangles as
  horizontal spans (`fillRect` of height 1, never per-pixel); triangles by integer scanline with the same span
  for any vertex order; strokes of width w > 1 with a w×w square brush along the Bresenham path; `rect` as
  four spans.
- **Angles**: `steamiSinQ14(deg)` from a 91-entry quarter-wave table of `int16_t` in Q14 (182 B of flash),
  any integer degree, normalised modulo 360. A point at angle a and distance d from the centre is
  `(cx + rnd(d·sin a), cy − rnd(d·cos a))`, where `rnd` rounds Q14 to nearest, halves away from zero, so
  shapes stay symmetric. Float angles from widgets are rounded to the nearest degree first.
- **Arc** (the one non-obvious algorithm; fixes the artefacts of #351):

  ```text
  arc(cx, cy, r, start, sweep, c, w):
    if sweep <= 0 or w == 0 or r < 0: return
    if sweep >= 360: sectors = [full]          else split [start, start+sweep] into sectors of at most 180°
    ri = max(0, r - w + 1)                      # inner radius, inclusive
    for dy in -r..r:                            # rows, clipped to the panel
      xo = largest x with 4(x² + dy²) <= (2r + 1)²            # outer half-chord, integer isqrt
      xi = smallest x >= 0 with 4(x² + dy²) >= (2ri - 1)²     # inner hole (0 when ri == 0)
      for each span [-xo, -xi] and [xi, xo]:                  # one span when xi == 0
        walk dx across the span, keep the pixels with
          cross(S, (dx, dy)) >= 0 and cross((dx, dy), E) >= 0  for some sector (S, E)
        emit each contiguous run as one fillRect(cx + dx0, cy + dy, run, 1)
    # S = (sin s, -cos s), E = (sin e, -cos e) in Q14; cross(u, v) = ux·vy - uy·vx, int32 (y down:
    # clockwise is positive). A pixel exactly on a shared edge belongs to both neighbours: draw order decides.
  ```

  Each pixel is decided once, by distance and two integer cross products: no holes, no double strokes, no
  `sin` per pixel. `arc(…, 0, 360, …)` is a ring; two arcs that share an edge leave no gap.

### 5.7 SteamiScreen

```cpp
enum class SteamiAt   : uint8_t { N, NE, E, SE, S, SW, W, NW, CENTER };   // MicroPython's strings
enum class SteamiFace : uint8_t { HAPPY, SAD, SURPRISED, SLEEPING, ANGRY, LOVE };

class SteamiValue {   // what value() shows; implicit by design, one constructor per student type
 public:
  SteamiValue(int v);  SteamiValue(unsigned v);  SteamiValue(long v);  SteamiValue(unsigned long v);
  SteamiValue(float v);  SteamiValue(double v);   // double stored as float
  SteamiValue(const char* text);                  // nullptr shows "--"
  // Writes the text into out (at most size - 1 chars): integers as is, floats with `decimals`
  // digits, "--" for NAN, ±INF, nullptr and floats beyond ±999999999; "-0.0" prints "0.0".
  size_t format(char* out, size_t size, uint8_t decimals) const;
 private:
  enum class Kind : uint8_t { INT, UINT, FLOAT, TEXT } _kind;
  union { long i; unsigned long u; float f; const char* s; } _v;
};

template <size_t N> class SteamiHistory {   // ring of the last N values, no heap, 4N + 4 bytes
 public:
  void add(float v);
  size_t size() const;                // values stored, <= N
  static constexpr size_t capacity() { return N; }
  float operator[](size_t i) const;   // 0 = oldest; NAN past size()
  float last() const;                 // NAN when empty
  void clear();
 private:
  float _data[N];  uint16_t _head = 0, _count = 0;   // static_assert(N >= 1 && N <= 65535)
};

class SteamiScreen : public SteamiDisplay {
 public:
  explicit constexpr SteamiScreen(SteamiBoard& board = Steami) noexcept;
  uint8_t maxChars();                 // width() / (8·k): 16 on 128, 15 on 240

  // Level 1: widgets (device.py order: value, unit, at, label, color, scale, y_offset)
  void title(const char* text, SteamiRgb c = STEAMI_GRAY);
  void subtitle(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr,
                SteamiRgb c = STEAMI_DARK);
  void subtitle(const char* l1, SteamiRgb c);
  void subtitle(const char* l1, const char* l2, SteamiRgb c);

  void value(SteamiValue v, const char* unit = nullptr, SteamiAt at = SteamiAt::CENTER,
             const char* label = nullptr, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 2,
             int16_t yOffset = 0);
  void value(SteamiValue v, const char* unit, SteamiRgb c);
  void setDecimals(uint8_t d);        // digits for float values, default 1, at most 4

  void bar(float v, float maxV = 100, int16_t yOffset = 0, SteamiRgb c = STEAMI_LIGHT);
  void bar(float v, float maxV, SteamiRgb c);
  void gauge(float v, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT, uint8_t arcWidth = 0);
  template <size_t N> void graph(const SteamiHistory<N>& h, float minV = 0, float maxV = 100,
                                 SteamiRgb c = STEAMI_LIGHT);   // forwards to a non-template .cpp function
  void graph(const float* data, size_t n, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT);
  template <size_t N> int menu(const char* const (&items)[N], int selected = 0, SteamiRgb c = STEAMI_WHITE);
  int menu(const char* const* items, size_t n, int selected = 0, SteamiRgb c = STEAMI_WHITE);
  void compass(float headingDeg, SteamiRgb c = STEAMI_LIGHT);
  void watch(uint8_t h, uint8_t m, uint8_t s = 0, SteamiRgb c = STEAMI_LIGHT);
  void face(SteamiFace f, bool compact = false, SteamiRgb c = STEAMI_LIGHT);
  void face(SteamiFace f, SteamiRgb c);
  void face(const uint8_t bitmap[8], bool compact = false, SteamiRgb c = STEAMI_LIGHT);

  // Level 2: text at a cardinal position, glyph scale × k; text(s, x, y, …) is inherited and physical
  void text(const char* s, SteamiAt at = SteamiAt::CENTER, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);
  using SteamiDisplay::text;
 private:
  uint8_t _decimals = 1;
};
```

Immediate mode, as MicroPython: `clear()`, widgets in call order, `show()`; later calls paint over earlier
ones (gauge before title and value).

Vocabulary table (also a wiki page "MicroPython ↔ Arduino"):

| MicroPython | Arduino |
|---|---|
| `screen = Screen(display)` | `SteamiScreen screen;` then `screen.begin();` first in `setup()` |
| `screen.value(v, unit="C", at="W", label="TEMP", color=C)` | `screen.value(v, "°C", SteamiAt::W, "TEMP", C)` |
| `screen.subtitle("a", "b", color=C)` | `screen.subtitle("a", "b", C)` (3 lines at most) |
| `screen.bar(v, max_val=100, color=C)` | `screen.bar(v, 100, C)` |
| `screen.gauge(v, min_val, max_val, color=C)` | `screen.gauge(v, min, max, C)` |
| `screen.graph(data, min_val, max_val)` | `screen.graph(history, min, max)` with `SteamiHistory<N>` |
| `screen.menu(items, selected=i)` | `i = screen.menu(ITEMS, i);` (array size deduced, index wrapped) |
| `screen.face("happy", color=C)` | `screen.face(SteamiFace::HAPPY, C)` |
| `screen.text("x", at="NE", scale=2)` | `screen.text("x", SteamiAt::NE, STEAMI_WHITE, 2)` |
| `"N" "NE" … "CENTER"`, `BLACK … YELLOW` | `SteamiAt::N … SteamiAt::CENTER`, `STEAMI_BLACK … STEAMI_YELLOW` |
| `screen.center`, `screen.radius`, `screen.width` | `centerX()`, `centerY()`, `radius()`, `width()` |

Overload rules: `uint8_t`, `int16_t`, `uint16_t`, `bool` promote to `int`; `size_t` and `millis()` match
`unsigned`/`unsigned long` exactly; `0` picks `int` over `const char*`. `long long` is not accepted (compile
error, acceptable). `menu()` must accept both `const char* const ITEMS[]` and `const char* ITEMS[]`; if the
reference-to-array template does not bind the second on both cores, a second template overload is added.

### 5.8 Scaling rules

Let `W = width()`. Two quantities, both integer:

- `u(v) = v·W / 128` (int32, truncation toward zero): positions, lengths and offsets written in 128-px units.
  Exactly ×15/8 on 240.
- `k = (W + 64) / 128`, at least 1: 1 on 128, 2 on 240. Glyph factor **and** stroke width.

Layout rules, applied by every widget:

1. Every widget constant (margins, offsets, bar size, graph box, menu rows, radius offsets) and every widget
   offset argument (`yOffset` of `value()` and `bar()`) is in 128-px units and goes through `u()`.
2. Glyphs and strokes are physical: an 8x8 glyph at widget scale s is `8·s·k` px; a 5x7 label advances
   `6·s·k` px; a widget line, circle, tick or curve is `k` px wide.
3. Rows are computed with the device.py formulas in 128-px units, using 128-px glyph sizes (`8·s`), then mapped
   with `u()`. Columns: centred text at `centerX() − physicalWidth/2`; left-anchored (W, NW, SW) at
   `u(anchor)`; right-anchored (E, NE, SE) at `W − u(margin) − physicalWidth`.
4. Stacked text (unit under a value, label above it) is placed from the physical height of the text it hangs
   from.
5. Round widgets scale from `radius()`: `radius() − u(c)` and fractions of it.
6. Nothing a widget draws leaves the circle of radius `radius() + 1`: `title`, `subtitle` and menu items are
   truncated to the chord of their row (no ellipsis); `value()` lowers its glyph scale one step at a time
   (minimum 1) until the text fits its region (5.9).
7. Primitives and `text(s, x, y, c, scale)` apply none of this (decision 1); `text(s, at, c, scale)` uses
   `scale × k`.

On 128 px, rules 1 to 4 reduce to device.py (with the corrections of 5.9), so golden positions computed from
device.py stay valid. Result, per element:

| Element | OLED 128 (k = 1) | TFT 240 (k = 2, positions ×15/8) |
|---|---|---|
| Title | 8x8, row 20 | 16x16, row 37 |
| Value | 8x8 ×2 = 16 px | ×4 = 32 px (×3 when the region is too narrow) |
| Unit, subtitle, menu items, compass and watch letters | 8x8 | 16x16 |
| Secondary labels (axes, min/max, value label) | 5x7 | 5x7 ×2 |
| Strokes (circles, ticks, curve, axes) | 1 px | 2 px |
| Gauge | outer radius 60, thickness 7 | 113, 13 |
| Menu | 6 rows of 14 px | 6 rows of 26 px |
| Face | 11 px cells (88 px) | 20 px cells (160 px) |

A portable title has at most 10 characters (chord 93 px on 128, 174 px on 240 at the title row).

### 5.9 Widgets

Constants in 128-px units (`SteamiScreenConst.h`); "MP" marks a deliberate difference from device.py v0.26.

| Widget | Layout and behaviour |
|---|---|
| `title` | `text(s, N)` in GRAY: row `max(safeMargin(tw), 2·8 + 4)` = 20, where `safeMargin(tw) = r − isqrt(r² − (tw/2)²) + 2` in 128 units. Truncated to the chord. |
| `subtitle` | 1 to 3 lines, 8x8 (×k), DARK by default; one line sits at the S row + 8; several lines are centred on the S row, line pitch 11. Each line centred and truncated to its chord. The colour applies to every line, as MicroPython. |
| `value` | Text from `SteamiValue::format()` with `setDecimals()`; NAN gives `--`. Glyph scale `scale·k` (default 2k). Block height `16 + 5 + 8` when a unit is given, centred on the centre row, plus `yOffset`. CENTER: centred on `centerX()` (**MP**: device.py halves the width twice, #351). W/E: centred on `W/4` / `3W/4`; other positions through `_resolve`. Fit region: CENTER = chord at the value's top and bottom rows minus `u(4)`; W = from the left chord edge + `u(2)` to `centerX() − u(4)`, E symmetric. Unit: 8x8 ×k, LIGHT, centred under the value. Label: **5x7** ×k, GRAY, centred above the value with a `u(4)` gap (**MP**: 8x8). |
| `bar` | Width `W − u(40)`, height `u(8)`, centred, top at `centerY() + u(20) + u(yOffset)`. Track **GRID_DARK** (mockup 02: `#222`; **MP**: DARK); fill `width · clamp(v, 0, maxV) / maxV`; NAN or `maxV <= 0` leaves it empty. |
| `gauge` | `arc()` of 270°, start 225° (noon, clockwise; device.py's 135° in screen-math angles), outer radius `radius() − u(4)`, thickness `arcWidth` (physical) or `max(5, radius()/9)`. Track **DARK** (mockup 04: `#666`) at full sweep; fill `round(270 · ratio)` with ratio clamped to [0, 1], NAN or `maxV == minV` gives 0; **track and fill have the same thickness** (mockup 04; **MP**: fill is 2 px thicker). Min/max labels (integer part) in **5x7** ×k, GRAY, horizontally centred on the point at angle 233° (min) or 127° (max) and distance `outer − thickness − u(10)`, top row on that point (as device.py). No value, no unit (decision 3). |
| `graph` | Box: x `u(21)`, y `u(38)`, w `W − u(15) − u(21)`, h `u(52)`; axes DARK (left and bottom, stroke k); dashed middle line GRID_DARK, dash `u(3)`, gap `u(3)`. Current value (integer part of the last value, `--` if NAN) in 8x8 ×k WHITE centred at row 31. Axis labels max/mid/min in **5x7** ×k, DARK, right-aligned to `x − u(1)`, `1000` written `1k` (**MP**: 8x8, clipped by the circle). Curve of stroke k from oldest to newest across the box width; NAN points are skipped (gap in the curve); 0 or 1 point draws the frame and the value only. |
| `menu` | Rows of 14 from row 35, `min(n, (128 − 40)/14)` = 6 visible; window start `clamp(sel − visible/2, 0, n − visible)`. `selected` wrapped modulo n in signed arithmetic (−1 → n−1, n → 0) and **returned** (**MP**: no wrap, `None`). Highlight bar DARK, x `u(15)`, w `W − u(30)`, from `row − u(2)`, height of one row. Each item **centred** horizontally and truncated to its chord (**MP**: left at x 18); the selected item in `c` with `"> "` drawn to the left of the centred text (the item does not move when selected), others GRAY. `n == 0` draws nothing and returns 0. |
| `compass` | **Real rose** (decision 2): every element at angle a is drawn at `a − heading`, heading rounded to a degree. Circles of radius `R = radius() − u(12)` and `R·7/10`, DARK, stroke k; 8 ticks from `R − u(6)` to `R`, cardinal LIGHT, diagonal DARK; letters N (WHITE) and E, S, W (GRAY) centred at `R + u(5)`, glyphs upright; needle: north half in `c` from the centre to `R·85/100` toward north, half width `u(3)`, south half DARK, filled triangles; pivot filled circle `u(3)` GRAY. Heading convention of v0.34.0: 0° when the top of the screen points north, clockwise (`atan2(-x, y)`). At heading 0 the drawing is the mockup; at 90 the N letter is on the left. NAN: rose at 0, pivot, no needle. |
| `watch` | `R = radius() − u(8)`, dial circle DARK; 12 ticks, every third from `R − u(8)` LIGHT, others from `R − u(5)` GRAY; 12, 3, 6, 9 in WHITE 8x8 ×k at `R − u(15)`; hour hand `R/2`, half width `u(3)`; minute hand `3R/4`, half width `u(2)`; both in `c`; second hand line `R·85/100` GRAY stroke k; pivot `u(3)` GRAY. Integer angles: hour `(h % 12)·30 + m/2`, minute `m·6 + s/10`, second `s·6`. |
| `face` | 8x8 bitmap (const.py `FACES`, MSB left), cells of `W·11/128` (full) or `W/16` (compact, raised by half a cell), centred, filled squares in `c`. |
| `text(s, at)` | `_resolve(at, len, scale)` of device.py under rules 3 and 6. |

### 5.10 Colour to grey, minimum contrast

- Conversion: `gray4()` of 5.2 (BT.601 on the expanded 565, as `colors.py`). The TFT receives RGB565 unchanged.
- On a grey panel (`!isColor()`), a widget foreground drawn over the widget's own background (bar fill over
  its track, gauge fill over its track, selected menu text over the highlight bar) is lifted to at least
  **background + 4 grey levels** (`STEAMI_MIN_CONTRAST = 4`, capped at 15): BLUE (6) over the DARK gauge track
  becomes 10, RED (8) becomes 10; BLUE over the GRID_DARK bar track becomes 7. The functional-layer
  WeatherStation's `gauge(h, 0, 100, STEAMI_BLUE)` is therefore visible on V1a/V1b. No effect on the TFT.
- Primitives never adjust colours: `rect(…, STEAMI_BLUE)` on a `STEAMI_DARK` background stays invisible on the
  OLED; the documentation says so.
- Default colours follow the mockups: title GRAY, value WHITE, unit LIGHT, subtitle DARK, value label and
  min/max GRAY, graph axes and labels DARK, bar track GRID_DARK, gauge track DARK, fills LIGHT.

### 5.11 Robustness

- NAN (what a function class returns when its sensor is missing) never crashes a widget: `value()` shows
  `--`, `bar()` and `gauge()` stay empty, `graph()` skips the point, `compass()` draws no needle. `int` -1
  from `SteamiDistance` simply clamps to the minimum.
- Every widget accepts empty or `nullptr` strings (draws nothing for that part).
- Widgets use at most 64 B of stack (formatting buffers of 16 B, triangle vertices); no copy of menu items or
  history.

### 5.12 Memory and refresh-time budget

| | V1a/V1b (SSD1327) | V1c (GC9A01) |
|---|---|---|
| Frame buffer (driver) | 8 192 B, `new` in the constructor | 115 200 B, `malloc` in `begin()` |
| `SteamiDisplay` / `SteamiScreen` object | under 16 B, no buffer | same |
| Board slots | two backend objects in `.bss`, a few dozen bytes each | same |
| `SteamiHistory<N>` | 4N + 4 B in the sketch | same |
| Widget stack | at most 64 B | same |
| Flash | 8x8 ≈ 770 B, 5x7 ≈ 480 B, faces 48 B, sine 182 B, code ≈ 8 to 12 KB | same |

Refresh time:
- GC9A01 `show()`: 115 200 B at 16 MHz is **57.6 ms at least**, about 15 frames/s full screen. `clear()`
  about 1 ms.
- SSD1327 `show()`: 8 192 B at 10 MHz is 6.6 ms in theory, 10 to 15 ms with the driver's byte loop (a block
  transfer is a separate driver improvement).
- Drawing: a few thousand virtual `fillRect` calls per frame, under 2 ms at 64 MHz; fills go through spans.
- Courses use `delay(200)` to `delay(1000)`: no constraint. A 20 frames/s compass on V1c is limited by
  `show()`; the later `showRect()` (dirty rectangle = union of the previous and current drawing) would lift
  that without changing the student API.

### 5.13 Messages

Same format and logger as the functional layer, `who` = the class the sketch uses (`SteamiDisplay` or
`SteamiScreen`), once per (who, code):

```
[STeaMi] SteamiScreen: cannot tell if this board is a V1a/V1b or a V1c. Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)
[STeaMi] SteamiScreen: not enough RAM for the colour screen (115200 bytes in one block). Call screen.begin() first in setup(). (E31)
```

`error()` gives the short form for `screen.text()`: `"unknown board"`, `"no memory"`. The French table on the
wiki gains E30 and E31.

## 6. The Arduino program of each mockup

Mockups: `https://github.com/steamicc/micropython-steami-tutorials/blob/main/docs/mockups/<name>_ref.png`.
Common header, not repeated: `#include <STeaMi.h>`, `SteamiScreen screen;`, and `screen.begin();` first in
`setup()`, then the sensors' `begin()`. Each program is the example named in brackets.

**01 Temperature** (`01_temperature_ref.png`, `ScreenTemperature`)
```cpp
SteamiTemperature temperature;
void loop() {
  screen.clear();
  screen.title("Temperature");
  screen.value(temperature.read(), "°C");
  screen.subtitle(temperature.chip());      // "WSEN-HIDS" on V1a/V1b, "BME280" on V1c
  screen.show();
  delay(500);
}
```

**02 Battery** (`02_battery_ref.png`, `ScreenBattery`)
```cpp
SteamiBattery battery;
void loop() {
  screen.clear();
  screen.title("Battery");
  screen.value(battery.percent(), "%");     // unit under the value (mockup: "72%" inline)
  screen.bar(battery.percent(), 100, STEAMI_GREEN);
  screen.subtitle("BQ27441");
  screen.show();
  delay(1000);
}
```

**03 Comfort, two values** (`03_comfort_dual_ref.png`, `ScreenComfort`)
```cpp
SteamiTemperature temperature;
SteamiHumidity humidity;
void loop() {
  float t = temperature.read(), h = humidity.read();
  bool comfortable = t >= 19 && t <= 24 && h >= 40 && h <= 60;
  screen.clear();
  screen.title("Comfort");
  screen.vline(screen.centerX(), screen.centerY() - screen.radius() / 2, screen.radius());
  screen.value(t, "°C", SteamiAt::W, "TEMP");
  screen.value((int)h, "%", SteamiAt::E, "HUM");
  screen.subtitle(comfortable ? "Comfortable" : "Uncomfortable", STEAMI_GREEN);
  screen.show();
  delay(1000);
}
```

**04 Circular gauge** (`04_circular_gauge_ref.png`, `ScreenGauge`)
```cpp
SteamiDistance distance;
void loop() {
  int d = distance.read();
  screen.clear();
  screen.gauge(d, 0, 500);                  // first: text paints over the arc
  screen.title("Distance");
  screen.value(d, "mm");
  screen.subtitle("VL53L1X");
  screen.show();
  delay(200);
}
```

**05 Scrolling graph** (`05_scrolling_graph_ref.png`, `ScreenGraph`)
```cpp
SteamiLight light;                          // V1a/V1b only; on V1c the example uses SteamiPressure, 950..1050
SteamiHistory<20> history;
void loop() {
  history.add(light.read());
  screen.clear();
  screen.title("Light");
  screen.graph(history, 0, 1000);
  screen.subtitle(light.isAvailable() ? "20s window" : light.error());
  screen.show();
  delay(1000);
}
```

**06 D-pad menu** (`06_dpad_menu_ref.png`, `ScreenMenu`)
```cpp
SteamiButtons buttons;
const char* const ITEMS[] = {"Temperature", "Humidity", "Distance", "Light", "Battery", "Proximity"};
int selected = 0;
void loop() {
  if (buttons.wasPressed(SteamiButton::UP)) selected--;
  if (buttons.wasPressed(SteamiButton::DOWN)) selected++;
  screen.clear();
  screen.title("Menu");
  selected = screen.menu(ITEMS, selected);  // wraps: -1 becomes 5
  screen.show();
  delay(20);
}
```

**07 Compass** (`07_compass_ref.png`, `ScreenCompass`)
```cpp
SteamiCompass compass;
void loop() {
  screen.clear();
  screen.compass(compass.heading());        // the needle points to real north
  screen.show();
  delay(50);
}
```

**08 Faces and reactive smiley** (`08_smiley_{happy,sad,surprised,angry,sleeping,love}_ref.png` and
`08_smiley_reactive_ref.png`, `ScreenFaces`)
```cpp
SteamiDistance distance;
void loop() {
  int d = distance.read();
  screen.clear();
  screen.title("Mood");
  if (d < 50)       { screen.face(SteamiFace::SURPRISED, STEAMI_YELLOW); screen.subtitle("SURPRISED"); }
  else if (d < 150) { screen.face(SteamiFace::HAPPY, STEAMI_GREEN);      screen.subtitle("HAPPY"); }
  else if (d < 300) { screen.face(SteamiFace::SLEEPING);                 screen.subtitle("SLEEPING"); }
  else              { screen.face(SteamiFace::SAD, STEAMI_RED);          screen.subtitle("SAD"); }
  screen.show();
  delay(200);
}
```
The six full-screen faces are `screen.face(SteamiFace::HAPPY);` and so on (button A cycles them in the
example).

**09 Watch** (`09_watch_ref.png`, `ScreenWatch`)
```cpp
const uint32_t START = 10 * 3600UL + 10 * 60;   // start time set by hand: 10:10:00
void loop() {
  uint32_t s = START + millis() / 1000;
  screen.clear();
  screen.watch((s / 3600) % 24, (s / 60) % 60, s % 60);
  screen.show();
  delay(500);
}
```

What the student meets: one `screen` object, `clear` … `show`, one object per sensor, `SteamiAt`, `STEAMI_*`,
`SteamiFace`, `SteamiHistory`. No bus, no chip, no scale factor.

Known differences from the mockups, accepted: the 8x8 bitmap font instead of the anti-aliased TTF (smaller,
thinner value); the battery unit under the value; the 03 and 08-reactive subtitles in one colour (mockup:
first line coloured, second grey); the screen bezel ring drawn in every mockup is the panel edge, not a widget.

## 7. Testing strategy

Native suites (`tests/native`, Unity, env:native and an AddressSanitizer + UBSan build of the display suites):

1. `test_steami_display` (3a), on `SteamiMemoryBackend` at 128 and 240, colour and grey:
   - `SteamiRgb`: gray4 and rgb565 of every const.py colour against golden values from a 10-line Python
     script; `operator==`; no implicit conversion from an integer (compile-fail script).
   - `SteamiColor::rgb()` normalisation over the APDS-9960 register mock (largest channel 255, zeros black).
   - Fonts: glyph sizes at scales 1 to 4, `textWidth("°C") == 16`, `"é"` draws `e`, invalid UTF-8 draws `?`.
   - Clipping: `pixel(-1, 0)`, `pixel(200, 5)`, text at y 125 and x −20, line (−50, −50)–(300, 300), circles and
     arcs crossing every edge; the out-of-bounds counter stays 0 and ASan stays silent.
   - Shapes: circles 8-way symmetric; filled triangle identical for the 6 vertex orders; `arc(…, 0, 360)` equals
     the ring defined by the distance test; `arc(0, 90) ∪ arc(90, 90) = arc(0, 180)` with no gap; sweep 0 draws
     nothing; stroke widths 1 and 2.
   - PPM dump of a primitives gallery, uploaded as a CI artefact.
2. `test_steami_display_board` (3b), over the SPI and Wire mocks: V1C never builds the SSD1327 slot, V1AB
   never builds the GC9A01 slot, a sketch without a screen builds neither; UNKNOWN gives E30 and no slot; an
   injected backend whose `begin()` fails gives E31, `status() == NO_MEMORY`, and later drawing calls do
   nothing; two `SteamiScreen` objects share one backend; the SSD1327 backend under ASan with the clipping
   cases above; `sizeof(SteamiBoard)` under 2.5 KB.
3. `test_steami_screen` (4a to 4c), on `SteamiMemoryBackend` at 128 and 240:
   - golden positions at 128 for every `SteamiAt` and every widget anchor, computed from device.py with the
     corrections of 5.9 (a small Python script, kept in `tests/native/test_steami_screen/`), and at 240 from the
     rules of 5.8;
   - every widget, with long strings and extreme values, leaves no pixel outside the circle of radius
     `radius() + 1`; title in the top band, subtitle in the bottom band;
   - `value()` overload smoke test with `0`, `23`, `23.5`, `23.5f`, `millis()`, `analogRead()`, `uint8_t`,
     `int16_t`, `size_t`, `"text"`, NAN; `"--"` for NAN; fit step-down at 240 for W/E;
   - `gauge(50, 0, 100)` lights half of the track within 2 %; track and fill of equal thickness;
   - `compass()` at 0, 90, 180, 270: needle tip and N letter positions; NAN draws no needle;
   - `watch(3, 0, 0)` hand directions; faces equal const.py; menu window and wrap (−1, n, 2n + 1, n == 0);
     `graph` with 0, 1 and N points and NAN gaps; `menu()` with `const char* const[]` and `const char*[]`;
   - grey contrast: a BLUE gauge fill on the grey backend is at least level 10, a BLUE bar fill at least 7.
4. `test_steami_screen_mockups` (grows with 4a to 4c): renders the 9 mockups (15 images with the six faces)
   at 128 and 240, with the inputs read from the SVGs (23.5 °C, 72 %, 342 mm, menu index 2, heading 0, faces),
   on the grey backend at 128 and the colour backend at 240, and writes PPM files. A script
   (`scripts/mockup-review.py`, Pillow + NumPy) fetches the tutorials' `_ref.png` at a pinned commit, resizes
   them to 128 and 240, masks outside the disc, computes the SSIM as `validate.py` does, and writes PNGs and a
   side-by-side page uploaded as a CI artefact. **A review tool, not a gate**: the indicative threshold is
   about 0.7 (bitmap font against TTF); a drop between two PRs is the signal to look.

Build and package CI: every new example compiles with arduino-cli (core 3.1.0) and PlatformIO (core 2.12);
`STeaMi.h` includes the new headers; duplicate-name check; footprint gate (flash under the cap with margin,
WeatherStation `.data + .bss` leaving at least 140 KB).

Hardware checks (PlatformIO env:steami, V1b and V1c):
- `Drawing` and each mockup example on V1b and V1c, compared by eye with `_ref.png`. The webcam mounted over
  the V1c captures the real screen for review (photos attached to the PR); the V1b can be placed under it the
  same way.
- On V1c, `screen.begin()` succeeds after STM32duinoBLE is initialised; `screen.begin()` called late (after
  large allocations) reports E31 and the sketch goes on.
- The `Drawing` example prints the measured `show()` time on both panels; the values go into the PR.
- WeatherStation, unchanged, on V1a/V1b/V1c (`scripts/qualify.py`).

## 8. Deliveries, in order

Each delivery is one issue and one PR branched from `main`, with tests and examples.

| # | Delivery | Content | Done when |
|---|---|---|---|
| 3a | Colours and drawing in memory | `SteamiRgb.h` + `SteamiColor::rgb()`, `SteamiFont` (8x8 + degree + folding, 5x7), `SteamiTrig`, `SteamiDisplayBackend`, `SteamiMemoryBackend`, `SteamiDisplay` drawn on an injected backend (clipping, lines, circles, triangles, integer arc, strokes, UTF-8 text) | `test_steami_display` passes natively, under ASan/UBSan too, and its PPM gallery is in the CI artefacts. No hardware. |
| 3b | Backends and board | `SteamiSsd1327Backend`, `SteamiGc9a01Backend`, board slots, `display()`, `setDisplayBackend()`, `beginDisplay()`, E30/E31, `Drawing` example, footprint gate | `test_steami_display_board` passes; `Drawing` builds on both cores and looks the same on V1b and V1c (photos); V1c `begin()` with BLE initialised succeeds; `show()` times recorded. |
| 4a | Layout | `SteamiScreen`, `u()` and `k`, `SteamiAt`, `_resolve`/`safeMargin`, `title`, `subtitle`, `SteamiValue`, `value` (fit, NAN), `setDecimals`, `text(at)`, `bar`, contrast rule | 128 goldens pass; mockups 01, 02, 03 render natively at 128 and 240 and their examples run on V1b and V1c, reviewed against `_ref.png`. |
| 4b | Round widgets | `gauge`, `compass` (real rose), `watch`, `face` + `SteamiFace` + bitmaps | Mockups 04, 07, 08 (seven images), 09 rendered and their examples reviewed on V1b and V1c; compass checked against a real heading on the board. |
| 4c | Data and navigation | `SteamiHistory`, `graph`, `menu`; `WeatherStation`; wiki page (widgets, vocabulary table, E30/E31) | Mockups 05, 06 reviewed on V1b and V1c; WeatherStation runs unchanged on V1a/V1b/V1c. Sub-project 4 is done: **every mockup has its Arduino example, reviewed against `_ref.png` on V1b and V1c.** |

## 9. Decisions

Maintainer, 2026-11-25 (the five recommendations of the design study):

1. Primitives called on `screen`/`display` take **physical pixels**, as MicroPython; portability through
   `centerX()`, `centerY()`, `radius()`. Tutorial 03 is fixed to use the centre and radius.
2. `compass(heading)` draws a **real compass rose** turned by −heading; at heading 0 it is the mockup. Same in
   both languages (MicroPython PR linked to #434). Heading convention as measured in v0.34.0.
3. `gauge()` draws the **arc only** (+ min/max labels); the value comes from a separate `value()`. Tutorial 04
   and the mockups README are updated.
4. An **internal 5x7 font** for secondary labels, invisible to the student; no big-digits font in v1
   (revisit after the V1c review on the board).
5. The **128 → 240 scaling rule** (×15/8 for positions and sizes, k = 1 or 2 for glyphs and strokes) applies
   in Arduino now; MicroPython gets the same rule in a separate issue completing #368.

Designer's calls (reversible): grey minimum contrast and GRID_DARK bar track; centred, wrapping `menu()`;
NAN shows `--`; `SteamiValue`; `arc()` angles 0° = noon, clockwise. Taken while writing this document:
gauge track DARK as in mockup 04 (the bar keeps GRID_DARK from mockup 02); widget `yOffset` in 128-px units;
`menu()` returns `int`; compass letters move with the rose but stay upright, and NAN keeps the rose with no
needle; subtitle colour applies to every line; own number formatter instead of `dtostrf`; `beginDisplay()` on
the board rather than extending `claim()`; mockup examples instead of one `ScreenWidgets`.

## 10. Relation to the design study

The study (2026-11-25, in French, from the mockups) is the source of sections 4 to 8. Its open questions are
answered by section 9, items 1 to 5. Its findings that shape the design: the MicroPython module has never
run on the GC9A01 and does not scale text there (#349, #368), so the old open question 2 ("follow MicroPython
unless its V1c backend already scales") is answered by "it does not"; two copies of `steami_screen` diverge
(tutorials' February copy and lib v0.26), and parity targets the lib, corrected.

## 11. Changes to the functional-layer spec

1. Sections 5.7 and 5.8: superseded by this document (a note under its status line says so).
2. 5.1 Files: `SteamiFont8x8` becomes `SteamiFont` (8x8 + 5x7); add `SteamiTrig`, `SteamiValue`; examples
   `Drawing` and the nine mockup examples instead of `ScreenWidgets`.
3. 5.2: `SteamiBoard` gains `beginDisplay()`; `display()` returns nullptr on an unknown release; E30/E31
   constants in `SteamiTypes.h`.
4. `SteamiRgb`: `operator==`/`!=`; `arc()` default width 1, angles 0° = noon clockwise, outer radius + radial
   thickness, integer scanline algorithm; protected 5x7 text and stroke helpers.
5. `value()`: one `SteamiValue` parameter instead of six overloads, plus the colour shortcut; NAN shows `--`;
   own formatter instead of `snprintf`/`dtostrf`.
6. Scaling rule of decision 9 formalised (5.8): `u()`, `k` for glyphs and strokes, rows in 128-px units,
   physical centring, fit and truncation.
7. Grey minimum contrast; bar track GRID_DARK, gauge track DARK.
8. `gauge()` start 225° (was `STEAMI_GAUGE_START_ANGLE 135` in screen-math angles), no value, equal thickness;
   `menu()` takes an `int`, wraps, returns the index, centres items; `compass()` is a real rose.
9. Section 6: the WeatherStation's `gauge(h, 0, 100, STEAMI_BLUE)` is now visible on V1a/V1b; its compass page
   shows north.
10. Section 7 suites 4 and 5: replaced by section 7 here; PPM dumps are reviewed against the tutorials'
    `_ref.png` with an SSIM, not against MicroPython SimBackend screenshots.
11. Section 8 items 3 and 4: split into 3a, 3b, 4a, 4b, 4c; the "done" of sub-project 4 becomes "every mockup
    has its Arduino example, reviewed against `_ref.png` on V1b and V1c".
12. Section 9: the five decisions above; the former open question 2 is closed.

## 12. Follow-ups in other repositories (separate PRs)

`steamicc/micropython-steami-lib`, `lib/steami_screen`:
- `value()` centred and arc without artefacts (#351); gauge track and fill of equal thickness, outer radius
  `radius − 4`; bar track GRID_DARK; grey minimum contrast; NAN shows `--`.
- Real compass rose, linked to #434 (and #371).
- GC9A01 wrapper with the 128 → 240 rule (×15/8, k = 2, a real `draw_scaled_text`): new issue completing #368,
  before the first courses on V1c.
- `menu()` centred, wrapping, returning the index; graph axis labels placed so they stay inside the circle.

`steamicc/micropython-steami-tutorials`:
- Tutorial 03: the separator drawn from `screen.center` and `screen.radius` instead of `line(64, 32, 64, 96)`.
- Tutorial 04 and `docs/mockups/README.md`: `gauge(dist, 0, 500)` then `value(dist, unit="mm")`.
- Tutorial 07: explain that the needle points north.
- `docs/design-constraints.md` becomes the reference shared by both languages (scaling rule, contrast, fonts).

STeaMi wiki: an Arduino screen page with the vocabulary table of 5.7, and E30/E31 in the French error table.

## 13. Left to the builder

- Exact rounding where device.py's float formulas become integers (the 128 goldens decide), the golden
  generation script, and the per-widget constants not listed in 5.9.
- The degree glyph design in both fonts and the exact accent-folding table.
- The SSD1327 backend colour cache, span merging and other speed details, as long as fills use spans.
- Whether the native build uses a separate ASan environment or flags in env:native; where the compile-fail
  checks live.
- The mockup-review script (pinned commit, resizing, masking, page layout) and the indicative SSIM threshold.
- A second `menu()` template if `const char* ITEMS[]` does not bind; the example that cycles the six faces.
- Splitting a delivery in two PRs if it grows, keeping the order 3a, 3b, 4a, 4b, 4c.
