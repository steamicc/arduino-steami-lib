// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include <cstdio>

#include "MCP23009E.h"
#include "Wire.h"

constexpr uint8_t RESET_PIN = 5;
constexpr uint8_t ADDR = MCP23009_I2C_ADDR;

MCP23009E* mcp = nullptr;

void setUp(void) {
    Wire = TwoWire();
    gpioPinState().clear();
    gpioPinMode().clear();
    mcp = new MCP23009E(Wire, RESET_PIN, ADDR);
}

void tearDown(void) {
    delete mcp;
    mcp = nullptr;
}

void testBeginConfiguresResetPinAsOutput(void) {
    mcp->begin();
    TEST_ASSERT_EQUAL(OUTPUT, gpioPinMode()[RESET_PIN]);
}

void testBeginResetsDevice(void) {
    mcp->begin();
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[RESET_PIN]);
}

void testBeginReturnsTrue(void) {
    TEST_ASSERT_TRUE(mcp->begin());
}

// begin() must probe the bus and surface a missing/incorrectly-wired
// expander as false, not silently succeed. Use the mock's NACK
// injection to simulate the device not ACKing.
void testBeginReturnsFalseWhenDeviceMissing(void) {
    Wire.setEndTransmissionResult(2);  // NACK on address
    TEST_ASSERT_FALSE(mcp->begin());
    Wire.setEndTransmissionResult(0);  // restore default for the next test
}

void testResetLeavesPinHigh(void) {
    mcp->reset();
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[RESET_PIN]);
}

void testPowerOffDrivesResetPinLow(void) {
    mcp->powerOff();
    TEST_ASSERT_EQUAL(LOW, gpioPinState()[RESET_PIN]);
}

void testPowerOnDrivesResetPinHigh(void) {
    mcp->powerOn();
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[RESET_PIN]);
}

void testReadRegReturnsPreloadedValue(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0xAB);
    TEST_ASSERT_EQUAL(0xAB, mcp->getIodir());
}

void testSetIodirWritesCorrectRegister(void) {
    mcp->setIodir(0x55);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_IODIR && w.value == 0x55) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testGetIodirReadsCorrectRegister(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0xAA);
    TEST_ASSERT_EQUAL(0xAA, mcp->getIodir());
}

void testSetGpioWritesCorrectRegister(void) {
    mcp->setGpio(0x3C);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x3C) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testGetGpioReadsCorrectRegister(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xC3);
    TEST_ASSERT_EQUAL(0xC3, mcp->getGpio());
}

void testSetGppuWritesCorrectRegister(void) {
    mcp->setGppu(0x0F);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPPU && w.value == 0x0F) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testGetGppuReadsCorrectRegister(void) {
    Wire.setRegister(ADDR, MCP23009_GPPU, 0xF0);
    TEST_ASSERT_EQUAL(0xF0, mcp->getGppu());
}

// OLAT round-trip — ported from the MicroPython scenario "Set and get
// OLAT register". OLAT is the latch register driving the output pins;
// callers can write it directly to bypass setLevel()'s pin-by-pin
// masking, so the round-trip is part of the public contract.
void testSetOlatWritesCorrectRegister(void) {
    mcp->setOlat(0x3C);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_OLAT && w.value == 0x3C) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testGetOlatReadsCorrectRegister(void) {
    Wire.setRegister(ADDR, MCP23009_OLAT, 0x3C);
    TEST_ASSERT_EQUAL(0x3C, mcp->getOlat());
}

void testSetupConfiguresDirection(void) {
    mcp->setup(0, MCP23009Pin::OUT);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_IODIR && w.value == 0x00) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testSetupConfiguresPullup(void) {
    mcp->setup(1, MCP23009Pin::OUT, MCP23009Pin::PULL_UP);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPPU && w.value == 0x02) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testSetupConfiguresPolarity(void) {
    mcp->setup(2, MCP23009Pin::OUT, MCP23009_NO_PULLUP, MCP23009_POL_INVERTED);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_IPOL && w.value == (1 << 2)) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testSetupIgnoresInvalidPinNumber(void) {
    mcp->setup(8, MCP23009Pin::OUT);
    for (const auto& w : Wire.getWrites()) {
        TEST_ASSERT_NOT_EQUAL(MCP23009_IODIR, w.reg);
    }
}

void testSetLevelWritesGpioRegister(void) {
    mcp->setLevel(3, 1);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x08) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testSetLevelIgnoresInputPin(void) {
    mcp->setup(4, MCP23009Pin::IN);
    Wire.clearWrites();
    mcp->setLevel(4, 1);
    for (const auto& w : Wire.getWrites()) {
        TEST_ASSERT_NOT_EQUAL(MCP23009_GPIO, w.reg);
    }
}

void testSetLevelIgnoresInvalidPin(void) {
    Wire.clearWrites();
    mcp->setLevel(8, 1);
    for (const auto& w : Wire.getWrites()) {
        TEST_ASSERT_NOT_EQUAL(MCP23009_GPIO, w.reg);
    }
}

void testGetLevelReturnsPreloadedValue(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xAA);
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, mcp->getLevel(1));
}

void testGetLevelReturnsLowOnInvalidPin(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xFF);
    TEST_ASSERT_EQUAL(0, mcp->getLevel(8));
}

void testSetIoconWritesRegisterValue(void) {
    MCP23009Config config;
    config.setSeqop();
    mcp->setIocon(config);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_IOCON && w.value == 0x20) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testGetIoconReturnsConfigObject(void) {
    Wire.setRegister(ADDR, MCP23009_IOCON, 0x20);
    MCP23009Config config = mcp->getIocon();
    TEST_ASSERT_TRUE(config.hasSeqop());
    TEST_ASSERT_FALSE(config.hasOdr());
    TEST_ASSERT_FALSE(config.hasIntpol());
    TEST_ASSERT_FALSE(config.hasIntcc());
}

void testConfigSetSeqopSetsBit(void) {
    MCP23009Config config;
    config.setSeqop();
    TEST_ASSERT_TRUE(config.hasSeqop());
}

void testConfigClearSeqopClearsBit(void) {
    MCP23009Config config(0xFF);
    config.clearSeqop();
    TEST_ASSERT_FALSE(config.hasSeqop());
}

void testConfigHasSeqopReturnsTrueWhenSet(void) {
    MCP23009Config config(0x20);
    TEST_ASSERT_TRUE(config.hasSeqop());
}

void testConfigSetOdrSetsBit(void) {
    MCP23009Config config;
    config.setOdr();
    TEST_ASSERT_TRUE(config.hasOdr());
}

void testConfigSetIntpolSetsBit(void) {
    MCP23009Config config;
    config.setIntpol();
    TEST_ASSERT_TRUE(config.hasIntpol());
}

void testConfigSetIntccSetsBit(void) {
    MCP23009Config config;
    config.setIntcc();
    TEST_ASSERT_TRUE(config.hasIntcc());
}

void testConfigGetRegisterValueReturnsCorrectByte(void) {
    MCP23009Config config;
    config.setSeqop();
    config.setIntpol();
    TEST_ASSERT_EQUAL(0x22, config.getRegisterValue());
}

void testInterruptOnChangeEnablesInterruptRegister(void) {
    mcp->interruptOnChange(0, [](uint8_t) {});
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPINTEN && w.value == 0x01) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testInterruptOnFallingEnablesInterruptRegister(void) {
    mcp->interruptOnFalling(1, []() {});
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPINTEN && w.value == 0x02) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testInterruptOnRaisingEnablesInterruptRegister(void) {
    mcp->interruptOnRaising(2, []() {});
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPINTEN && w.value == 0x04) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testDisableInterruptClearsInterruptRegister(void) {
    mcp->disableInterrupt(3);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPINTEN && w.value == 0x00) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testPinOnSetsLevelHigh(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    MCP23009Pin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.on();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x20) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testPinOffSetsLevelLow(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    MCP23009Pin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.off();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x00) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testPinToggleInvertsLevel(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    MCP23009Pin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.toggle();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x20) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testPinValueReadReturnsGpioLevel(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x20);
    MCP23009Pin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    TEST_ASSERT_EQUAL(1, pin.value());
}

void testPinValueWriteSetsGpioLevel(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    MCP23009Pin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.value(1);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && w.value == 0x20) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testActiveLowOnSetsGpioLow(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x20);
    MCP23009ActiveLowPin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.on();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && !(w.value & (1 << 5))) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testActiveLowOffSetsGpioHigh(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x00);
    MCP23009ActiveLowPin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.off();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && (w.value & (1 << 5))) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testActiveLowValueReadReturnsInverted(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x20);
    MCP23009ActiveLowPin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    TEST_ASSERT_EQUAL(0, pin.value());
}

void testActiveLowValueWriteInvertsBeforeApplying(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    MCP23009ActiveLowPin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.value(1);
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && !(w.value & (1 << 5))) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testActiveLowToggleInvertsState(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x20);
    MCP23009ActiveLowPin pin(*mcp, 5, MCP23009Pin::OUT, 0xff, 0xff);
    Wire.clearWrites();
    pin.toggle();
    bool found = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPIO && !(w.value & (1 << 5))) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testBeginConfiguresResetPinAsOutput);
    RUN_TEST(testBeginResetsDevice);
    RUN_TEST(testBeginReturnsTrue);
    RUN_TEST(testBeginReturnsFalseWhenDeviceMissing);
    RUN_TEST(testResetLeavesPinHigh);
    RUN_TEST(testPowerOffDrivesResetPinLow);
    RUN_TEST(testPowerOnDrivesResetPinHigh);
    RUN_TEST(testReadRegReturnsPreloadedValue);
    RUN_TEST(testSetIodirWritesCorrectRegister);
    RUN_TEST(testGetIodirReadsCorrectRegister);
    RUN_TEST(testSetGpioWritesCorrectRegister);
    RUN_TEST(testGetGpioReadsCorrectRegister);
    RUN_TEST(testSetGppuWritesCorrectRegister);
    RUN_TEST(testGetGppuReadsCorrectRegister);
    RUN_TEST(testSetOlatWritesCorrectRegister);
    RUN_TEST(testGetOlatReadsCorrectRegister);
    RUN_TEST(testSetupConfiguresDirection);
    RUN_TEST(testSetupConfiguresPullup);
    RUN_TEST(testSetupConfiguresPolarity);
    RUN_TEST(testSetupIgnoresInvalidPinNumber);
    RUN_TEST(testSetLevelWritesGpioRegister);
    RUN_TEST(testSetLevelIgnoresInputPin);
    RUN_TEST(testSetLevelIgnoresInvalidPin);
    RUN_TEST(testGetLevelReturnsPreloadedValue);
    RUN_TEST(testGetLevelReturnsLowOnInvalidPin);
    RUN_TEST(testSetIoconWritesRegisterValue);
    RUN_TEST(testGetIoconReturnsConfigObject);
    RUN_TEST(testConfigSetSeqopSetsBit);
    RUN_TEST(testConfigClearSeqopClearsBit);
    RUN_TEST(testConfigHasSeqopReturnsTrueWhenSet);
    RUN_TEST(testConfigSetOdrSetsBit);
    RUN_TEST(testConfigSetIntpolSetsBit);
    RUN_TEST(testConfigSetIntccSetsBit);
    RUN_TEST(testConfigGetRegisterValueReturnsCorrectByte);
    RUN_TEST(testInterruptOnChangeEnablesInterruptRegister);
    RUN_TEST(testInterruptOnFallingEnablesInterruptRegister);
    RUN_TEST(testInterruptOnRaisingEnablesInterruptRegister);
    RUN_TEST(testDisableInterruptClearsInterruptRegister);
    RUN_TEST(testPinOnSetsLevelHigh);
    RUN_TEST(testPinOffSetsLevelLow);
    RUN_TEST(testPinToggleInvertsLevel);
    RUN_TEST(testPinValueReadReturnsGpioLevel);
    RUN_TEST(testPinValueWriteSetsGpioLevel);
    RUN_TEST(testActiveLowOnSetsGpioLow);
    RUN_TEST(testActiveLowOffSetsGpioHigh);
    RUN_TEST(testActiveLowValueReadReturnsInverted);
    RUN_TEST(testActiveLowValueWriteInvertsBeforeApplying);
    RUN_TEST(testActiveLowToggleInvertsState);
    return UNITY_END();
}