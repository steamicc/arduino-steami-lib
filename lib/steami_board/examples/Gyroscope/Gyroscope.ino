// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the angular speed measured by the ISM330DL five times
// per second (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Values are in degrees per second;
// they stay close to 0 while the board lies still.
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiGyroscope.h>

SteamiGyroscope gyroscope;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (gyroscope.begin()) {
        Serial.print("Gyroscope: ");
        Serial.println(gyroscope.chip());
    } else {
        Serial.print("No gyroscope: ");
        Serial.println(gyroscope.error());
    }
}

void loop() {
    const SteamiVector g = gyroscope.read();
    Serial.print("x=");
    Serial.print(g.x, 1);
    Serial.print(" y=");
    Serial.print(g.y, 1);
    Serial.print(" z=");
    Serial.print(g.z, 1);
    Serial.println(" dps");
    delay(200);
}
