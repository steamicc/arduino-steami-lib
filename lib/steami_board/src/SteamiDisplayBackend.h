// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

/// The only virtual interface of the display layer: one panel, drawn with RGB565 colours.
/// Coordinates reaching pixel() and fillRect() are already clipped by the layer.
class SteamiDisplayBackend {
   protected:
    ~SteamiDisplayBackend() = default;  // non-virtual: no deleting destructor needed

   public:
    virtual bool begin() = 0;              // false = NO_MEMORY
    virtual const char* chip() const = 0;  // "SSD1327", "GC9A01", "memory"
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual bool isColor() const = 0;
    virtual void fill(uint16_t rgb565) = 0;
    virtual void show() = 0;
    virtual void pixel(int16_t x, int16_t y, uint16_t rgb565) = 0;  // already clipped
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t rgb565) = 0;  // already clipped
    virtual void rotate(bool upsideDown) = 0;
    virtual void invert(bool on) = 0;
    virtual void powerOn() = 0;
    virtual void powerOff() = 0;
};
