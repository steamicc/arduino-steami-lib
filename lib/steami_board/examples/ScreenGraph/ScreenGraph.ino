// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - a graph of the last 20 readings of a sensor, as mockup 05 of the tutorials
// (05_scrolling_graph_ref.png): a title, the latest reading in the middle, the curve in a box
// with its top and bottom labels, and the name of the sensor below. One reading a second, so the
// curve shows the last 20 seconds and moves to the left.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels and
// graph the raw light count of the APDS-9960 from 0 to 1000. V1c, a 240 px colour screen, has
// no light sensor: there the graph shows the pressure of the BME280 from 950 to 1050 hPa. The
// screen lays itself out for the panel it finds.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the light sensor does not answer in setup(), the graph shows the pressure. If no sensor
// answers, each reading is NAN: the curve leaves a gap there, the screen shows "--" in place of
// the latest reading, and no sensor name.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiLight.h>
#include <SteamiPressure.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiLight light;        // V1a/V1b only
SteamiPressure pressure;  // used instead on V1c
SteamiHistory<20> history;
bool hasLight = false;

void setup() {
    Serial.begin(115200);  // the screen and the sensors report their errors here
    screen.begin();
    hasLight = Steami.has(SteamiFunction::LIGHT) && light.begin();  // no E20 on V1c
    pressure.begin();
}

void loop() {
    history.add(hasLight ? light.read() : pressure.read());
    screen.clear();
    screen.title(hasLight ? "Light" : "Pressure");  // a raw light count, not lux: no "(lux)"
    if (hasLight) {
        screen.graph(history, 0, 1000);  // axis labels "1000" and "0"
    } else {
        screen.graph(history, 950, 1050);  // axis labels "1050" and "950"
    }
    screen.subtitle(hasLight ? light.chip() : pressure.chip(), "20s window");
    screen.show();
    delay(1000);
}
