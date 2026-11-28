// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "SteamiMCP23009E.h"
#include "Wire.h"
#include "steami_fake_mcp23009.h"

constexpr uint8_t RESET_PIN = 5;
constexpr uint8_t ADDR = MCP23009_I2C_ADDR;

SteamiMCP23009E* mcp = nullptr;

void setUp(void) {
    Wire = TwoWire();
    gpioPinState().clear();
    gpioPinMode().clear();
    mcp = new SteamiMCP23009E(Wire, RESET_PIN, ADDR);
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

void testGpioReadsPort(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xB0);
    uint8_t value = 0;
    TEST_ASSERT_TRUE(mcp->gpio(value));
    TEST_ASSERT_EQUAL_HEX8(0xB0, value);
}

void testGpioReportsBusErrorWhenAbsent(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xB0);
    Wire.setAbsent(ADDR);
    uint8_t value = 0x5A;
    TEST_ASSERT_FALSE(mcp->gpio(value));
    TEST_ASSERT_EQUAL_HEX8(0x5A, value);
}

void testGpioReportsEndTransmissionError(void) {
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xB0);
    Wire.setEndTransmissionResult(2);
    uint8_t value = 0x5A;
    TEST_ASSERT_FALSE(mcp->gpio(value));
    TEST_ASSERT_EQUAL_HEX8(0x5A, value);
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
        if (w.reg == MCP23009_OLAT && w.value == 0x08) {
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
    Wire.setRegister(ADDR, MCP23009_OLAT, 0xAA);
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

void testInterruptOnRisingEnablesInterruptRegister(void) {
    mcp->interruptOnRising(2, []() {});
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
        if (w.reg == MCP23009_OLAT && w.value == 0x20) {
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
        if (w.reg == MCP23009_OLAT && w.value == 0x00) {
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
        if (w.reg == MCP23009_OLAT && w.value == 0x20) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testPinValueReadReturnsGpioLevel(void) {
    Wire.setRegister(ADDR, MCP23009_OLAT, 0x20);
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
        if (w.reg == MCP23009_OLAT && w.value == 0x20) {
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
        if (w.reg == MCP23009_OLAT && !(w.value & (1 << 5))) {
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
        if (w.reg == MCP23009_OLAT && (w.value & (1 << 5))) {
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
        if (w.reg == MCP23009_OLAT && !(w.value & (1 << 5))) {
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
        if (w.reg == MCP23009_OLAT && !(w.value & (1 << 5))) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

// Regression: 9ef4737 — setLevel() reads OLAT (the output latch), not
// GPIO (the physical pin states). Reading GPIO on a mixed in/out port
// would latch input levels into OLAT and drive surprise values once
// the input is reconfigured as an output.
void testSetLevelReadsFromOlatNotGpio(void) {
    Wire.setRegister(ADDR, MCP23009_IODIR, 0x00);  // all output
    // Stage divergent OLAT and GPIO snapshots so we can tell which
    // one setLevel() chose as the RMW source.
    Wire.setRegister(ADDR, MCP23009_OLAT, 0x00);
    Wire.setRegister(ADDR, MCP23009_GPIO, 0xFF);
    Wire.clearWrites();

    mcp->setLevel(0, 1);

    // The write to GPIO must reflect OLAT(=0x00) with bit 0 set,
    // i.e. 0x01. A regression that reads GPIO instead would write
    // 0xFF (OR-in bit 0 on already-0xFF). The first GPIO write is
    // what the driver committed.
    uint8_t gpioWrite = 0xAA;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_OLAT) {
            gpioWrite = w.value;
            break;
        }
    }
    TEST_ASSERT_EQUAL_HEX8(0x01, gpioWrite);
}

// Regression: 9ef4737 — interruptEvent() classifies edges from INTCAP
// (the chip's snapshot at the moment of the interrupt) regardless of
// the INTCC bit. Reading GPIO instead would mis-classify rising vs
// falling when the line bounced or already changed back between the
// IRQ and the deferred dispatch. Since #303, the later change found in
// GPIO is reported too, after the captured one.
void testInterruptEventUsesIntcapNotGpio(void) {
    std::string edges;
    mcp->interruptOnRising(0, [&]() { edges += 'r'; });
    mcp->interruptOnFalling(0, [&]() { edges += 'f'; });

    // INTF says pin 0 fired. INTCAP says it was HIGH at the event
    // (-> rising). GPIO says it's currently LOW (bounced back) —
    // which is what the buggy code would mis-classify as falling.
    Wire.setRegister(ADDR, MCP23009_INTF, 0x01);
    Wire.setRegister(ADDR, MCP23009_INTCAP, 0x01);
    Wire.setRegister(ADDR, MCP23009_GPIO, 0x00);

    TEST_ASSERT_TRUE(mcp->interruptEvent());

    // The rising edge from INTCAP first, then the fall found in GPIO.
    TEST_ASSERT_EQUAL_STRING("rf", edges.c_str());
}

// Regression: 9ef4737 — poll() drains the _irqPending flag set by
// irqHandler() and dispatches in non-ISR context. With an interrupt pin
// and no pending flag, poll() must be a strict no-op (no I2C, no
// callbacks); this guarantees the ISR-only path stays the only way to
// schedule work.
void testPollIsNoOpWhenNoPending(void) {
    delete mcp;
    mcp = new SteamiMCP23009E(Wire, RESET_PIN, ADDR, /*interruptPin=*/7);
    mcp->begin();
    int callCount = 0;
    mcp->interruptOnChange(0, [&](uint8_t) { ++callCount; });
    Wire.setRegister(ADDR, MCP23009_INTF, 0x01);
    Wire.setRegister(ADDR, MCP23009_INTCAP, 0x01);
    Wire.clearWrites();

    mcp->poll();

    TEST_ASSERT_EQUAL_MESSAGE(0, callCount, "poll() with no pending flag must not fire callbacks");
    TEST_ASSERT_TRUE_MESSAGE(Wire.getWrites().empty(),
                             "poll() with no pending flag must not do I2C");
}

// Regression: 9ef4737 — MCP23009Pin destructor must call
// disableInterrupt() on its pin number so the parent expander's
// [this]-capturing callback lambda is cleared. Without this, a
// destroyed Pin still has its slot live in the expander and a later
// poll() would dereference dangling state.
void testPinDestructorClearsExpanderCallbackSlot(void) {
    // Pre-set GPINTEN so we can detect the destructor's clear-write.
    Wire.setRegister(ADDR, MCP23009_GPINTEN, 0xFF);
    {
        MCP23009Pin pin(*mcp, 5, MCP23009Pin::IN, 0xff, 0xff);
        pin.irq([]() {}, MCP23009Pin::IRQ_RISING);
        Wire.clearWrites();
        // Pin destructor runs at scope exit.
    }
    // Locate the LAST GPINTEN write — that's the one the destructor
    // pushed. Bit 5 must be cleared (pin 5 disabled).
    uint8_t lastGpinten = 0xFF;
    bool sawGpintenWrite = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == MCP23009_GPINTEN) {
            lastGpinten = w.value;
            sawGpintenWrite = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawGpintenWrite, "destructor must write GPINTEN to disable the pin");
    TEST_ASSERT_FALSE_MESSAGE(lastGpinten & (1 << 5),
                              "destructor must clear pin 5's bit in GPINTEN");
}

// ----- Interrupt path against a model of the chip (issue #303) -----
//
// FakeMCP23009 (tests/shared/steami_fake_mcp23009.h) models INTF, INTCAP
// and the compare-with-previous logic. The tests call interruptEvent()
// where poll() would run on the board.

constexpr uint8_t UP = MCP23009_BTN_UP;
constexpr uint8_t DOWN = MCP23009_BTN_DOWN;
constexpr uint8_t LEFT = MCP23009_BTN_LEFT;
constexpr uint8_t RIGHT = MCP23009_BTN_RIGHT;
constexpr uint8_t RELEASED = 0xF0;  // D-PAD: active low, with pull-ups

FakeMCP23009 chip;

// One reported event: pin name, irqLevel() in the handler, value() in
// the handler.
struct Event {
    std::string name;
    int level;
    int value;
    bool operator==(const Event& o) const {
        return name == o.name && level == o.level && value == o.value;
    }
};

std::string describe(const std::vector<Event>& events) {
    std::string out;
    for (const auto& e : events) {
        out += "(" + e.name + "," + std::to_string(e.level) + "," + std::to_string(e.value) + ")";
    }
    return out;
}

#define ASSERT_EVENTS(expected, actual) \
    TEST_ASSERT_EQUAL_STRING(describe(expected).c_str(), describe(actual).c_str())

void attachChip() {
    chip.reset();
    Wire.attachDevice(ADDR, &chip);
}

// D-PAD pin: input with pull-up, as on the STeaMi.
struct DpadPin {
    explicit DpadPin(uint8_t gp) : pin(*mcp, gp, MCP23009Pin::IN, MCP23009Pin::PULL_UP) {}
    MCP23009Pin pin;
};

std::function<void()> recorder(std::vector<Event>& events, const char* name, MCP23009Pin& pin) {
    return [&events, name, &pin]() { events.push_back({name, pin.irqLevel(), pin.value()}); };
}

constexpr uint16_t BOTH = MCP23009Pin::IRQ_FALLING | MCP23009Pin::IRQ_RISING;

// ----- Captured level -----

void testIrqLevelIsNoneBeforeAnyInterrupt(void) {
    attachChip();
    DpadPin up(UP);
    TEST_ASSERT_EQUAL_UINT8(MCP23009Pin::IRQ_LEVEL_NONE, up.pin.irqLevel());
}

void testHandlerSeesPressReleasedBeforeItRuns(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);
    chip.release(UP);  // released before poll() runs
    mcp->interruptEvent();
    // The press, with the captured level, then the release read from GPIO.
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 1}, {"UP", 1, 1}}), events);
}

void testPressThenReleaseEachReported(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}, {"UP", 1, 1}}), events);
}

void testChangeCallbackGetsLevelFromIntcap(void) {
    attachChip();
    DpadPin down(DOWN);
    std::vector<int> levels;
    mcp->interruptOnChange(DOWN, [&](uint8_t level) { levels.push_back(level); });
    chip.press(DOWN);
    chip.release(DOWN);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(2, levels.size());
    TEST_ASSERT_EQUAL(0, levels[0]);
    TEST_ASSERT_EQUAL(1, levels[1]);
}

void testIntccSetClearsOnIntcapRead(void) {
    attachChip();
    mcp->setIocon(MCP23009Config().setIntcc());
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_HEX8(0, chip.regs[MCP23009_INTF]);
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}, {"UP", 1, 1}}), events);
}

void testActiveLowPinInvertsCapturedLevel(void) {
    attachChip();
    MCP23009ActiveLowPin up(*mcp, UP, MCP23009Pin::IN, MCP23009Pin::PULL_UP);
    std::vector<int> levels;
    up.irq([&]() { levels.push_back(up.irqLevel()); });  // both edges by default
    TEST_ASSERT_EQUAL_UINT8(MCP23009Pin::IRQ_LEVEL_NONE, up.irqLevel());
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(2, levels.size());
    TEST_ASSERT_EQUAL(1, levels[0]);  // pressed = GPIO low = logical 1
    TEST_ASSERT_EQUAL(0, levels[1]);
}

// ----- Edge filters -----

void testFallingOnlyHandler(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), MCP23009Pin::IRQ_FALLING);
    chip.press(UP);
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 1}}), events);
}

void testRisingOnlyHandler(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), MCP23009Pin::IRQ_RISING);
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 1, 1}}), events);
}

void testDeviceCallbacksGetChangesFoundInGpio(void) {
    attachChip();
    std::vector<std::string> calls;
    DpadPin up(UP);
    DpadPin left(LEFT);
    mcp->interruptOnChange(UP,
                           [&](uint8_t level) { calls.push_back("UP" + std::to_string(level)); });
    mcp->interruptOnFalling(LEFT, [&]() { calls.push_back("fall"); });
    mcp->interruptOnRising(LEFT, [&]() { calls.push_back("rise"); });
    chip.press(UP);
    chip.press(LEFT);
    chip.release(LEFT);
    chip.press(LEFT);  // pressed at the read, after a missed tap
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(2, calls.size());
    TEST_ASSERT_EQUAL_STRING("UP0", calls[0].c_str());
    TEST_ASSERT_EQUAL_STRING("fall", calls[1].c_str());
}

// ----- Registration -----

void testRegisteringDpadHandlersFiresNoEvent(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN), left(LEFT), right(RIGHT);
    chip.reference = 0x00;  // port value before the pull-ups were set
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);
    right.pin.irq(recorder(events, "RIGHT", right.pin), BOTH);
    mcp->interruptEvent();  // the deferred poll(), if INT was raised
    TEST_ASSERT_EQUAL(0, events.size());
    TEST_ASSERT_EQUAL_HEX8(0, chip.regs[MCP23009_INTF]);
}

void reregisterWithPendingPress() {
    std::vector<Event> first;
    std::vector<Event> second;
    DpadPin up(UP);
    up.pin.irq(recorder(first, "first", up.pin), BOTH);
    chip.press(UP);  // poll() has not run yet
    TEST_ASSERT_EQUAL_HEX8(1 << UP, chip.regs[MCP23009_INTF]);
    up.pin.irq(recorder(second, "second", up.pin), BOTH);
    TEST_ASSERT_EQUAL_HEX8(0, chip.regs[MCP23009_INTF]);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(0, first.size());
    TEST_ASSERT_EQUAL(0, second.size());
}

void testReregistrationClearsPendingInterrupt(void) {
    attachChip();
    reregisterWithPendingPress();
}

void testReregistrationClearsPendingInterruptWithIntcc(void) {
    attachChip();
    mcp->setIocon(MCP23009Config().setIntcc());
    reregisterWithPendingPress();
}

void testCallbackStoredAndInterruptClearedBeforeGpinten(void) {
    // A poll() that runs as GPINTEN is written finds no stale interrupt.
    attachChip();
    std::vector<Event> events;
    std::vector<uint8_t> intfAtEnable;
    DpadPin up(UP);
    chip.reference = 0x00;  // a change latched before registration
    chip.onWriteHook = [&](uint8_t reg, uint8_t value) {
        if (reg == MCP23009_GPINTEN && (value & (1 << UP))) {
            intfAtEnable.push_back(chip.regs[MCP23009_INTF]);
            mcp->interruptEvent();
        }
    };
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.onWriteHook = nullptr;
    TEST_ASSERT_EQUAL(1, intfAtEnable.size());
    TEST_ASSERT_EQUAL_HEX8(0, intfAtEnable[0]);
    TEST_ASSERT_EQUAL(0, events.size());
}

void testPressAtEnableTimeReachesNewHandler(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    chip.onWriteHook = [&](uint8_t reg, uint8_t value) {
        if (reg == MCP23009_GPINTEN && (value & (1 << UP))) {
            chip.onWriteHook = nullptr;
            chip.press(UP);
            mcp->interruptEvent();
        }
    };
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}}), events);
}

void testIntconWrittenBeforeGpinten(void) {
    attachChip();
    DpadPin up(UP);
    chip.log.clear();
    mcp->interruptOnChange(UP, [](uint8_t) {});
    int intconAt = -1;
    int gpintenAt = -1;
    int intcapReadAt = -1;
    for (size_t i = 0; i < chip.log.size(); ++i) {
        if (chip.log[i].first == 'w' && chip.log[i].second == MCP23009_INTCON && intconAt < 0) {
            intconAt = static_cast<int>(i);
        }
        if (chip.log[i].first == 'w' && chip.log[i].second == MCP23009_GPINTEN && gpintenAt < 0) {
            gpintenAt = static_cast<int>(i);
        }
        if (chip.log[i].first == 'r' && chip.log[i].second == MCP23009_INTCAP && intcapReadAt < 0) {
            intcapReadAt = static_cast<int>(i);
        }
    }
    TEST_ASSERT_TRUE(intconAt >= 0);
    TEST_ASSERT_TRUE(intconAt < gpintenAt);
    // The pending interrupt is read (and cleared) before GPINTEN is written.
    TEST_ASSERT_TRUE(intcapReadAt >= 0);
    TEST_ASSERT_TRUE(intcapReadAt < gpintenAt);
}

void testPendingEventOnOtherPinIsKept(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin down(DOWN), up(UP);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(DOWN);  // poll() has not run yet
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}}), events);
}

void testHandlerRunAtRegistrationCanEnableAnotherPin(void) {
    // A handler called during a registration enables DOWN: the
    // registration must not write back an older GPINTEN without DOWN.
    attachChip();
    std::vector<Event> events;
    DpadPin left(LEFT), down(DOWN), right(RIGHT);
    left.pin.irq(
        [&]() {
            events.push_back({"LEFT", left.pin.irqLevel(), -1});
            down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
        },
        BOTH);
    chip.press(LEFT);  // pending when RIGHT is registered
    right.pin.irq(recorder(events, "RIGHT", right.pin), BOTH);
    ASSERT_EVENTS((std::vector<Event>{{"LEFT", 0, -1}}), events);
    TEST_ASSERT_EQUAL_HEX8((1 << LEFT) | (1 << DOWN) | (1 << RIGHT), chip.regs[MCP23009_GPINTEN]);
    chip.press(DOWN);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"LEFT", 0, -1}, {"DOWN", 0, 0}}), events);
}

void testHandlerRunAtRegistrationCanDisableItsPin(void) {
    attachChip();
    std::vector<std::string> events;
    DpadPin left(LEFT), right(RIGHT);
    left.pin.irq(
        [&]() {
            events.push_back("LEFT");
            mcp->disableInterrupt(LEFT);
        },
        BOTH);
    chip.press(LEFT);
    right.pin.irq([&]() { events.push_back("RIGHT"); }, BOTH);
    TEST_ASSERT_EQUAL(1, events.size());
    TEST_ASSERT_EQUAL_HEX8(1 << RIGHT, chip.regs[MCP23009_GPINTEN]);
}

void testDisableInterruptStopsCallbacks(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    mcp->disableInterrupt(UP);
    chip.press(DOWN);
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    chip.release(DOWN);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}, {"DOWN", 1, 1}}), events);
}

void testDisableWithPendingInterruptReportsNothing(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);  // pending
    mcp->disableInterrupt(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(0, events.size());
}

void testReenabledPinTakesItsLevelAgain(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    mcp->disableInterrupt(UP);
    chip.press(UP);  // not reported: disabled
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(0, events.size());
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 1, 1}}), events);
}

void testRegistrationTakesHeldLevelWithoutEvent(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin left(LEFT), up(UP);
    chip.press(LEFT);  // held before the registration
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}}), events);
    chip.release(LEFT);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}, {"LEFT", 1, 1}}), events);
}

// ----- Changes the chip misses while an interrupt is pending -----

void testV1cLostReleaseDuringPendingPress(void) {
    // V1c measurement (a): LEFT pressed, then UP pressed; LEFT released
    // while the UP interrupt is pending. The chip never signals the LEFT
    // release: the driver reports it from GPIO, after the UP press.
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN), left(LEFT), right(RIGHT);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);
    right.pin.irq(recorder(events, "RIGHT", right.pin), BOTH);
    chip.press(LEFT);
    mcp->interruptEvent();
    chip.press(UP);
    chip.release(LEFT);  // INTF is set (UP): no interrupt for LEFT
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    chip.press(LEFT);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{
                      {"LEFT", 0, 0}, {"UP", 0, 0}, {"LEFT", 1, 1}, {"UP", 1, 1}, {"LEFT", 0, 0}}),
                  events);
}

void testV1cLostPressDuringPendingPress(void) {
    // V1c measurement (b): RIGHT pressed while the UP interrupt is pending.
    // Without the GPIO comparison, the release came with no press before it.
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), right(RIGHT);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    right.pin.irq(recorder(events, "RIGHT", right.pin), BOTH);
    chip.press(RIGHT);
    mcp->interruptEvent();
    chip.release(RIGHT);
    mcp->interruptEvent();
    chip.press(UP);
    chip.press(RIGHT);  // INTF is set (UP): no interrupt for RIGHT
    mcp->interruptEvent();
    chip.release(RIGHT);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"RIGHT", 0, 0},
                                      {"RIGHT", 1, 1},
                                      {"UP", 0, 0},
                                      {"RIGHT", 0, 0},
                                      {"RIGHT", 1, 1},
                                      {"UP", 1, 1}}),
                  events);
}

void testShortTapGivesPressThenRelease(void) {
    attachChip();
    DpadPin down(DOWN);
    std::vector<int> levels;
    down.pin.irq([&]() { levels.push_back(down.pin.irqLevel()); }, BOTH);
    chip.press(DOWN);
    chip.release(DOWN);
    mcp->interruptEvent();
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(2, levels.size());
    TEST_ASSERT_EQUAL(0, levels[0]);
    TEST_ASSERT_EQUAL(1, levels[1]);
}

void testCapturedLevelEqualToLastKnownIsNotReportedTwice(void) {
    // With IOCON.INTCC, reading INTCAP clears the interrupt: a press just
    // after it raises a new interrupt and is also read in GPIO. The press
    // is reported once, from GPIO; the next interrupt captures the same
    // level.
    attachChip();
    mcp->setIocon(MCP23009Config().setIntcc());
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(DOWN);
    chip.onReadHook = [&](uint8_t reg) {
        if (reg == MCP23009_INTCAP && (chip.pins & (1 << UP))) {
            chip.press(UP);
        }
    };
    mcp->interruptEvent();
    chip.onReadHook = nullptr;
    TEST_ASSERT_EQUAL_HEX8(1 << UP, chip.regs[MCP23009_INTF]);
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}, {"UP", 0, 0}}), events);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}, {"UP", 0, 0}}), events);
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}, {"UP", 0, 0}, {"UP", 1, 1}}), events);
}

void testFallingOnlyFiltersChangesFoundInGpio(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), MCP23009Pin::IRQ_FALLING);
    chip.press(UP);
    chip.press(DOWN);  // missed by the chip, found in GPIO
    mcp->interruptEvent();
    chip.release(UP);
    chip.release(DOWN);  // missed by the chip, found in GPIO
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}, {"DOWN", 0, 0}, {"UP", 1, 1}}), events);
}

void testPinNotInterruptEnabledIsNeverReported(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin down(DOWN), left(LEFT);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(DOWN);
    chip.press(LEFT);  // LEFT has no interrupt enabled
    mcp->interruptEvent();
    chip.release(LEFT);
    chip.release(DOWN);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}, {"DOWN", 1, 1}}), events);
}

// ----- Reads nested in a dispatch -----

void testNestedReadDuringDispatchReportsEachChangeOnce(void) {
    // A read made by a handler while the events of an earlier read are
    // delivered compares with the levels already updated, and its events
    // come after those still queued: no event twice, none lost.
    attachChip();
    std::vector<Event> events;
    DpadPin down(DOWN), up(UP), left(LEFT);
    bool nested = false;
    down.pin.irq(
        [&]() {
            events.push_back({"DOWN", down.pin.irqLevel(), -1});
            if (!nested) {
                nested = true;
                chip.release(DOWN);
                mcp->interruptEvent();
            }
        },
        BOTH);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(DOWN);
    chip.press(UP);                                          // missed by the chip, found in GPIO
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);  // reads the DOWN press
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, -1}, {"UP", 0, 0}, {"DOWN", 1, -1}}), events);
}

void testReleaseDuringDispatchComesAfterQueuedPress(void) {
    // DOWN is pending and LEFT is pressed but missed. The DOWN handler
    // releases LEFT, then registers UP: the LEFT press, then its release.
    attachChip();
    std::vector<Event> events;
    DpadPin down(DOWN), left(LEFT), up(UP);
    down.pin.irq(
        [&]() {
            events.push_back({"DOWN", down.pin.irqLevel(), -1});
            chip.release(LEFT);
            up.pin.irq(recorder(events, "UP", up.pin), BOTH);
        },
        BOTH);
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);
    chip.press(DOWN);
    chip.press(LEFT);  // missed by the chip
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, -1}, {"LEFT", 0, 1}, {"LEFT", 1, 1}}), events);
    chip.press(LEFT);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(4, events.size());
    TEST_ASSERT_TRUE((events[3] == Event{"LEFT", 0, 0}));
}

void testReregistrationInHandlerDropsQueuedEvents(void) {
    // A tap before the new irq() call does not reach the new handler, even
    // when its release is still queued by the read being delivered.
    attachChip();
    std::vector<int> first;
    std::vector<Event> second;
    DpadPin up(UP);
    up.pin.irq(
        [&]() {
            first.push_back(up.pin.irqLevel());
            up.pin.irq(recorder(second, "second", up.pin), BOTH);
        },
        BOTH);
    chip.press(UP);
    chip.release(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(1, first.size());
    TEST_ASSERT_EQUAL(0, first[0]);
    TEST_ASSERT_EQUAL(0, second.size());
    chip.press(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(1, first.size());
    ASSERT_EVENTS((std::vector<Event>{{"second", 0, 0}}), second);
}

void testManyNestedReadsDuringASlowHandlerLoseNoLevel(void) {
    // More nested reads than the queue holds: the changes that found no
    // room are read again from GPIO once the queue is empty, so the last
    // reported level is the real one.
    attachChip();
    std::vector<int> downs;
    DpadPin up(UP), down(DOWN);
    bool tapped = false;
    up.pin.irq(
        [&]() {
            if (tapped) {
                return;
            }
            tapped = true;
            for (int i = 0; i < 20; ++i) {
                chip.press(DOWN);
                mcp->interruptEvent();
                chip.release(DOWN);
                mcp->interruptEvent();
            }
            chip.press(DOWN);
            mcp->interruptEvent();
        },
        BOTH);
    down.pin.irq([&]() { downs.push_back(down.pin.irqLevel()); }, BOTH);
    chip.press(UP);
    mcp->interruptEvent();
    TEST_ASSERT_TRUE(downs.size() >= 2);
    for (size_t i = 1; i < downs.size(); ++i) {
        TEST_ASSERT_NOT_EQUAL(downs[i - 1], downs[i]);  // levels alternate
    }
    TEST_ASSERT_EQUAL(0, downs.front());
    TEST_ASSERT_EQUAL(0, downs.back());  // the real level: pressed
    chip.release(DOWN);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(1, downs.back());
    TEST_ASSERT_NOT_EQUAL(downs[downs.size() - 2], downs.back());
}

// ----- Bus errors -----

void testBusErrorGivesNoEventAndReturnsFalse(void) {
    // readReg() returns 0 on a bus error: for active-low buttons, an INTCAP
    // or GPIO read as 0 would look like every button pressed.
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(UP);
    chip.failReads = true;
    TEST_ASSERT_FALSE(mcp->interruptEvent());
    TEST_ASSERT_EQUAL(0, events.size());
    chip.failReads = false;
    // The levels did not move: the next good read reports the press only.
    TEST_ASSERT_TRUE(mcp->interruptEvent());
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}}), events);
}

// ----- Re-registration and reset -----

void testSwitchFromBothEdgesToFallingOnly(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    up.pin.irq(recorder(events, "UP", up.pin), MCP23009Pin::IRQ_FALLING);
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 0}}), events);
}

void testHardwareResetForgetsEnabledPins(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    mcp->reset();
    chip.regs[MCP23009_GPINTEN] = 0;  // what the reset pulse does to the chip
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(DOWN);
    chip.press(UP);  // no longer watched
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}}), events);
}

void testPowerOffForgetsEnabledPins(void) {
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP), down(DOWN);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    mcp->powerOff();
    mcp->powerOn();
    chip.regs[MCP23009_GPINTEN] = 0;
    down.pin.irq(recorder(events, "DOWN", down.pin), BOTH);
    chip.press(DOWN);
    chip.press(UP);  // no longer watched
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"DOWN", 0, 0}}), events);
}

// ----- poll() -----

void testPollWithoutInterruptPinReadsChip(void) {
    // Without an MCU interrupt line, poll() reads the chip at each call,
    // so callbacks still run when the sketch only polls.
    attachChip();
    std::vector<Event> events;
    DpadPin up(UP);
    up.pin.irq(recorder(events, "UP", up.pin), BOTH);
    chip.press(UP);
    chip.release(UP);
    mcp->poll();
    ASSERT_EVENTS((std::vector<Event>{{"UP", 0, 1}, {"UP", 1, 1}}), events);
}

void testPollWithoutInterruptPinAndNoEnabledPinDoesNoI2c(void) {
    attachChip();
    size_t before = Wire.totalTransactions();
    mcp->poll();
    TEST_ASSERT_EQUAL(before, Wire.totalTransactions());
}

// ----- interruptOnRaising() alias (#183) -----

void testInterruptOnRaisingAliasCallsInterruptOnRising(void) {
    attachChip();
    DpadPin up(UP);
    int rises = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    mcp->interruptOnRaising(UP, [&]() { ++rises; });
#pragma GCC diagnostic pop
    TEST_ASSERT_EQUAL_HEX8(1 << UP, chip.regs[MCP23009_GPINTEN]);
    chip.press(UP);
    mcp->interruptEvent();
    chip.release(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL(1, rises);
}

// ----- watchChanges(): a callback the sketch does not replace -----

// Events given to the watch: "<pin><level> " each.
std::string watchedEvents;

std::string ev(uint8_t gpx, uint8_t level) {
    return std::to_string(gpx) + std::to_string(level) + " ";
}

void watchRecorder(uint8_t gpx, uint8_t level) {
    watchedEvents += ev(gpx, level);
}

std::function<void(uint8_t)> levelRecorder(std::vector<int>& levels) {
    return [&levels](uint8_t level) { levels.push_back(level); };
}

void attachWatchedChip(uint8_t mask) {
    attachChip();
    watchedEvents.clear();
    for (uint8_t i = 0; i < 8; i++) {
        if ((mask & (1 << i)) != 0) {
            mcp->setup(i, MCP23009_DIR_INPUT, MCP23009_PULLUP);
        }
    }
    mcp->watchChanges(mask, watchRecorder);
}

void testWatchGetsEventsBesideTheSketchCallbacks(void) {
    attachWatchedChip((1 << UP) | (1 << DOWN));
    TEST_ASSERT_EQUAL_HEX8((1 << UP) | (1 << DOWN), mcp->watched());
    TEST_ASSERT_EQUAL_HEX8((1 << UP) | (1 << DOWN), chip.regs[MCP23009_GPINTEN]);
    std::vector<int> sketch;
    mcp->interruptOnChange(UP, levelRecorder(sketch));  // does not replace the watch
    chip.press(UP);
    chip.release(UP);
    mcp->interruptEvent();
    chip.press(DOWN);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_STRING((ev(UP, 0) + ev(UP, 1) + ev(DOWN, 0)).c_str(), watchedEvents.c_str());
    TEST_ASSERT_EQUAL(2, sketch.size());
    TEST_ASSERT_EQUAL(0, sketch[0]);
    TEST_ASSERT_EQUAL(1, sketch[1]);
}

void testSketchRegistrationLeavesThePendingEventToTheWatch(void) {
    attachWatchedChip(1 << UP);
    int falls = 0;
    chip.press(UP);  // pending when the sketch registers
    mcp->interruptOnFalling(UP, [&falls]() { falls++; });
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_STRING(ev(UP, 0).c_str(), watchedEvents.c_str());
    TEST_ASSERT_EQUAL(0, falls);  // a change made before the registration
    chip.release(UP);
    mcp->interruptEvent();
    chip.press(UP);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_STRING((ev(UP, 0) + ev(UP, 1) + ev(UP, 0)).c_str(), watchedEvents.c_str());
    TEST_ASSERT_EQUAL(1, falls);
}

void testDisableKeepsAWatchedPinEnabled(void) {
    attachWatchedChip(1 << DOWN);
    std::vector<int> sketch;
    mcp->interruptOnChange(DOWN, levelRecorder(sketch));
    mcp->disableInterrupt(DOWN);
    TEST_ASSERT_EQUAL_HEX8(1 << DOWN, mcp->watched());
    TEST_ASSERT_EQUAL_HEX8(1 << DOWN, chip.regs[MCP23009_GPINTEN]);
    chip.press(DOWN);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_STRING(ev(DOWN, 0).c_str(), watchedEvents.c_str());
    TEST_ASSERT_EQUAL(0, sketch.size());
}

void testWatchOnAPinOfTheSketchKeepsItsCallbacks(void) {
    attachChip();
    watchedEvents.clear();
    std::vector<Event> events;
    DpadPin left(LEFT);
    left.pin.irq(recorder(events, "LEFT", left.pin), BOTH);
    chip.press(LEFT);  // pending when the watch starts: for the sketch only
    mcp->watchChanges(1 << LEFT, watchRecorder);
    chip.release(LEFT);
    mcp->interruptEvent();
    ASSERT_EVENTS((std::vector<Event>{{"LEFT", 0, 1}, {"LEFT", 1, 1}}), events);
    TEST_ASSERT_EQUAL_STRING((ev(LEFT, 0) + ev(LEFT, 1)).c_str(), watchedEvents.c_str());
}

void testWatchRemovedByNullptrAndByReset(void) {
    attachWatchedChip(1 << RIGHT);
    mcp->watchChanges(1 << RIGHT, nullptr);
    TEST_ASSERT_EQUAL_HEX8(0, mcp->watched());
    TEST_ASSERT_EQUAL_HEX8(1 << RIGHT, chip.regs[MCP23009_GPINTEN]);  // stays enabled
    chip.press(RIGHT);
    mcp->interruptEvent();
    TEST_ASSERT_EQUAL_STRING("", watchedEvents.c_str());
    mcp->watchChanges(1 << RIGHT, watchRecorder);
    TEST_ASSERT_EQUAL_HEX8(1 << RIGHT, mcp->watched());
    mcp->reset();
    TEST_ASSERT_EQUAL_HEX8(0, mcp->watched());
    mcp->watchChanges(1 << RIGHT, watchRecorder);
    mcp->powerOff();
    TEST_ASSERT_EQUAL_HEX8(0, mcp->watched());
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
    RUN_TEST(testGpioReadsPort);
    RUN_TEST(testGpioReportsBusErrorWhenAbsent);
    RUN_TEST(testGpioReportsEndTransmissionError);
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
    RUN_TEST(testInterruptOnRisingEnablesInterruptRegister);
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
    RUN_TEST(testSetLevelReadsFromOlatNotGpio);
    RUN_TEST(testInterruptEventUsesIntcapNotGpio);
    RUN_TEST(testPollIsNoOpWhenNoPending);
    RUN_TEST(testPinDestructorClearsExpanderCallbackSlot);
    RUN_TEST(testIrqLevelIsNoneBeforeAnyInterrupt);
    RUN_TEST(testHandlerSeesPressReleasedBeforeItRuns);
    RUN_TEST(testPressThenReleaseEachReported);
    RUN_TEST(testChangeCallbackGetsLevelFromIntcap);
    RUN_TEST(testIntccSetClearsOnIntcapRead);
    RUN_TEST(testActiveLowPinInvertsCapturedLevel);
    RUN_TEST(testFallingOnlyHandler);
    RUN_TEST(testRisingOnlyHandler);
    RUN_TEST(testDeviceCallbacksGetChangesFoundInGpio);
    RUN_TEST(testRegisteringDpadHandlersFiresNoEvent);
    RUN_TEST(testReregistrationClearsPendingInterrupt);
    RUN_TEST(testReregistrationClearsPendingInterruptWithIntcc);
    RUN_TEST(testCallbackStoredAndInterruptClearedBeforeGpinten);
    RUN_TEST(testPressAtEnableTimeReachesNewHandler);
    RUN_TEST(testIntconWrittenBeforeGpinten);
    RUN_TEST(testPendingEventOnOtherPinIsKept);
    RUN_TEST(testHandlerRunAtRegistrationCanEnableAnotherPin);
    RUN_TEST(testHandlerRunAtRegistrationCanDisableItsPin);
    RUN_TEST(testDisableInterruptStopsCallbacks);
    RUN_TEST(testDisableWithPendingInterruptReportsNothing);
    RUN_TEST(testReenabledPinTakesItsLevelAgain);
    RUN_TEST(testRegistrationTakesHeldLevelWithoutEvent);
    RUN_TEST(testV1cLostReleaseDuringPendingPress);
    RUN_TEST(testV1cLostPressDuringPendingPress);
    RUN_TEST(testShortTapGivesPressThenRelease);
    RUN_TEST(testCapturedLevelEqualToLastKnownIsNotReportedTwice);
    RUN_TEST(testFallingOnlyFiltersChangesFoundInGpio);
    RUN_TEST(testPinNotInterruptEnabledIsNeverReported);
    RUN_TEST(testNestedReadDuringDispatchReportsEachChangeOnce);
    RUN_TEST(testReleaseDuringDispatchComesAfterQueuedPress);
    RUN_TEST(testReregistrationInHandlerDropsQueuedEvents);
    RUN_TEST(testManyNestedReadsDuringASlowHandlerLoseNoLevel);
    RUN_TEST(testBusErrorGivesNoEventAndReturnsFalse);
    RUN_TEST(testSwitchFromBothEdgesToFallingOnly);
    RUN_TEST(testHardwareResetForgetsEnabledPins);
    RUN_TEST(testPowerOffForgetsEnabledPins);
    RUN_TEST(testPollWithoutInterruptPinReadsChip);
    RUN_TEST(testPollWithoutInterruptPinAndNoEnabledPinDoesNoI2c);
    RUN_TEST(testInterruptOnRaisingAliasCallsInterruptOnRising);
    RUN_TEST(testWatchGetsEventsBesideTheSketchCallbacks);
    RUN_TEST(testSketchRegistrationLeavesThePendingEventToTheWatch);
    RUN_TEST(testDisableKeepsAWatchedPinEnabled);
    RUN_TEST(testWatchOnAPinOfTheSketchKeepsItsCallbacks);
    RUN_TEST(testWatchRemovedByNullptrAndByReset);
    return UNITY_END();
}