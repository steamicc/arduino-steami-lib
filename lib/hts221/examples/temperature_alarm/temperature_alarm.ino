// SPDX-License-Identifier: GPL-3.0-or-later
//
// HTS221 — monitor temperature continuously and buzz the on-board SPEAKER
// pin whenever it crosses a configurable high threshold. A short beep
// (100 ms on / 100 ms off) repeats while the temperature stays above the
// limit; silence resumes as soon as it drops back.

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

// Tune to the use case. 30 C triggers indoors in most climates; lower it
// for refrigeration alarms, raise it for heat-soak warnings.
constexpr float ALARM_THRESHOLD_C = 30.0f;

// SPEAKER is exported by the STeaMi variant; on the board it's a
// piezo-style buzzer on PA11.
constexpr int BUZZER_PIN = SPEAKER;

constexpr unsigned int BEEP_FREQUENCY_HZ = 2000;
constexpr uint32_t BEEP_ON_MS = 100;
constexpr uint32_t BEEP_OFF_MS = 100;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();
    pinMode(BUZZER_PIN, OUTPUT);

    if (!sensor.begin()) {
        Serial.println("HTS221 not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(HTS221_ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    float temperatureC = sensor.temperature();

    Serial.print("Temperature: ");
    Serial.print(temperatureC, 2);
    Serial.println(" C");

    if (temperatureC > ALARM_THRESHOLD_C) {
        Serial.println("ALARM");
        tone(BUZZER_PIN, BEEP_FREQUENCY_HZ);
        delay(BEEP_ON_MS);
        noTone(BUZZER_PIN);
        delay(BEEP_OFF_MS);
    } else {
        noTone(BUZZER_PIN);
        delay(500);
    }
}
