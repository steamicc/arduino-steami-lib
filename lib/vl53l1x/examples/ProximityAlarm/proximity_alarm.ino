#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

float distanceMax = 0.0f;

void buzzAlert() {
    tone(SPEAKER, 1500, 200);
    delay(300);
    tone(SPEAKER, 1500, 200);
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

    Serial.println("Entrez la distance maximale (mm) :");
    while (Serial.available() == 0);
    distanceMax = Serial.parseFloat();
    Serial.print("Distance max configurée : ");
    Serial.print(distanceMax);
    Serial.println(" mm");
}

void loop() {
    if (sensor.dataReady()) {
        uint16_t newDistance = sensor.read();

        if (newDistance < distanceMax) {
            buzzAlert();
        }
        delay(100);
    }

    delay(50);
}