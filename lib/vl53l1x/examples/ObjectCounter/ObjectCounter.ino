// SPDX-License-Identifier: GPL-3.0-or-later
// ObjectCounter.ino
// Count the objects passing in front of the sensor, like a break-beam counter.
// Keep the field of view clear during the first second: the sketch measures the
// background distance, then counts each passage that comes clearly closer.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiVL53L1X.h>

constexpr uint8_t CALIBRATION_SAMPLES = 20;
constexpr uint16_t ENTER_MARGIN_MM = 50;  // an object is this much closer than the background
constexpr uint16_t LEAVE_MARGIN_MM = 30;  // smaller margin to leave: avoids double counts

SteamiVL53L1X sensor(Steami.wire());  // the board's internal I2C bus

static uint16_t backgroundMm = 0;
static uint32_t counter = 0;
static bool objectPresent = false;

// Average of the valid readings (0 means "no measurement").
static uint16_t measureBackground() {
    uint32_t sum = 0;
    uint8_t valid = 0;
    for (uint8_t i = 0; i < CALIBRATION_SAMPLES; ++i) {
        uint16_t distance = sensor.distanceMm();
        if (distance > 0) {
            sum += distance;
            ++valid;
        }
        delay(50);
    }
    return valid > 0 ? static_cast<uint16_t>(sum / valid) : 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the USB serial port so the messages below are not lost.
    }

    if (!sensor.begin()) {
        Serial.println("VL53L1X not detected. Check the internal I2C bus.");
        while (true) {
            delay(1000);
        }
    }

    sensor.startRanging();
    backgroundMm = measureBackground();
    if (backgroundMm <= ENTER_MARGIN_MM) {
        Serial.println("Background too close or not measured: clear the field of view and reset.");
        while (true) {
            delay(1000);
        }
    }

    Serial.print("Background: ");
    Serial.print(backgroundMm);
    Serial.println(" mm. Counting objects...");
}

void loop() {
    if (!sensor.dataReady()) {
        delay(20);
        return;
    }

    uint16_t distance = sensor.distanceMm();
    if (distance == 0) {
        return;
    }

    if (!objectPresent && distance < backgroundMm - ENTER_MARGIN_MM) {
        objectPresent = true;
        ++counter;
        Serial.print("Object detected! Count: ");
        Serial.println(counter);
    } else if (objectPresent && distance > backgroundMm - LEAVE_MARGIN_MM) {
        objectPresent = false;
    }

    delay(20);
}
