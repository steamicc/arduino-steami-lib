// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Behavioural model of the ISM330DL for the native tests. The Wire register map cannot hold the
// driver's begin(): it writes SW_RESET to CTRL3_C and polls the register until the chip clears
// the bit, which a plain map never does.
//
// The model is 128 registers behind a register pointer with auto-increment (the IF_INC
// behaviour): the first byte of a write sets the pointer, the following bytes are stored from
// there, and a read returns bytes from the pointer on. A write to CTRL3_C stores the value then
// clears SW_RESET, as the reset completes at once. WHO_AM_I answers 0x6A and STATUS reports the
// accelerometer, gyroscope and temperature data ready.
//
// Attach it with Wire.attachDevice(ISM330DL_DEFAULT_ADDRESS, &fake) (0x6B).

#include <SteamiISM330DL.h>
#include <stdint.h>

#include <cstddef>
#include <vector>

#include "Wire.h"

class SteamiFakeIsm330dl : public I2CDevice {
   public:
    static constexpr size_t REGISTER_COUNT = 128;

    SteamiFakeIsm330dl() {
        registers[ISM330DL_REG_WHO_AM_I] = ISM330DL_WHO_AM_I_VALUE;
        registers[ISM330DL_REG_STATUS] = ISM330DL_STATUS_ALL_READY;
    }

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (frame.empty()) {
            return 0;  // address-only probe
        }
        pointer = frame[0] & MASK;
        for (size_t i = 1; i < frame.size(); i++) {
            uint8_t value = frame[i];
            if (pointer == ISM330DL_REG_CTRL3_C && (value & ISM330DL_CTRL3_C_SW_RESET) != 0) {
                resetWrites++;
                value &= static_cast<uint8_t>(~ISM330DL_CTRL3_C_SW_RESET);
            }
            registers[pointer] = value;
            pointer = (pointer + 1) & MASK;
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> out;
        for (size_t i = 0; i < quantity; i++) {
            out.push_back(registers[pointer]);
            pointer = (pointer + 1) & MASK;
        }
        return out;
    }

    /// Raw accelerometer sample, OUTX_L_XL to OUTZ_H_XL (16393 LSB is 1 g at +-2 g).
    void setAccelRaw(int16_t x, int16_t y, int16_t z) {
        setVector(ISM330DL_REG_OUTX_L_XL, x, y, z);
    }

    /// Raw gyroscope sample, OUTX_L_G to OUTZ_H_G (8.75 mdps per LSB at +-250 dps).
    void setGyroRaw(int16_t x, int16_t y, int16_t z) { setVector(ISM330DL_REG_OUTX_L_G, x, y, z); }

    uint8_t registers[REGISTER_COUNT] = {};
    uint8_t pointer = 0;
    /// CTRL3_C writes with SW_RESET set.
    int resetWrites = 0;

   private:
    static constexpr uint8_t MASK = REGISTER_COUNT - 1;

    void setLe16(uint8_t reg, int16_t value) {
        const uint16_t bits = static_cast<uint16_t>(value);
        registers[reg] = bits & 0xFF;
        registers[reg + 1] = (bits >> 8) & 0xFF;
    }

    void setVector(uint8_t reg, int16_t x, int16_t y, int16_t z) {
        setLe16(reg, x);
        setLe16(reg + 2, y);
        setLe16(reg + 4, z);
    }
};
