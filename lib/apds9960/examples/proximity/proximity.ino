// SPDX-License-Identifier: GPL-3.0-or-later
//
// Proximity — detect nearby objects with the APDS9960.
//
// Open the serial monitor at 115200 baud.

#include <SteamiAPDS9960.h>
#include <SteamiBoard.h>

SteamiAPDS9960 sensor(Steami.wire());  // the board's internal I2C bus

static const uint8_t NEAR_THRESHOLD = 80;
static const uint32_t PRINT_PERIOD_MS = 100;

uint32_t lastPrintMs = 0;
bool wasNear = false;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (!sensor.begin()) {
        Serial.println("APDS9960 not detected.");
        while (true)
            delay(1000);
    }

    // Conservative settings to avoid saturating the proximity channel.
    sensor.setLedDrive(SteamiAPDS9960::LedDrive::MA_25);
    sensor.setLedBoost(SteamiAPDS9960::LedBoost::PERCENT_100);
    sensor.setProximityGain(SteamiAPDS9960::ProximityGain::X1);
    sensor.enableProximitySensor(false);

    Serial.println("=======================");
    Serial.println("   Proximity detector  ");
    Serial.println("=======================");
    Serial.print("Near threshold: ");
    Serial.println(NEAR_THRESHOLD);
}

void loop() {
    uint32_t now = millis();

    uint32_t elapsed = now - lastPrintMs;
    if (elapsed < PRINT_PERIOD_MS) {
        delay(PRINT_PERIOD_MS - elapsed);
        return;
    }

    lastPrintMs = now;

    uint8_t proximity = 0;
    if (!sensor.proximity(proximity)) {
        Serial.println("Proximity read failed.");
        return;
    }

    bool isNear = proximity >= NEAR_THRESHOLD;

    Serial.print("Proximity=");
    Serial.print(proximity);
    Serial.print(" | ");
    Serial.println(isNear ? "OBJECT NEAR" : "clear");

    if (isNear != wasNear) {
        wasNear = isNear;
        digitalWrite(LED_BUILTIN, isNear ? HIGH : LOW);
    }
}
