# 🛰️ BLE Indoor Map — Activity Sheet

> **Duration**: 2h
> **Hardware**: 4 STeaMi boards (STM32WB55), tape measure, serial monitor

---

## 🎯 Learning Objectives

By the end of this activity, students will be able to:

- **Map a physical space** using a Cartesian coordinate system (cm) from real measurements
- **Understand BLE trilateration**: RSSI → distance → (x, y) position conversion
- **Visualize in real time** a position on an ASCII map in the serial monitor
- **Apply an exponential filter** to smooth noisy measurements
- **Implement a complete embedded pipeline**: BLE sensor → processing → display
---

## 📦 Required Hardware

| Quantity | Item |
|----------|------|
| 3 | STeaMi boards (role: Beacon) |
| 1 | STeaMi board (role: Mobile/Scanner) |
| 1 | Tape measure (≥ 5m) |
| 1 | PC with PlatformIO and serial monitor |
| 1 | Classroom or open corridor |

---

## 🏗️ Room Setup

### 1. Choosing Beacon Positions

The 3 beacons must form a **non-degenerate triangle** covering the movement area. A right-angle or isosceles triangle works well. Avoid collinear configurations (all 3 beacons on the same line).

**Example layout for a standard classroom:**

```
M3 (330, 278)
  *

              * M2 (420, 0)
* M1 (0, 0)
```

### 2. Physical Measurements

1. Place **M1** in a corner of the room — this is the **origin (0, 0)**
2. Place **M2** along a wall from M1 — record the distance in **cm**
3. Place **M3** at a third point — measure its distances to M1 and M2, then compute its (x, y) coordinates using trigonometry or a set square
> 💡 **Tip**: use a right angle between M1-M2 and M1-M3 to simplify coordinate calculation.

### 3. Update Coordinates in the Code

In `main.cpp`, modify the following arrays:

```cpp
static const float BEACON_X[BEACON_COUNT] = {0.0f, 420.0f, 330.0f};
static const float BEACON_Y[BEACON_COUNT] = {0.0f,   0.0f, 278.0f};
```

Replace the values with your measurements in centimeters.

---

## 📡 Coordinate System

```
Y (cm)
↑
|        M3
|
|
+----------→ X (cm)
M1       M2
```

- **Origin (0, 0)**: beacon M1, reference corner
- **X axis**: room length (direction M1 → M2)
- **Y axis**: room width (direction M1 → M3 projected)
- **Unit**: centimeters
---

## ⚙️ Coordinate System Calibration

### RSSI Parameters to Adjust

Each beacon has two radio model parameters to calibrate:

```cpp
static const float RSSI_REF[BEACON_COUNT]    = {-75.0f, -75.0f, -80.0f};
static const float PATH_LOSS_N[BEACON_COUNT] = { 3.99f,  4.32f,  2.66f};
```

| Parameter | Meaning | Typical value |
|-----------|---------|---------------|
| `RSSI_REF` | RSSI measured at exactly 1 meter from the beacon (dBm) | -65 to -80 dBm |
| `PATH_LOSS_N` | Path loss exponent (environment) | 2.0 (free space) to 4.5 (heavy obstruction) |

### Calibration Procedure

1. **Measure RSSI_REF**: place the mobile board at exactly **100 cm** from each beacon and record the RSSI shown in the terminal
2. **Adjust PATH_LOSS_N**: move the mobile board to known distances (100, 200, 300 cm) and tune N until the computed distance matches the real distance
3. **Verify**: place the mobile board at a known position (e.g. center of the triangle) and check that the displayed position is consistent
> 💡 **Tip**: indoors with walls and furniture, `N` is typically between 3.0 and 4.5.

---

## 🚀 Deployment

### Flashing the Beacons

Each beacon must run the `BleBeacon` firmware with its unique name:

```
Beacon_M1 → STeaMi board #1
Beacon_M2 → STeaMi board #2
Beacon_M3 → STeaMi board #3
```

### Flashing the Scanner

The fourth board runs `BleIndoorMap`:

```bash
pio run --target upload && pio device monitor
```

### Available Serial Commands

| Key | Action |
|-----|--------|
| `c` | Clear trail and reset position |
| `h` | Display help and beacon layout |

---

## 🗺️ Reading the ASCII Map

```
+------ BLE Indoor Map ------+
|                            |
|  3                         |
|                            |
|              X             |
|         .                  |
|    .                       |
|1                    2      |
+----------------------------+
1=M1  2=M2  3=M3  X=You  .=trail

--- Beacons ---
  Beacon_M1: OK  RSSI=-72dBm  dist=185cm  age=0s
  Beacon_M2: OK  RSSI=-78dBm  dist=310cm  age=1s
  Beacon_M3: OK  RSSI=-80dBm  dist=220cm  age=0s

Position: X=210cm  Y=140cm
```

| Symbol | Meaning |
|--------|---------|
| `1` `2` `3` | Fixed positions of beacons M1, M2, M3 |
| `X` | Current estimated position of the mobile board |
| `.` | Trail of the last 5 positions |
| `NOT SEEN` | Beacon not detected for more than 5s |

---

## 🎓 Classroom Demo Scenario

### Suggested Schedule (30 min)

**Phase 1 — Setup (5 min)**
> The teacher places the 3 beacons at the defined positions. Students measure distances and compute coordinates.

**Phase 2 — Static Observation (5 min)**
> A student holds the mobile board still at a known position (e.g. center of the room). The class observes the displayed position and compares it with the actual position.

**Phase 3 — Linear Movement (5 min)**
> The student walks slowly in a straight line from M1 to M2. The class observes the `.` trail forming and verifies that the trajectory matches the real movement.

**Phase 4 — Free Movement (10 min)**
> The student moves freely around the room while the mobile board traces their path in real time. The class observes the effect of the filter (`ALPHA = 0.15`) on position stability.

**Phase 5 — Wrap-up Questions (5 min)**

- Why are at least 3 beacons required?
- What happens if a beacon disappears (`NOT SEEN`)?
- How does the exponential filter affect responsiveness vs. stability?
- What are the limitations of BLE trilateration indoors?
---

## 🧮 Mathematical Background

### RSSI → Distance Conversion

$$d = 10^{\frac{RSSI_{ref} - RSSI}{10 \times N}} \times 100 \text{ cm}$$

### Trilateration (Linear System)

Given 3 distances $r_1, r_2, r_3$ to beacons $(x_1,y_1)$, $(x_2,y_2)$, $(x_3,y_3)$:

$$A \cdot x + B \cdot y = C$$
$$D \cdot x + E \cdot y = F$$

The system is solved by direct substitution (Cramer's rule).

### Exponential Filter

$$x_{filtered} = \alpha \cdot x_{new} + (1 - \alpha) \cdot x_{filtered}$$

With $\alpha = 0.15$: high inertia, stable position but responsive to large movements thanks to the `MIN_MOVE_CM = 15 cm` threshold.

---

*Documentation written for the STeaMi wiki — STM32WB55 / BLE trilateration*
