// SPDX-License-Identifier: GPL-3.0-or-later
// BasicDistance.ino
// Print the distance measured by the VL53L1X time-of-flight sensor, in millimetres.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiVL53L1X.h>

SteamiVL53L1X sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the USB serial port so the messages below are not lost.
    }

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
    Serial.println("VL53L1X basic distance example");
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t distance = sensor.distanceMm();
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
    }

    delay(50);
}
