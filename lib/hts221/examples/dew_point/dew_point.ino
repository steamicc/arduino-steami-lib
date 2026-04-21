// SPDX-License-Identifier: GPL-3.0-or-later
//
// HTS221 — compute the dew point from temperature and humidity using the
// Magnus formula, and warn when the measured temperature comes within
// 2 degrees of it (visible condensation risk on cold surfaces).

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>
#include <math.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

// Magnus formula constants for water over a flat surface, good over the
// 0-60 C / 1-100 %RH range with <0.4 C error.
constexpr float MAGNUS_A = 17.62f;
constexpr float MAGNUS_B = 243.12f;

// Temperatures within this margin of the dew point are reported as a
// condensation risk.
constexpr float CONDENSATION_MARGIN_C = 2.0f;

float dewPoint(float temperatureC, float humidityPct) {
    // log(0) is -infinity and the Magnus inverse below would propagate
    // NaN / inf into the caller. Humidity reaches 0 at the driver's
    // clamp boundary, so guard explicitly.
    if (humidityPct <= 0.0f) {
        return NAN;
    }
    float gamma = (MAGNUS_A * temperatureC) / (MAGNUS_B + temperatureC) + log(humidityPct / 100.0f);
    return (MAGNUS_B * gamma) / (MAGNUS_A - gamma);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("HTS221 not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(HTS221_ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();
    float dp = dewPoint(reading.temperature, reading.humidity);

    Serial.print("T=");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  H=");
    Serial.print(reading.humidity, 1);
    Serial.print(" %  DewPoint=");
    Serial.print(dp, 2);
    Serial.print(" C");

    if (fabs(reading.temperature - dp) < CONDENSATION_MARGIN_C) {
        Serial.print("  -> condensation risk");
    }
    Serial.println();

    delay(1000);
}
