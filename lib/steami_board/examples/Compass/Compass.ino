// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the heading measured by the LIS2MDL twice per second
// (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Hold the board flat, screen up.
// heading() is in degrees clockwise from north, 0 when the top of the screen
// (the side opposite the USB connector) points north, 90 for east. direction()
// gives the nearest of eight sectors ("N", "NE", "E", ...).
//
// Press MENU to calibrate for 12 seconds: turn the board slowly in every
// direction meanwhile. The calibration lives in RAM and is lost on reset.
//
// If the sensor does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiButtons.h>
#include <SteamiCompass.h>

SteamiCompass compass;
SteamiButtons buttons;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (compass.begin()) {
        Serial.println("Compass ready. Press MENU to calibrate (12 s).");
    } else {
        Serial.print("No compass: ");
        Serial.println(compass.error());
    }
    buttons.begin();
}

void loop() {
    // Poll the buttons often: a short tap is missed if they are read only twice per second.
    static uint32_t lastPrint = 0;

    if (buttons.wasPressed(SteamiButton::MENU)) {
        Serial.println("Calibrating: turn the board in every direction...");
        compass.calibrate(12);
        Serial.println("Calibration done.");
        lastPrint = millis();
    }

    if (millis() - lastPrint >= 500) {
        lastPrint = millis();
        Serial.print(compass.heading(), 0);
        Serial.print(" deg ");
        Serial.println(compass.direction());
    }
    delay(5);
}
