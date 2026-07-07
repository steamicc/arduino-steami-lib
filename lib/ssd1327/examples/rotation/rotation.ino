// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

WS_OLED_128X128_STEAMI display;

bool rotated = false;

void drawScreen() {
    display.fill(0);
    display.text(rotated ? "Rotated" : "Normal", 34, 16, 15);
    display.fillRect(10, 36, 30, 30, 15);
    display.fillRect(88, 88, 30, 30, 8);
    display.line(0, 0, 127, 127, 12);
    display.show();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("SSD1327 not found");
        while (true) {
        }
    }

    drawScreen();
}

void loop() {
    rotated = !rotated;
    display.rotate(rotated);
    drawScreen();
    delay(2000);
}
