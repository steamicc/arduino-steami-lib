// SPDX-License-Identifier: GPL-3.0-or-later
// BasicReader.ino
// Read LIS2MDL magnetic field XYZ and temperature, then print to Serial.

#include <Arduino.h>
#include <LIS2MDL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL magnetometer(internalI2C);

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

    magnetometer.setContinuous(10);

    Serial.println("LIS2MDL Basic Reader");
    Serial.println("X_uT,Y_uT,Z_uT,temperature_C,magnitude_uT");
}

void loop() {
    if (!magnetometer.dataReady()) {
        delay(10);
        return;
    }

    MagneticFieldUt field = magnetometer.magneticFieldUt();
    float temperatureC = magnetometer.temperature();
    float magnitude = magnetometer.magnitudeUt();

    Serial.print(field.x, 2);
    Serial.print(',');
    Serial.print(field.y, 2);
    Serial.print(',');
    Serial.print(field.z, 2);
    Serial.print(',');
    Serial.print(temperatureC, 2);
    Serial.print(',');
    Serial.println(magnitude, 2);

    delay(100);
}
