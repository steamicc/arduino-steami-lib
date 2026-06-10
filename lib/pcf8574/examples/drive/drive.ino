// SPDX-License-Identifier: GPL-3.0-or-later
//
// Drive — test the PCF8574 driver by running a motor in one direction.

#include <PCF8574.h>
#include <Wire.h>

#define I2C3_SDA PC1
#define I2C3_SCL PC0

TwoWire internalI2C(PB9, PB8);
PCF8574 pcf8574_0(0x39, &internalI2C);

void setup() {
    Wire3.begin();
    pcf8574_0.begin();
    pinMode(ENA_ARD, OUTPUT);
}

void loop() {
    pcf8574_0.write(IN1_ARD, LOW);
    pcf8574_0.write(IN2_ARD, HIGH);
    analogWrite(ENA_ARD, 100);

    pcf8574_0.write(IN3_AVD, LOW);
    pcf8574_0.write(IN4_AVD, HIGH);
    analogWrite(ENB_AVD, 100);

    pcf8574_0.write(IN1_AVG, LOW);
    pcf8574_0.write(IN2_AVG, HIGH);
    analogWrite(ENA_AVG, 100);

    pcf8574_0.write(IN3_ARG, LOW);
    pcf8574_0.write(IN4_ARG, HIGH);
    analogWrite(ENB_ARG, 100);
}