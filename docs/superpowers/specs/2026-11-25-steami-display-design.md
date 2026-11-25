# STeaMi display layer: SteamiDisplay and SteamiScreen

Status: Draft for maintainer review. The five recommendations of the design study (section 9) were
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

Vocabulary table (also in the `lib/steami_board` README; a wiki page is a separate PR, section 12):

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
   with `u()`. Exception, the N/S safe margin of `_resolve` (titles, subtitles, `text(s, N|S)`): it is computed
   in physical pixels from the physical text width, `r − isqrt(r² − (tw/2)²) + 2` with `r = radius()`, floored
   at `u(2·8·s + 4)`, so a text that is 7 % wider on V1c moves down far enough to fit *(designer's call, B3)*.
   Columns: centred text at `centerX() − physicalWidth/2`; left-anchored (W, NW, SW) at `u(anchor)`;
   right-anchored (E, NE, SE) at `W − u(margin) − physicalWidth`.
4. Stacked text (unit under a value, label above it) is placed from the physical height of the text it hangs
   from.
5. Round widgets scale from `radius()`: `radius() − u(c)` and fractions of it.
6. **Available width** *(designer's call, B3)*: the width available to a text at a given position is the chord
   of the circle of radius `r = radius()` at the pixel rows the text actually inks (the union of the inked
   rows of its glyphs, read from the font bitmaps at the drawn scale), not at the full 8-row cell; the
   narrowest of those rows decides. The text's width is its cell width (`textWidth()`).
7. Nothing a widget draws leaves the circle of radius `radius() + 1`: `title`, `subtitle` and menu items are
   truncated to their available width (no ellipsis); `value()` lowers its glyph scale one step at a time
   (minimum 1) until the text fits its region (5.9).
8. Primitives and `text(s, x, y, c, scale)` apply none of this (decision 1); `text(s, at, c, scale)` uses
   `scale × k`.

On 128 px, rules 1 to 4 reduce to device.py (with the corrections of 5.9), so golden positions computed from
device.py stay valid, except where 5.9 marks a difference ("MP"). Result, per element:

| Element | OLED 128 (k = 1) | TFT 240 (k = 2, positions ×15/8) |
|---|---|---|
| Title | 8x8, row 20 (also for "Temperature") | 16x16, row 37 for short titles, row 41 for "Temperature" |
| Value, centred | 8x8 ×2 = 16 px | ×4 = 32 px (×3 when the region is too narrow) |
| Values side by side (W and E) | ×1 = 8 px (a 4-cell value at ×2 does not fit) | ×3 = 24 px |
| Unit, subtitle, menu items, compass and watch letters | 8x8 | 16x16 |
| Secondary labels (graph min/max, gauge min/max, value label) | 5x7 | 5x7 ×2 |
| Strokes (circles, ticks, curve, axes) | 1 px | 2 px |
| Gauge | outer radius 60, thickness 7 | 113, 13 |
| Menu | 5 visible rows of 14 px | 5 visible rows of 26 px |
| Face | 11 px cells (88 px) | 20 px cells (160 px) |

Titles have no fixed character limit: as in device.py, a longer title moves down until it fits.
"Temperature" (11 cells) fits on both panels: on 128 its inked rows start at row 20, where the chord is 93 px
for 88 px of text; on 240 the physical safe margin puts it at row 41 (instead of 37), where the chord is 181 px
for 176 px of text. At row 37 it would not have fitted (chord 173 px, and the capital T inks the top row).

### 5.9 Widgets

Constants in 128-px units (`SteamiScreenConst.h`); "MP" marks a deliberate difference from device.py v0.26.

| Widget | Layout and behaviour |
|---|---|
| `title` | `text(s, N)` in GRAY: row `max(safeMargin(tw), u(2·8 + 4))`, where `safeMargin(tw) = r − isqrt(r² − (tw/2)²) + 2` in physical pixels with the physical width (rule 3): 20 on 128; 37 on 240 for short titles, 41 for "Temperature". Truncated to its available width (rule 6) only if it still does not fit. |
| `subtitle` | 1 to 3 lines, 8x8 (×k), DARK by default; one line sits at the S row + 8; several lines are centred on the S row, line pitch 11. Each line centred and truncated to its chord. The colour applies to every line, as MicroPython. |
| `value` | Text from `SteamiValue::format()` with `setDecimals()`; NAN gives `--`. Glyph scale `scale·k` (default 2k). Block height `16 + 5 + 8` when a unit is given, centred on the centre row, plus `yOffset`. CENTER: centred on `centerX()` (**MP**: device.py halves the width twice, #351). W/E: centred in their half region (below); other positions through `_resolve`. Fit, CENTER and other anchors: the available width of rule 6 minus `u(4)`, scale lowered until the text fits. **Side by side, W and E** *(designer's call, B2)*: the half region is W = [left chord edge + k, `centerX() − u(3)`), E symmetric, with the chord taken at the value's full cell rows at the scale being tried (not its inked rows, so the result does not depend on the digits). The scale is the largest physical factor, from `scale·k` down to 1, at which a **reference of `max(len, 4)` cells** fits that region. The computation uses neither the text nor the other value, and the two regions are mirror images, so two values of up to 4 characters with the same options (unit or not, same `scale`, same `yOffset`) always get the same scale: "23.5" and "45" are drawn at ×1 on 128 (4 cells at ×2 are 64 px, the region is 58 px) and ×3 on 240 (96 px in 109 px). A longer value gets its own, smaller scale; `SteamiScreen` keeps no state between calls. Unit: 8x8 ×k, LIGHT, centred under the value. Label: **5x7** ×k, GRAY, centred above the value with a `u(4)` gap (**MP**: 8x8). |
| `bar` | Width `W − u(40)`, height `u(8)`, centred, top at `centerY() + u(20) + u(yOffset)`. Track **GRID_DARK** (mockup 02: `#222`; **MP**: DARK); fill `width · clamp(v, 0, maxV) / maxV`; NAN or `maxV <= 0` leaves it empty. |
| `gauge` | `arc()` of 270°, start 225° (noon, clockwise; device.py's 135° in screen-math angles), outer radius `radius() − u(4)`, thickness `arcWidth` (physical) or `max(5, radius()/9)`. Track **DARK** (mockup 04: `#666`) at full sweep; fill `round(270 · ratio)` with ratio clamped to [0, 1], NAN or `maxV == minV` gives 0; **track and fill have the same thickness** (mockup 04; **MP**: fill is 2 px thicker). Min/max labels (integer part) in **5x7** ×k, GRAY, horizontally centred on the point at angle 233° (min) or 127° (max) and distance `outer − thickness − u(10)`, top row on that point (as device.py). No value, no unit (decision 3). |
| `graph` | Box: x `u(21)`, y `u(38)`, w `W − u(15) − u(21)`, h `u(52)`; axes DARK (left and bottom, stroke k); dashed middle line GRID_DARK, dash `u(3)`, gap `u(3)`. Current value (integer part of the last value, `--` if NAN) in 8x8 ×k WHITE centred at row 31. Axis labels *(designer's call, B1)*: **only max and min** (no middle label), integer part written in full (`950`, `1050`; no `1k`), in **5x7** ×k, DARK, **inside the plot box**: the max label's top at `y + u(2)`, the min label's bottom at `y + h − k − u(2)`; each left-aligned at `max(chord left edge at its inked rows, box left + k) + u(2)`. With the default box the box edge is the binding term on both panels (chord edge at the label rows about 5 px on 128 and 8 to 10 px on 240, box left 21 and 39 px); the chord term keeps the rule valid for any box (**MP**: three 8x8 labels left of the axis, clipped by the circle). Labels are drawn before the curve, which paints over them. Curve of stroke k from oldest to newest across the box width; NAN points are skipped (gap in the curve); 0 or 1 point draws the frame and the value only. |
| `menu` | Rows of 14 from row 35. **Visible rows** *(designer's call, B3)*: a row is used only if the chord at its full cell rows holds the marker plus 10 cells (12 cells: 96 px on 128, 192 px on 240). Chords of rows 1 to 6: 115, 125, 127, 121, 109, 85 px on 128; 213, 233, 239, 227, 201, 157 px on 240; so **5 visible rows on both panels** (`min(n, 5)`; **MP**: 6). Window start `clamp(sel − visible/2, 0, n − visible)`. `selected` wrapped modulo n in signed arithmetic (−1 → n−1, n → 0) and **returned** (**MP**: no wrap, `None`). Highlight bar DARK from `row − u(2)`, one row high, each scanline **clipped to the chord at that scanline** (from left edge + k to right edge − k), so its ends follow the circle *(B3)*. The `"> "` marker starts at the **chord's left edge** at the item's inked rows, plus `u(2)` *(B3)*. Items are **centred** horizontally (**MP**: left at x 18); the selected item starts after the marker if centring would overlap it; every item is truncated to its available width minus `u(2)`. Selected item in `c`, others GRAY. `n == 0` draws nothing and returns 0. |
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

`error()` gives the short form for `screen.text()`: `"unknown board"`, `"no memory"`. E30 and E31 are
documented in the `lib/steami_board` README; the French error table of the wiki gains them in a separate PR
(section 12).

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
SteamiLight light;                          // V1a/V1b only
SteamiPressure pressure;                    // used instead on V1c
SteamiHistory<20> history;
bool hasLight = false;
void setup() {
  screen.begin();
  hasLight = Steami.has(SteamiFunction::LIGHT) && light.begin();   // no E20 on V1c
  pressure.begin();
}
void loop() {
  history.add(hasLight ? light.read() : pressure.read());
  screen.clear();
  screen.title(hasLight ? "Light" : "Pressure");
  if (hasLight) screen.graph(history, 0, 1000);     // axis labels "1000" and "0"
  else          screen.graph(history, 950, 1050);   // axis labels "1050" and "950"
  screen.subtitle("20s window");
  screen.show();
  delay(1000);
}
```
The axis shows only its max and min labels, in full (mockup: `1k`, `500`, `0`; designer's call B1).

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

**WeatherStation** (4c; replaces the program of functional-layer section 6, same file on V1a/V1b/V1c)
```cpp
SteamiTemperature temperature;  SteamiHumidity humidity;  SteamiPressure pressure;
SteamiCompass compass;          SteamiButtons buttons;   SteamiGesture gesture;   // gesture: V1a/V1b only
SteamiHistory<40> history;                  // last 40 pressures, no heap
const char* const PAGES[] = {"Temperature", "Humidity", "Pressure", "Compass"};
int page = 0;
bool hasGesture = false;
void setup() {
  Serial.begin(115200);
  screen.begin();
  temperature.begin(); humidity.begin(); pressure.begin(); compass.begin(); buttons.begin();
  hasGesture = gesture.begin();             // on V1c: prints E20 and returns false; the sketch goes on
  Steami.printInfo();
}
void loop() {
  if (buttons.wasPressed(SteamiButton::RIGHT) || buttons.wasPressed(SteamiButton::A)) page = (page + 1) % 4;
  if (buttons.wasPressed(SteamiButton::LEFT) || buttons.wasPressed(SteamiButton::B)) page = (page + 3) % 4;
  if (hasGesture && gesture.read() == SteamiGesture::RIGHT) page = (page + 1) % 4;
  history.add(pressure.read());
  screen.clear();
  if (page == 0) {
    screen.title(PAGES[page]);
    screen.value(temperature.read(), "°C");
  } else if (page == 1) {
    float h = humidity.read();
    screen.gauge(h, 0, 100, STEAMI_BLUE);   // lifted to grey 10 on the OLED (5.10)
    screen.title(PAGES[page]);
    screen.value(h, "%");
  } else if (page == 2) {
    screen.title(PAGES[page]);
    screen.graph(history, 950, 1050, STEAMI_RED);   // labels "1050" and "950", inside the circle
    screen.subtitle("hPa");
  } else {
    screen.compass(compass.heading());      // the rose fills the screen: no extra text
  }
  screen.show();
  delay(200);
}
```
Differences from the functional-layer version: no compact face over the gauge page (it overlapped the value),
no direction text over the compass rose.

What the student meets: one `screen` object, `clear` … `show`, one object per sensor, `SteamiAt`, `STEAMI_*`,
`SteamiFace`, `SteamiHistory`. No bus, no chip, no scale factor.

Known differences from the mockups, accepted: the 8x8 bitmap font instead of the anti-aliased TTF (smaller,
thinner value); the battery unit under the value; the 03 values at ×1 on 128 (B2: the mockup's 4-character
value does not fit half the disc at ×2); graph labels min/max only, inside the box (B1); 5 menu rows instead of
6 (B3); the 03 and 08-reactive subtitles in one colour (mockup: first line coloured, second grey); the screen
bezel ring drawn in every mockup is the panel edge, not a widget.

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
     `int16_t`, `size_t`, `"text"`, NAN; `"--"` for NAN;
   - (4a) values side by side: `value(23.5, "°C", W, "TEMP")` and `value(45, "%", E, "HUM")` get the same glyph
     scale, ×1 on 128 and ×3 on 240; so do `"1"` against `"8888"`; a 6-character W value gets a smaller scale
     and still stays in its half region; the result does not depend on the order of the two calls;
   - (4a) available width at inked rows: "Temperature" as a title is drawn whole at row 20 on 128 and row 41
     on 240; a 20-character title is truncated and leaves no pixel outside the circle; a lowercase-only title
     (no top-row ink) and a title with descenders use the rows they ink;
   - `gauge(50, 0, 100)` lights half of the track within 2 %; track and fill of equal thickness;
   - `compass()` at 0, 90, 180, 270: needle tip and N letter positions; NAN draws no needle;
   - `watch(3, 0, 0)` hand directions; faces equal const.py;
   - (4c) menu: 5 visible rows on both panels, window and wrap (−1, n, 2n + 1, n == 0); the highlight bar,
     the marker and every item stay inside the circle for 6 items of 11 characters, selected at each index;
     the marker starts `u(2)` from the chord's left edge; `menu()` with `const char* const[]` and
     `const char*[]`;
   - (4c) graph: 0, 1 and N points and NAN gaps; exactly two axis labels, `"1050"` and `"950"` for
     `graph(history, 950, 1050)` and `"1000"` and `"0"` for `(0, 1000)`, both inside the plot box; **no pixel
     outside the circle for `graph(history, 950, 1050)` on 128 and 240**, with the history empty, full at 950,
     full at 1050, and full of values outside the range (clamped);
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

Hardware checks (PlatformIO env:steami) *(designer's call, B5)*:
- **V1c, always**: the controller runs the checks on the shared V1c under the webcam. Each example of the
  delivery is flashed, the webcam photographs the real screen, and the photos are compared with the mockups'
  `_ref.png` and attached to the PR.
- **V1a/V1b, if a board is available** (to be confirmed by the maintainer): the same examples, photographed the
  same way. When no V1a/V1b board is available, the OLED path is covered by the native rendering tests (grey
  memory backend at 128, SSD1327 backend over the SPI mock) and the PR says that the V1a/V1b check was not run.
- On V1c, `screen.begin()` succeeds after STM32duinoBLE is initialised; `screen.begin()` called late (after
  large allocations) reports E31 and the sketch goes on.
- The `Drawing` example prints the measured `show()` time; the value goes into the PR (V1c; V1b when
  available).
- WeatherStation, unchanged, on V1c and on any available V1a/V1b (`scripts/qualify.py`).

## 8. Deliveries, in order

Each delivery is one issue and one PR branched from `main`, with tests and examples. A delivery changes only
this repository: code, tests and examples under `lib/steami_board`, plus its README and `keywords.txt`. The
follow-ups of section 12 are outside these deliveries *(designer's call, B4)*.

"Reviewed on V1c" means photographed by the controller on the shared V1c under the webcam and compared with
`_ref.png`; "and on V1a/V1b if available" means the same on such a board when the maintainer confirms one is
available, otherwise covered by the native rendering tests, as the PR states (B5).

| # | Delivery | Content | Done when |
|---|---|---|---|
| 3a | Colours and drawing in memory | `SteamiRgb.h` + `SteamiColor::rgb()`, `SteamiFont` (8x8 + degree + folding, 5x7), `SteamiTrig`, `SteamiDisplayBackend`, `SteamiMemoryBackend`, `SteamiDisplay` drawn on an injected backend (clipping, lines, circles, triangles, integer arc, strokes, UTF-8 text); README and keywords for `SteamiRgb` and `rgb()` | `test_steami_display` passes natively, under ASan/UBSan too, and its PPM gallery is in the CI artefacts. No hardware. |
| 3b | Backends and board | `SteamiSsd1327Backend`, `SteamiGc9a01Backend`, board slots, `display()`, `setDisplayBackend()`, `beginDisplay()`, E30/E31, `Drawing` example, footprint gate; README (primitives, E30/E31) and keywords | `test_steami_display_board` passes; `Drawing` builds on both cores; reviewed on V1c (photo matches the native 240 render), and on V1a/V1b if available; V1c `begin()` with BLE initialised succeeds; V1c `show()` time recorded. |
| 4a | Layout | `SteamiScreen`, `u()` and `k`, `SteamiAt`, `_resolve`/`safeMargin`, available width, `title`, `subtitle`, `SteamiValue`, `value` (fit, side by side, NAN), `setDecimals`, `text(at)`, `bar`, contrast rule; examples 01 to 03; README and keywords | 128 goldens pass; mockups 01, 02, 03 render natively at 128 and 240 with no pixel outside the circle; their examples reviewed on V1c, and on V1a/V1b if available. |
| 4b | Round widgets | `gauge`, `compass` (real rose), `watch`, `face` + `SteamiFace` + bitmaps; examples 04, 07, 08, 09; README and keywords | Mockups 04, 07, 08 (seven images), 09 render natively at 128 and 240; their examples reviewed on V1c, and on V1a/V1b if available; on V1c the needle is checked against a known north. |
| 4c | Data and navigation | `SteamiHistory`, `graph`, `menu`; examples 05 and 06, `WeatherStation`; README (widget reference, vocabulary table) and keywords | Mockups 05, 06 render natively; their examples and WeatherStation reviewed on V1c, and on V1a/V1b if available. Sub-project 4 is done: **every mockup has its Arduino example, rendered natively at 128 and 240 and reviewed against `_ref.png` on V1c (and on V1a/V1b when a board is available).** |

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

Designer's calls answering the builder's questions (2026-11-25), for the maintainer to review; the
document marks each place they apply with B1 to B5:

- **B1, graph axis labels**: only min and max, in full (`950`, `1050`, no `1k`), 5x7, inside the plot box,
  left-aligned to the larger of the chord's left edge at their inked rows and the box edge (5.9).
- **B2, values side by side**: a W or E value is sized for a reference of `max(len, 4)` cells in its half
  region, so two values of up to 4 characters share the largest scale that fits both halves (×1 on 128, ×3 on
  240), without state between calls (5.9).
- **B3, edges**: the available width is the chord at the rows the glyphs ink, radius `radius()` (5.8 rule 6);
  the N/S safe margin uses the physical width, so "Temperature" fits on V1c at row 41; the menu bar is clipped
  to the chord, the marker starts at the chord's left edge, and 5 rows are visible on both panels (5.9).
- **B4, scope**: deliveries change only this repository (`lib/steami_board` code, tests, examples, README,
  keywords); MicroPython, tutorials, `design-constraints.md` and wiki changes are separate PRs (section 12).
- **B5, hardware**: V1c checks by the controller under the webcam against `_ref.png`; V1a/V1b checks when the
  maintainer confirms a board is available, otherwise covered by the native rendering tests (sections 7, 8).

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
   `menu()` takes an `int`, wraps, returns the index, centres items, shows 5 rows and clips its bar to the
   circle; `graph()` draws only min and max labels, in full, inside the box (`STEAMI_GRAPH_VALUE_Y` and the box
   constants stay); W/E values share one scale; `compass()` is a real rose.
9. Section 6: the WeatherStation program is replaced by the one in section 6 here (no compact face over the
   gauge, no text over the compass); its `gauge(h, 0, 100, STEAMI_BLUE)` is now visible on V1a/V1b, its graph
   labels read `1050` and `950`, and its compass page shows north.
10. Section 7 suites 4 and 5: replaced by section 7 here; PPM dumps are reviewed against the tutorials'
    `_ref.png` with an SSIM, not against MicroPython SimBackend screenshots.
11. Section 8 items 3 and 4: split into 3a, 3b, 4a, 4b, 4c; the "done" of sub-project 4 becomes "every mockup
    has its Arduino example, rendered natively and reviewed against `_ref.png` on V1c (and on V1a/V1b when a
    board is available)"; the wiki page leaves sub-project 4 (separate PR).
12. Section 9: the five decisions above; the former open question 2 is closed.

## 12. Follow-ups in other repositories (separate PRs)

Scope *(designer's call, B4)*: everything in this section is a separate issue and PR in its own repository,
**outside the deliveries 3a to 4c**. No delivery waits for them, and none of them is part of a delivery's
"done". The deliveries update only `lib/steami_board` in this repository (code, tests, examples, README,
`keywords.txt`).

`steamicc/micropython-steami-lib`, `lib/steami_screen`:
- `value()` centred and arc without artefacts (#351); gauge track and fill of equal thickness, outer radius
  `radius − 4`; bar track GRID_DARK; grey minimum contrast; NAN shows `--`.
- Real compass rose, linked to #434 (and #371).
- GC9A01 wrapper with the 128 → 240 rule (×15/8, k = 2, a real `draw_scaled_text`): new issue completing #368,
  before the first courses on V1c.
- `menu()` centred, wrapping, returning the index, 5 visible rows, bar clipped to the circle; graph axis
  labels min and max only, in full, inside the box (B1); W/E values at one shared scale (B2); N/S safe margin
  from the physical width (B3).

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
