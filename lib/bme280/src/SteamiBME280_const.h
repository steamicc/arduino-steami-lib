// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// 7-bit I2C addresses. The STeaMi V1c ties SDO low, which selects 0x76.
constexpr uint8_t BME280_DEFAULT_ADDRESS = 0x76;
constexpr uint8_t BME280_ALT_ADDRESS = 0x77;  // SDO tied to VDDIO

// Chip ID register fixed value — used by begin() to probe the device.
constexpr uint8_t BME280_CHIP_ID_VALUE = 0x60;

// Register map (Bosch BME280 datasheet, section 5.3).
constexpr uint8_t BME280_REG_CALIB_TEMP_PRESS = 0x88;  // 26 bytes: T1..T3, P1..P9, -, H1
constexpr uint8_t BME280_REG_CHIP_ID = 0xD0;
constexpr uint8_t BME280_REG_SOFT_RESET = 0xE0;
constexpr uint8_t BME280_REG_CALIB_HUM = 0xE1;  // 7 bytes: H2..H6
constexpr uint8_t BME280_REG_CTRL_HUM = 0xF2;
constexpr uint8_t BME280_REG_STATUS = 0xF3;
constexpr uint8_t BME280_REG_CTRL_MEAS = 0xF4;
constexpr uint8_t BME280_REG_CONFIG = 0xF5;
constexpr uint8_t BME280_REG_DATA_START = 0xF7;  // press[3], temp[3], hum[2]

constexpr uint8_t BME280_CALIB_TP_SIZE = 26;
constexpr uint8_t BME280_CALIB_H_SIZE = 7;
constexpr uint8_t BME280_DATA_BLOCK_SIZE = 8;

// Writing this value to SOFT_RESET restarts the chip (power-on defaults).
constexpr uint8_t BME280_SOFT_RESET_CMD = 0xB6;

// STATUS bits.
constexpr uint8_t BME280_STATUS_MEASURING = 0x08;  // conversion running
constexpr uint8_t BME280_STATUS_IM_UPDATE = 0x01;  // NVM copy running

// CTRL_MEAS mode[1:0].
constexpr uint8_t BME280_MODE_SLEEP = 0x00;
constexpr uint8_t BME280_MODE_FORCED = 0x01;
constexpr uint8_t BME280_MODE_NORMAL = 0x03;
constexpr uint8_t BME280_MODE_MASK = 0x03;

// Oversampling values for CTRL_HUM osrs_h[2:0] and CTRL_MEAS osrs_t / osrs_p.
constexpr uint8_t BME280_OSRS_SKIP = 0x00;  // channel disabled
constexpr uint8_t BME280_OSRS_X1 = 0x01;
constexpr uint8_t BME280_OSRS_X2 = 0x02;
constexpr uint8_t BME280_OSRS_X4 = 0x03;
constexpr uint8_t BME280_OSRS_X8 = 0x04;
constexpr uint8_t BME280_OSRS_X16 = 0x05;
constexpr uint8_t BME280_OSRS_MASK = 0x07;
// Pass to setOversampling() to leave a channel unchanged.
constexpr uint8_t BME280_OSRS_KEEP = 0xFF;

// CTRL_MEAS bit positions.
constexpr uint8_t BME280_OSRS_T_SHIFT = 5;
constexpr uint8_t BME280_OSRS_P_SHIFT = 2;

// CONFIG filter[2:0] (bits 4:2): IIR filter coefficient.
constexpr uint8_t BME280_FILTER_OFF = 0x00;
constexpr uint8_t BME280_FILTER_2 = 0x01;
constexpr uint8_t BME280_FILTER_4 = 0x02;
constexpr uint8_t BME280_FILTER_8 = 0x03;
constexpr uint8_t BME280_FILTER_16 = 0x04;
constexpr uint8_t BME280_FILTER_MASK = 0x07;
constexpr uint8_t BME280_FILTER_SHIFT = 2;

// CONFIG t_sb[2:0] (bits 7:5): standby time between two conversions in normal mode.
constexpr uint8_t BME280_STANDBY_0_5_MS = 0x00;
constexpr uint8_t BME280_STANDBY_62_5_MS = 0x01;
constexpr uint8_t BME280_STANDBY_125_MS = 0x02;
constexpr uint8_t BME280_STANDBY_250_MS = 0x03;
constexpr uint8_t BME280_STANDBY_500_MS = 0x04;
constexpr uint8_t BME280_STANDBY_1000_MS = 0x05;
constexpr uint8_t BME280_STANDBY_10_MS = 0x06;
constexpr uint8_t BME280_STANDBY_20_MS = 0x07;
constexpr uint8_t BME280_STANDBY_MASK = 0x07;
constexpr uint8_t BME280_STANDBY_SHIFT = 5;

// Raw ADC value reported for a channel whose oversampling is "skip".
constexpr int32_t BME280_RAW_SKIPPED_20BIT = 0x80000;
constexpr int32_t BME280_RAW_SKIPPED_16BIT = 0x8000;

// Timing.
constexpr uint8_t BME280_RESET_DELAY_MS = 10;
constexpr uint8_t BME280_BOOT_TIMEOUT_MS = 50;

// Standard-atmosphere sea-level pressure used by altitude() by default.
constexpr float BME280_SEA_LEVEL_HPA = 1013.25f;
