#include "hw.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

static const uint8_t row_pins[NUM_ROWS] = ROW_PIN_LIST;
static const uint8_t col_pins[NUM_COLS] = COL_PIN_LIST;
static uint32_t row_mask;
static bool state[NUM_ROWS][NUM_COLS];
static int rotation;

void hw_init(void) {
  row_mask = 0;
  for (int r = 0; r < NUM_ROWS; r++) {
    gpio_init(row_pins[r]);
    gpio_set_dir(row_pins[r], GPIO_IN);
    gpio_pull_up(row_pins[r]);
    row_mask |= 1u << row_pins[r];
  }
  for (int c = 0; c < NUM_COLS; c++) {
    gpio_init(col_pins[c]);
    gpio_set_dir(col_pins[c], GPIO_OUT);
    gpio_put(col_pins[c], 1);
  }
  sleep_ms(1);
}

// drives one column low and returns the pressed rows as a bitmask
static uint8_t read_column(int c) {
  gpio_put(col_pins[c], 0);
  busy_wait_us_32(COL_SETTLE_US);
  uint32_t in = ~gpio_get_all() & row_mask;
  gpio_put(col_pins[c], 1);
  busy_wait_us_32(COL_RELEASE_US);
  uint8_t bits = 0;
  for (int r = 0; r < NUM_ROWS; r++)
    if (in & (1u << row_pins[r])) bits |= (uint8_t)(1u << r);
  return bits;
}

void hw_scan(hw_key_cb cb) {
  for (int c = 0; c < NUM_COLS; c++) {
    uint8_t bits = read_column(c);
    for (int r = 0; r < NUM_ROWS; r++) {
      bool z = bits & (1u << r);
      if (z != state[r][c]) {
        state[r][c] = z;
        if (cb) cb(r, c, z);
      }
    }
  }
}

bool hw_pressed(int row, int col) { return state[row][col]; }

uint32_t hw_boot_keys(void) {
  hw_scan(NULL);
  sleep_ms(20);
  hw_scan(NULL);
  uint32_t k = 0;
  if (state[0][0]) k |= BOOT_KEY_TOP_LEFT;
  if (state[NUM_ROWS - 1][NUM_COLS - 1]) k |= BOOT_KEY_BOTTOM_RIGHT;
  return k;
}

void hw_set_rotation(int r) { rotation = r; }

void hw_map(int a_row, int a_col, int *b_row, int *b_col) {
  if (rotation == 2) {
    *b_row = NUM_ROWS - 1 - a_row;
    *b_col = NUM_COLS - 1 - a_col;
  } else {
    *b_row = a_row;
    *b_col = a_col;
  }
}

int hw_led_index(int x, int y) {
  int r, c;
  hw_map(y, x, &r, &c);
  return r * NUM_COLS + c;  // chain runs row by row, 16 per row
}
