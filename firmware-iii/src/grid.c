// iii device layer for butns 16x8: grid Lua API with colour extensions.
//
// Lua API (coordinates 1-based, levels 0..15):
//   event_grid(x, y, z)          called on key press (z=1) and release (z=0)
//   grid_led(x, y, z [, rel])    set level; rel adds z to the current level
//   grid_led_get(x, y)
//   grid_led_all(z [, rel])
//   grid_refresh()               show the changes
//   grid_size_x(), grid_size_y()
//   grid_intensity(z)            overall brightness 0..15
// colour extensions, as in the NeoTrellis iii firmware and the diii emulator:
//   grid_color(r, g, b)          tint for grid_led / grid_led_all (default white)
//   grid_led_rgb(x, y, r, g, b)  colour of one key, ignores the tint
//   grid_color_intensity(z)      brightness 0..15 of grid_led_rgb pixels

#include <stdbool.h>

#include "config.h"
#include "device.h"
#include "lauxlib.h"
#include "leds.h"
#include "lua.h"
#include "settings.h"
#include "vm.h"

static bool key_held_at_boot;

void grid_set_boot_key(bool held) { key_held_at_boot = held; }

static void vm_handle_grid(int x, int y, int z) {
  if (L == NULL) return;
  lua_getglobal(L, "event_grid");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return;
  }
  lua_pushinteger(L, x + 1);
  lua_pushinteger(L, y + 1);
  lua_pushinteger(L, z);
  l_report(L, docall(L, 3, 0));
}

// --- device.h ---------------------------------------------------------------

bool check_device_key() { return key_held_at_boot; }

void device_init() { ui_init(vm_handle_grid); }

void device_task() { ui_task(); }

void device_handle_serial(uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
}

const char *device_str1() { return "monome"; }
const char *device_str2() { return g_mode == MODE_III ? "iii grid" : "monome"; }
const char *device_id() { return "butns16x8"; }
const char *device_version() { return DEVICE_VERSION; }

static const char *help_txt =
    "grid\n"
    "\tevent_grid(x,y,z)\n"
    "\tgrid_led(x,y,z,rel)\n"
    "\tgrid_led_get(x,y)\n"
    "\tgrid_led_all(z,rel)\n"
    "\tgrid_refresh()\n"
    "\tgrid_size_x()\n"
    "\tgrid_size_y()\n"
    "\tgrid_intensity(z)\n"
    "\tgrid_color(r,g,b)\n"
    "\tgrid_led_rgb(x,y,r,g,b)\n"
    "\tgrid_color_intensity(z)\n";

const char *device_help_txt() { return help_txt; }

// reset the LED state whenever iii starts a new VM (boot, upload, reset)
extern void __real_vm_init(bool run_script);
void __wrap_vm_init(bool run_script) {
  leds_reset(true);
  __real_vm_init(run_script);
}

// --- Lua bindings -----------------------------------------------------------

// numbers are accepted like in Lua arithmetic: 3, 3.0 and 3.7 all give 3
static int argi(lua_State *l, int i) { return (int)luaL_checknumber(l, i); }

static int l_grid_led(lua_State *l) {
  leds_set(argi(l, 1) - 1, argi(l, 2) - 1,
           argi(l, 3), lua_toboolean(l, 4));
  return 0;
}

static int l_grid_led_get(lua_State *l) {
  lua_pushinteger(l, leds_get(argi(l, 1) - 1, argi(l, 2) - 1));
  return 1;
}

static int l_grid_led_all(lua_State *l) {
  leds_all(argi(l, 1), lua_toboolean(l, 2));
  return 0;
}

static int l_grid_refresh(lua_State *l) {
  (void)l;
  leds_mark_dirty();
  return 0;
}

static int l_grid_size_x(lua_State *l) {
  lua_pushinteger(l, NUM_COLS);
  return 1;
}

static int l_grid_size_y(lua_State *l) {
  lua_pushinteger(l, NUM_ROWS);
  return 1;
}

static int l_grid_intensity(lua_State *l) {
  leds_intensity(argi(l, 1));
  leds_mark_dirty();
  return 0;
}

static int l_grid_color(lua_State *l) {
  leds_tint(argi(l, 1), argi(l, 2), argi(l, 3));
  return 0;
}

static int l_grid_led_rgb(lua_State *l) {
  leds_rgb(argi(l, 1) - 1, argi(l, 2) - 1,
           argi(l, 3), argi(l, 4), argi(l, 5));
  return 0;
}

static int l_grid_color_intensity(lua_State *l) {
  leds_color_intensity(argi(l, 1));
  return 0;
}

static const struct luaL_Reg device_lib[] = {
    {"grid_led", l_grid_led},
    {"grid_led_get", l_grid_led_get},
    {"grid_led_all", l_grid_led_all},
    {"grid_refresh", l_grid_refresh},
    {"grid_size_x", l_grid_size_x},
    {"grid_size_y", l_grid_size_y},
    {"grid_intensity", l_grid_intensity},
    {"grid_color", l_grid_color},
    {"grid_led_rgb", l_grid_led_rgb},
    {"grid_color_intensity", l_grid_color_intensity},
    {NULL, NULL},
};

const struct luaL_Reg *get_device_lib(void) { return device_lib; }
