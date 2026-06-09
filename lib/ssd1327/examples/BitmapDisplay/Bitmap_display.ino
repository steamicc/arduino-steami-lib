// SPDX-License-Identifier: GPL-3.0-or-later
//
// SSD1327 — display a smiley face using a pixel matrix.

#include <Arduino.h>
#include <SPI.h>

#include "SSD1327.h"

#define OLED_CS PD0

SPIClass oledSPI(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
SSD1327_SPI display(128, 128, oledSPI, SPI_INT_MISO, RST_DISPLAY, OLED_CS);

const uint8_t SMILEY_W = 16;
const uint8_t SMILEY_H = 16;

const uint8_t smiley[SMILEY_H][SMILEY_W] = {
    {0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0},
    {0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0},
    {0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {5, 5, 5, 15, 15, 5, 5, 5, 5, 5, 15, 15, 5, 5, 5, 5},
    {5, 5, 5, 15, 15, 5, 5, 5, 5, 5, 15, 15, 5, 5, 5, 5},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {5, 5, 15, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 15, 5, 5},
    {5, 5, 5, 15, 5, 5, 5, 5, 5, 5, 5, 5, 15, 5, 5, 5},
    {5, 5, 5, 5, 15, 15, 15, 15, 15, 15, 15, 15, 5, 5, 5, 5},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0},
    {0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0},
    {0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

void drawMatrix(const uint8_t matrix[][SMILEY_W], uint8_t x, uint8_t y) {
    for (uint8_t row = 0; row < SMILEY_H; row++) {
        for (uint8_t col = 0; col < SMILEY_W; col++) {
            for (uint8_t i = 0; i < 4; i++) {
                for (uint8_t j = 0; j < 4; j++) {
                    display.pixel(x + col * 4 + i, y + row * 4 + j, matrix[row][col]);
                }
            }
        }
    }
}

void setup() {
    oledSPI.begin();
    display.begin();
    display.fill(0);
    drawMatrix(smiley, 30, 20);
    display.show();
}

void loop() {}