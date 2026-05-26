// SPDX-License-Identifier: GPL-3.0-or-later

// WSEN-PADS — baseline sketch: print barometric pressure (hPa) and
// temperature (°C) once per second on the serial monitor. Useful as a
// starting point for any WSEN-PADS application or as a hardware sanity
// check.

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

    auto reading = sensor.read();
    Serial.print("P=");
    Serial.print(reading.pressure, 2);
    Serial.print(" hPa  T=");
    Serial.print(reading.temperature, 2);
    Serial.println(" C");

    delay(1000);
}
