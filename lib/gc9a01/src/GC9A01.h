// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#ifdef ARDUINO
#include <SPI.h>
#else
#include "SPI.h"
#endif

#include "GC9A01_const.h"

// GC9A01 240x240 round colour TFT, RGB565 frame buffer.
//
// The drawing API mirrors the SSD1327 driver: draw into the frame buffer,
// then call show() to send it to the screen. Colours are 16-bit RGB565
// values (see color565() and the GC9A01_* colour constants).
class GC9A01 {
   public:
    GC9A01() = default;
    virtual ~GC9A01();

    GC9A01(const GC9A01&) = delete;
    GC9A01& operator=(const GC9A01&) = delete;

    // Allocate the frame buffer (115 200 bytes), reset and initialise the
    // controller, clear the screen. Returns false if the allocation fails.
    virtual bool begin();
    void show();
    void fill(uint16_t color);
    void pixel(int16_t x, int16_t y, uint16_t color);
    uint16_t pixel(int16_t x, int16_t y) const;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void scroll(int16_t dx, int16_t dy);
    void text(const char* str, int16_t x, int16_t y, uint16_t color);
    void rotate(bool rotate);
    void invert(bool invert);
    void powerOff();
    void powerOn();

    uint16_t width() const { return GC9A01_WIDTH; }
    uint16_t height() const { return GC9A01_HEIGHT; }

    static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    static constexpr size_t BUFFER_SIZE = static_cast<size_t>(GC9A01_WIDTH) * GC9A01_HEIGHT * 2;

   protected:
    virtual void reset() {}
    virtual void writeCmd(uint8_t cmd) = 0;
    virtual void writeData(const uint8_t* data, size_t len) = 0;

    void command(uint8_t cmd, const uint8_t* params = nullptr, size_t len = 0);

   private:
    // Pixels in wire order: high byte first, so show() streams the buffer
    // as is.
    uint8_t* _buffer = nullptr;
    bool _rotated = false;
    bool _inverted = false;

    void initDisplay();
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
};

class GC9A01_SPI : public GC9A01 {
   public:
    // rst may be GC9A01_PIN_NONE: a software reset is used instead.
    GC9A01_SPI(SPIClass& spi, uint8_t dc, uint8_t rst, uint8_t cs);

   protected:
    void reset() override;
    void writeCmd(uint8_t cmd) override;
    void writeData(const uint8_t* data, size_t len) override;

   private:
    SPIClass* _spi;
    uint8_t _dc;
    uint8_t _rst;
    uint8_t _cs;
    uint32_t _rate;

    void beginWrite(bool data);
    void endWrite();
};

#ifdef ARDUINO
// Built-in display of the STeaMi V1c: internal SPI bus, D/C on the internal
// MISO pin, CS_DISPLAY and RST_DISPLAY. The backlight is always on.
class GC9A01_STEAMI : public GC9A01_SPI {
   public:
    GC9A01_STEAMI();
    bool begin() override;

   private:
    static SPIClass& internalSpi();
};
#endif
