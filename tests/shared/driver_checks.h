// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/*
 * Shared driver checks for dual-use test suites.
 *
 * Call these helpers from native tests with exact mock-loaded values, and from
 * hardware tests with plausibility ranges. The goal is to keep native and
 * hardware-unit suites aligned around the same public driver contract.
 */

#include <unity.h>

template <class D>
inline void check_begin(D& drv) {
    TEST_ASSERT_TRUE(drv.begin());
}

template <class D>
inline void check_who_am_i(D& drv, uint8_t expected) {
    TEST_ASSERT_EQUAL_UINT8(expected, drv.deviceId());
}

template <class D, class Reading>
inline void check_read_plausible(D& drv, Reading minValue, Reading maxValue) {
    const Reading value = drv.read();
    TEST_ASSERT_GREATER_OR_EQUAL(minValue, value);
    TEST_ASSERT_LESS_OR_EQUAL(maxValue, value);
}

template <class D, class Reading>
inline void check_read_plausible(D& drv, Reading (D::*reader)(), Reading minValue,
                                 Reading maxValue) {
    const Reading value = (drv.*reader)();
    TEST_ASSERT_GREATER_OR_EQUAL(minValue, value);
    TEST_ASSERT_LESS_OR_EQUAL(maxValue, value);
}

template <class D, class Result, class Value>
inline void check_read_plausible(D& drv, Result (D::*reader)(), Value Result::* field,
                                 Value minValue, Value maxValue) {
    const Result value = (drv.*reader)();
    TEST_ASSERT_GREATER_OR_EQUAL(minValue, value.*field);
    TEST_ASSERT_LESS_OR_EQUAL(maxValue, value.*field);
}
