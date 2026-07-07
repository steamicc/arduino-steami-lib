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
        Serial.println("Display not found");
        while (true) {
            delay(1000);
        }
    }

    display.fill(15);

    const int sq = 12;
    int seq = 100;

    for (int y = 0; y < 128; y += sq + 1) {
        int offset = round(((seq & 3) / 3.0) * sq);
        seq >>= 2;

        if (seq == 0) {
            seq = 100;
        }

        for (int x = 0; x < 128; x += sq * 2) {
            display.fillRect(x + offset, y, sq, sq, 0);
        }

        display.fillRect(0, y + sq, 128, 1, 6);
    }

    display.show();
}

void loop() {}
