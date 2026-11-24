// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SteamiSSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

SteamiSSD1327 display;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("SSD1327 not found");
        while (true) {
        }
    }

    display.fill(0);
    for (uint8_t shade = 0; shade <= 15; shade++) {
        display.fillRect((shade) * 8, 0, 8, 128, shade);
    }
    display.show();
}

void loop() {}
