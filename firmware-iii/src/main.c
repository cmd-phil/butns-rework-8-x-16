// butns 16x8 firmware with two modes, stored in flash by iii:
//   iii mode    Lua scripts via monome iii (diii, USB MIDI), colour extensions
//   monome mode monome serial protocol for serialosc / norns
// Hold the top-left key while plugging in to switch modes.
// Hold the bottom-right key while plugging in to rotate by 180 degrees.

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "device.h"
#include "flash.h"
#include "hw.h"
#include "iii.h"
#include "leds.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "settings.h"
#include "tusb.h"
#include "ws2812.h"

unsigned char g_mode;

extern void monome_loop(void);
extern void grid_set_boot_key(bool held);

// core 1 only has to pause while core 0 writes the flash (iii file system, settings)
static void core1_entry(void) {
  multicore_lockout_victim_init();
  while (1) tight_loop_contents();
}

// short colour flash on the top row: blue = iii, green = monome
static void show_mode(void) {
  for (int x = 0; x < NUM_COLS; x++) leds_rgb(x, 0, 0, g_mode == MODE_III ? 40 : 160, g_mode == MODE_III ? 255 : 40);
  leds_mark_dirty();
  for (int i = 0; i < 40; i++) {  // 400 ms
    leds_task();
    sleep_ms(10);
  }
  leds_reset(true);
  leds_mark_dirty();
  while (!ws2812_ready()) tight_loop_contents();
  leds_task();
}

int main(void) {
  flash_init();
  hw_init();
  settings_load();
  leds_set_default_tint(TINT_III_R, TINT_III_G, TINT_III_B);
  leds_init();

  uint32_t boot = hw_boot_keys();
  hw_set_rotation((boot & BOOT_KEY_BOTTOM_RIGHT) ? 2 : 0);
  grid_set_boot_key(boot & BOOT_KEY_TOP_LEFT);

  uint8_t m = flash_read_mode();
  if (m > MODE_MONOME) m = MODE_III;
  if (boot & BOOT_KEY_TOP_LEFT) {
    m = (m == MODE_III) ? MODE_MONOME : MODE_III;
    flash_write_mode(m);  // before core 1 starts, as required by iii
  }
  g_mode = m;
  show_mode();

  tud_init(BOARD_TUD_RHPORT);
  multicore_launch_core1(core1_entry);

  if (g_mode == MODE_III) {
    device_init();
    iii_loop(true);
  } else {
    monome_loop();
  }
  return 0;
}
