// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <SteamiGC9A01.h>
#include <stdint.h>

#include "SteamiDisplayBackend.h"

/// The 240x240 round colour TFT of the V1c. Passes RGB565 straight to the driver, which keeps the
/// 115 200 B frame buffer that begin() allocates and the destructor frees.
class SteamiGc9a01Backend final : public SteamiDisplayBackend {
   public:
    SteamiGc9a01Backend();  // builds the driver, touches no pin
    ~SteamiGc9a01Backend() = default;

    SteamiGc9a01Backend(const SteamiGc9a01Backend&) = delete;
    SteamiGc9a01Backend& operator=(const SteamiGc9a01Backend&) = delete;
    SteamiGc9a01Backend(SteamiGc9a01Backend&&) = delete;
    SteamiGc9a01Backend& operator=(SteamiGc9a01Backend&&) = delete;

    bool begin() override;  // false when the frame buffer cannot be allocated
    const char* chip() const override { return "GC9A01"; }
    int16_t width() const override { return static_cast<int16_t>(GC9A01_WIDTH); }
    int16_t height() const override { return static_cast<int16_t>(GC9A01_HEIGHT); }
    bool isColor() const override { return true; }
    void fill(uint16_t rgb565) override { _tft.fill(rgb565); }
    void show() override { _tft.show(); }
    void pixel(int16_t x, int16_t y, uint16_t rgb565) override { _tft.pixel(x, y, rgb565); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t rgb565) override {
        _tft.fillRect(x, y, w, h, rgb565);
    }
    void rotate(bool upsideDown) override { _tft.rotate(upsideDown); }
    void invert(bool on) override { _tft.invert(on); }
    void powerOn() override { _tft.powerOn(); }
    void powerOff() override { _tft.powerOff(); }

   private:
#ifdef ARDUINO
    using SteamiGc9a01Driver = SteamiGC9A01;
#else
    // SteamiGC9A01 exists only on target: natively the backend.cpp builds the same driver on the
    // internal SPI pins over the SPI mock.
    using SteamiGc9a01Driver = GC9A01_SPI;
#endif

    SteamiGc9a01Driver _tft;
};
