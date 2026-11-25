// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board — print the detected board release (V1a, V1b or V1c), where
// the answer comes from and which I2C chips answered (115200 baud).
//
// The release is found from three sources, in this order:
//   1. a forced value: Steami.begin(SteamiRelease::V1C) at run time, or
//      #define STEAMI_BOARD_V1C (or _V1A, _V1B) before the #include below;
//   2. the board revision stored in the DAPLink config zone (when the
//      firmware has one);
//   3. an I2C probe of the sensors: BME280 means V1c, WSEN-HIDS, WSEN-PADS
//      or APDS-9960 means V1a/V1b.
//
// To force a V1c, uncomment the next line (it must come before the include):
// #define STEAMI_BOARD_V1C

#include <Arduino.h>
#include <SteamiBoard.h>

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // banner below isn't silently dropped.
    }

    Steami.begin();
    Steami.printInfo();
}

void loop() {}
