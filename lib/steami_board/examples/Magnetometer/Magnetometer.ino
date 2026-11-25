// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the magnetic field measured by the LIS2MDL twice per
// second (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Values are in microtesla. The
// hard-iron offset is removed only after a calibration (the cm key in the
// board config, or calibrate()); before that the values include the board's own
// field and can be much larger than the Earth field (about 25 to 65 uT).
//
// Press MENU to calibrate for 12 seconds: turn the board slowly in every
// direction meanwhile. The calibration lives in RAM and is lost on reset.
// Away from magnets and metal, strength() is then about 25 to 65 uT.
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiButtons.h>
#include <SteamiMagnetometer.h>

SteamiMagnetometer magnetometer;
SteamiButtons buttons;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (magnetometer.begin()) {
        Serial.print("Magnetometer: ");
        Serial.print(magnetometer.chip());
        Serial.println(". Press MENU to calibrate (12 s).");
    } else {
        Serial.print("No magnetometer: ");
        Serial.println(magnetometer.error());
    }
    buttons.begin();
}

void loop() {
    // Poll the buttons often: a short tap is missed if they are read only twice per second.
    static uint32_t lastPrint = 0;

    if (buttons.wasPressed(SteamiButton::MENU)) {
        Serial.println("Calibrating: turn the board in every direction...");
        magnetometer.calibrate(12);
        Serial.println("Calibration done.");
        lastPrint = millis();
    }

    if (millis() - lastPrint >= 500) {
        lastPrint = millis();
        const SteamiVector m = magnetometer.read();
        Serial.print("x=");
        Serial.print(m.x, 1);
        Serial.print(" y=");
        Serial.print(m.y, 1);
        Serial.print(" z=");
        Serial.print(m.z, 1);
        Serial.print(" uT, strength ");
        Serial.println(magnetometer.strength(), 1);
    }
    delay(5);
}
