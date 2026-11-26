// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - show temperature and humidity side by side, as mockup 03 of the tutorials
// (03_comfort_dual_ref.png): a title, the two values with their labels and units on either side
// of a line, and "Comfortable" in green when both are in the comfort zone (19 to 24 degrees
// Celsius, 40 to 60 % humidity), above the name of the sensor.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels and a
// WSEN-HIDS sensor, V1c a 240 px colour screen and a BME280. The screen lays itself out for the
// panel it finds.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If a sensor does not answer, the screen shows "--" in place of its value, "Uncomfortable", and
// no sensor name.

#include <Arduino.h>
#include <SteamiHumidity.h>
#include <SteamiScreen.h>
#include <SteamiTemperature.h>

SteamiScreen screen;
SteamiTemperature temperature;
SteamiHumidity humidity;

void setup() {
    Serial.begin(115200);  // the screen and the sensors report their errors here
    screen.begin();
    temperature.begin();
    humidity.begin();
}

void loop() {
    float t = temperature.read(), h = humidity.read();
    bool comfortable = t >= 19 && t <= 24 && h >= 40 && h <= 60;
    screen.clear();
    screen.title("Comfort");
    screen.vline(screen.centerX(), screen.centerY() - screen.radius() / 2, screen.radius(),
                 STEAMI_DARK);
    screen.value(t, "°C", SteamiAt::W, "TEMP");
    screen.value(h, "%", SteamiAt::E, "HUM");  // a float: "45.3", and "--" if the sensor is missing
    screen.subtitle(comfortable ? "Comfortable" : "Uncomfortable", humidity.chip(), STEAMI_GREEN);
    screen.show();
    delay(1000);
}
