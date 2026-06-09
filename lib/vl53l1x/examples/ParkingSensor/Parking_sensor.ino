#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

void buzzAlert(uint16_t waitTime) {
    tone(SPEAKER, 1500, 200);
    delay(waitTime);
    tone(SPEAKER, 1500, 200);
    delay(waitTime);
}

void setup() {
  Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t distance = sensor.read();

        if (distance <= 300 && distance > 200) {
            buzzAlert(700);
        }
        else if (distance <= 200 && distance > 100) {
            buzzAlert(500);
        }
        else if (distance <= 100 && distance > 50) {
            buzzAlert(300);
        }
        else if (distance <= 50) {
            buzzAlert(100);
        }
    }

    delay(50);
}