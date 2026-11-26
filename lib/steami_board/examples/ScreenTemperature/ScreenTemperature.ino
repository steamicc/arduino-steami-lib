// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - show the temperature on the screen, as mockup 01 of the tutorials
// (01_temperature_ref.png): a title, the value in large digits with its unit, and the name of
// the sensor below.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels and a
// WSEN-HIDS sensor, V1c a 240 px colour screen and a BME280. The screen lays itself out for the
// panel it finds.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the sensor does not answer, the screen shows "--" in place of the value and no sensor name.

#include <Arduino.h>
#include <SteamiScreen.h>
#include <SteamiTemperature.h>

SteamiScreen screen;
SteamiTemperature temperature;

void setup() {
    Serial.begin(115200);  // the screen and the sensor report their errors here
    screen.begin();
    temperature.begin();
}

void loop() {
    screen.clear();
    screen.title("Temperature");
    screen.value(temperature.read(), "°C");
    screen.subtitle(temperature.chip());  // "WSEN-HIDS" on V1a/V1b, "BME280" on V1c
    screen.show();
    delay(500);
}
