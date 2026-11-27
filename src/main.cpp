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
#include <SteamiBME280.h>
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiIMP34DT05.h>
#include <SteamiWsenHids.h>
#include <SteamiWsenPads.h>

// Every on-board driver sits on the board's internal I2C bus, which
// Steami.wire() returns (and starts on first use).
SteamiBME280 bme280(Steami.wire());
SteamiWsenHids wsen_hids(Steami.wire());
SteamiWsenPads wsen_pads(Steami.wire());
SteamiDaplinkBridge daplink_bridge(Steami.wire());

// The microphone is not on I2C: SAI1 clocks it on MIC_CLK and reads MIC_IN.
SteamiIMP34DT05 microphone;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("STeaMi smoke test");

    Steami.begin();
    Steami.printInfo();

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    // --- BME280 (pressure + humidity + temperature, V1c only) ---
    if (bme280.begin()) {
        Serial.print("BME280 detected, chip ID = 0x");
        Serial.println(bme280.deviceId(), HEX);

        auto r = bme280.read();
        Serial.print("BME280 first read: T = ");
        Serial.print(r.temperature, 2);
        Serial.print(" C, H = ");
        Serial.print(r.humidity, 1);
        Serial.print(" %, P = ");
        Serial.print(r.pressure, 2);
        Serial.println(" hPa");
    } else {
        Serial.println("BME280 not detected");
    }

    // --- WSEN-HIDS (humidity + temperature) ---
    if (wsen_hids.begin()) {
        Serial.print("WSEN-HIDS detected, WHO_AM_I = 0x");
        Serial.println(wsen_hids.deviceId(), HEX);

        wsen_hids.setContinuous(WSEN_HIDS_ODR_1_HZ);
        delay(1200);

        auto r = wsen_hids.read();
        Serial.print("WSEN-HIDS first read: T = ");
        Serial.print(r.temperature, 2);
        Serial.print(" C, H = ");
        Serial.print(r.humidity, 1);
        Serial.println(" %");
    } else {
        Serial.println("WSEN-HIDS not detected");
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

    // --- DAPLink bridge (I2C-to-flash protocol on the on-board F103) ---
    if (daplink_bridge.begin()) {
        Serial.print("DAPLink bridge detected, WHO_AM_I = 0x");
        Serial.println(daplink_bridge.deviceId(), HEX);
    } else {
        Serial.println("DAPLink bridge not detected");
    }

    // --- IMP34DT05 (PDM microphone on SAI1) ---
    if (microphone.begin()) {
        Serial.print("IMP34DT05 detected on SAI slot ");
        Serial.println(microphone.slot());

        delay(150);  // one 100 ms level window
        Serial.print("IMP34DT05 first level: ");
        Serial.print(microphone.levelDbfs(), 1);
        Serial.println(" dBFS");
        microphone.end();
    } else {
        Serial.println("IMP34DT05 not detected");
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
