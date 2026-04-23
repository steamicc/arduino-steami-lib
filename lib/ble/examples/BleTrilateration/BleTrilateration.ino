// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleTrilateration — estimate 2D position using RSSI trilateration.
//
// 3 STeaMi boards run BleBeacon at known positions (cm).
// This mobile board scans all 3, converts RSSI to distance using the
// log-distance path loss model, then trilaterates a 2D position.
//
// Beacon coordinates (cm, measured on site):
//   Beacon_M1 = (  0,   0)
//   Beacon_M2 = (420,   0)
//   Beacon_M3 = (330, 278)
//
// Calibration (nRF Connect, iPhone 17 Pro Max):
//   RSSI_ref at 1m: M1=-75, M2=-75, M3=-80
//   Path loss n:    M1=3.99, M2=4.32, M3=2.66
//
// Open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <STM32duinoBLE.h>
#include <math.h>

// === Beacon configuration ===
static const int BEACON_COUNT = 3;

static const char* BEACON_NAMES[BEACON_COUNT] = {
    "Beacon_M1",
    "Beacon_M2",
    "Beacon_M3",
};

// Beacon positions in cm
static const float BEACON_X[BEACON_COUNT] = {0.0f, 420.0f, 330.0f};
static const float BEACON_Y[BEACON_COUNT] = {0.0f, 0.0f, 278.0f};

// Path loss calibration (measured on site)
static const float RSSI_REF[BEACON_COUNT] = {-75.0f, -75.0f, -80.0f};
static const float PATH_LOSS_N[BEACON_COUNT] = {3.99f, 4.32f, 2.66f};

// === Trilateration validity ===
static const float MAX_DIST_CM = 430.0f;
static const float MAX_VALID_CM = 550.0f;
static const float CENTROID_X = (0.0f + 420.0f + 330.0f) / 3.0f;
static const float CENTROID_Y = (0.0f + 0.0f + 278.0f) / 3.0f;

// === RSSI smoothing ===
static const int RSSI_SAMPLES = 8;
static int rssiHistory[BEACON_COUNT][RSSI_SAMPLES] = {{0}};
static int rssiIndex[BEACON_COUNT] = {0};
static int rssiCount[BEACON_COUNT] = {0};
static int currentRssi[BEACON_COUNT];
static bool beaconSeen[BEACON_COUNT] = {false};

// === Position filtering ===
static const float ALPHA = 0.15f;
static const float MIN_MOVE_CM = 15.0f;
static float filteredX = -1.0f;
static float filteredY = -1.0f;

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

float rssiToDistance(int rssi, int idx) {
    float d = pow(10.0f, (RSSI_REF[idx] - rssi) / (10.0f * PATH_LOSS_N[idx]));
    d *= 100.0f;  // metres -> cm
    if (d > MAX_DIST_CM)
        d = MAX_DIST_CM;
    return d;
}

bool trilaterate(float r0, float r1, float r2, float& outX, float& outY) {
    float x1 = BEACON_X[0], y1 = BEACON_Y[0];
    float x2 = BEACON_X[1], y2 = BEACON_Y[1];
    float x3 = BEACON_X[2], y3 = BEACON_Y[2];

    float A = 2.0f * (x2 - x1);
    float B = 2.0f * (y2 - y1);
    float C = r0 * r0 - r1 * r1 - x1 * x1 + x2 * x2 - y1 * y1 + y2 * y2;
    float D = 2.0f * (x3 - x1);
    float E = 2.0f * (y3 - y1);
    float F = r0 * r0 - r2 * r2 - x1 * x1 + x3 * x3 - y1 * y1 + y3 * y3;

    float denom = A * E - B * D;
    if (fabsf(denom) < 1e-6f)
        return false;

    outX = (C * E - F * B) / denom;
    outY = (A * F - D * C) / denom;

    // Reject if too far from centroid
    float dx = outX - CENTROID_X;
    float dy = outY - CENTROID_Y;
    if (sqrtf(dx * dx + dy * dy) > MAX_VALID_CM)
        return false;

    return true;
}

void applyFilter(float newX, float newY) {
    if (filteredX < 0.0f) {
        filteredX = newX;
        filteredY = newY;
        return;
    }
    float fx = ALPHA * newX + (1.0f - ALPHA) * filteredX;
    float fy = ALPHA * newY + (1.0f - ALPHA) * filteredY;

    float dx = fx - filteredX;
    float dy = fy - filteredY;
    if (sqrtf(dx * dx + dy * dy) < MIN_MOVE_CM)
        return;

    filteredX = fx;
    filteredY = fy;
}

void printMap() {
    // ASCII 2D map — 42x28 chars, 1 char = 10cm
    const int COLS = 42;
    const int ROWS = 28;
    char map[ROWS][COLS + 1];

    // Fill with spaces
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++)
            map[r][c] = '.';
        map[r][COLS] = '\0';
    }

    // Place beacons
    for (int i = 0; i < BEACON_COUNT; i++) {
        int c = (int)(BEACON_X[i] / 10.0f);
        int r = ROWS - 1 - (int)(BEACON_Y[i] / 10.0f);
        if (c >= 0 && c < COLS && r >= 0 && r < ROWS) {
            map[r][c] = '0' + i + 1;  // '1', '2', '3'
        }
    }

    // Place estimated position
    if (filteredX >= 0.0f) {
        int c = (int)(filteredX / 10.0f);
        int r = ROWS - 1 - (int)(filteredY / 10.0f);
        c = max(0, min(COLS - 1, c));
        r = max(0, min(ROWS - 1, r));
        map[r][c] = 'X';
    }

    // Print map
    Serial.println();
    Serial.println("=== Position Map (1 char = 10cm) ===");
    for (int r = 0; r < ROWS; r++) {
        Serial.println(map[r]);
    }
    Serial.println("1=M1  2=M2  3=M3  X=You");
}

// =============================================================================
// === SETUP / LOOP ============================================================
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    Serial.println("BLE Trilateration Indoor Positioning");
    Serial.println("Beacon layout (cm):");
    for (int i = 0; i < BEACON_COUNT; i++) {
        Serial.print("  ");
        Serial.print(BEACON_NAMES[i]);
        Serial.print(" = (");
        Serial.print((int)BEACON_X[i]);
        Serial.print(", ");
        Serial.print((int)BEACON_Y[i]);
        Serial.println(")");
    }
    Serial.println();

    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    BLE.scan(true);
    Serial.println("Scanning...");
}

void loop() {
    // Update RSSI from scan
    BLEDevice device = BLE.available();
    if (device) {
        int idx = beaconIndex(device.localName());
        if (idx >= 0) {
            smoothRssi(idx, device.rssi());
            beaconSeen[idx] = true;
        }
    }

    BLE.poll();

    // Update position every 500ms
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500)
        return;
    lastUpdate = millis();

    // Check all 3 beacons are seen
    int seen = 0;
    for (int i = 0; i < BEACON_COUNT; i++) {
        if (beaconSeen[i])
            seen++;
    }

    if (seen < 3) {
        Serial.print("Waiting for beacons (");
        Serial.print(seen);
        Serial.println("/3 seen)...");
        return;
    }

    // Compute distances
    float d[BEACON_COUNT];
    for (int i = 0; i < BEACON_COUNT; i++) {
        d[i] = rssiToDistance(currentRssi[i], i);
    }

    // Print distances
    Serial.print("Distances: ");
    for (int i = 0; i < BEACON_COUNT; i++) {
        Serial.print(BEACON_NAMES[i]);
        Serial.print("=");
        Serial.print((int)d[i]);
        Serial.print("cm  ");
    }
    Serial.println();

    // Trilaterate
    float rawX, rawY;
    if (!trilaterate(d[0], d[1], d[2], rawX, rawY)) {
        Serial.println("Trilateration failed — aberrant result rejected.");
        return;
    }

    // Filter
    applyFilter(rawX, rawY);

    // Print position
    Serial.print("Position: X=");
    Serial.print((int)filteredX);
    Serial.print("cm  Y=");
    Serial.print((int)filteredY);
    Serial.println("cm");

    // Print ASCII map every 2s
    static unsigned long lastMap = 0;
    if (millis() - lastMap > 2000) {
        lastMap = millis();
        printMap();
    }
}
