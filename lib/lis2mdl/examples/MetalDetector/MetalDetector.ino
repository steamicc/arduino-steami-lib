// SPDX-License-Identifier: GPL-3.0-or-later
// MetalDetector.ino
// Calibrate a local magnetic baseline, then buzz when the field magnitude changes.

#include <Arduino.h>
#include <LIS2MDL.h>
#include <Wire.h>

#ifndef SPEAKER
#define SPEAKER LED_BUILTIN
#endif

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL magnetometer(internalI2C);

static float baselineUt = 0.0f;
static float triggerDeltaUt = 20.0f;

static float averageMagnitude(uint16_t samples, uint16_t delayMs) {
    float sum = 0.0f;
    for (uint16_t i = 0; i < samples; ++i) {
        sum += magnetometer.magnitudeUt();
        delay(delayMs);
    }
    return sum / samples;
}

static void beep(uint16_t frequency, uint16_t durationMs) {
#if defined(SPEAKER)
    tone(SPEAKER, frequency, durationMs);
#else
    (void)frequency;
    (void)durationMs;
#endif
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }

    internalI2C.begin();

    if (!magnetometer.begin()) {
        Serial.println("LIS2MDL not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    magnetometer.setContinuous(50);

    Serial.println("Keep the board away from metal while baseline is calibrated...");
    baselineUt = averageMagnitude(100, 20);
    triggerDeltaUt = max(15.0f, baselineUt * 0.20f);

    Serial.print("Baseline: ");
    Serial.print(baselineUt, 2);
    Serial.print(" uT, trigger delta: ");
    Serial.print(triggerDeltaUt, 2);
    Serial.println(" uT");
}

void loop() {
    float magnitude = magnetometer.magnitudeUt();
    float delta = fabs(magnitude - baselineUt);
    bool detected = delta > triggerDeltaUt;

    Serial.print("Magnitude: ");
    Serial.print(magnitude, 2);
    Serial.print(" uT | delta: ");
    Serial.print(delta, 2);
    Serial.print(" uT | ");
    Serial.println(detected ? "METAL NEARBY" : "clear");

    if (detected) {
        uint16_t pitch = 1000 + static_cast<uint16_t>(min(delta * 15.0f, 2500.0f));
        beep(pitch, 80);
    }

    delay(100);
}
