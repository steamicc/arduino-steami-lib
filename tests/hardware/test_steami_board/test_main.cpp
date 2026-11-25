// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for the board layer on a real STeaMi.
 *
 * test_factory_revision_round_trip writes the DAPLink configuration zone.
 * It writes only on a V1c detected by probing whose config loads, has no
 * revision and could be saved (non-empty); in every other case it is
 * ignored before any write. The saved zone is written back before any
 * assertion.
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

    // Every check below comes before the first write: in doubt, the zone is left alone.
    SteamiConfig cfg(bridge);
    if (!cfg.begin() || !cfg.load()) {
        TEST_IGNORE_MESSAGE("the board config could not be loaded: nothing written.");
        return;
    }
    int32_t existing = 0;
    if (cfg.boardRevision(existing)) {
        TEST_IGNORE_MESSAGE("the board already has a revision: nothing written.");
        return;
    }
    backup.take(bridge);
    if (backup.length == 0) {
        TEST_IGNORE_MESSAGE("the config zone could not be saved: nothing written.");
        return;
    }
    Steami.end();
    const bool probed = Steami.begin() && Steami.release() == SteamiRelease::V1C &&
                        Steami.origin() == SteamiOrigin::PROBE;
    Steami.end();
    if (!probed) {
        TEST_IGNORE_MESSAGE("not a V1c detected by probing: nothing written.");
        return;
    }

    cfg.setBoardRevision(131);
    const bool saved = cfg.save();

    Steami.end();
    const bool began = Steami.begin();
    const SteamiOrigin origin = Steami.origin();
    const int32_t revision = Steami.revision();

    // Restore before asserting, so that a failed assertion leaves the zone as it was.
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
