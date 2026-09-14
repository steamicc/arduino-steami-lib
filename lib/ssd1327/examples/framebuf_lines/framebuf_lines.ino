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
    for (uint8_t i = 0; i < 128; i += 16) {
        display.line(0, i, 127, 127 - i, 15);
        display.line(i, 0, 127 - i, 127, 8);
    }
    display.show();

    Serial.println("SSD1327 lines example : displaying stars pattern");
}

void loop() {}
