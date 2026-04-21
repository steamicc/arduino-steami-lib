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

// Alarm state, updated whenever a fresh reading lands.
bool alarmActive = false;

// Non-blocking beep state — the buzzer toggles independently of the
// sensor polling so the 100/100 ms pattern stays audible even though
// the sensor only delivers one sample per second.
bool beepOn = false;
uint32_t beepChangedAt = 0;

void updateBuzzer() {
    uint32_t now = millis();

    if (!alarmActive) {
        if (beepOn) {
            noTone(BUZZER_PIN);
            beepOn = false;
        }
        return;
    }

    uint32_t elapsed = now - beepChangedAt;
    if (beepOn && elapsed >= BEEP_ON_MS) {
        noTone(BUZZER_PIN);
        beepOn = false;
        beepChangedAt = now;
    } else if (!beepOn && elapsed >= BEEP_OFF_MS) {
        tone(BUZZER_PIN, BEEP_FREQUENCY_HZ);
        beepOn = true;
        beepChangedAt = now;
    }
}

void loop() {
    if (sensor.dataReady()) {
        float temperatureC = sensor.temperature();

        Serial.print("Temperature: ");
        Serial.print(temperatureC, 2);
        Serial.println(" C");

        bool aboveThreshold = temperatureC > ALARM_THRESHOLD_C;
        if (aboveThreshold && !alarmActive) {
            Serial.println("ALARM");
        }
        alarmActive = aboveThreshold;
    }

    updateBuzzer();
    delay(10);
}
