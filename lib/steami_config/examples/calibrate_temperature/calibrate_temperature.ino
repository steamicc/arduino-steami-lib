// SPDX-License-Identifier: GPL-3.0-or-later
//
// SteamiConfig — calibrate the temperature sensors of the board against its
// reference sensor, and save the result in the DAPLink config zone. Output on
// the serial monitor (115200 baud).
//
// The reference is the BME280 on a V1c and the WSEN-HIDS on a V1a/V1b. Every
// other temperature sensor gets an offset so that it reads like the
// reference. The offsets are stored under the keys that both the Arduino and
// the MicroPython libraries read ("bme", or "hid" and "pad", plus "mag" and
// "ism") and survive power cycles. The legacy "hts" entry of older tools is
// removed, never written. Same steps as calibrate_temperature.py in the
// MicroPython steami_config library.
//
// Leave the board a few minutes at a stable room temperature, away from any
// heat source, before running it.

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

// Config names; the first one of each list is the reference.
const char* const V1C_SENSORS[] = {
    STEAMI_CONFIG_SENSOR_BME280,
    STEAMI_CONFIG_SENSOR_LIS2MDL,
    STEAMI_CONFIG_SENSOR_ISM330DL,
};
const char* const V1AB_SENSORS[] = {
    STEAMI_CONFIG_SENSOR_WSEN_HIDS,
    STEAMI_CONFIG_SENSOR_WSEN_PADS,
    STEAMI_CONFIG_SENSOR_LIS2MDL,
    STEAMI_CONFIG_SENSOR_ISM330DL,
};

// One reading of a sensor, NAN when it does not answer. With a config, the
// calibration stored for `name` is applied first; without, the calibration is
// removed (setTemperatureOffset(0) sets the gain back to 1 as well).
float measure(SteamiBME280* sensor, const SteamiConfig* config, const char* name) {
    if (sensor == nullptr) {
        return NAN;
    }
    if (config == nullptr) {
        sensor->setTemperatureOffset(0.0f);
    } else {
        config->applyTemperatureCalibration(name, *sensor);
    }
    return sensor->temperature();
}

float measure(SteamiWsenHids* sensor, const SteamiConfig* config, const char* name) {
    if (sensor == nullptr) {
        return NAN;
    }
    if (config == nullptr) {
        sensor->setTemperatureOffset(0.0f);
    } else {
        config->applyTemperatureCalibration(name, *sensor);
    }
    return sensor->temperature();
}

float measure(SteamiWsenPads* sensor, const SteamiConfig* config, const char* name) {
    if (sensor == nullptr) {
        return NAN;
    }
    if (config == nullptr) {
        sensor->setTemperatureOffset(0.0f);
    } else {
        config->applyTemperatureCalibration(name, *sensor);
    }
    return sensor->temperature();
}

float measure(SteamiLIS2MDL* sensor, const SteamiConfig* config, const char* name) {
    if (sensor == nullptr) {
        return NAN;
    }
    if (config == nullptr) {
        sensor->setTemperatureOffset(0.0f);
    } else {
        config->applyTemperatureCalibration(name, *sensor);
    }
    return sensor->temperature();
}

float measure(SteamiISM330DL* sensor, const SteamiConfig* config, const char* name) {
    if (sensor == nullptr) {
        return NAN;
    }
    if (config == nullptr) {
        sensor->setTemperatureOffset(0.0f);
    } else {
        config->applyTemperatureCalibration(name, *sensor);
    }
    float value = NAN;
    return sensor->temperature(value) ? value : NAN;
}

// Reads the sensor stored under the config name `name` through the board's
// shared driver.
float measure(const char* name, const SteamiConfig* config) {
    if (strcmp(name, STEAMI_CONFIG_SENSOR_BME280) == 0) {
        return measure(Steami.bme280(), config, name);
    }
    if (strcmp(name, STEAMI_CONFIG_SENSOR_WSEN_HIDS) == 0) {
        return measure(Steami.wsenHids(), config, name);
    }
    if (strcmp(name, STEAMI_CONFIG_SENSOR_WSEN_PADS) == 0) {
        return measure(Steami.wsenPads(), config, name);
    }
    if (strcmp(name, STEAMI_CONFIG_SENSOR_LIS2MDL) == 0) {
        return measure(Steami.lis2mdl(), config, name);
    }
    if (strcmp(name, STEAMI_CONFIG_SENSOR_ISM330DL) == 0) {
        return measure(Steami.ism330dl(), config, name);
    }
    return NAN;
}

// "  bme280    :  23.41 C", padded like the MicroPython example.
void printName(const char* name) {
    Serial.print("  ");
    Serial.print(name);
    for (size_t i = strlen(name); i < 10; i++) {
        Serial.print(' ');
    }
    Serial.print(": ");
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

    const char* const* sensors = V1AB_SENSORS;
    size_t count = sizeof(V1AB_SENSORS) / sizeof(V1AB_SENSORS[0]);
    if (Steami.isV1c()) {
        sensors = V1C_SENSORS;
        count = sizeof(V1C_SENSORS) / sizeof(V1C_SENSORS[0]);
    }
    const char* reference = sensors[0];

    // Measure without any calibration, then let ISM330DL and LIS2MDL take a sample.
    for (size_t i = 0; i < count; i++) {
        measure(sensors[i], nullptr);
    }
    delay(200);

    const float referenceTemperature = measure(reference, nullptr);
    if (isnan(referenceTemperature)) {
        Serial.print("The reference sensor ");
        Serial.print(reference);
        Serial.println(" does not answer.");
        return;
    }
    Serial.print("Board ");
    Serial.print(Steami.releaseName());
    Serial.print(", reference ");
    Serial.print(reference);
    Serial.print(": ");
    Serial.print(referenceTemperature, 2);
    Serial.println(" C");

    config->setTemperatureCalibration(reference, 1.0f, 0.0f);
    // The board reads the legacy "hts" entry when "hid" is the identity: drop it.
    config->clearTemperatureCalibration(STEAMI_CONFIG_SENSOR_HTS221);

    for (size_t i = 1; i < count; i++) {
        printName(sensors[i]);
        const float raw = measure(sensors[i], nullptr);
        if (isnan(raw)) {
            Serial.println("does not answer, skipped");
            continue;
        }
        const float offset = referenceTemperature - raw;
        config->setTemperatureCalibration(sensors[i], 1.0f, offset);
        Serial.print(raw, 2);
        Serial.print(" C -> offset ");
        if (offset >= 0.0f) {
            Serial.print('+');
        }
        Serial.println(offset, 2);
    }

    if (!config->save()) {
        Serial.println("\nThe calibration does not fit in the config zone: nothing saved.");
        return;
    }
    Serial.println("\nCalibration saved.");

    // Verify: read the config zone back and apply it.
    if (!config->load()) {
        Serial.println("The config zone could not be read back.");
        return;
    }
    Serial.println("\nVerification (after reload):");
    for (size_t i = 0; i < count; i++) {
        printName(sensors[i]);
        const float value = measure(sensors[i], config);
        if (isnan(value)) {
            Serial.println("does not answer");
        } else {
            Serial.print(value, 2);
            Serial.println(" C");
        }
    }
}

void loop() {}
