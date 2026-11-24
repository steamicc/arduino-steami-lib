// SPDX-License-Identifier: GPL-3.0-or-later
//
// BQ27441 — blink the red LED when the battery drops below 20 % and sound
// the on-board buzzer when it falls below 5 %. The state of charge is
// printed to the serial monitor (115200 baud) on every check.
//
// The BQ27441 sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver.

#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 fuelGauge(internalI2C);

// Tune to the use case: the LED warns early, the buzzer only when the
// board is about to shut down.
constexpr uint8_t LOW_BATTERY_PERCENT = 20;
constexpr uint8_t CRITICAL_BATTERY_PERCENT = 5;

// LED_RED and SPEAKER are exported by the STeaMi variant.
constexpr int ALERT_LED_PIN = LED_RED;
constexpr int BUZZER_PIN = SPEAKER;

constexpr unsigned int BEEP_FREQUENCY_HZ = 1500;
constexpr uint32_t BEEP_DURATION_MS = 200;
constexpr uint32_t CHECK_PERIOD_MS = 1000;

void blinkLed() {
    digitalWrite(ALERT_LED_PIN, HIGH);
    delay(500);
    digitalWrite(ALERT_LED_PIN, LOW);
}

void beepTwice() {
    for (int i = 0; i < 2; i++) {
        tone(BUZZER_PIN, BEEP_FREQUENCY_HZ, BEEP_DURATION_MS);
        delay(BEEP_DURATION_MS + 100);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();
    pinMode(ALERT_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(ALERT_LED_PIN, LOW);

    if (!fuelGauge.begin()) {
        Serial.println("BQ27441 not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    uint8_t stateOfCharge = fuelGauge.stateOfCharge();

    Serial.print("State of charge: ");
    Serial.print(stateOfCharge);
    Serial.print(" %");

    if (stateOfCharge <= CRITICAL_BATTERY_PERCENT) {
        Serial.println(" — CRITICAL");
        blinkLed();
        beepTwice();
    } else if (stateOfCharge <= LOW_BATTERY_PERCENT) {
        Serial.println(" — low");
        blinkLed();
    } else {
        Serial.println();
    }

    delay(CHECK_PERIOD_MS);
}
