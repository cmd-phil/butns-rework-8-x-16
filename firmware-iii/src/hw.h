#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

// boot keys, physical positions (USB connector on the left)
#define BOOT_KEY_TOP_LEFT     (1u << 0)
#define BOOT_KEY_BOTTOM_RIGHT (1u << 1)

typedef void (*hw_key_cb)(int row, int col, int z);

void hw_init(void);
// full matrix scan; calls cb for every physical key whose state changed
void hw_scan(hw_key_cb cb);
bool hw_pressed(int row, int col);
// reads the matrix once and stores it as the current state without callbacks
uint32_t hw_boot_keys(void);

// rotation: 0 = default, 2 = 180 degrees
void hw_set_rotation(int r);
// grid coordinates <-> physical position (both directions, the mapping is its own inverse)
void hw_map(int a_row, int a_col, int *b_row, int *b_col);
// LED chain index of a grid coordinate
int hw_led_index(int x, int y);
