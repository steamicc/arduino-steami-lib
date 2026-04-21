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
        if (txBuffer_.size() >= 2) {
            uint8_t reg = txBuffer_[0];
            uint8_t val = txBuffer_[1];
            registers_[makeKey(currentAddress_, reg)] = val;
            writes_.push_back({currentAddress_, reg, val});
            currentRegisterByAddr_[currentAddress_] = reg;
        } else if (txBuffer_.size() == 1) {
            currentRegisterByAddr_[currentAddress_] = txBuffer_[0];
        }
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t quantity) {
        rxBuffer_.clear();
        uint8_t reg = currentRegisterByAddr_[address];
        for (uint8_t i = 0; i < quantity; ++i) {
            rxBuffer_.push_back(registers_[makeKey(address, reg + i)]);
        }
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

    struct WriteOp {
        uint8_t address;
        uint8_t reg;
        uint8_t value;
    };

    const std::vector<WriteOp>& getWrites() const { return writes_; }

    void clearWrites() { writes_.clear(); }

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
};

inline TwoWire Wire;
