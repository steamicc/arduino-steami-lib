// SPDX-License-Identifier: GPL-3.0-or-later

#include <Wire.h>
#include <WsenHids.h>
#include <WsenHids_const.h>
#include <math.h>

TwoWire internalWire(I2C_INT_SDA, I2C_INT_SCL);
WsenHids hids(internalWire, WsenHids::DEFAULT_ADDRESS);

float computeDewPoint(float temperature, float humidity) {
    constexpr float A = 17.27F;
    constexpr float B = 237.7F;

    float gamma = ((A * temperature) / (B + temperature)) + log(humidity / 100.0F);
    return (B * gamma) / (A - gamma);
}

void setup() {
    Serial.begin(115200);
    internalWire.begin();

    if (!hids.begin()) {
        Serial.println("WSEN-HIDS not detected");
        while (true) {
            delay(1000);
        }
    }

    hids.setContinuous(WsenHidsConst::ODR_1HZ);
    Serial.println("Dew point monitor started");
}

void loop() {
    if (hids.dataReady()) {
        float temperature = hids.temperature();
        float humidity = hids.humidity();
        float dewPoint = computeDewPoint(temperature, humidity);

        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" C | Humidity: ");
        Serial.print(humidity);
        Serial.print(" % | Dew point: ");
        Serial.print(dewPoint);
        Serial.println(" C");

        if ((temperature - dewPoint) < 2.0F) {
            Serial.println("Condensation risk is high");
            tone(SPEAKER, 2000, 150);
        }
    }

    delay(500);
}
