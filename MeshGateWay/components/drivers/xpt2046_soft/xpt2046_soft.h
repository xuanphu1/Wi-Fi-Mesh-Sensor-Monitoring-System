#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t z1;
  uint16_t z2;
  int32_t pressure;
  int16_t x;
  int16_t y;
  bool pressed;
} xpt2046_soft_sample_t;

esp_err_t xpt2046_soft_init(void);
bool xpt2046_soft_get_last_sample(xpt2046_soft_sample_t *sample);
bool xpt2046_soft_poll(int16_t *out_x, int16_t *out_y);

#ifdef __cplusplus
}
#endif
