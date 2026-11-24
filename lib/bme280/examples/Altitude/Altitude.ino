// SPDX-License-Identifier: GPL-3.0-or-later
//
// Altitude - estimate the altitude from the air pressure.
//
// Two numbers are printed every second:
//   * the altitude above sea level, from the international barometric
//     formula. It is only as good as SEA_LEVEL_HPA: put the pressure of the
//     nearest weather station (reduced to sea level) for a good estimate.
//   * the altitude relative to where the board was switched on, which does
//     not depend on SEA_LEVEL_HPA. Carry the board up a staircase and watch
//     it change (about 0.1 hPa per meter near sea level).
//
// The BME280 of the STeaMi V1c sits on the internal I2C bus. Flash and open
// the serial monitor at 115200 baud.

#include <Arduino.h>
#include <BME280.h>
#include <Wire.h>

// Local sea-level pressure in hPa. 1013.25 is the standard atmosphere.
constexpr float SEA_LEVEL_HPA = 1013.25f;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BME280 sensor(internalI2C);

float startAltitude = 0.0f;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("BME280 not detected - check that this is a STeaMi V1c board.");
        while (true) {
            delay(1000);
        }
    }

    // Oversample the pressure and smooth it: altitude is very sensitive to
    // pressure noise.
    sensor.setOversampling(BME280_OSRS_X1, BME280_OSRS_X16, BME280_OSRS_X1);
    sensor.setIirFilter(BME280_FILTER_16);
    sensor.setContinuous(BME280_STANDBY_62_5_MS);
    delay(500);

    startAltitude = sensor.altitude(SEA_LEVEL_HPA);
    Serial.println("Altitude meter started.");
}

void loop() {
    float pressure = sensor.pressureHpa();

    if (isnan(pressure)) {
        Serial.println("BME280 read failed.");
    } else {
        float altitude = BME280::altitudeFromPressure(pressure, SEA_LEVEL_HPA);
        Serial.print("P = ");
        Serial.print(pressure, 2);
        Serial.print(" hPa  |  altitude = ");
        Serial.print(altitude, 1);
        Serial.print(" m  |  since start = ");
        Serial.print(altitude - startAltitude, 1);
        Serial.println(" m");
    }

    delay(1000);
}
