// SPDX-License-Identifier: GPL-3.0-or-later
// DigitalCompass.ino
// Flat compass using the LIS2MDL. Prints heading and cardinal direction to Serial.
// If the SSD1327 display library is available, it also displays the heading on OLED.

#include <Arduino.h>
#include <LIS2MDL.h>
#include <Wire.h>

#if __has_include(<SSD1327.h>)
#include <SSD1327.h>
#define HAS_STEAMI_OLED 1
#else
#define HAS_STEAMI_OLED 0
#endif

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL magnetometer(internalI2C);

#if HAS_STEAMI_OLED
// Adjust this constructor if your SSD1327 driver exposes a different STeaMi preset.
SSD1327 display;
#endif

static void showOnOled(float headingDeg, const char* direction) {
#if HAS_STEAMI_OLED
    display.fill(0);
    display.setCursor(0, 10);
    display.print("Compass");
    display.setCursor(0, 35);
    display.print(headingDeg, 1);
    display.print(" deg");
    display.setCursor(0, 60);
    display.print(direction);
    display.show();
#else
    (void)headingDeg;
    (void)direction;
#endif
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }

    internalI2C.begin();

#if HAS_STEAMI_OLED
    display.begin();
    display.fill(0);
    display.setCursor(0, 0);
    display.print("Starting compass...");
    display.show();
#endif

    if (!magnetometer.begin()) {
        Serial.println("LIS2MDL not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    magnetometer.setContinuous(20);
    magnetometer.setHeadingFilter(0.2f);

    delay(1000);

    Serial.println("Rotate the board flat in a full circle for calibration for 30 seconds...");
    magnetometer.calibrateMinmax2d(300, 20);
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

    showOnOled(heading, direction);
    delay(250);
}
