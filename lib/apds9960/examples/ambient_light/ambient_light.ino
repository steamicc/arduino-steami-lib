// SPDX-License-Identifier: GPL-3.0-or-later
//
// AmbientLight — read APDS9960 clear and RGB channels.
//
// Open the serial monitor at 115200 baud.

#include <APDS9960.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
APDS9960 sensor(internalI2C);

static const uint32_t PRINT_PERIOD_MS = 250;
uint32_t lastPrintMs = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("APDS9960 not detected.");
        while (true)
            delay(1000);
    }

    sensor.enableLightSensor(false);

    Serial.println("=======================");
    Serial.println(" Ambient light and RGB ");
    Serial.println("=======================");
}

void loop() {
    uint32_t now = millis();
    uint32_t elapsed = now - lastPrintMs;
    if (elapsed < PRINT_PERIOD_MS) {
        delay(PRINT_PERIOD_MS - elapsed);
        return;
    }

    lastPrintMs = now;

    uint16_t clear = 0;
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    bool ok = sensor.ambientLight(clear) && sensor.redLight(red) && sensor.greenLight(green) &&
              sensor.blueLight(blue);

    if (!ok) {
        Serial.println("Light read failed.");
        return;
    }

    Serial.print("C=");
    Serial.print(clear);
    Serial.print(" | R=");
    Serial.print(red);
    Serial.print(" | G=");
    Serial.print(green);
    Serial.print(" | B=");
    Serial.println(blue);
}
