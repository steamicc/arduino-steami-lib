// SPDX-License-Identifier: GPL-3.0-or-later
//
// SteamiConfig — load the configuration stored in the DAPLink config zone
// and print it to the serial monitor (115200 baud). The sketch only reads:
// nothing is written to the board.
//
// The DAPLink bridge sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// bridge driver.

#include <Arduino.h>
#include <SteamiConfig.h>
#include <SteamiDaplinkBridge.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiDaplinkBridge bridge(internalI2C);
SteamiConfig config(bridge);

const char* const TEMPERATURE_SENSORS[] = {
    STEAMI_CONFIG_SENSOR_HTS221,    STEAMI_CONFIG_SENSOR_LIS2MDL,   STEAMI_CONFIG_SENSOR_ISM330DL,
    STEAMI_CONFIG_SENSOR_WSEN_HIDS, STEAMI_CONFIG_SENSOR_WSEN_PADS,
};

void printNotSet(const char* label) {
    Serial.print(label);
    Serial.println("(not set)");
}

// Prints a revision code the way the STeaMi Board library reads it: hundreds
// are the generation, tens the release (1 = a, 2 = b, 3 = c), units the
// schematic index (0 = prototype). Done inline so this sketch does not need
// the SteamiBoard library.
void printRevision(int32_t rev) {
    Serial.print(rev);
    const int32_t generation = rev / 100;
    const int32_t release = (rev / 10) % 10;
    if (rev < 100 || rev > 999 || generation != 1 || release < 1 || release > 3) {
        Serial.println(" (unknown code)");
        return;
    }
    Serial.print(" (V1");
    Serial.print(static_cast<char>('a' + release - 1));
    if (rev % 10 == 0) {
        Serial.println(", prototype)");
    } else {
        Serial.print(", index ");
        Serial.print(rev % 10);
        Serial.println(")");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }
    internalI2C.begin();

    if (!bridge.begin()) {
        Serial.println("DAPLink bridge not detected.");
        return;
    }
    if (!config.begin()) {
        Serial.println("This board's DAPLink firmware has no config zone: update it.");
        return;
    }
    if (!config.load()) {
        Serial.println("The config zone does not hold valid JSON.");
        return;
    }

    int32_t revision = 0;
    if (config.boardRevision(revision)) {
        Serial.print("Board revision: ");
        printRevision(revision);
    } else {
        printNotSet("Board revision: ");
    }

    String name;
    if (config.boardName(name)) {
        Serial.print("Board name:     ");
        Serial.println(name);
    } else {
        printNotSet("Board name:     ");
    }

    uint32_t bootCount = 0;
    if (config.bootCount(bootCount)) {
        Serial.print("Boot count:     ");
        Serial.println(bootCount);
    } else {
        printNotSet("Boot count:     ");
    }

    Serial.println("Temperature calibration (gain / offset):");
    for (const char* sensor : TEMPERATURE_SENSORS) {
        Serial.print("  ");
        Serial.print(sensor);
        Serial.print(": ");
        TemperatureCalibration calibration;
        if (config.getTemperatureCalibration(sensor, calibration)) {
            Serial.print(calibration.gain, 4);
            Serial.print(" / ");
            Serial.println(calibration.offset, 4);
        } else {
            Serial.println("(not set)");
        }
    }

    MagnetometerCalibration magnetometer;
    if (config.getMagnetometerCalibration(magnetometer)) {
        Serial.print("Magnetometer hard iron: ");
        Serial.print(magnetometer.hardIronX);
        Serial.print(", ");
        Serial.print(magnetometer.hardIronY);
        Serial.print(", ");
        Serial.println(magnetometer.hardIronZ);
        Serial.print("Magnetometer soft iron: ");
        Serial.print(magnetometer.softIronX);
        Serial.print(", ");
        Serial.print(magnetometer.softIronY);
        Serial.print(", ");
        Serial.println(magnetometer.softIronZ);
    } else {
        printNotSet("Magnetometer calibration: ");
    }

    AccelerometerCalibration accelerometer;
    if (config.getAccelerometerCalibration(accelerometer)) {
        Serial.print("Accelerometer offsets: ");
        Serial.print(accelerometer.offsetX, 4);
        Serial.print(", ");
        Serial.print(accelerometer.offsetY, 4);
        Serial.print(", ");
        Serial.println(accelerometer.offsetZ, 4);
    } else {
        printNotSet("Accelerometer calibration: ");
    }
}

void loop() {}
