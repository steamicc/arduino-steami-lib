// SPDX-License-Identifier: GPL-3.0-or-later

#define STEAMI_BOARD_V1C
#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"

int steamiOtherUnit();

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

static void test_two_defines_report_e03(void) {
    TEST_ASSERT_EQUAL_INT(2, steamiOtherUnit());
    Steami.begin();
    bool found = false;
    for (const std::string& l : g_lines) {
        size_t n = l.size();
        if (n >= 5 && l.compare(n - 5, 5, "(E03)") == 0)
            found = true;
    }
    TEST_ASSERT_TRUE(found);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_two_defines_report_e03);
    return UNITY_END();
}
