// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

constexpr uint8_t APDS9960_DEFAULT_ADDRESS = 0x39;

constexpr uint8_t APDS9960_DEVICE_ID_1 = 0xAB;
constexpr uint8_t APDS9960_DEVICE_ID_2 = 0x9C;
constexpr uint8_t APDS9960_DEVICE_ID_3 = 0xA8;

// Gesture processing parameters.
constexpr uint8_t APDS9960_GESTURE_THRESHOLD_OUT = 10;
constexpr int16_t APDS9960_GESTURE_SENSITIVITY_1 = 50;
constexpr int16_t APDS9960_GESTURE_SENSITIVITY_2 = 20;
constexpr uint16_t APDS9960_FIFO_PAUSE_MS = 30;
constexpr uint8_t APDS9960_GESTURE_DATASETS_MAX = 32;

// Register map.
constexpr uint8_t APDS9960_REG_ENABLE = 0x80;
constexpr uint8_t APDS9960_REG_ATIME = 0x81;
constexpr uint8_t APDS9960_REG_WTIME = 0x83;
constexpr uint8_t APDS9960_REG_AILTL = 0x84;
constexpr uint8_t APDS9960_REG_AILTH = 0x85;
constexpr uint8_t APDS9960_REG_AIHTL = 0x86;
constexpr uint8_t APDS9960_REG_AIHTH = 0x87;
constexpr uint8_t APDS9960_REG_PILT = 0x89;
constexpr uint8_t APDS9960_REG_PIHT = 0x8B;
constexpr uint8_t APDS9960_REG_PERS = 0x8C;
constexpr uint8_t APDS9960_REG_CONFIG1 = 0x8D;
constexpr uint8_t APDS9960_REG_PPULSE = 0x8E;
constexpr uint8_t APDS9960_REG_CONTROL = 0x8F;
constexpr uint8_t APDS9960_REG_CONFIG2 = 0x90;
constexpr uint8_t APDS9960_REG_ID = 0x92;
constexpr uint8_t APDS9960_REG_STATUS = 0x93;
constexpr uint8_t APDS9960_REG_CDATAL = 0x94;
constexpr uint8_t APDS9960_REG_CDATAH = 0x95;
constexpr uint8_t APDS9960_REG_RDATAL = 0x96;
constexpr uint8_t APDS9960_REG_RDATAH = 0x97;
constexpr uint8_t APDS9960_REG_GDATAL = 0x98;
constexpr uint8_t APDS9960_REG_GDATAH = 0x99;
constexpr uint8_t APDS9960_REG_BDATAL = 0x9A;
constexpr uint8_t APDS9960_REG_BDATAH = 0x9B;
constexpr uint8_t APDS9960_REG_PDATA = 0x9C;
constexpr uint8_t APDS9960_REG_POFFSET_UR = 0x9D;
constexpr uint8_t APDS9960_REG_POFFSET_DL = 0x9E;
constexpr uint8_t APDS9960_REG_CONFIG3 = 0x9F;
constexpr uint8_t APDS9960_REG_GPENTH = 0xA0;
constexpr uint8_t APDS9960_REG_GEXTH = 0xA1;
constexpr uint8_t APDS9960_REG_GCONF1 = 0xA2;
constexpr uint8_t APDS9960_REG_GCONF2 = 0xA3;
constexpr uint8_t APDS9960_REG_GOFFSET_U = 0xA4;
constexpr uint8_t APDS9960_REG_GOFFSET_D = 0xA5;
constexpr uint8_t APDS9960_REG_GPULSE = 0xA6;
constexpr uint8_t APDS9960_REG_GOFFSET_L = 0xA7;
constexpr uint8_t APDS9960_REG_GOFFSET_R = 0xA9;
constexpr uint8_t APDS9960_REG_GCONF3 = 0xAA;
constexpr uint8_t APDS9960_REG_GCONF4 = 0xAB;
constexpr uint8_t APDS9960_REG_GFLVL = 0xAE;
constexpr uint8_t APDS9960_REG_GSTATUS = 0xAF;
constexpr uint8_t APDS9960_REG_PICLEAR = 0xE5;
constexpr uint8_t APDS9960_REG_CICLEAR = 0xE6;
constexpr uint8_t APDS9960_REG_AICLEAR = 0xE7;
constexpr uint8_t APDS9960_REG_GFIFO_U = 0xFC;

// ENABLE bits.
constexpr uint8_t APDS9960_ENABLE_PON = 0x01;
constexpr uint8_t APDS9960_ENABLE_AEN = 0x02;
constexpr uint8_t APDS9960_ENABLE_PEN = 0x04;
constexpr uint8_t APDS9960_ENABLE_WEN = 0x08;
constexpr uint8_t APDS9960_ENABLE_AIEN = 0x10;
constexpr uint8_t APDS9960_ENABLE_PIEN = 0x20;
constexpr uint8_t APDS9960_ENABLE_GEN = 0x40;

// STATUS / GSTATUS bits.
constexpr uint8_t APDS9960_STATUS_AVALID = 0x01;
constexpr uint8_t APDS9960_STATUS_PVALID = 0x02;
constexpr uint8_t APDS9960_GSTATUS_GVALID = 0x01;

// GCONF4 bits.
constexpr uint8_t APDS9960_GCONF4_GMODE = 0x01;
constexpr uint8_t APDS9960_GCONF4_GIEN = 0x02;

// CONFIG2 / CONFIG3 fields.
constexpr uint8_t APDS9960_CONFIG2_LED_BOOST_MASK = 0x30;
constexpr uint8_t APDS9960_CONFIG3_PMASK_MASK = 0x0F;
constexpr uint8_t APDS9960_CONFIG3_PCMP = 0x20;

// CONTROL fields.
constexpr uint8_t APDS9960_CONTROL_AGAIN_MASK = 0x03;
constexpr uint8_t APDS9960_CONTROL_PGAIN_MASK = 0x0C;
constexpr uint8_t APDS9960_CONTROL_LDRIVE_MASK = 0xC0;

// GCONF2 fields.
constexpr uint8_t APDS9960_GCONF2_GWTIME_MASK = 0x07;
constexpr uint8_t APDS9960_GCONF2_GLDRIVE_MASK = 0x18;
constexpr uint8_t APDS9960_GCONF2_GGAIN_MASK = 0x60;

// Default register values.
constexpr uint8_t APDS9960_DEFAULT_ATIME = 219;
constexpr uint8_t APDS9960_DEFAULT_WTIME = 246;
constexpr uint8_t APDS9960_DEFAULT_PROX_PPULSE = 0x87;
constexpr uint8_t APDS9960_DEFAULT_GESTURE_PPULSE = 0x89;
constexpr uint8_t APDS9960_DEFAULT_POFFSET_UR = 0;
constexpr uint8_t APDS9960_DEFAULT_POFFSET_DL = 0;
constexpr uint8_t APDS9960_DEFAULT_CONFIG1 = 0x60;
constexpr uint8_t APDS9960_DEFAULT_PILT = 0;
constexpr uint8_t APDS9960_DEFAULT_PIHT = 50;
constexpr uint16_t APDS9960_DEFAULT_AILT = 0xFFFF;
constexpr uint16_t APDS9960_DEFAULT_AIHT = 0;
constexpr uint8_t APDS9960_DEFAULT_PERS = 0x11;
constexpr uint8_t APDS9960_DEFAULT_CONFIG2 = 0x01;
constexpr uint8_t APDS9960_DEFAULT_CONFIG3 = 0;
constexpr uint8_t APDS9960_DEFAULT_GPENTH = 40;
constexpr uint8_t APDS9960_DEFAULT_GEXTH = 30;
constexpr uint8_t APDS9960_DEFAULT_GCONF1 = 0x40;
constexpr uint8_t APDS9960_DEFAULT_GOFFSET = 0;
constexpr uint8_t APDS9960_DEFAULT_GPULSE = 0xC9;
constexpr uint8_t APDS9960_DEFAULT_GCONF3 = 0;
constexpr uint32_t APDS9960_DEFAULT_READY_TIMEOUT_MS = 500;
