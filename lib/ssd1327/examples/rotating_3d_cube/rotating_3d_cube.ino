// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SSD1327.h>

#ifndef DATA_COMMAND_DISPLAY
#define DATA_COMMAND_DISPLAY SPI_INT_MISO
#endif

WS_OLED_128X128_STEAMI display;

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

int16_t screenX[8];
uint8_t screenY[8];
float angle = 0.0f;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!display.begin()) {
        Serial.println("SSD1327 not found");
        while (true) {
        }
    }
}

void loop() {
    display.fill(0);

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

        screenX[i] = 64 + (int16_t)(x1 * 42.0f / z2);
        screenY[i] = 64 + (int16_t)(y1 * 42.0f / z2);
    }

    for (uint8_t i = 0; i < 12; i++) {
        uint8_t a = edges[i][0];
        uint8_t b = edges[i][1];
        display.line(screenX[a], screenY[a], screenX[b], screenY[b], 15);
    }

    display.show();
    angle += 0.06f;
    delay(30);
}
