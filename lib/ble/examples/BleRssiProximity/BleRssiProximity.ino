// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleRssiProximity — scan for a named BLE beacon and display a proximity
// gauge on the Serial monitor based on the received RSSI.
//
// One STeaMi runs the BleBeacon example (broadcaster).
// This STeaMi scans, filters on the beacon name, reads RSSI, applies a
// moving-average filter, and displays a 0-100% ASCII gauge.
//
// Open the serial monitor at 115200 baud to see the gauge update.

#include <Arduino.h>
#include <STM32duinoBLE.h>

// === Configuration ===
static const char* BEACON_NAME = "STeaMi-Beacon";
static const int RSSI_NEAR = -30;   // dBm — very close
static const int RSSI_FAR = -90;    // dBm — far away
static const int RSSI_SAMPLES = 5;  // moving average window

// === RSSI smoothing ===
static int rssiHistory[RSSI_SAMPLES] = {0};
static int rssiIndex = 0;
static int rssiCount = 0;

// =============================================================================

int smoothRssi(int newRssi) {
    rssiHistory[rssiIndex] = newRssi;
    rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES;
    if (rssiCount < RSSI_SAMPLES) {
        rssiCount++;
    }
    int sum = 0;
    for (int i = 0; i < rssiCount; i++) {
        sum += rssiHistory[i];
    }
    return sum / rssiCount;
}

int rssiToProximity(int rssi) {
    if (rssi >= RSSI_NEAR)
        return 100;
    if (rssi <= RSSI_FAR)
        return 0;
    return (int)((float)(rssi - RSSI_FAR) / (RSSI_NEAR - RSSI_FAR) * 100);
}

void printGauge(int rssi, int proximity) {
    // Build ASCII gauge [=========>         ]
    const int barWidth = 20;
    int filled = (proximity * barWidth) / 100;

    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.print(" dBm | [");
    for (int i = 0; i < barWidth; i++) {
        if (i < filled - 1)
            Serial.print("=");
        else if (i == filled - 1)
            Serial.print(">");
        else
            Serial.print(" ");
    }
    Serial.print("] ");
    Serial.print(proximity);
    Serial.println("%");
}

// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    Serial.println("BLE RSSI Proximity Scanner");
    Serial.print("Looking for: ");
    Serial.println(BEACON_NAME);

    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    BLE.scan(true);  // withDuplicates=true to get continuous RSSI updates
    Serial.println("Scanning...");
}

void loop() {
    BLEDevice device = BLE.available();

    if (device) {
        String name = device.localName();

        if (name == BEACON_NAME) {
            int rawRssi = device.rssi();
            int avgRssi = smoothRssi(rawRssi);
            int proximity = rssiToProximity(avgRssi);
            printGauge(avgRssi, proximity);
        }
    }

    BLE.poll();
}
