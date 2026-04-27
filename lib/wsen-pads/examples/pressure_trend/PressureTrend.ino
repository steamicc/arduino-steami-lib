// SPDX-License-Identifier: GPL-3.0-or-later
// WSEN-PADS — monitor temperature trends (rise, drop, stable) using repeated
// sampling and basic averaging. Useful for simple thermal behavior detection
// and debugging sensor drift.

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

float temperatureAverage = 0;
float currentTemperature = 0;

float initialization() {
    float temperatureAverage = 0;
    for (int i = 0; i < 10; i++) {
        while (!sensor.dataReady()) {
            delay(10);
        }
        auto reading = sensor.read(false);
        temperatureAverage += reading.temperature;
        delay(20);
    }
    return temperatureAverage / 10.0f;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-PADS not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(ODR_1_HZ);

    temperatureAverage = initialization();
    currentTemperature = temperatureAverage;
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read(false);
    float newTemperatureAverage = reading.temperature;

    if (newTemperatureAverage < currentTemperature) {
        Serial.println("temperatures drop");
    } else if (newTemperatureAverage > currentTemperature) {
        Serial.println("temperatures rise");
    } else {
        Serial.println("temperatures stable");
    }
    Serial.println(newTemperatureAverage);
    currentTemperature = newTemperatureAverage;
    delay(2000);
}