// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

WS_OLED_128X128_STEAMI display;

int16_t x = 128;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("SSD1327 not found");
        while (true) {
        }
    }
}

void loop() {
    display.fill(0);
    display.text("Scrolling text", x, 56, 15);
    display.show();

    x -= 2;
    if (x < -110) {
        x = 128;
    }

    delay(40);
}
