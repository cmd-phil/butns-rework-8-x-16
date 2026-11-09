#pragma once

#include <stdbool.h>
#include <stdint.h>

void ws2812_init(unsigned pin, int count);
// true when the previous frame has been sent and latched
bool ws2812_ready(void);
// grb[i] = 0x00GGRRBB; copied, then sent by DMA in the background
void ws2812_show(const uint32_t *grb);
