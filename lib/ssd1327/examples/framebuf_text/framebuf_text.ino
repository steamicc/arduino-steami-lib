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
    display.text("SSD1327", 34, 18, 15);
    display.text("Arduino", 34, 42, 12);
    display.text("STeaMi", 38, 66, 8);
    display.text("OLED 128x128", 18, 90, 5);
    display.show();

    Serial.println("SSD1327 text example : displaying text with different shades of gray");
}

void loop() {}
