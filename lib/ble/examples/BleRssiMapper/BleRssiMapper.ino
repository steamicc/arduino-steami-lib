// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleRssiMapper — record RSSI measurements at different positions in a room.
//
// Place 2-3 STeaMi boards running BleBeacon at known positions.
// Move around the room with this board and send 'r' via Serial to record
// the current RSSI of all visible beacons as a CSV row.
// Send 's' to stop and print the full CSV.
//
// CSV format: point_id,beacon_name,rssi
//
// Open the serial monitor at 115200 baud.
// Commands:
//   r → record current RSSI for all visible beacons
//   s → stop and print full CSV
//   h → print help

#include <Arduino.h>
#include <STM32duinoBLE.h>

// === Configuration ===
static const char* BEACON_NAMES[] = {
    "Beacon_M1",
    "Beacon_M2",
    "Beacon_M3",
};
static const int BEACON_COUNT = 3;
static const int RSSI_SAMPLES = 5;
static const int MAX_POINTS = 50;  // max measurement points
static const int MAX_ROWS = MAX_POINTS * BEACON_COUNT;

// === RSSI smoothing per beacon ===
static int rssiHistory[3][RSSI_SAMPLES] = {{0}};
static int rssiIndex[3] = {0};
static int rssiCount[3] = {0};
static int currentRssi[3];
static bool beaconSeen[3] = {false};

// === CSV storage (in RAM) ===
struct CsvRow {
    int pointId;
    char beaconName[16];
    int rssi;
};
static CsvRow csvData[MAX_ROWS];
static int rowCount = 0;
static int pointId = 0;
static bool recording = true;

// =============================================================================
// === HELPERS =================================================================
// =============================================================================

int beaconIndex(const String& name) {
    for (int i = 0; i < BEACON_COUNT; i++) {
        if (name == BEACON_NAMES[i])
            return i;
    }
    return -1;
}

void smoothRssi(int idx, int newRssi) {
    rssiHistory[idx][rssiIndex[idx]] = newRssi;
    rssiIndex[idx] = (rssiIndex[idx] + 1) % RSSI_SAMPLES;
    if (rssiCount[idx] < RSSI_SAMPLES)
        rssiCount[idx]++;
    int sum = 0;
    for (int i = 0; i < rssiCount[idx]; i++)
        sum += rssiHistory[idx][i];
    currentRssi[idx] = sum / rssiCount[idx];
}

void printHelp() {
    Serial.println("=== BLE RSSI Room Mapper ===");
    Serial.println("r → record current point");
    Serial.println("s → stop and print CSV");
    Serial.println("h → help");
    Serial.println("============================");
}

void printCurrentScan() {
    Serial.print("Point #");
    Serial.print(pointId + 1);
    Serial.print(" | Beacons: ");
    for (int i = 0; i < BEACON_COUNT; i++) {
        if (beaconSeen[i]) {
            Serial.print(BEACON_NAMES[i]);
            Serial.print("=");
            Serial.print(currentRssi[i]);
            Serial.print("dBm  ");
        }
    }
    Serial.println();
}

void recordPoint() {
    int seen = 0;
    for (int i = 0; i < BEACON_COUNT; i++) {
        if (beaconSeen[i])
            seen++;
    }

    if (seen == 0) {
        Serial.println("No beacon seen — move closer and retry.");
        return;
    }

    pointId++;
    Serial.print("Recording point #");
    Serial.print(pointId);
    Serial.print(" (");
    Serial.print(seen);
    Serial.println(" beacons)");

    for (int i = 0; i < BEACON_COUNT; i++) {
        if (beaconSeen[i] && rowCount < MAX_ROWS) {
            csvData[rowCount].pointId = pointId;
            strncpy(csvData[rowCount].beaconName, BEACON_NAMES[i], 15);
            csvData[rowCount].rssi = currentRssi[i];
            rowCount++;

            Serial.print("  ");
            Serial.print(BEACON_NAMES[i]);
            Serial.print(", ");
            Serial.println(currentRssi[i]);
        }
    }
}

void printCsv() {
    Serial.println();
    Serial.println("=== CSV OUTPUT — copy and save as rssi_map.csv ===");
    Serial.println("point_id,beacon_name,rssi");
    for (int i = 0; i < rowCount; i++) {
        Serial.print(csvData[i].pointId);
        Serial.print(",");
        Serial.print(csvData[i].beaconName);
        Serial.print(",");
        Serial.println(csvData[i].rssi);
    }
    Serial.println("=== END OF CSV ===");
    Serial.print("Total: ");
    Serial.print(pointId);
    Serial.print(" points, ");
    Serial.print(rowCount);
    Serial.println(" rows.");
}

// =============================================================================
// === SETUP / LOOP ============================================================
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    BLE.scan(true);
    printHelp();
    Serial.println("Scanning for beacons...");
}

void loop() {
    // Handle Serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'r' && recording) {
            recordPoint();
        } else if (cmd == 's') {
            recording = false;
            BLE.stopScan();
            printCsv();
        } else if (cmd == 'h') {
            printHelp();
        } else if (cmd != '\n' && cmd != '\r') {
            Serial.println("Unknown command. Send 'h' for help.");
        }
    }

    // Update RSSI from scan results
    if (recording) {
        BLEDevice device = BLE.available();
        if (device) {
            int idx = beaconIndex(device.localName());
            if (idx >= 0) {
                smoothRssi(idx, device.rssi());
                beaconSeen[idx] = true;
            }
        }

        // Print current scan status every 2s
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 2000) {
            lastPrint = millis();
            printCurrentScan();
        }
    }

    BLE.poll();
}
