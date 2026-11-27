// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - a menu moved with the D-pad, as mockup 06 of the tutorials
// (06_dpad_menu_ref.png): the title "Menu", the items in a column, and the selected one on a
// dark bar with a "> " marker. UP and DOWN move the bar. The index wraps: UP on the first item
// selects the last one, DOWN on the last selects the first.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels and
// show 5 of the 6 items at a time, V1c a 240 px colour screen that shows all 6. The list scrolls
// so that the selected item stays in view. The screen lays itself out for the panel it finds.
//
// menu() returns the wrapped index: keep it in an int. With an unsigned, long or size_t index,
// the call would pick the other menu(), which takes the number of items next.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the D-pad does not answer, its buttons read as released: the menu stays on its first item.

#include <Arduino.h>
#include <SteamiButtons.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiButtons buttons;

const char* const ITEMS[] = {"Temperature", "Humidity", "Distance",
                             "Light",       "Battery",  "Proximity"};
int selected = 0;

void setup() {
    Serial.begin(115200);  // the screen and the D-pad report their errors here
    screen.begin();
    buttons.begin();
}

void loop() {
    if (buttons.wasPressed(SteamiButton::UP)) {
        selected--;
    }
    if (buttons.wasPressed(SteamiButton::DOWN)) {
        selected++;
    }
    screen.clear();
    screen.title("Menu");
    selected = screen.menu(ITEMS, selected);  // wraps: -1 becomes 5
    screen.show();
    delay(20);
}
