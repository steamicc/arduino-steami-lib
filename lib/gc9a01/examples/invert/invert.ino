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

    display.fill(GC9A01_BLACK);
    display.fillRect(80, 80, 40, 40, GC9A01_RED);
    display.fillRect(120, 80, 40, 40, GC9A01_GREEN);
    display.fillRect(80, 120, 40, 40, GC9A01_BLUE);
    display.fillRect(120, 120, 40, 40, GC9A01_WHITE);
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
