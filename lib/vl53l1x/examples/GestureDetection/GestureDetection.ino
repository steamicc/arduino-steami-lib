// SPDX-License-Identifier: GPL-3.0-or-later
// GestureDetection.ino
// Wave a hand quickly in front of the sensor to toggle the LED on or off.
// A wave is a hand that comes closer than NEAR_MM, then moves away beyond FAR_MM
// within WAVE_MAX_MS. A hand that stays in front of the sensor does nothing.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiVL53L1X.h>

#ifndef LED_BLUE
#define LED_BLUE LED_BUILTIN
#endif

constexpr uint16_t NEAR_MM = 150;
constexpr uint16_t FAR_MM = 250;  // larger than NEAR_MM: ignores jitter around the threshold
constexpr uint32_t WAVE_MAX_MS = 600;
constexpr uint8_t NEAR_READINGS = 2;  // consecutive near readings: ignores a single spurious one

SteamiVL53L1X sensor(Steami.wire());  // the board's internal I2C bus

static bool ledOn = false;
static bool handNear = false;
static uint32_t handArrivedMs = 0;
static uint8_t nearCount = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the USB serial port so the messages below are not lost.
    }

    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_BLUE, LOW);

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
    Serial.println("VL53L1X gesture example: wave a hand to toggle the LED");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    uint16_t distance = sensor.distanceMm();
    bool near = distance > 0 && distance < NEAR_MM;
    bool far = distance == 0 || distance > FAR_MM;  // 0: nothing in range

    if (!near) {
        nearCount = 0;
    } else if (nearCount < NEAR_READINGS) {
        if (nearCount == 0) {
            handArrivedMs = millis();  // the wave starts with the first near reading
        }
        ++nearCount;
    }

    if (!handNear && nearCount >= NEAR_READINGS) {
        handNear = true;
    } else if (handNear && far) {
        handNear = false;
        if (millis() - handArrivedMs <= WAVE_MAX_MS) {
            ledOn = !ledOn;
            digitalWrite(LED_BLUE, ledOn ? HIGH : LOW);
            Serial.println(ledOn ? "Wave: LED on" : "Wave: LED off");
        }
    }
}
