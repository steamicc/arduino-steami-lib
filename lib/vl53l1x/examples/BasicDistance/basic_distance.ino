// SPDX-License-Identifier: GPL-3.0-or-later
//
// VL53L1X — read and print the measured distance over Serial at 10 Hz.

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

void setup() {
  Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t distance = sensor.read();

        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
        delay(100);
    }

    delay(50);
}