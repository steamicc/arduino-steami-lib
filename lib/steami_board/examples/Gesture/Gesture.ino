// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print each hand gesture seen by the APDS-9960 (115200 baud).
//
// V1a and V1b only. A V1c has no gesture sensor: begin() returns false and the
// library logs one message ending in (E20).
//
// Move a hand above the sensor: LEFT, RIGHT, UP, DOWN, NEAR or FAR. Nothing is
// printed while there is no gesture. begin() turns the gesture engine on, and
// while it tracks a hand the light and colour readings freeze.

#include <Arduino.h>
#include <SteamiGesture.h>

SteamiGesture gesture;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (gesture.begin()) {
        Serial.print("Gesture sensor: ");
        Serial.println(gesture.chip());
    } else {
        Serial.print("No gesture sensor: ");
        Serial.println(gesture.error());
    }
}

void loop() {
    const SteamiGesture::Direction direction = gesture.read(200);
    if (direction != SteamiGesture::NONE) {
        Serial.println(SteamiGesture::name(direction));
    }
}
