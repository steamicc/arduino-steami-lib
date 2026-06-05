// SPDX-License-Identifier: GPL-3.0-or-later

#include "SSD1327.h"

#include <math.h>

#include <cstring>
#ifdef ARDUINO
#include <SPI.h>
#endif

static const uint8_t font5x7[][5] = {
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

SSD1327::SSD1327(uint8_t width, uint8_t height) : _width(width), _height(height) {
    _buffSize = (static_cast<size_t>(width) / 2) * height;
    _buffer = new uint8_t[_buffSize];
    _colAddr[0] = (128 - _width) / 4;
    _colAddr[1] = 63 - (128 - _width) / 4;
    _rowAddr[0] = 0;
    _rowAddr[1] = _height - 1;
    _offset = 128 - _height;
}

SSD1327::~SSD1327() {
    delete[] _buffer;
}

bool SSD1327::begin() {
    initDisplay();
    powerOn();
    return true;
}

void SSD1327::initDisplay() {
    writeCmd(SET_COMMAND_LOCK);
    writeCmd(0x12);
    writeCmd(SET_DISP);
    writeCmd(SET_DISP_START_LINE);
    writeCmd(0x00);
    writeCmd(SET_DISP_OFFSET);
    writeCmd(_offset);
    writeCmd(SET_SEG_REMAP);
    writeCmd(0x51);
    writeCmd(SET_MUX_RATIO);
    writeCmd(_height - 1);
    writeCmd(SET_FN_SELECT_A);
    writeCmd(0x01);
    writeCmd(SET_PHASE_LEN);
    writeCmd(0x51);
    writeCmd(SET_DISP_CLK_DIV);
    writeCmd(0x01);
    writeCmd(SET_PRECHARGE);
    writeCmd(0x08);
    writeCmd(SET_VCOM_DESEL);
    writeCmd(0x07);
    writeCmd(SET_SECOND_PRECHARGE);
    writeCmd(0x01);
    writeCmd(SET_FN_SELECT_B);
    writeCmd(0x62);
    writeCmd(SET_GRAYSCALE_LINEAR);
    writeCmd(SET_CONTRAST);
    writeCmd(0x7F);
    writeCmd(SET_DISP_MODE);
    writeCmd(SET_COL_ADDR);
    writeCmd(_colAddr[0]);
    writeCmd(_colAddr[1]);
    writeCmd(SET_ROW_ADDR);
    writeCmd(_rowAddr[0]);
    writeCmd(_rowAddr[1]);
    writeCmd(SET_SCROLL_DEACTIVATE);
    writeCmd(SET_DISP | 0x01);

    fill(0);
    writeData(_buffer, _buffSize);
}

void SSD1327::powerOff() {
    writeCmd(SET_FN_SELECT_A);
    writeCmd(0x00);
    writeCmd(SET_DISP);
}

void SSD1327::powerOn() {
    writeCmd(SET_FN_SELECT_A);
    writeCmd(0x01);
    writeCmd(SET_DISP | 0x01);
}

void SSD1327::contrast(uint8_t contrast) {
    writeCmd(SET_CONTRAST);
    writeCmd(contrast);
}

void SSD1327::rotate(uint8_t rotate) {
    powerOff();
    writeCmd(SET_DISP_OFFSET);
    writeCmd(rotate ? _height : _offset);
    writeCmd(SET_SEG_REMAP);
    writeCmd(rotate ? 0x42 : 0x51);
    powerOn();
}

void SSD1327::invert(uint8_t invert) {
    writeCmd(SET_DISP_MODE | (invert & 1) << 1 | (invert & 1));
}

void SSD1327::show() {
    writeCmd(SET_COL_ADDR);
    writeCmd(_colAddr[0]);
    writeCmd(_colAddr[1]);
    writeCmd(SET_ROW_ADDR);
    writeCmd(_rowAddr[0]);
    writeCmd(_rowAddr[1]);
    writeData(_buffer, _buffSize);
}

void SSD1327::fill(uint8_t color) {
    uint8_t byte = ((color & 0x0F) << 4) | (color & 0x0F);
    memset(_buffer, byte, _buffSize);
}

void SSD1327::pixel(uint8_t x, uint8_t y, uint8_t color) {
    size_t index = (x / 2) + (y * (_width / 2));
    if (x % 2 == 0) {
        _buffer[index] = (_buffer[index] & 0x0F) | ((color & 0x0F) << 4);
    } else {
        _buffer[index] = (_buffer[index] & 0xF0) | (color & 0x0F);
    }
}

void SSD1327::fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    uint16_t xEnd = static_cast<uint16_t>(x) + static_cast<uint16_t>(w);
    uint16_t yEnd = static_cast<uint16_t>(y) + static_cast<uint16_t>(h);
    if (xEnd > _width)
        xEnd = _width;
    if (yEnd > _height)
        yEnd = _height;
    for (uint16_t row = y; row < yEnd; row++) {
        for (uint16_t col = x; col < xEnd; col++) {
            pixel(static_cast<uint8_t>(col), static_cast<uint8_t>(row), color);
        }
    }
}

void SSD1327::line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (true) {
        pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void SSD1327::scroll(int16_t dx, int16_t dy) {
    size_t stride = static_cast<size_t>(_width) / 2;
    if (dy > 0) {
        size_t offset = static_cast<size_t>(dy) * stride;
        memmove(_buffer + offset, _buffer, _buffSize - offset);
        memset(_buffer, 0, offset);
    } else if (dy < 0) {
        size_t offset = static_cast<size_t>(-dy) * stride;
        memmove(_buffer, _buffer + offset, _buffSize - offset);
        memset(_buffer + _buffSize - offset, 0, offset);
    }
    (void)dx;
}

void SSD1327::text(const char* str, uint8_t x, uint8_t y, uint8_t color) {
    while (*str) {
        char c = *str++;

        if (c < 32 || c > 127) {
            x += 6;
            continue;
        }

        const uint8_t* glyph = font5x7[c - 32];

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t line = glyph[col];

            for (uint8_t row = 0; row < 7; row++) {
                if (line & (1 << row)) {
                    pixel(x + col, y + row, color & 0x0F);
                }
            }
        }

        x += 6;
    }
}

SSD1327_I2C::SSD1327_I2C(uint8_t width, uint8_t height, TwoWire& wire, uint8_t address)
    : SSD1327(width, height),
      _wire(&wire),
      _address(address),
      _cmdArr{REG_CMD, 0},
      _dataList{REG_DATA, 0} {}

void SSD1327_I2C::writeCmd(uint8_t cmd) {
    _cmdArr[1] = cmd;
    _wire->beginTransmission(_address);
    _wire->write(_cmdArr, 2);
    _wire->endTransmission();
}

void SSD1327_I2C::writeData(uint8_t* data, size_t len) {
    const size_t chunkSize = 31;
    size_t offset = 0;

    while (offset < len) {
        size_t toSend = len - offset;
        if (toSend > chunkSize)
            toSend = chunkSize;

        _wire->beginTransmission(_address);
        _wire->write(REG_DATA);
        _wire->write(data + offset, toSend);
        _wire->endTransmission();

        offset += toSend;
    }
}

SSD1327_SPI::SSD1327_SPI(uint8_t width, uint8_t height, SPIClass& spi, uint8_t dc, uint8_t res,
                         uint8_t cs)
    : SSD1327(width, height), _spi(&spi), _dc(dc), _res(res), _cs(cs) {
    _rate = 10000000;
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, LOW);
    if (_res != 255) {
        pinMode(_res, OUTPUT);
        digitalWrite(_res, HIGH);
        reset();
    }
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
}

void SSD1327_SPI::reset() {
    if (_res == 255)
        return;
    digitalWrite(_res, LOW);
    delayMicroseconds(500);
    digitalWrite(_res, HIGH);
}
void SSD1327_SPI::writeCmd(uint8_t cmd) {
    _spi->beginTransaction(SPISettings(_rate, MSBFIRST, SPI_MODE0));
    pinMode(_dc, OUTPUT);
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, LOW);
    digitalWrite(_cs, LOW);
    _spi->transfer(cmd);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}

void SSD1327_SPI::writeData(uint8_t* data, size_t len) {
    _spi->beginTransaction(SPISettings(_rate, MSBFIRST, SPI_MODE0));
    pinMode(_dc, OUTPUT);
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    _spi->transfer(data, len);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}