// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>

// I2C addresses and identity
constexpr uint8_t ISM330DL_I2C_ADDR_LOW = 0x6A;
constexpr uint8_t ISM330DL_I2C_ADDR_HIGH = 0x6B;
constexpr uint8_t ISM330DL_DEFAULT_ADDRESS = ISM330DL_I2C_ADDR_HIGH;
constexpr uint8_t ISM330DL_WHO_AM_I_VALUE = 0x6A;

// Registers
constexpr uint8_t ISM330DL_REG_WHO_AM_I = 0x0F;
constexpr uint8_t ISM330DL_REG_CTRL1_XL = 0x10;
constexpr uint8_t ISM330DL_REG_CTRL2_G = 0x11;
constexpr uint8_t ISM330DL_REG_CTRL3_C = 0x12;
constexpr uint8_t ISM330DL_REG_STATUS = 0x1E;
constexpr uint8_t ISM330DL_REG_OUT_TEMP_L = 0x20;
constexpr uint8_t ISM330DL_REG_OUTX_L_G = 0x22;
constexpr uint8_t ISM330DL_REG_OUTX_L_XL = 0x28;

// CTRL3_C bits
constexpr uint8_t ISM330DL_CTRL3_C_BDU = 1U << 6;
constexpr uint8_t ISM330DL_CTRL3_C_IF_INC = 1U << 2;
constexpr uint8_t ISM330DL_CTRL3_C_SW_RESET = 1U << 0;

// STATUS_REG bits
constexpr uint8_t ISM330DL_STATUS_TDA = 1U << 2;
constexpr uint8_t ISM330DL_STATUS_GDA = 1U << 1;
constexpr uint8_t ISM330DL_STATUS_XLDA = 1U << 0;
constexpr uint8_t ISM330DL_STATUS_ALL_READY =
    ISM330DL_STATUS_TDA | ISM330DL_STATUS_GDA | ISM330DL_STATUS_XLDA;

// Conversion constants
constexpr float ISM330DL_STANDARD_GRAVITY = 9.80665F;
constexpr float ISM330DL_DEG_TO_RAD = 0.01745329251994329577F;
constexpr float ISM330DL_TEMP_SENSITIVITY = 256.0F;
constexpr float ISM330DL_TEMP_OFFSET = 25.0F;
