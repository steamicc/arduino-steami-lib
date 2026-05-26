// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Interactive D-PAD integration tests for the MCP23009E expander.
 *
 * **REQUIRES A HUMAN.** Each test prints a prompt to Serial @115200,
 * lights LED_GREEN as the "press now" signal, then polls a register
 * for up to 5 s. The whole run takes ~50 s if every button is hit
 * promptly. Run with the board flat on the table and the serial
 * monitor open. These tests will fail unattended — they live under
 * `tests/integration/` rather than `tests/hardware/` so `make ci`
 * doesn't drag them in.
 *
 * Mirrors the polling + interrupt scenarios from the MicroPython
 * sister project (tests/scenarios/mcp23009e.yaml) — same button
 * mapping (UP=7 DOWN=5 LEFT=6 RIGHT=4), same 5 s window, same
 * LED_GREEN convention.
 */

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

static constexpr uint32_t PRESS_TIMEOUT_MS = 5000;
static constexpr uint32_t PROMPT_READ_TIME_MS = 1500;

static void prompt(const char* message) {
    Serial.println();
    Serial.println(message);
    Serial.flush();
    delay(PROMPT_READ_TIME_MS);
}

// Polls getLevel() until the pin reads LOW (button pressed against the
// pull-up). LED_GREEN signals "go" and goes back off when the test ends
// — same UX as the MicroPython scenario.
static bool waitForPress(uint8_t pin, uint32_t timeoutMs = PRESS_TIMEOUT_MS) {
    digitalWrite(LED_GREEN, HIGH);
    uint32_t start = millis();
    bool pressed = false;
    while (millis() - start < timeoutMs) {
        if (expander.getLevel(pin) == MCP23009_LOGIC_LOW) {
            pressed = true;
            break;
        }
        delay(50);
    }
    digitalWrite(LED_GREEN, LOW);
    return pressed;
}

// Polls INTF until the per-pin bit fires. Pre-arms the interrupt by
// reading GPIO so the chip's "previous value" snapshot lines up with
// the resting (HIGH) state, and reads INTCAP at the end so the next
// interrupt can fire on the same pin.
static bool waitForInterrupt(uint8_t pin, uint32_t timeoutMs = PRESS_TIMEOUT_MS) {
    expander.interruptOnChange(pin, [](uint8_t /*level*/) {});
    (void)expander.getGpio();  // arm the comparator against the current state

    digitalWrite(LED_GREEN, HIGH);
    uint32_t start = millis();
    bool detected = false;
    while (millis() - start < timeoutMs) {
        if (expander.getIntf() & (1 << pin)) {
            detected = true;
            (void)expander.getIntcap();
            break;
        }
        delay(50);
    }
    digitalWrite(LED_GREEN, LOW);
    expander.disableInterrupt(pin);
    return detected;
}

void setUp(void) {
    expander.begin();
    expander.setup(MCP23009_BTN_UP, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_DOWN, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_LEFT, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_RIGHT, MCP23009_DIR_INPUT, MCP23009_PULLUP);
}

void tearDown(void) {}

// ----- Polling tests (one per D-PAD button) -----

void test_mcp23009e_dpad_up_polling() {
    prompt("POLLING test — press UP when LED_GREEN lights up (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForPress(MCP23009_BTN_UP), "UP not pressed in time");
}

void test_mcp23009e_dpad_down_polling() {
    prompt("POLLING test — press DOWN (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForPress(MCP23009_BTN_DOWN), "DOWN not pressed in time");
}

void test_mcp23009e_dpad_left_polling() {
    prompt("POLLING test — press LEFT (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForPress(MCP23009_BTN_LEFT), "LEFT not pressed in time");
}

void test_mcp23009e_dpad_right_polling() {
    prompt("POLLING test — press RIGHT (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForPress(MCP23009_BTN_RIGHT), "RIGHT not pressed in time");
}

// ----- Interrupt tests (one per D-PAD button) -----

void test_mcp23009e_dpad_up_interrupt() {
    prompt("INTERRUPT test — tap UP (5 s). Watching INTF for the bit to fire.");
    TEST_ASSERT_TRUE_MESSAGE(waitForInterrupt(MCP23009_BTN_UP), "UP interrupt not detected");
}

void test_mcp23009e_dpad_down_interrupt() {
    prompt("INTERRUPT test — tap DOWN (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForInterrupt(MCP23009_BTN_DOWN), "DOWN interrupt not detected");
}

void test_mcp23009e_dpad_left_interrupt() {
    prompt("INTERRUPT test — tap LEFT (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForInterrupt(MCP23009_BTN_LEFT), "LEFT interrupt not detected");
}

void test_mcp23009e_dpad_right_interrupt() {
    prompt("INTERRUPT test — tap RIGHT (5 s).");
    TEST_ASSERT_TRUE_MESSAGE(waitForInterrupt(MCP23009_BTN_RIGHT), "RIGHT interrupt not detected");
}

void setup() {
    delay(2000);
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
    }

    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    internalI2C.begin();

    Serial.println();
    Serial.println("MCP23009E D-PAD interactive integration suite — 8 tests,");
    Serial.println("each waits 5 s for a button press. Board on a flat surface.");

    UNITY_BEGIN();
    RUN_TEST(test_mcp23009e_dpad_up_polling);
    RUN_TEST(test_mcp23009e_dpad_down_polling);
    RUN_TEST(test_mcp23009e_dpad_left_polling);
    RUN_TEST(test_mcp23009e_dpad_right_polling);
    RUN_TEST(test_mcp23009e_dpad_up_interrupt);
    RUN_TEST(test_mcp23009e_dpad_down_interrupt);
    RUN_TEST(test_mcp23009e_dpad_left_interrupt);
    RUN_TEST(test_mcp23009e_dpad_right_interrupt);
    UNITY_END();
}

void loop() {}
