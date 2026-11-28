// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// LIS2MDL I2C address
constexpr uint8_t LIS2MDL_I2C_ADDR = 0x1E;

// Register addresses
constexpr uint8_t LIS2MDL_WHO_AM_I = 0x4F;      // Device identification register
constexpr uint8_t LIS2MDL_WHO_AM_I_VAL = 0x40;  // Expected value of WHO_AM_I register
constexpr uint8_t LIS2MDL_CFG_REG_A = 0x60;     // Configuration register A
constexpr uint8_t LIS2MDL_CFG_REG_B = 0x61;     // Configuration register B
constexpr uint8_t LIS2MDL_CFG_REG_C = 0x62;     // Configuration register C
constexpr uint8_t LIS2MDL_STATUS_REG = 0x67;    // Status register

// Output data registers
constexpr uint8_t LIS2MDL_OUTX_L_REG = 0x68;  // X-axis output low byte
constexpr uint8_t LIS2MDL_OUTX_H_REG = 0x69;  // X-axis output high byte
constexpr uint8_t LIS2MDL_OUTY_L_REG = 0x6A;  // Y-axis output low byte
constexpr uint8_t LIS2MDL_OUTY_H_REG = 0x6B;  // Y-axis output high byte
constexpr uint8_t LIS2MDL_OUTZ_L_REG = 0x6C;  // Z-axis output low byte
constexpr uint8_t LIS2MDL_OUTZ_H_REG = 0x6D;  // Z-axis output high byte

// Offset registers
constexpr uint8_t LIS2MDL_OFFSET_X_REG_L = 0x45;  // X-axis offset low byte
constexpr uint8_t LIS2MDL_OFFSET_X_REG_H = 0x46;  // X-axis offset high byte
constexpr uint8_t LIS2MDL_OFFSET_Y_REG_L = 0x47;  // Y-axis offset low byte
constexpr uint8_t LIS2MDL_OFFSET_Y_REG_H = 0x48;  // Y-axis offset high byte
constexpr uint8_t LIS2MDL_OFFSET_Z_REG_L = 0x49;  // Z-axis offset low byte
constexpr uint8_t LIS2MDL_OFFSET_Z_REG_H = 0x4A;  // Z-axis offset high byte

// Temperature output registers
constexpr uint8_t LIS2MDL_TEMP_OUT_L_REG = 0x6E;  // Temperature output low byte
constexpr uint8_t LIS2MDL_TEMP_OUT_H_REG = 0x6F;  // Temperature output high byte

// Temperature conversion
constexpr int LIS2MDL_TEMP_SENSITIVITY = 8;  // LSB/°C
constexpr int LIS2MDL_TEMP_OFFSET = 25;      // °C (not guaranteed by datasheet, empirical default)

// Interrupt control and source registers
constexpr uint8_t LIS2MDL_INT_CTRL_REG = 0x63;    // Interrupt control register
constexpr uint8_t LIS2MDL_INT_SOURCE_REG = 0x64;  // Interrupt source register
constexpr uint8_t LIS2MDL_INT_THS_L_REG = 0x65;   // Interrupt threshold low byte
constexpr uint8_t LIS2MDL_INT_THS_H_REG = 0x66;   // Interrupt threshold high byte
// Axes on the STeaMi, board frame: R right of the screen, T top of the screen (side opposite the
// USB connector), U out of the screen. LIS2MDL: X = R and Y = T (measured on a V1c, see
// headingFlatOnly()), Z = LIS2MDL_MAG_Z_SIGN * U. ISM330DL: X = -T, Y = -R, Z = -U.
// Provisional: -1 awaits the V1c measurement of steamicc/micropython-steami-lib#344 (screen up
// and flat, the calibrated z is positive under the field of France when the sign is -1).
constexpr float LIS2MDL_MAG_Z_SIGN = -1.0f;
