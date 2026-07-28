// SPDX-License-Identifier: GPL-3.0-or-later
#include <ISM330DL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL imu(internalI2C);

ISM330DL::Orientation lastOrientation = ISM330DL::Orientation::MOVING;

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

    Serial.println("Rotate the board...");
}

void loop() {
    ISM330DL::Orientation orientation;

    if (imu.orientation(orientation) && orientation != lastOrientation) {
        lastOrientation = orientation;

        Serial.print("Orientation: ");
        Serial.println(ISM330DL::orientationToString(orientation));
    }

    delay(300);
}
