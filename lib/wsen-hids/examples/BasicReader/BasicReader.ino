// SPDX-License-Identifier: GPL-3.0-or-later
//
// WSEN-HIDS — read humidity and temperature at 1 Hz and print them over Serial.
//
// Wiring on the STeaMi board: the WSEN-HIDS sits on the internal I2C bus, no
// external hookup needed. Just flash this example and open the serial
// monitor at 115200 baud.

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>

// The WSEN-HIDS hangs off the STeaMi internal I2C bus, not the default
// global Wire. Spin up a dedicated TwoWire pointed at the variant pin
// macros and hand it to the driver.
TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("WSEN-HIDS not detected — check wiring and I2C address.");
        while (true) {
            delay(1000);
        }
    }

    Serial.print("WSEN-HIDS detected (WHO_AM_I = 0x");
    Serial.print(sensor.deviceId(), HEX);
    Serial.println(")");

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
}

void loop() {
    if (!sensor.dataReady()) {
        delay(10);
        return;
    }

    auto reading = sensor.read();

    Serial.print("T = ");
    Serial.print(reading.temperature, 2);
    Serial.print(" C  |  H = ");
    Serial.print(reading.humidity, 1);
    Serial.println(" %");

    delay(1000);
}
