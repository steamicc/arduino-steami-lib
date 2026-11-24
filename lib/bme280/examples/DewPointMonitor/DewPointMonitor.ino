// SPDX-License-Identifier: GPL-3.0-or-later
//
// DewPointMonitor - compute the dew point from temperature and humidity,
// and warn when condensation risk is high (T - Td < 2 C).
//
// dewPointFrom() uses the Magnus formula. dewPoint() would take a new
// reading; here the values already come from a single read() so that
// temperature and humidity stay consistent.
//
// The BME280 of the STeaMi V1c sits on the internal I2C bus. Flash and open
// the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiBME280.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiBME280 sensor(internalI2C);

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

    Serial.println("Dew point monitor started.");
}

void loop() {
    auto reading = sensor.read();

    if (isnan(reading.temperature) || isnan(reading.humidity)) {
        Serial.println("BME280 read failed.");
    } else {
        float dewPoint = SteamiBME280::dewPointFrom(reading.temperature, reading.humidity);

        Serial.print("T = ");
        Serial.print(reading.temperature, 2);
        Serial.print(" C  |  H = ");
        Serial.print(reading.humidity, 1);
        Serial.print(" %  |  Td = ");
        Serial.print(dewPoint, 2);
        Serial.println(" C");

        if ((reading.temperature - dewPoint) < 2.0f) {
            Serial.println("Condensation risk is high.");
        }
    }

    delay(1000);
}
