// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <GC9A01.h>
#include <string.h>

GC9A01_STEAMI display;

const char* const lines[] = {"STeaMi", "GC9A01", "240x240", "RGB565", "scroll()"};
const uint16_t colors[] = {GC9A01_WHITE, GC9A01_CYAN, GC9A01_YELLOW, GC9A01_GREEN, GC9A01_MAGENTA};
const uint8_t lineCount = sizeof(lines) / sizeof(lines[0]);
const int16_t lineHeight = 12;
const int16_t step = 3;

uint8_t nextLine = 0;
int16_t scrolled = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("GC9A01 begin failed: not enough RAM for the frame buffer");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("GC9A01 scroll example : text scrolling up with scroll()");
}

void loop() {
    // Move the whole frame buffer up, then write the next line in the
    // freed rows at the bottom of the visible circle.
    display.scroll(0, -step);
    scrolled += step;

    if (scrolled >= lineHeight) {
        scrolled = 0;
        const char* str = lines[nextLine];
        int16_t x = 120 - static_cast<int16_t>(strlen(str)) * 3;
        display.text(str, x, 200, colors[nextLine]);
        nextLine = (nextLine + 1) % lineCount;
    }

    display.show();
}
