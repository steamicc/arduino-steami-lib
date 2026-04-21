// SPDX-License-Identifier: GPL-3.0-or-later
//
// HTS221 — classify ambient conditions as comfortable, too dry, too humid,
// too cold, or too hot, using simple humidity and temperature thresholds.
// Useful as a starting point for climate-aware UI or HVAC logic.

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>

// HTS221 sits on the STeaMi internal I2C bus (PB8 / PB9 via the variant
// macros), not the default global Wire.
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

// Thresholds chosen for indoor "office" conditions; tune to taste.
constexpr float HUMIDITY_DRY = 40.0f;
constexpr float HUMIDITY_HUMID = 60.0f;
constexpr float TEMPERATURE_COLD = 18.0f;
constexpr float TEMPERATURE_HOT = 26.0f;

const char* comfortLabel(float temperature, float humidity) {
    if (humidity < HUMIDITY_DRY)
        return "Too dry";
    if (humidity > HUMIDITY_HUMID)
        return "Too humid";
    if (temperature < TEMPERATURE_COLD)
        return "Cold";
    if (temperature > TEMPERATURE_HOT)
        return "Hot";
    return "Comfortable";
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("HTS221 not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(HTS221_ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();

    Serial.print("T=");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  H=");
    Serial.print(reading.humidity, 1);
    Serial.print(" %  -> ");
    Serial.println(comfortLabel(reading.temperature, reading.humidity));

    delay(1000);
}
