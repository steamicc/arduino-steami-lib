// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiDisplay.h"
#include "SteamiRgb.h"

/// The primitives gallery, shared by the native display suites. The Drawing example copies its
/// body line for line, and a script checks that the two bodies match. The 240 px colour dump is
/// the reference for the V1c photo.
inline void drawGallery(SteamiDisplay& d) {
    const int16_t cx = d.centerX(), cy = d.centerY(), r = d.radius();
    // s scales the gauge strokes; text() follows the panel's font by itself.
    const uint8_t s = d.width() >= 240 ? 2 : 1;
    d.clear();
    d.circle(cx, cy, r - 1, STEAMI_DARK);                         // panel edge
    d.arc(cx, cy, r - 4 * s, 225, 270, STEAMI_GRID_DARK, 6 * s);  // gauge track
    d.arc(cx, cy, r - 4 * s, 225, 135, STEAMI_GREEN, 6 * s);      // half fill
    d.text("STeaMi", cx - d.textWidth("STeaMi") / 2, cy - r / 2, STEAMI_WHITE);
    const char* temp =
        "20\xC2\xB0"
        "C \xC3\xA9";  // "20°C é"
    d.text(temp, cx - d.textWidth(temp) / 2, cy - r / 2 + d.textHeight() * 3 / 2, STEAMI_YELLOW);
    d.rect(cx - r / 2, cy - r / 8, r / 3, r / 4, STEAMI_LIGHT);
    d.rect(cx - r / 2 + 2, cy - r / 8 + 2, r / 3 - 4, r / 4 - 4, STEAMI_BLUE, true);
    d.circle(cx + r / 3, cy, r / 8, STEAMI_RED, true);
    d.triangle(cx - r / 4, cy + r / 2, cx + r / 4, cy + r / 2, cx, cy + r / 4, STEAMI_YELLOW, true);
    d.line(cx - r / 2, cy + r / 6, cx + r / 2, cy + r / 6, STEAMI_GRAY);
    d.show();
}
