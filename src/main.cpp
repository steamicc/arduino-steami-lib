// SPDX-License-Identifier: GPL-3.0-or-later
//
// Build smoke-test sketch: validates the board definition, the STM32duino
// variant, and driver linkage for every implemented driver in the
// collection. As a side effect, every driver's .cpp file ends up in
// `compile_commands.json` (the native env follows the includes here),
// which gives `make tidy` proper compile flags for each driver.
//
// When a new driver lands, add its `#include`, instantiate it on the
// right bus, and add a `begin()` + identity probe section below. The
// loop stays a simple LED blink — this file is not an example, see
// lib/<component>/examples/ for driver usage.

#include <Arduino.h>
#include <HTS221.h>
#include <WSEN_PADS.h>
#include <Wire.h>

// The STeaMi routes its on-board I2C peripherals to an internal bus
// (pins PB8/PB9, exported as I2C_INT_SCL / I2C_INT_SDA by the variant).
// The default global Wire sits on a different pair, so spin up a
// dedicated TwoWire and hand it to every on-board driver.
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);

HTS221 hts221(internalI2C);
WSEN_PADS wsen_pads(internalI2C);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("STeaMi smoke test");

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    internalI2C.begin();

    // --- HTS221 (humidity + temperature) ---
    if (hts221.begin()) {
        Serial.print("HTS221 detected, WHO_AM_I = 0x");
        Serial.println(hts221.deviceId(), HEX);

        auto r = hts221.read();
        Serial.print("HTS221 first read: T = ");
        Serial.print(r.temperature, 2);
        Serial.print(" C, H = ");
        Serial.print(r.humidity, 1);
        Serial.println(" %");
    } else {
        Serial.println("HTS221 not detected");
    }

    // --- WSEN-PADS (pressure + temperature) ---
    if (wsen_pads.begin()) {
        Serial.print("WSEN-PADS detected, DEVICE_ID = 0x");
        Serial.println(wsen_pads.deviceId(), HEX);

        auto r = wsen_pads.read();
        Serial.print("WSEN-PADS first read: P = ");
        Serial.print(r.pressure, 2);
        Serial.print(" hPa, T = ");
        Serial.print(r.temperature, 2);
        Serial.println(" C");
    } else {
        Serial.println("WSEN-PADS not detected");
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
