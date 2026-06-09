// SPDX-License-Identifier: GPL-3.0-or-later
//
// SSD1327 — render 16 horizontal bands stepping through all grayscale
// levels to validate the full intensity range of the display.

#include <Arduino.h>
#include <SPI.h>

#include "SSD1327.h"

#define OLED_CS PD0

SPIClass oledSPI(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
SSD1327_SPI display(128, 128, oledSPI, SPI_INT_MISO, RST_DISPLAY, OLED_CS);

void setup() {
    oledSPI.begin();
    display.begin();
    display.fill(0);
    for (uint8_t i = 0; i < 16; i++) {
        display.line(0, i * 8, 127, i * 8, i);
    }
    display.show();
}

void loop() {}