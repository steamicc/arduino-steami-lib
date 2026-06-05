// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>
#ifdef ARDUINO
#include <SPI.h>
#else
#include "SPI.h"
#endif

#include "SSD1327_const.h"

class SSD1327 {
   public:
    SSD1327(uint8_t width = 128, uint8_t height = 128);
    ~SSD1327();

    bool begin();
    void show();

   private:
    uint8_t _width;
    uint8_t _height;
    uint8_t* _buffer;
    size_t _buffSize;

    uint8_t _colAddr[2];
    uint8_t _rowAddr[2];
    uint8_t _offset;

    void powerOff();
    void powerOn();
    void contrast(uint8_t contrast);
    void rotate(uint8_t rotate);
    void invert(uint8_t invert);
    void initDisplay();
    void fill(uint8_t color);
    void pixel(uint8_t x, uint8_t y, uint8_t color);
    void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
    void line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
    void scroll(int16_t dx, int16_t dy);
    void text(const char* str, uint8_t x, uint8_t y, uint8_t color);

   protected:
    virtual void writeCmd(uint8_t cmd) = 0;
    virtual void writeData(uint8_t* data, size_t len) = 0;
};

class SSD1327_I2C : public SSD1327 {
   public:
    SSD1327_I2C(uint8_t width, uint8_t height, TwoWire& wire = Wire, uint8_t address = 0x3C);

   private:
    TwoWire* _wire;
    uint8_t _address;
    uint8_t _cmdArr[2];
    uint8_t _dataList[2];

    void writeCmd(uint8_t cmd);
    void writeData(uint8_t* data, size_t len);
};

class SSD1327_SPI : public SSD1327 {
   public:
    SSD1327_SPI(uint8_t width, uint8_t height, SPIClass& spi, uint8_t dc, uint8_t res, uint8_t cs);

   private:
    SPIClass* _spi;
    uint8_t _dc;
    uint8_t _res;
    uint8_t _cs;
    uint32_t _rate;

    void reset();
    void writeCmd(uint8_t cmd);
    void writeData(uint8_t* data, size_t len);
};

class WS_OLED_128X128_SPI : public SSD1327_SPI {
   public:
    WS_OLED_128X128_SPI(SPIClass& spi, uint8_t dc, uint8_t res, uint8_t cs)
        : SSD1327_SPI(128, 128, spi, dc, res, cs) {}
};

class WS_OLED_128X128_I2C : public SSD1327_I2C {
   public:
    WS_OLED_128X128_I2C(TwoWire& wire = Wire, uint8_t address = 0x3C)
        : SSD1327_I2C(128, 128, wire, address) {}
};