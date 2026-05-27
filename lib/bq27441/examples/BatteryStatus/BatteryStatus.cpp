#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>
#include <math.h>

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
        }
    }
}

void loop(){
    Serial.print("Voltage: ");
    Serial.print(sensor.voltageMv());
    Serial.print("mV \n");
    Serial.print("State of Charge: ");
    Serial.print(sensor.stateOfCharge());
    Serial.print("% \n");
    Serial.print("Remaining Capacity: ");
    Serial.print(sensor.capacityRemaining());
    Serial.print("mAh \n");
    Serial.print("State of Health: ");
    Serial.print(sensor.stateOfHealth());
    Serial.println("% \n");
    delay(3000);
}