// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Interactive integration tests for SteamiButtons: taps caught between two slow calls (issues #251
 * and #341).
 *
 * **REQUIRES A HUMAN.** Each step prints a prompt to Serial @115200 and shows it, in French, on
 * the screen: a 3-2-1 countdown, then "À vous !". No LED is lit. The whole run takes about six
 * minutes. These tests fail unattended: they live under tests/integration/ so that `make ci`
 * leaves them out. Run them with `make test-integration/steami_buttons` and the serial monitor
 * open.
 *
 * - The expander does not take EXTI line 0: an interrupt the sketch attaches to USER_BTN (MENU)
 *   before the expander is built keeps working.
 * - Registering the four D-PAD callbacks five times in a row gives no event.
 * - The EXTI lines of A, B and MENU are routed to their pins, unmasked on both edges, with their
 *   NVIC interrupt enabled, after a screen refresh too.
 * - Ten quick taps on each of the seven buttons, in a fixed window of 15 s, with a loop that calls
 *   the buttons every 300 ms: exactly ten presses and ten releases on that button, and no press
 *   on the others. The test counts for the whole window, it does not stop at ten. For A, B and
 *   MENU, a 1 kHz timer that reads the pins counts the taps independently ("au doigt"): the
 *   presses reported must match it.
 * - Ten long presses (two seconds) on A and on UP: ten presses and ten releases, each held more
 *   than one second between its press and its release, and isPressed() true at every call in
 *   between.
 * - 60 s of fast presses on the D-PAD: the levels the driver reports alternate on each pin, and
 *   the last one is the level of the pin.
 *
 * The screen is redrawn only when what it shows changes (the count, the second left), at most
 * once per tap and once per second, and the time of the redraw comes off the wait: the loop keeps
 * its 300 ms period.
 *
 * The MicroPython Buttons class is to pass the same scenario: same buttons, same 300 ms loop,
 * same counts.
 */

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <SteamiScreen.h>
#include <unity.h>

static constexpr uint32_t LOOP_MS = 300;  // a slow loop, like WeatherStation's
static constexpr uint32_t TAPS = 10;      // per button
static constexpr uint32_t TAP_WINDOW_MS = 15000;
static constexpr uint32_t PROMPT_TIMEOUT_MS = 5000;
static constexpr uint32_t LONG_PRESS_MS = 1000;  // the hold asked is two seconds
static constexpr uint32_t LONG_WINDOW_MS = 60000;
static constexpr uint32_t STORM_MS = 60000;
static constexpr uint32_t RESULT_MS = 2000;

static const SteamiButton kButtons[7] = {SteamiButton::A,    SteamiButton::B,    SteamiButton::MENU,
                                         SteamiButton::UP,   SteamiButton::DOWN, SteamiButton::LEFT,
                                         SteamiButton::RIGHT};
static const char* const kNames[7] = {"A", "B", "MENU", "UP", "DOWN", "LEFT", "RIGHT"};
static const char* const kFrench[7] = {"A", "B", "MENU", "HAUT", "BAS", "GAUCHE", "DROITE"};
static const uint8_t kDpadBits[4] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                                     MCP23009_BTN_RIGHT};

void setUp(void) {}

void tearDown(void) {
    Steami.end();
}

// --- Screen: the French instructions ---

static int gStep = 0;
static constexpr int kSteps = 13;

static void stepTitle(char* head, size_t size) {
    snprintf(head, size, "Épreuve %d/%d", gStep, kSteps);
}

// A still screen: the step, a word in the centre, two lines below.
static void showOnScreen(const char* word, const char* l1, const char* l2, SteamiRgb c) {
    SteamiScreen screen;
    if (!screen.begin()) {
        return;
    }
    char head[24];
    stepTitle(head, sizeof(head));
    screen.clear();
    screen.title(head);
    screen.text(word, SteamiAt::CENTER, c, 2);
    screen.subtitle(l1, l2, STEAMI_WHITE);
    screen.show();
}

// The prompt on Serial, then on the screen a 3-2-1 countdown and "À vous !".
static void prompt(const char* text, const char* l1, const char* l2) {
    TEST_MESSAGE(text);
    SteamiScreen screen;
    if (screen.begin()) {
        char head[24];
        stepTitle(head, sizeof(head));
        for (int n = 3; n >= 1; n--) {
            const uint32_t t0 = millis();
            screen.clear();
            screen.title(head);
            screen.value(n, nullptr, SteamiAt::CENTER, nullptr, STEAMI_YELLOW, 3);
            screen.subtitle(l1, l2, STEAMI_LIGHT);
            screen.show();
            while (millis() - t0 < 1000) {
                delay(10);
            }
        }
    }
    showOnScreen("À vous !", l1, l2, STEAMI_GREEN);
}

// Waits what is left of the turn of the loop, so that a redraw does not stretch it.
static void waitRestOfTurn(uint32_t turnStart) {
    const uint32_t spent = millis() - turnStart;
    if (spent < LOOP_MS) {
        delay(LOOP_MS - spent);
    }
}

// --- Reference: the taps seen by a 1 kHz timer on the A, B and MENU pins ---

// A 10 ms stable level changes the state; a change to pressed counts a tap. Independent of
// SteamiButtons and of the EXTI.
static HardwareTimer* gRefTimer = nullptr;
static volatile uint32_t gRefTaps = 0;
static uint32_t gRefPin = 0;
static uint8_t gRefStable = 0;  // samples at the same level
static bool gRefRaw = false;
static bool gRefDown = false;

static void refSample() {
    const bool down = digitalRead(gRefPin) == LOW;
    if (down != gRefRaw) {
        gRefRaw = down;
        gRefStable = 0;
        return;
    }
    if (gRefStable < 10) {
        gRefStable++;
        return;
    }
    if (down != gRefDown) {
        gRefDown = down;
        if (down) {
            gRefTaps = gRefTaps + 1;
        }
    }
}

static void startReference(uint32_t pin) {
    if (gRefTimer == nullptr) {
        gRefTimer = new HardwareTimer(TIM2);
        gRefTimer->setOverflow(1000, HERTZ_FORMAT);
        gRefTimer->attachInterrupt(refSample);
    }
    gRefTimer->pause();
    gRefPin = pin;
    gRefRaw = digitalRead(pin) == LOW;
    gRefDown = gRefRaw;
    gRefStable = 10;
    gRefTaps = 0;
    gRefTimer->resume();
}

static uint32_t stopReference() {
    gRefTimer->pause();
    return gRefTaps;
}

// --- EXTI lines of A, B and MENU ---

// The EXTI line of `pin` is routed to its port (SYSCFG), unmasked on both edges (CHANGE) and its
// NVIC interrupt is enabled. A line turned off (a detachInterrupt() on the same IRQ) or taken by
// another port would leave that button sampled at each call only.
static void checkExtiLine(uint32_t pin, const char* name) {
    const PinName pn = digitalPinToPinName(pin);
    const uint32_t line = STM_PIN(pn);
    const uint32_t port = STM_PORT(pn);
    const uint32_t mask = 1UL << line;
    const uint32_t routed = (SYSCFG->EXTICR[line >> 2] >> ((line & 3U) * 4U)) & 0x7U;
    static const IRQn_Type kLow[5] = {EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn};
    const IRQn_Type irq = line < 5 ? kLow[line] : (line < 10 ? EXTI9_5_IRQn : EXTI15_10_IRQn);
    char text[112];
    snprintf(text, sizeof(text),
             "%s: EXTI line %lu, port %lu routed %lu, IMR %d, rising %d, falling %d, NVIC %lu",
             name, static_cast<unsigned long>(line), static_cast<unsigned long>(port),
             static_cast<unsigned long>(routed), (EXTI->IMR1 & mask) != 0,
             (EXTI->RTSR1 & mask) != 0, (EXTI->FTSR1 & mask) != 0,
             static_cast<unsigned long>(NVIC_GetEnableIRQ(irq)));
    TEST_MESSAGE(text);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(port, routed, "EXTI line routed to another port");
    TEST_ASSERT_TRUE_MESSAGE((EXTI->IMR1 & mask) != 0, "EXTI line masked");
    TEST_ASSERT_TRUE_MESSAGE((EXTI->RTSR1 & mask) != 0, "EXTI rising edge off");
    TEST_ASSERT_TRUE_MESSAGE((EXTI->FTSR1 & mask) != 0, "EXTI falling edge off");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, NVIC_GetEnableIRQ(irq), "EXTI interrupt off in the NVIC");
}

static void checkMcuExti(void) {
    checkExtiLine(A_BUTTON, "A");
    checkExtiLine(B_BUTTON, "B");
    checkExtiLine(MENU_BUTTON, "MENU");
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
    gStep = 1;
    prompt("Press MENU once.", "MENU : 1 seul appui", "dans les 5 s");
    const uint32_t start = millis();
    while (!gMenuFired && millis() - start < PROMPT_TIMEOUT_MS) {
        delay(10);
    }
    detachInterrupt(digitalPinToInterrupt(USER_BTN));
    TEST_ASSERT_TRUE_MESSAGE(gMenuFired, "the USER_BTN interrupt did not fire");
}

// --- Registration gives no event ---

void test_dpad_registration_five_times_gives_no_event(void) {
    gStep = 2;
    showOnScreen("Attendez", "Ne touchez à rien", nullptr, STEAMI_LIGHT);
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

// --- The interrupts of A, B and MENU are armed ---

void test_exti_lines_of_a_b_menu_are_armed(void) {
    gStep = 3;
    showOnScreen("Attendez", "Ne touchez à rien", nullptr, STEAMI_LIGHT);
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    checkMcuExti();
    showOnScreen("Attendez", "Ne touchez à rien", "écran redessiné", STEAMI_LIGHT);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    checkMcuExti();
}

// --- Quick taps with a slow loop, in a fixed window ---

static void drawTaps(SteamiScreen& screen, const char* head, uint32_t seen, uint32_t leftS) {
    char l1[24];
    char l2[24];
    snprintf(l1, sizeof(l1), "objectif : %u", static_cast<unsigned>(TAPS));
    snprintf(l2, sizeof(l2), "reste %u s", static_cast<unsigned>(leftS));
    screen.clear();
    screen.title(head);
    screen.value(static_cast<int>(seen), "tapes vues", STEAMI_WHITE);
    screen.subtitle(l1, l2, STEAMI_LIGHT);
    screen.show();
}

static void countTapsOn(int target) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    const bool mcu = target < 3;
    char text[128];
    snprintf(text, sizeof(text),
             "Tap %s exactly %u times, quickly (under 150 ms), about once per second, within "
             "%u s. Touch nothing else.",
             kNames[target], static_cast<unsigned>(TAPS),
             static_cast<unsigned>(TAP_WINDOW_MS / 1000));
    char head[32];
    snprintf(head, sizeof(head), "%s : %u tapes brèves", kFrench[target],
             static_cast<unsigned>(TAPS));
    gStep = 4 + target;
    prompt(text, head, "une par seconde");
    // What was pressed during the countdown does not count.
    for (int k = 0; k < 7; k++) {
        while (buttons.wasPressed(kButtons[k])) {
        }
        while (buttons.wasReleased(kButtons[k])) {
        }
    }
    if (mcu) {
        startReference(target == 0 ? A_BUTTON : (target == 1 ? B_BUTTON : MENU_BUTTON));
    }
    SteamiScreen live;
    const bool liveOk = live.begin();
    uint32_t presses[7] = {};
    uint32_t releases[7] = {};
    uint32_t shownSeen = UINT32_MAX;
    uint32_t shownLeft = UINT32_MAX;
    const uint32_t start = millis();
    while (millis() - start < TAP_WINDOW_MS) {
        const uint32_t turn = millis();
        for (int k = 0; k < 7; k++) {
            presses[k] += buttons.wasPressed(kButtons[k]) ? 1 : 0;
            releases[k] += buttons.wasReleased(kButtons[k]) ? 1 : 0;
        }
        const uint32_t elapsed = millis() - start;
        const uint32_t left = elapsed < TAP_WINDOW_MS ? (TAP_WINDOW_MS - elapsed + 999) / 1000 : 0;
        if (liveOk && (presses[target] != shownSeen || left != shownLeft)) {
            shownSeen = presses[target];
            shownLeft = left;
            drawTaps(live, head, shownSeen, shownLeft);
        }
        waitRestOfTurn(turn);
    }
    // The presses and releases made in the last turn, still counted.
    for (int k = 0; k < 7; k++) {
        while (buttons.wasPressed(kButtons[k])) {
            presses[k]++;
        }
        while (buttons.wasReleased(kButtons[k])) {
            releases[k]++;
        }
    }
    const uint32_t finger = mcu ? stopReference() : 0;
    for (int k = 0; k < 7; k++) {
        snprintf(text, sizeof(text), "%s: %u presses, %u releases", kNames[k],
                 static_cast<unsigned>(presses[k]), static_cast<unsigned>(releases[k]));
        TEST_MESSAGE(text);
    }
    char l1[24];
    char l2[24];
    snprintf(l1, sizeof(l1), "tapes vues : %u", static_cast<unsigned>(presses[target]));
    if (mcu) {
        snprintf(text, sizeof(text), "%s: %u taps seen by the 1 kHz reference", kNames[target],
                 static_cast<unsigned>(finger));
        TEST_MESSAGE(text);
        snprintf(l2, sizeof(l2), "au doigt : %u", static_cast<unsigned>(finger));
    } else {
        snprintf(l2, sizeof(l2), "attendu : %u", static_cast<unsigned>(TAPS));
    }
    const bool good = presses[target] == TAPS && (!mcu || finger == TAPS);
    showOnScreen(good ? "OK" : "Écart", l1, l2, good ? STEAMI_GREEN : STEAMI_RED);
    delay(RESULT_MS);
    if (mcu) {
        checkMcuExti();  // the interrupts were still armed at the end of the window
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(finger, presses[target],
                                         "presses reported differ from the taps of the reference");
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

static void drawHold(SteamiScreen& screen, const char* head, uint32_t presses, bool holding,
                     uint32_t heldS) {
    char l1[24];
    char l2[24];
    snprintf(l1, sizeof(l1), "objectif : %u", static_cast<unsigned>(TAPS));
    if (holding) {
        snprintf(l2, sizeof(l2), "maintenu %u s", static_cast<unsigned>(heldS));
    } else {
        snprintf(l2, sizeof(l2), "relâché");
    }
    screen.clear();
    screen.title(head);
    screen.value(static_cast<int>(presses), "appuis", STEAMI_WHITE);
    screen.subtitle(l1, l2, holding ? STEAMI_GREEN : STEAMI_LIGHT);
    screen.show();
}

// The hold is measured between the call that reports the press and the one that reports the
// release: one turn of the loop of error at most, so a hold of two seconds is far above one.
// isPressed() must be true at every call in between.
static void countLongPressesOn(int target) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    char text[128];
    snprintf(text, sizeof(text),
             "Press and hold %s for two seconds (the screen counts them), %u times, with a pause "
             "between two.",
             kNames[target], static_cast<unsigned>(TAPS));
    char head[32];
    snprintf(head, sizeof(head), "%s : %u appuis longs", kFrench[target],
             static_cast<unsigned>(TAPS));
    gStep = target == 0 ? 11 : 12;
    prompt(text, head, "2 s chacun");
    while (buttons.wasPressed(kButtons[target])) {
    }
    while (buttons.wasReleased(kButtons[target])) {
    }
    SteamiScreen live;
    const bool liveOk = live.begin();
    uint32_t presses = 0;
    uint32_t releases = 0;
    uint32_t heldLong = 0;  // presses held more than one second
    uint32_t drops = 0;     // calls with isPressed() false between a press and its release
    bool holding = false;
    uint32_t pressedAt = 0;
    uint32_t shownPresses = UINT32_MAX;
    uint32_t shownHeld = UINT32_MAX;
    bool shownHolding = false;
    const uint32_t start = millis();
    while (releases < TAPS && millis() - start < LONG_WINDOW_MS) {
        const uint32_t turn = millis();
        if (buttons.wasPressed(kButtons[target])) {
            presses++;
            holding = true;
            pressedAt = millis();
        }
        const bool down = buttons.isPressed(kButtons[target]);
        if (buttons.wasReleased(kButtons[target])) {
            releases++;
            if (holding && millis() - pressedAt >= LONG_PRESS_MS) {
                heldLong++;
            }
            holding = false;
        } else if (holding && !down) {
            drops++;
        }
        const uint32_t heldS = holding ? (millis() - pressedAt) / 1000 : 0;
        if (liveOk && (presses != shownPresses || holding != shownHolding || heldS != shownHeld)) {
            shownPresses = presses;
            shownHolding = holding;
            shownHeld = heldS;
            drawHold(live, head, presses, holding, heldS);
        }
        waitRestOfTurn(turn);
    }
    snprintf(text, sizeof(text),
             "%s: %u presses, %u releases, %u held more than 1 s, %u calls not pressed while held",
             kNames[target], static_cast<unsigned>(presses), static_cast<unsigned>(releases),
             static_cast<unsigned>(heldLong), static_cast<unsigned>(drops));
    TEST_MESSAGE(text);
    char l1[24];
    char l2[24];
    snprintf(l1, sizeof(l1), "tenus 1 s : %u", static_cast<unsigned>(heldLong));
    snprintf(l2, sizeof(l2), "coupures : %u", static_cast<unsigned>(drops));
    const bool good = presses == TAPS && releases == TAPS && heldLong == TAPS && drops == 0;
    showOnScreen(good ? "OK" : "Écart", l1, l2, good ? STEAMI_GREEN : STEAMI_RED);
    delay(RESULT_MS);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, drops, "isPressed() false while the button was held");
    TEST_ASSERT_EQUAL_UINT32(TAPS, presses);
    TEST_ASSERT_EQUAL_UINT32(TAPS, releases);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TAPS, heldLong, "presses held more than one second");
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
    gStep = 13;
    prompt("For 60 s, press the four D-PAD buttons as fast as you can, together or not.",
           "D-PAD : 60 s", "le plus vite possible");
    const uint32_t start = millis();
    while (millis() - start < STORM_MS) {
        expander->poll();  // no interrupt pin: reads the chip latch at each call
        delay(LOOP_MS / 10);
    }
    TEST_MESSAGE("Stop and release every button.");
    showOnScreen("Stop", "Relâchez tout", nullptr, STEAMI_YELLOW);
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

    UNITY_BEGIN();
    RUN_TEST(test_user_btn_interrupt_survives_the_expander);
    RUN_TEST(test_dpad_registration_five_times_gives_no_event);
    RUN_TEST(test_exti_lines_of_a_b_menu_are_armed);
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
    gStep = kSteps;
    showOnScreen("Fini", "Test terminé", "merci !", STEAMI_GREEN);
    UNITY_END();
}

void loop() {}
