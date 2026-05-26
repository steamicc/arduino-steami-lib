// SPDX-License-Identifier: GPL-3.0-or-later

// WSEN-PADS — detect floor changes inside a building from barometric
// pressure. Calibrates a reference altitude over the first few seconds
// (treated as floor 0), then reports the current floor as the carrier
// moves. A simple hysteresis margin avoids bouncing between two floors
// when the reading sits right on a boundary.
//
// Tune `floorHeight` to your building. Typical residential floors are
// 2.7–3.0 m; offices closer to 3.5–4.0 m.

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>
#include <math.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

constexpr float seaLevelPressure = 1013.25f;
constexpr float seaLevelTemp = 288.15f;
constexpr float lapseRate = 0.0065f;
constexpr float gravity = 9.80665f;
constexpr float dryAirGasConst = 287.058f;
constexpr float exponent = (dryAirGasConst * lapseRate) / gravity;

constexpr int CALIBRATION_SAMPLES = 10;
constexpr float floorHeight = 2.8f;  // metres
constexpr float hysteresis = 0.3f;   // metres of slack at each floor boundary

float referenceAltitude = 0.0f;
int currentFloor = 0;

float altitudeMeters(float pressureHpa) {
    return (seaLevelTemp / lapseRate) * (1.0f - powf(pressureHpa / seaLevelPressure, exponent));
}

float averageAltitude(int samples) {
    float sum = 0.0f;
    int collected = 0;
    while (collected < samples) {
        if (sensor.dataReady()) {
            sum += altitudeMeters(sensor.read().pressure);
            ++collected;
        }
        delay(10);
    }
    return sum / static_cast<float>(samples);
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

    Serial.println("Calibrating reference altitude (floor 0)...");
    referenceAltitude = averageAltitude(CALIBRATION_SAMPLES);
    Serial.print("Reference altitude: ");
    Serial.print(referenceAltitude, 1);
    Serial.println(" m");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    float altitude = altitudeMeters(sensor.read().pressure);
    float deltaFromBase = altitude - referenceAltitude;

    // Compute the candidate floor index. The hysteresis margin makes the
    // boundary between floor N and floor N+1 fuzzy, so a reading that
    // sits right on the threshold doesn't oscillate.
    float adjusted = deltaFromBase;
    if (deltaFromBase > currentFloor * floorHeight) {
        adjusted -= hysteresis;
    } else if (deltaFromBase < currentFloor * floorHeight) {
        adjusted += hysteresis;
    }
    int candidate = static_cast<int>(roundf(adjusted / floorHeight));

    if (candidate != currentFloor) {
        currentFloor = candidate;
        Serial.print("Floor: ");
        Serial.print(currentFloor);
        Serial.print("  (Δ=");
        Serial.print(deltaFromBase, 1);
        Serial.println(" m)");
    }

    delay(500);
}
