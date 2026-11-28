// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print the sound level five times per second (115200 baud).
//
// Works on every release: the IMP34DT05 PDM microphone is on the V1a, the V1b
// and the V1c. read() gives the level of the last 200 ms window in dB SPL,
// estimated from the datasheet sensitivity and not calibrated: a quiet room
// reads about 30 to 50 dB, a voice close to the board 70 dB or more. The
// dominant frequency is printed when the sound is above 50 dB SPL.
//
// If the microphone sends no sound, begin() returns false and error() says
// why; the library also logs one message with its code.

#include <Arduino.h>
#include <SteamiMicrophone.h>

SteamiMicrophone microphone;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    microphone.setWindow(200);  // a steady level, as a sound level meter
    if (microphone.begin()) {
        Serial.print("Microphone: ");
        Serial.println(microphone.chip());
    } else {
        Serial.print("No microphone: ");
        Serial.println(microphone.error());
    }
}

void loop() {
    if (!microphone.levelReady()) {
        return;  // a new window every 200 ms
    }
    const float db = microphone.read();
    if (isnan(db)) {
        return;
    }
    Serial.print(db, 1);
    Serial.print(" dB SPL, peak ");
    Serial.print(microphone.peak());
    const float hz = microphone.frequency();  // NAN below 50 dB SPL
    if (!isnan(hz)) {
        Serial.print(", ");
        Serial.print(hz, 0);
        Serial.print(" Hz");
    }
    Serial.println();
}
