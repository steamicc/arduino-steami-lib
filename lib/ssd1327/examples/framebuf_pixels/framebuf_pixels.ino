// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

WS_OLED_128X128_STEAMI display;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("SSD1327 not found");
        while (true) {
        }
    }

    display.fill(0);
    display.pixel(0, 0, 15);
    display.pixel(127, 0, 15);
    display.pixel(0, 127, 15);
    display.pixel(127, 127, 15);
    display.pixel(64, 64, 15);
    display.pixel(32, 32, 8);
    display.pixel(96, 32, 8);
    display.pixel(32, 96, 8);
    display.pixel(96, 96, 8);
    display.show();

    Serial.println("SSD1327 pixels example : displaying corners and center pixels");
}

void loop() {}
