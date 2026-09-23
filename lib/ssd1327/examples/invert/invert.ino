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
    display.fillRect(54, 54, 10, 10, 15);
    display.fillRect(64, 64, 10, 10, 10);
    display.fillRect(54, 64, 10, 10, 5);
    display.fillRect(64, 54, 10, 10, 1);
    display.show();
}

void loop() {
    display.invert(false);
    Serial.println("Invert false");
    delay(1000);

    display.invert(true);
    Serial.println("Invert true");
    delay(1000);
}
