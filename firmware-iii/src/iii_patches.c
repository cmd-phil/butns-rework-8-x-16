// Workarounds for two problems in iii v1.1.2.
//
// 1. serial() (--wrap=serial)
//    iii's serial() appends to a 16 KB buffer without checking the end of the
//    buffer. Nothing drains the buffer while no host has the USB serial port
//    open, so a script that keeps raising errors (each error message is sent
//    with serial()) overruns it within seconds and overwrites other RAM.
//    This version hands short messages to serial_tx(), which drops what does
//    not fit and everything while no host is connected. Long messages, such
//    as a file printed with ^^p for "diii download", are written directly
//    to USB while a host is connected.
//
// 2. lib.lua: _quote() drops trailing zeros of integers
//    pset_write() stores numbers with
//        string.format('%.6g',val):gsub("%.?0+$","")
//    which turns 0 into "", 10 into "1" and 120 into "12". Presets written
//    this way lose values or cannot be read back (stepiii: bpm 120 is stored
//    as 12, step value 0 gives a syntax error). iii_fix_lib() replaces the
//    expression in lib.lua on flash, the rest of the file stays as it is.
//    Preset files written before the fix stay broken.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "iii.h"
#include "pico/stdlib.h"
#include "resource/lib_lua.h"
#include "serial.h"
#include "tusb.h"

// --- serial() -----------------------------------------------------------------

#define SERIAL_DIRECT_MIN 2048  // longer messages go directly to USB

static void write_direct(const char *d, uint32_t n) {
  if (!tud_cdc_n_connected(0)) return;
  absolute_time_t give_up = make_timeout_time_ms(1000);
  while (n > 0 && !time_reached(give_up)) {
    uint32_t w = tud_cdc_n_write(0, d, n);
    if (w > 0) give_up = make_timeout_time_ms(1000);
    d += w;
    n -= w;
    tud_cdc_n_write_flush(0);
    tud_task();
  }
}

int __wrap_serial(const char *fmt, ...) {
  if (fmt == NULL) return 0;
  char small[256];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(small, sizeof(small), fmt, ap);
  va_end(ap);
  if (n <= 0) return 0;
  if ((size_t)n < sizeof(small)) {
    serial_tx((const uint8_t *)small, (uint32_t)n);
    return 0;
  }
  char *big = malloc((size_t)n + 1);
  if (big == NULL) return 0;
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  if (n >= SERIAL_DIRECT_MIN)
    write_direct(big, (uint32_t)n);
  else
    serial_tx((const uint8_t *)big, (uint32_t)n);
  free(big);
  return 0;
}

// --- lib.lua ------------------------------------------------------------------

static const char lib_bug[] = "string.format('%.6g',val):gsub(\"%.?0+$\",\"\")";
static const char lib_fix[] =
    "(math.type(val)=='integer' and string.format('%d',val) or string.format('%.6g',val))";

static char *read_file(const char *path, uint32_t *size) {
  lfs_file_t f;
  if (fs_file_open(&f, path, LFS_O_RDONLY) != 0) return NULL;
  int32_t sz = fs_file_size(&f);
  char *buf = sz >= 0 ? malloc((size_t)sz + 1) : NULL;
  if (buf != NULL) {
    int32_t got = fs_file_read(&f, buf, (uint32_t)sz);
    buf[got > 0 ? got : 0] = 0;
    *size = got > 0 ? (uint32_t)got : 0;
  }
  fs_file_close(&f);
  return buf;
}

static void write_file(const char *path, const char *data, uint32_t size) {
  lfs_file_t f;
  fs_remove(path);
  if (fs_file_open(&f, path, LFS_O_CREAT | LFS_O_WRONLY) != 0) return;
  fs_file_write(&f, data, size);
  fs_file_close(&f);
}

// call before iii's vm_init(). If lib.lua is missing or outdated, iii would
// write its built-in copy, so the fix is applied to that copy instead.
void iii_fix_lib(void) {
  uint32_t size = 0;
  char *lib = read_file("lib.lua", &size);
  if (lib == NULL || strstr(lib, III_VERSION) == NULL) {
    free(lib);
    size = (uint32_t)strlen((const char *)lib_lua_data);
    lib = malloc(size + 1);
    if (lib == NULL) return;
    memcpy(lib, lib_lua_data, size + 1);
  }
  char *at = strstr(lib, lib_bug);
  if (at != NULL) {
    size_t head = (size_t)(at - lib), tail = size - head - (sizeof(lib_bug) - 1);
    uint32_t nsize = (uint32_t)(head + (sizeof(lib_fix) - 1) + tail);
    char *fixed = malloc(nsize + 1);
    if (fixed != NULL) {
      memcpy(fixed, lib, head);
      memcpy(fixed + head, lib_fix, sizeof(lib_fix) - 1);
      memcpy(fixed + head + sizeof(lib_fix) - 1, at + sizeof(lib_bug) - 1, tail + 1);
      write_file("lib.lua", fixed, nsize);
      serial("-- init: fixed number format in lib.lua\r\n");
      free(fixed);
    }
  }
  free(lib);
}
