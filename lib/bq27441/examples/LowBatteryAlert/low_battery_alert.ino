// SPDX-License-Identifier: GPL-3.0-or-later
//
// LowBatteryAlert — blink the red LED when the battery drops below 20 %
// and sound the on-board buzzer when it falls below 5 %.
//
// The BQ27441 sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see the live readings.

#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 sensor(internalI2C);

void blink() {
    digitalWrite(LED_RED, HIGH);
    delay(500);
    digitalWrite(LED_RED, LOW);
    delay(500);
}

void buzzAlert() {
    tone(SPEAKER, 1500, 200);
    delay(300);
    tone(SPEAKER, 1500, 200);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        while (true) {
            delay(1000);
            Serial.println("Failed to initialize BQ27441! Check your wiring.");
        }
    }

    pinMode(LED_RED, OUTPUT);
    pinMode(SPEAKER, OUTPUT);
}

void loop() {
    uint8_t state = sensor.stateOfCharge();
    if (state <= 20) {
        blink();
        delay(200);
    }
    if (state <= 5) {
        buzzAlert();
        delay(1000);
    } else {
        delay(1000);
    }
}