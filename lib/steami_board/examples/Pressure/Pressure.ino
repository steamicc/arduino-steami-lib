// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the pressure in hPa and the altitude in metres once
// per second (115200 baud).
//
// Works on every release: the BME280 on a V1c, the WSEN-PADS on a V1a or
// V1b. chip() tells which one answered. The altitude comes from the same
// formula on every release, with the standard sea-level pressure of
// 1013.25 hPa; pass your local value to altitude() for a better figure.
//
// If no sensor answers, begin() returns false and error() says why; the
// library also logs one message with its code.

#include <Arduino.h>
#include <SteamiPressure.h>

SteamiPressure pressure;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (pressure.begin()) {
        Serial.print("Pressure sensor: ");
        Serial.println(pressure.chip());
    } else {
        Serial.print("No pressure sensor: ");
        Serial.println(pressure.error());
    }
}

void loop() {
    const float hpa = pressure.read();
    if (!isnan(hpa)) {
        Serial.print(hpa, 1);
        Serial.print(" hPa, ");
        Serial.print(pressure.altitude(), 0);
        Serial.println(" m");
    }
    delay(1000);
}
