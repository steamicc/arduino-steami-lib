// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <GC9A01.h>
#include <string.h>

GC9A01_STEAMI display;

// Characters are 5x7 pixels with a 6-pixel advance.
void centeredText(const char* str, int16_t y, uint16_t color) {
    int16_t x = 120 - static_cast<int16_t>(strlen(str)) * 3;
    display.text(str, x, y, color);
}

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
    centeredText("GC9A01", 70, GC9A01_WHITE);
    centeredText("Arduino", 90, GC9A01_CYAN);
    centeredText("STeaMi", 110, GC9A01_YELLOW);
    centeredText("TFT 240x240", 130, GC9A01_GREEN);
    centeredText("RGB565", 150, GC9A01_MAGENTA);
    display.show();

    Serial.println("GC9A01 text example : displaying text in different colours");
}

void loop() {}
