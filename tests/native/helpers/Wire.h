// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

class TwoWire {
   public:
    void begin() {}

    void beginTransmission(uint8_t address) {
        currentAddress_ = address;
        txBuffer_.clear();
        // A new transmission re-selects the response stream on the next
        // requestFrom(), so any in-flight stream is closed here.
        activeResponseByAddr_.erase(address);
    }

    size_t write(uint8_t value) {
        txBuffer_.push_back(value);
        return 1;
    }

    uint8_t endTransmission(bool = true) {
        if (txBuffer_.size() >= 2) {
            // [reg, val0, val1, ...] — I2C auto-increment: each value lands at
            // reg + offset, one WriteOp per byte so tests can assert the full
            // write sequence.
            uint8_t reg = txBuffer_[0];
            for (size_t i = 1; i < txBuffer_.size(); ++i) {
                uint8_t targetReg = static_cast<uint8_t>(reg + (i - 1));
                uint8_t val = txBuffer_[i];
                registers_[makeKey(currentAddress_, targetReg)] = val;
                writes_.push_back({currentAddress_, targetReg, val});
            }
            currentRegisterByAddr_[currentAddress_] = reg;
        } else if (txBuffer_.size() == 1) {
            uint8_t cmd = txBuffer_[0];
            commands_.push_back({currentAddress_, cmd});
            currentRegisterByAddr_[currentAddress_] = cmd;
        }
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t quantity) {
        // A single-byte transmission immediately followed by requestFrom
        // is an I2C register-pointer-select preceding a read, not a
        // standalone device command. Roll back the matching entry from
        // commands_ so tests scanning getCommands() for actuation
        // commands don't trip on pointer selects.
        if (!commands_.empty() && commands_.back().address == address) {
            commands_.pop_back();
        }
        rxBuffer_.clear();
        uint8_t reg = currentRegisterByAddr_[address];

        // Pick (or continue) a response stream for this address.
        // beginTransmission() resets the binding; the first requestFrom()
        // after a fresh select latches onto the response queue keyed by
        // the currently selected register. Subsequent chunks keep pulling
        // from the same queue, mirroring the bridge firmware which
        // streams its 256-byte TX buffer in one continuous read.
        auto activeIt = activeResponseByAddr_.find(address);
        if (activeIt == activeResponseByAddr_.end()) {
            const uint16_t respKey = makeKey(address, reg);
            if (responses_.count(respKey)) {
                activeResponseByAddr_[address] = respKey;
                activeIt = activeResponseByAddr_.find(address);
            }
        }

        for (uint8_t i = 0; i < quantity; ++i) {
            if (activeIt != activeResponseByAddr_.end()) {
                auto& queue = responses_[activeIt->second];
                auto& cursor = responseCursors_[activeIt->second];
                if (cursor < queue.size()) {
                    rxBuffer_.push_back(queue[cursor++]);
                    continue;
                }
            }
            rxBuffer_.push_back(registers_[makeKey(address, reg + i)]);
        }
        // Advance the per-address register cursor by `quantity` so that
        // successive requestFrom() calls without an intervening
        // beginTransmission stream contiguous data, matching the real
        // I2C auto-increment used by the DAPLink bridge response buffer.
        currentRegisterByAddr_[address] = static_cast<uint8_t>(reg + quantity);
        rxIndex_ = 0;
        return quantity;
    }

    int available() { return static_cast<int>(rxBuffer_.size() - rxIndex_); }

    int read() {
        if (rxIndex_ < rxBuffer_.size()) {
            return rxBuffer_[rxIndex_++];
        }
        return -1;
    }

    // Host-side helpers — not part of the real Arduino TwoWire API.

    void setRegister(uint8_t address, uint8_t reg, uint8_t value) {
        registers_[makeKey(address, reg)] = value;
    }

    uint8_t getRegister(uint8_t address, uint8_t reg) const {
        auto it = registers_.find(makeKey(address, reg));
        return (it != registers_.end()) ? it->second : 0x00;
    }

    // Pre-load a response payload streamed back when the next
    // requestFrom() targets `cmd` as the selected register. Lets
    // command-style protocols (DAPLink) stage response data without
    // colliding with payload bytes that sendCommand writes into the
    // register space at the same offsets.
    void setResponse(uint8_t address, uint8_t cmd, const std::vector<uint8_t>& data) {
        uint16_t key = makeKey(address, cmd);
        responses_[key] = data;
        responseCursors_[key] = 0;
    }

    struct WriteOp {
        uint8_t address;
        uint8_t reg;
        uint8_t value;
    };

    struct CommandOp {
        uint8_t address;
        uint8_t cmd;
    };

    const std::vector<WriteOp>& getWrites() const { return writes_; }

    void clearWrites() { writes_.clear(); }

    const std::vector<CommandOp>& getCommands() const { return commands_; }

    void clearCommands() { commands_.clear(); }

   private:
    static uint16_t makeKey(uint8_t addr, uint8_t reg) {
        return (static_cast<uint16_t>(addr) << 8) | reg;
    }

    uint8_t currentAddress_ = 0;
    std::map<uint8_t, uint8_t> currentRegisterByAddr_;
    std::vector<uint8_t> txBuffer_;
    std::vector<uint8_t> rxBuffer_;
    size_t rxIndex_ = 0;
    std::map<uint16_t, uint8_t> registers_;
    std::map<uint16_t, std::vector<uint8_t>> responses_;
    std::map<uint16_t, size_t> responseCursors_;
    std::map<uint8_t, uint16_t> activeResponseByAddr_;
    std::vector<WriteOp> writes_;
    std::vector<CommandOp> commands_;
};

inline TwoWire Wire;