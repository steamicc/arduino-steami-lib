// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - an analog watch on the screen, as mockup 09 of the tutorials (09_watch_ref.png):
// a dial with twelve ticks and the numbers 12, 3, 6 and 9, the hour and minute hands, and a
// second hand.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen. The screen lays itself out for the panel it finds.
//
// The board has no clock that keeps the time: the start time is set by hand below (10:10:30, as
// mockup 09), and the watch counts the seconds since reset with millis(). Change START to the
// time at which you reset the board.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.

#include <Arduino.h>
#include <SteamiScreen.h>

SteamiScreen screen;

const uint32_t START = 10 * 3600UL + 10 * 60 + 30;  // start time set by hand: 10:10:30

void setup() {
    Serial.begin(115200);  // the screen reports its errors here
    screen.begin();
}

void loop() {
    uint32_t s = START + millis() / 1000;
    screen.clear();
    screen.watch((s / 3600) % 24, (s / 60) % 60, s % 60);
    screen.show();
    delay(500);
}
