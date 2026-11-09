#include "settings.h"

#include <string.h>

#include "config.h"
#include "flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hw.h"
#include "leds.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// ---------------------------------------------------------------------------
// storage: the sector after iii's status sector. iii reserves a whole
// 64 KB block for status, the file system starts after it.

#define SETTINGS_MAGIC 0xB5

static uint8_t max_brightness = 16;

static uint32_t settings_offset(void) { return flash_get_status_offset() + FLASH_SECTOR_SIZE; }

void settings_load(void) {
  const uint8_t *p = (const uint8_t *)(XIP_BASE + settings_offset());
  if (p[0] == SETTINGS_MAGIC && p[1] >= 1 && p[1] <= 16) max_brightness = p[1];
}

// core 1 runs as a multicore lockout victim, see main.c
static void settings_save(void) {
  uint8_t buf[FLASH_PAGE_SIZE];
  memset(buf, 0xFF, sizeof(buf));
  buf[0] = SETTINGS_MAGIC;
  buf[1] = max_brightness;
  multicore_lockout_start_blocking();
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(settings_offset(), FLASH_SECTOR_SIZE);
  flash_range_program(settings_offset(), buf, FLASH_PAGE_SIZE);
  restore_interrupts(ints);
  multicore_lockout_end_blocking();
}

uint8_t settings_max_brightness(void) { return max_brightness; }

// ---------------------------------------------------------------------------
// keys and brightness setting
//
// Hold both top corner keys (grid 1,1 and 16,1) for SETTINGS_HOLD_MS.
// Then each key press selects its column as maximum brightness
// (column 1 = 1/16 ... column 16 = 100 %). The setting ends
// SETTINGS_TIMEOUT_MS after the last key activity and is stored.

static grid_key_fn key_fn;
static bool ignore[NUM_ROWS][NUM_COLS];  // held keys whose release is swallowed
static uint32_t last_scan;
static bool ui_active;
static uint32_t combo_since;
static uint32_t last_activity;
static uint8_t previous;

bool settings_ui_active(void) { return ui_active; }

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

static void on_key(int row, int col, int z) {
  if (ignore[row][col]) {
    if (!z) ignore[row][col] = false;
    return;
  }
  int y, x;
  hw_map(row, col, &y, &x);
  if (ui_active) {
    last_activity = now_ms();
    if (z) max_brightness = (uint8_t)(x + 1);
    return;
  }
  if (key_fn) key_fn(x, y, z);
}

void ui_init(grid_key_fn fn) {
  key_fn = fn;
  hw_scan(NULL);
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < NUM_COLS; c++) ignore[r][c] = hw_pressed(r, c);
  last_scan = time_us_32();
}

static bool any_pressed(void) {
  for (int r = 0; r < NUM_ROWS; r++)
    for (int c = 0; c < NUM_COLS; c++)
      if (hw_pressed(r, c)) return true;
  return false;
}

static void check_combo(void) {
  int r0, c0, r1, c1;
  hw_map(0, 0, &r0, &c0);             // grid (1,1)
  hw_map(0, NUM_COLS - 1, &r1, &c1);  // grid (16,1)
  uint32_t now = now_ms();

  if (ui_active) {
    if (any_pressed()) last_activity = now;
    if (!any_pressed() && now - last_activity >= SETTINGS_TIMEOUT_MS) {
      ui_active = false;
      combo_since = 0;
      if (max_brightness != previous) settings_save();
      leds_mark_dirty();
    }
    return;
  }

  if (hw_pressed(r0, c0) && hw_pressed(r1, c1) && !ignore[r0][c0] && !ignore[r1][c1]) {
    if (combo_since == 0) {
      combo_since = now ? now : 1;
    } else if (now - combo_since >= SETTINGS_HOLD_MS) {
      // release every held key towards the script / host, then take over
      for (int r = 0; r < NUM_ROWS; r++) {
        for (int c = 0; c < NUM_COLS; c++) {
          if (hw_pressed(r, c) && !ignore[r][c]) {
            int y, x;
            hw_map(r, c, &y, &x);
            if (key_fn) key_fn(x, y, 0);
            ignore[r][c] = true;
          }
        }
      }
      ui_active = true;
      previous = max_brightness;
      last_activity = now;
    }
  } else {
    combo_since = 0;
  }
}

void ui_task(void) {
  uint32_t t = time_us_32();
  if (t - last_scan >= SCAN_INTERVAL_US) {
    last_scan = t;
    hw_scan(on_key);
    check_combo();
  }
  leds_task();
}
