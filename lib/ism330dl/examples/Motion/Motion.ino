// SPDX-License-Identifier: GPL-3.0-or-later
#include <SteamiBoard.h>
#include <SteamiISM330DL.h>

SteamiISM330DL imu(Steami.wire());  // the board's internal I2C bus

SteamiISM330DL::MotionType lastMotion = SteamiISM330DL::MotionType::STABLE;

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

    Serial.println("Move the board...");
}

void loop() {
    SteamiISM330DL::Motion motion;

    if (imu.motion(motion) && motion.type != lastMotion) {
        lastMotion = motion.type;

        Serial.print("Motion: ");
        Serial.print(SteamiISM330DL::motionToString(motion.type));

        if (motion.type != SteamiISM330DL::MotionType::STABLE) {
            Serial.print(" (");
            Serial.print(motion.value, 1);
            Serial.print(" dps)");
        }

        Serial.println();
    }

    delay(100);
}
