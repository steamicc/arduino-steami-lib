// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

constexpr float seaLevelTemp = 288.15f;       // K
constexpr float lapseRate = 0.0065f;          // K/m
constexpr float seaLevelPressure = 1013.25f;  // hPa
constexpr float gravity = 9.80665f;           // m/s²
constexpr float dryAirGasConst = 287.058f;    // J/(kg·K)

float altitude(float pressureHpa) {
    return (seaLevelTemp / lapseRate) *
           (1 - powf(pressureHpa / seaLevelPressure, dryAirGasConst * lapseRate / gravity));
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
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

    auto reading = sensor.read(false);
    float altitudeResult = altitude(reading.pressure);

    Serial.print("Temperature= ");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  Pressure= ");
    Serial.print(reading.pressure, 2);
    Serial.print(" C  altitude Result= ");
    Serial.println(altitudeResult);

    // TO-DO : add the DAPLink parts
}