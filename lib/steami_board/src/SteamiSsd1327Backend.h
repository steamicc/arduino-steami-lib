// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <SteamiSSD1327.h>
#include <stdint.h>

#include "SteamiDisplayBackend.h"

/// The 128x128 16-grey OLED of the V1a and V1b. Converts every RGB565 colour to a grey level 0..15
/// and leaves the drawing to the layer; the driver keeps the 8 KB frame buffer that its constructor
/// allocates and its destructor frees.
class SteamiSsd1327Backend final : public SteamiDisplayBackend {
   public:
    SteamiSsd1327Backend();  // builds the driver and pulses its reset line, sends no SPI byte
    ~SteamiSsd1327Backend() = default;

    SteamiSsd1327Backend(const SteamiSsd1327Backend&) = delete;
    SteamiSsd1327Backend& operator=(const SteamiSsd1327Backend&) = delete;
    SteamiSsd1327Backend(SteamiSsd1327Backend&&) = delete;
    SteamiSsd1327Backend& operator=(SteamiSsd1327Backend&&) = delete;

    bool begin() override;
    const char* chip() const override { return "SSD1327"; }
    int16_t width() const override { return 128; }
    int16_t height() const override { return 128; }
    bool isColor() const override { return false; }
    void fill(uint16_t rgb565) override { _oled.fill(gray(rgb565)); }
    void show() override { _oled.show(); }
    void pixel(int16_t x, int16_t y, uint16_t rgb565) override {
        _oled.pixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), gray(rgb565));
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t rgb565) override {
        _oled.fillRect(static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(w),
                       static_cast<uint8_t>(h), gray(rgb565));
    }
    void rotate(bool upsideDown) override { _oled.rotate(upsideDown); }
    void invert(bool on) override { _oled.invert(on); }
    void powerOn() override { _oled.powerOn(); }
    void powerOff() override { _oled.powerOff(); }

   private:
#ifdef ARDUINO
    using SteamiSsd1327Driver = SteamiSSD1327;
#else
    // SteamiSSD1327 exists only on target: natively the backend.cpp builds the same driver on the
    // internal SPI pins over the SPI mock.
    using SteamiSsd1327Driver = SSD1327_SPI;
#endif

    uint8_t gray(uint16_t rgb565);

    SteamiSsd1327Driver _oled;
    uint16_t _lastColor = 0;  // black converts to grey 0
    uint8_t _lastGray = 0;
};
