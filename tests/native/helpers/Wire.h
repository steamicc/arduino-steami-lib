// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

class TwoWire {
   public:
    void begin() {}

    void setRegister(uint8_t reg, uint8_t value) { registers_[reg] = value; }

    void beginTransmission(uint8_t address) {
        currentAddress_ = address;
        txBuffer_.clear();
    }

    size_t write(uint8_t value) {
        txBuffer_.push_back(value);
        return 1;
    }

    uint8_t endTransmission(bool = true) {
        if (txBuffer_.size() == 1) {
            currentRegister_ = txBuffer_[0];
        } else if (txBuffer_.size() >= 2) {
            uint8_t reg = txBuffer_[0];
            uint8_t val = txBuffer_[1];
            registers_[reg] = val;
            currentRegister_ = reg;
        }
        return 0;
    }

    uint8_t requestFrom(uint8_t, uint8_t quantity) {
        rxBuffer_.clear();
        for (uint8_t i = 0; i < quantity; ++i) {
            rxBuffer_.push_back(registers_[currentRegister_ + i]);
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

   private:
    uint8_t currentAddress_ = 0;
    uint8_t currentRegister_ = 0;
    std::vector<uint8_t> txBuffer_;
    std::vector<uint8_t> rxBuffer_;
    size_t rxIndex_ = 0;
    std::map<uint8_t, uint8_t> registers_;
};

inline TwoWire Wire;
