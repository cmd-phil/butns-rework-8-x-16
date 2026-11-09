#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

void leds_init(void);

// default tint used by leds_reset()
void leds_set_default_tint(uint8_t r, uint8_t g, uint8_t b);
// back to defaults: tint, intensity, colour gain, no RGB overrides; clear = all levels 0
void leds_reset(bool clear);

// grid coordinates, 0-based. Levels 0..15.
void leds_set(int x, int y, int z, bool rel);
int leds_get(int x, int y);
void leds_all(int z, bool rel);
void leds_rgb(int x, int y, int r, int g, int b);
void leds_tint(int r, int g, int b);
void leds_intensity(int z);        // 0..15, whole grid
void leds_color_intensity(int z);  // 0..15, gain for leds_rgb pixels

void leds_mark_dirty(void);
// sends a frame when something changed and the LED chain is free
void leds_task(void);
