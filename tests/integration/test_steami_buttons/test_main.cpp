// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Interactive integration tests for SteamiButtons: taps caught between two slow calls (issue #251).
 *
 * **REQUIRES A HUMAN.** Each test prints a prompt to Serial @115200 and lights LED_GREEN while it
 * waits for you. The whole run takes about five minutes. These tests fail unattended: they live
 * under tests/integration/ so that `make ci` leaves them out. Run them with
 * `make test-integration/steami_buttons` and the serial monitor open.
 *
 * - The expander does not take EXTI line 0: an interrupt the sketch attaches to USER_BTN (MENU)
 *   before the expander is built keeps working.
 * - Registering the four D-PAD callbacks five times in a row gives no event.
 * - Ten quick taps on each of the seven buttons, with a loop that calls the buttons every 300 ms,
 *   give exactly ten presses and ten releases on that button, and no press on the others.
 * - Ten long presses (more than one second) on A and on UP give ten presses and ten releases, and
 *   isPressed() stays true while the button is held.
 * - 60 s of fast presses on the D-PAD: the levels the driver reports alternate on each pin, and
 *   the last one is the level of the pin.
 *
 * The MicroPython Buttons class is to pass the same scenario: same buttons, same 300 ms loop,
 * same counts.
 */

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <unity.h>

static constexpr uint32_t LOOP_MS = 300;  // a slow loop, like WeatherStation's
static constexpr uint32_t TAPS = 10;      // per button
static constexpr uint32_t TAP_WINDOW_MS = 30000;
static constexpr uint32_t PROMPT_TIMEOUT_MS = 5000;
static constexpr uint32_t LONG_PRESS_MS = 1000;
static constexpr uint32_t STORM_MS = 60000;

static const SteamiButton kButtons[7] = {SteamiButton::A,    SteamiButton::B,    SteamiButton::MENU,
                                         SteamiButton::UP,   SteamiButton::DOWN, SteamiButton::LEFT,
                                         SteamiButton::RIGHT};
static const char* const kNames[7] = {"A", "B", "MENU", "UP", "DOWN", "LEFT", "RIGHT"};
static const uint8_t kDpadBits[4] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                                     MCP23009_BTN_RIGHT};

void setUp(void) {}

void tearDown(void) {
    digitalWrite(LED_GREEN, LOW);
    Steami.end();
}

static void prompt(const char* text) {
    TEST_MESSAGE(text);
    digitalWrite(LED_GREEN, HIGH);
}

// --- EXTI line 0 stays with MENU ---

static volatile bool gMenuFired = false;

static void onMenu() {
    gMenuFired = true;
}

void test_user_btn_interrupt_survives_the_expander(void) {
    gMenuFired = false;
    attachInterrupt(digitalPinToInterrupt(USER_BTN), onMenu, FALLING);
    TEST_ASSERT_NOT_NULL(Steami.mcp23009e());  // builds the expander after the attach
    prompt("Press MENU once.");
    const uint32_t start = millis();
    while (!gMenuFired && millis() - start < PROMPT_TIMEOUT_MS) {
        delay(10);
    }
    detachInterrupt(digitalPinToInterrupt(USER_BTN));
    TEST_ASSERT_TRUE_MESSAGE(gMenuFired, "the USER_BTN interrupt did not fire");
}

// --- Registration gives no event ---

void test_dpad_registration_five_times_gives_no_event(void) {
    SteamiMCP23009E* expander = Steami.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    int events = 0;
    for (int round = 0; round < 5; round++) {
        for (uint8_t bit : kDpadBits) {
            expander->setup(bit, MCP23009_DIR_INPUT, MCP23009_PULLUP);
            expander->interruptOnChange(bit, [&events](uint8_t) { events++; });
        }
        expander->poll();
    }
    for (uint8_t bit : kDpadBits) {
        expander->disableInterrupt(bit);
    }
    TEST_ASSERT_EQUAL_MESSAGE(0, events, "no button touched: no interrupt event");
}

// --- Quick taps with a slow loop ---

static void countTapsOn(int target) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    char text[96];
    snprintf(text, sizeof(text),
             "Tap %s %u times, quickly (under 150 ms), about once per second. Touch nothing else.",
             kNames[target], static_cast<unsigned>(TAPS));
    prompt(text);
    uint32_t presses[7] = {};
    uint32_t releases[7] = {};
    const uint32_t start = millis();
    uint32_t doneAt = 0;
    for (;;) {
        for (int k = 0; k < 7; k++) {
            presses[k] += buttons.wasPressed(kButtons[k]) ? 1 : 0;
            releases[k] += buttons.wasReleased(kButtons[k]) ? 1 : 0;
        }
        if (presses[target] >= TAPS && doneAt == 0) {
            doneAt = millis();
            digitalWrite(LED_GREEN, LOW);
        }
        // One second more after the last tap, for its release and for a tap too many.
        if ((doneAt != 0 && millis() - doneAt >= 1000) || millis() - start >= TAP_WINDOW_MS) {
            break;
        }
        delay(LOOP_MS);
    }
    for (int k = 0; k < 7; k++) {
        snprintf(text, sizeof(text), "%s: %u presses, %u releases", kNames[k],
                 static_cast<unsigned>(presses[k]), static_cast<unsigned>(releases[k]));
        TEST_MESSAGE(text);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TAPS, presses[target], "presses of the tapped button");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TAPS, releases[target], "releases of the tapped button");
    for (int k = 0; k < 7; k++) {
        if (k != target) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, presses[k], "press on a button nobody touched");
        }
    }
    TEST_ASSERT_FALSE(buttons.isPressed(kButtons[target]));
}

void test_quick_taps_on_a(void) {
    countTapsOn(0);
}
void test_quick_taps_on_b(void) {
    countTapsOn(1);
}
void test_quick_taps_on_menu(void) {
    countTapsOn(2);
}
void test_quick_taps_on_up(void) {
    countTapsOn(3);
}
void test_quick_taps_on_down(void) {
    countTapsOn(4);
}
void test_quick_taps_on_left(void) {
    countTapsOn(5);
}
void test_quick_taps_on_right(void) {
    countTapsOn(6);
}

// --- Long presses ---

static void countLongPressesOn(int target) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    char text[96];
    snprintf(text, sizeof(text), "Press and hold %s for more than one second, %u times.",
             kNames[target], static_cast<unsigned>(TAPS));
    prompt(text);
    uint32_t presses = 0;
    uint32_t releases = 0;
    uint32_t heldLong = 0;  // presses during which isPressed() stayed true for one second
    uint32_t heldSince = 0;
    bool counted = false;
    const uint32_t start = millis();
    while (releases < TAPS && millis() - start < 4 * TAP_WINDOW_MS) {
        if (buttons.wasPressed(kButtons[target])) {
            presses++;
            heldSince = millis();
            counted = false;
        }
        if (buttons.isPressed(kButtons[target])) {
            if (!counted && millis() - heldSince >= LONG_PRESS_MS) {
                heldLong++;
                counted = true;
            }
        }
        releases += buttons.wasReleased(kButtons[target]) ? 1 : 0;
        delay(LOOP_MS);
    }
    snprintf(text, sizeof(text), "%s: %u presses, %u releases, %u held 1 s", kNames[target],
             static_cast<unsigned>(presses), static_cast<unsigned>(releases),
             static_cast<unsigned>(heldLong));
    TEST_MESSAGE(text);
    TEST_ASSERT_EQUAL_UINT32(TAPS, presses);
    TEST_ASSERT_EQUAL_UINT32(TAPS, releases);
    TEST_ASSERT_EQUAL_UINT32(TAPS, heldLong);
}

void test_long_presses_on_a(void) {
    countLongPressesOn(0);
}
void test_long_presses_on_up(void) {
    countLongPressesOn(3);
}

// --- Driver: levels alternate under fast presses ---

void test_dpad_levels_alternate_under_fast_presses(void) {
    SteamiMCP23009E* expander = Steami.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    int last[4] = {-1, -1, -1, -1};
    uint32_t events[4] = {};
    bool alternate = true;
    for (uint8_t d = 0; d < 4; d++) {
        expander->setup(kDpadBits[d], MCP23009_DIR_INPUT, MCP23009_PULLUP);
        expander->interruptOnChange(kDpadBits[d], [d, &last, &events, &alternate](uint8_t level) {
            if (last[d] == static_cast<int>(level)) {
                alternate = false;
            }
            last[d] = level;
            events[d]++;
        });
    }
    prompt("For 60 s, press the four D-PAD buttons as fast as you can, together or not.");
    const uint32_t start = millis();
    while (millis() - start < STORM_MS) {
        expander->poll();  // no interrupt pin: reads the chip latch at each call
        delay(LOOP_MS / 10);
    }
    digitalWrite(LED_GREEN, LOW);
    TEST_MESSAGE("Stop and release every button.");
    delay(2000);
    expander->poll();
    char text[96];
    for (uint8_t d = 0; d < 4; d++) {
        snprintf(text, sizeof(text), "%s: %u events", kNames[3 + d],
                 static_cast<unsigned>(events[d]));
        TEST_MESSAGE(text);
    }
    for (uint8_t d = 0; d < 4; d++) {
        expander->disableInterrupt(kDpadBits[d]);
    }
    TEST_ASSERT_TRUE_MESSAGE(alternate, "two events in a row with the same level");
    for (uint8_t d = 0; d < 4; d++) {
        if (last[d] >= 0) {
            TEST_ASSERT_EQUAL_INT_MESSAGE(expander->getLevel(kDpadBits[d]), last[d],
                                          "last event level differs from the pin");
        }
    }
}

void setup() {
    delay(2000);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    UNITY_BEGIN();
    RUN_TEST(test_user_btn_interrupt_survives_the_expander);
    RUN_TEST(test_dpad_registration_five_times_gives_no_event);
    RUN_TEST(test_quick_taps_on_a);
    RUN_TEST(test_quick_taps_on_b);
    RUN_TEST(test_quick_taps_on_menu);
    RUN_TEST(test_quick_taps_on_up);
    RUN_TEST(test_quick_taps_on_down);
    RUN_TEST(test_quick_taps_on_left);
    RUN_TEST(test_quick_taps_on_right);
    RUN_TEST(test_long_presses_on_a);
    RUN_TEST(test_long_presses_on_up);
    RUN_TEST(test_dpad_levels_alternate_under_fast_presses);
    UNITY_END();
}

void loop() {}
