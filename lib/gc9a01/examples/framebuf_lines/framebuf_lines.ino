// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <GC9A01.h>

GC9A01_STEAMI display;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("GC9A01 begin failed: not enough RAM for the frame buffer");
        while (true) {
            delay(1000);
        }
    }

    display.fill(GC9A01_BLACK);
    for (uint8_t i = 0; i < 240; i += 20) {
        // colour shifts from red to blue across the pattern
        uint16_t color = GC9A01::color565(255 - i, 64, i);
        display.line(0, i, 239, 239 - i, color);
        display.line(i, 0, 239 - i, 239, GC9A01_CYAN);
    }
    display.show();

    Serial.println("GC9A01 lines example : displaying stars pattern");
}

void loop() {}
