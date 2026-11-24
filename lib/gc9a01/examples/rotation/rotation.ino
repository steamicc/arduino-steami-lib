// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <GC9A01.h>

GC9A01_STEAMI display;

bool rotated = false;

void drawScreen() {
    display.fill(GC9A01_BLACK);
    display.text(rotated ? "Rotated" : "Normal", rotated ? 99 : 102, 40, GC9A01_WHITE);
    // arrow pointing to the top of the screen
    display.line(120, 60, 100, 90, GC9A01_YELLOW);
    display.line(120, 60, 140, 90, GC9A01_YELLOW);
    display.fillRect(116, 60, 9, 70, GC9A01_YELLOW);
    // red square on the left, blue square on the right
    display.fillRect(50, 140, 40, 40, GC9A01_RED);
    display.fillRect(150, 140, 40, 40, GC9A01_BLUE);
    display.show();
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

    drawScreen();
}

void loop() {
    delay(2000);
    rotated = !rotated;
    // rotate() resends the frame: no need to call show()
    display.rotate(rotated);
    drawScreen();
    Serial.println(rotated ? "Rotated 180 degrees" : "Normal orientation");
}
