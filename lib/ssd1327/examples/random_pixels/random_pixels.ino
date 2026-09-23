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

    randomSeed(analogRead(A0));
}

void loop() {
    uint8_t x = random(0, 128);
    uint8_t y = random(0, 128);
    uint8_t c = random(1, 16);
    display.pixel(x, y, c);
    display.show();
    delay(100);
}
