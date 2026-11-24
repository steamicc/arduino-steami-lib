// SPDX-License-Identifier: GPL-3.0-or-later
//
// ComfortZone - classify the room as comfortable / too dry / too humid /
// too cold / too hot.
//
// Thresholds picked for indoor comfort in temperate climates:
//   T  in [18 C, 26 C]
//   RH in [30 %, 65 %]
// Anything outside these bounds is flagged with a textual label. The dew
// point is printed too: above about 16 C the air feels muggy.
//
// The BME280 of the STeaMi V1c sits on the internal I2C bus. Flash and open
// the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiBME280.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiBME280 sensor(internalI2C);

const char* classifyComfort(float temperature, float humidity) {
    if (temperature < 18.0f) {
        return "Too cold";
    }
    if (temperature > 26.0f) {
        return "Too hot";
    }
    if (humidity < 30.0f) {
        return "Too dry";
    }
    if (humidity > 65.0f) {
        return "Too humid";
    }
    return "Comfortable";
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("BME280 not detected - check that this is a STeaMi V1c board.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Comfort zone monitor started.");
}

void loop() {
    auto reading = sensor.read();

    if (isnan(reading.temperature) || isnan(reading.humidity)) {
        Serial.println("BME280 read failed.");
    } else {
        Serial.print("T = ");
        Serial.print(reading.temperature, 2);
        Serial.print(" C  |  H = ");
        Serial.print(reading.humidity, 1);
        Serial.print(" %  |  Td = ");
        Serial.print(SteamiBME280::dewPointFrom(reading.temperature, reading.humidity), 1);
        Serial.print(" C  ->  ");
        Serial.println(classifyComfort(reading.temperature, reading.humidity));
    }

    delay(1000);
}
