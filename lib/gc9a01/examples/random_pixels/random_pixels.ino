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

    randomSeed(analogRead(A0));
}

void loop() {
    // a batch of random pixels per frame: show() sends the whole screen
    for (uint8_t i = 0; i < 50; i++) {
        int16_t x = random(0, 240);
        int16_t y = random(0, 240);
        uint16_t color = GC9A01::color565(random(0, 256), random(0, 256), random(0, 256));
        display.pixel(x, y, color);
    }
    display.show();
    delay(100);
}
