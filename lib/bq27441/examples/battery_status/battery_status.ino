// SPDX-License-Identifier: GPL-3.0-or-later
//
// BQ27441 — print voltage, state of charge, remaining capacity and state
// of health to the serial monitor every 3 seconds.
//
// The BQ27441 sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see the live readings.

#include <Arduino.h>
#include <SteamiBQ27441.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiBQ27441 fuelGauge(internalI2C);

constexpr uint32_t REFRESH_PERIOD_MS = 3000;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!fuelGauge.begin()) {
        Serial.println("BQ27441 not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    Serial.print("Voltage:            ");
    Serial.print(fuelGauge.voltageMv());
    Serial.println(" mV");

    Serial.print("State of charge:    ");
    Serial.print(fuelGauge.stateOfCharge());
    Serial.println(" %");

    Serial.print("Remaining capacity: ");
    Serial.print(fuelGauge.capacityRemaining());
    Serial.println(" mAh");

    Serial.print("State of health:    ");
    Serial.print(fuelGauge.stateOfHealth());
    Serial.println(" %");

    Serial.println();
    delay(REFRESH_PERIOD_MS);
}
