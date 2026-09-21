// SPDX-License-Identifier: GPL-3.0-or-later
//
// BatteryStatus — print voltage, state of charge, remaining capacity
// and state of health to the serial monitor every 3 seconds.
//
// The BQ27441 sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see the live readings.

#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        while (true) {
            delay(1000);
            Serial.println("Failed to initialize BQ27441! Check your wiring.");
        }
    }
}

void loop() {
    Serial.print("Voltage: ");
    Serial.print(sensor.voltageMv());
    Serial.println("mV\n");
    Serial.print("State of Charge: ");
    Serial.print(sensor.stateOfCharge());
    Serial.print("%\n");
    Serial.print("Remaining Capacity: ");
    Serial.print(sensor.capacityRemaining());
    Serial.print("mAh\n");
    Serial.print("State of Health: ");
    Serial.print(sensor.stateOfHealth());
    Serial.println("%\n");
    delay(3000);
}