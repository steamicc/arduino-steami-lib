// SPDX-License-Identifier: GPL-3.0-or-later

// WSEN-PADS — read barometric pressure and temperature once per second and
// print both values to the serial monitor. Useful as a starting point for
// weather station or altitude-sensing applications.

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

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
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read(false);

    Serial.print("Temperature= ");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  Pressure= ");
    Serial.print(reading.pressure, 2);
    Serial.println(" hPa");

    delay(1000);
}
