// SPDX-License-Identifier: GPL-3.0-or-later
//
// BleHotCold — BLE treasure hunt game.
//
// One STeaMi runs as TREASURE (beacon), the other as SEEKER (scanner).
// The seeker maps RSSI to proximity zones and gives audio + serial feedback.
//
// Mode selection: send '1' for TREASURE, '2' for SEEKER via Serial monitor.
//
// Zones:
//   COLD    → proximity < 25%  → slow beeps  (440 Hz)
//   WARM    → proximity < 50%  → medium beeps (600 Hz)
//   HOT     → proximity < 75%  → fast beeps  (800 Hz)
//   BURNING → proximity >= 75% → rapid beeps (1200 Hz)
//
// Open the serial monitor at 115200 baud and send '1' or '2' to select mode.

#include <Arduino.h>
#include <STM32duinoBLE.h>

// === Configuration ===
static const char* BEACON_NAME = "STeaMi-Treasure";
static const int RSSI_NEAR = -30;
static const int RSSI_FAR = -90;
static const int RSSI_SAMPLES = 5;

// === Buzzer pin ===
static const int BUZZER_PIN = SPEAKER;  // STeaMi built-in speaker pin

// === RSSI smoothing ===
static int rssiHistory[RSSI_SAMPLES] = {0};
static int rssiIndex = 0;
static int rssiCount = 0;

// === State ===
static bool isTreasure = false;
static bool modeSelected = false;

// =============================================================================
// === HELPERS =================================================================
// =============================================================================

int smoothRssi(int newRssi) {
    rssiHistory[rssiIndex] = newRssi;
    rssiIndex = (rssiIndex + 1) % RSSI_SAMPLES;
    if (rssiCount < RSSI_SAMPLES)
        rssiCount++;
    int sum = 0;
    for (int i = 0; i < rssiCount; i++)
        sum += rssiHistory[i];
    return sum / rssiCount;
}

int rssiToProximity(int rssi) {
    if (rssi >= RSSI_NEAR)
        return 100;
    if (rssi <= RSSI_FAR)
        return 0;
    return (int)((float)(rssi - RSSI_FAR) / (RSSI_NEAR - RSSI_FAR) * 100);
}

void tone(int pin, int freq, int durationMs) {
    if (freq == 0) {
        delay(durationMs);
        return;
    }
    int period = 1000000 / freq;
    int half = period / 2;
    long end = micros() + (long)durationMs * 1000;
    while (micros() < end) {
        digitalWrite(pin, HIGH);
        delayMicroseconds(half);
        digitalWrite(pin, LOW);
        delayMicroseconds(half);
    }
}

void printMenu() {
    Serial.println("=== BLE Hot/Cold ===");
    Serial.println("1 - TREASURE (beacon)");
    Serial.println("2 - SEEKER  (scanner)");
    Serial.println("Enter your choice:");
}

// =============================================================================
// === TREASURE MODE ===========================================================
// =============================================================================

void runTreasure() {
    Serial.print("TREASURE mode — advertising as: ");
    Serial.println(BEACON_NAME);

    BLE.setLocalName(BEACON_NAME);
    BLE.setDeviceName(BEACON_NAME);
    BLE.setAdvertisingInterval(100);
    BLE.setConnectable(false);
    BLE.advertise();

    while (true) {
        BLE.poll();
        Serial.println("Broadcasting... (reset to change mode)");
        delay(2000);
    }
}

// =============================================================================
// === SEEKER MODE =============================================================
// =============================================================================

void printZone(int proximity, int rssi) {
    const char* zone;
    int freq;
    int beepMs;
    int pauseMs;

    if (proximity < 25) {
        zone = "COLD";
        freq = 440;
        beepMs = 80;
        pauseMs = 800;
    } else if (proximity < 50) {
        zone = "WARM";
        freq = 600;
        beepMs = 80;
        pauseMs = 400;
    } else if (proximity < 75) {
        zone = "HOT";
        freq = 800;
        beepMs = 80;
        pauseMs = 200;
    } else {
        zone = "BURNING!";
        freq = 1200;
        beepMs = 80;
        pauseMs = 80;
    }

    // ASCII gauge
    const int barWidth = 20;
    int filled = (proximity * barWidth) / 100;
    Serial.print(zone);
    Serial.print(" | RSSI: ");
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

    // Buzzer beep
    tone(BUZZER_PIN, freq, beepMs);
    delay(pauseMs);
}

void runSeeker() {
    Serial.print("SEEKER mode — scanning for: ");
    Serial.println(BEACON_NAME);

    BLE.scan(true);

    while (true) {
        BLEDevice device = BLE.available();

        if (device) {
            if (device.localName() == BEACON_NAME) {
                int rawRssi = device.rssi();
                int avgRssi = smoothRssi(rawRssi);
                int proximity = rssiToProximity(avgRssi);
                printZone(proximity, avgRssi);
            }
        }

        BLE.poll();
    }
}

// =============================================================================
// === SETUP / LOOP ============================================================
// =============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    pinMode(BUZZER_PIN, OUTPUT);

    if (!BLE.begin()) {
        Serial.println("BLE init failed!");
        while (true)
            ;
    }

    printMenu();
}

void loop() {
    if (!modeSelected && Serial.available()) {
        char choice = Serial.read();

        if (choice == '1') {
            modeSelected = true;
            isTreasure = true;
            runTreasure();
        } else if (choice == '2') {
            modeSelected = true;
            isTreasure = false;
            runSeeker();
        } else if (choice != '\n' && choice != '\r') {
            Serial.println("Invalid choice.");
            printMenu();
        }
    }
}
