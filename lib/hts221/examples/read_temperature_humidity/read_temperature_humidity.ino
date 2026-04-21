// SPDX-License-Identifier: GPL-3.0-or-later
//
// HTS221 — read humidity and temperature at 1 Hz and print them over Serial.
//
// Wiring on the STeaMi board: the HTS221 sits on the internal I2C bus, no
// external hookup needed. Just flash this example and open the serial
// monitor at 115200 baud.

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>

HTS221 sensor;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    Wire.begin();

    if (!sensor.begin()) {
        Serial.println("HTS221 not detected — check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    Serial.print("HTS221 detected (WHO_AM_I = 0x");
    Serial.print(sensor.deviceId(), HEX);
    Serial.println(")");

    sensor.setContinuous(HTS221_ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();

    Serial.print("T = ");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  |  H = ");
    Serial.print(reading.humidity, 1);
    Serial.println(" %");

    delay(1000);
}
