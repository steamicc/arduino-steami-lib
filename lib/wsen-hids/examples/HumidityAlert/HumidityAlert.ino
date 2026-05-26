// SPDX-License-Identifier: GPL-3.0-or-later
//
// HumidityAlert — sound the on-board buzzer when ambient humidity climbs
// past a mold-risk threshold.
//
// The WSEN-HIDS sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see the live readings.

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

constexpr float HUMIDITY_THRESHOLD = 70.0f;  // %RH — mold risk above this

void buzzAlert() {
    tone(SPEAKER, 1500, 200);
    delay(300);
    tone(SPEAKER, 1500, 200);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-HIDS not detected — check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    Serial.println("Humidity alert started.");
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

    if (reading.humidity > HUMIDITY_THRESHOLD) {
        Serial.println("Warning: humidity too high (mold risk).");
        buzzAlert();
    }
}
