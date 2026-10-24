#ifndef SCREEN_DRIVER_H
#define SCREEN_DRIVER_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t tft_hardware_init(void);

void tft_draw_bitmap_wait(int16_t x_start, int16_t y_start,
                          int16_t x_end, int16_t y_end,
                          const void *color_data);

#endif // SCREEN_DRIVER_H
