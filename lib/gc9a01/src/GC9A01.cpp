// SPDX-License-Identifier: GPL-3.0-or-later

#include "GC9A01.h"

#include <stdlib.h>
#include <string.h>

// 5x7 ASCII font (characters 32 to 127), one byte per column, bit 0 at the
// top. Same table as the SSD1327 driver, copied so the drivers stay
// independent.
static const uint8_t kFont5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40}, {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00}, {0x10, 0x08, 0x08, 0x10, 0x08}, {0x00, 0x00, 0x00, 0x00, 0x00},
};

// Initialisation sequence: command, parameter count, parameters.
// Register values from the GC9A01 datasheet / manufacturer reference
// sequence (also used by the open-source Arduino GC9A01 drivers). The
// "inter register enable" commands unlock the extended registers.
// clang-format off
static const uint8_t kInitSequence[] = {
    GC9A01_CMD_INREGEN2, 0,
    0xEB, 1, 0x14,
    GC9A01_CMD_INREGEN1, 0,
    GC9A01_CMD_INREGEN2, 0,
    0xEB, 1, 0x14,
    0x84, 1, 0x40,
    0x85, 1, 0xFF,
    0x86, 1, 0xFF,
    0x87, 1, 0xFF,
    0x88, 1, 0x0A,
    0x89, 1, 0x21,
    0x8A, 1, 0x00,
    0x8B, 1, 0x80,
    0x8C, 1, 0x01,
    0x8D, 1, 0x01,
    0x8E, 1, 0xFF,
    0x8F, 1, 0xFF,
    // gate scan direction (GS): with MADCTL = BGR it gives the expected
    // orientation on the STeaMi V1c
    GC9A01_CMD_DISP_FUNC_CTRL, 2, 0x00, 0x20,
    GC9A01_CMD_COLMOD, 1, GC9A01_COLMOD_RGB565,
    0x90, 4, 0x08, 0x08, 0x08, 0x08,
    0xBD, 1, 0x06,
    0xBC, 1, 0x00,
    0xFF, 3, 0x60, 0x01, 0x04,
    GC9A01_CMD_POWER2, 1, 0x13,
    GC9A01_CMD_POWER3, 1, 0x13,
    GC9A01_CMD_POWER4, 1, 0x22,
    0xBE, 1, 0x11,
    0xE1, 2, 0x10, 0x0E,
    0xDF, 3, 0x21, 0x0C, 0x02,
    GC9A01_CMD_GAMMA1, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    GC9A01_CMD_GAMMA2, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    GC9A01_CMD_GAMMA3, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    GC9A01_CMD_GAMMA4, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    0xED, 2, 0x1B, 0x0B,
    0xAE, 1, 0x77,
    0xCD, 1, 0x63,
    0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
    GC9A01_CMD_FRAME_RATE, 1, 0x34,
    0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
    0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
    0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
    0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
    0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
    0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
    0x98, 2, 0x3E, 0x07,
};
// clang-format on

static const int16_t kWidth = static_cast<int16_t>(GC9A01_WIDTH);
static const int16_t kHeight = static_cast<int16_t>(GC9A01_HEIGHT);
static const size_t kStride = static_cast<size_t>(GC9A01_WIDTH) * 2;

GC9A01::~GC9A01() {
    free(_buffer);
}

bool GC9A01::begin() {
    if (_buffer == nullptr) {
        _buffer = static_cast<uint8_t*>(malloc(BUFFER_SIZE));
        if (_buffer == nullptr) {
            return false;
        }
    }
    reset();
    initDisplay();
    fill(GC9A01_BLACK);
    show();
    return true;
}

void GC9A01::command(uint8_t cmd, const uint8_t* params, size_t len) {
    writeCmd(cmd);
    if (params != nullptr && len > 0) {
        writeData(params, len);
    }
}

void GC9A01::initDisplay() {
    size_t i = 0;
    while (i + 1 < sizeof(kInitSequence)) {
        const uint8_t cmd = kInitSequence[i];
        const uint8_t len = kInitSequence[i + 1];
        command(cmd, &kInitSequence[i + 2], len);
        i += 2 + len;
    }

    const uint8_t madctl = _rotated ? GC9A01_ORIENT_ROTATED : GC9A01_ORIENT_NORMAL;
    command(GC9A01_CMD_MADCTL, &madctl, 1);
    // The V1c panel is IPS: colours are right with the controller inversion
    // on, so invert(false) sends INVON.
    writeCmd(_inverted ? GC9A01_CMD_INVOFF : GC9A01_CMD_INVON);

    writeCmd(GC9A01_CMD_SLPOUT);
    delay(GC9A01_SLEEP_DELAY_MS);
    writeCmd(GC9A01_CMD_DISPON);
    delay(20);
}

void GC9A01::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t cols[4] = {static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0xFF),
                             static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0xFF)};
    const uint8_t rows[4] = {static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0xFF),
                             static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0xFF)};
    command(GC9A01_CMD_CASET, cols, sizeof(cols));
    command(GC9A01_CMD_RASET, rows, sizeof(rows));
    writeCmd(GC9A01_CMD_RAMWR);
}

void GC9A01::show() {
    if (_buffer == nullptr) {
        return;
    }
    setWindow(0, 0, GC9A01_WIDTH - 1, GC9A01_HEIGHT - 1);
    writeData(_buffer, BUFFER_SIZE);
}

void GC9A01::powerOff() {
    writeCmd(GC9A01_CMD_DISPOFF);
    writeCmd(GC9A01_CMD_SLPIN);
    // SLPOUT must not follow SLPIN within 120 ms
    delay(GC9A01_SLEEP_DELAY_MS);
}

void GC9A01::powerOn() {
    writeCmd(GC9A01_CMD_SLPOUT);
    delay(GC9A01_SLEEP_DELAY_MS);
    writeCmd(GC9A01_CMD_DISPON);
}

void GC9A01::rotate(bool rotate) {
    _rotated = rotate;
    const uint8_t madctl = rotate ? GC9A01_ORIENT_ROTATED : GC9A01_ORIENT_NORMAL;
    command(GC9A01_CMD_MADCTL, &madctl, 1);
    // MADCTL only changes how new data is written to the display RAM:
    // resend the frame so the picture turns right away.
    show();
}

void GC9A01::invert(bool invert) {
    _inverted = invert;
    writeCmd(invert ? GC9A01_CMD_INVOFF : GC9A01_CMD_INVON);
}

void GC9A01::fill(uint16_t color) {
    if (_buffer == nullptr) {
        return;
    }
    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
    if (hi == lo) {
        memset(_buffer, hi, BUFFER_SIZE);
        return;
    }
    for (size_t i = 0; i < BUFFER_SIZE; i += 2) {
        _buffer[i] = hi;
        _buffer[i + 1] = lo;
    }
}

void GC9A01::pixel(int16_t x, int16_t y, uint16_t color) {
    if (_buffer == nullptr || x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return;
    }
    uint8_t* p = _buffer + static_cast<size_t>(y) * kStride + static_cast<size_t>(x) * 2;
    p[0] = static_cast<uint8_t>(color >> 8);
    p[1] = static_cast<uint8_t>(color & 0xFF);
}

uint16_t GC9A01::pixel(int16_t x, int16_t y) const {
    if (_buffer == nullptr || x < 0 || y < 0 || x >= kWidth || y >= kHeight) {
        return 0;
    }
    const uint8_t* p = _buffer + static_cast<size_t>(y) * kStride + static_cast<size_t>(x) * 2;
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

void GC9A01::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (_buffer == nullptr || w <= 0 || h <= 0) {
        return;
    }
    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = static_cast<int32_t>(x) + w;
    int32_t y1 = static_cast<int32_t>(y) + h;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > kWidth)
        x1 = kWidth;
    if (y1 > kHeight)
        y1 = kHeight;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
    for (int32_t row = y0; row < y1; row++) {
        uint8_t* p = _buffer + static_cast<size_t>(row) * kStride + static_cast<size_t>(x0) * 2;
        for (int32_t col = x0; col < x1; col++) {
            *p++ = hi;
            *p++ = lo;
        }
    }
}

void GC9A01::line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    int32_t x = x1;
    int32_t y = y1;
    const int32_t dx = abs(static_cast<int32_t>(x2) - x1);
    const int32_t dy = abs(static_cast<int32_t>(y2) - y1);
    const int32_t sx = (x1 < x2) ? 1 : -1;
    const int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx - dy;

    while (true) {
        pixel(static_cast<int16_t>(x), static_cast<int16_t>(y), color);
        if (x == x2 && y == y2)
            break;
        const int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void GC9A01::scroll(int16_t dx, int16_t dy) {
    if (_buffer == nullptr || (dx == 0 && dy == 0)) {
        return;
    }
    if (dx <= -kWidth || dx >= kWidth || dy <= -kHeight || dy >= kHeight) {
        fill(GC9A01_BLACK);
        return;
    }

    // In place with memmove: a second 115 200-byte buffer would not fit in
    // the STeaMi RAM next to the frame buffer.
    if (dy > 0) {
        const size_t shift = static_cast<size_t>(dy) * kStride;
        memmove(_buffer + shift, _buffer, BUFFER_SIZE - shift);
        memset(_buffer, 0, shift);
    } else if (dy < 0) {
        const size_t shift = static_cast<size_t>(-dy) * kStride;
        memmove(_buffer, _buffer + shift, BUFFER_SIZE - shift);
        memset(_buffer + BUFFER_SIZE - shift, 0, shift);
    }

    if (dx == 0) {
        return;
    }
    const size_t shift = static_cast<size_t>(dx > 0 ? dx : -dx) * 2;
    for (int16_t row = 0; row < kHeight; row++) {
        uint8_t* p = _buffer + static_cast<size_t>(row) * kStride;
        if (dx > 0) {
            memmove(p + shift, p, kStride - shift);
            memset(p, 0, shift);
        } else {
            memmove(p, p + shift, kStride - shift);
            memset(p + kStride - shift, 0, shift);
        }
    }
}

void GC9A01::text(const char* str, int16_t x, int16_t y, uint16_t color) {
    if (str == nullptr) {
        return;
    }
    while (*str) {
        const uint8_t c = static_cast<uint8_t>(*str++);

        if (c < 32 || c > 127) {
            x += 6;
            continue;
        }

        const uint8_t* glyph = kFont5x7[c - 32];

        for (uint8_t col = 0; col < 5; col++) {
            const uint8_t bits = glyph[col];
            for (uint8_t row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    pixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
                }
            }
        }

        x += 6;
    }
}

GC9A01_SPI::GC9A01_SPI(SPIClass& spi, uint8_t dc, uint8_t rst, uint8_t cs)
    : _spi(&spi), _dc(dc), _rst(rst), _cs(cs), _rate(32000000) {}

void GC9A01_SPI::reset() {
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, HIGH);

    if (_rst == GC9A01_PIN_NONE) {
        writeCmd(GC9A01_CMD_SWRESET);
        delay(GC9A01_RESET_DELAY_MS);
        return;
    }
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(10);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(GC9A01_RESET_DELAY_MS);
}

// The SPI clock is the highest rate the core can derive from the bus clock
// without exceeding _rate: on the STeaMi (PCLK2 = 32 MHz) SPI1 runs at
// 16 MHz, its maximum.
void GC9A01_SPI::beginWrite(bool data) {
    _spi->beginTransaction(SPISettings(_rate, MSBFIRST, SPI_MODE0));
    // On the STeaMi D/C is the internal SPI MISO pin: beginTransaction()
    // hands it to the SPI peripheral, take it back as a GPIO.
    pinMode(_dc, OUTPUT);
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, data ? HIGH : LOW);
    digitalWrite(_cs, LOW);
}

void GC9A01_SPI::endWrite() {
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}

void GC9A01_SPI::writeCmd(uint8_t cmd) {
    beginWrite(false);
    _spi->transfer(cmd);
    endWrite();
}

void GC9A01_SPI::writeData(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }
    beginWrite(true);
#ifdef SPI_TRANSMITONLY
    // STM32 core: transmit-only block transfers straight from the caller's
    // buffer (nothing is written back). The core takes a 16-bit length.
    const size_t maxChunk = 32768;
    while (len > 0) {
        const size_t n = len < maxChunk ? len : maxChunk;
        _spi->transfer(const_cast<uint8_t*>(data), n, SPI_TRANSMITONLY);
        data += n;
        len -= n;
    }
#else
    // Generic cores: transfer(buf, n) overwrites buf with the received
    // bytes, so send a copy.
    uint8_t chunk[64];
    while (len > 0) {
        const size_t n = len < sizeof(chunk) ? len : sizeof(chunk);
        memcpy(chunk, data, n);
        _spi->transfer(chunk, n);
        data += n;
        len -= n;
    }
#endif
    endWrite();
}

#ifdef ARDUINO
SPIClass& GC9A01_STEAMI::internalSpi() {
    static SPIClass spi(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
    return spi;
}

GC9A01_STEAMI::GC9A01_STEAMI() : GC9A01_SPI(internalSpi(), SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY) {}

bool GC9A01_STEAMI::begin() {
    internalSpi().begin();
    return GC9A01_SPI::begin();
}
#endif
