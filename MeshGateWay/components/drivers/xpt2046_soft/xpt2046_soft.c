#include "xpt2046_soft.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soft_spi_master.h"

#define TOUCH_IRQ_GPIO 36
#define TOUCH_MISO_GPIO 39
#define TOUCH_MOSI_GPIO 32
#define TOUCH_CS_GPIO 13
#define TOUCH_SCK_GPIO 15
#define TOUCH_SOFT_SPI_CLOCK_HZ 500000

#define XPT2046_CMD_X_READ 0x90
#define XPT2046_CMD_Y_READ 0xD0
#define XPT2046_CMD_Z1_READ 0xB0
#define XPT2046_CMD_Z2_READ 0xC0
#define XPT2046_PRESS_THRESHOLD 300
#define XPT2046_Z1_MIN 20
#define XPT2046_Z2_MIN 20

/* Values are calibrated from 4-corner raw logs. Swap is applied before mapping. */
#define XPT2046_RAW_X_MIN 300
#define XPT2046_RAW_X_MAX 1800
#define XPT2046_RAW_Y_MIN 170
#define XPT2046_RAW_Y_MAX 1850
#define XPT2046_SWAP_XY 1
#define XPT2046_INVERT_X 1
#define XPT2046_INVERT_Y 0

static const char *TAG = "xpt2046_soft";

static soft_spi_device_handle_t s_touch_spi = NULL;
static lv_indev_t *s_indev = NULL;
static lv_point_t s_last_point = {0, 0};
static xpt2046_soft_sample_t s_last_sample = {0};

static esp_err_t xpt2046_read_word(uint8_t cmd, uint16_t *value) {
  if (s_touch_spi == NULL || value == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t tx[3] = {cmd, 0x00, 0x00};
  uint8_t rx[3] = {0};
  soft_spi_transaction_t trans = {
      .length = 24,
      .rxlength = 24,
      .tx_buffer = tx,
      .rx_buffer = rx,
  };

  esp_err_t ret = soft_spi_device_polling_transmit(s_touch_spi, &trans);
  if (ret == ESP_OK) {
    *value = ((uint16_t)rx[1] << 8) | rx[2];
  }
  return ret;
}

static lv_coord_t map_axis(uint16_t raw, uint16_t raw_min, uint16_t raw_max,
                           lv_coord_t out_max, bool invert) {
  if (raw_max <= raw_min) {
    return 0;
  }

  int32_t value = raw;
  if (value < raw_min) {
    value = raw_min;
  } else if (value > raw_max) {
    value = raw_max;
  }

  int32_t mapped =
      ((value - raw_min) * (int32_t)(out_max - 1)) / (raw_max - raw_min);
  if (invert) {
    mapped = (out_max - 1) - mapped;
  }
  return (lv_coord_t)mapped;
}

static bool read_raw(uint16_t *raw_x, uint16_t *raw_y, uint16_t *z1_out,
                     uint16_t *z2_out, int32_t *pressure_out) {
  if (gpio_get_level(TOUCH_IRQ_GPIO) != 0) {
    return false;
  }

  uint16_t x_word = 0;
  uint16_t y_word = 0;
  uint16_t z1_word = 0;
  uint16_t z2_word = 0;

  esp_err_t ret = xpt2046_read_word(XPT2046_CMD_X_READ, &x_word);
  ret |= xpt2046_read_word(XPT2046_CMD_Y_READ, &y_word);
  ret |= xpt2046_read_word(XPT2046_CMD_Z1_READ, &z1_word);
  ret |= xpt2046_read_word(XPT2046_CMD_Z2_READ, &z2_word);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(ret));
    return false;
  }

  uint16_t x = x_word >> 4;
  uint16_t y = y_word >> 4;
  uint16_t z1 = z1_word >> 3;
  uint16_t z2 = z2_word >> 3;
  int32_t pressure = (int32_t)z1 + 4096 - (int32_t)z2;

  if (z1 < XPT2046_Z1_MIN || z2 < XPT2046_Z2_MIN) {
    return false;
  }

  if (pressure < XPT2046_PRESS_THRESHOLD) {
    return false;
  }

  if (raw_x) {
    *raw_x = x;
  }
  if (raw_y) {
    *raw_y = y;
  }
  if (z1_out) {
    *z1_out = z1;
  }
  if (z2_out) {
    *z2_out = z2;
  }
  if (pressure_out) {
    *pressure_out = pressure;
  }
  return true;
}

esp_err_t xpt2046_soft_init(void) {
  if (s_touch_spi != NULL) {
    return ESP_OK;
  }

  gpio_config_t irq_config = {
      .pin_bit_mask = 1ULL << TOUCH_IRQ_GPIO,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&irq_config), TAG, "configure IRQ failed");

  soft_spi_bus_config_t buscfg = {
      .mosi_io_num = TOUCH_MOSI_GPIO,
      .miso_io_num = TOUCH_MISO_GPIO,
      .sclk_io_num = TOUCH_SCK_GPIO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 3,
  };

  esp_err_t ret = soft_spi_bus_initialize(SOFT_SPI2_HOST, &buscfg, 0);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "soft_spi_bus_initialize failed: %s", esp_err_to_name(ret));
    return ret;
  }

  soft_spi_device_interface_config_t devcfg = {
      .mode = 0,
      .clock_speed_hz = TOUCH_SOFT_SPI_CLOCK_HZ,
      .spics_io_num = TOUCH_CS_GPIO,
      .queue_size = 1,
  };

  ret = soft_spi_bus_add_device(SOFT_SPI2_HOST, &devcfg, &s_touch_spi);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "soft_spi_bus_add_device failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "initialized: SCK=%d MISO=%d MOSI=%d CS=%d IRQ=%d",
           TOUCH_SCK_GPIO, TOUCH_MISO_GPIO, TOUCH_MOSI_GPIO, TOUCH_CS_GPIO,
           TOUCH_IRQ_GPIO);
  return ESP_OK;
}

void xpt2046_soft_indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;

  uint16_t raw_x = 0;
  uint16_t raw_y = 0;
  uint16_t z1 = 0;
  uint16_t z2 = 0;
  int32_t pressure = 0;

  if (read_raw(&raw_x, &raw_y, &z1, &z2, &pressure)) {
#if XPT2046_SWAP_XY
    uint16_t tmp = raw_x;
    raw_x = raw_y;
    raw_y = tmp;
#endif

    lv_coord_t x = map_axis(raw_x, XPT2046_RAW_X_MIN, XPT2046_RAW_X_MAX,
                            LV_HOR_RES, XPT2046_INVERT_X);
    lv_coord_t y = map_axis(raw_y, XPT2046_RAW_Y_MIN, XPT2046_RAW_Y_MAX,
                            LV_VER_RES, XPT2046_INVERT_Y);

    s_last_point.x = x;
    s_last_point.y = y;
    s_last_sample = (xpt2046_soft_sample_t){
        .raw_x = raw_x,
        .raw_y = raw_y,
        .z1 = z1,
        .z2 = z2,
        .pressure = pressure,
        .x = x,
        .y = y,
        .pressed = true,
    };
    data->point = s_last_point;
    data->state = LV_INDEV_STATE_PR;

    static TickType_t last_log = 0;
    TickType_t now = xTaskGetTickCount();
    if ((now - last_log) >= pdMS_TO_TICKS(250)) {
      last_log = now;
      ESP_LOGI(TAG, "raw=(%u,%u) z1=%u z2=%u p=%ld lv=(%d,%d)",
               (unsigned)raw_x, (unsigned)raw_y, (unsigned)z1, (unsigned)z2,
               (long)pressure, (int)x, (int)y);
    }
  } else {
    s_last_sample.pressed = false;
    data->point = s_last_point;
    data->state = LV_INDEV_STATE_REL;
  }
}

esp_err_t xpt2046_soft_register_lvgl_indev(void) {
  esp_err_t ret = xpt2046_soft_init();
  if (ret != ESP_OK) {
    return ret;
  }

  if (s_indev != NULL) {
    return ESP_OK;
  }

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = xpt2046_soft_indev_read;
  s_indev = lv_indev_drv_register(&indev_drv);
  return s_indev != NULL ? ESP_OK : ESP_FAIL;
}

bool xpt2046_soft_get_last_sample(xpt2046_soft_sample_t *sample) {
  if (sample == NULL) {
    return false;
  }

  *sample = s_last_sample;
  return true;
}
