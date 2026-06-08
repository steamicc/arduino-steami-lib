// SPDX-License-Identifier: GPL-3.0-or-later
//
// LIS2MDL — lecture brute du champ magnétique sur les 3 axes,
// température interne et magnitude totale en µT.
// Point de départ pour tout projet boussole ou détection magnétique.

#include <LIS2MDL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("LIS2MDL not detected");
        while (true) delay(1000);
    }

    sensor.setContinuous(10);
}

void loop() {
    if (sensor.dataReady()) {
        MagneticField f = sensor.magneticField();
        float temperature = sensor.temperature();
        float magnitudeUt = sensor.magnitudeUt();
        Serial.print("Magnetic field x: ");
        Serial.print(f.x);
        Serial.print(" Magnetic field y: ");
        Serial.print(f.y);
        Serial.print(" Magnetic field z: ");
        Serial.print(f.z);
        Serial.print(" Temperature: ");
        Serial.print(temperature);
        Serial.print(" Magnitude µT: ");
        Serial.println(magnitudeUt);
        delay(1000);
    }
    delay(1000);
}