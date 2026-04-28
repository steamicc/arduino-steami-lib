// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleIndoorMap — real-time indoor position display using BLE trilateration.

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

static const float BEACON_X[BEACON_COUNT] = {0.0f, 420.0f, 330.0f};
static const float BEACON_Y[BEACON_COUNT] = {0.0f, 0.0f, 278.0f};

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
static unsigned long lastSeenMs[BEACON_COUNT] = {0};
static const unsigned long BEACON_TIMEOUT_MS = 5000;

// === Position filtering ===
static const float ALPHA = 0.15f;
static const float MIN_MOVE_CM = 15.0f;
static float filteredX = -1.0f;
static float filteredY = -1.0f;

// === Position trail ===
static const int TRAIL_LEN = 5;
static float trailX[TRAIL_LEN];
static float trailY[TRAIL_LEN];
static int trailCount = 0;
static int trailIdx = 0;

// === ASCII map dimensions ===
static const int MAP_COLS = 44;
static const int MAP_ROWS = 30;
static const float CM_PER_COL = 10.0f;
static const float CM_PER_ROW = 10.0f;

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
    d *= 100.0f;
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

    trailX[trailIdx] = filteredX;
    trailY[trailIdx] = filteredY;
    trailIdx = (trailIdx + 1) % TRAIL_LEN;
    if (trailCount < TRAIL_LEN)
        trailCount++;

    filteredX = fx;
    filteredY = fy;
}

void clearTrail() {
    trailCount = 0;
    trailIdx = 0;
    filteredX = -1.0f;
    filteredY = -1.0f;
    Serial.println("Trail cleared.");
}

void printBeaconStatus() {
    unsigned long now = millis();
    Serial.println("--- Beacons ---");
    for (int i = 0; i < BEACON_COUNT; i++) {
        // Expire beacon si pas vu depuis BEACON_TIMEOUT_MS
        if (beaconSeen[i] && (now - lastSeenMs[i]) > BEACON_TIMEOUT_MS) {
            beaconSeen[i] = false;
        }
        Serial.print("  ");
        Serial.print(BEACON_NAMES[i]);
        Serial.print(": ");
        if (beaconSeen[i]) {
            Serial.print("OK  RSSI=");
            Serial.print(currentRssi[i]);
            Serial.print("dBm  dist=");
            Serial.print((int)rssiToDistance(currentRssi[i], i));
            Serial.print("cm  age=");
            Serial.print((now - lastSeenMs[i]) / 1000);
            Serial.println("s");
        } else {
            Serial.println("NOT SEEN");
        }
    }
}

void printMap() {
    char map[MAP_ROWS][MAP_COLS + 1];

    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++)
            map[r][c] = ' ';
        map[r][MAP_COLS] = '\0';
    }

    for (int c = 0; c < MAP_COLS; c++) {
        map[0][c] = '-';
        map[MAP_ROWS - 1][c] = '-';
    }
    for (int r = 0; r < MAP_ROWS; r++) {
        map[r][0] = '|';
        map[r][MAP_COLS - 1] = '|';
    }
    map[0][0] = '+';
    map[0][MAP_COLS - 1] = '+';
    map[MAP_ROWS - 1][0] = '+';
    map[MAP_ROWS - 1][MAP_COLS - 1] = '+';

    for (int i = 0; i < BEACON_COUNT; i++) {
        int c = 1 + (int)(BEACON_X[i] / CM_PER_COL);
        int r = MAP_ROWS - 2 - (int)(BEACON_Y[i] / CM_PER_ROW);
        c = max(1, min(MAP_COLS - 2, c));
        r = max(1, min(MAP_ROWS - 2, r));
        map[r][c] = '1' + i;
    }

    for (int i = 0; i < trailCount; i++) {
        int ti = (trailIdx - trailCount + i + TRAIL_LEN) % TRAIL_LEN;
        int c = 1 + (int)(trailX[ti] / CM_PER_COL);
        int r = MAP_ROWS - 2 - (int)(trailY[ti] / CM_PER_ROW);
        c = max(1, min(MAP_COLS - 2, c));
        r = max(1, min(MAP_ROWS - 2, r));
        if (map[r][c] == ' ')
            map[r][c] = '.';
    }

    if (filteredX >= 0.0f) {
        int c = 1 + (int)(filteredX / CM_PER_COL);
        int r = MAP_ROWS - 2 - (int)(filteredY / CM_PER_ROW);
        c = max(1, min(MAP_COLS - 2, c));
        r = max(1, min(MAP_ROWS - 2, r));
        map[r][c] = 'X';
    }

    Serial.print("\033[2J\033[H");
    Serial.println("+------ BLE Indoor Map ------+");
    for (int r = 0; r < MAP_ROWS; r++) {
        Serial.println(map[r]);
    }
    Serial.println("1=M1  2=M2  3=M3  X=You  .=trail");

    if (filteredX >= 0.0f) {
        Serial.print("Position: X=");
        Serial.print((int)filteredX);
        Serial.print("cm  Y=");
        Serial.print((int)filteredY);
        Serial.println("cm");
    } else {
        Serial.println("Position: en attente des 3 beacons...");
    }

    // Affichage status beacons
    printBeaconStatus();

    Serial.println("Commands: c=clear trail  h=help");
}

void printHelp() {
    Serial.println("=== BLE Indoor Map ===");
    Serial.println("c → clear trail and reset position");
    Serial.println("h → help");
    Serial.println("Beacon layout (cm):");
    for (int i = 0; i < BEACON_COUNT; i++) {
        Serial.print("  M");
        Serial.print(i + 1);
        Serial.print(" = (");
        Serial.print((int)BEACON_X[i]);
        Serial.print(", ");
        Serial.print((int)BEACON_Y[i]);
        Serial.println(")");
    }
}

// =============================================================================
// === SETUP / LOOP ============================================================
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    printHelp();

    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    BLE.scan(true);
    Serial.println("Scanning for beacons...");
}

void loop() {
    // Handle Serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c') {
            clearTrail();
        } else if (cmd == 'h') {
            printHelp();
        }
    }

    // === Drainer TOUS les devices disponibles ===
    BLEDevice device;
    while ((device = BLE.available())) {
        String name = device.localName();
        int idx = beaconIndex(name);
        if (idx >= 0) {
            smoothRssi(idx, device.rssi());
            beaconSeen[idx] = true;
            lastSeenMs[idx] = millis();
        }
    }

    BLE.poll();

    // Update position every 500ms
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 500)
        return;
    lastUpdate = millis();

    // Expirer les beacons trop vieux
    unsigned long now = millis();
    int seen = 0;
    for (int i = 0; i < BEACON_COUNT; i++) {
        if (beaconSeen[i] && (now - lastSeenMs[i]) > BEACON_TIMEOUT_MS)
            beaconSeen[i] = false;
        if (beaconSeen[i])
            seen++;
    }

    if (seen == 3) {
        float d[BEACON_COUNT];
        for (int i = 0; i < BEACON_COUNT; i++)
            d[i] = rssiToDistance(currentRssi[i], i);

        float rawX, rawY;
        if (trilaterate(d[0], d[1], d[2], rawX, rawY))
            applyFilter(rawX, rawY);
    }

    printMap();
}
