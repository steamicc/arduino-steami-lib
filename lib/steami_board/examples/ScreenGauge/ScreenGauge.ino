// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - show the distance in front of the board on a round gauge, as mockup 04 of the
// tutorials (04_circular_gauge_ref.png): a ring filled from 0 to 500 mm, a title, the distance in
// large digits with its unit, and the name of the sensor below.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen; both read the VL53L1X time-of-flight sensor. The screen lays itself out
// for the panel it finds.
//
// gauge() draws the ring and its "0" and "500" labels only: value() shows the number. The gauge
// comes first, so the text paints over the ring.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the sensor does not answer, read() gives -1: the gauge stays empty and the screen shows
// "-1 mm".

#include <Arduino.h>
#include <SteamiDistance.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiDistance distance;

void setup() {
    Serial.begin(115200);  // the screen and the sensor report their errors here
    screen.begin();
    distance.begin();
}

void loop() {
    int d = distance.read();
    screen.clear();
    screen.gauge(d, 0, 500);  // first: text paints over the arc
    screen.title("Distance");
    screen.value(d, "mm");
    screen.subtitle("VL53L1X ToF");
    screen.show();
    delay(200);
}
