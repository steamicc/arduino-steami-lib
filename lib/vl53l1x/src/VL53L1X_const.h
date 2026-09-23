// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// I2C address
constexpr uint8_t VL53L1X_I2C_DEFAULT_ADDR = 0x29;

// Device identification
constexpr uint16_t REG_MODEL_ID = 0x010F;
constexpr uint16_t VL53L1X_DEVICE_ID = 0xEACC;

// System control
constexpr uint16_t REG_SOFT_RESET = 0x0000;
constexpr uint8_t SOFT_RESET_ASSERT = 0x00;
constexpr uint8_t SOFT_RESET_RELEASE = 0x01;

// Default configuration start register
constexpr uint16_t REG_DEFAULT_CONFIG_START = 0x2D;

// Timing
constexpr uint16_t REG_RESULT_OSC_CALIBRATE_VAL = 0x0022;
constexpr uint16_t REG_RANGE_CONFIG_VCSEL_PERIOD_A = 0x001E;

// Ranging control
constexpr uint16_t REG_SYSTEM_START = 0x0087;
constexpr uint8_t RANGING_START = 0x40;
constexpr uint8_t RANGING_STOP = 0x00;

// Interrupt
constexpr uint16_t REG_GPIO_HV_MUX_CTRL = 0x0030;
constexpr uint8_t GPIO_HV_MUX_CTRL_POLARITY = 0x10;
constexpr uint16_t REG_GPIO_TIO_HV_STATUS = 0x0031;
constexpr uint8_t GPIO_TIO_HV_STATUS_DATA_READY = 0x01;
constexpr uint16_t REG_SYSTEM_INTERRUPT_CLEAR = 0x0086;
constexpr uint8_t INTERRUPT_CLEAR = 0x01;

// Result registers
constexpr uint16_t REG_RESULT_RANGE_STATUS = 0x0089;
constexpr uint8_t RESULT_BLOCK_SIZE = 17;
constexpr uint8_t RESULT_DISTANCE_MSB_OFFSET = 13;
constexpr uint8_t RESULT_DISTANCE_LSB_OFFSET = 14;