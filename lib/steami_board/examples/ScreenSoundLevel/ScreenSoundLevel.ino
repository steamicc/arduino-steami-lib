// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - a sound level meter on the screen: a round gauge filled from 30 to 100 dB SPL, a
// title, the level in large digits with its unit, and the name of the microphone below.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen; all of them have the IMP34DT05 PDM microphone. The screen lays itself out
// for the panel it finds.
//
// The level is the estimate of read() over a 200 ms window, not calibrated: a quiet room reads
// about 30 to 50 dB, a voice close to the board 70 dB or more. The gauge turns red above 85 dB.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the microphone sends no sound, read() gives NAN: the gauge stays empty and the screen shows
// "--".

#include <Arduino.h>
#include <SteamiMicrophone.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiMicrophone microphone;

void setup() {
    Serial.begin(115200);  // the screen and the microphone report their errors here
    screen.begin();
    microphone.setWindow(200);
    microphone.begin();
}

void loop() {
    const float db = microphone.read();  // the last 200 ms window, never waits once started
    screen.clear();
    screen.gauge(db, 30, 100, db > 85.0f ? STEAMI_RED : STEAMI_LIGHT);  // first: text paints over
    screen.title("Sound level");
    screen.value(db, "dB");
    screen.subtitle(microphone.chip());  // "IMP34DT05"
    screen.show();
    delay(200);
}
