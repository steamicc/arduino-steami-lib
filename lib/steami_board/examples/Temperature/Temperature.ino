// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the temperature once per second (115200 baud).
//
// Works on every release: the BME280 on a V1c, the WSEN-HIDS (then the
// WSEN-PADS) on a V1a or V1b. chip() tells which one answered. The value is
// in degrees Celsius, with the calibration of the board config applied when
// there is one.
//
// If no sensor answers, begin() returns false and error() says why; the
// library also logs one message with its code.

#include <Arduino.h>
#include <SteamiTemperature.h>

SteamiTemperature temperature;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (temperature.begin()) {
        Serial.print("Temperature sensor: ");
        Serial.println(temperature.chip());
    } else {
        Serial.print("No temperature sensor: ");
        Serial.println(temperature.error());
    }
}

void loop() {
    const float celsius = temperature.read();
    if (!isnan(celsius)) {
        Serial.print(celsius, 1);
        Serial.println(" C");
    }
    delay(1000);
}
