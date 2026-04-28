// SPDX-License-Identifier: GPL-3.0-or-later

#include <Wire.h>
#include <WsenHids.h>
#include <WsenHids_const.h>

TwoWire internalWire(I2C_INT_SDA, I2C_INT_SCL);
WsenHids hids(internalWire, WsenHids::DEFAULT_ADDRESS);

const char* classifyComfort(float temperature, float humidity) {
    if (temperature < 18.0F) {
        return "Too cold";
    }

    if (temperature > 26.0F) {
        return "Too hot";
    }

    if (humidity < 30.0F) {
        return "Too dry";
    }

    if (humidity > 65.0F) {
        return "Too humid";
    }

    return "Comfortable";
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
    Serial.println("Comfort zone monitor started");
}

void loop() {
    if (hids.dataReady()) {
        float temperature = hids.temperature();
        float humidity = hids.humidity();

        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" C | Humidity: ");
        Serial.print(humidity);
        Serial.print(" % -> ");
        Serial.println(classifyComfort(temperature, humidity));
    }

    delay(500);
}
