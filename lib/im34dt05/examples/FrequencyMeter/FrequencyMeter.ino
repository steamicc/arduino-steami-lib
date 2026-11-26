// SPDX-License-Identifier: GPL-3.0-or-later
//
// FrequencyMeter - show the dominant frequency of the sound and its note.
//
// dominantFrequencyHz() records 32 ms of sound (512 samples) and finds its
// strongest frequency with an FFT. The sketch converts it into the nearest
// note of the equal-tempered scale (A4 = 440 Hz) and the gap in cents
// (100 cents = one semitone): a small tuner. Whistle, sing a note, or play
// one with a phone app near the board.
//
// Quiet sounds are skipped: below SILENCE_DBFS the strongest frequency is
// just noise.
//
// Testing it with the board's own buzzer (tone(SPEAKER, f)): stay between
// 2000 and 4000 Hz. The piezo buzzer resonates near 3 kHz, so for a lower
// tone one of its harmonics is louder than the tone itself and the meter
// shows that harmonic (1000 Hz reads about 3000 Hz). A real instrument,
// a voice or a phone app does not have this problem.
//
// Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiIMP34DT05.h>
#include <math.h>

SteamiIMP34DT05 mic;

static const float SILENCE_DBFS = -50.0f;
static const float A4_HZ = 440.0f;
static const uint32_t PERIOD_MS = 250;

static const char* const NOTE_NAMES[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                           "F#", "G",  "G#", "A",  "A#", "B"};

void printNote(float frequency) {
    // MIDI note number: 69 is A4, one unit per semitone.
    float midi = 69.0f + 12.0f * log2f(frequency / A4_HZ);
    int nearest = static_cast<int>(lroundf(midi));
    int cents = static_cast<int>(lroundf((midi - nearest) * 100.0f));
    int octave = nearest / 12 - 1;

    Serial.print(NOTE_NAMES[nearest % 12]);
    Serial.print(octave);
    Serial.print(cents >= 0 ? " +" : " ");
    Serial.print(cents);
    Serial.print(" cents");
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
    mic.setLevelWindowMs(50);

    Serial.println("Make a sound: whistle, sing or play a note.");
}

void loop() {
    float frequency = mic.dominantFrequencyHz();
    float dbfs = mic.levelDbfs();  // level of the last 50 ms

    if (isnan(frequency) || isnan(dbfs) || dbfs < SILENCE_DBFS) {
        Serial.println("(too quiet)");
    } else {
        Serial.print(frequency, 1);
        Serial.print(" Hz  ");
        printNote(frequency);
        Serial.print("  (");
        Serial.print(dbfs, 0);
        Serial.println(" dBFS)");
    }

    delay(PERIOD_MS);
}
