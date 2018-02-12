#pragma once

#define GPIO_NUM_VDD        153

enum {
    Tsc2046_start = 0x1 << 7,
    Tsc2046_x     = 0x1 << 4,
    Tsc2046_z1    = 0x3 << 4,
    Tsc2046_z2    = 0x4 << 4,
    Tsc2046_y     = 0x5 << 4,
};
