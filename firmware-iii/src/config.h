#pragma once

// butns 16x8 hardware
#define NUM_ROWS 8
#define NUM_COLS 16
#define NUM_LEDS (NUM_ROWS * NUM_COLS)

#define LED_PIN 28

// GPIOs of SW_ROW_1..8 and SW_COL_1..16 (physical order, left to right)
#define ROW_PIN_LIST {0, 1, 2, 3, 4, 5, 6, 7}
#define COL_PIN_LIST {8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 26, 15}

// matrix timing
#define SCAN_INTERVAL_US 10000   // one full scan every 10 ms (also debounces)
#define COL_SETTLE_US 5          // after driving a column low
#define COL_RELEASE_US 20        // row pull-ups recharge before the next column

// LED power budget. SK6812-EC20: 12 mA per colour channel at full scale,
// about 1 mA idle per LED. Frames above the budget are scaled down as a whole.
#define LED_MA_PER_CHANNEL 12
#define LED_MA_IDLE 1
#define MAX_LED_CURRENT_MA 400

// default tint for brightness levels (grid_led / monome apps)
#define TINT_III_R 255
#define TINT_III_G 255
#define TINT_III_B 255
#define TINT_MONOME_R 128
#define TINT_MONOME_G 255
#define TINT_MONOME_B 128

// brightness setting: hold both top corner keys, then pick a column
#define SETTINGS_HOLD_MS 1000
#define SETTINGS_TIMEOUT_MS 3000

#define DEVICE_VERSION "butns16x8-iii-1.0"

// 0 = iii (Lua), 1 = monome serial protocol; stored by iii's flash_write_mode()
#define MODE_III 0
#define MODE_MONOME 1
extern unsigned char g_mode;
