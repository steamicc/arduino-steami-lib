// SPDX-License-Identifier: GPL-3.0-or-later
//
// SSD1327 — display a static text label centered on screen to verify
// font rendering and SPI communication.

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
    display.text("STeaMi !", 40, 55, 15);
    display.show();
}

void loop() {}