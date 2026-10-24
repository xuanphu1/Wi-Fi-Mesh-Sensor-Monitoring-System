#include "screen_driver.h"
#include "screen_internal.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

static const char *TAG = "ScreenManager";

// Cấu hình chân phần cứng kết nối màn hình ILI9341
#define LCD_HOST SPI3_HOST
#define LCD_PIN_NUM_SCLK 25
#define LCD_PIN_NUM_MOSI 26
#define LCD_PIN_NUM_MISO -1
#define LCD_PIN_NUM_LCD_DC 27
#define LCD_PIN_NUM_LCD_RST 14
#define LCD_PIN_NUM_LCD_CS 12
#define LCD_PIN_NUM_BK_LIGHT 33

esp_lcd_panel_handle_t screen_internal_panel_handle = NULL;
SemaphoreHandle_t screen_internal_refresh_done_sem = NULL;
uint16_t *screen_internal_dma_buf = NULL;

static bool IRAM_ATTR
tft_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                        esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  BaseType_t high_task_awoken = pdFALSE;
  if (screen_internal_refresh_done_sem) {
    xSemaphoreGiveFromISR(screen_internal_refresh_done_sem, &high_task_awoken);
  }
  return high_task_awoken == pdTRUE;
}

esp_err_t tft_hardware_init(void) {
  if (screen_internal_refresh_done_sem == NULL) {
    screen_internal_refresh_done_sem = xSemaphoreCreateBinary();
  }

  if (screen_internal_dma_buf == NULL) {
    screen_internal_dma_buf = (uint16_t *)heap_caps_malloc(TFT_DMA_BUF_SIZE, MALLOC_CAP_DMA);
    if (!screen_internal_dma_buf) {
      ESP_LOGE(TAG, "Failed to allocate TFT DMA buffer!");
      return ESP_ERR_NO_MEM;
    }
  }

  // 1. Cấu hình đèn nền (GPIO 33)
  gpio_config_t bk_gpio_config = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT,
  };
  gpio_config(&bk_gpio_config);
  gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 0);

  // 2. Cấu hình SPI Bus
  spi_bus_config_t buscfg = {
      .sclk_io_num = LCD_PIN_NUM_SCLK,
      .mosi_io_num = LCD_PIN_NUM_MOSI,
      .miso_io_num = LCD_PIN_NUM_MISO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = TFT_DMA_BUF_SIZE,
  };
  esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // 3. Cấu hình Panel IO SPI
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = LCD_PIN_NUM_LCD_DC,
      .cs_gpio_num = LCD_PIN_NUM_LCD_CS,
      .pclk_hz = 26 * 1000 * 1000, // 26 MHz
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .spi_mode = 0,
      .trans_queue_depth = 10,
      .on_color_trans_done = tft_on_color_trans_done,
      .user_ctx = NULL,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                           &io_config, &io_handle));

  // 4. Khởi tạo driver ILI9341 chính thức
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_PIN_NUM_LCD_RST,
      .rgb_endian = LCD_RGB_ENDIAN_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &screen_internal_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(screen_internal_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(screen_internal_panel_handle));

  // 5. Cài đặt định hướng đứng dọc (Portrait Inverted 240x320)
  // mirror_x = false (chữ đọc xuôi bình thường, không soi gương), mirror_y =
  // true (Portrait Inverted)
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(screen_internal_panel_handle, false, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(screen_internal_panel_handle, false));

  // 6. Bật hiển thị và đèn nền
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(screen_internal_panel_handle, true));
  gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);

  ESP_LOGI(TAG,
           "Official esp_lcd_ili9341 initialized (Portrait Inverted 240x320)");
  return ESP_OK;
}

void tft_draw_bitmap_wait(int16_t x_start, int16_t y_start,
                          int16_t x_end, int16_t y_end,
                          const void *color_data) {
  if (!screen_internal_panel_handle || x_start >= x_end || y_start >= y_end || !color_data)
    return;
  if (screen_internal_refresh_done_sem) {
    xSemaphoreTake(screen_internal_refresh_done_sem, 0);
  }
  esp_lcd_panel_draw_bitmap(screen_internal_panel_handle, x_start, y_start, x_end, y_end,
                            color_data);
  if (screen_internal_refresh_done_sem) {
    xSemaphoreTake(screen_internal_refresh_done_sem, pdMS_TO_TICKS(50));
  }
}
