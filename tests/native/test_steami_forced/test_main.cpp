// SPDX-License-Identifier: GPL-3.0-or-later

#define STEAMI_BOARD_V1C
#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiTemperature.h"

int steamiSecondUnit();

static std::vector<std::string> g_lines;
static void capture(const char* line) {
    g_lines.push_back(line);
}

void setUp(void) {
    Wire = TwoWire();
    g_lines.clear();
    Steami.end();
    Steami.setLogger(&capture);
}
void tearDown(void) {
    Steami.end();
    Steami.setLogger(nullptr);
}

static void test_define_forces_v1c_without_traffic(void) {
    TEST_ASSERT_EQUAL_INT(1, steamiSecondUnit());
    TEST_ASSERT_TRUE(Steami.begin());
    TEST_ASSERT_TRUE(Steami.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(Steami.origin() == SteamiOrigin::FORCED_DEFINE);
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    bool found = false;
    for (const std::string& l : g_lines) {
        if (l == "[STeaMi] Board V1c (forced by STEAMI_BOARD_V1C)")
            found = true;
    }
    TEST_ASSERT_TRUE(found);
}

static void test_define_contradicted_by_missing_bme280_reports_e11(void) {
    SteamiTemperature temperature;
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_TRUE(Steami.origin() == SteamiOrigin::FORCED_DEFINE);
    bool found = false;
    for (const std::string& l : g_lines) {
        if (l ==
            "[STeaMi] SteamiTemperature: this STeaMi is set as a V1c (forced by #define), but its "
            "BME280 temperature sensor (I2C 0x76) did not answer. Check the #define, "
            "Steami.begin() or the board config. (E11)")
            found = true;
    }
    TEST_ASSERT_TRUE(found);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_define_forces_v1c_without_traffic);
    RUN_TEST(test_define_contradicted_by_missing_bme280_reports_e11);
    return UNITY_END();
}
