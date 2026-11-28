// SPDX-License-Identifier: GPL-3.0-or-later
//
// SteamiConfig — calibrate the LIS2MDL magnetometer (hard iron and soft iron)
// and save the result in the DAPLink config zone. Output on the serial
// monitor (115200 baud).
//
// The calibration survives power cycles: the STeaMi Board library applies it
// to the magnetometer and the compass (key "cm"), in Arduino as in
// MicroPython. Same steps as calibrate_magnetometer.py in the MicroPython
// steami_config library, with the instructions on the serial monitor
// instead of the screen.
//
// Press MENU, then turn the board slowly in every direction for 12 s.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
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
    if (!storedConfig.load()) {
        Serial.println("The config zone does not hold valid JSON: nothing changed.");
        return nullptr;
    }
    return &storedConfig;
}

const uint16_t SAMPLES = 600;  // 12 s at one sample every 20 ms
const uint16_t SAMPLE_DELAY_MS = 20;

SteamiButtons buttons;

void printTriple(const char* label, float x, float y, float z) {
    Serial.print(label);
    Serial.print(x, 2);
    Serial.print(", ");
    Serial.print(y, 2);
    Serial.print(", ");
    Serial.println(z, 2);
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
    SteamiLIS2MDL* mag = Steami.lis2mdl();
    if (mag == nullptr) {
        Serial.println("The LIS2MDL does not answer.");
        return;
    }
    buttons.begin();

    // --- Step 1: instructions ---

    Serial.println("=== Magnetometer Calibration ===\n");
    Serial.println("Press MENU to start, then turn the board in every direction.");
    while (buttons.waitForPress() != SteamiButton::MENU) {
        // Another button: keep waiting for MENU.
    }

    // --- Step 2: acquisition, from a neutral calibration ---

    Serial.println("Acquisition for 12 s: keep turning the board...");
    mag->calibrateReset();
    mag->calibrateMinmax3d(SAMPLES, SAMPLE_DELAY_MS);
    Serial.println("Calibration complete!");
    printTriple("  hard iron (LSB): ", mag->xOff, mag->yOff, mag->zOff);
    printTriple("  soft iron (LSB): ", mag->xScale, mag->yScale, mag->zScale);

    // --- Step 3: save ---

    config->setMagnetometerCalibration(mag->xOff, mag->yOff, mag->zOff, mag->xScale, mag->yScale,
                                       mag->zScale);
    if (!config->save()) {
        Serial.println("\nThe calibration does not fit in the config zone: nothing saved.");
        return;
    }
    Serial.println("\nCalibration saved to config zone.");

    // --- Step 4: verify after reload ---

    MagnetometerCalibration cal;
    if (!config->load() || !config->getMagnetometerCalibration(cal)) {
        Serial.println("The calibration could not be read back.");
        return;
    }
    mag->setCalibrateStep(cal.hardIronX, cal.hardIronY, cal.hardIronZ, cal.softIronX, cal.softIronY,
                          cal.softIronZ);

    Serial.println("\nVerification (5 readings, board flat):");
    for (int i = 0; i < 5; i++) {
        Serial.print("  Reading ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(mag->headingFlatOnly(), 1);
        Serial.println(" deg");
        delay(500);
    }

    Serial.println("\nDone! Calibration stored.");
}

void loop() {}
