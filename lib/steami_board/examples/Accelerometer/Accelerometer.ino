// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the acceleration measured by the ISM330DL five times per
// second (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Values are in g, and begin() sets
// the shared accelerometer to +-8 g. Flat with the screen up, z reads about
// -1 g. strength() is the norm, and isShaken() is true above 2 g.
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiAccelerometer.h>

SteamiAccelerometer accelerometer;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (accelerometer.begin()) {
        Serial.print("Accelerometer: ");
        Serial.println(accelerometer.chip());
    } else {
        Serial.print("No accelerometer: ");
        Serial.println(accelerometer.error());
    }
}

void loop() {
    const SteamiVector a = accelerometer.read();
    Serial.print("x=");
    Serial.print(a.x, 2);
    Serial.print(" y=");
    Serial.print(a.y, 2);
    Serial.print(" z=");
    Serial.print(a.z, 2);
    Serial.print(" g, strength ");
    Serial.print(accelerometer.strength(), 2);
    Serial.println(accelerometer.isShaken() ? " SHAKEN" : "");
    delay(200);
}
