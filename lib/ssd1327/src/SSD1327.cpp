// SPDX-License-Identifier: GPL-3.0-or-later

#include "SSD1327.h"

#include <math.h>

#include <cstring>
#ifdef ARDUINO
#include <SPI.h>
#endif

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
    powerOn();
    initDisplay();
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
    for (uint8_t row = y; row < y + h; row++) {
        for (uint8_t col = x; col < x + w; col++) {
            pixel(col, row, color);
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
    (void)str;
    (void)x;
    (void)y;
    (void)color;
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
    _wire->beginTransmission(_address);
    _wire->write(REG_DATA);
    _wire->write(data, len);
    _wire->endTransmission();
}

SSD1327_SPI::SSD1327_SPI(uint8_t width, uint8_t height, SPIClass& spi, uint8_t dc, uint8_t res,
                         uint8_t cs)
    : SSD1327(width, height), _spi(&spi), _dc(dc), _res(res), _cs(cs) {
    _rate = 10000000;
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, LOW);
    pinMode(_res, OUTPUT);
    digitalWrite(_res, HIGH);
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    reset();
}

void SSD1327_SPI::reset() {
    digitalWrite(_res, LOW);
    delayMicroseconds(500);
    digitalWrite(_res, HIGH);
}
void SSD1327_SPI::writeCmd(uint8_t cmd) {
    _spi->beginTransaction(SPISettings(_rate, MSBFIRST, SPI_MODE0));
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, LOW);
    digitalWrite(_cs, LOW);
    _spi->transfer(cmd);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}

void SSD1327_SPI::writeData(uint8_t* data, size_t len) {
    _spi->beginTransaction(SPISettings(_rate, MSBFIRST, SPI_MODE0));
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    _spi->transfer(data, len);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}