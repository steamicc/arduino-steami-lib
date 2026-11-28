// SPDX-License-Identifier: GPL-3.0-or-later
//
// MelodyPlayer — play a different 8-bit melody on the on-board buzzer
// depending on which D-PAD button is pressed: UP plays Tetris, DOWN plays
// Mario, LEFT plays Zelda and RIGHT plays Pokemon.
//
// The MCP23009E sits on the STeaMi internal I2C bus; the driver uses the
// board's bus, Steami.wire().
// Open the serial monitor at 115200 baud to see the active button.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiMCP23009E.h>

#include <functional>
#include <map>

#define NOTE_E4 330
#define NOTE_G4 392
#define NOTE_C5 523
#define NOTE_E5 659
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_B5 988
#define NOTE_F5 698
#define NOTE_A4 440
#define NOTE_F4 349
#define NOTE_D5 587
#define NOTE_B4 494

int tetrisMelody[] = {NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_A4,
                      NOTE_A4, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_C5,
                      NOTE_D5, NOTE_E5, NOTE_C5, NOTE_A4, NOTE_A4};
int tetrisDurations[] = {200, 100, 100, 200, 100, 100, 200, 100, 100, 200,
                         100, 100, 150, 100, 200, 200, 200, 200, 400};

int marioMelody[] = {NOTE_E5, NOTE_E5, 0, NOTE_E5, 0, NOTE_C5, NOTE_E5, 0, NOTE_G5, 0, NOTE_G4, 0};
int marioDurations[] = {150, 150, 150, 150, 150, 150, 150, 150, 300, 300, 300, 300};

int zeldaMelody[] = {NOTE_A4, NOTE_F4, NOTE_C5, NOTE_A4, NOTE_F4,
                     NOTE_C5, NOTE_A4, NOTE_G4, NOTE_E4};
int zeldaDurations[] = {300, 300, 400, 300, 300, 400, 200, 200, 600};

int pokemonMelody[] = {NOTE_D5, NOTE_D5, NOTE_D5, NOTE_G4, NOTE_D5, NOTE_C5, NOTE_B4,
                       NOTE_A4, NOTE_G4, NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G4};
int pokemonDurations[] = {200, 200, 200, 400, 200, 200, 200, 400, 200, 200, 200, 200, 600};

void playTetris() {
    for (int i = 0; i < 19; i++) {
        tone(SPEAKER, tetrisMelody[i], tetrisDurations[i]);
        delay(tetrisDurations[i] * 1.3);
        noTone(SPEAKER);
    }
}

void playPokemon() {
    for (int i = 0; i < 13; i++) {
        tone(SPEAKER, pokemonMelody[i], pokemonDurations[i]);
        delay(pokemonDurations[i] * 1.3);
        noTone(SPEAKER);
    }
}

void playZelda() {
    for (int i = 0; i < 9; i++) {
        tone(SPEAKER, zeldaMelody[i], zeldaDurations[i]);
        delay(zeldaDurations[i] * 1.3);
        noTone(SPEAKER);
    }
}

void playMario() {
    for (int i = 0; i < 12; i++) {
        if (marioMelody[i] == 0) {
            noTone(SPEAKER);
        } else {
            tone(SPEAKER, marioMelody[i], marioDurations[i]);
        }
        delay(marioDurations[i] * 1.3);
        noTone(SPEAKER);
    }
}

static const std::map<uint8_t, std::function<void()>> kMelodies = {
    {MCP23009_BTN_UP, playTetris},
    {MCP23009_BTN_DOWN, playMario},
    {MCP23009_BTN_LEFT, playZelda},
    {MCP23009_BTN_RIGHT, playPokemon},
};

// No interrupt pin: INT_EXPANDER (PB0) shares EXTI line 0 with MENU_BUTTON (PA0).
SteamiMCP23009E expander(Steami.wire(), RST_EXPANDER, MCP23009_I2C_ADDR);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    for (const auto& [pinNumber, melody] : kMelodies) {
        expander.setup(pinNumber, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    }

    expander.begin();

    Serial.println("MusicPlayer — UP: Tetris, DOWN: Mario, LEFT: Zelda, RIGHT: Pokemon.");
}

void loop() {
    for (const auto& [pinNumber, melody] : kMelodies) {
        if (expander.getLevel(pinNumber) == MCP23009_LOGIC_LOW) {
            melody();
            break;
        }
    }
    delay(100);
}
