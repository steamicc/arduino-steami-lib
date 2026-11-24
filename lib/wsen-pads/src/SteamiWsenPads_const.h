// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// ============================================================================
// I2C addressing
// ============================================================================

// 7-bit I2C address when SAO pin is tied low
constexpr uint8_t WSEN_PADS_I2C_ADDR_SAO_LOW = 0x5C;

// 7-bit I2C address when SAO pin is tied high
constexpr uint8_t WSEN_PADS_I2C_ADDR_SAO_HIGH = 0x5D;

// Default address recommended for a single device on the bus
constexpr uint8_t WSEN_PADS_I2C_DEFAULT_ADDR = WSEN_PADS_I2C_ADDR_SAO_HIGH;

// Expected device ID value
constexpr uint8_t WSEN_PADS_DEVICE_ID = 0xB3;

// ============================================================================
// Register map
// ============================================================================

constexpr uint8_t REG_INT_CFG = 0x0B;
constexpr uint8_t REG_THR_P_L = 0x0C;
constexpr uint8_t REG_THR_P_H = 0x0D;
constexpr uint8_t REG_INTERFACE_CTRL = 0x0E;
constexpr uint8_t REG_DEVICE_ID = 0x0F;
constexpr uint8_t REG_CTRL_1 = 0x10;
constexpr uint8_t REG_CTRL_2 = 0x11;
constexpr uint8_t REG_CTRL_3 = 0x12;
constexpr uint8_t REG_FIFO_CTRL = 0x13;
constexpr uint8_t REG_FIFO_WTM = 0x14;
constexpr uint8_t REG_REF_P_L = 0x15;
constexpr uint8_t REG_REF_P_H = 0x16;
constexpr uint8_t REG_OPC_P_L = 0x18;
constexpr uint8_t REG_OPC_P_H = 0x19;
constexpr uint8_t REG_INT_SOURCE = 0x24;
constexpr uint8_t REG_FIFO_STATUS_1 = 0x25;
constexpr uint8_t REG_FIFO_STATUS_2 = 0x26;
constexpr uint8_t REG_STATUS = 0x27;
constexpr uint8_t REG_DATA_P_XL = 0x28;
constexpr uint8_t REG_DATA_P_L = 0x29;
constexpr uint8_t REG_DATA_P_H = 0x2A;
constexpr uint8_t REG_DATA_T_L = 0x2B;
constexpr uint8_t REG_DATA_T_H = 0x2C;

// FIFO output registers (not used yet in the V1 driver)
constexpr uint8_t REG_FIFO_DATA_P_XL = 0x78;
constexpr uint8_t REG_FIFO_DATA_P_L = 0x79;
constexpr uint8_t REG_FIFO_DATA_P_H = 0x7A;
constexpr uint8_t REG_FIFO_DATA_T_L = 0x7B;
constexpr uint8_t REG_FIFO_DATA_T_H = 0x7C;

// ============================================================================
// CTRL_1 register bits
// ============================================================================

// ODR[2:0] field occupies bits 6:4
constexpr uint8_t CTRL1_ODR_SHIFT = 4;
constexpr uint8_t CTRL1_ODR_MASK = 0x70;

// Enable second low-pass filter for pressure
constexpr uint8_t CTRL1_EN_LPFP = 1 << 3;

// Low-pass filter bandwidth configuration
constexpr uint8_t CTRL1_LPFP_CFG = 1 << 2;

// Block data update
constexpr uint8_t CTRL1_BDU = 1 << 1;

// SPI mode selection (not used in I2C mode)
constexpr uint8_t CTRL1_SIM = 1 << 0;

// ============================================================================
// CTRL_2 register bits
// ============================================================================

constexpr uint8_t CTRL2_BOOT = 1 << 7;
constexpr uint8_t CTRL2_INT_H_L = 1 << 6;
constexpr uint8_t CTRL2_PP_OD = 1 << 5;
constexpr uint8_t CTRL2_IF_ADD_INC = 1 << 4;
constexpr uint8_t CTRL2_SWRESET = 1 << 2;
constexpr uint8_t CTRL2_LOW_NOISE_EN = 1 << 1;
constexpr uint8_t CTRL2_ONE_SHOT = 1 << 0;

// ============================================================================
// INT_SOURCE register bits
// ============================================================================

constexpr uint8_t INT_SOURCE_BOOT_ON = 1 << 7;
constexpr uint8_t INT_SOURCE_IA = 1 << 2;
constexpr uint8_t INT_SOURCE_PL = 1 << 1;
constexpr uint8_t INT_SOURCE_PH = 1 << 0;

// ============================================================================
// STATUS register bits
// ============================================================================

constexpr uint8_t STATUS_T_OR = 1 << 5;
constexpr uint8_t STATUS_P_OR = 1 << 4;
constexpr uint8_t STATUS_T_DA = 1 << 1;
constexpr uint8_t STATUS_P_DA = 1 << 0;

// ============================================================================
// Output data rate (ODR) values for CTRL_1[6:4]
// ============================================================================

constexpr uint8_t ODR_POWER_DOWN = 0x00;
constexpr uint8_t ODR_1_HZ = 0x01;
constexpr uint8_t ODR_10_HZ = 0x02;
constexpr uint8_t ODR_25_HZ = 0x03;
constexpr uint8_t ODR_50_HZ = 0x04;
constexpr uint8_t ODR_75_HZ = 0x05;
constexpr uint8_t ODR_100_HZ = 0x06;
constexpr uint8_t ODR_200_HZ = 0x07;

// ============================================================================
// Conversion constants
// ============================================================================

// Pressure sensitivity:
// 1 digit = 1 / 40960 kPa = 1 / 4096 hPa = 1 / 40.96 Pa
constexpr float PRESSURE_HPA_PER_DIGIT = 1.0 / 4096.0;
constexpr float PRESSURE_KPA_PER_DIGIT = 1.0 / 40960.0;
constexpr float PRESSURE_PA_PER_DIGIT = 1.0 / 40.96;

// Temperature sensitivity:
// 1 digit = 0.01 °C
constexpr float TEMPERATURE_C_PER_DIGIT = 0.01;

// ============================================================================
// Timing helpers
// ============================================================================

// Typical boot time after power-up is up to 4.5 ms, so 5 ms is a safe minimum.
constexpr uint8_t BOOT_DELAY_MS = 5;

// Typical conversion time in one-shot mode
constexpr uint8_t ONE_SHOT_LOW_POWER_DELAY_MS = 5;
constexpr uint8_t ONE_SHOT_LOW_NOISE_DELAY_MS = 15;