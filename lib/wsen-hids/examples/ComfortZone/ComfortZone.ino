// SPDX-License-Identifier: GPL-3.0-or-later
//
// ComfortZone — classify the room as comfortable / too dry / too humid /
// too cold / too hot based on the temperature-humidity chart taught in
// most building-physics courses.
//
// Thresholds picked for indoor comfort in temperate climates:
//   T  in [18 °C, 26 °C]
//   RH in [30 %, 65 %]
// Anything outside these bounds is flagged with a textual label.
//
// The WSEN-HIDS sits on the STeaMi internal I2C bus. Flash and open the
// serial monitor at 115200 baud.

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

const char* classifyComfort(float temperature, float humidity) {
    if (temperature < 18.0f) {
        return "Too cold";
    }
    if (temperature > 26.0f) {
        return "Too hot";
    }
    if (humidity < 30.0f) {
        return "Too dry";
    }
    if (humidity > 65.0f) {
        return "Too humid";
    }
    return "Comfortable";
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-HIDS not detected — check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    Serial.println("Comfort zone monitor started.");
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
    Serial.print(" %  ->  ");
    Serial.println(classifyComfort(reading.temperature, reading.humidity));

    // Wait for the next 1 Hz sample. The 10 ms poll above only kicks in
    // for the last fraction of a second before the next dataReady flag.
    delay(1000);
}
