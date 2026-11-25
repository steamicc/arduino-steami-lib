// SPDX-License-Identifier: GPL-3.0-or-later
// ProximityAlarm.ino
// Sound the buzzer while an object is closer than a configurable threshold.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiVL53L1X.h>

#ifndef SPEAKER
#define SPEAKER LED_BUILTIN
#endif

constexpr uint16_t ALARM_THRESHOLD_MM = 30;
constexpr uint16_t ALARM_TONE_HZ = 2000;

SteamiVL53L1X sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the USB serial port so the messages below are not lost.
    }

    pinMode(SPEAKER, OUTPUT);
    noTone(SPEAKER);

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected. Check the internal I2C bus.");
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
        tone(SPEAKER, ALARM_TONE_HZ);
    } else {
        noTone(SPEAKER);
    }

    delay(50);
}
