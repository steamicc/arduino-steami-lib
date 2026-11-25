// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the distance measured by the VL53L1X once per second
// (115200 baud).
//
// Works on every release (V1a, V1b and V1c). read() gives millimetres, or -1
// when the sensor gives no result (nothing in range, or a true 0 mm).
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiDistance.h>

SteamiDistance distance;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (distance.begin()) {
        Serial.print("Distance sensor: ");
        Serial.println(distance.chip());
    } else {
        Serial.print("No distance sensor: ");
        Serial.println(distance.error());
    }
}

void loop() {
    const int mm = distance.read();
    if (mm >= 0) {
        Serial.print(mm);
        Serial.print(" mm (");
        Serial.print(distance.cm(), 1);
        Serial.println(" cm)");
    } else {
        Serial.println("no result");
    }
    delay(1000);
}
