// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the battery charge, voltage and current once per
// second (115200 baud).
//
// Works on every release (V1a, V1b and V1c): the BQ27441 gauge. The current
// is positive while the battery charges.
//
// If the gauge does not answer, begin() returns false and error() says why;
// the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiBattery.h>

SteamiBattery battery;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (battery.begin()) {
        Serial.print("Battery gauge: ");
        Serial.println(battery.chip());
    } else {
        Serial.print("No battery gauge: ");
        Serial.println(battery.error());
    }
}

void loop() {
    if (battery.isAvailable()) {
        Serial.print(battery.percent());
        Serial.print(" %, ");
        Serial.print(battery.voltage(), 2);
        Serial.print(" V, ");
        Serial.print(battery.current());
        Serial.println(battery.isCharging() ? " mA (charging)" : " mA");
    }
    delay(1000);
}
