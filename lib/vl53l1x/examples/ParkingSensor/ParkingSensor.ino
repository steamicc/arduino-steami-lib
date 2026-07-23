// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

constexpr uint8_t BUZZER_PIN = SPEAKER;
constexpr uint16_t MIN_DISTANCE_MM = 10;
constexpr uint16_t MAX_DISTANCE_MM = 200;
constexpr uint16_t MIN_BEEP_DELAY_MS = 10;
constexpr uint16_t MAX_BEEP_DELAY_MS = 700;
constexpr uint16_t BEEP_DURATION_MS = 20;
constexpr uint16_t BEEP_TONE_HZ = 1600;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

unsigned long lastBeepMs = 0;

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
    Serial.println("VL53L1X parking sensor example");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(20);
        return;
    }

    uint16_t distance = sensor.distanceMm();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" mm");

    if (distance == 0 || distance > MAX_DISTANCE_MM) {
        noTone(BUZZER_PIN);
        delay(50);
        return;
    }

    distance = constrain(distance, MIN_DISTANCE_MM, MAX_DISTANCE_MM);
    uint16_t beepDelay =
        map(distance, MIN_DISTANCE_MM, MAX_DISTANCE_MM, MIN_BEEP_DELAY_MS, MAX_BEEP_DELAY_MS);

    if (millis() - lastBeepMs >= beepDelay) {
        tone(BUZZER_PIN, BEEP_TONE_HZ, BEEP_DURATION_MS);
        lastBeepMs = millis();
    }
}
