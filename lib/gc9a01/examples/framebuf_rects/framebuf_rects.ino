// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SteamiGC9A01.h>

SteamiGC9A01 display;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("GC9A01 begin failed: not enough RAM for the frame buffer");
        while (true) {
            delay(1000);
        }
    }

    // The largest square fully visible on the round screen is 168x168,
    // from (36, 36) to (203, 203).
    display.fill(GC9A01_BLACK);
    display.fillRect(36, 36, 168, 168, GC9A01_BLUE);
    display.fillRect(54, 54, 132, 132, GC9A01_GREEN);
    display.fillRect(72, 72, 96, 96, GC9A01_YELLOW);
    display.fillRect(90, 90, 60, 60, GC9A01_ORANGE);
    display.fillRect(108, 108, 24, 24, GC9A01_RED);
    display.show();

    Serial.println("GC9A01 rects example : displaying nested coloured squares");
}

void loop() {}
