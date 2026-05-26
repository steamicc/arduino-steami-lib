// SPDX-License-Identifier: GPL-3.0-or-later
//
// DewPointMonitor — compute the dew point from temperature and humidity,
// and warn when condensation risk becomes high (T − Td < 2 °C).
//
// Uses the Magnus-Tetens approximation:
//   γ(T, RH) = (a·T) / (b + T) + ln(RH / 100)
//   Td      = (b·γ) / (a − γ)        with a = 17.27, b = 237.7 (°C)
//
// The WSEN-HIDS sits on the STeaMi internal I2C bus. Flash and open the
// serial monitor at 115200 baud.

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>
#include <math.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

float computeDewPoint(float temperature, float humidity) {
    constexpr float A = 17.27f;
    constexpr float B = 237.7f;
    constexpr float MIN_HUMIDITY = 0.001f;

    // logf() argument must be strictly positive — clamp dry-sensor readings.
    float safeHumidity = fmaxf(humidity, MIN_HUMIDITY);

    float gamma = ((A * temperature) / (B + temperature)) + logf(safeHumidity / 100.0f);
    return (B * gamma) / (A - gamma);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-HIDS not detected — check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    Serial.println("Dew point monitor started.");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();
    float dewPoint = computeDewPoint(reading.temperature, reading.humidity);

    Serial.print("T = ");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  |  H = ");
    Serial.print(reading.humidity, 1);
    Serial.print(" %  |  Td = ");
    Serial.print(dewPoint, 2);
    Serial.println(" C");

    if ((reading.temperature - dewPoint) < 2.0f) {
        Serial.println("Condensation risk is high.");
        tone(SPEAKER, 2000, 150);
    }

    // Wait for the next 1 Hz sample. The 10 ms poll above only kicks in
    // for the last fraction of a second before the next dataReady flag.
    delay(1000);
}
