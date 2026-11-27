// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - a smiley that reacts to the distance in front of the board, as mockup 08 of the
// tutorials (08_smiley_reactive_ref.png): the title "Mood", a face, and the mood below in its
// colour. Closer than 50 mm it is surprised, then happy up to 150 mm, sleeping up to 300 mm, and
// sad beyond. Button A shows the six faces one by one, full screen (08_smiley_happy_ref.png and
// the five others), then the mood again.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen; both read the VL53L1X time-of-flight sensor. The screen lays itself out
// for the panel it finds.
//
// Hold A until the face changes: drawing a screen takes some time, and a short tap between two
// reads of the button can be missed.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the sensor does not answer, read() gives -1: the mood shows SURPRISED.

#include <Arduino.h>
#include <SteamiButtons.h>
#include <SteamiDistance.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiDistance distance;
SteamiButtons buttons;

const SteamiFace FACES[] = {SteamiFace::HAPPY,    SteamiFace::SAD,   SteamiFace::SURPRISED,
                            SteamiFace::SLEEPING, SteamiFace::ANGRY, SteamiFace::LOVE};
int shown = 6;  // 0..5: FACES[shown] alone; 6: the mood of the distance

void setup() {
    Serial.begin(115200);  // the screen and the sensor report their errors here
    screen.begin();
    distance.begin();
    buttons.begin();
}

void loop() {
    if (buttons.wasPressed(SteamiButton::A)) {
        shown = (shown + 1) % 7;
    }
    int d = distance.read();
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
    delay(50);  // read the buttons at least every 50 ms
}
