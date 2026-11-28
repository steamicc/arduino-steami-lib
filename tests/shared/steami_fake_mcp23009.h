// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Behavioural model of the MCP23009E interrupt logic, for the native tests.
//
// Port of FakeMCP23009 from the MicroPython suite
// (tests/test_mcp23009e_irq_mock.py, steamicc/micropython-steami-lib#466),
// aligned on the datasheet (DS20002121C, 1.7.3 to 1.7.5). The first
// change on an enabled pin sets INTF, copies the port into INTCAP and
// makes that port the compare reference ("the default state is reset
// after an Interrupt occurs"). Later changes, on any pin, raise no
// interrupt while INTF is set. Reading GPIO (or INTCAP when IOCON.INTCC
// is set) clears it; if the port then differs from the reference (a
// change made while INTF was set, still there), the interrupt sets again
// at once with a new INTCAP (1.7.4, note). A change made and undone
// while INTF was set leaves no trace. Attach it with
// Wire.attachDevice(MCP23009_I2C_ADDR, &chip).
//
// readUs: bus time of one register read (about 500 us at 100 kHz: the
// pointer write, then the byte), added to millis() through delay(). 0,
// the default, keeps the reads instantaneous.

#include <Wire.h>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "SteamiMCP23009E_const.h"

class FakeMCP23009 : public I2CDevice {
   public:
    static constexpr uint8_t kReleased = 0xF0;  // D-PAD: active low, with pull-ups

    uint8_t regs[11] = {};
    uint8_t pins = kReleased;
    // Port value at the last interrupt: the "previous value" reference of INTCON = 0.
    uint8_t reference = kReleased;
    bool failReads = false;
    // Registers whose reads fail (one bit per register address), on top of
    // failReads.
    uint16_t failReadRegs = 0;
    uint32_t readUs = 0;
    std::vector<std::pair<char, uint8_t>> log;
    // Run once a write or a read is applied, where a nested poll() could
    // run on the board.
    std::function<void(uint8_t, uint8_t)> onWriteHook;
    std::function<void(uint8_t)> onReadHook;

    void reset() {
        for (auto& r : regs) {
            r = 0;
        }
        regs[MCP23009_IODIR] = 0xFF;
        pins = kReleased;
        reference = kReleased;
        failReads = false;
        failReadRegs = 0;
        readUs = 0;
        busUs_ = 0;
        log.clear();
        onWriteHook = nullptr;
        onReadHook = nullptr;
        pointer_ = 0;
    }

    void press(uint8_t gp) {
        pins &= static_cast<uint8_t>(~(1 << gp));
        update();
    }

    void release(uint8_t gp) {
        pins |= static_cast<uint8_t>(1 << gp);
        update();
    }

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (frame.empty()) {
            return 0;  // address probe
        }
        pointer_ = frame[0];
        if (frame.size() < 2) {
            return 0;  // register pointer for a read
        }
        uint8_t reg = frame[0];
        log.push_back({'w', reg});
        regs[reg == MCP23009_GPIO ? MCP23009_OLAT : reg] = frame[1];
        update();
        auto hook = onWriteHook;
        if (hook) {
            hook(reg, frame[1]);
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t /*quantity*/) override {
        if (failReads || (failReadRegs & (1u << pointer_)) != 0) {
            return {};
        }
        busUs_ += readUs;
        if (busUs_ >= 1000) {
            const uint32_t ms = busUs_ / 1000;
            busUs_ %= 1000;
            delay(ms);  // the buttons may move meanwhile (clockHook())
        }
        uint8_t reg = pointer_;
        log.push_back({'r', reg});
        uint8_t value = (reg == MCP23009_GPIO) ? pins : regs[reg];
        uint8_t clearing =
            (regs[MCP23009_IOCON] & MCP23009_IOCON_INTCC) ? MCP23009_INTCAP : MCP23009_GPIO;
        if (reg == clearing) {
            clear();
        }
        auto hook = onReadHook;
        if (hook) {
            hook(reg);
        }
        return {value};
    }

   private:
    uint8_t pointer_ = 0;
    uint32_t busUs_ = 0;

    void update() {
        if (regs[MCP23009_INTF] != 0) {
            return;
        }
        uint8_t intcon = regs[MCP23009_INTCON];
        uint8_t changed =
            ((pins ^ reference) & ~intcon) | ((pins ^ regs[MCP23009_DEFVAL]) & intcon);
        changed &= regs[MCP23009_GPINTEN];
        if (changed != 0) {
            regs[MCP23009_INTF] = changed;
            regs[MCP23009_INTCAP] = pins;
            reference = pins;
        }
    }

    // A change made while INTF was set, and still there, sets the interrupt again. With no
    // interrupt set, the reference follows the port.
    void clear() {
        if (regs[MCP23009_INTF] == 0) {
            reference = pins;
        }
        regs[MCP23009_INTF] = 0;
        update();
    }
};
