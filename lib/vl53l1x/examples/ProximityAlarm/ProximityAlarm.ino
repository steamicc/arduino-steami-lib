// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

constexpr uint16_t ALARM_THRESHOLD_MM = 30;
constexpr uint16_t ALARM_TONE_HZ = 2000;
constexpr uint8_t BUZZER_PIN = SPEAKER;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // Wait up to 2 s for the host USB CDC to enumerate.
    }

    pinMode(BUZZER_PIN, OUTPUT);
    noTone(BUZZER_PIN);

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("VL53L1X not found. Check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
    Serial.println("VL53L1X proximity alarm example");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(20);
        return;
    }

    uint16_t distance = sensor.distanceMm();
    bool objectClose = distance > 0 && distance <= ALARM_THRESHOLD_MM;

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" mm");

    if (objectClose) {
        tone(BUZZER_PIN, ALARM_TONE_HZ);
    } else {
        noTone(BUZZER_PIN);
    }

    delay(50);
}
