// SPDX-License-Identifier: GPL-3.0-or-later
//
// SoundLevel - print the sound level five times per second.
//
// The STeaMi microphone (IMP34DT05) measures the sound continuously. Every
// 200 ms the driver computes, over the last window:
//   - the RMS amplitude of the samples (0 to 32768),
//   - the same value in dBFS (0 dBFS = the loudest sine the microphone can
//     encode, a quiet room is around -60 dBFS),
//   - an estimate in dB SPL (decibels, like a sound level meter) computed
//     from the datasheet sensitivity: not calibrated, compare it with a
//     real sound level meter and use setLevelOffset() to correct it.
// A bar graph shows the level. Talk, clap or whistle near the board.
//
// Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiIMP34DT05.h>

SteamiIMP34DT05 mic;

static const uint16_t WINDOW_MS = 200;
static const float BAR_MIN_DBFS = -80.0f;  // empty bar
static const float BAR_MAX_DBFS = 0.0f;    // full bar
static const int BAR_WIDTH = 40;

void printBar(float dbfs) {
    float ratio = (dbfs - BAR_MIN_DBFS) / (BAR_MAX_DBFS - BAR_MIN_DBFS);
    int filled = static_cast<int>(ratio * BAR_WIDTH + 0.5f);
    filled = constrain(filled, 0, BAR_WIDTH);
    Serial.print('|');
    for (int i = 0; i < BAR_WIDTH; i++) {
        Serial.print(i < filled ? '#' : ' ');
    }
    Serial.print('|');
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    if (!mic.begin()) {
        Serial.println("Microphone not detected - check that this is a STeaMi board.");
        while (true) {
            delay(1000);
        }
    }
    mic.setLevelWindowMs(WINDOW_MS);

    Serial.println("Microphone ready. RMS, dBFS, estimated dB SPL:");
}

void loop() {
    if (!mic.levelReady()) {
        return;  // the current 200 ms window is not finished yet
    }

    SteamiIMP34DT05::LevelResult level = mic.level();

    Serial.print("RMS ");
    Serial.print(level.rms, 0);
    Serial.print("\t");
    Serial.print(level.dbfs, 1);
    Serial.print(" dBFS\t~");
    Serial.print(level.dbSpl, 0);
    Serial.print(" dB SPL\t");
    printBar(level.dbfs);
    Serial.println();
}
