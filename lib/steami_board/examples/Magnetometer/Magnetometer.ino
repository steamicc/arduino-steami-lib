// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the magnetic field measured by the LIS2MDL five times
// per second (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Values are in microtesla, with
// the hard-iron offset removed. The Earth field is about 25 to 65 uT, so
// strength() should be in that range away from magnets and metal.
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiMagnetometer.h>

SteamiMagnetometer magnetometer;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (magnetometer.begin()) {
        Serial.print("Magnetometer: ");
        Serial.println(magnetometer.chip());
    } else {
        Serial.print("No magnetometer: ");
        Serial.println(magnetometer.error());
    }
}

void loop() {
    const SteamiVector m = magnetometer.read();
    Serial.print("x=");
    Serial.print(m.x, 1);
    Serial.print(" y=");
    Serial.print(m.y, 1);
    Serial.print(" z=");
    Serial.print(m.z, 1);
    Serial.print(" uT, strength ");
    Serial.println(magnetometer.strength(), 1);
    delay(200);
}
