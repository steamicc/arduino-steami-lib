// SPDX-License-Identifier: GPL-3.0-or-later
//
// Build smoke-test sketch: validates the board definition, the STM32duino
// variant, and driver linkage for the sensors that have already landed.
// Not an example — see lib/<component>/examples/ for driver usage.

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>

HTS221 hts221;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("STeaMi smoke test");

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    Wire.begin();
    if (hts221.begin()) {
        Serial.print("HTS221 detected, WHO_AM_I = 0x");
        Serial.println(hts221.deviceId(), HEX);
    } else {
        Serial.println("HTS221 not detected");
    }
}

void loop() {
    Serial.println("Blink");

    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    delay(500);

    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    delay(500);
}
