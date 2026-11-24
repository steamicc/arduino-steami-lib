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

    // The screen is round: points stay inside the 240-pixel circle.
    display.fill(GC9A01_BLACK);
    display.pixel(120, 120, GC9A01_WHITE);
    display.pixel(60, 60, GC9A01_RED);
    display.pixel(180, 60, GC9A01_GREEN);
    display.pixel(60, 180, GC9A01_BLUE);
    display.pixel(180, 180, GC9A01_YELLOW);
    display.show();

    Serial.println("GC9A01 pixels example : displaying center and four coloured pixels");
}

void loop() {}
