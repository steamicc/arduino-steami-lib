// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "SteamiLazy.h"
#include "SteamiTypes.h"

void setUp(void) {}
void tearDown(void) {}

static_assert(steamiDecodeRevision(131).board == SteamiRelease::V1C, "rev 131 is V1c");

static void test_revision_codes(void) {
    SteamiRevisionInfo r = steamiDecodeRevision(110);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(1, r.generation);
    TEST_ASSERT_EQUAL_UINT8(1, r.release);
    TEST_ASSERT_EQUAL_UINT8(0, r.index);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1A);
    TEST_ASSERT_TRUE(r.prototype);

    r = steamiDecodeRevision(111);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1A);
    TEST_ASSERT_FALSE(r.prototype);

    r = steamiDecodeRevision(120);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1B);
    TEST_ASSERT_TRUE(r.prototype);
    r = steamiDecodeRevision(121);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1B);
    TEST_ASSERT_FALSE(r.prototype);

    r = steamiDecodeRevision(130);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(r.prototype);
    TEST_ASSERT_EQUAL_UINT8(0, r.index);

    r = steamiDecodeRevision(131);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(1, r.generation);
    TEST_ASSERT_EQUAL_UINT8(3, r.release);
    TEST_ASSERT_EQUAL_UINT8(1, r.index);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1C);
    TEST_ASSERT_FALSE(r.prototype);

    const int32_t invalid[] = {3, 99, 100, 140, 210, 230, 1000, -131};
    for (int32_t code : invalid) {
        r = steamiDecodeRevision(code);
        TEST_ASSERT_FALSE(r.valid);
        TEST_ASSERT_TRUE(r.board == SteamiRelease::UNKNOWN);
    }
}

static void test_routing_v1c_has_no_apds_and_uses_bme280(void) {
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::TEMPERATURE, 0) ==
                     SteamiChip::BME280);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::TEMPERATURE, 1) ==
                     SteamiChip::NONE);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::GESTURE, 0) ==
                     SteamiChip::NONE);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::GC9A01);
}

static void test_routing_v1ab_and_unknown_candidates(void) {
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1AB, SteamiFunction::TEMPERATURE, 1) ==
                     SteamiChip::WSEN_PADS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1B, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::SSD1327);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 0) ==
                     SteamiChip::BME280);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 1) ==
                     SteamiChip::WSEN_HIDS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::NONE);
}

struct Probe {
    static int alive;
    int v;
    explicit Probe(int x) : v(x) { ++alive; }
    ~Probe() { --alive; }
};
int Probe::alive = 0;

static void test_lazy_slot_builds_once_and_resets(void) {
    SteamiLazy<Probe> slot;
    TEST_ASSERT_FALSE(slot.built());
    slot.emplace(4);
    slot.emplace(9);
    TEST_ASSERT_TRUE(slot.built());
    TEST_ASSERT_EQUAL_INT(4, slot.get()->v);
    TEST_ASSERT_EQUAL_INT(1, Probe::alive);
    slot.reset();
    TEST_ASSERT_EQUAL_INT(0, Probe::alive);
    TEST_ASSERT_FALSE(slot.built());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_revision_codes);
    RUN_TEST(test_routing_v1c_has_no_apds_and_uses_bme280);
    RUN_TEST(test_routing_v1ab_and_unknown_candidates);
    RUN_TEST(test_lazy_slot_builds_once_and_resets);
    return UNITY_END();
}
