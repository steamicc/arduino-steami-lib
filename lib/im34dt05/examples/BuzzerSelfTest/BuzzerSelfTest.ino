// SPDX-License-Identifier: GPL-3.0-or-later
//
// BuzzerSelfTest - the board listens to its own buzzer.
//
// The sketch plays a series of tones on the STeaMi buzzer with tone(), and
// for each one measures the dominant frequency heard by the microphone.
// A tone passes when the measured frequency is within 2 % of the played
// one (the microphone clock comes from the internal 16 MHz oscillator,
// accurate to about 1 %). It is a quick check that both the buzzer and the
// microphone work, and a demonstration of how a frequency is measured.
//
// The buzzer plays a square wave: if the measured frequency is a multiple
// of the played one, the buzzer made a harmonic louder than the
// fundamental (the sketch says so).
//
// Press RESET to run the test again. Open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiIMP34DT05.h>
#include <math.h>

SteamiIMP34DT05 mic;

static const int BUZZER_PIN = SPEAKER;
static const uint16_t TONES_HZ[] = {1000, 1500, 2000, 2500, 3000, 4000};
static const size_t TONE_COUNT = sizeof(TONES_HZ) / sizeof(TONES_HZ[0]);
static const float TOLERANCE = 0.02f;  // 2 %
static const int MEASURES = 3;         // per tone, the median is kept

float median3(float a, float b, float c) {
    if (a > b) {
        float t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        b = c;
    }
    return a > b ? a : b;
}

bool testTone(uint16_t played) {
    tone(BUZZER_PIN, played);
    delay(100);  // let the sound and the filters settle

    float measures[MEASURES];
    for (int i = 0; i < MEASURES; i++) {
        measures[i] = mic.dominantFrequencyHz();
    }
    float dbfs = mic.levelDbfs();
    noTone(BUZZER_PIN);

    float heard = median3(measures[0], measures[1], measures[2]);
    bool pass = !isnan(heard) && fabsf(heard - played) <= TOLERANCE * played;

    Serial.print("Played ");
    Serial.print(played);
    Serial.print(" Hz, heard ");
    Serial.print(heard, 1);
    Serial.print(" Hz (");
    Serial.print(dbfs, 0);
    Serial.print(" dBFS): ");
    if (pass) {
        Serial.println("PASS");
    } else {
        float ratio = heard / played;
        float rounded = roundf(ratio);
        if (rounded >= 2.0f && fabsf(ratio - rounded) <= TOLERANCE * rounded) {
            Serial.print("FAIL (harmonic x");
            Serial.print(static_cast<int>(rounded));
            Serial.println(")");
        } else {
            Serial.println("FAIL");
        }
    }

    delay(150);  // silence between tones
    return pass;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    pinMode(BUZZER_PIN, OUTPUT);
    if (!mic.begin()) {
        Serial.println("Microphone not detected - check that this is a STeaMi board.");
        while (true) {
            delay(1000);
        }
    }
    mic.setLevelWindowMs(50);

    delay(200);
    Serial.print("Background level: ");
    Serial.print(mic.levelDbfs(), 1);
    Serial.println(" dBFS");

    size_t passed = 0;
    for (size_t i = 0; i < TONE_COUNT; i++) {
        if (testTone(TONES_HZ[i])) {
            passed++;
        }
    }

    Serial.print("Result: ");
    Serial.print(passed);
    Serial.print("/");
    Serial.print(TONE_COUNT);
    Serial.println(passed == TONE_COUNT ? " tones recognised - SELF-TEST PASSED"
                                        : " tones recognised - SELF-TEST FAILED");
}

void loop() {
    delay(1000);
}
