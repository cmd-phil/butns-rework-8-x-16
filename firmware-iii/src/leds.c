#include "leds.h"

#include <string.h>

#include "hw.h"
#include "pico/stdlib.h"
#include "settings.h"
#include "ws2812.h"

static const uint8_t gamma_table[16] = {0,  2,  3,  6,  11, 18,  25,  32,
                                        41, 59, 70, 80, 92, 103, 115, 127};

static uint8_t level[NUM_LEDS];     // grid order: y * NUM_COLS + x
static bool override_rgb[NUM_LEDS];
static uint32_t rgb[NUM_LEDS];      // 0x00RRGGBB
static uint8_t tint_r, tint_g, tint_b;
static uint8_t def_r = 255, def_g = 255, def_b = 255;
static uint8_t intensity = 15;
static uint8_t px_gain = 255;
static bool dirty = true;
static uint32_t last_overlay;

void leds_init(void) {
  ws2812_init(LED_PIN, NUM_LEDS);
  leds_reset(true);
}

void leds_set_default_tint(uint8_t r, uint8_t g, uint8_t b) {
  def_r = r;
  def_g = g;
  def_b = b;
}

void leds_reset(bool clear) {
  tint_r = def_r;
  tint_g = def_g;
  tint_b = def_b;
  intensity = 15;
  px_gain = 255;
  memset(override_rgb, 0, sizeof(override_rgb));
  if (clear) memset(level, 0, sizeof(level));
  dirty = true;
}

static inline int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline bool inside(int x, int y) { return x >= 0 && x < NUM_COLS && y >= 0 && y < NUM_ROWS; }

void leds_set(int x, int y, int z, bool rel) {
  if (!inside(x, y)) return;
  int i = y * NUM_COLS + x;
  level[i] = (uint8_t)clamp(rel ? level[i] + z : z, 0, 15);
  override_rgb[i] = false;
}

int leds_get(int x, int y) {
  if (!inside(x, y)) return 0;
  return level[y * NUM_COLS + x];
}

void leds_all(int z, bool rel) {
  for (int i = 0; i < NUM_LEDS; i++) level[i] = (uint8_t)clamp(rel ? level[i] + z : z, 0, 15);
  memset(override_rgb, 0, sizeof(override_rgb));
}

void leds_rgb(int x, int y, int r, int g, int b) {
  if (!inside(x, y)) return;
  int i = y * NUM_COLS + x;
  rgb[i] = ((uint32_t)clamp(r, 0, 255) << 16) | ((uint32_t)clamp(g, 0, 255) << 8) |
           (uint32_t)clamp(b, 0, 255);
  override_rgb[i] = true;
}

void leds_tint(int r, int g, int b) {
  tint_r = (uint8_t)clamp(r, 0, 255);
  tint_g = (uint8_t)clamp(g, 0, 255);
  tint_b = (uint8_t)clamp(b, 0, 255);
}

void leds_intensity(int z) { intensity = (uint8_t)clamp(z, 0, 15); }

void leds_color_intensity(int z) { px_gain = (uint8_t)(clamp(z, 0, 15) * 255 / 15); }

void leds_mark_dirty(void) { dirty = true; }

// scales an 8 bit channel, keeps non-zero values visible
static inline uint32_t scale8(uint32_t c, uint32_t f) {
  if (c == 0 || f == 0) return 0;
  uint32_t o = (c * f + 128) >> 8;
  return o ? o : 1;
}

static void send(uint32_t *col) {
  // col[] holds 0x00RRGGBB in grid order; apply global factors, power limit, map to the chain
  static uint32_t grb[NUM_LEDS];
  uint32_t gain = (uint32_t)(intensity + 1) * settings_max_brightness();  // 1..256 (both 1..16)
  uint32_t sum = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    uint32_t r = scale8((col[i] >> 16) & 0xFF, gain);
    uint32_t g = scale8((col[i] >> 8) & 0xFF, gain);
    uint32_t b = scale8(col[i] & 0xFF, gain);
    col[i] = (r << 16) | (g << 8) | b;
    sum += r + g + b;
  }
  uint32_t budget = MAX_LED_CURRENT_MA - NUM_LEDS * LED_MA_IDLE;
  uint32_t estimate = sum * LED_MA_PER_CHANNEL / 255;
  uint32_t limit = 256;
  if (estimate > budget) limit = budget * 256 / estimate;
  for (int y = 0; y < NUM_ROWS; y++) {
    for (int x = 0; x < NUM_COLS; x++) {
      uint32_t c = col[y * NUM_COLS + x];
      uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
      if (limit < 256) {
        r = (r * limit) >> 8;
        g = (g * limit) >> 8;
        b = (b * limit) >> 8;
      }
      grb[hw_led_index(x, y)] = (g << 16) | (r << 8) | b;
    }
  }
  ws2812_show(grb);
}

static void render_frame(void) {
  static uint32_t col[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    if (override_rgb[i]) {
      col[i] = (scale8((rgb[i] >> 16) & 0xFF, px_gain + 1) << 16) |
               (scale8((rgb[i] >> 8) & 0xFF, px_gain + 1) << 8) |
               scale8(rgb[i] & 0xFF, px_gain + 1);
    } else {
      uint32_t v = (uint32_t)gamma_table[level[i]] * 2;  // 0..254
      col[i] = (scale8(tint_r, v + 1) << 16) | (scale8(tint_g, v + 1) << 8) | scale8(tint_b, v + 1);
      if (v == 0) col[i] = 0;
    }
  }
  send(col);
}

// brightness setting view: selected column full, columns left of it dim
static void render_overlay(void) {
  static uint32_t col[NUM_LEDS];
  int sel = settings_max_brightness() - 1;  // 0..15
  for (int y = 0; y < NUM_ROWS; y++) {
    for (int x = 0; x < NUM_COLS; x++) {
      uint32_t v = x == sel ? 255 : (x < sel ? 12 : 0);
      col[y * NUM_COLS + x] =
          v ? (scale8(def_r, v + 1) << 16) | (scale8(def_g, v + 1) << 8) | scale8(def_b, v + 1) : 0;
    }
  }
  uint8_t keep = intensity;
  intensity = 15;  // show the real maximum
  send(col);
  intensity = keep;
}

void leds_task(void) {
  if (!ws2812_ready()) return;
  if (settings_ui_active()) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_overlay >= 30) {
      last_overlay = now;
      render_overlay();
    }
    dirty = true;  // redraw the normal frame when the setting ends
    return;
  }
  if (dirty) {
    dirty = false;
    render_frame();
  }
}
