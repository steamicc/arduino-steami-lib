// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - a weather station on four screen pages:
// - Temperature: the temperature in the middle, as mockup 01 of the tutorials;
// - Humidity: the humidity on a blue gauge, as mockup 04;
// - Pressure: a graph of the last 40 pressures from 950 to 1050 hPa, as mockup 05;
// - Compass: the compass rose, with its needle to the north, as mockup 07.
// RIGHT or A shows the next page, LEFT or B the previous one. On V1a/V1b a hand swiped to the
// right above the gesture sensor also shows the next page.
//
// The same file runs on V1a, V1b and V1c: V1a/V1b have a 128 px screen with 16 grey levels, V1c
// a 240 px colour screen, and the screen lays itself out for the panel it finds. The blue gauge is
// drawn in grey 10 on the grey screen, so that it stands out from its track. V1c has no gesture
// sensor: there gesture.begin() prints an E20 message and returns false, and the sketch goes on
// with the buttons alone.
//
// SteamiHistory<40> keeps the last 40 pressures in the sketch's memory, with no heap: about one
// reading every 200 ms, so the curve shows about the last 8 seconds and moves to the left.
//
// Unlike the earlier weather station program of the functional layer, no face sits over the gauge
// page (it hid the value) and no direction text sits over the compass rose (the rose fills the
// screen).
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If a sensor does not answer, its reading is NAN: the page shows "--" in place of the value, the
// curve leaves a gap, and the compass draws its rose with no needle.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <SteamiCompass.h>
#include <SteamiGesture.h>
#include <SteamiHumidity.h>
#include <SteamiPressure.h>
#include <SteamiScreen.h>
#include <SteamiTemperature.h>

SteamiScreen screen;
SteamiTemperature temperature;
SteamiHumidity humidity;
SteamiPressure pressure;
SteamiCompass compass;
SteamiButtons buttons;
SteamiGesture gesture;  // V1a/V1b only

SteamiHistory<40> history;  // the last 40 pressures, no heap
const char* const PAGES[] = {"Temperature", "Humidity", "Pressure", "Compass"};
int page = 0;
bool hasGesture = false;

void setup() {
    Serial.begin(115200);  // the screen and the sensors report their errors here
    screen.begin();
    temperature.begin();
    humidity.begin();
    pressure.begin();
    compass.begin();
    buttons.begin();
    hasGesture = gesture.begin();  // on V1c: prints E20 and returns false; the sketch goes on
    Steami.printInfo();
}

void loop() {
    if (buttons.wasPressed(SteamiButton::RIGHT) || buttons.wasPressed(SteamiButton::A)) {
        page = (page + 1) % 4;
    }
    if (buttons.wasPressed(SteamiButton::LEFT) || buttons.wasPressed(SteamiButton::B)) {
        page = (page + 3) % 4;
    }
    if (hasGesture && gesture.read() == SteamiGesture::RIGHT) {
        page = (page + 1) % 4;
    }
    history.add(pressure.read());
    screen.clear();
    if (page == 0) {
        screen.title(PAGES[page]);
        screen.value(temperature.read(), "°C");
    } else if (page == 1) {
        float h = humidity.read();
        screen.gauge(h, 0, 100, STEAMI_BLUE);  // lifted to grey 10 on the OLED (5.10)
        screen.title(PAGES[page]);
        screen.value(h, "%");
    } else if (page == 2) {
        screen.title(PAGES[page]);
        screen.graph(history, 950, 1050, STEAMI_RED);  // labels "1050" and "950", inside the circle
        screen.subtitle("hPa");
    } else {
        screen.compass(compass.heading());  // the rose fills the screen: no extra text
    }
    screen.show();
    delay(200);
}
