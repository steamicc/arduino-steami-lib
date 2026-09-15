// SPDX-License-Identifier: GPL-3.0-or-later
#include <ISM330DL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL imu(internalI2C);

ISM330DL::MotionType lastMotion = ISM330DL::MotionType::STABLE;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();

    if (!imu.begin()) {
        Serial.println("ISM330DL not detected.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Move the board...");
}

void loop() {
    ISM330DL::Motion motion;

    if (imu.motion(motion) && motion.type != lastMotion) {
        lastMotion = motion.type;

        Serial.print("Motion: ");
        Serial.print(ISM330DL::motionToString(motion.type));

        if (motion.type != ISM330DL::MotionType::STABLE) {
            Serial.print(" (");
            Serial.print(motion.value, 1);
            Serial.print(" dps)");
        }

        Serial.println();
    }

    delay(100);
}
