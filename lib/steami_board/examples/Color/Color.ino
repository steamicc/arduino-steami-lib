// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the colour measured by the APDS-9960 twice per second
// (115200 baud).
//
// V1a and V1b only. A V1c has no colour sensor: begin() returns false and the
// library logs one message ending in (E20).
//
// read() gives the raw red, green and blue counts of the sensor, and returns
// false when it is unavailable. The reading freezes while a hand is tracked by
// SteamiGesture.

#include <Arduino.h>
#include <SteamiColor.h>

SteamiColor color;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (color.begin()) {
        Serial.print("Colour sensor: ");
        Serial.println(color.chip());
    } else {
        Serial.print("No colour sensor: ");
        Serial.println(color.error());
    }
}

void loop() {
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    if (color.read(r, g, b)) {
        Serial.print("r=");
        Serial.print(r);
        Serial.print(" g=");
        Serial.print(g);
        Serial.print(" b=");
        Serial.println(b);
    } else {
        Serial.println("no result");
    }
    delay(500);
}
