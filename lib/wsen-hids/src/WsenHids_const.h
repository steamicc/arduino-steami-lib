// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WSEN_HIDS_CONST_H
#define WSEN_HIDS_CONST_H

#include <Arduino.h>

namespace WsenHidsConst {

// =========================
// I2C
// =========================

// Adresse I2C (7 bits) — confirmée dans le repo
constexpr uint8_t DEFAULT_ADDRESS = 0x5F;

// =========================
// Device ID
// =========================

constexpr uint8_t WHO_AM_I_REG = 0x0F;
// (optionnel si tu trouves la valeur exacte plus tard)
// constexpr uint8_t WHO_AM_I_VALUE = 0xBC;
constexpr uint8_t WHO_AM_I_VALUE = 0xBC;

// =========================
// Control registers
// =========================

constexpr uint8_t AV_CONF_REG = 0x10;
constexpr uint8_t AV_CONF_AVGH_MASK = 0x07;
constexpr uint8_t AV_CONF_AVGT_MASK = 0x38;
constexpr uint8_t CTRL1_REG = 0x20;
constexpr uint8_t CTRL2_REG = 0x21;
constexpr uint8_t CTRL3_REG = 0x22;
constexpr uint8_t STATUS_REG = 0x27;

// =========================
// Output registers
// =========================

constexpr uint8_t HUMIDITY_OUT_L_REG = 0x28;
constexpr uint8_t HUMIDITY_OUT_H_REG = 0x29;
constexpr uint8_t TEMP_OUT_L_REG = 0x2A;
constexpr uint8_t TEMP_OUT_H_REG = 0x2B;

// =========================
// Calibration registers
// =========================

constexpr uint8_t H0_RH_X2_REG = 0x30;
constexpr uint8_t H1_RH_X2_REG = 0x31;
constexpr uint8_t T0_DEGC_X8_REG = 0x32;
constexpr uint8_t T1_DEGC_X8_REG = 0x33;
constexpr uint8_t T1_T0_MSB_REG = 0x35;

constexpr uint8_t H0_T0_OUT_L_REG = 0x36;
constexpr uint8_t H0_T0_OUT_H_REG = 0x37;
constexpr uint8_t H1_T0_OUT_L_REG = 0x3A;
constexpr uint8_t H1_T0_OUT_H_REG = 0x3B;

constexpr uint8_t T0_OUT_L_REG = 0x3C;
constexpr uint8_t T0_OUT_H_REG = 0x3D;
constexpr uint8_t T1_OUT_L_REG = 0x3E;
constexpr uint8_t T1_OUT_H_REG = 0x3F;

// =========================
// CTRL1 bits
// =========================

// Power
constexpr uint8_t CTRL1_PD_BIT = 0x80;

// Block Data Update
constexpr uint8_t CTRL1_BDU_BIT = 0x04;

// Output Data Rate (ODR)
constexpr uint8_t CTRL1_ODR_MASK = 0x03;
constexpr uint8_t ODR_ONE_SHOT = 0x00;
constexpr uint8_t ODR_1HZ = 0x01;
constexpr uint8_t ODR_7HZ = 0x02;
constexpr uint8_t ODR_12_5HZ = 0x03;

// =========================
// CTRL2 bits
// =========================

constexpr uint8_t CTRL2_ONE_SHOT_BIT = 0x01;
constexpr uint8_t CTRL2_REBOOT_BIT = 0x80;

// =========================
// STATUS bits
// =========================

constexpr uint8_t STATUS_T_DA = 0x01;
constexpr uint8_t STATUS_H_DA = 0x02;

// =========================
// Helpers
// =========================

constexpr uint8_t AUTO_INCREMENT = 0x80;

constexpr uint8_t AVGH_4 = 0x00;
constexpr uint8_t AVGH_8 = 0x01;
constexpr uint8_t AVGH_16 = 0x02;
constexpr uint8_t AVGH_32 = 0x03;
constexpr uint8_t AVGH_64 = 0x04;
constexpr uint8_t AVGH_128 = 0x05;
constexpr uint8_t AVGH_256 = 0x06;
constexpr uint8_t AVGH_512 = 0x07;

constexpr uint8_t AVGT_2 = 0x00;
constexpr uint8_t AVGT_4 = 0x01;
constexpr uint8_t AVGT_8 = 0x02;
constexpr uint8_t AVGT_16 = 0x03;
constexpr uint8_t AVGT_32 = 0x04;
constexpr uint8_t AVGT_64 = 0x05;
constexpr uint8_t AVGT_128 = 0x06;
constexpr uint8_t AVGT_256 = 0x07;

}  // namespace WsenHidsConst
#endif
