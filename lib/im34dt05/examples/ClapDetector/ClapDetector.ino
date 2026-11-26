// SPDX-License-Identifier: GPL-3.0-or-later
//
// ClapDetector - toggle the blue LED each time you clap your hands.
//
// The driver measures the sound over short 10 ms windows. A clap is a
// sharp peak: much louder than the background noise (whose level the
// sketch follows slowly) and above an absolute threshold. After a clap,
// the sketch ignores the sound for 300 ms so that the echo of the same
// clap does not count twice.
//
// Tune PEAK_MIN and PEAK_RATIO if the room is noisy or the claps too far.
//
// Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiIMP34DT05.h>

SteamiIMP34DT05 mic;

static const int CLAP_LED = LED_BLUE;

static const uint16_t WINDOW_MS = 10;
static const uint16_t PEAK_MIN = 8000;    // absolute threshold, PCM units (max 32768)
static const float PEAK_RATIO = 8.0f;     // peak / background RMS
static const uint32_t HOLD_OFF_MS = 300;  // ignore the echo of a clap
static const float BACKGROUND_SMOOTHING = 0.02f;

float background = 0.0f;  // slow average of the RMS, between claps
uint32_t lastClapMs = 0;
uint32_t claps = 0;
bool ledOn = false;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    pinMode(CLAP_LED, OUTPUT);
    digitalWrite(CLAP_LED, LOW);

    if (!mic.begin()) {
        Serial.println("Microphone not detected - check that this is a STeaMi board.");
        while (true) {
            delay(1000);
        }
    }
    mic.setLevelWindowMs(WINDOW_MS);

    Serial.println("Clap your hands near the board.");
}

void loop() {
    if (!mic.levelReady()) {
        return;
    }
    SteamiIMP34DT05::LevelResult level = mic.level();
    uint32_t now = millis();

    if (now - lastClapMs < HOLD_OFF_MS) {
        return;  // still in the echo of the last clap
    }

    bool loud = level.peak >= PEAK_MIN;
    bool sharp = level.peak >= PEAK_RATIO * background;
    if (loud && sharp) {
        lastClapMs = now;
        claps++;
        ledOn = !ledOn;
        digitalWrite(CLAP_LED, ledOn ? HIGH : LOW);

        Serial.print("Clap #");
        Serial.print(claps);
        Serial.print("  peak ");
        Serial.print(level.peak);
        Serial.print("  background RMS ");
        Serial.println(background, 0);
        return;
    }

    // Follow the background noise slowly (first window: take it as is).
    if (background == 0.0f) {
        background = level.rms;
    } else {
        background += BACKGROUND_SMOOTHING * (level.rms - background);
    }
}
