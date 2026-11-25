// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the relative humidity once per second (115200 baud).
//
// Works on every release: the BME280 on a V1c, the WSEN-HIDS on a V1a or
// V1b. chip() tells which one answered.
//
// If no sensor answers, begin() returns false and error() says why; the
// library also logs one message with its code.

#include <Arduino.h>
#include <SteamiHumidity.h>

SteamiHumidity humidity;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (humidity.begin()) {
        Serial.print("Humidity sensor: ");
        Serial.println(humidity.chip());
    } else {
        Serial.print("No humidity sensor: ");
        Serial.println(humidity.error());
    }
}

void loop() {
    const float percent = humidity.read();
    if (!isnan(percent)) {
        Serial.print(percent, 1);
        Serial.println(" %RH");
    }
    delay(1000);
}
