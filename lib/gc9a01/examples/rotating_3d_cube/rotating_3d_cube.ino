// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SteamiGC9A01.h>

SteamiGC9A01 display;

struct Point3D {
    float x;
    float y;
    float z;
};

const Point3D cube[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};

const uint8_t edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

// back face, front face, then the four edges joining them
const uint16_t edgeColors[3] = {GC9A01_RED, GC9A01_CYAN, GC9A01_YELLOW};

int16_t screenX[8];
int16_t screenY[8];
float angle = 0.0f;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("GC9A01 begin failed: not enough RAM for the frame buffer");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    display.fill(GC9A01_BLACK);

    float sinA = sin(angle);
    float cosA = cos(angle);
    float sinB = sin(angle * 0.7f);
    float cosB = cos(angle * 0.7f);

    for (uint8_t i = 0; i < 8; i++) {
        float x = cube[i].x;
        float y = cube[i].y;
        float z = cube[i].z;

        float x1 = x * cosA - z * sinA;
        float z1 = x * sinA + z * cosA;
        float y1 = y * cosB - z1 * sinB;
        float z2 = y * sinB + z1 * cosB + 4.0f;

        screenX[i] = 120 + (int16_t)(x1 * 110.0f / z2);
        screenY[i] = 120 + (int16_t)(y1 * 110.0f / z2);
    }

    for (uint8_t i = 0; i < 12; i++) {
        uint8_t a = edges[i][0];
        uint8_t b = edges[i][1];
        display.line(screenX[a], screenY[a], screenX[b], screenY[b], edgeColors[i / 4]);
    }

    display.show();
    angle += 0.08f;
}
