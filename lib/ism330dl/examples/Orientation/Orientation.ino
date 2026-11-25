// SPDX-License-Identifier: GPL-3.0-or-later
#include <SteamiBoard.h>
#include <SteamiISM330DL.h>

SteamiISM330DL imu(Steami.wire());  // the board's internal I2C bus

SteamiISM330DL::Orientation lastOrientation = SteamiISM330DL::Orientation::MOVING;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    if (!imu.begin()) {
        Serial.println("ISM330DL not detected.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Rotate the board...");
}

void loop() {
    SteamiISM330DL::Orientation orientation;

    if (imu.orientation(orientation) && orientation != lastOrientation) {
        lastOrientation = orientation;

        Serial.print("Orientation: ");
        Serial.println(SteamiISM330DL::orientationToString(orientation));
    }

    delay(300);
}
