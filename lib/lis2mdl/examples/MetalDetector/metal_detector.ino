// SPDX-License-Identifier: GPL-3.0-or-later
//
// LIS2MDL — détection d'objet métallique par perturbation du champ ambiant.
// Une baseline de magnitude est calculée au démarrage sur 20 mesures.
// Si la magnitude courante s'écarte de plus de 30 µT, le buzzer est déclenché.
// Utile comme point de départ pour un détecteur de métaux simple.

#include <LIS2MDL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL sensor(internalI2C);

float magnitudeMoy = 0.0f;

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
        Serial.println("LIS2MDL not detected");
        while (true)
            delay(1000);
    }

    sensor.setContinuous(10);
    pinMode(SPEAKER, OUTPUT);

    for (int i = 0; i < 20; i++) {
        magnitudeMoy += sensor.magnitudeUt();
        delay(100);
    }
    magnitudeMoy /= 20;
    Serial.print("Magnitude average: ");
    Serial.print(magnitudeMoy);
    Serial.println(" µT");
}

void loop() {
    if (sensor.dataReady()) {
        float newMagnitude = sensor.magnitudeUt();
        if (abs(newMagnitude - magnitudeMoy) > 30.0f) {
            buzzAlert();
        }
        delay(500);
    }
}