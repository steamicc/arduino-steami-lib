// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for the board layer on a real STeaMi.
 *
 * test_factory_revision_round_trip writes the DAPLink configuration zone.
 * The zone content is saved first and written back before any assertion.
 * It is skipped on a board without a config zone, and no write happens
 * when the board already has a revision.
 */

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiConfig.h>
#include <SteamiDaplinkBridge.h>
#include <string.h>
#include <unity.h>

#include "config_zone_backup.h"

static ConfigZoneBackup backup;

void setUp(void) {}

void tearDown(void) {
    Steami.end();
}

void test_detects_a_known_release(void) {
    TEST_ASSERT_TRUE(Steami.begin());

    const SteamiRelease release = Steami.release();
    TEST_ASSERT_TRUE(release == SteamiRelease::V1C || release == SteamiRelease::V1AB);

    Steami.printInfo(Serial);
}

void test_factory_revision_round_trip(void) {
    Steami.end();

    // One bus only: the board's own. Built here, never at static-init time.
    SteamiDaplinkBridge bridge(Steami.wire());
    if (!bridge.begin() || !bridge.hasConfigZone()) {
        TEST_IGNORE_MESSAGE("DAPLink firmware without a config zone.");
        return;
    }

    SteamiConfig cfg(bridge);
    cfg.begin();
    cfg.load();

    int32_t existing = 0;
    if (cfg.boardRevision(existing)) {
        // The board already has a factory revision: do not touch it.
        const bool valid = steamiDecodeRevision(existing).valid;
        TEST_ASSERT_TRUE_MESSAGE(valid, "the revision stored on the board does not decode");
        return;
    }

    backup.take(bridge);

    cfg.setBoardRevision(131);
    const bool saved = cfg.save();

    Steami.end();
    const bool began = Steami.begin();
    const SteamiOrigin origin = Steami.origin();
    const int32_t revision = Steami.revision();

    const bool restored = backup.restore(bridge);
    Steami.end();

    TEST_ASSERT_TRUE_MESSAGE(restored, "the config zone was not restored");
    TEST_ASSERT_TRUE(saved);
    TEST_ASSERT_TRUE(began);
    TEST_ASSERT_TRUE(origin == SteamiOrigin::CONFIG);
    TEST_ASSERT_EQUAL_INT32(131, revision);
}

void test_logger_before_serial_begin_does_not_block(void) {
    // 200 characters, longer than the stored message.
    char text[201];
    memset(text, 'x', 200);
    memcpy(text, "LONG-TEXT-", 10);
    text[200] = '\0';

    Serial.end();
    Steami.report("SteamiBoard", SteamiStatus::OK, text, 99);
    Steami.report("SteamiBoard", SteamiStatus::OK, text, 99);
    Serial.begin(115200);
    delay(100);  // Unity prints through Serial: let the port settle.

    TEST_ASSERT_NOT_NULL(strstr(Steami.lastError(), "LONG-TEXT-"));
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_detects_a_known_release);
    RUN_TEST(test_factory_revision_round_trip);
    RUN_TEST(test_logger_before_serial_begin_does_not_block);
    UNITY_END();
}

void loop() {}
