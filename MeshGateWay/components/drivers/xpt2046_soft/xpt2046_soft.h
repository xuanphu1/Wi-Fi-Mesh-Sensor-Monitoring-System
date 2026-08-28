#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t z1;
  uint16_t z2;
  int32_t pressure;
  lv_coord_t x;
  lv_coord_t y;
  bool pressed;
} xpt2046_soft_sample_t;

esp_err_t xpt2046_soft_init(void);
esp_err_t xpt2046_soft_register_lvgl_indev(void);
void xpt2046_soft_indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
bool xpt2046_soft_get_last_sample(xpt2046_soft_sample_t *sample);

#ifdef __cplusplus
}
#endif
