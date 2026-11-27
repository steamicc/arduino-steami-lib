// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - show a compass rose on the screen, as mockup 07 of the tutorials
// (07_compass_ref.png): two circles, eight ticks, the letters N, E, S and W, and a needle.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen; both read the LIS2MDL magnetometer. The screen lays itself out for the
// panel it finds.
//
// Hold the board flat, screen up. The whole rose turns so that the needle and the N point to
// real north: at heading 90 (the top of the screen pointing east) the N letter is on the left.
//
// The heading is right only once the magnetometer is calibrated: the Compass example calibrates
// it for the session when you press MENU, and the board config can hold a calibration (the `cm`
// key, see "Calibration" in the README).
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the sensor does not answer, heading() gives NAN: the screen shows the rose with no needle.

#include <Arduino.h>
#include <SteamiCompass.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiCompass compass;

void setup() {
    Serial.begin(115200);  // the screen and the sensor report their errors here
    screen.begin();
    compass.begin();
}

void loop() {
    screen.clear();
    screen.compass(compass.heading());  // the needle points to real north
    screen.show();
    delay(50);
}
