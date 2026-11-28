// SPDX-License-Identifier: GPL-3.0-or-later
//
// VoiceActivityDetector - light the green LED while someone speaks.
//
// The sketch reads the microphone samples (IMP34DT05, 16 kHz) in 20 ms
// frames and tells voice from silence with three simple rules:
//   - loud enough: the frame is MARGIN_DB above the background noise,
//     whose level the sketch follows (and above MIN_VOICE_DBFS);
//   - voice-like: few zero crossings. Voiced sounds (vowels) carry most of
//     their energy below 1 kHz, while hiss and rustle cross zero far more
//     often;
//   - long enough: ATTACK_FRAMES voice frames in a row (80 ms), so that a
//     clap or a knock does not count.
// The LED stays on HANGOVER_MS after the last voice frame, to ride over
// the short pauses between words.
//
// The serial monitor shows each start and end of speech, and twice per
// second the level, the background noise and the zero crossing rate: use
// them to tune the constants for the room.
//
// Flash and open the serial monitor at 115200 baud.

#include <Arduino.h>
#include <SteamiIMP34DT05.h>

SteamiIMP34DT05 mic;

static const int VOICE_LED = LED_GREEN;

static const size_t FRAME_SAMPLES = 320;       // 20 ms at 16 kHz
static const float MARGIN_DB = 10.0f;          // frame level above the background
static const float MIN_VOICE_DBFS = -55.0f;    // quieter frames are never voice
static const float MAX_CROSSING_RATE = 0.25f;  // zero crossings per sample
static const uint8_t ATTACK_FRAMES = 4;        // 80 ms of voice before the LED lights
static const uint32_t HANGOVER_MS = 400;       // LED kept on through short pauses
static const uint8_t REPORT_FRAMES = 25;       // one status line every 500 ms

// The background level goes down fast when the room gets quieter, up
// slowly when it gets louder, and even more slowly during voice frames, so
// that a long speech is not taken for background noise, while a steady new
// noise (a fan) still ends up in the background.
static const float FLOOR_DOWN = 0.2f;
static const float FLOOR_UP = 0.01f;         // about 2 s
static const float FLOOR_UP_VOICE = 0.001f;  // about 20 s

int16_t frame[FRAME_SAMPLES];
size_t filled = 0;

float background = NAN;  // dBFS
uint8_t voiceRun = 0;    // voice frames in a row
bool speaking = false;
uint32_t speechStartMs = 0;
uint32_t lastVoiceMs = 0;
uint8_t framesSinceReport = 0;

// Share of consecutive samples whose sign differs (a sine at f Hz gives
// 2 * f / 16000).
float zeroCrossingRate(const int16_t* samples, size_t count) {
    size_t crossings = 0;
    for (size_t i = 1; i < count; i++) {
        if ((samples[i - 1] < 0) != (samples[i] < 0)) {
            crossings++;
        }
    }
    return static_cast<float>(crossings) / static_cast<float>(count - 1);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    pinMode(VOICE_LED, OUTPUT);
    digitalWrite(VOICE_LED, LOW);

    if (!mic.begin()) {
        Serial.println("Microphone not detected - check that this is a STeaMi board.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Speak near the board.");
}

void loop() {
    // The driver keeps 64 ms of samples: read them often enough.
    filled += mic.read(frame + filled, FRAME_SAMPLES - filled);
    if (filled < FRAME_SAMPLES) {
        return;
    }
    filled = 0;

    float dbfs = SteamiIMP34DT05::rmsToDbfs(SteamiIMP34DT05::computeRms(frame, FRAME_SAMPLES));
    float crossingRate = zeroCrossingRate(frame, FRAME_SAMPLES);
    uint32_t now = millis();

    if (isnan(background)) {
        background = dbfs;  // first frame
    }

    bool loud = dbfs >= background + MARGIN_DB && dbfs >= MIN_VOICE_DBFS;
    bool voiced = crossingRate <= MAX_CROSSING_RATE;
    bool voiceFrame = loud && voiced;

    // Follow the background noise.
    if (dbfs < background) {
        background += FLOOR_DOWN * (dbfs - background);
    } else {
        background += (voiceFrame ? FLOOR_UP_VOICE : FLOOR_UP) * (dbfs - background);
    }

    if (voiceFrame) {
        if (voiceRun < ATTACK_FRAMES) {
            voiceRun++;
        }
        if (voiceRun >= ATTACK_FRAMES) {
            lastVoiceMs = now;
            if (!speaking) {
                speaking = true;
                speechStartMs = now;
                digitalWrite(VOICE_LED, HIGH);
                Serial.println("Voice");
            }
        }
    } else {
        voiceRun = 0;
    }

    if (speaking && now - lastVoiceMs >= HANGOVER_MS) {
        speaking = false;
        digitalWrite(VOICE_LED, LOW);
        Serial.print("Silence (spoke for ");
        Serial.print(lastVoiceMs - speechStartMs);
        Serial.println(" ms)");
    }

    if (++framesSinceReport >= REPORT_FRAMES) {
        framesSinceReport = 0;
        Serial.print("  level ");
        Serial.print(dbfs, 1);
        Serial.print(" dBFS  background ");
        Serial.print(background, 1);
        Serial.print(" dBFS  zero crossings ");
        Serial.println(crossingRate, 2);
    }
}
