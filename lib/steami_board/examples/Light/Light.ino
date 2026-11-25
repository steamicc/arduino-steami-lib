// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the light level measured by the APDS-9960 twice per
// second (115200 baud).
//
// V1a and V1b only. A V1c has no light sensor: begin() returns false and the
// library logs one message ending in (E20).
//
// read() is the raw clear count of the sensor, higher in brighter light (not a
// lux value). The reading freezes while a hand is tracked by SteamiGesture.

#include <Arduino.h>
#include <SteamiLight.h>

SteamiLight light;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (light.begin()) {
        Serial.print("Light sensor: ");
        Serial.println(light.chip());
    } else {
        Serial.print("No light sensor: ");
        Serial.println(light.error());
    }
}

void loop() {
    Serial.print("light ");
    Serial.println(light.read());
    delay(500);
}
