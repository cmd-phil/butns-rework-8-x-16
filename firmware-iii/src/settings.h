#pragma once

#include <stdbool.h>
#include <stdint.h>

// key handler in grid coordinates (0-based), z = 1 pressed, 0 released
typedef void (*grid_key_fn)(int x, int y, int z);

void settings_load(void);
uint8_t settings_max_brightness(void);  // 1..16, 16 = 100 %
bool settings_ui_active(void);

// keys: scans the matrix, runs the brightness setting, passes all other keys on.
// Keys held while ui_init() runs are ignored until released.
void ui_init(grid_key_fn fn);
void ui_task(void);
