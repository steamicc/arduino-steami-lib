// SPDX-License-Identifier: GPL-3.0-or-later
//
// Gesture — detect directional gestures with the APDS9960.
//
// Swipe a hand over the sensor.
// Open the serial monitor at 115200 baud.

#include <SteamiAPDS9960.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiAPDS9960 sensor(internalI2C);

const char* gestureName(SteamiAPDS9960::Gesture gesture) {
    switch (gesture) {
        case SteamiAPDS9960::Gesture::LEFT:
            return "LEFT";
        case SteamiAPDS9960::Gesture::RIGHT:
            return "RIGHT";
        case SteamiAPDS9960::Gesture::UP:
            return "UP";
        case SteamiAPDS9960::Gesture::DOWN:
            return "DOWN";
        case SteamiAPDS9960::Gesture::NEAR:
            return "NEAR";
        case SteamiAPDS9960::Gesture::FAR:
            return "FAR";
        default:
            return "NONE";
    }
}

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

    sensor.enableGestureSensor(false);

    Serial.println("=======================");
    Serial.println("    Gesture detector   ");
    Serial.println("=======================");
    Serial.println("Swipe over the sensor.");
}

void loop() {
    if (!sensor.gestureAvailable()) {
        delay(10);
        return;
    }

    SteamiAPDS9960::Gesture gesture = sensor.readGesture();

    // Ignore FIFO activity that does not decode into a valid gesture.
    if (gesture == SteamiAPDS9960::Gesture::NONE)
        return;

    Serial.print("Gesture: ");
    Serial.println(gestureName(gesture));
}
