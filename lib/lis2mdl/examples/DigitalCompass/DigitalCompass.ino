// SPDX-License-Identifier: GPL-3.0-or-later
// DigitalCompass.ino
// Flat compass using the LIS2MDL. Prints the heading and cardinal direction
// to Serial and shows them on the on-board SSD1327 OLED display.
//
// Board revisions: the SSD1327 OLED is only fitted on STeaMi V1a and V1b.
// V1c has a GC9A01 colour display instead, which this sketch does not
// drive: on V1c, read the heading on the serial monitor (115200 baud).

#include <Arduino.h>
#include <LIS2MDL.h>
#include <SSD1327.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL magnetometer(internalI2C);
WS_OLED_128X128_STEAMI display;

static bool hasDisplay = false;

static void showOnDisplay(float headingDeg, const char* direction) {
    if (!hasDisplay) {
        return;
    }

    // String(float, decimals) avoids printf("%f"), which newlib-nano
    // does not support by default.
    String line = String(headingDeg, 1) + " deg";

    display.fill(0);
    display.text("Compass", 36, 24, 15);
    display.text(line.c_str(), 28, 56, 12);
    display.text(direction, 56, 88, 15);
    display.show();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }

    internalI2C.begin();

    hasDisplay = display.begin();
    if (hasDisplay) {
        display.fill(0);
        display.text("Starting", 32, 48, 15);
        display.text("compass...", 24, 64, 15);
        display.show();
    } else {
        Serial.println("SSD1327 display not found, serial output only.");
    }

    if (!magnetometer.begin()) {
        Serial.println("LIS2MDL not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    magnetometer.setContinuous(50);
    magnetometer.setHeadingFilter(0.2f);

    delay(1000);

    Serial.println("Rotate the board flat in a full circle for calibration for 30 seconds...");
    if (hasDisplay) {
        display.fill(0);
        display.text("Calibrating", 20, 48, 15);
        display.text("rotate flat", 20, 64, 12);
        display.show();
    }
    magnetometer.calibrateMinmax2d(1000, 20);
    Serial.println("Calibration done.");

    Serial.println("Rotate the board flat in a full circle for best results.");
    Serial.println("heading_deg,direction");
}

void loop() {
    float heading = magnetometer.headingFlatOnly();
    const char* direction = magnetometer.directionLabel(heading);

    Serial.print(heading, 1);
    Serial.print(',');
    Serial.println(direction);

    showOnDisplay(heading, direction);
    delay(250);
}
