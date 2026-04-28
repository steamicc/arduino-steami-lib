// SPDX-License-Identifier: GPL-3.0-or-later

// WSEN-PADS — derive an altitude estimate from barometric pressure using
// the international standard atmosphere model.
//
// The conversion uses a fixed reference sea-level pressure (1013.25 hPa).
// Real-world accuracy depends on local weather: a high-pressure system
// shifts the readout down by ~80 m, a low-pressure system up by the same.
// For absolute altitude on a given day, calibrate `seaLevelPressure` to a
// local METAR / weather-station QNH value before flashing.

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>
#include <math.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

// International Standard Atmosphere reference values.
constexpr float seaLevelPressure = 1013.25f;  // hPa
constexpr float seaLevelTemp = 288.15f;       // K (15 °C)
constexpr float lapseRate = 0.0065f;          // K/m
constexpr float gravity = 9.80665f;           // m/s²
constexpr float dryAirGasConst = 287.058f;    // J/(kg·K)
constexpr float exponent = (dryAirGasConst * lapseRate) / gravity;

float altitudeMeters(float pressureHpa) {
    return (seaLevelTemp / lapseRate) * (1.0f - powf(pressureHpa / seaLevelPressure, exponent));
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-PADS not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();
    float altitude = altitudeMeters(reading.pressure);

    Serial.print("P=");
    Serial.print(reading.pressure, 2);
    Serial.print(" hPa  alt=");
    Serial.print(altitude, 1);
    Serial.println(" m");

    delay(1000);
}
