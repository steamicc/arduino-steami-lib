// SPDX-License-Identifier: GPL-3.0-or-later
//
// WeatherStation - low-noise continuous monitoring in normal mode.
//
// The chip is configured the way Bosch recommends for weather monitoring:
// oversampling x1 on every channel and IIR filter off. In normal mode the
// chip converts by itself (here once per second, then sleeps), so read()
// returns the latest values immediately without triggering anything. The
// pressure trend (rising / falling) compares each report with the previous
// one.
//
// The BME280 of the STeaMi V1c sits on the internal I2C bus. Flash and open
// the serial monitor at 115200 baud. Change REPORT_PERIOD_MS to report more
// or less often.

#include <Arduino.h>
#include <BME280.h>
#include <Wire.h>

constexpr uint32_t REPORT_PERIOD_MS = 10000;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BME280 sensor(internalI2C);

float lastPressure = NAN;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("BME280 not detected - check that this is a STeaMi V1c board.");
        while (true) {
            delay(1000);
        }
    }

    sensor.setOversampling(BME280_OSRS_X1, BME280_OSRS_X1, BME280_OSRS_X1);
    sensor.setIirFilter(BME280_FILTER_OFF);
    sensor.setContinuous(BME280_STANDBY_1000_MS);
    // Let the first conversion finish: the data registers are empty before.
    delay(sensor.measurementTimeMs() + 10);
    Serial.println("Weather station started.");
}

void loop() {
    auto reading = sensor.read();
    if (isnan(reading.temperature)) {
        Serial.println("BME280 read failed.");
        delay(REPORT_PERIOD_MS);
        return;
    }

    Serial.print("T = ");
    Serial.print(reading.temperature, 1);
    Serial.print(" C  |  H = ");
    Serial.print(reading.humidity, 0);
    Serial.print(" %  |  P = ");
    Serial.print(reading.pressure, 1);
    Serial.print(" hPa  |  trend: ");
    if (isnan(lastPressure)) {
        Serial.println("n/a");
    } else if (reading.pressure > lastPressure + 0.1f) {
        Serial.println("rising");
    } else if (reading.pressure < lastPressure - 0.1f) {
        Serial.println("falling");
    } else {
        Serial.println("steady");
    }
    lastPressure = reading.pressure;

    delay(REPORT_PERIOD_MS);
}
