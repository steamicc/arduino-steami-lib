#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>
#include <math.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 sensor(internalI2C);

void blink() {
    digitalWrite(LED_RED, HIGH);
    delay(500);
    digitalWrite(LED_RED, LOW);
    delay(500);
}

void buzzAlert() {
    tone(SPEAKER, 1500, 200);
    delay(300);
    tone(SPEAKER, 1500, 200);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        while (true) {
            delay(1000);
        }
    }

    pinMode(LED_RED, OUTPUT);
    pinMode(SPEAKER, OUTPUT);
}

void loop() {
    if (sensor.stateOfCharge() <= 20) {
        blink();
        delay(200);
    }
    if (sensor.stateOfCharge() <= 5) {
        buzzAlert();
        delay(1000);
    }
    else {
        delay(1000);
    }
}