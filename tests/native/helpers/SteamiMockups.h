// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "SteamiScreen.h"

// The drawing code of mockups 01 to 09 (spec section 6), shared by the native mockups suite. Each
// function is the loop() body of its example up to show(), the sensor readings taken as
// parameters; a script checks that the examples make the same screen calls with the same literals.
// The 240 px colour renders are the references for the V1c photos.

/// 01 Temperature (ScreenTemperature): t in degrees Celsius, NAN when the sensor is missing.
inline void mockupTemperature(SteamiScreen& screen, float t, const char* chip) {
    screen.clear();
    screen.title("Temperature");
    screen.value(t, "°C");
    screen.subtitle(chip);  // "WSEN-HIDS" on V1a/V1b, "BME280" on V1c
    screen.show();
}

/// 02 Battery (ScreenBattery): percent 0 and v NAN when the gauge does not answer.
inline void mockupBattery(SteamiScreen& screen, uint8_t percent, float v) {
    char volts[12] = "--";
    if (!isnan(v)) {
        snprintf(volts, sizeof volts, "%d mV", (int)lroundf(v * 1000.0f));
    }
    screen.clear();
    screen.title("Battery");
    // yOffset -12 (128-px units) for both, as the tutorial:
    // value, bar and two subtitle lines fit together
    screen.value(percent, "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, -12);
    screen.bar(percent, 100, -12, STEAMI_GREEN);  // rows 72..79 on 128, as mockup 02
    screen.subtitle(volts, "BQ27441");
    screen.show();
}

/// 03 Comfort, two values (ScreenComfort): t and h NAN when a sensor is missing.
inline void mockupComfort(SteamiScreen& screen, float t, float h, const char* chip) {
    bool comfortable = t >= 19 && t <= 24 && h >= 40 && h <= 60;
    screen.clear();
    screen.title("Comfort");
    screen.vline(screen.centerX(), screen.centerY() - screen.radius() / 2, screen.radius(),
                 STEAMI_DARK);
    screen.value(t, "°C", SteamiAt::W, "TEMP");
    screen.value(h, "%", SteamiAt::E, "HUM");  // a float: "45.3", and "--" if the sensor is missing
    screen.subtitle(comfortable ? "Comfortable" : "Uncomfortable", chip, STEAMI_GREEN);
    screen.show();
}

/// 04 Circular gauge (ScreenGauge): d in mm, -1 when the sensor is missing.
inline void mockupGauge(SteamiScreen& screen, int d) {
    screen.clear();
    screen.gauge(d, 0, 500);  // first: text paints over the arc
    screen.title("Distance");
    screen.value(d, "mm");
    screen.subtitle("VL53L1X ToF");
    screen.show();
}

/// 05 Scrolling graph (ScreenGraph): the last 20 readings, of the light sensor when hasLight, else
/// of the pressure sensor (V1c); chip is that sensor's chip().
inline void mockupGraph(SteamiScreen& screen, const SteamiHistory<20>& history, bool hasLight,
                        const char* chip) {
    screen.clear();
    screen.title(hasLight ? "Light" : "Pressure");  // a raw light count, not lux: no "(lux)"
    if (hasLight) {
        screen.graph(history, 0, 1000);  // axis labels "1000" and "0"
    } else {
        screen.graph(history, 950, 1050);  // axis labels "1050" and "950"
    }
    screen.subtitle(chip, "20s window");
    screen.show();
}

/// The six items of program 06, which ScreenMenu shows.
constexpr const char* const MENU_ITEMS[6] = {"Temperature", "Humidity", "Distance",
                                             "Light",       "Battery",  "Proximity"};

/// 06 D-pad menu (ScreenMenu): selected is the index after the buttons, wrapped in place.
inline void mockupMenu(SteamiScreen& screen, int& selected) {
    screen.clear();
    screen.title("Menu");
    selected = screen.menu(MENU_ITEMS, selected);  // wraps: -1 becomes 5
    screen.show();
}

/// 07 Compass (ScreenCompass): heading in degrees, NAN when the sensor is missing.
inline void mockupCompass(SteamiScreen& screen, float heading) {
    screen.clear();
    screen.compass(heading);  // the needle points to real north
    screen.show();
}

/// The six faces in SteamiFace order, which button A cycles in ScreenFaces.
constexpr SteamiFace FACES[6] = {SteamiFace::HAPPY,    SteamiFace::SAD,   SteamiFace::SURPRISED,
                                 SteamiFace::SLEEPING, SteamiFace::ANGRY, SteamiFace::LOVE};

/// 08 Faces and reactive smiley (ScreenFaces): shown 0..5 draws FACES[shown] alone, 6 the mood
/// of the distance d in mm (-1 when the sensor is missing).
inline void mockupFaces(SteamiScreen& screen, int shown, int d) {
    screen.clear();
    if (shown < 6) {
        screen.face(FACES[shown]);
    } else {
        screen.title("Mood");
        // face in its default LIGHT, the mood colour on the subtitle, as mockup 08-reactive
        if (d < 50) {
            screen.face(SteamiFace::SURPRISED);
            screen.subtitle("SURPRISED", STEAMI_YELLOW);
        } else if (d < 150) {
            screen.face(SteamiFace::HAPPY);
            screen.subtitle("HAPPY", STEAMI_GREEN);
        } else if (d < 300) {
            screen.face(SteamiFace::SLEEPING);
            screen.subtitle("SLEEPING", STEAMI_LIGHT);
        } else {
            screen.face(SteamiFace::SAD);
            screen.subtitle("SAD", STEAMI_RED);
        }
    }
    screen.show();
}

/// 09 Watch (ScreenWatch): s in seconds since midnight.
inline void mockupWatch(SteamiScreen& screen, uint32_t s) {
    screen.clear();
    screen.watch((s / 3600) % 24, (s / 60) % 60, s % 60);
    screen.show();
}
