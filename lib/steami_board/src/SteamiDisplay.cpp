// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiDisplay.h"

#include "SteamiTrig.h"

namespace {

const char* const kUnknownBoardHint =
    "cannot tell if this board is a V1a/V1b or a V1c. "
    "Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>.";
const char* const kNoMemoryHint =
    "not enough RAM for the colour screen (115200 bytes in one block). "
    "Call screen.begin() first in setup().";

/// The largest |dx| that the midpoint outline of radius r (1 to 32767) lights on the row k rows
/// from its centre (0 <= k <= r), in closed form so a filled circle visits only the panel rows.
/// The walk of circle() puts on row k the octant point x, the largest with x(x - 1) + k^2 < r^2,
/// as long as 2k^2 - k < r^2; further out the row holds only mirrored points, the widest at the
/// largest y with y^2 < r^2 - k(k - 1). Both arguments of steamiIsqrt() stay under 2^32.
int32_t circleHalfWidth(int32_t r, int32_t k) {
    const int64_t r2 = static_cast<int64_t>(r) * r;
    const int64_t k2 = static_cast<int64_t>(k) * k;
    if (2 * k2 - k < r2) {
        // x(x - 1) <= r^2 - k^2 - 1  <=>  (2x - 1)^2 <= 4(r^2 - k^2 - 1) + 1
        const uint32_t s = steamiIsqrt(static_cast<uint32_t>(4 * (r2 - k2 - 1) + 1));
        return static_cast<int32_t>((s + 1) / 2);
    }
    return static_cast<int32_t>(steamiIsqrt(static_cast<uint32_t>(r2 - k2 + k - 1)));
}

/// A triangle edge oriented from its lower-y end (ties: lower x), so that the vertex order of the
/// call does not change the pixels.
struct Edge {
    int32_t xa, ya, xb, yb;
};

Edge orientedEdge(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (y2 < y1 || (y2 == y1 && x2 < x1)) {
        return {x2, y2, x1, y1};
    }
    return {x1, y1, x2, y2};
}

/// x of a non-horizontal edge on the row y (ya <= y <= yb): xa + (xb - xa)(y - ya) / (yb - ya),
/// rounded to nearest with halves away from zero. The product needs int64_t at the int16_t limits.
int32_t edgeX(const Edge& e, int32_t y) {
    const int64_t num = static_cast<int64_t>(e.xb - e.xa) * (y - e.ya);
    const int64_t den = e.yb - e.ya;
    const int64_t q = (2 * (num < 0 ? -num : num) + den) / (2 * den);
    return e.xa + static_cast<int32_t>(num < 0 ? -q : q);
}

}  // namespace

bool SteamiDisplay::begin() {
    _status = _board->beginDisplay();
    if (_status == SteamiStatus::UNKNOWN_BOARD) {
        _board->report(_who, _status, kUnknownBoardHint, STEAMI_E_DISPLAY_UNKNOWN_BOARD);
    } else if (_status == SteamiStatus::NO_MEMORY) {
        _board->report(_who, _status, kNoMemoryHint, STEAMI_E_NO_MEMORY);
    }
    return _status == SteamiStatus::OK;
}

const char* SteamiDisplay::error() const {
    switch (_status) {
        case SteamiStatus::OK:
            return "";
        case SteamiStatus::NOT_STARTED:
            return "not started";
        case SteamiStatus::UNKNOWN_BOARD:
            return "unknown board";
        case SteamiStatus::NO_MEMORY:
            return "no memory";
        default:
            return "not available";
    }
}

SteamiDisplayBackend* SteamiDisplay::backend() {
    return _board->display();
}

bool SteamiDisplay::isColor() {
    SteamiDisplayBackend* b = backend();
    return b != nullptr && b->isColor();
}

int16_t SteamiDisplay::width() {
    SteamiDisplayBackend* b = backend();
    return b == nullptr ? 0 : b->width();
}

int16_t SteamiDisplay::height() {
    SteamiDisplayBackend* b = backend();
    return b == nullptr ? 0 : b->height();
}

int16_t SteamiDisplay::centerX() {
    return static_cast<int16_t>(width() / 2);
}

int16_t SteamiDisplay::centerY() {
    return static_cast<int16_t>(height() / 2);
}

int16_t SteamiDisplay::radius() {
    SteamiDisplayBackend* b = backend();
    if (b == nullptr) {
        return 0;
    }
    return static_cast<int16_t>((b->width() < b->height() ? b->width() : b->height()) / 2);
}

SteamiDisplayBackend* SteamiDisplay::ready() {
    if (_status == SteamiStatus::NOT_STARTED) {
        begin();
    }
    // The board may have been ended since: ask it again, it answers from its cache.
    if (_status != SteamiStatus::OK || _board->beginDisplay() != SteamiStatus::OK) {
        return nullptr;
    }
    return _board->display();
}

void SteamiDisplay::dot(SteamiDisplayBackend* b, int32_t x, int32_t y, uint16_t c) {
    if (x < 0 || y < 0 || x >= b->width() || y >= b->height()) {
        return;
    }
    b->pixel(static_cast<int16_t>(x), static_cast<int16_t>(y), c);
}

void SteamiDisplay::span(SteamiDisplayBackend* b, int32_t x, int32_t y, int32_t w, int32_t h,
                         uint16_t c) {
    if (w <= 0 || h <= 0) {
        return;
    }
    const int32_t x0 = x < 0 ? 0 : x;
    const int32_t y0 = y < 0 ? 0 : y;
    const int32_t x1 = x + w > b->width() ? b->width() : x + w;  // exclusive
    const int32_t y1 = y + h > b->height() ? b->height() : y + h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    b->fillRect(static_cast<int16_t>(x0), static_cast<int16_t>(y0), static_cast<int16_t>(x1 - x0),
                static_cast<int16_t>(y1 - y0), c);
}

void SteamiDisplay::walk(SteamiDisplayBackend* b, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                         uint8_t w, uint16_t c) {
    if (w == 0) {
        return;
    }
    // Canonical order (lower x, then lower y): a reversed line lights the same pixels.
    if (x2 < x1 || (x2 == x1 && y2 < y1)) {
        const int32_t tx = x1;
        const int32_t ty = y1;
        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }
    const int32_t dx = x2 - x1;
    const int32_t dy = y2 > y1 ? y1 - y2 : y2 - y1;  // minus the absolute height
    const int32_t sy = y1 < y2 ? 1 : -1;
    int32_t err = dx + dy;
    int32_t x = x1;
    int32_t y = y1;
    for (;;) {
        if (w == 1) {
            dot(b, x, y, c);
        } else {
            span(b, x - (w - 1) / 2, y - (w - 1) / 2, w, w, c);
        }
        if (x == x2 && y == y2) {
            break;
        }
        const int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += 1;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

void SteamiDisplay::clear(SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->fill(c.rgb565());
    }
}

void SteamiDisplay::pixel(int16_t x, int16_t y, SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        dot(b, x, y, c.rgb565());
    }
}

void SteamiDisplay::show() {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->show();
    }
}

void SteamiDisplay::rotate(bool upsideDown) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->rotate(upsideDown);
    }
}

void SteamiDisplay::invert(bool on) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->invert(on);
    }
}

void SteamiDisplay::powerOn() {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->powerOn();
    }
}

void SteamiDisplay::powerOff() {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        b->powerOff();
    }
}

void SteamiDisplay::line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        walk(b, x1, y1, x2, y2, 1, c.rgb565());
    }
}

void SteamiDisplay::strokeLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t w,
                               SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        walk(b, x1, y1, x2, y2, w, c.rgb565());
    }
}

void SteamiDisplay::hline(int16_t x, int16_t y, int16_t w, SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b != nullptr) {
        span(b, x, y, w, 1, c.rgb565());
    }
}

void SteamiDisplay::vline(int16_t x, int16_t y, int16_t h, SteamiRgb c) {
    SteamiDisplayBackend* b = ready();
    if (b == nullptr) {
        return;
    }
    span(b, x, y, 1, h, c.rgb565());
}

void SteamiDisplay::rect(int16_t x, int16_t y, int16_t w, int16_t h, SteamiRgb c, bool fill) {
    SteamiDisplayBackend* b = ready();
    if (b == nullptr || w <= 0 || h <= 0) {
        return;
    }
    const uint16_t v = c.rgb565();
    if (fill || w == 1 || h == 1) {
        span(b, x, y, w, h, v);
        return;
    }
    // Outline: no pixel twice. The sides sit between the top and the bottom row.
    span(b, x, y, w, 1, v);
    span(b, x, static_cast<int32_t>(y) + h - 1, w, 1, v);
    if (h > 2) {
        span(b, x, static_cast<int32_t>(y) + 1, 1, h - 2, v);
        span(b, static_cast<int32_t>(x) + w - 1, static_cast<int32_t>(y) + 1, 1, h - 2, v);
    }
}

void SteamiDisplay::circle(int16_t cx, int16_t cy, int16_t r, SteamiRgb c, bool fill) {
    SteamiDisplayBackend* b = ready();
    if (b == nullptr || r < 0) {
        return;
    }
    const uint16_t v = c.rgb565();
    if (r == 0) {
        if (fill) {
            span(b, cx, cy, 1, 1, v);
        } else {
            dot(b, cx, cy, v);
        }
        return;
    }
    if (fill) {
        // One span per panel row of the disc, as wide as the outline on that row.
        const int32_t top = cy - r < 0 ? 0 : cy - r;
        const int32_t bottom = cy + r >= b->height() ? b->height() - 1 : cy + r;
        for (int32_t y = top; y <= bottom; y++) {
            const int32_t half = circleHalfWidth(r, y < cy ? cy - y : y - cy);
            span(b, cx - half, y, 2 * half + 1, 1, v);
        }
        return;
    }
    // Midpoint walk over the octant from (r, 0) to the diagonal, mirrored 8 ways. d is the test of
    // the next midpoint, (x - 1/2)^2 + (y + 1)^2 - r^2 - 1/4 = x(x - 1) + (y + 1)^2 - r^2: an
    // integer, negative exactly when the midpoint is inside. Off the panel dot() drops the point.
    int32_t x = r;
    int32_t y = 0;
    int32_t d = 1 - r;
    while (x >= y) {
        dot(b, cx + x, cy + y, v);
        dot(b, cx - x, cy + y, v);
        dot(b, cx + x, cy - y, v);
        dot(b, cx - x, cy - y, v);
        dot(b, cx + y, cy + x, v);
        dot(b, cx - y, cy + x, v);
        dot(b, cx + y, cy - x, v);
        dot(b, cx - y, cy - x, v);
        y++;
        if (d < 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void SteamiDisplay::triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                             SteamiRgb c, bool fill) {
    SteamiDisplayBackend* b = ready();
    if (b == nullptr) {
        return;
    }
    const uint16_t v = c.rgb565();
    if (!fill) {  // the three line()s
        walk(b, x0, y0, x1, y1, 1, v);
        walk(b, x1, y1, x2, y2, 1, v);
        walk(b, x2, y2, x0, y0, 1, v);
        return;
    }
    const Edge edges[3] = {orientedEdge(x0, y0, x1, y1), orientedEdge(x1, y1, x2, y2),
                           orientedEdge(x2, y2, x0, y0)};
    int32_t ymin = edges[0].ya;
    int32_t ymax = edges[0].yb;
    for (const Edge& e : edges) {
        ymin = e.ya < ymin ? e.ya : ymin;
        ymax = e.yb > ymax ? e.yb : ymax;
    }
    const int32_t top = ymin < 0 ? 0 : ymin;
    const int32_t bottom = ymax >= b->height() ? b->height() - 1 : ymax;
    for (int32_t y = top; y <= bottom; y++) {
        // The edge from the lowest to the highest vertex spans every row: lo <= hi below.
        int32_t lo = INT32_MAX;
        int32_t hi = INT32_MIN;
        const auto take = [&lo, &hi](int32_t x) {
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
        };
        for (const Edge& e : edges) {
            if (y < e.ya || y > e.yb) {
                continue;
            }
            if (e.ya == e.yb) {  // a horizontal edge gives both of its ends
                take(e.xa);
                take(e.xb);
            } else {
                take(edgeX(e, y));
            }
        }
        span(b, lo, y, hi - lo + 1, 1, v);
    }
}
