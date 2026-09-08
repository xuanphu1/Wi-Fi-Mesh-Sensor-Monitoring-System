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

#define TOUCH_HOR_RES 240
#define TOUCH_VER_RES 320

static const char *TAG = "xpt2046_soft";

static soft_spi_device_handle_t s_touch_spi = NULL;
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

static int16_t map_axis(uint16_t raw, uint16_t raw_min, uint16_t raw_max,
                        int16_t out_max, bool invert) {
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
  return (int16_t)mapped;
}

static bool read_raw(uint16_t *raw_x, uint16_t *raw_y, uint16_t *z1_out,
                     uint16_t *z2_out, int32_t *pressure_out) {
  if (gpio_get_level(TOUCH_IRQ_GPIO) != 0) {
    return false;
  }

  uint16_t dummy = 0;
  uint16_t w_90 = 0;
  uint16_t w_d0 = 0;
  uint16_t w_94 = 0;
  uint16_t w_d4 = 0;

  // Dummy read nạp tụ và chuyển kênh
  xpt2046_read_word(XPT2046_CMD_X_READ, &dummy);

  // Đọc X và Y ở cả 2 chế độ vi sai (0x90, 0xD0) và đơn cực (0x94, 0xD4)
  xpt2046_read_word(0x90, &w_90);
  xpt2046_read_word(0xD0, &w_d0);
  xpt2046_read_word(0x94, &w_94);
  xpt2046_read_word(0xD4, &w_d4);

  // Gửi lệnh 0x00 để bật lại ngắt PENIRQ cho lần chạm sau
  xpt2046_read_word(0x00, &dummy);

  uint16_t v_90 = w_90 >> 4;
  uint16_t v_d0 = w_d0 >> 4;
  uint16_t v_94 = w_94 >> 4;
  uint16_t v_d4 = w_d4 >> 4;

  // Chọn kênh có tín hiệu hợp lệ (> 20)
  uint16_t x = (v_90 > 20) ? v_90 : v_94;
  uint16_t y = (v_d0 > 20) ? v_d0 : v_d4;

  static TickType_t s_last_raw_log = 0;
  TickType_t now_tick = xTaskGetTickCount();
  if ((now_tick - s_last_raw_log) >= pdMS_TO_TICKS(150)) {
    s_last_raw_log = now_tick;
    ESP_LOGI(TAG, "RAW CHANNELS: [0x90]=%u, [0xD0]=%u, [0x94]=%u, [0xD4]=%u => x=%u, y=%u",
             v_90, v_d0, v_94, v_d4, x, y);
  }

  if (raw_x) *raw_x = x;
  if (raw_y) *raw_y = y;
  if (z1_out) *z1_out = 100;
  if (z2_out) *z2_out = 50;
  if (pressure_out) *pressure_out = 1000;
  return true;
}

esp_err_t xpt2046_soft_init(void) {
  if (s_touch_spi != NULL) {
    return ESP_OK;
  }

  gpio_config_t irq_config = {
      .pin_bit_mask = (1ULL << TOUCH_IRQ_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&irq_config), TAG, "irq gpio_config failed");

  soft_spi_bus_config_t bus_config = {
      .mosi_io_num = TOUCH_MOSI_GPIO,
      .miso_io_num = TOUCH_MISO_GPIO,
      .sclk_io_num = TOUCH_SCK_GPIO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };
  esp_err_t err = soft_spi_bus_initialize(SOFT_SPI2_HOST, &bus_config, 0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "soft_spi_bus_initialize failed: %s", esp_err_to_name(err));
    return err;
  }

  soft_spi_device_interface_config_t dev_config = {
      .clock_speed_hz = TOUCH_SOFT_SPI_CLOCK_HZ,
      .mode = 0,
      .spics_io_num = TOUCH_CS_GPIO,
      .queue_size = 1,
  };
  ESP_RETURN_ON_ERROR(soft_spi_bus_add_device(SOFT_SPI2_HOST, &dev_config, &s_touch_spi), TAG,
                      "soft_spi_bus_add_device failed");

  ESP_LOGI(TAG, "xpt2046_soft initialized (X_RANGE=[%d..%d], Y_RANGE=[%d..%d], SWAP=%d, INV_X=%d, INV_Y=%d)",
           XPT2046_RAW_X_MIN, XPT2046_RAW_X_MAX, XPT2046_RAW_Y_MIN, XPT2046_RAW_Y_MAX,
           XPT2046_SWAP_XY, XPT2046_INVERT_X, XPT2046_INVERT_Y);
  return ESP_OK;
}

bool xpt2046_soft_poll(int16_t *out_x, int16_t *out_y) {
  uint16_t raw_x = 0;
  uint16_t raw_y = 0;
  uint16_t z1 = 0;
  uint16_t z2 = 0;
  int32_t pressure = 0;

  if (read_raw(&raw_x, &raw_y, &z1, &z2, &pressure)) {
#if XPT2046_SWAP_XY
    uint16_t swap = raw_x;
    raw_x = raw_y;
    raw_y = swap;
#endif

    int16_t x = map_axis(raw_x, XPT2046_RAW_X_MIN, XPT2046_RAW_X_MAX,
                         TOUCH_HOR_RES, XPT2046_INVERT_X);
    int16_t y = map_axis(raw_y, XPT2046_RAW_Y_MIN, XPT2046_RAW_Y_MAX,
                         TOUCH_VER_RES, XPT2046_INVERT_Y);

    static TickType_t s_last_poll_log = 0;
    TickType_t now_poll = xTaskGetTickCount();
    if ((now_poll - s_last_poll_log) >= pdMS_TO_TICKS(100)) {
      s_last_poll_log = now_poll;
      ESP_LOGI(TAG, "[TOUCH] Raw[X=%4u, Y=%4u] | Z1=%3u, Z2=%3u, Press=%4ld ==> Screen[X=%3d, Y=%3d]",
               (unsigned)raw_x, (unsigned)raw_y, (unsigned)z1, (unsigned)z2, (long)pressure, (int)x, (int)y);
    }

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
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    return true;
  }

  s_last_sample.pressed = false;
  return false;
}

bool xpt2046_soft_get_last_sample(xpt2046_soft_sample_t *sample) {
  if (sample == NULL) return false;
  *sample = s_last_sample;
  return true;
}
