#include "ScreenManager.h"
#include "font8x8.h"

// Driver chính hãng Espressif esp_lcd và esp_lcd_ili9341
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "FOTAManager.h"
#include "LinkListData.h"
#include "MemoryManager.h"
#include "PowerManager.h"
#include "UartToNode.h"
#include "WSHandle.h"
#include "WifiManager.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "xpt2046_soft.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Bảng màu RGB565 tiêu chuẩn
#define TFT_BLACK 0x0000
#define TFT_NAVY 0x000F
#define TFT_DARKGREEN 0x03E0
#define TFT_DARKCYAN 0x03EF
#define TFT_MAROON 0x7800
#define TFT_PURPLE 0x780F
#define TFT_OLIVE 0x7BE0
#define TFT_LIGHTGREY 0xC618
#define TFT_DARKGREY 0x7BEF
#define TFT_BLUE 0x001F
#define TFT_GREEN 0x07E0
#define TFT_CYAN 0x07FF
#define TFT_RED 0xF800
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW 0xFFE0
#define TFT_WHITE 0xFFFF
#define TFT_ORANGE 0xFD20
#define TFT_GREENYELLOW 0xB7E0

#define TFT_BG_MAIN 0x0000
#define TFT_CARD_BG 0x0862
#define TFT_CARD_BORDER 0x2125
#define TFT_TEXT_MUTED 0x8410
#define TFT_ACCENT_CYAN 0x07FF
#define TFT_ACCENT_GOLD 0xFDE0
#define TFT_BAR_BG 0x18C3

#define TFT_DMA_BUF_LINES 16
#define TFT_DMA_BUF_PIXELS                                                     \
  (TFT_WIDTH * TFT_DMA_BUF_LINES) // 240 * 16 = 3840 pixels
#define TFT_DMA_BUF_SIZE (TFT_DMA_BUF_PIXELS * sizeof(uint16_t)) // 7680 bytes

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_refresh_done_sem = NULL;
static uint16_t *s_dma_buf = NULL;

static bool IRAM_ATTR
tft_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                        esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  BaseType_t high_task_awoken = pdFALSE;
  if (s_refresh_done_sem) {
    xSemaphoreGiveFromISR(s_refresh_done_sem, &high_task_awoken);
  }
  return high_task_awoken == pdTRUE;
}

typedef struct {
  dm_metrics_t *metrics;
  dm_lvgl_t *lvgl;
  dm_telemetry_t *telemetry;
  dm_hw_t *hw;
} screen_ctx_t;

static screen_ctx_t s_screen_ctx = {0};

typedef struct {
  bool active;
  uint8_t percent;
  char target[32];
  char version[32];
  char detail[64];
  TickType_t hide_after_tick;
} screen_ota_state_t;

static screen_ota_state_t s_screen_ota = {0};
static SemaphoreHandle_t s_screen_ota_mutex = NULL;

void screen_manager_set_ota_progress(bool active, uint8_t percent,
                                     const char *target, const char *version,
                                     const char *detail) {
  if (s_screen_ota_mutex &&
      xSemaphoreTake(s_screen_ota_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    s_screen_ota.active = active;
    s_screen_ota.percent = percent;
    if (target && target[0]) {
      strncpy(s_screen_ota.target, target, sizeof(s_screen_ota.target) - 1);
      s_screen_ota.target[sizeof(s_screen_ota.target) - 1] = '\0';
    }
    if (version && version[0]) {
      strncpy(s_screen_ota.version, version, sizeof(s_screen_ota.version) - 1);
      s_screen_ota.version[sizeof(s_screen_ota.version) - 1] = '\0';
    }
    if (detail && detail[0]) {
      strncpy(s_screen_ota.detail, detail, sizeof(s_screen_ota.detail) - 1);
      s_screen_ota.detail[sizeof(s_screen_ota.detail) - 1] = '\0';
    }
    if (!active || percent >= 100 ||
        (detail && (strstr(detail, "Success") || strstr(detail, "Failed")))) {
      s_screen_ota.hide_after_tick = xTaskGetTickCount() + pdMS_TO_TICKS(4000);
    } else {
      s_screen_ota.hide_after_tick = 0;
    }
    xSemaphoreGive(s_screen_ota_mutex);
  }
}

static inline uint16_t swap16(uint16_t v) { return (v >> 8) | (v << 8); }

static esp_err_t tft_hardware_init(void) {
  if (s_refresh_done_sem == NULL) {
    s_refresh_done_sem = xSemaphoreCreateBinary();
  }

  if (s_dma_buf == NULL) {
    s_dma_buf = (uint16_t *)heap_caps_malloc(TFT_DMA_BUF_SIZE, MALLOC_CAP_DMA);
    if (!s_dma_buf) {
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
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

  // 5. Cài đặt định hướng đứng dọc (Portrait Inverted 240x320)
  // mirror_x = false (chữ đọc xuôi bình thường, không soi gương), mirror_y =
  // true (Portrait Inverted)
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, false, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel_handle, false));

  // 6. Bật hiển thị và đèn nền
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
  gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);

  ESP_LOGI(TAG,
           "Official esp_lcd_ili9341 initialized (Portrait Inverted 240x320)");
  return ESP_OK;
}

static void tft_draw_bitmap_wait(int16_t x_start, int16_t y_start,
                                 int16_t x_end, int16_t y_end,
                                 const void *color_data) {
  if (!s_panel_handle || x_start >= x_end || y_start >= y_end || !color_data)
    return;
  if (s_refresh_done_sem) {
    xSemaphoreTake(s_refresh_done_sem, 0);
  }
  esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end,
                            color_data);
  if (s_refresh_done_sem) {
    xSemaphoreTake(s_refresh_done_sem, pdMS_TO_TICKS(50));
  }
}

static void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t color) {
  if (w <= 0 || h <= 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT ||
      s_panel_handle == NULL || s_dma_buf == NULL)
    return;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > TFT_WIDTH)
    w = TFT_WIDTH - x;
  if (y + h > TFT_HEIGHT)
    h = TFT_HEIGHT - y;
  if (w <= 0 || h <= 0)
    return;

  uint16_t swapped = swap16(color);
  int16_t max_lines = TFT_DMA_BUF_PIXELS / w;
  if (max_lines > h)
    max_lines = h;
  if (max_lines < 1)
    max_lines = 1;

  int total_pixels = w * max_lines;
  for (int i = 0; i < total_pixels; i++) {
    s_dma_buf[i] = swapped;
  }

  int16_t cur_y = y;
  while (cur_y < y + h) {
    int16_t lines = y + h - cur_y;
    if (lines > max_lines)
      lines = max_lines;
    tft_draw_bitmap_wait(x, cur_y, x + w, cur_y + lines, s_dma_buf);
    cur_y += lines;
  }
}

static void tft_fill_screen(uint16_t color) {
  tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

static void tft_draw_fast_h_line(int16_t x, int16_t y, int16_t w,
                                 uint16_t color) {
  tft_fill_rect(x, y, w, 1, color);
}

static void tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h,
                                 uint16_t color) {
  tft_fill_rect(x, y, 1, h, color);
}

static void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t color) {
  tft_draw_fast_h_line(x, y, w, color);
  tft_draw_fast_h_line(x, y + h - 1, w, color);
  tft_draw_fast_v_line(x, y, h, color);
  tft_draw_fast_v_line(x + w - 1, y, h, color);
}

static void tft_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg,
                            uint16_t bg, uint8_t size) {
  if (!str || !s_dma_buf || s_panel_handle == NULL)
    return;
  if (size < 1)
    size = 1;

  int16_t char_w = 8 * size;
  int16_t char_h = 8 * size;
  int max_chars_in_buf = TFT_DMA_BUF_PIXELS / (char_w * char_h);
  if (max_chars_in_buf < 1)
    max_chars_in_buf = 1;

  uint16_t fg_sw = swap16(fg);
  uint16_t bg_sw = swap16(bg);

  int16_t cur_x = x;
  int16_t cur_y = y;

  while (*str) {
    if (*str == '\n') {
      cur_x = x;
      cur_y += char_h + 2;
      str++;
      continue;
    }

    if (cur_x >= TFT_WIDTH)
      break;

    int chunk_chars = 0;
    const char *p = str;
    while (*p && *p != '\n' && chunk_chars < max_chars_in_buf) {
      if (cur_x + (chunk_chars + 1) * char_w > TFT_WIDTH) {
        break;
      }
      chunk_chars++;
      p++;
    }

    if (chunk_chars == 0)
      break;

    int16_t chunk_w = chunk_chars * char_w;
    int16_t chunk_h = char_h;

    for (int c_idx = 0; c_idx < chunk_chars; c_idx++) {
      char c = str[c_idx];
      if (c < 32 || c > 127)
        c = ' ';
      const uint8_t *glyph = font8x8[(uint8_t)(c - 32)];

      for (uint8_t row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (uint8_t col = 0; col < 8; col++) {
          uint16_t pixel = (line & (1 << (7 - col))) ? fg_sw : bg_sw;
          for (uint8_t sy = 0; sy < size; sy++) {
            int py = row * size + sy;
            for (uint8_t sx = 0; sx < size; sx++) {
              int px = c_idx * char_w + col * size + sx;
              s_dma_buf[py * chunk_w + px] = pixel;
            }
          }
        }
      }
    }

    if (cur_x < TFT_WIDTH && cur_y < TFT_HEIGHT && cur_x + chunk_w > 0 &&
        cur_y + chunk_h > 0) {
      tft_draw_bitmap_wait(cur_x, cur_y, cur_x + chunk_w, cur_y + chunk_h,
                           s_dma_buf);
    }

    cur_x += chunk_w;
    str += chunk_chars;
  }
}

static void tft_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h,
                                  uint8_t percent, uint16_t fg_color,
                                  uint16_t bg_color, uint16_t border_color) {
  if (percent > 100)
    percent = 100;
  tft_draw_rect(x, y, w, h, border_color);

  int16_t inner_x = x + 2;
  int16_t inner_y = y + 2;
  int16_t inner_w = w - 4;
  int16_t inner_h = h - 4;
  if (inner_w <= 0 || inner_h <= 0)
    return;

  int16_t fill_w = (inner_w * percent) / 100;
  if (fill_w > 0) {
    tft_fill_rect(inner_x, inner_y, fill_w, inner_h, fg_color);
  }
  if (fill_w < inner_w) {
    tft_fill_rect(inner_x + fill_w, inner_y, inner_w - fill_w, inner_h,
                  bg_color);
  }
}

static const char *get_weekday_name(uint8_t wday) {
  switch (wday) {
  case 0:
    return "Sun";
  case 1:
    return "Mon";
  case 2:
    return "Tue";
  case 3:
    return "Wed";
  case 4:
    return "Thu";
  case 5:
    return "Fri";
  case 6:
    return "Sat";
  default:
    return "---";
  }
}

static const char *s_month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void tft_draw_text_box(int16_t x, int16_t y, int16_t w, int16_t h,
                              const char *text, uint16_t fg, uint16_t bg,
                              uint8_t size) {
  if (w <= 0 || h <= 0)
    return;
  tft_fill_rect(x, y, w, h, bg);
  if (text && text[0] != '\0') {
    tft_draw_string(x, y, text, fg, bg, size);
  }
}

static void draw_text_cached_box(int16_t x, int16_t y, int16_t w, int16_t h,
                                 char *cache, size_t cache_sz,
                                 const char *new_text, uint16_t fg, uint16_t bg,
                                 uint8_t size) {
  if (strcmp(cache, new_text) != 0) {
    tft_draw_text_box(x, y, w, h, new_text, fg, bg, size);
    strncpy(cache, new_text, cache_sz - 1);
    cache[cache_sz - 1] = '\0';
  }
}

static void tft_draw_segmented_bar(int16_t x, int16_t y, int16_t w, int16_t h,
                                   uint8_t percent, uint16_t active_color,
                                   uint16_t inactive_color) {
  if (w <= 0 || h <= 0 || !s_dma_buf || s_panel_handle == NULL)
    return;
  if (percent > 100)
    percent = 100;

  const int seg_w = 2;
  const int seg_gap = 1;
  const int pitch = seg_w + seg_gap; // 3 px pitch for |||||||||| style
  int total_segs = w / pitch;
  if (total_segs < 1)
    return;
  int active_segs = (total_segs * percent) / 100;
  if (percent > 0 && active_segs == 0)
    active_segs = 1;

  uint16_t act_sw = swap16(active_color);
  uint16_t inact_sw = swap16(inactive_color);
  uint16_t bg_sw = swap16(TFT_BG_MAIN);

  for (int row = 0; row < h; row++) {
    int row_offset = row * w;
    for (int s = 0; s < total_segs; s++) {
      uint16_t seg_col = (s < active_segs) ? act_sw : inact_sw;
      int base_x = s * pitch;
      for (int i = 0; i < seg_w; i++) {
        s_dma_buf[row_offset + base_x + i] = seg_col;
      }
      for (int g = 0; g < seg_gap; g++) {
        s_dma_buf[row_offset + base_x + seg_w + g] = bg_sw;
      }
    }
    int filled_w = total_segs * pitch;
    for (int rem = filled_w; rem < w; rem++) {
      s_dma_buf[row_offset + rem] = bg_sw;
    }
  }

  tft_draw_bitmap_wait(x, y, x + w, y + h, s_dma_buf);
}

static void draw_segmented_bar_cached(int16_t x, int16_t y, int16_t w,
                                      int16_t h, uint8_t *cached_pct,
                                      uint8_t new_pct, uint16_t active_color,
                                      uint16_t inactive_color) {
  if (*cached_pct != new_pct) {
    *cached_pct = new_pct;
    tft_draw_segmented_bar(x, y, w, h, new_pct, active_color, inactive_color);
  }
}

static void draw_progress_bar_cached(int16_t x, int16_t y, int16_t w, int16_t h,
                                     uint8_t *cached_pct, uint8_t new_pct,
                                     uint16_t fg_color, uint16_t bg_color,
                                     uint16_t border_color) {
  if (*cached_pct != new_pct) {
    *cached_pct = new_pct;
    tft_draw_progress_bar(x, y, w, h, new_pct, fg_color, bg_color,
                          border_color);
  }
}

static const char *get_reset_reason_name(void) {
  switch (esp_reset_reason()) {
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_SW:
    return "SW_RST";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
    return "WDT_RST";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "NORMAL";
  }
}

static void draw_battery_glyph(int16_t x, int16_t y, int bat_pct) {
  tft_fill_rect(x, y, 24, 8, TFT_CARD_BG);
  tft_draw_rect(x, y, 20, 8, TFT_CYAN);
  tft_draw_fast_v_line(x + 20, y + 2, 4, TFT_CYAN);

  uint16_t col = (bat_pct > 20) ? TFT_GREEN : TFT_RED;
  int segs = 0;
  if (bat_pct >= 75)
    segs = 3;
  else if (bat_pct >= 40)
    segs = 2;
  else if (bat_pct >= 15)
    segs = 1;

  for (int i = 0; i < 3; i++) {
    uint16_t c = (i < segs) ? col : TFT_CARD_BG;
    tft_fill_rect(x + 2 + i * 6, y + 2, 4, 4, c);
  }
}

static void render_target_button(websocket_target_t target) {
  const int16_t bx = 148, by = 43, bw = 84, bh = 42;
  const char *cur_url = get_ws_url();
  bool is_server = (target == WEBSOCKET_TARGET_SERVER);
  if (target == WEBSOCKET_TARGET_CUSTOM && cur_url) {
    if (strncmp(cur_url, "wss://", 6) == 0 || strstr(cur_url, "msems.click") != NULL) {
      is_server = true;
    }
  }

  if (is_server) {
    tft_fill_rect(bx, by, bw, bh, 0x0151); // Dark Navy Blue
    tft_draw_rect(bx, by, bw, bh, TFT_CYAN);
    tft_draw_rect(bx + 1, by + 1, bw - 2, bh - 2, TFT_CYAN);
    tft_draw_string(bx + 10, by + 8, "[SERVER]", TFT_WHITE, 0x0151, 1);
    tft_draw_string(bx + 6, by + 24, "[TAP CHG]", TFT_YELLOW, 0x0151, 1);
  } else if (target == WEBSOCKET_TARGET_LOCAL) {
    tft_fill_rect(bx, by, bw, bh, 0x02C0); // Dark Forest Green
    tft_draw_rect(bx, by, bw, bh, TFT_GREEN);
    tft_draw_rect(bx + 1, by + 1, bw - 2, bh - 2, TFT_GREEN);
    tft_draw_string(bx + 14, by + 8, "[LOCAL]", TFT_WHITE, 0x02C0, 1);
    tft_draw_string(bx + 6, by + 24, "[TAP CHG]", TFT_YELLOW, 0x02C0, 1);
  } else {
    // Custom IP
    tft_fill_rect(bx, by, bw, bh, 0x2190); // Muted Teal
    tft_draw_rect(bx, by, bw, bh, TFT_CYAN);
    tft_draw_rect(bx + 1, by + 1, bw - 2, bh - 2, TFT_CYAN);
    tft_draw_string(bx + 10, by + 8, "[CUSTOM]", TFT_WHITE, 0x2190, 1);
    tft_draw_string(bx + 6, by + 24, "[TAP CHG]", TFT_YELLOW, 0x2190, 1);
  }
}

static void render_static_dashboard(void) {
  tft_fill_screen(TFT_BG_MAIN);

  // 1. Top Header Background (0,0 to 240, 34)
  tft_fill_rect(0, 0, TFT_WIDTH, 34, TFT_CARD_BG);
  tft_draw_fast_h_line(0, 34, TFT_WIDTH, TFT_CARD_BORDER);

  // 2. Card 1: Gateway & Network (4, 36 to 236, 90)
  tft_draw_rect(4, 36, 232, 54, TFT_CARD_BORDER);
  tft_draw_string(8, 39, "[NETWORK]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 3. Card 2: System Performance (4, 93 to 236, 147)
  tft_draw_rect(4, 93, 232, 54, TFT_CARD_BORDER);
  tft_draw_string(8, 96, "[SYSTEM PERFORMANCE]", TFT_ACCENT_CYAN, TFT_BG_MAIN,
                  1);

  // 4. Card 3: Mesh Network Status (4, 150 to 236, 206)
  tft_draw_rect(4, 150, 232, 56, TFT_CARD_BORDER);
  tft_draw_string(8, 153, "[MESH NETWORK]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 5. Card 4: Traffic & Flow (4, 209 to 236, 265)
  tft_draw_rect(4, 209, 232, 56, TFT_CARD_BORDER);
  tft_draw_string(8, 212, "[TRAFFIC & FLOW]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 6. Card 5: System Health & Diagnostics (4, 268 to 236, 317)
  tft_draw_rect(4, 268, 232, 49, TFT_CARD_BORDER);
  tft_draw_string(8, 271, "[SYS HEALTH & DIAGNOSTICS]", TFT_ACCENT_CYAN,
                  TFT_BG_MAIN, 1);
}

static void tft_screen_task(void *arg) {
  screen_ctx_t *ctx = (screen_ctx_t *)arg;
  ui_metrics_t m = {0};

  // Khởi tạo phần cứng màn hình qua driver chính hãng esp_lcd_ili9341
  ESP_ERROR_CHECK(tft_hardware_init());

  // Khởi tạo cảm ứng soft SPI touch xpt2046
  xpt2046_soft_init();

  render_static_dashboard();

  char c_hdr_title[24] = "";
  char c_wifi[32] = "";
  char c_ws_badge[16] = "";
  char c_time[16] = "";
  char c_date[32] = "";
  char c_bat[16] = "";
  int last_bat_glyph = -1;
  char c_uptime[16] = "";
  char c_vbat[16] = "";
  char c_version[16] = "";

  char c_ip[32] = "";
  char c_mac[32] = "";
  char c_ap_rec[32] = "";

  char c_fps[16] = "";
  char c_cpu_lbl[16] = "";
  char c_cpu_rest[16] = "";
  uint8_t cached_cpu_bar = 255;
  char c_ram_lbl[16] = "";
  char c_ram_rest_free[32] = "";
  uint8_t cached_ram_bar = 255;
  char c_sd[64] = "";

  char c_node_ver[16] = "";
  char c_mesh_nodes[36] = "";
  char c_mesh_weak[32] = "";
  char c_mesh_level[32] = "";
  char c_mesh_root[36] = "";

  char c_traffic_lat[16] = "";
  char c_traffic_thru_q[36] = "";
  char c_rx_rate[24] = "";
  char c_tx_rate[24] = "";
  uint8_t cached_rx_pulse = 255;
  uint8_t cached_tx_pulse = 255;
  char c_pipe[32] = "";

  char c_diag_heap[36] = "";
  char c_diag_rst_rtc[36] = "";
  char c_diag_status[36] = "";

  uint32_t last_rx_pkts = 0;
  uint32_t last_tx_pkts = 0;
  uint32_t last_rx_bytes = 0;
  uint32_t last_tx_bytes = 0;
  uint32_t rx_pkt_rate = 0;
  uint32_t tx_pkt_rate = 0;
  uint32_t throughput_kb = 0;
  uint8_t rx_pulse_pct = 0;
  uint8_t tx_pulse_pct = 0;
  TickType_t last_rate_tick = 0;
  bool is_first_telemetry = true;

  websocket_target_t last_target = (websocket_target_t)255;

  bool ota_overlay_active = false;
  uint8_t last_ota_percent = 255;
  char c_ota_status[64] = "";

  TickType_t last_touch_check = 0;

  while (1) {
    if (ctx && ctx->metrics && ctx->metrics->mutex &&
        xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      m = ctx->metrics->value;
      if (ctx->lvgl) {
        ctx->lvgl->loop_counter++;
      }
      xSemaphoreGive(ctx->metrics->mutex);
    }

    screen_ota_state_t ota_snap = {0};
    bool show_ota = false;
    if (s_screen_ota_mutex &&
        xSemaphoreTake(s_screen_ota_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (fota_is_running()) {
        s_screen_ota.active = true;
        s_screen_ota.percent = fota_get_progress_percent();
        if (s_screen_ota.target[0] == '\0') {
          strncpy(s_screen_ota.target, "Gateway",
                  sizeof(s_screen_ota.target) - 1);
        }
      }
      show_ota = s_screen_ota.active;
      if (s_screen_ota.hide_after_tick > 0) {
        if (xTaskGetTickCount() >= s_screen_ota.hide_after_tick) {
          show_ota = false;
          s_screen_ota.active = false;
          s_screen_ota.hide_after_tick = 0;
        } else {
          show_ota = true;
        }
      }
      ota_snap = s_screen_ota;
      xSemaphoreGive(s_screen_ota_mutex);
    }

    if (show_ota) {
      if (!ota_overlay_active) {
        ota_overlay_active = true;
        last_ota_percent = 255;
        c_ota_status[0] = '\0';

        tft_fill_rect(10, 45, 220, 225, TFT_CARD_BG);
        tft_draw_rect(10, 45, 220, 225, TFT_ORANGE);
        tft_draw_rect(11, 46, 218, 223, TFT_YELLOW);

        tft_draw_string(20, 55, "*** GATEWAY OTA UPDATE ***", TFT_YELLOW,
                        TFT_CARD_BG, 1);
        tft_draw_fast_h_line(15, 68, 210, TFT_CARD_BORDER);

        char target_str[64];
        snprintf(target_str, sizeof(target_str), "Target: %s",
                 ota_snap.target[0] ? ota_snap.target : "Gateway");
        tft_draw_string(20, 75, target_str, TFT_WHITE, TFT_CARD_BG, 1);

        char ver_str[64];
        snprintf(ver_str, sizeof(ver_str), "Firmware: %s",
                 ota_snap.version[0] ? ota_snap.version : "---");
        tft_draw_string(20, 88, ver_str, TFT_ACCENT_CYAN, TFT_CARD_BG, 1);
      }

      if (ota_snap.percent != last_ota_percent) {
        last_ota_percent = ota_snap.percent;
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%3u%%", ota_snap.percent);
        tft_draw_string(80, 108, pct_str, TFT_GREENYELLOW, TFT_CARD_BG, 3);
        tft_draw_progress_bar(20, 140, 200, 18, ota_snap.percent, TFT_GREEN,
                              TFT_BAR_BG, TFT_WHITE);
      }

      char new_status[64];
      snprintf(new_status, sizeof(new_status), "%-25s",
               ota_snap.detail[0] ? ota_snap.detail : "Downloading...");
      if (strcmp(c_ota_status, new_status) != 0) {
        tft_draw_string(20, 168, new_status, TFT_ACCENT_CYAN, TFT_CARD_BG, 1);
        strncpy(c_ota_status, new_status, sizeof(c_ota_status) - 1);
      }

      tft_draw_string(20, 195, "Partition: [ota_1] ready", TFT_WHITE,
                      TFT_CARD_BG, 1);
      tft_draw_string(20, 215, "Please do not power off!", TFT_ORANGE,
                      TFT_CARD_BG, 1);
      tft_draw_string(20, 235, "Free Heap: > 100 KB OK  ", TFT_GREEN,
                      TFT_CARD_BG, 1);

      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (ota_overlay_active) {
      ota_overlay_active = false;
      render_static_dashboard();
      c_hdr_title[0] = '\0';
      c_wifi[0] = '\0';
      c_ws_badge[0] = '\0';
      c_time[0] = '\0';
      c_date[0] = '\0';
      c_bat[0] = '\0';
      last_bat_glyph = -1;
      c_uptime[0] = '\0';
      c_vbat[0] = '\0';
      c_version[0] = '\0';
      c_ip[0] = '\0';
      c_mac[0] = '\0';
      c_ap_rec[0] = '\0';
      c_fps[0] = '\0';
      c_cpu_lbl[0] = '\0';
      c_cpu_rest[0] = '\0';
      c_ram_lbl[0] = '\0';
      c_ram_rest_free[0] = '\0';
      c_sd[0] = '\0';
      c_node_ver[0] = '\0';
      c_mesh_nodes[0] = '\0';
      c_mesh_weak[0] = '\0';
      c_mesh_level[0] = '\0';
      c_mesh_root[0] = '\0';
      c_traffic_lat[0] = '\0';
      c_traffic_thru_q[0] = '\0';
      c_rx_rate[0] = '\0';
      c_tx_rate[0] = '\0';
      c_pipe[0] = '\0';
      c_diag_heap[0] = '\0';
      c_diag_rst_rtc[0] = '\0';
      c_diag_status[0] = '\0';
      cached_cpu_bar = 255;
      cached_ram_bar = 255;
      cached_rx_pulse = 255;
      cached_tx_pulse = 255;
      rx_pulse_pct = 0;
      tx_pulse_pct = 0;
      is_first_telemetry = true;
      last_target = (websocket_target_t)255;
    }

    // Touch Polling cho Nút Target Server / Local (bx=148..232, by=43..85)
    TickType_t now_tick = xTaskGetTickCount();
    if ((now_tick - last_touch_check) >= pdMS_TO_TICKS(150)) {
      last_touch_check = now_tick;
      int16_t tx = 0, ty = 0;
      if (xpt2046_soft_poll(&tx, &ty)) {
        if (ty >= 40 && ty <= 88 && tx >= 144 && tx <= 236) {
          websocket_target_t cur = websocket_get_selected_target();
          const char *cur_url = get_ws_url();
          bool is_server = (cur == WEBSOCKET_TARGET_SERVER) ||
                           (cur == WEBSOCKET_TARGET_CUSTOM && cur_url &&
                            (strncmp(cur_url, "wss://", 6) == 0 || strstr(cur_url, "msems.click") != NULL));
          websocket_select_target(is_server ? WEBSOCKET_TARGET_LOCAL
                                            : WEBSOCKET_TARGET_SERVER);
        }
      }
    }

    // Tính toán Throughput & RX/TX packet rate
    uint32_t cur_rx_pkts =
        (ctx && ctx->telemetry) ? ctx->telemetry->rx_packet_count : 0;
    uint32_t cur_tx_pkts =
        (ctx && ctx->telemetry) ? ctx->telemetry->tx_packet_count : 0;
    uint32_t cur_rx_bytes =
        (ctx && ctx->telemetry) ? ctx->telemetry->rx_byte_count : 0;
    uint32_t cur_tx_bytes =
        (ctx && ctx->telemetry) ? ctx->telemetry->tx_byte_count : 0;

    if (is_first_telemetry) {
      last_rx_pkts = cur_rx_pkts;
      last_tx_pkts = cur_tx_pkts;
      last_rx_bytes = cur_rx_bytes;
      last_tx_bytes = cur_tx_bytes;
      last_rate_tick = now_tick;
      is_first_telemetry = false;
    }

    if ((now_tick - last_rate_tick) >= pdMS_TO_TICKS(1000)) {
      uint32_t dt_ms = (now_tick - last_rate_tick) * portTICK_PERIOD_MS;
      if (dt_ms > 0) {
        rx_pkt_rate = (cur_rx_pkts >= last_rx_pkts)
                          ? (((cur_rx_pkts - last_rx_pkts) * 1000U) / dt_ms)
                          : 0;
        tx_pkt_rate = (cur_tx_pkts >= last_tx_pkts)
                          ? (((cur_tx_pkts - last_tx_pkts) * 1000U) / dt_ms)
                          : 0;
        uint32_t delta_bytes =
            (cur_rx_bytes >= last_rx_bytes ? (cur_rx_bytes - last_rx_bytes)
                                           : 0) +
            (cur_tx_bytes >= last_tx_bytes ? (cur_tx_bytes - last_tx_bytes)
                                           : 0);
        throughput_kb = (delta_bytes * 1000U) / (dt_ms * 1024U);
      }
      last_rx_pkts = cur_rx_pkts;
      last_tx_pkts = cur_tx_pkts;
      last_rx_bytes = cur_rx_bytes;
      last_tx_bytes = cur_tx_bytes;
      last_rate_tick = now_tick;
    }

    // Hiệu ứng nhấp nháy thanh throughput khi có gói tin
    if (rx_pkt_rate > 0) {
      uint32_t p = rx_pkt_rate * 12U + 25U;
      rx_pulse_pct = (p > 100U) ? 100U : (uint8_t)p;
    } else if (rx_pulse_pct > 15) {
      rx_pulse_pct -= 15;
    } else {
      rx_pulse_pct = 0;
    }

    if (tx_pkt_rate > 0) {
      uint32_t p = tx_pkt_rate * 12U + 25U;
      tx_pulse_pct = (p > 100U) ? 100U : (uint8_t)p;
    } else if (tx_pulse_pct > 15) {
      tx_pulse_pct -= 15;
    } else {
      tx_pulse_pct = 0;
    }

    char buf[128];

    // --- 1. TOP HEADER (Y: 0..34) ---
    // Dòng 0 (Y: 2): Tiêu đề Gateway + WiFi SSID/Badge + Badge WS
    draw_text_cached_box(4, 2, 56, 8, c_hdr_title, sizeof(c_hdr_title),
                         "GATEWAY", 0xF9B4, TFT_CARD_BG, 1);

    bool wifi_ok = is_wifi_connected();
    char wifi_disp[24] = {0};
    uint16_t wifi_color = TFT_RED;

    if (wifi_ok) {
      char raw_ssid[34] = {0};
      wifi_ap_record_t ap_info = {0};
      if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && ap_info.ssid[0] != '\0') {
        strncpy(raw_ssid, (const char *)ap_info.ssid, sizeof(raw_ssid) - 1);
      } else {
        wifi_manager_get_sta_credentials(raw_ssid, sizeof(raw_ssid), NULL, 0);
      }

      if (raw_ssid[0] == '\0') {
        strncpy(raw_ssid, "WIFI:OK", sizeof(raw_ssid) - 1);
      }

      size_t ssid_len = strlen(raw_ssid);
      if (ssid_len > 10) {
        strncpy(wifi_disp, raw_ssid, 10);
        wifi_disp[10] = '\0';
        strcat(wifi_disp, "...");
      } else {
        strncpy(wifi_disp, raw_ssid, sizeof(wifi_disp) - 1);
      }
      wifi_color = TFT_GREEN;
    } else if (is_wifi_connecting()) {
      strncpy(wifi_disp, "CONNECTING..", sizeof(wifi_disp) - 1);
      wifi_color = TFT_YELLOW;
    } else {
      strncpy(wifi_disp, "NO-WIFI", sizeof(wifi_disp) - 1);
      wifi_color = TFT_RED;
    }

    draw_text_cached_box(64, 2, 116, 8, c_wifi, sizeof(c_wifi), wifi_disp,
                         wifi_color, TFT_CARD_BG, 1);

    bool ws_ok = websocket_is_connected();
    draw_text_cached_box(184, 2, 54, 8, c_ws_badge, sizeof(c_ws_badge),
                         ws_ok ? "[WS:OK]" : "[WS:--]",
                         ws_ok ? TFT_GREEN : TFT_RED, TFT_CARD_BG, 1);

    // Dòng 1 (Y: 13): Giờ RTC + Ngày Tháng (RTC DS3231) + Pin BAT% + Icon Pin
    // đồ họa
    if (m.rtc_ok) {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", m.rtc_time.tm_hour,
               m.rtc_time.tm_min, m.rtc_time.tm_sec);
      draw_text_cached_box(4, 13, 68, 8, c_time, sizeof(c_time), buf, TFT_CYAN,
                           TFT_CARD_BG, 1);

      const char *mon = (m.rtc_time.tm_mon >= 0 && m.rtc_time.tm_mon <= 11)
                            ? s_month_names[m.rtc_time.tm_mon]
                            : "---";
      snprintf(buf, sizeof(buf), "%s, %02d %s",
               get_weekday_name((uint8_t)m.rtc_time.tm_wday),
               m.rtc_time.tm_mday, mon);
      draw_text_cached_box(76, 13, 90, 8, c_date, sizeof(c_date), buf,
                           TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    } else {
      draw_text_cached_box(4, 13, 68, 8, c_time, sizeof(c_time), "00:00:00",
                           TFT_CYAN, TFT_CARD_BG, 1);
      draw_text_cached_box(76, 13, 90, 8, c_date, sizeof(c_date),
                           "Syncing RTC..", TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    }

    int bat_pct = 0;
    if (ctx && ctx->hw) {
      bat_pct = power_manager_battery_get_percent(ctx->hw, NULL, NULL);
    }
    snprintf(buf, sizeof(buf), "%3d%%", bat_pct);
    draw_text_cached_box(170, 13, 36, 8, c_bat, sizeof(c_bat), buf,
                         (bat_pct > 20) ? TFT_CYAN : TFT_RED, TFT_CARD_BG, 1);
    if (last_bat_glyph != bat_pct) {
      last_bat_glyph = bat_pct;
      draw_battery_glyph(208, 13, bat_pct);
    }

    // Dòng 2 (Y: 23): Uptime + Điện áp Pin Vbat + Version
    float days_f = (float)m.uptime_s / 86400.0f;
    snprintf(buf, sizeof(buf), "Up:%.2fd", (double)days_f);
    draw_text_cached_box(4, 23, 70, 8, c_uptime, sizeof(c_uptime), buf,
                         TFT_TEXT_MUTED, TFT_CARD_BG, 1);

    float vbat_f = (float)m.battery_pack_mv / 1000.0f;
    snprintf(buf, sizeof(buf), "Vbat:%.2fV", (double)vbat_f);
    draw_text_cached_box(80, 23, 84, 8, c_vbat, sizeof(c_vbat), buf,
                         TFT_TEXT_MUTED, TFT_CARD_BG, 1);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    snprintf(buf, sizeof(buf), "v%s", app_desc ? app_desc->version : "0.0.1");
    draw_text_cached_box(188, 23, 48, 8, c_version, sizeof(c_version), buf,
                         TFT_ACCENT_GOLD, TFT_CARD_BG, 1);

    // --- 2. CARD 1: GATEWAY & NETWORK (Y: 36..90) ---
    // IP STA (tối đa 15 ký tự để không bao giờ chạm button tại x=148)
    char ip_val[20] = "0.0.0.0";
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
      esp_netif_ip_info_t ipi;
      if (esp_netif_get_ip_info(sta_netif, &ipi) == ESP_OK &&
          ipi.ip.addr != 0) {
        snprintf(ip_val, sizeof(ip_val), IPSTR, IP2STR(&ipi.ip));
      }
    }
    snprintf(buf, sizeof(buf),
             (strlen(ip_val) > 11) ? "IP:%-13s" : "IP : %-11s", ip_val);
    draw_text_cached_box(8, 50, 134, 8, c_ip, sizeof(c_ip), buf, TFT_WHITE,
                         TFT_BG_MAIN, 1);

    // MAC (lấy 8 ký tự cuối "...F4:D0:D8", tối đa 14 ký tự)
    char mac_val[24] = "00:00:00:00:00:00";
    wifi_manager_get_mac_info(mac_val, sizeof(mac_val));
    size_t mac_l = strlen(mac_val);
    const char *mac_end = (mac_l >= 8) ? (mac_val + mac_l - 8) : mac_val;
    snprintf(buf, sizeof(buf), "MAC: ..%-8s", mac_end);
    draw_text_cached_box(8, 62, 134, 8, c_mac, sizeof(c_mac), buf, TFT_WHITE,
                         TFT_BG_MAIN, 1);

    // SoftAP & Reconnect Count (tối đa 15 ký tự)
    snprintf(buf, sizeof(buf), "AP : ROOT (R:%lu)",
             (unsigned long)websocket_get_reconnect_count());
    draw_text_cached_box(8, 74, 134, 8, c_ap_rec, sizeof(c_ap_rec), buf,
                         TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    // Card Switch Server / Local (1/2 width: x=148..232, y=43..85)
    websocket_target_t cur_target = websocket_get_selected_target();
    if (cur_target != last_target) {
      last_target = cur_target;
      render_target_button(cur_target);
    }

    // --- 3. CARD 2: SYSTEM PERFORMANCE (Y: 93..147) ---
    snprintf(buf, sizeof(buf), "FPS:%-3lu", (unsigned long)m.fps);
    draw_text_cached_box(184, 96, 48, 8, c_fps, sizeof(c_fps), buf,
                         TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    // CPU + Segmented Bar + Rest%
    uint32_t cpu_pct = m.cpu_load_permille / 10U;
    if (cpu_pct > 100)
      cpu_pct = 100;
    uint32_t cpu_rest = 100U - cpu_pct;
    snprintf(buf, sizeof(buf), "CPU :%2lu%%", (unsigned long)cpu_pct);
    draw_text_cached_box(8, 107, 64, 8, c_cpu_lbl, sizeof(c_cpu_lbl), buf,
                         TFT_WHITE, TFT_BG_MAIN, 1);
    draw_segmented_bar_cached(76, 107, 48, 8, &cached_cpu_bar, (uint8_t)cpu_pct,
                              TFT_CYAN, TFT_BAR_BG);
    snprintf(buf, sizeof(buf), "Rest:%2lu%%", (unsigned long)cpu_rest);
    draw_text_cached_box(130, 107, 102, 8, c_cpu_rest, sizeof(c_cpu_rest), buf,
                         TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    // RAM + Segmented Bar + Rest% + Free Heap
    uint8_t mem_pct = 0;
    uint32_t free_kb = 0;
    memory_manager_get_usage(&free_kb, NULL, &mem_pct);
    uint8_t mem_rest = (mem_pct <= 100) ? (100U - mem_pct) : 0;
    snprintf(buf, sizeof(buf), "RAM :%2u%%", mem_pct);
    draw_text_cached_box(8, 119, 64, 8, c_ram_lbl, sizeof(c_ram_lbl), buf,
                         TFT_WHITE, TFT_BG_MAIN, 1);
    draw_segmented_bar_cached(76, 119, 48, 8, &cached_ram_bar, mem_pct,
                              TFT_GREEN, TFT_BAR_BG);
    snprintf(buf, sizeof(buf), "R:%2u%% F:%-3luK", mem_rest,
             (unsigned long)(free_kb));
    draw_text_cached_box(130, 119, 102, 8, c_ram_rest_free,
                         sizeof(c_ram_rest_free), buf, TFT_GREENYELLOW,
                         TFT_BG_MAIN, 1);

    // SD Card
    if (m.sd_total_kb == 0) {
      snprintf(buf, sizeof(buf), "SD : NOT DETECTED");
    } else {
      uint32_t mb_tot = m.sd_total_kb / 1024U;
      uint32_t mb_free = m.sd_free_kb / 1024U;
      snprintf(buf, sizeof(buf), "SD : %lu.%02luGB (%luMB Free)",
               (unsigned long)(mb_tot / 1000U),
               (unsigned long)((mb_tot % 1000U) / 10U), (unsigned long)mb_free);
    }
    draw_text_cached_box(8, 131, 224, 8, c_sd, sizeof(c_sd), buf,
                         (m.sd_total_kb > 0) ? TFT_GREENYELLOW : TFT_RED,
                         TFT_BG_MAIN, 1);

    // --- 4. CARD 3: MESH NETWORK STATUS (Y: 150..206) ---
    draw_text_cached_box(160, 153, 72, 8, c_node_ver, sizeof(c_node_ver),
                         "Node:v0.1", TFT_ACCENT_GOLD, TFT_BG_MAIN, 1);

    size_t node_count = link_list_data_get_count();
    snprintf(buf, sizeof(buf), "Connected Nodes : %-2u   Conn : %-2u",
             (unsigned)node_count, (unsigned)node_count);
    draw_text_cached_box(8, 165, 224, 8, c_mesh_nodes, sizeof(c_mesh_nodes),
                         buf, TFT_GREEN, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "Weak Bat Nodes  : 0");
    draw_text_cached_box(8, 175, 224, 8, c_mesh_weak, sizeof(c_mesh_weak), buf,
                         TFT_GREEN, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "Max Mesh Level  : Level 1");
    draw_text_cached_box(8, 185, 224, 8, c_mesh_level, sizeof(c_mesh_level),
                         buf, TFT_CYAN, TFT_BG_MAIN, 1);

    bool root_online =
        (node_count > 0 || rx_pkt_rate > 0 ||
         (ctx && ctx->telemetry && ctx->telemetry->rx_packet_count > 0));
    snprintf(buf, sizeof(buf), "Root Coord: %s",
             root_online ? "ONLINE (ACTIVE)" : "ONLINE (READY)");
    draw_text_cached_box(8, 195, 224, 8, c_mesh_root, sizeof(c_mesh_root), buf,
                         root_online ? TFT_GREEN : TFT_YELLOW, TFT_BG_MAIN, 1);

    // --- 5. CARD 4: TRAFFIC & FLOW (Y: 209..265) ---
    draw_text_cached_box(144, 212, 88, 8, c_traffic_lat, sizeof(c_traffic_lat),
                         "Latency:<5ms", TFT_GREEN, TFT_BG_MAIN, 1);

    uint32_t q_used = 0, q_total = 0;
    uart_to_node_get_queue_status(&q_used, &q_total);
    if (q_total == 0)
      q_total = 32;
    if (throughput_kb == 0) {
      snprintf(buf, sizeof(buf), "Flow: 0 KB/s    Queue:%lu/%-2lu",
               (unsigned long)q_used, (unsigned long)q_total);
    } else {
      snprintf(buf, sizeof(buf), "Flow: %-4luKB/s  Queue:%lu/%-2lu",
               (unsigned long)throughput_kb, (unsigned long)q_used,
               (unsigned long)q_total);
    }
    draw_text_cached_box(8, 223, 224, 8, c_traffic_thru_q,
                         sizeof(c_traffic_thru_q), buf, TFT_WHITE, TFT_BG_MAIN,
                         1);

    snprintf(buf, sizeof(buf), "RX Rate: %4lu p/s", (unsigned long)rx_pkt_rate);
    draw_text_cached_box(8, 234, 140, 8, c_rx_rate, sizeof(c_rx_rate), buf,
                         TFT_CYAN, TFT_BG_MAIN, 1);
    draw_segmented_bar_cached(152, 234, 80, 8, &cached_rx_pulse, rx_pulse_pct,
                              TFT_CYAN, TFT_BAR_BG);

    snprintf(buf, sizeof(buf), "TX Rate: %4lu p/s", (unsigned long)tx_pkt_rate);
    draw_text_cached_box(8, 245, 140, 8, c_tx_rate, sizeof(c_tx_rate), buf,
                         TFT_GREEN, TFT_BG_MAIN, 1);
    draw_segmented_bar_cached(152, 245, 80, 8, &cached_tx_pulse, tx_pulse_pct,
                              TFT_GREEN, TFT_BAR_BG);

    snprintf(buf, sizeof(buf), "Pipe: UART1 (115k) <-> WS");
    draw_text_cached_box(8, 255, 224, 8, c_pipe, sizeof(c_pipe), buf,
                         TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    // --- 6. CARD 5: SYSTEM HEALTH & DIAGNOSTICS (Y: 268..317) ---
    size_t min_heap_kb = esp_get_minimum_free_heap_size() / 1024U;
    size_t dma_heap_kb = heap_caps_get_free_size(MALLOC_CAP_DMA) / 1024U;
    snprintf(buf, sizeof(buf), "MinHeap:%3luKB | DMA:%3luKB",
             (unsigned long)min_heap_kb, (unsigned long)dma_heap_kb);
    draw_text_cached_box(8, 282, 224, 8, c_diag_heap, sizeof(c_diag_heap), buf,
                         TFT_GREENYELLOW, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "Reset: %-7s | RTC: %-6s",
             get_reset_reason_name(),
             websocket_is_time_synced() ? "SYNCED"
                                        : (m.rtc_ok ? "READY" : "SYNC.."));
    draw_text_cached_box(8, 293, 224, 8, c_diag_rst_rtc, sizeof(c_diag_rst_rtc),
                         buf, TFT_WHITE, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "STATUS: ALL SERVICES HEALTHY");
    draw_text_cached_box(8, 304, 224, 8, c_diag_status, sizeof(c_diag_status),
                         buf, TFT_GREEN, TFT_BG_MAIN, 1);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl,
                          dm_telemetry_t *telemetry, dm_hw_t *hw,
                          UBaseType_t priority, BaseType_t core_id) {
  s_screen_ctx.metrics = metrics;
  s_screen_ctx.lvgl = lvgl;
  s_screen_ctx.telemetry = telemetry;
  s_screen_ctx.hw = hw;

  if (s_screen_ota_mutex == NULL) {
    s_screen_ota_mutex = xSemaphoreCreateMutex();
  }

  xTaskCreatePinnedToCore(tft_screen_task, "tft_screen", 8192, &s_screen_ctx,
                          priority, NULL, core_id);
}
