// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    delay(500);

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("begin_failed=1");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(HTS221_ODR_1_HZ);

    Serial.println("ready=1");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();

    Serial.print("temperature=");
    Serial.print(reading.temperature, 2);
    Serial.print(" ");

    Serial.print("humidity=");
    Serial.print(reading.humidity, 2);
    Serial.println();

    delay(50);
}
