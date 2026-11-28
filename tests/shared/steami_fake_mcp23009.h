// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Behavioural model of the MCP23009E interrupt logic, for the native tests.
//
// Port of FakeMCP23009 from the MicroPython suite
// (tests/test_mcp23009e_irq_mock.py, steamicc/micropython-steami-lib#466).
// It keeps the interrupt logic that matters: the first change on an
// enabled pin sets INTF and copies the port into INTCAP, later changes
// (on any pin) are ignored until the interrupt is cleared, and reading
// GPIO (or INTCAP when IOCON.INTCC is set) clears it and makes the
// current port the new compare reference, so a change ignored while INTF
// was set never raises an interrupt. Attach it with
// Wire.attachDevice(MCP23009_I2C_ADDR, &chip).

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
    // Port value at the last clear: the "previous state" reference.
    uint8_t reference = kReleased;
    bool failReads = false;
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
        if (failReads) {
            return {};
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
        }
    }

    void clear() {
        regs[MCP23009_INTF] = 0;
        reference = pins;
        update();
    }
};
