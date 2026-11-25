// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasicReader - print temperature, humidity and pressure every second.
//
// The BME280 of the STeaMi V1c sits on the internal I2C bus (address 0x76).
// read() starts one conversion, waits for it and returns the three channels
// together, so the values are consistent with each other.
//
// Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiBME280.h>
#include <SteamiBoard.h>

SteamiBME280 sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    if (!sensor.begin()) {
        Serial.println("BME280 not detected - check that this is a STeaMi V1c board.");
        while (true) {
            delay(1000);
        }
    }

    Serial.print("BME280 detected, chip ID = 0x");
    Serial.println(sensor.deviceId(), HEX);
}

void loop() {
    auto reading = sensor.read();

    if (isnan(reading.temperature)) {
        Serial.println("BME280 read failed.");
    } else {
        Serial.print("T = ");
        Serial.print(reading.temperature, 2);
        Serial.print(" C  |  H = ");
        Serial.print(reading.humidity, 1);
        Serial.print(" %  |  P = ");
        Serial.print(reading.pressure, 2);
        Serial.println(" hPa");
    }

    delay(1000);
}
