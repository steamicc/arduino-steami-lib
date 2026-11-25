// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board — print the detected board release (V1a, V1b or V1c), where
// the answer comes from and the state of the I2C chips of that release
// (115200 baud).
//
// The release is found from three sources, in this order:
//   1. a forced value: Steami.begin(SteamiRelease::V1C) at run time, or
//      #define STEAMI_BOARD_V1C (or _V1A, _V1B) before the #include below;
//   2. the board revision stored in the DAPLink config zone (when the
//      firmware has one);
//   3. an I2C probe of the sensors: BME280 means V1c, WSEN-HIDS, WSEN-PADS
//      or APDS-9960 means V1a/V1b.
//
// printInfo() lists the I2C chips of the release with OK, "not responding"
// or "not tried". "Not tried" means that no function used that chip yet, so
// this sketch first claims the chip of every function (the screen aside):
// each line then shows whether the chip really answers.
//
// To force a V1c, uncomment the next line (it must come before the include):
// #define STEAMI_BOARD_V1C

#include <Arduino.h>
#include <SteamiBoard.h>

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is the LPUART wired to the DAPLink virtual COM port, not
        // USB CDC: it is ready as soon as Serial.begin() returns, so this
        // loop ends at once.
    }

    Steami.begin();
    for (uint8_t f = 0; f < static_cast<uint8_t>(SteamiFunction::COUNT); f++) {
        const SteamiFunction function = static_cast<SteamiFunction>(f);
        if (function != SteamiFunction::SCREEN) {
            Steami.claim(steamiRoute(Steami.release(), function, 0));
        }
    }
    Steami.printInfo();
}

void loop() {}
