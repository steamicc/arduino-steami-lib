// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Default 7-bit I2C address. Pin SA0 is tied low on the STeaMi board.
constexpr uint8_t HTS221_DEFAULT_ADDRESS = 0x5F;

// WHO_AM_I register fixed value — used by begin() to probe the device.
constexpr uint8_t HTS221_WHO_AM_I_VALUE = 0xBC;

// Register map (ST HTS221 datasheet, rev 5, table 17).
constexpr uint8_t HTS221_REG_WHO_AM_I = 0x0F;
constexpr uint8_t HTS221_REG_AV_CONF = 0x10;
constexpr uint8_t HTS221_REG_CTRL1 = 0x20;
constexpr uint8_t HTS221_REG_CTRL2 = 0x21;
constexpr uint8_t HTS221_REG_CTRL3 = 0x22;
constexpr uint8_t HTS221_REG_STATUS = 0x27;
constexpr uint8_t HTS221_REG_HUMIDITY_OUT_L = 0x28;
constexpr uint8_t HTS221_REG_TEMP_OUT_L = 0x2A;

// Calibration block (0x30-0x3F).
constexpr uint8_t HTS221_REG_H0_RH_X2 = 0x30;
constexpr uint8_t HTS221_REG_H1_RH_X2 = 0x31;
constexpr uint8_t HTS221_REG_T0_DEGC_X8 = 0x32;
constexpr uint8_t HTS221_REG_T1_DEGC_X8 = 0x33;
constexpr uint8_t HTS221_REG_T1_T0_MSB = 0x35;
constexpr uint8_t HTS221_REG_H0_T0_OUT_L = 0x36;
constexpr uint8_t HTS221_REG_H1_T0_OUT_L = 0x3A;
constexpr uint8_t HTS221_REG_T0_OUT_L = 0x3C;
constexpr uint8_t HTS221_REG_T1_OUT_L = 0x3E;

// CTRL_REG1 bits.
constexpr uint8_t HTS221_CTRL1_PD = 0x80;   // 1 = active, 0 = power-down
constexpr uint8_t HTS221_CTRL1_BDU = 0x04;  // Block Data Update
constexpr uint8_t HTS221_CTRL1_ODR_MASK = 0x03;

// CTRL_REG2 bits.
constexpr uint8_t HTS221_CTRL2_BOOT = 0x80;      // Reload calibration trimming
constexpr uint8_t HTS221_CTRL2_HEATER = 0x02;    // Integrated heater
constexpr uint8_t HTS221_CTRL2_ONE_SHOT = 0x01;  // Trigger a single measurement

// STATUS_REG bits.
constexpr uint8_t HTS221_STATUS_H_DA = 0x02;  // Humidity data available
constexpr uint8_t HTS221_STATUS_T_DA = 0x01;  // Temperature data available

// Output Data Rate values for CTRL_REG1 ODR[1:0].
constexpr uint8_t HTS221_ODR_ONE_SHOT = 0x00;
constexpr uint8_t HTS221_ODR_1_HZ = 0x01;
constexpr uint8_t HTS221_ODR_7_HZ = 0x02;
constexpr uint8_t HTS221_ODR_12_5_HZ = 0x03;

// MSB of the sub-address enables register auto-increment on burst reads.
constexpr uint8_t HTS221_AUTO_INCREMENT = 0x80;
