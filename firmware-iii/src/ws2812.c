#include "ws2812.h"

#include <string.h>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define MAX_PIXELS 128
// 24 bits * 1.25 us per pixel, plus > 80 us low for the latch
#define US_PER_PIXEL 30
#define LATCH_US 300

static PIO pio = pio0;
static uint sm;
static int dma_chan;
static int n_pixels;
static uint32_t buf[MAX_PIXELS];
static uint32_t started_at;
static bool started;

void ws2812_init(unsigned pin, int count) {
  n_pixels = count > MAX_PIXELS ? MAX_PIXELS : count;
  uint offset = pio_add_program(pio, &ws2812_program);
  sm = pio_claim_unused_sm(pio, true);
  ws2812_program_init(pio, sm, offset, pin, 800000.0f);

  dma_chan = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
  dma_channel_configure(dma_chan, &c, &pio->txf[sm], buf, n_pixels, false);
}

bool ws2812_ready(void) {
  if (!started) return true;
  if (dma_channel_is_busy(dma_chan)) return false;
  return (time_us_32() - started_at) >= (uint32_t)(n_pixels * US_PER_PIXEL + LATCH_US);
}

void ws2812_show(const uint32_t *grb) {
  while (!ws2812_ready()) tight_loop_contents();
  for (int i = 0; i < n_pixels; i++) buf[i] = grb[i] << 8;  // 24 bits, MSB first
  started_at = time_us_32();
  started = true;
  dma_channel_transfer_from_buffer_now(dma_chan, buf, n_pixels);
}
