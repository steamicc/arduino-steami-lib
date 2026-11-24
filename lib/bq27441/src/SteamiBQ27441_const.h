// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Lipo Battery Capacity
constexpr uint16_t LIPO_BATTERY_CAPACITY = 650;  // 650 mAh

constexpr uint8_t BQ27441_I2C_ADDRESS = 0x55;  // Default I2C address of the BQ27441-G1A

// General Constants
constexpr uint16_t BQ27441_UNSEAL_KEY = 0x8000;  // Secret code to unseal the BQ27441-G1A
constexpr uint16_t BQ27441_DEVICE_ID = 0x0421;   // Default device ID

// Standard Commands

// The fuel gauge uses a series of 2-byte standard commands to enable system
// reading and writing of battery information. Each command has an associated
// sequential command-code pair.

constexpr uint8_t BQ27441_COMMAND_CONTROL = 0x00;         // Control()
constexpr uint8_t BQ27441_COMMAND_TEMP = 0x02;            // Temperature()
constexpr uint8_t BQ27441_COMMAND_VOLTAGE = 0x04;         // Voltage()
constexpr uint8_t BQ27441_COMMAND_FLAGS = 0x06;           // Flags()
constexpr uint8_t BQ27441_COMMAND_NOM_CAPACITY = 0x08;    // NominalAvailableCapacity()
constexpr uint8_t BQ27441_COMMAND_AVAIL_CAPACITY = 0x0A;  // FullAvailableCapacity()
constexpr uint8_t BQ27441_COMMAND_REM_CAPACITY = 0x0C;    // RemainingCapacity()
constexpr uint8_t BQ27441_COMMAND_FULL_CAPACITY = 0x0E;   // FullChargeCapacity()
constexpr uint8_t BQ27441_COMMAND_AVG_CURRENT = 0x10;     // AverageCurrent()
constexpr uint8_t BQ27441_COMMAND_STDBY_CURRENT = 0x12;   // StandbyCurrent()
constexpr uint8_t BQ27441_COMMAND_MAX_CURRENT = 0x14;     // MaxLoadCurrent()
constexpr uint8_t BQ27441_COMMAND_AVG_POWER = 0x18;       // AveragePower()
constexpr uint8_t BQ27441_COMMAND_SOC = 0x1C;             // StateOfCharge()
constexpr uint8_t BQ27441_COMMAND_INT_TEMP = 0x1E;        // InternalTemperature()
constexpr uint8_t BQ27441_COMMAND_SOH = 0x20;             // StateOfHealth()
constexpr uint8_t BQ27441_COMMAND_REM_CAP_UNFL = 0x28;    // RemainingCapacityUnfiltered()
constexpr uint8_t BQ27441_COMMAND_REM_CAP_FIL = 0x2A;     // RemainingCapacityFiltered()
constexpr uint8_t BQ27441_COMMAND_FULL_CAP_UNFL = 0x2C;   // FullChargeCapacityUnfiltered()
constexpr uint8_t BQ27441_COMMAND_FULL_CAP_FIL = 0x2E;    // FullChargeCapacityFiltered()
constexpr uint8_t BQ27441_COMMAND_SOC_UNFL = 0x30;        // StateOfChargeUnfiltered()

// Control Sub-commands #

// Issuing a Control() command requires a subsequent 2-byte subcommand. These
// additional bytes specify the particular control function desired. The
// Control() command allows the system to control specific features of the fuel
// gauge during normal operation and additional features when the device is in
// different access modes.

constexpr uint16_t BQ27441_CONTROL_STATUS = 0x00;
constexpr uint16_t BQ27441_CONTROL_DEVICE_TYPE = 0x01;
constexpr uint16_t BQ27441_CONTROL_FW_VERSION = 0x02;
constexpr uint16_t BQ27441_CONTROL_DM_CODE = 0x04;
constexpr uint16_t BQ27441_CONTROL_PREV_MACWRITE = 0x07;
constexpr uint16_t BQ27441_CONTROL_CHEM_ID = 0x08;
constexpr uint16_t BQ27441_CONTROL_BAT_INSERT = 0x0C;
constexpr uint16_t BQ27441_CONTROL_BAT_REMOVE = 0x0D;
constexpr uint16_t BQ27441_CONTROL_SET_HIBERNATE = 0x11;
constexpr uint16_t BQ27441_CONTROL_CLEAR_HIBERNATE = 0x12;
constexpr uint16_t BQ27441_CONTROL_SET_CFGUPDATE = 0x13;
constexpr uint16_t BQ27441_CONTROL_SHUTDOWN_ENABLE = 0x1B;
constexpr uint16_t BQ27441_CONTROL_SHUTDOWN = 0x1C;
constexpr uint16_t BQ27441_CONTROL_SEALED = 0x20;
constexpr uint16_t BQ27441_CONTROL_PULSE_SOC_INT = 0x23;
constexpr uint16_t BQ27441_CONTROL_RESET = 0x41;
constexpr uint16_t BQ27441_CONTROL_SOFT_RESET = 0x42;
constexpr uint16_t BQ27441_CONTROL_EXIT_CFGUPDATE = 0x43;
constexpr uint16_t BQ27441_CONTROL_EXIT_RESIM = 0x44;

// Control Status Word - Bit Definitions #

// Bit positions for the 16-bit data of CONTROL_STATUS.
// CONTROL_STATUS instructs the fuel gauge to return status information to
// Control() addresses 0x00 and 0x01. The read-only status word contains status
// bits that are set or cleared either automatically as conditions warrant or
// through using specified subcommands.
constexpr uint16_t BQ27441_STATUS_SHUTDOWNEN = 1 << 15;
constexpr uint16_t BQ27441_STATUS_WDRESET = 1 << 14;
constexpr uint16_t BQ27441_STATUS_SS = 1 << 13;
constexpr uint16_t BQ27441_STATUS_CALMODE = 1 << 12;
constexpr uint16_t BQ27441_STATUS_CCA = 1 << 11;
constexpr uint16_t BQ27441_STATUS_BCA = 1 << 10;
constexpr uint16_t BQ27441_STATUS_QMAX_UP = 1 << 9;
constexpr uint16_t BQ27441_STATUS_RES_UP = 1 << 8;
constexpr uint16_t BQ27441_STATUS_INITCOMP = 1 << 7;
constexpr uint16_t BQ27441_STATUS_HIBERNATE = 1 << 6;
constexpr uint16_t BQ27441_STATUS_SLEEP = 1 << 4;
constexpr uint16_t BQ27441_STATUS_LDMD = 1 << 3;
constexpr uint16_t BQ27441_STATUS_RUP_DIS = 1 << 2;
constexpr uint16_t BQ27441_STATUS_VOK = 1 << 1;

// Flag Command - Bit Definitions #

// Bit positions for the 16-bit data of Flags()
// This read-word function returns the contents of the fuel gauging status
// register, depicting the current operating status.
constexpr uint16_t BQ27441_FLAG_OT = 1 << 15;
constexpr uint16_t BQ27441_FLAG_UT = 1 << 14;
constexpr uint16_t BQ27441_FLAG_FC = 1 << 9;
constexpr uint16_t BQ27441_FLAG_CHG = 1 << 8;
constexpr uint16_t BQ27441_FLAG_OCVTAKEN = 1 << 7;
constexpr uint16_t BQ27441_FLAG_ITPOR = 1 << 5;
constexpr uint16_t BQ27441_FLAG_CFGUPMODE = 1 << 4;
constexpr uint16_t BQ27441_FLAG_BAT_DET = 1 << 3;
constexpr uint16_t BQ27441_FLAG_SOC1 = 1 << 2;
constexpr uint16_t BQ27441_FLAG_SOCF = 1 << 1;
constexpr uint16_t BQ27441_FLAG_DSG = 1 << 0;

// Extended Data Commands #

// Extended data commands offer additional functionality beyond the standard
// set of commands. They are used in the same manner; however, unlike standard
// commands, extended commands are not limited to 2-byte words.
constexpr uint16_t BQ27441_EXTENDED_OPCONFIG = 0x3A;   // OpConfig()
constexpr uint16_t BQ27441_EXTENDED_CAPACITY = 0x3C;   // DesignCapacity()
constexpr uint16_t BQ27441_EXTENDED_DATACLASS = 0x3E;  // DataClass()
constexpr uint16_t BQ27441_EXTENDED_DATABLOCK = 0x3F;  // DataBlock()
constexpr uint16_t BQ27441_EXTENDED_BLOCKDATA = 0x40;  // BlockData()
constexpr uint16_t BQ27441_EXTENDED_CHECKSUM = 0x60;   // BlockDataCheckSum()
constexpr uint16_t BQ27441_EXTENDED_CONTROL = 0x61;    // BlockDataControl()

// Configuration Class, Subclass ID's #

// To access a subclass of the extended data, set the DataClass() function
// with one of these values.
// Configuration Classes
constexpr uint16_t BQ27441_ID_SAFETY = 2;            // Safety
constexpr uint16_t BQ27441_ID_CHG_TERMINATION = 36;  // Charge Termination
constexpr uint16_t BQ27441_ID_CONFIG_DATA = 48;      // Data
constexpr uint16_t BQ27441_ID_DISCHARGE = 49;        // Discharge
constexpr uint16_t BQ27441_ID_REGISTERS = 64;        // Registers
constexpr uint16_t BQ27441_ID_POWER = 68;            // Power
// Gas Gauging Classes
constexpr uint16_t BQ27441_ID_IT_CFG = 80;          // IT Cfg
constexpr uint16_t BQ27441_ID_CURRENT_THRESH = 81;  // Current Thresholds
constexpr uint16_t BQ27441_ID_STATE = 82;           // State
// Ra Tables Classes
constexpr uint16_t BQ27441_ID_R_A_RAM = 89;  // R_a RAM
// Calibration Classes
constexpr uint16_t BQ27441_ID_CALIB_DATA = 104;  // Data
constexpr uint16_t BQ27441_ID_CC_CAL = 105;      // CC Cal
constexpr uint16_t BQ27441_ID_CURRENT = 107;     // Current
// Security Classes
constexpr uint16_t BQ27441_ID_CODES = 112;  // Codes

// OpConfig Register - Bit Definitions #

// Bit positions of the OpConfig Register
constexpr uint16_t BQ27441_OPCONFIG_BIE = 1 << 13;
constexpr uint16_t BQ27441_OPCONFIG_BI_PU_EN = 1 << 12;
constexpr uint16_t BQ27441_OPCONFIG_GPIOPOL = 1 << 11;
constexpr uint16_t BQ27441_OPCONFIG_SLEEP = 1 << 5;
constexpr uint16_t BQ27441_OPCONFIG_RMFCC = 1 << 4;
constexpr uint16_t BQ27441_OPCONFIG_BATLOWEN = 1 << 2;
constexpr uint16_t BQ27441_OPCONFIG_TEMPS = 1 << 0;

constexpr uint32_t BQ27441_I2C_TIMEOUT = 2000;  // ms