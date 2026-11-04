// Lua memory management for the 264 KB RAM of the RP2040.
//
// iii v1.1.2 creates its Lua state with the default garbage collector, which
// starts a new cycle only when the heap has grown to 2.5 times the live data.
// A large script such as stepiii keeps about 130 KB live, so the heap grows
// past the 219 KB that are free; the failed realloc() then halts the firmware
// (pico-sdk panic) and the grid freezes. Measured with stepiii in a 32 bit
// host simulation: the heap passes 219 KB within about 20 s of playing.
//
// This file replaces the allocator and the GC settings of every Lua state
// (linker option --wrap=lua_newstate):
// - incremental GC with LUA_GC_PAUSE / LUA_GC_STEPMUL from config.h, which
//   keeps stepiii at about 160 KB
// - Lua may use at most the heap minus LUA_HEAP_RESERVE, so that iii's own
//   buffers (32 KB for an upload, the script file while loading) still fit
// - at the limit the allocator fails and Lua runs an emergency collection;
//   if that is not enough, the script gets a "not enough memory" error
//   instead of the device halting (PICO_MALLOC_PANIC=0 in CMakeLists.txt)
//
// It also lets an upload compile when a large script is running
// (--wrap=vm_test_script): iii compiles the received file inside the running
// Lua state, which needs about three times the file size on top of the script.
// If that runs out of memory, the running script is stopped first.

#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "hardware/watchdog.h"
#include "lauxlib.h"
#include "lua.h"
#include "serial.h"
#include "vm.h"

extern char __end__, __HeapLimit;  // heap bounds from the pico-sdk linker script

static size_t used;    // bytes of the Lua heap, including malloc overhead
static size_t budget;  // limit for 'used'

// newlib malloc: 4 byte header, 8 byte alignment, 16 bytes minimum
static size_t chunk(size_t n) {
  if (n == 0) return 0;
  size_t c = (n + 4 + 7) & ~(size_t)7;
  return c < 16 ? 16 : c;
}

static void *alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  if (ptr == NULL) osize = 0;  // osize then encodes the object type
  if (nsize == 0) {
    free(ptr);
    used -= chunk(osize);
    return NULL;
  }
  size_t after = used - chunk(osize) + chunk(nsize);
  if (nsize > osize && after > budget) return NULL;  // Lua collects and retries
  void *p = realloc(ptr, nsize);
  if (p == NULL) return nsize <= osize ? ptr : NULL;  // shrinking must not fail
  used = after;
  return p;
}

static int on_panic(lua_State *l) {
  // error outside any protected call: restart, as iii does when memory runs out
  const char *msg = lua_tostring(l, -1);
  serial("-- lua panic: %s\r\n", msg ? msg : "?");
  watchdog_reboot(0, 0, 100);
  while (1) tight_loop_contents();
  return 0;
}

lua_State *__real_lua_newstate(lua_Alloc f, void *ud, unsigned seed);

lua_State *__wrap_lua_newstate(lua_Alloc f, void *ud, unsigned seed) {
  (void)f;
  if (budget == 0) {
    size_t heap = (size_t)(&__HeapLimit - &__end__);
    budget = heap > 2 * LUA_HEAP_RESERVE ? heap - LUA_HEAP_RESERVE : heap / 2;
  }
  lua_State *l = __real_lua_newstate(alloc, ud, seed);
  if (l != NULL) {
    lua_atpanic(l, on_panic);
    lua_gc(l, LUA_GCINC);
    lua_gc(l, LUA_GCPARAM, LUA_GCPPAUSE, LUA_GC_PAUSE);
    lua_gc(l, LUA_GCPARAM, LUA_GCPSTEPMUL, LUA_GC_STEPMUL);
  }
  return l;
}

bool __real_vm_test_script(const char *buf);

bool __wrap_vm_test_script(const char *buf) {
  if (L != NULL) {
    lua_gc(L, LUA_GCCOLLECT);
    int res = luaL_loadstring(L, buf);
    lua_pop(L, 1);  // compiled chunk or error message
    lua_gc(L, LUA_GCCOLLECT);
    if (res == LUA_ERRMEM) {
      serial("-- not enough memory next to the running script, stopping it\r\n");
      vm_deinit();
      vm_init(false);
    }
  }
  // iii's own test: prints "compiled ok" or the syntax error
  return __real_vm_test_script(buf);
}
