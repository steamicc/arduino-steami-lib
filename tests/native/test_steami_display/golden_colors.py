# SPDX-License-Identifier: GPL-3.0-or-later
"""Golden RGB565 and grey levels of the STeaMi colours (colors.py rule). Run: python3 golden_colors.py"""
C = {"BLACK": (0, 0, 0), "DARK": (102,) * 3, "GRAY": (153,) * 3, "LIGHT": (187,) * 3, "WHITE": (255,) * 3,
     "GREEN": (119, 255, 119), "RED": (255, 85, 85), "BLUE": (85, 85, 255), "YELLOW": (255, 255, 85), "GRID_DARK": (51,) * 3}
for name, (r, g, b) in C.items():
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    r5, g6, b5 = v >> 11, (v >> 5) & 63, v & 31
    R, G, B = (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)
    print(f"{name:10} 0x{v:04X} {((R * 77 + G * 150 + B * 29) >> 8) >> 4}")
