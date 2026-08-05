// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t STEAMI_CONFIG_MAX_SIZE = 1024;
constexpr uint8_t STEAMI_CONFIG_SENSOR_COUNT = 5;

constexpr const char* STEAMI_CONFIG_SENSOR_HTS221 = "hts221";
constexpr const char* STEAMI_CONFIG_SENSOR_LIS2MDL = "lis2mdl";
constexpr const char* STEAMI_CONFIG_SENSOR_ISM330DL = "ism330dl";
constexpr const char* STEAMI_CONFIG_SENSOR_WSEN_HIDS = "wsen_hids";
constexpr const char* STEAMI_CONFIG_SENSOR_WSEN_PADS = "wsen_pads";
