// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleBeacon — advertise the board name and a live temperature reading.
//
// The HTS221 temperature is encoded as int16 x100 in the manufacturer-
// specific data field (company ID 0x0059). The beacon is visible from
// any BLE scanner app (nRF Connect, LightBlue, etc.).
//
// Wiring: no external hookup needed. The HTS221 sits on the STeaMi
// internal I2C bus. Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <HTS221.h>
#include <STM32duinoBLE.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    // Init sensor
    internalI2C.begin();
    if (!sensor.begin()) {
        Serial.println("HTS221 not detected — check wiring.");
        while (true)
            ;
    }
    sensor.setContinuous(HTS221_ODR_1_HZ);
    Serial.println("HTS221 ready.");

    // Init BLE
    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    BLE.setLocalName("STeaMi-Beacon");
    BLE.setDeviceName("STeaMi-Beacon");
    BLE.setAdvertisingInterval(160);  // 100 ms
    BLE.setConnectable(false);

    Serial.println("BLE Beacon ready.");
}

void loop() {
    // Read temperature and encode as int16 x100
    float temp = sensor.temperature();
    int16_t tempRaw = (int16_t)(temp * 100);
    uint8_t data[2] = {
        (uint8_t)(tempRaw >> 8),    // MSB
        (uint8_t)(tempRaw & 0xFF),  // LSB
    };

    // Update manufacturer data and re-advertise
    BLE.stopAdvertise();
    BLE.setManufacturerData(0x0059, data, sizeof(data));
    BLE.advertise();

    Serial.print("Advertising — T = ");
    Serial.print(temp, 2);
    Serial.println(" C");

    BLE.poll();
    delay(1000);
}
