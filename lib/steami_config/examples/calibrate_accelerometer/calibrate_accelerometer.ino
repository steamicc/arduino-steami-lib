// SPDX-License-Identifier: GPL-3.0-or-later
//
// SteamiConfig — measure the ISM330DL accelerometer bias while the board lies
// flat, and save it in the DAPLink config zone. Output on the serial monitor
// (115200 baud).
//
// The offsets (ox, oy, oz) survive power cycles: the STeaMi Board library
// applies them to the accelerometer (key "ca"), in Arduino as in MicroPython.
// Same steps as calibrate_accelerometer.py in the MicroPython steami_config
// library.
//
// Place the board flat, screen up, and keep it still.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiConfig.h>
#include <SteamiDaplinkBridge.h>

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiConfig storedConfig(bridge);

// Loads the config zone; nullptr, after a message, when it cannot be used. The
// sketch reads the zone itself: Steami.config() stays empty when the release
// is forced.
SteamiConfig* openConfig() {
    if (!bridge.begin()) {
        Serial.println("DAPLink bridge not detected.");
        return nullptr;
    }
    if (!storedConfig.begin()) {
        Serial.println("No config zone: update the DAPLink firmware of the board.");
        return nullptr;
    }
    // A failed first read of the zone looks like an empty zone, and save() would then erase
    // the other settings: read an empty zone again before trusting it.
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            delay(200);
        }
        if (!storedConfig.load()) {
            Serial.println("The config zone does not hold valid JSON: nothing changed.");
            return nullptr;
        }
        if (!storedConfig.isEmpty()) {
            return &storedConfig;
        }
    }
    // Still empty: refuse when the board found settings in the zone at startup.
    const SteamiConfig* boardConfig = Steami.config();
    if (boardConfig != nullptr && !boardConfig->isEmpty()) {
        Serial.println(
            "The config zone reads empty, but the board read it at startup: "
            "nothing changed, reset the board and try again.");
        return nullptr;
    }
    Serial.println("The config zone is empty: a new one is written.");
    return &storedConfig;
}

const int SAMPLES = 100;
const uint32_t SAMPLE_DELAY_MS = 20;

// Prints "  ax = 0.012 g" and the same for y and z, `prefix` being 'a' or 'o'.
void printVector(char prefix, float x, float y, float z) {
    const char axes[] = {'x', 'y', 'z'};
    const float values[] = {x, y, z};
    for (int i = 0; i < 3; i++) {
        Serial.print("  ");
        Serial.print(prefix);
        Serial.print(axes[i]);
        Serial.print(" = ");
        Serial.print(values[i], 3);
        Serial.println(" g");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }

    if (!Steami.begin()) {
        Serial.println(
            "Unknown board release: call Steami.begin(SteamiRelease::V1C) (or V1B, V1A).");
        return;
    }
    SteamiConfig* config = openConfig();
    if (config == nullptr) {
        return;
    }
    SteamiISM330DL* imu = Steami.ism330dl();
    if (imu == nullptr) {
        Serial.println("The ISM330DL does not answer.");
        return;
    }

    Serial.println("=== Accelerometer Calibration ===\n");
    Serial.println("Place the board flat (screen up) and keep it still...");
    delay(2000);

    // --- Step 1: collect samples, without the stored offsets ---

    imu->setAccelOffset(0.0F, 0.0F, 0.0F);
    float sx = 0.0F;
    float sy = 0.0F;
    float sz = 0.0F;
    int valid = 0;
    for (int i = 0; i < SAMPLES; i++) {
        SteamiISM330DL::Vector3 a;
        if (imu->accelerationG(a)) {
            sx += a.x;
            sy += a.y;
            sz += a.z;
            valid++;
        }
        delay(SAMPLE_DELAY_MS);
    }
    if (valid == 0) {
        Serial.println("No reading from the accelerometer: nothing saved.");
        return;
    }
    const float ax = sx / valid;
    const float ay = sy / valid;
    const float az = sz / valid;

    // Flat, screen up, the accelerometer reads (0, 0, -1 g): the Z offset is
    // the measured value minus -1 g.
    const float ox = ax;
    const float oy = ay;
    const float oz = az + 1.0F;

    Serial.println("\nMeasured average:");
    printVector('a', ax, ay, az);
    Serial.println("\nComputed offsets:");
    printVector('o', ox, oy, oz);

    // --- Step 2: save to the config zone ---

    config->setAccelerometerCalibration(ox, oy, oz);
    if (!config->save()) {
        Serial.println("\nThe calibration does not fit in the config zone: nothing saved.");
        return;
    }
    Serial.println("\nCalibration saved to config zone.");

    // --- Step 3: verify after reload ---

    if (!config->load() || !config->applyAccelerometerCalibration(*imu)) {
        Serial.println("The calibration could not be read back.");
        return;
    }
    const SteamiISM330DL::Vector3 applied = imu->accelOffset();
    Serial.println("\nApplied offsets after reload:");
    printVector('o', applied.x, applied.y, applied.z);

    Serial.println("\nVerification (5 corrected readings):");
    for (int i = 0; i < 5; i++) {
        SteamiISM330DL::Vector3 a;
        if (imu->accelerationG(a)) {
            printVector('a', a.x, a.y, a.z);
        } else {
            Serial.println("  (no reading)");
        }
        delay(500);
    }

    Serial.println("\nDone! Calibration is stored and will be restored at next boot.");
}

void loop() {}
