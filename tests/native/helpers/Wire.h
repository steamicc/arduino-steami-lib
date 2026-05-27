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
    }

    size_t write(uint8_t value) {
        txBuffer_.push_back(value);
        return 1;
    }

    uint8_t endTransmission(bool = true) {
        // Tests that want to exercise the I2C error path opt-in via
        // setEndTransmissionResult(non-zero). Default = success.
        if (endTransmissionResult_ != 0) {
            txBuffer_.clear();
            return endTransmissionResult_;
        }
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
            currentRegisterByAddr_[currentAddress_] = txBuffer_[0];
        }
        return 0;
    }

    // Force endTransmission() to fail with the given Arduino I2C error
    // code (1 = data too long, 2 = NACK on addr, 3 = NACK on data, 4 = other).
    void setEndTransmissionResult(uint8_t result) { endTransmissionResult_ = result; }

    uint8_t requestFrom(uint8_t address, uint8_t quantity) {
        rxBuffer_.clear();
        uint8_t reg = currentRegisterByAddr_[address];

        if (reg == 0 && quantity == 2) {
            uint16_t subcommand = (static_cast<uint16_t>(registers_[makeKey(address, 1)]) << 8) |
                                  registers_[makeKey(address, 0)];
            for (const auto& resp : controlResponses_) {
                if (resp.subcommand == subcommand) {
                    rxBuffer_.push_back(resp.response & 0xFF);
                    rxBuffer_.push_back((resp.response >> 8) & 0xFF);
                    rxIndex_ = 0;
                    return quantity;
                }
            }
        }

        for (uint8_t i = 0; i < quantity; ++i) {
            rxBuffer_.push_back(registers_[makeKey(address, reg + i)]);
        }
        rxIndex_ = 0;
        return quantity;
    }

    struct ControlWordResponse {
        uint16_t subcommand;
        uint16_t response;
    };

    std::vector<ControlWordResponse> controlResponses_;

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

    struct WriteOp {
        uint8_t address;
        uint8_t reg;
        uint8_t value;
    };

    const std::vector<WriteOp>& getWrites() const { return writes_; }

    void clearWrites() { writes_.clear(); }

    void setControlResponse(uint16_t subcommand, uint16_t response) {
        for (auto& resp : controlResponses_) {
            if (resp.subcommand == subcommand) {
                resp.response = response;
                return;
            }
        }
        controlResponses_.push_back({subcommand, response});
    }

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
    std::vector<WriteOp> writes_;
    uint8_t endTransmissionResult_ = 0;
};

inline TwoWire Wire;
