// SPDX-License-Identifier: GPL-3.0-or-later

#include <Wire.h>
#include <WsenHids.h>
#include <WsenHids_const.h>

TwoWire internalWire(I2C_INT_SDA, I2C_INT_SCL);
WsenHids hids(internalWire, WsenHids::DEFAULT_ADDRESS);

constexpr float HUMIDITY_THRESHOLD = 70.0F;

void buzzAlert() {
    tone(SPEAKER, 1500, 200);
    delay(300);
    tone(SPEAKER, 1500, 200);
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
    Serial.println("Humidity alert started");
}

void loop() {
    if (hids.dataReady()) {
        float temperature = hids.temperature();
        float humidity = hids.humidity();

        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" C | Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");

        if (humidity > HUMIDITY_THRESHOLD) {
            Serial.println("Warning: humidity too high (mold risk)");
            buzzAlert();
        }
    }

    delay(250);
}
