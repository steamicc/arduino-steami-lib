// SPDX-License-Identifier: GPL-3.0-or-later
// MagneticCalibration.ino
// Guided hard-iron calibration. Rotate the board in all directions.
// The computed offsets/scales are printed and can be stored in steami_config later.

#include <Arduino.h>
#include <LIS2MDL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL magnetometer(internalI2C);

static void printCalibration() {
    Serial.println("Calibration values:");
    Serial.print("xOff=");
    Serial.println(magnetometer.xOff, 6);
    Serial.print("yOff=");
    Serial.println(magnetometer.yOff, 6);
    Serial.print("zOff=");
    Serial.println(magnetometer.zOff, 6);
    Serial.print("xScale=");
    Serial.println(magnetometer.xScale, 6);
    Serial.print("yScale=");
    Serial.println(magnetometer.yScale, 6);
    Serial.print("zScale=");
    Serial.println(magnetometer.zScale, 6);
}

static void printQuality() {
    CalibrationQuality quality = magnetometer.calibrateQuality(200, 20);
    Serial.println("Calibration quality:");
    Serial.print("center=(");
    Serial.print(quality.meanX, 3);
    Serial.print(", ");
    Serial.print(quality.meanY, 3);
    Serial.print(", ");
    Serial.print(quality.meanZ, 3);
    Serial.println(")");
    Serial.print("XY radius mean=");
    Serial.print(quality.rMeanXY, 3);
    Serial.print(" std=");
    Serial.print(quality.rStdXY, 3);
    Serial.print(" anisotropy=");
    Serial.println(quality.anisotropyXY, 3);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {
    }

    internalI2C.begin();

    if (!magnetometer.begin()) {
        Serial.println("LIS2MDL not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    magnetometer.setContinuous(50);

    Serial.println("Hard-iron calibration");
    Serial.println("Rotate the board slowly in every direction until the countdown ends.");
    for (int second = 5; second > 0; --second) {
        Serial.print("Starting in ");
        Serial.println(second);
        delay(1000);
    }

    Serial.println("Calibrating for about 12 seconds...");
    magnetometer.calibrateMinmax3d(600, 20);
    Serial.println("Done.");

    printCalibration();
    printQuality();

    Serial.println();
    Serial.println("TODO steami_config: save these six values in the board configuration zone.");
    Serial.println(
        "Paste them into your sketch or persist them once the steami_config API is available.");
}

void loop() {
    float heading = magnetometer.headingFlatOnly();
    MagneticFieldUt field = magnetometer.magneticFieldUt();

    Serial.print("heading=");
    Serial.print(heading, 1);
    Serial.print(" deg direction=");
    Serial.print(magnetometer.directionLabel(heading));
    Serial.print(" field_uT=(");
    Serial.print(field.x, 1);
    Serial.print(", ");
    Serial.print(field.y, 1);
    Serial.print(", ");
    Serial.print(field.z, 1);
    Serial.println(")");

    delay(500);
}
