// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);

static constexpr uint32_t PRESS_TIMEOUT_MS = 5000;

static constexpr uint8_t MCP23009_ADDRESS = 0x20;
static constexpr uint8_t MCP23009_IODIR = 0x00;
static constexpr uint8_t MCP23009_GPPU = 0x06;
static constexpr uint8_t MCP23009_GPIO = 0x09;

static constexpr uint8_t DPAD_RIGHT_BIT = 4;
static constexpr uint8_t DPAD_DOWN_BIT = 5;
static constexpr uint8_t DPAD_LEFT_BIT = 6;
static constexpr uint8_t DPAD_UP_BIT = 7;

static void writeExpanderRegister(uint8_t reg, uint8_t value) {
    internalI2C.beginTransmission(MCP23009_ADDRESS);
    internalI2C.write(reg);
    internalI2C.write(value);
    internalI2C.endTransmission();
}

static uint8_t readExpanderRegister(uint8_t reg) {
    internalI2C.beginTransmission(MCP23009_ADDRESS);
    internalI2C.write(reg);
    internalI2C.endTransmission(false);

    internalI2C.requestFrom(MCP23009_ADDRESS, static_cast<uint8_t>(1));

    return internalI2C.available() ? internalI2C.read() : 0xFF;
}

static bool waitForDirectButton(uint32_t pin) {
    while (digitalRead(pin) == LOW) {
        delay(20);
    }

    digitalWrite(LED_GREEN, HIGH);

    uint32_t start = millis();

    while (millis() - start < PRESS_TIMEOUT_MS) {
        if (digitalRead(pin) == LOW) {
            digitalWrite(LED_GREEN, LOW);
            return true;
        }

        delay(20);
    }

    digitalWrite(LED_GREEN, LOW);
    return false;
}

static bool waitForDpadButton(uint8_t bit) {
    digitalWrite(LED_GREEN, HIGH);

    uint32_t start = millis();

    while (millis() - start < PRESS_TIMEOUT_MS) {
        uint8_t gpio = readExpanderRegister(MCP23009_GPIO);

        if ((gpio & (1U << bit)) == 0) {
            digitalWrite(LED_GREEN, LOW);
            return true;
        }

        delay(20);
    }

    digitalWrite(LED_GREEN, LOW);
    return false;
}

void test_button_a() {
    TEST_MESSAGE("Press A when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDirectButton(A_BUTTON), "A button not pressed in time");
}

void test_button_b() {
    TEST_MESSAGE("Press B when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDirectButton(B_BUTTON), "B button not pressed in time");
}

void test_button_menu() {
    TEST_MESSAGE("Press MENU when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDirectButton(MENU_BUTTON), "MENU button not pressed in time");
}

void test_dpad_up() {
    TEST_MESSAGE("Press D-PAD UP when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDpadButton(DPAD_UP_BIT), "D-PAD UP not pressed in time");
}

void test_dpad_down() {
    TEST_MESSAGE("Press D-PAD DOWN when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDpadButton(DPAD_DOWN_BIT), "D-PAD DOWN not pressed in time");
}

void test_dpad_left() {
    TEST_MESSAGE("Press D-PAD LEFT when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDpadButton(DPAD_LEFT_BIT), "D-PAD LEFT not pressed in time");
}

void test_dpad_right() {
    TEST_MESSAGE("Press D-PAD RIGHT when LED_GREEN lights.");
    TEST_ASSERT_TRUE_MESSAGE(waitForDpadButton(DPAD_RIGHT_BIT), "D-PAD RIGHT not pressed in time");
}

void setup() {
    delay(2000);

    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    pinMode(A_BUTTON, INPUT_PULLUP);
    pinMode(B_BUTTON, INPUT_PULLUP);
    pinMode(MENU_BUTTON, INPUT_PULLUP);

    pinMode(RST_EXPANDER, OUTPUT);
    digitalWrite(RST_EXPANDER, HIGH);
    delay(10);

    internalI2C.begin();

    // D-PAD GPIO4..7 as inputs + pull-ups.
    writeExpanderRegister(MCP23009_IODIR, 0xF0);
    writeExpanderRegister(MCP23009_GPPU, 0xF0);

    UNITY_BEGIN();
    RUN_TEST(test_button_a);
    RUN_TEST(test_button_b);
    RUN_TEST(test_button_menu);
    RUN_TEST(test_dpad_up);
    RUN_TEST(test_dpad_down);
    RUN_TEST(test_dpad_left);
    RUN_TEST(test_dpad_right);
    UNITY_END();
}

void loop() {}
