// SPDX-License-Identifier: GPL-3.0-or-later
//
// WSEN-PADS + SSD1327 — display pressure, temperature and estimated
// relative humidity on the OLED screen, refreshed every second.

#include <Arduino.h>
#include <SPI.h>
#include <WSEN_PADS.h>
#include <Wire.h>
#include <math.h>

#include "SSD1327.h"

#define OLED_CS PD0

SPIClass oledSPI(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
SSD1327_SPI display(128, 128, oledSPI, SPI_INT_MISO, RST_DISPLAY, OLED_CS);
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-PADS not detected — check wiring.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setContinuous(ODR_1_HZ);
    oledSPI.begin();
    display.begin();
    display.fill(0);
    display.show();
}

void loop() {
    static uint32_t lastUpdate = 0;

    auto reading = sensor.read();
    float pressure = reading.pressure;
    float temperature = reading.temperature;
    float humidity = 100.0f *
                     (pressure - 6.11f * pow(10.0f, 7.5f * temperature / (237.7f + temperature))) /
                     pressure;

    display.fill(0);
    display.text("pressure:", 35, 30, 15);
    display.text(String(pressure, 2).c_str(), 35, 40, 15);
    display.text("temperature:", 35, 60, 15);
    display.text(String(temperature, 2).c_str(), 35, 70, 15);
    display.text("humidity:", 35, 90, 15);
    display.text(String(humidity, 2).c_str(), 35, 100, 15);
    display.show();
    delay(1000);
}