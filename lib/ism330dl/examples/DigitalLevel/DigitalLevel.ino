// SPDX-License-Identifier: GPL-3.0-or-later
//
// ISM330DL - a bubble level on the screen.
//
// Lay the board screen up. The bubble moves to the highest side, as the air bubble of a spirit
// level, and the angle to the horizontal is written below. When the angle drops under
// LEVEL_DEG the bubble turns GREEN and the text reads "LEVEL"; it goes back to WHITE above
// UNLEVEL_DEG (the gap between the two keeps the state from flickering at the limit).
//
// Press A to zero the level on the surface the board lies on (the ISM330DL offsets, averaged over
// ZERO_SAMPLES readings); press B to drop the zero. Hold the button until the screen reacts:
// drawing a frame takes some time, and a short tap between two reads of the button can be missed.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen. The drawing is sized from screen.width(). The acceleration comes from the
// SteamiISM330DL driver, the drawing from SteamiScreen, the buttons from SteamiButtons.
//
// Axes: with the screen up, the accelerometer reads x > 0 when the top edge goes down and y > 0
// when the right edge goes down (the convention of SteamiISM330DL::orientation()).
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the ISM330DL does not answer, the screen says so and the sketch stops.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <SteamiISM330DL.h>
#include <SteamiScreen.h>

// --- Settings -------------------------------------------------------------------------------

constexpr float LEVEL_DEG = 1.0F;        // below this angle, the board is level
constexpr float UNLEVEL_DEG = 1.5F;      // above this angle, it is no longer level
constexpr float FULL_SCALE_DEG = 15.0F;  // the bubble touches the ring at this angle
constexpr float SMOOTHING = 0.3F;        // weight of a new reading, 1 for no smoothing
constexpr uint8_t ZERO_SAMPLES = 20;     // readings averaged when A zeroes the level
constexpr uint32_t LOOP_MS = 50;         // read the buttons at least every 50 ms

// --------------------------------------------------------------------------------------------

SteamiScreen screen;
SteamiButtons buttons;
SteamiISM330DL imu(Steami.wire());  // the board's internal I2C bus

float ax = 0.0F;  // smoothed acceleration, in g
float ay = 0.0F;
float az = -1.0F;
bool level = false;

void stopWithError(const char* message) {
    Serial.println(message);
    screen.clear();
    screen.title("Level");
    screen.subtitle("ISM330DL", "not detected", STEAMI_RED);
    screen.show();
    while (true) {
        delay(1000);
    }
}

// Averages a few readings with the offsets cleared, then stores them as the new offsets of x and
// y. z keeps no offset: it carries the gravity.
void zeroLevel() {
    screen.clear();
    screen.title("Level");
    screen.subtitle("Zeroing...", "keep still");
    screen.show();

    imu.setAccelOffset(0.0F, 0.0F, 0.0F);
    float sumX = 0.0F;
    float sumY = 0.0F;
    uint8_t count = 0;
    for (uint8_t i = 0; i < ZERO_SAMPLES; i++) {
        SteamiISM330DL::Vector3 a{};
        if (imu.accelerationG(a)) {
            sumX += a.x;
            sumY += a.y;
            count++;
        }
        delay(20);  // a new sample at 104 Hz
    }
    if (count > 0) {
        imu.setAccelOffset(sumX / count, sumY / count, 0.0F);
        ax = 0.0F;  // the smoothed values start again from the new zero
        ay = 0.0F;
    }

    Serial.print("Zero offsets (g): x=");
    Serial.print(imu.accelOffset().x, 3);
    Serial.print(" y=");
    Serial.println(imu.accelOffset().y, 3);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    screen.begin();  // first: see the frame buffer note above
    buttons.begin();

    if (!imu.begin()) {
        stopWithError("ISM330DL not detected.");
    }

    SteamiISM330DL::Vector3 a{};
    if (imu.accelerationG(a)) {
        ax = a.x;
        ay = a.y;
        az = a.z;
    }

    Serial.println("Tilt the board. A: zero the level, B: drop the zero.");
}

void draw(float tiltDeg) {
    const int16_t cx = screen.centerX();
    const int16_t cy = screen.centerY();
    const int16_t w = screen.width();
    // Sizes in pixels of the 128 px screen, scaled to the panel.
    const int16_t ringR = 34 * w / 128;
    const int16_t bubbleR = 8 * w / 128;
    const int16_t targetR = 10 * w / 128;
    const int16_t maxOffset = ringR - bubbleR - 2 * w / 128;

    // The bubble goes to the high side: opposite to the downhill direction (y, -x) on the screen.
    int16_t bx = cx;
    int16_t by = cy;
    const float h = sqrtf(ax * ax + ay * ay);
    if (h > 1e-4F) {
        const float ratio = tiltDeg >= FULL_SCALE_DEG ? 1.0F : tiltDeg / FULL_SCALE_DEG;
        const float d = ratio * maxOffset;
        bx = cx + static_cast<int16_t>(lroundf(-ay / h * d));
        by = cy + static_cast<int16_t>(lroundf(ax / h * d));
    }

    char text[24];
    if (level) {
        snprintf(text, sizeof(text), "LEVEL");
    } else {
        // Tenths of a degree in integers: the angle is from 0 to 90 degrees.
        const int tenths = static_cast<int>(lroundf(tiltDeg * 10.0F));
        snprintf(text, sizeof(text), "%d.%d deg", tenths / 10, tenths % 10);
    }

    const SteamiRgb bubbleColor = level ? STEAMI_GREEN : STEAMI_WHITE;

    screen.clear();
    screen.title("Level");
    screen.circle(cx, cy, ringR, STEAMI_DARK);
    screen.hline(cx - ringR, cy, 2 * ringR + 1, STEAMI_GRID_DARK);
    screen.vline(cx, cy - ringR, 2 * ringR + 1, STEAMI_GRID_DARK);
    screen.circle(cx, cy, targetR, STEAMI_GRAY);
    screen.circle(bx, by, bubbleR, bubbleColor, true);
    screen.subtitle(text, level ? STEAMI_GREEN : STEAMI_LIGHT);
    screen.show();  // one frame at a time: no flicker
}

void loop() {
    const uint32_t start = millis();

    if (buttons.wasPressed(SteamiButton::A)) {
        zeroLevel();
    } else if (buttons.wasPressed(SteamiButton::B)) {
        imu.setAccelOffset(0.0F, 0.0F, 0.0F);
        Serial.println("Zero dropped.");
    }

    SteamiISM330DL::Vector3 a{};
    if (imu.accelerationG(a)) {
        ax += SMOOTHING * (a.x - ax);
        ay += SMOOTHING * (a.y - ay);
        az += SMOOTHING * (a.z - az);
    }

    // Angle between the gravity and the axis of the screen, whichever face is up.
    const float tiltDeg = atan2f(sqrtf(ax * ax + ay * ay), fabsf(az)) * 180.0F / PI;

    const bool wasLevel = level;
    level = level ? tiltDeg < UNLEVEL_DEG : tiltDeg < LEVEL_DEG;
    if (level != wasLevel) {
        Serial.println(level ? "Level." : "Not level.");
    }

    draw(tiltDeg);

    const uint32_t spent = millis() - start;
    if (spent < LOOP_MS) {
        delay(LOOP_MS - spent);
    }
}
