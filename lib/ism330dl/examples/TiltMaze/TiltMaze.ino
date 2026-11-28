// SPDX-License-Identifier: GPL-3.0-or-later
//
// ISM330DL - tilt the board to roll a ball through a maze on the screen.
//
// Each round draws a new random maze. The ball (WHITE) starts in the top-left corner and the goal
// (GREEN) sits in the bottom-right one. Tilt the board: the ball rolls one cell towards the lower
// side every MOVE_PERIOD_MS while the tilt is above TILT_THRESHOLD_G. At the goal, the screen shows
// a score: 1000 points for the shortest path, fewer for every extra step. Then a new round starts.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen. The maze is sized from screen.width(), inside the square that fits the
// round panel. The acceleration comes from the SteamiISM330DL driver; the drawing from
// SteamiScreen.
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
#include <SteamiISM330DL.h>
#include <SteamiScreen.h>

// --- Settings -------------------------------------------------------------------------------

constexpr uint8_t MAZE_SIZE = 11;          // cells per side, odd, at most 15
constexpr float TILT_THRESHOLD_G = 0.30F;  // tilt needed to roll the ball, in g
constexpr uint32_t MOVE_PERIOD_MS = 180;   // time between two steps of the ball
constexpr uint32_t START_SCREEN_MS = 2000;
constexpr uint32_t WIN_SCREEN_MS = 4000;
constexpr int MAX_SCORE = 1000;

// --------------------------------------------------------------------------------------------

static_assert(MAZE_SIZE % 2 == 1 && MAZE_SIZE >= 5 && MAZE_SIZE <= 15,
              "MAZE_SIZE must be odd, from 5 to 15");

constexpr uint8_t START_ROW = 1;
constexpr uint8_t START_COL = 1;
constexpr uint8_t GOAL_ROW = MAZE_SIZE - 2;
constexpr uint8_t GOAL_COL = MAZE_SIZE - 2;

SteamiScreen screen;
SteamiISM330DL imu(Steami.wire());  // the board's internal I2C bus

bool wall[MAZE_SIZE][MAZE_SIZE];  // true: a wall, false: a path

int16_t cellPx = 0;   // side of one cell in pixels
int16_t originX = 0;  // top-left corner of the maze on the screen
int16_t originY = 0;

uint8_t ballRow = START_ROW;
uint8_t ballCol = START_COL;
int steps = 0;
int bestSteps = 0;
uint32_t lastMoveMs = 0;

// --- Maze -----------------------------------------------------------------------------------

// A perfect maze (one path between any two cells) by depth-first search, with an explicit stack
// instead of recursion. The odd cells are rooms; carving opens the wall between two rooms.
void generateMaze() {
    for (uint8_t r = 0; r < MAZE_SIZE; r++) {
        for (uint8_t c = 0; c < MAZE_SIZE; c++) {
            wall[r][c] = true;
        }
    }

    constexpr uint8_t ROOMS = (MAZE_SIZE / 2) * (MAZE_SIZE / 2);
    uint8_t stackRow[ROOMS];
    uint8_t stackCol[ROOMS];
    uint8_t depth = 0;

    wall[START_ROW][START_COL] = false;
    stackRow[depth] = START_ROW;
    stackCol[depth] = START_COL;
    depth++;

    const int8_t dRow[4] = {-2, 2, 0, 0};
    const int8_t dCol[4] = {0, 0, -2, 2};

    while (depth > 0) {
        const uint8_t row = stackRow[depth - 1];
        const uint8_t col = stackCol[depth - 1];

        // The unvisited rooms next to this one.
        uint8_t choices[4];
        uint8_t count = 0;
        for (uint8_t d = 0; d < 4; d++) {
            const int nr = row + dRow[d];
            const int nc = col + dCol[d];
            if (nr > 0 && nr < MAZE_SIZE - 1 && nc > 0 && nc < MAZE_SIZE - 1 && wall[nr][nc]) {
                choices[count++] = d;
            }
        }

        if (count == 0) {
            depth--;  // dead end: go back
            continue;
        }

        const uint8_t d = choices[random(count)];
        const uint8_t nr = row + dRow[d];
        const uint8_t nc = col + dCol[d];
        wall[row + dRow[d] / 2][col + dCol[d] / 2] = false;
        wall[nr][nc] = false;
        stackRow[depth] = nr;
        stackCol[depth] = nc;
        depth++;
    }
}

// Length of the shortest path from the start to the goal, by breadth-first search (every step
// costs 1). -1 when the goal cannot be reached.
int shortestPath() {
    constexpr uint16_t CELLS = MAZE_SIZE * MAZE_SIZE;
    uint8_t queue[CELLS];  // cell index row * MAZE_SIZE + col
    int16_t dist[CELLS];
    for (uint16_t i = 0; i < CELLS; i++) {
        dist[i] = -1;
    }

    uint16_t head = 0;
    uint16_t tail = 0;
    const uint8_t start = START_ROW * MAZE_SIZE + START_COL;
    dist[start] = 0;
    queue[tail++] = start;

    const int8_t dRow[4] = {-1, 1, 0, 0};
    const int8_t dCol[4] = {0, 0, -1, 1};

    while (head < tail) {
        const uint8_t cell = queue[head++];
        const uint8_t row = cell / MAZE_SIZE;
        const uint8_t col = cell % MAZE_SIZE;
        if (row == GOAL_ROW && col == GOAL_COL) {
            return dist[cell];
        }
        for (uint8_t d = 0; d < 4; d++) {
            const uint8_t nr = row + dRow[d];  // the border is all walls: no underflow is read
            const uint8_t nc = col + dCol[d];
            const uint8_t next = nr * MAZE_SIZE + nc;
            if (!wall[nr][nc] && dist[next] < 0) {
                dist[next] = dist[cell] + 1;
                queue[tail++] = next;
            }
        }
    }
    return -1;
}

int score() {
    if (bestSteps <= 0) {
        return 0;
    }
    if (steps <= bestSteps) {
        return MAX_SCORE;
    }
    return static_cast<int>(static_cast<long>(MAX_SCORE) * bestSteps / steps);
}

// --- Input ----------------------------------------------------------------------------------

// Tries to roll the ball one cell towards the lower side. Returns true when it moved.
bool rollBall() {
    SteamiISM330DL::Vector3 a{};
    if (!imu.accelerationG(a)) {
        return false;
    }

    // Downhill on the screen: right when the right edge is low (y > 0), up when the top edge is
    // low (x > 0).
    const float downX = a.y;
    const float downY = -a.x;
    const bool xFirst = fabsf(downX) >= fabsf(downY);

    // The steeper axis first; when a wall blocks it, the other one if it is tilted enough too, so
    // the ball can slide into a side corridor.
    for (uint8_t pass = 0; pass < 2; pass++) {
        const bool alongX = (pass == 0) == xFirst;
        const float g = alongX ? downX : downY;
        if (fabsf(g) < TILT_THRESHOLD_G) {
            continue;
        }
        const int8_t step = g > 0 ? 1 : -1;
        const uint8_t nr = alongX ? ballRow : ballRow + step;
        const uint8_t nc = alongX ? ballCol + step : ballCol;
        if (!wall[nr][nc]) {
            ballRow = nr;
            ballCol = nc;
            return true;
        }
    }
    return false;
}

// --- Drawing --------------------------------------------------------------------------------

void layout() {
    // The largest square inside the round panel has a side of radius * sqrt(2).
    const int16_t side = static_cast<int16_t>(screen.radius() * 1414L / 1000L);
    cellPx = side / MAZE_SIZE;
    const int16_t mazePx = cellPx * MAZE_SIZE;
    originX = screen.centerX() - mazePx / 2;
    originY = screen.centerY() - mazePx / 2;
}

void drawDisc(uint8_t row, uint8_t col, SteamiRgb c) {
    const int16_t r = cellPx / 2 - 1;
    screen.circle(originX + col * cellPx + cellPx / 2, originY + row * cellPx + cellPx / 2,
                  r > 1 ? r : 1, c, true);
}

void drawMaze() {
    screen.clear();
    for (uint8_t r = 0; r < MAZE_SIZE; r++) {
        for (uint8_t c = 0; c < MAZE_SIZE; c++) {
            if (wall[r][c]) {
                screen.rect(originX + c * cellPx, originY + r * cellPx, cellPx, cellPx, STEAMI_BLUE,
                            true);
            }
        }
    }
    drawDisc(GOAL_ROW, GOAL_COL, STEAMI_GREEN);
    drawDisc(ballRow, ballCol, STEAMI_WHITE);
    screen.show();  // one frame at a time: no flicker
}

void showStart() {
    screen.clear();
    screen.title("Tilt Maze");
    screen.face(SteamiFace::HAPPY);
    screen.subtitle("Tilt to play!");
    screen.show();
    delay(START_SCREEN_MS);
}

void showWin() {
    char line1[20];
    char line2[20];
    snprintf(line1, sizeof(line1), "Steps: %d", steps);
    snprintf(line2, sizeof(line2), "Best: %d", bestSteps);

    screen.clear();
    screen.title("You win!", STEAMI_GREEN);
    screen.value(score(), "pts");
    screen.subtitle(line1, line2);
    screen.show();
    delay(WIN_SCREEN_MS);
}

void stopWithError(const char* message) {
    Serial.println(message);
    screen.clear();
    screen.title("Tilt Maze");
    screen.subtitle("ISM330DL", "not detected", STEAMI_RED);
    screen.show();
    while (true) {
        delay(1000);
    }
}

// --- Game -----------------------------------------------------------------------------------

void newRound() {
    generateMaze();
    bestSteps = shortestPath();
    ballRow = START_ROW;
    ballCol = START_COL;
    steps = 0;

    Serial.print("New maze, shortest path: ");
    Serial.print(bestSteps);
    Serial.println(" steps.");

    showStart();
    drawMaze();
    lastMoveMs = millis();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    screen.begin();  // first: see the frame buffer note above
    layout();

    if (!imu.begin()) {
        stopWithError("ISM330DL not detected.");
    }

    // A different maze after each reset: mix the low bits of a few raw readings with the time.
    uint32_t seed = micros();
    for (uint8_t i = 0; i < 8; i++) {
        SteamiISM330DL::RawVector raw{};
        imu.accelerationRaw(raw);
        seed = seed * 31U + static_cast<uint16_t>(raw.x ^ raw.y ^ raw.z);
        delay(10);
    }
    randomSeed(seed);

    newRound();
}

void loop() {
    if (millis() - lastMoveMs < MOVE_PERIOD_MS) {
        return;
    }
    lastMoveMs = millis();

    if (!rollBall()) {
        return;  // nothing changed: keep the frame on screen
    }
    steps++;
    drawMaze();

    if (ballRow == GOAL_ROW && ballCol == GOAL_COL) {
        Serial.print("Goal in ");
        Serial.print(steps);
        Serial.print(" steps, score ");
        Serial.println(score());
        showWin();
        newRound();
    }
}
