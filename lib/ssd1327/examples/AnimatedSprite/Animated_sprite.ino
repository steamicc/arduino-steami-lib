// SPDX-License-Identifier: GPL-3.0-or-later
//
// SSD1327 — bouncing ball animation across the screen.

#include <Arduino.h>
#include <SPI.h>

#include "SSD1327.h"

#define OLED_CS PD0

SPIClass oledSPI(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
SSD1327_SPI display(128, 128, oledSPI, SPI_INT_MISO, RST_DISPLAY, OLED_CS);

constexpr uint8_t BALL_SIZE = 8;
constexpr uint8_t SCREEN_SIZE = 120;

int16_t x = 0;
int16_t y = 0;
int16_t dx = 2;
int16_t dy = 3;

void setup() {
    oledSPI.begin();
    display.begin();
    display.fill(0);
    display.show();
}

void loop() {
    x += dx;
    y += dy;

    if (x <= 0 || x >= SCREEN_SIZE - BALL_SIZE) {
        dx = -dx;
    }
    if (y <= 0 || y >= SCREEN_SIZE - BALL_SIZE) {
        dy = -dy;
    }

    display.fill(0);
    display.fillRect(x, y, BALL_SIZE, BALL_SIZE, 15);
    display.show();

    delay(20);
}