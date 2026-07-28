// SPDX-License-Identifier: GPL-3.0-or-later

#include <ISM330DL.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL imu(internalI2C);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();

    if (!imu.begin()) {
        Serial.println("ISM330DL not detected.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("ISM330DL ready.");
}

void loop() {
    ISM330DL::Vector3 accel;
    ISM330DL::Vector3 gyro;
    float temperature;

    if (imu.accelerationG(accel) && imu.gyroscopeDps(gyro) && imu.temperature(temperature)) {
        Serial.println("Acceleration (g)");
        Serial.print("X: ");
        Serial.println(accel.x, 3);
        Serial.print("Y: ");
        Serial.println(accel.y, 3);
        Serial.print("Z: ");
        Serial.println(accel.z, 3);

        Serial.println();

        Serial.println("Gyroscope (dps)");
        Serial.print("X: ");
        Serial.println(gyro.x, 2);
        Serial.print("Y: ");
        Serial.println(gyro.y, 2);
        Serial.print("Z: ");
        Serial.println(gyro.z, 2);

        Serial.println();

        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.println(" °C");

        Serial.println();
    }

    delay(300);
}
