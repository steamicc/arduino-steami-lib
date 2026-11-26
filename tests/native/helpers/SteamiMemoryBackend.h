// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "SteamiDisplayBackend.h"
#include "SteamiRgb.h"

/// Test-only backend: an RGB565 array that counts what the layer asks of it.
/// A grey backend stores what the OLED shows, so a dump matches the panel.
class SteamiMemoryBackend final : public SteamiDisplayBackend {
   public:
    SteamiMemoryBackend(int16_t width, int16_t height, bool color = true, bool beginResult = true)
        : _w(width),
          _h(height),
          _color(color),
          _beginResult(beginResult),
          _px(static_cast<size_t>(width) * static_cast<size_t>(height), 0) {}

    bool begin() override {
        _beginCalls++;
        return _beginResult;
    }
    const char* chip() const override { return "memory"; }
    int16_t width() const override { return _w; }
    int16_t height() const override { return _h; }
    bool isColor() const override { return _color; }

    void fill(uint16_t v) override {
        _fillCalls++;
        const uint16_t s = store(v);
        for (uint16_t& p : _px) {
            p = s;
        }
    }
    void show() override { _showCalls++; }

    void pixel(int16_t x, int16_t y, uint16_t v) override {
        _pixelCalls++;
        if (x < 0 || y < 0 || x >= _w || y >= _h) {
            _outOfBounds++;
            return;
        }
        _area++;
        _px[index(x, y)] = store(v);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t v) override {
        _fillRectCalls++;
        if (w <= 0 || h <= 0 || x < 0 || y < 0 || static_cast<int32_t>(x) + w > _w ||
            static_cast<int32_t>(y) + h > _h) {
            _outOfBounds++;
            return;
        }
        _area += static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
        const uint16_t s = store(v);
        for (int16_t j = y; j < y + h; j++) {
            for (int16_t i = x; i < x + w; i++) {
                _px[index(i, j)] = s;
            }
        }
    }

    void rotate(bool upsideDown) override { _upsideDown = upsideDown; }
    void invert(bool on) override { _inverted = on; }
    void powerOn() override { _poweredOn = true; }
    void powerOff() override { _poweredOn = false; }

    uint16_t at(int16_t x, int16_t y) const {
        return (x < 0 || y < 0 || x >= _w || y >= _h) ? 0 : _px[index(x, y)];
    }
    bool lit(int16_t x, int16_t y) const { return at(x, y) != 0; }
    size_t litCount() const {
        size_t n = 0;
        for (uint16_t p : _px) {
            n += (p != 0) ? 1 : 0;
        }
        return n;
    }
    const std::vector<uint16_t>& pixels() const { return _px; }
    uint32_t outOfBounds() const { return _outOfBounds; }
    uint32_t pixelCalls() const { return _pixelCalls; }
    uint32_t fillRectCalls() const { return _fillRectCalls; }
    /// Pixel calls plus w*h of each accepted fillRect.
    uint64_t areaDrawn() const { return _area; }
    uint32_t fillCalls() const { return _fillCalls; }
    uint32_t beginCalls() const { return _beginCalls; }
    uint32_t showCalls() const { return _showCalls; }
    bool upsideDown() const { return _upsideDown; }
    bool inverted() const { return _inverted; }
    bool poweredOn() const { return _poweredOn; }

    /// Zeroes the call counters; the pixels and the state flags stay.
    void clearCounters() {
        _outOfBounds = 0;
        _pixelCalls = 0;
        _fillRectCalls = 0;
        _area = 0;
        _fillCalls = 0;
        _beginCalls = 0;
        _showCalls = 0;
    }

    /// Binary P6, each pixel expanded from RGB565 to 8 bits per channel.
    bool writePpm(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (f == nullptr) {
            return false;
        }
        bool ok = fprintf(f, "P6\n%d %d\n255\n", _w, _h) > 0;
        for (uint16_t p : _px) {
            const uint32_t r5 = p >> 11;
            const uint32_t g6 = (p >> 5) & 63;
            const uint32_t b5 = p & 31;
            const unsigned char rgb[3] = {static_cast<unsigned char>((r5 << 3) | (r5 >> 2)),
                                          static_cast<unsigned char>((g6 << 2) | (g6 >> 4)),
                                          static_cast<unsigned char>((b5 << 3) | (b5 >> 2))};
            ok = ok && fwrite(rgb, 1, 3, f) == 3;
        }
        return (fclose(f) == 0) && ok;
    }

   private:
    size_t index(int16_t x, int16_t y) const {
        return static_cast<size_t>(y) * static_cast<size_t>(_w) + static_cast<size_t>(x);
    }
    uint16_t store(uint16_t v) const {
        return _color ? v : SteamiRgb::gray(SteamiRgb::fromRgb565(v).gray4()).rgb565();
    }

    int16_t _w;
    int16_t _h;
    bool _color;
    bool _beginResult;
    std::vector<uint16_t> _px;
    uint32_t _outOfBounds = 0;
    uint32_t _pixelCalls = 0;
    uint32_t _fillRectCalls = 0;
    uint64_t _area = 0;
    uint32_t _fillCalls = 0;
    uint32_t _beginCalls = 0;
    uint32_t _showCalls = 0;
    bool _upsideDown = false;
    bool _inverted = false;
    bool _poweredOn = false;
};
