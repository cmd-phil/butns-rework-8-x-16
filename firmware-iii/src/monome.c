// monome serial protocol (grid) over USB CDC, for serialosc / norns.
// Command set and byte layout as in the butns Arduino firmware
// (MonomeSerialDevice), with a byte-stream parser.

#include <string.h>

#include "config.h"
#include "leds.h"
#include "settings.h"
#include "tusb.h"

static char device_name[32] = "btns";  // as in the Arduino firmware
static uint8_t cmd;
static uint8_t payload[40];
static int need;  // payload bytes still missing, -1 = waiting for a command byte
static int have;

static void tx(const uint8_t *b, uint32_t n) {
  tud_cdc_write(b, n);  // dropped if the host does not read
}

static void key(int x, int y, int z) {
  uint8_t b[3] = {(uint8_t)(z ? 0x21 : 0x20), (uint8_t)x, (uint8_t)y};
  tx(b, 3);
}

static int payload_size(uint8_t c) {
  switch (c) {
    case 0x02: return 32;  // write id
    case 0x04: return 3;   // report addr
    case 0x06: return 2;   // set size (ignored)
    case 0x08: return 2;   // i2c set addr (ignored)
    case 0x10: case 0x11: return 2;  // led off / on
    case 0x14: return 10;  // map x y d[8]
    case 0x15: case 0x16: return 3;  // row / col
    case 0x17: return 1;   // intensity
    case 0x18: return 3;   // level set
    case 0x19: return 1;   // level all
    case 0x1A: return 34;  // level map x y d[32]
    case 0x1B: case 0x1C: return 6;  // level row / col
    case 0x80: return 2;   // tilt set active
    case 0x81: case 0x82: return 1;
    default: return 0;
  }
}

// x/y offsets for 8x8 blocks: floor to a multiple of 8, negative = outside
static int offset(uint8_t v) {
  int o = (int8_t)v;
  if (o < 0) return -100;
  return o & ~7;
}

static void on_off(int x, int y, int on) { leds_set(x, y, on ? 15 : 0, false); }

static void handle(void) {
  const uint8_t *p = payload;
  switch (cmd) {
    case 0x00: {  // query
      uint8_t b[6] = {0x00, 0x01, NUM_COLS / 8, 0x00, 0x02, NUM_COLS / 8};
      tx(b, 6);
      break;
    }
    case 0x01: {  // id
      uint8_t b[33];
      b[0] = 0x01;
      memcpy(b + 1, device_name, 32);
      tx(b, 33);
      break;
    }
    case 0x02:
      memcpy(device_name, p, 32);
      break;
    case 0x03: {  // grid offset
      uint8_t b[4] = {0x02, 0x01, 0, 0};
      tx(b, 4);
      break;
    }
    case 0x05: {  // grid size
      uint8_t b[3] = {0x03, NUM_COLS, NUM_ROWS};
      tx(b, 3);
      break;
    }
    case 0x10: on_off(p[0], p[1], 0); break;
    case 0x11: on_off(p[0], p[1], 1); break;
    case 0x12: leds_all(0, false); break;
    case 0x13: leds_all(15, false); break;
    case 0x14: {  // map: 8 rows, one bit per led
      int ox = offset(p[0]), oy = offset(p[1]);
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) on_off(ox + x, oy + y, (p[2 + y] >> x) & 1);
      break;
    }
    case 0x15: {  // row: x offset, y, 8 bits
      int ox = offset(p[0]);
      for (int x = 0; x < 8; x++) on_off(ox + x, p[1], (p[2] >> x) & 1);
      break;
    }
    case 0x16: {  // col: x, y offset, 8 bits
      int oy = offset(p[1]);
      for (int y = 0; y < 8; y++) on_off(p[0], oy + y, (p[2] >> y) & 1);
      break;
    }
    case 0x17: leds_intensity(p[0]); break;
    case 0x18: leds_set(p[0], p[1], p[2] & 0x0F, false); break;
    case 0x19: leds_all(p[0] & 0x0F, false); break;
    case 0x1A: {  // level map: 64 levels, 4 bit each, upper nibble first
      int ox = offset(p[0]), oy = offset(p[1]);
      for (int i = 0; i < 64; i++) {
        uint8_t b = p[2 + i / 2];
        leds_set(ox + i % 8, oy + i / 8, (i & 1) ? (b & 0x0F) : (b >> 4), false);
      }
      break;
    }
    case 0x1B: {  // level row: x offset, y, 8 levels
      int ox = offset(p[0]);
      for (int i = 0; i < 8; i++) {
        uint8_t b = p[2 + i / 2];
        leds_set(ox + i, p[1], (i & 1) ? (b & 0x0F) : (b >> 4), false);
      }
      break;
    }
    case 0x1C: {  // level col: x, y offset, 8 levels
      int oy = offset(p[1]);
      for (int i = 0; i < 8; i++) {
        uint8_t b = p[2 + i / 2];
        leds_set(p[0], oy + i, (i & 1) ? (b & 0x0F) : (b >> 4), false);
      }
      break;
    }
    default:
      break;
  }
  if (cmd >= 0x10 && cmd <= 0x1C) leds_mark_dirty();
}

static void rx(void) {
  while (tud_cdc_available()) {
    uint8_t buf[64];
    uint32_t n = tud_cdc_read(buf, sizeof(buf));
    for (uint32_t i = 0; i < n; i++) {
      if (need < 0) {
        cmd = buf[i];
        need = payload_size(cmd);
        have = 0;
      } else {
        payload[have++] = buf[i];
        need--;
      }
      if (need == 0) {
        handle();
        need = -1;
      }
    }
  }
}

void monome_loop(void) {
  need = -1;
  leds_set_default_tint(TINT_MONOME_R, TINT_MONOME_G, TINT_MONOME_B);
  leds_reset(true);
  ui_init(key);
  while (1) {
    tud_task();
    rx();
    ui_task();
    tud_cdc_write_flush();
  }
}
