// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SteamiGC9A01.h>

SteamiGC9A01 display;

const uint16_t swatches[] = {
    GC9A01_RED,    GC9A01_ORANGE,  GC9A01_YELLOW, GC9A01_GREEN, GC9A01_CYAN, GC9A01_BLUE,
    GC9A01_PURPLE, GC9A01_MAGENTA, GC9A01_PINK,   GC9A01_WHITE, GC9A01_GREY, GC9A01_DARKGREY,
};

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

    // named colours: 3 rows of 4 swatches
    for (uint8_t i = 0; i < 12; i++) {
        int16_t x = 56 + (i % 4) * 32;
        int16_t y = 40 + (i / 4) * 32;
        display.fillRect(x, y, 28, 28, swatches[i]);
    }

    // red, green, blue and grey gradients built with color565()
    for (int16_t x = 0; x < 160; x++) {
        uint8_t level = static_cast<uint8_t>(x * 255 / 159);
        display.fillRect(40 + x, 140, 1, 12, GC9A01::color565(level, 0, 0));
        display.fillRect(40 + x, 154, 1, 12, GC9A01::color565(0, level, 0));
        display.fillRect(40 + x, 168, 1, 12, GC9A01::color565(0, 0, level));
        display.fillRect(40 + x, 182, 1, 12, GC9A01::color565(level, level, level));
    }

    display.show();

    Serial.println("GC9A01 colour palette example : named colours and RGB gradients");
}

void loop() {}
