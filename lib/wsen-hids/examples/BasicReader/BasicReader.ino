#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>

TwoWire internalWire(I2C_INT_SDA, I2C_INT_SCL);
WsenHids hids(internalWire, WsenHids::DEFAULT_ADDRESS);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("WSEN-HIDS BasicReader");
    Serial.println("---------------------");

    internalWire.begin();

    if (!hids.begin()) {
        Serial.println("Error: WSEN-HIDS not found.");
        while (true) {
            delay(1000);
        }
    }

    uint8_t id = hids.deviceId();
    Serial.print("Device ID: 0x");

    if (id < 0x10) {
        Serial.print("0");
    }

    Serial.println(id, HEX);
    Serial.println("Sensor initialized.");
    Serial.println();
}

void loop() {
    auto [hum, temp] = hids.readOneShot();

    Serial.print("Humidity: ");
    Serial.print(hum, 2);
    Serial.println(" %RH");

    Serial.print("Temperature: ");
    Serial.print(temp, 2);
    Serial.println(" C");

    delay(1000);
}
