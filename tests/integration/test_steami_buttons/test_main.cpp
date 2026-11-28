// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Interactive integration tests for SteamiButtons: taps caught between two slow calls (issues #251
 * and #341).
 *
 * **REQUIRES A HUMAN.** Each step prints a prompt to Serial @115200 and shows it, in French, on
 * the screen: a 3-2-1 countdown, then "À vous !". No LED is lit. The whole run takes about eight
 * minutes. These tests fail unattended: they live under tests/integration/ so that `make ci`
 * leaves them out. Run them with `make test-integration/steami_buttons` and the serial monitor
 * open. One step alone (its number is on the screen): build with STEAMI_BUTTONS_STEP set, as in
 * `PLATFORMIO_BUILD_FLAGS=-DSTEAMI_BUTTONS_STEP=14 make test-integration/steami_buttons`.
 *
 * - The expander does not take EXTI line 0: an interrupt the sketch attaches to USER_BTN (MENU)
 *   before the expander is built keeps working.
 * - Registering the four D-PAD callbacks five times in a row gives no event.
 * - The EXTI lines of A, B and MENU are routed to their pins, unmasked on both edges, with their
 *   NVIC interrupt enabled, after a screen refresh too.
 * - Steps 4 to 10: ten quick taps on each of the seven buttons, about one per second, in a fixed
 *   window of 15 s, with a loop that calls the buttons every 300 ms: exactly ten presses counted
 *   by getPresses() on that button, and no press on the others. The test counts for the whole
 *   window, it does not stop at ten. For A, B and MENU, a 1 kHz timer that reads the pins counts
 *   the taps independently ("au doigt"): the presses reported must match it.
 * - Steps 11 and 12: the same on A and on B, two or three taps per second, faster than the loop:
 *   some turns of the loop see two taps, and getPresses() must still count ten, as the reference
 *   does. wasPressed() is a flag and would count fewer.
 * - Step 13, reference for the D-PAD: ten taps on UP, two or three per second, counted by reading
 *   the port of the expander every 2 ms (no SteamiButtons object, no interrupt). It tells whether
 *   the finger did ten taps at that pace, which step 14 then asks again.
 * - Step 14: ten taps on UP at that pace, with the loop of the second hardware run (520 ms, the
 *   screen redrawn at each turn): getPresses() must count ten (the D-PAD release one turn late
 *   merged such taps).
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
 * The MicroPython Buttons class is to pass the same scenario: same buttons, same loops, same
 * counts. It has no get_presses() yet: steamicc/micropython-steami-lib#515.
 */

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <SteamiScreen.h>
#include <unity.h>

static constexpr uint32_t LOOP_MS = 300;       // a slow loop, like WeatherStation's
static constexpr uint32_t SLOW_LOOP_MS = 520;  // the loop of the second hardware run (#341)
static constexpr uint32_t REF_POLL_MS = 2;     // the D-PAD reference reads the expander that often
static constexpr uint32_t TAPS = 10;           // per button
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

// 0 runs every step; n runs step n only (the number shown on the screen).
#ifndef STEAMI_BUTTONS_STEP
#define STEAMI_BUTTONS_STEP 0
#endif
#define RUN_STEP(n, fn)                                               \
    do {                                                              \
        if (STEAMI_BUTTONS_STEP == 0 || STEAMI_BUTTONS_STEP == (n)) { \
            RUN_TEST(fn);                                             \
        }                                                             \
    } while (0)

void setUp(void) {}

void tearDown(void) {
    Steami.end();
}

// --- Screen: the French instructions ---

static int gStep = 0;
static constexpr int kSteps = 17;

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
static void waitRestOfTurn(uint32_t turnStart, uint32_t loopMs = LOOP_MS) {
    const uint32_t spent = millis() - turnStart;
    if (spent < loopMs) {
        delay(loopMs - spent);
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

// `fast`: two or three taps per second instead of one, so faster than the loop of `loopMs`.
static void countTapsOn(int target, int step, uint32_t loopMs = LOOP_MS, bool fast = false) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    const bool mcu = target < 3;
    char text[160];
    snprintf(text, sizeof(text),
             "Tap %s exactly %u times, quickly (under 150 ms), %s, within %u s. Touch nothing "
             "else. The loop calls the buttons every %u ms.",
             kNames[target], static_cast<unsigned>(TAPS),
             fast ? "two or three times per second" : "about once per second",
             static_cast<unsigned>(TAP_WINDOW_MS / 1000), static_cast<unsigned>(loopMs));
    char head[32];
    snprintf(head, sizeof(head), "%s : %u tapes brèves", kFrench[target],
             static_cast<unsigned>(TAPS));
    gStep = step;
    prompt(text, head, fast ? "2 à 3 par seconde" : "une par seconde");
    // What was pressed during the countdown does not count.
    for (int k = 0; k < 7; k++) {
        buttons.getPresses(kButtons[k]);
        buttons.wasPressed(kButtons[k]);
        buttons.wasReleased(kButtons[k]);
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
            presses[k] += buttons.getPresses(kButtons[k]);
            releases[k] += buttons.wasReleased(kButtons[k]) ? 1 : 0;
        }
        const uint32_t elapsed = millis() - start;
        const uint32_t left = elapsed < TAP_WINDOW_MS ? (TAP_WINDOW_MS - elapsed + 999) / 1000 : 0;
        if (liveOk && (presses[target] != shownSeen || left != shownLeft)) {
            shownSeen = presses[target];
            shownLeft = left;
            drawTaps(live, head, shownSeen, shownLeft);
        }
        waitRestOfTurn(turn, loopMs);
    }
    // The presses and releases made in the last turn, still counted.
    for (int k = 0; k < 7; k++) {
        presses[k] += buttons.getPresses(kButtons[k]);
        releases[k] += buttons.wasReleased(kButtons[k]) ? 1 : 0;
    }
    const uint32_t finger = mcu ? stopReference() : 0;
    // wasReleased() is a flag: the turns that saw two releases count one.
    for (int k = 0; k < 7; k++) {
        snprintf(text, sizeof(text), "%s: %u presses (getPresses), %u turns with a release",
                 kNames[k], static_cast<unsigned>(presses[k]), static_cast<unsigned>(releases[k]));
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
    TEST_ASSERT_TRUE_MESSAGE(releases[target] >= 1 && releases[target] <= TAPS,
                             "turns with a release of the tapped button");
    for (int k = 0; k < 7; k++) {
        if (k != target) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, presses[k], "press on a button nobody touched");
        }
    }
    TEST_ASSERT_FALSE(buttons.isPressed(kButtons[target]));
}

void test_quick_taps_on_a(void) {
    countTapsOn(0, 4);
}
void test_quick_taps_on_b(void) {
    countTapsOn(1, 5);
}
void test_quick_taps_on_menu(void) {
    countTapsOn(2, 6);
}
void test_quick_taps_on_up(void) {
    countTapsOn(3, 7);
}
void test_quick_taps_on_down(void) {
    countTapsOn(4, 8);
}
void test_quick_taps_on_left(void) {
    countTapsOn(5, 9);
}
void test_quick_taps_on_right(void) {
    countTapsOn(6, 10);
}

// --- Taps faster than the loop ---

void test_fast_taps_on_a(void) {
    countTapsOn(0, 11, LOOP_MS, true);
}
void test_fast_taps_on_b(void) {
    countTapsOn(1, 12, LOOP_MS, true);
}

// The taps on the D-PAD seen by reading the port of the expander every 2 ms, out of any
// interrupt, with no SteamiButtons object: a 10 ms stable level changes the state, a change to
// pressed counts a tap. Reading the port clears the latch of the chip, so this reference cannot
// run in the step that measures SteamiButtons: it is a step of its own, at the same pace.
void test_dpad_reference_fast_taps_on_up(void) {
    SteamiMCP23009E* expander = Steami.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    for (uint8_t bit : kDpadBits) {
        expander->setup(bit, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    }
    gStep = 13;
    char text[128];
    snprintf(text, sizeof(text),
             "Reference: tap UP exactly %u times, quickly, two or three times per second, within "
             "%u s. Touch nothing else.",
             static_cast<unsigned>(TAPS), static_cast<unsigned>(TAP_WINDOW_MS / 1000));
    prompt(text, "HAUT : 10 tapes (réf.)", "2 à 3 par seconde");
    uint8_t port = 0xFF;
    TEST_ASSERT_TRUE(expander->gpio(port));
    bool raw[4];
    bool down[4];
    uint8_t stable[4];
    uint32_t taps[4] = {};
    uint32_t errors = 0;
    for (uint8_t d = 0; d < 4; d++) {
        raw[d] = (port & (1U << kDpadBits[d])) == 0;
        down[d] = raw[d];
        stable[d] = 0;
    }
    const uint8_t stableReads = 10 / REF_POLL_MS;
    const uint32_t start = millis();
    uint32_t next = start;
    while (millis() - start < TAP_WINDOW_MS) {
        while (static_cast<int32_t>(millis() - next) < 0) {
        }
        next += REF_POLL_MS;
        if (!expander->gpio(port)) {
            errors++;
            continue;
        }
        for (uint8_t d = 0; d < 4; d++) {
            const bool now = (port & (1U << kDpadBits[d])) == 0;
            if (now != raw[d]) {
                raw[d] = now;
                stable[d] = 0;
            } else if (stable[d] < stableReads) {
                stable[d]++;
            } else if (now != down[d]) {
                down[d] = now;
                taps[d] += now ? 1 : 0;
            }
        }
    }
    for (uint8_t d = 0; d < 4; d++) {
        snprintf(text, sizeof(text), "%s: %u taps seen by reading the expander every %u ms",
                 kNames[3 + d], static_cast<unsigned>(taps[d]), static_cast<unsigned>(REF_POLL_MS));
        TEST_MESSAGE(text);
    }
    char l1[24];
    snprintf(l1, sizeof(l1), "au doigt : %u", static_cast<unsigned>(taps[0]));
    const bool good = taps[0] == TAPS && taps[1] + taps[2] + taps[3] == 0 && errors == 0;
    showOnScreen(good ? "OK" : "Écart", l1, "attendu : 10", good ? STEAMI_GREEN : STEAMI_RED);
    delay(RESULT_MS);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, errors, "expander reads that failed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TAPS, taps[0], "taps on UP seen by the reference");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, taps[1] + taps[2] + taps[3], "taps on the other arrows");
}

// The pace of step 13, with the 520 ms loop of the second hardware run.
void test_fast_taps_on_up_with_a_520_ms_loop(void) {
    countTapsOn(3, 14, SLOW_LOOP_MS, true);
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
    gStep = target == 0 ? 15 : 16;
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
    gStep = 17;
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
    RUN_STEP(1, test_user_btn_interrupt_survives_the_expander);
    RUN_STEP(2, test_dpad_registration_five_times_gives_no_event);
    RUN_STEP(3, test_exti_lines_of_a_b_menu_are_armed);
    RUN_STEP(4, test_quick_taps_on_a);
    RUN_STEP(5, test_quick_taps_on_b);
    RUN_STEP(6, test_quick_taps_on_menu);
    RUN_STEP(7, test_quick_taps_on_up);
    RUN_STEP(8, test_quick_taps_on_down);
    RUN_STEP(9, test_quick_taps_on_left);
    RUN_STEP(10, test_quick_taps_on_right);
    RUN_STEP(11, test_fast_taps_on_a);
    RUN_STEP(12, test_fast_taps_on_b);
    RUN_STEP(13, test_dpad_reference_fast_taps_on_up);
    RUN_STEP(14, test_fast_taps_on_up_with_a_520_ms_loop);
    RUN_STEP(15, test_long_presses_on_a);
    RUN_STEP(16, test_long_presses_on_up);
    RUN_STEP(17, test_dpad_levels_alternate_under_fast_presses);
    gStep = kSteps;
    showOnScreen("Fini", "Test terminé", "merci !", STEAMI_GREEN);
    UNITY_END();
}

void loop() {}
