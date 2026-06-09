// SPDX-License-Identifier: GPL-3.0-or-later
//
// VL53L1X — count objects crossing a baseline threshold, using a
// state flag to avoid counting the same passage multiple times.

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

float distanceMoy = 0.0f;
uint8_t counter = 0;
bool objectDetected = false;

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

    for (int i = 0; i < 20; i++) {
        distanceMoy += sensor.read();
    }
    distanceMoy /= 20.0f;
    Serial.print("end of calibration ");

    sensor.startRanging();
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t distance = sensor.read();

        if(distance < distanceMoy - 50 && !objectDetected) {
            counter++;
            delay(700);
            objectDetected = true;
            Serial.print("Object detected! Counter: ");
            Serial.println(counter);
        }
        if (distance >= distanceMoy - 50) {
            objectDetected = false;
        }
    }

    delay(50);
}