// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Register images of the I2C chips for the native tests: what the Wire mock must hold so that a
// driver's begin() succeeds and a read gives a known value. Shared by the driver suites and the
// board suites.
//
// Each chip section exists only when the suite pulled in that driver (PlatformIO adds a library
// to a test build only when the test file includes it), so a driver suite compiles its own chip
// alone, and a suite that includes SteamiBoard.h gets all of them. Include the drivers first.

#include <stdint.h>

#include "Wire.h"

#if __has_include(<SteamiBME280.h>)
#include <SteamiBME280.h>

// ---------------------------------------------------------------- BME280

struct SteamiFakeBme280Calibration {
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4, h5;
    int8_t h6;
};

// The calibration set of the Bosch BME280 datasheet example.
constexpr SteamiFakeBme280Calibration STEAMI_FAKE_BME280_BOSCH = {
    27504, 26435,  -1000, 36477, -10685, 3024, 2855, 140, -7,
    15500, -14600, 6000,  75,    362,    0,    312,  50,  30};

// Raw sample that reads 25.08 degC, 1006.5325 hPa and 55.3535 %RH with the calibration above.
constexpr int32_t STEAMI_FAKE_BME280_RAW_TEMP = 519888;
constexpr int32_t STEAMI_FAKE_BME280_RAW_PRESS = 415148;
constexpr int32_t STEAMI_FAKE_BME280_RAW_HUM = 30000;

inline void steamiFakeBme280Le16(TwoWire& wire, uint8_t reg, int32_t value) {
    wire.setRegister(BME280_DEFAULT_ADDRESS, reg, value & 0xFF);
    wire.setRegister(BME280_DEFAULT_ADDRESS, reg + 1, (value >> 8) & 0xFF);
}

inline void steamiFakeBme280ChipId(TwoWire& wire, bool valid = true) {
    wire.setRegister(BME280_DEFAULT_ADDRESS, BME280_REG_CHIP_ID,
                     valid ? BME280_CHIP_ID_VALUE : 0x42);
}

inline void steamiFakeBme280Calibration(TwoWire& wire, const SteamiFakeBme280Calibration& c) {
    const uint8_t addr = BME280_DEFAULT_ADDRESS;
    steamiFakeBme280Le16(wire, 0x88, c.t1);
    steamiFakeBme280Le16(wire, 0x8A, c.t2);
    steamiFakeBme280Le16(wire, 0x8C, c.t3);
    steamiFakeBme280Le16(wire, 0x8E, c.p1);
    steamiFakeBme280Le16(wire, 0x90, c.p2);
    steamiFakeBme280Le16(wire, 0x92, c.p3);
    steamiFakeBme280Le16(wire, 0x94, c.p4);
    steamiFakeBme280Le16(wire, 0x96, c.p5);
    steamiFakeBme280Le16(wire, 0x98, c.p6);
    steamiFakeBme280Le16(wire, 0x9A, c.p7);
    steamiFakeBme280Le16(wire, 0x9C, c.p8);
    steamiFakeBme280Le16(wire, 0x9E, c.p9);
    wire.setRegister(addr, 0xA0, 0x00);  // reserved
    wire.setRegister(addr, 0xA1, c.h1);
    steamiFakeBme280Le16(wire, 0xE1, c.h2);
    wire.setRegister(addr, 0xE3, c.h3);
    // H4 = E4[7:0] << 4 | E5[3:0]; H5 = E6[7:0] << 4 | E5[7:4] (12-bit signed).
    wire.setRegister(addr, 0xE4, (c.h4 >> 4) & 0xFF);
    wire.setRegister(addr, 0xE5, ((c.h5 & 0x0F) << 4) | (c.h4 & 0x0F));
    wire.setRegister(addr, 0xE6, (c.h5 >> 4) & 0xFF);
    wire.setRegister(addr, 0xE7, static_cast<uint8_t>(c.h6));
}

inline void steamiFakeBme280Raw(TwoWire& wire, int32_t press, int32_t temp, int32_t hum) {
    const uint8_t addr = BME280_DEFAULT_ADDRESS;
    wire.setRegister(addr, 0xF7, (press >> 12) & 0xFF);
    wire.setRegister(addr, 0xF8, (press >> 4) & 0xFF);
    wire.setRegister(addr, 0xF9, (press & 0x0F) << 4);
    wire.setRegister(addr, 0xFA, (temp >> 12) & 0xFF);
    wire.setRegister(addr, 0xFB, (temp >> 4) & 0xFF);
    wire.setRegister(addr, 0xFC, (temp & 0x0F) << 4);
    wire.setRegister(addr, 0xFD, (hum >> 8) & 0xFF);
    wire.setRegister(addr, 0xFE, hum & 0xFF);
}

/// Chip ID, Bosch example calibration and raw sample: reads 25.08 degC, 1006.5325 hPa and
/// 55.3535 %RH.
inline void steamiFakeBme280(TwoWire& wire) {
    steamiFakeBme280ChipId(wire, true);
    steamiFakeBme280Calibration(wire, STEAMI_FAKE_BME280_BOSCH);
    steamiFakeBme280Raw(wire, STEAMI_FAKE_BME280_RAW_PRESS, STEAMI_FAKE_BME280_RAW_TEMP,
                        STEAMI_FAKE_BME280_RAW_HUM);
}

#endif

#if __has_include(<SteamiWsenHids.h>)
#include <SteamiWsenHids.h>

// -------------------------------------------------------------- WSEN-HIDS

// The driver uses bit 7 of the sub-address as the ST auto-increment flag for multi-byte reads.
// The Wire mock is a straight register map, so preloads for burst-read ranges sit at the post-OR
// address the driver emits on the wire.
constexpr uint8_t STEAMI_FAKE_HIDS_CAL_BASE = WSEN_HIDS_REG_H0_RH_X2 | WSEN_HIDS_AUTO_INCREMENT;
constexpr uint8_t STEAMI_FAKE_HIDS_HUM_BASE =
    WSEN_HIDS_REG_HUMIDITY_OUT_L | WSEN_HIDS_AUTO_INCREMENT;
constexpr uint8_t STEAMI_FAKE_HIDS_TEMP_BASE = WSEN_HIDS_REG_TEMP_OUT_L | WSEN_HIDS_AUTO_INCREMENT;

inline void steamiFakeWsenHidsWhoAmI(TwoWire& wire, bool valid = true) {
    wire.setRegister(WSEN_HIDS_DEFAULT_ADDRESS, WSEN_HIDS_REG_WHO_AM_I,
                     valid ? WSEN_HIDS_WHO_AM_I_VALUE : 0x42);
}

// Factory calibration chosen so the math is trivial to verify:
//   temperature = 0.001 * raw       (T0=0 degC at raw 0, T1=25 degC at raw 25000)
//   humidity    = (100/30000) * raw (H0=0 %RH at raw 0, H1=100 %RH at raw 30000)
inline void steamiFakeWsenHidsCalibration(TwoWire& wire) {
    const uint8_t addr = WSEN_HIDS_DEFAULT_ADDRESS;
    const uint8_t base = STEAMI_FAKE_HIDS_CAL_BASE;
    wire.setRegister(addr, base + 0x00, 0);     // H0_rH_x2: H0 = 0 %RH
    wire.setRegister(addr, base + 0x01, 200);   // H1_rH_x2: H1 = 100 %RH
    wire.setRegister(addr, base + 0x02, 0);     // T0_degC_x8: T0 = 0 degC
    wire.setRegister(addr, base + 0x03, 200);   // T1_degC_x8: T1 = 25 degC
    wire.setRegister(addr, base + 0x04, 0);     // reserved
    wire.setRegister(addr, base + 0x05, 0);     // T1_T0_MSB, high bits all zero
    wire.setRegister(addr, base + 0x06, 0x00);  // H0_T0_OUT = 0
    wire.setRegister(addr, base + 0x07, 0x00);
    wire.setRegister(addr, base + 0x08, 0);  // reserved
    wire.setRegister(addr, base + 0x09, 0);
    wire.setRegister(addr, base + 0x0A, 0x30);  // H1_T0_OUT = 30000 = 0x7530
    wire.setRegister(addr, base + 0x0B, 0x75);
    wire.setRegister(addr, base + 0x0C, 0x00);  // T0_OUT = 0
    wire.setRegister(addr, base + 0x0D, 0x00);
    wire.setRegister(addr, base + 0x0E, 0xA8);  // T1_OUT = 25000 = 0x61A8
    wire.setRegister(addr, base + 0x0F, 0x61);
}

/// OUT registers that map, through the calibration above, to the requested values, and STATUS
/// with H_DA | T_DA.
inline void steamiFakeWsenHidsMeasurement(TwoWire& wire, float tempC, float humidity) {
    const uint8_t addr = WSEN_HIDS_DEFAULT_ADDRESS;
    const int16_t tempRaw = static_cast<int16_t>(tempC / 0.001f);    // 1 degC = 1000 LSB
    const int16_t humRaw = static_cast<int16_t>(humidity * 300.0f);  // 100 %RH = 30000 LSB
    wire.setRegister(addr, STEAMI_FAKE_HIDS_TEMP_BASE + 0, tempRaw & 0xFF);
    wire.setRegister(addr, STEAMI_FAKE_HIDS_TEMP_BASE + 1, (tempRaw >> 8) & 0xFF);
    wire.setRegister(addr, STEAMI_FAKE_HIDS_HUM_BASE + 0, humRaw & 0xFF);
    wire.setRegister(addr, STEAMI_FAKE_HIDS_HUM_BASE + 1, (humRaw >> 8) & 0xFF);
    wire.setRegister(addr, WSEN_HIDS_REG_STATUS, WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA);
}

/// ID, factory calibration, measurement and STATUS H_DA | T_DA.
inline void steamiFakeWsenHids(TwoWire& wire, float tempC, float humidity) {
    steamiFakeWsenHidsWhoAmI(wire, true);
    steamiFakeWsenHidsCalibration(wire);
    steamiFakeWsenHidsMeasurement(wire, tempC, humidity);
}

#endif

#if __has_include(<SteamiWsenPads.h>)
#include <SteamiWsenPads.h>
#include <SteamiWsenPads_const.h>

// -------------------------------------------------------------- WSEN-PADS

inline void steamiFakeWsenPadsId(TwoWire& wire, bool valid = true) {
    wire.setRegister(WSEN_PADS_I2C_DEFAULT_ADDR, REG_DEVICE_ID, valid ? WSEN_PADS_DEVICE_ID : 0x42);
}

/// Measurement registers and STATUS P_DA | T_DA.
inline void steamiFakeWsenPadsMeasurement(TwoWire& wire, float tempC, float hPa) {
    const uint8_t addr = WSEN_PADS_I2C_DEFAULT_ADDR;
    const int16_t tempRaw = static_cast<int16_t>(tempC / 0.01f);
    const int32_t pressureRaw = static_cast<int32_t>(hPa * 4096);
    wire.setRegister(addr, REG_DATA_P_XL, pressureRaw & 0xFF);
    wire.setRegister(addr, REG_DATA_P_L, (pressureRaw >> 8) & 0xFF);
    wire.setRegister(addr, REG_DATA_P_H, (pressureRaw >> 16) & 0xFF);
    wire.setRegister(addr, REG_DATA_T_L, tempRaw & 0xFF);
    wire.setRegister(addr, REG_DATA_T_H, (tempRaw >> 8) & 0xFF);
    wire.setRegister(addr, REG_STATUS, STATUS_P_DA | STATUS_T_DA);
}

/// ID, measurement and STATUS P_DA | T_DA.
inline void steamiFakeWsenPads(TwoWire& wire, float tempC, float hPa) {
    steamiFakeWsenPadsId(wire, true);
    steamiFakeWsenPadsMeasurement(wire, tempC, hPa);
}

#endif

#if __has_include(<SteamiVL53L1X.h>)
#include <SteamiVL53L1X.h>

// --------------------------------------------------------------- VL53L1X

inline void steamiFakeVl53l1xWhoAmI(TwoWire& wire, bool valid = true) {
    const uint8_t msb = valid ? static_cast<uint8_t>(VL53L1X_DEVICE_ID >> 8) : 0x42;
    const uint8_t lsb = valid ? static_cast<uint8_t>(VL53L1X_DEVICE_ID & 0xFF) : 0x42;
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, 0x010F, msb);
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, 0x0110, lsb);
}

inline void steamiFakeVl53l1xDataReady(TwoWire& wire, bool ready = true) {
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, static_cast<uint8_t>(REG_GPIO_HV_MUX_CTRL), 0x00);
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, static_cast<uint8_t>(REG_GPIO_TIO_HV_STATUS),
                     ready ? GPIO_TIO_HV_STATUS_DATA_READY : 0x00);
}

/// Who-am-I.
inline void steamiFakeVl53l1x(TwoWire& wire) {
    steamiFakeVl53l1xWhoAmI(wire, true);
}

/// Data ready and a distance. Call it after the driver's begin(), which resets the registers.
inline void steamiFakeVl53l1xDistance(TwoWire& wire, uint16_t mm) {
    steamiFakeVl53l1xDataReady(wire, true);
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, REG_RESULT_RANGE_STATUS + RESULT_DISTANCE_MSB_OFFSET,
                     (mm >> 8) & 0xFF);
    wire.setRegister(VL53L1X_I2C_DEFAULT_ADDR, REG_RESULT_RANGE_STATUS + RESULT_DISTANCE_LSB_OFFSET,
                     mm & 0xFF);
}

#endif

#if __has_include(<SteamiBQ27441.h>)
#include <SteamiBQ27441.h>

// --------------------------------------------------------------- BQ27441

inline void steamiFakeBq27441WhoAmI(TwoWire& wire, bool valid = true) {
    const uint16_t value = valid ? BQ27441_DEVICE_ID : 0x1234;
    wire.setRegister(BQ27441_I2C_ADDRESS, 0x00, value & 0xFF);
    wire.setRegister(BQ27441_I2C_ADDRESS, 0x01, (value >> 8) & 0xFF);
}

/// The control responses and the CFGUPMODE flag: begin() goes through powerOn(), setCapacity()
/// and enterConfig(), which would otherwise wait until its timeout and fail.
inline void steamiFakeBq27441(TwoWire& wire) {
    wire.setControlResponse(BQ27441_CONTROL_DEVICE_TYPE, BQ27441_DEVICE_ID);
    wire.setControlResponse(BQ27441_CONTROL_STATUS, 0x0080);
    wire.setRegister(BQ27441_I2C_ADDRESS, BQ27441_COMMAND_FLAGS, BQ27441_FLAG_CFGUPMODE & 0xFF);
    wire.setRegister(BQ27441_I2C_ADDRESS, BQ27441_COMMAND_FLAGS + 1,
                     (BQ27441_FLAG_CFGUPMODE >> 8) & 0xFF);
    steamiFakeBq27441WhoAmI(wire, true);
}

inline void steamiFakeBq27441Measurement(TwoWire& wire, uint16_t mV, int16_t mA, uint8_t soc) {
    const uint8_t addr = BQ27441_I2C_ADDRESS;
    wire.setRegister(addr, BQ27441_COMMAND_VOLTAGE, mV & 0xFF);
    wire.setRegister(addr, BQ27441_COMMAND_VOLTAGE + 1, (mV >> 8) & 0xFF);
    wire.setRegister(addr, BQ27441_COMMAND_AVG_CURRENT, mA & 0xFF);
    wire.setRegister(addr, BQ27441_COMMAND_AVG_CURRENT + 1, (mA >> 8) & 0xFF);
    wire.setRegister(addr, BQ27441_COMMAND_SOC, soc & 0xFF);
    wire.setRegister(addr, BQ27441_COMMAND_SOC + 1, 0x00);
}

#endif

#if __has_include(<SteamiLIS2MDL.h>)
#include <SteamiLIS2MDL.h>

// --------------------------------------------------------------- LIS2MDL

/// WHO_AM_I, STATUS with ZYXDA (data ready) and the raw field on X, Y and Z (1.5 mG per LSB).
inline void steamiFakeLis2mdl(TwoWire& wire, int16_t x, int16_t y, int16_t z) {
    const uint8_t addr = LIS2MDL_I2C_ADDR;
    const uint16_t ux = static_cast<uint16_t>(x);
    const uint16_t uy = static_cast<uint16_t>(y);
    const uint16_t uz = static_cast<uint16_t>(z);
    wire.setRegister(addr, LIS2MDL_WHO_AM_I, LIS2MDL_WHO_AM_I_VAL);
    wire.setRegister(addr, LIS2MDL_STATUS_REG, 0x08);  // ZYXDA
    wire.setRegister(addr, LIS2MDL_OUTX_L_REG, ux & 0xFF);
    wire.setRegister(addr, LIS2MDL_OUTX_H_REG, (ux >> 8) & 0xFF);
    wire.setRegister(addr, LIS2MDL_OUTY_L_REG, uy & 0xFF);
    wire.setRegister(addr, LIS2MDL_OUTY_H_REG, (uy >> 8) & 0xFF);
    wire.setRegister(addr, LIS2MDL_OUTZ_L_REG, uz & 0xFF);
    wire.setRegister(addr, LIS2MDL_OUTZ_H_REG, (uz >> 8) & 0xFF);
}

#endif

#if __has_include(<SteamiAPDS9960.h>)
#include <SteamiAPDS9960.h>

// ------------------------------------------------------------- APDS-9960

/// ID 0xAB, STATUS with AVALID and the clear, red, green and blue channels.
inline void steamiFakeApds9960(TwoWire& wire, uint16_t clear, uint16_t red, uint16_t green,
                               uint16_t blue) {
    const uint8_t addr = APDS9960_DEFAULT_ADDRESS;
    wire.setRegister(addr, APDS9960_REG_ID, APDS9960_DEVICE_ID_1);
    wire.setRegister(addr, APDS9960_REG_STATUS, APDS9960_STATUS_AVALID);
    wire.setRegister(addr, APDS9960_REG_CDATAL, clear & 0xFF);
    wire.setRegister(addr, APDS9960_REG_CDATAH, (clear >> 8) & 0xFF);
    wire.setRegister(addr, APDS9960_REG_RDATAL, red & 0xFF);
    wire.setRegister(addr, APDS9960_REG_RDATAH, (red >> 8) & 0xFF);
    wire.setRegister(addr, APDS9960_REG_GDATAL, green & 0xFF);
    wire.setRegister(addr, APDS9960_REG_GDATAH, (green >> 8) & 0xFF);
    wire.setRegister(addr, APDS9960_REG_BDATAL, blue & 0xFF);
    wire.setRegister(addr, APDS9960_REG_BDATAH, (blue >> 8) & 0xFF);
}

#endif

#if __has_include(<SteamiMCP23009E.h>)
#include <SteamiMCP23009E.h>

// ------------------------------------------------------------- MCP23009E

/// IODIR 0xF0 (the D-pad pins 4 to 7 as inputs) and the GPIO port value.
inline void steamiFakeMcp23009e(TwoWire& wire, uint8_t gpio) {
    wire.setRegister(MCP23009_I2C_ADDR, MCP23009_IODIR, 0xF0);
    wire.setRegister(MCP23009_I2C_ADDR, MCP23009_GPIO, gpio);
}

#endif
