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
// The seeker is fully non-blocking: BLE.poll() and BLE.available() are
// drained on every loop tick, the buzzer is driven by a millis()-based
// state machine through the Arduino core's hardware tone()/noTone(), and
// the gauge is throttled — so the smoothed RSSI is never older than one
// advertising interval.
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

// === Print throttle ===
static const uint32_t PRINT_PERIOD_MS = 250;

// === RSSI smoothing ===
static int rssiHistory[RSSI_SAMPLES] = {0};
static int rssiIndex = 0;
static int rssiCount = 0;

// === Mode selection ===
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

// Audio profile for a proximity zone. freq == 0 means silent.
struct ZoneProfile {
    const char* label;
    int freq;     // Hz
    int beepMs;   // sounding time per period
    int pauseMs;  // silent time per period
};

ZoneProfile zoneFor(int proximity) {
    if (proximity < 25)
        return {"COLD", 440, 80, 800};
    if (proximity < 50)
        return {"WARM", 600, 80, 400};
    if (proximity < 75)
        return {"HOT", 800, 80, 200};
    return {"BURNING!", 1200, 80, 80};
}

// Non-blocking buzzer driven by the Arduino core's tone()/noTone(), which use
// a hardware timer on STM32duino — neither call blocks the CPU. The state
// machine flips on/off based on millis() so the caller's loop stays free to
// poll BLE.
struct BuzzerState {
    bool on = false;
    uint32_t transitionAtMs = 0;
    int activeFreq = 0;
};

void buzzerTick(BuzzerState& s, const ZoneProfile& z) {
    uint32_t now = millis();

    // Silent profile or no profile yet — make sure the buzzer is off.
    if (z.freq <= 0) {
        if (s.on) {
            noTone(BUZZER_PIN);
            s.on = false;
            s.activeFreq = 0;
        }
        return;
    }

    // Re-arm the timer (and restart the tone) when the active frequency
    // changes — otherwise the user would hear the previous zone's tone
    // until the next beep boundary.
    if (s.on && s.activeFreq != z.freq) {
        tone(BUZZER_PIN, z.freq);
        s.activeFreq = z.freq;
    }

    if ((int32_t)(now - s.transitionAtMs) < 0)
        return;

    if (s.on) {
        noTone(BUZZER_PIN);
        s.on = false;
        s.activeFreq = 0;
        s.transitionAtMs = now + (uint32_t)z.pauseMs;
    } else {
        tone(BUZZER_PIN, z.freq);
        s.on = true;
        s.activeFreq = z.freq;
        s.transitionAtMs = now + (uint32_t)z.beepMs;
    }
}

void printGauge(const char* zone, int rssi, int proximity) {
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

    uint32_t lastHeartbeatMs = 0;
    while (true) {
        BLE.poll();
        uint32_t now = millis();
        if (now - lastHeartbeatMs >= 2000) {
            lastHeartbeatMs = now;
            Serial.println("Broadcasting... (reset to change mode)");
        }
    }
}

// =============================================================================
// === SEEKER MODE =============================================================
// =============================================================================

void runSeeker() {
    Serial.print("SEEKER mode — scanning for: ");
    Serial.println(BEACON_NAME);

    BLE.scan(true);

    BuzzerState buzzer;
    ZoneProfile zone = {"—", 0, 0, 0};
    int latestRssi = RSSI_FAR;
    int latestProximity = 0;
    bool seenAny = false;
    uint32_t lastPrintMs = 0;

    while (true) {
        BLE.poll();

        BLEDevice device;
        while ((device = BLE.available())) {
            if (device.localName() == BEACON_NAME) {
                latestRssi = smoothRssi(device.rssi());
                latestProximity = rssiToProximity(latestRssi);
                zone = zoneFor(latestProximity);
                seenAny = true;
            }
        }

        buzzerTick(buzzer, zone);

        uint32_t now = millis();
        if (seenAny && (now - lastPrintMs >= PRINT_PERIOD_MS)) {
            lastPrintMs = now;
            printGauge(zone.label, latestRssi, latestProximity);
        }
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
            runTreasure();
        } else if (choice == '2') {
            modeSelected = true;
            runSeeker();
        } else if (choice != '\n' && choice != '\r') {
            Serial.println("Invalid choice.");
            printMenu();
        }
    }
}
