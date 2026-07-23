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
    display.fillRect(8, 8, 112, 112, 2);
    display.fillRect(20, 20, 88, 88, 5);
    display.fillRect(32, 32, 64, 64, 8);
    display.fillRect(44, 44, 40, 40, 12);
    display.fillRect(56, 56, 16, 16, 15);
    display.show();

    Serial.println("SSD1327 rects example : displaying nested rectangles");
}

void loop() {}
