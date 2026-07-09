// SPDX-License-Identifier: GPL-3.0-or-later#include <Arduino.h>
#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("VL53L1X not found");
        while (1)
            ;
    }

    sensor.startRanging();
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
