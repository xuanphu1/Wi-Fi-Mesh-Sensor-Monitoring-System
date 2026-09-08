#include "ScreenManager.h"
#include "font8x8.h"

// Driver chính hãng Espressif esp_lcd và esp_lcd_ili9341
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"

#include "FOTAManager.h"
#include "LinkListData.h"
#include "MemoryManager.h"
#include "PowerManager.h"
#include "UartToNode.h"
#include "WSHandle.h"
#include "WifiManager.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "xpt2046_soft.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ScreenManager";

// Cấu hình chân phần cứng kết nối màn hình ILI9341
#define LCD_HOST               SPI3_HOST
#define LCD_PIN_NUM_SCLK       25
#define LCD_PIN_NUM_MOSI       26
#define LCD_PIN_NUM_MISO       -1
#define LCD_PIN_NUM_LCD_DC     27
#define LCD_PIN_NUM_LCD_RST    14
#define LCD_PIN_NUM_LCD_CS     12
#define LCD_PIN_NUM_BK_LIGHT   33

#define TFT_WIDTH              240
#define TFT_HEIGHT             320

// Bảng màu RGB565 tiêu chuẩn
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFD20
#define TFT_GREENYELLOW 0xB7E0

#define TFT_BG_MAIN     0x0000
#define TFT_CARD_BG     0x0862
#define TFT_CARD_BORDER 0x2125
#define TFT_TEXT_MUTED  0x8410
#define TFT_ACCENT_CYAN 0x07FF
#define TFT_ACCENT_GOLD 0xFDE0
#define TFT_BAR_BG      0x18C3

#define TFT_DMA_BUF_LINES      16
#define TFT_DMA_BUF_PIXELS     (TFT_WIDTH * TFT_DMA_BUF_LINES) // 240 * 16 = 3840 pixels
#define TFT_DMA_BUF_SIZE       (TFT_DMA_BUF_PIXELS * sizeof(uint16_t)) // 7680 bytes

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_refresh_done_sem = NULL;
static uint16_t *s_dma_buf = NULL;

static bool IRAM_ATTR tft_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                              esp_lcd_panel_io_event_data_t *edata,
                                              void *user_ctx) {
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
  if (s_screen_ota_mutex == NULL) {
    s_screen_ota_mutex = xSemaphoreCreateMutex();
  }
  if (s_screen_ota_mutex && xSemaphoreTake(s_screen_ota_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
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

static inline uint16_t swap16(uint16_t v) {
  return (v >> 8) | (v << 8);
}

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
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  // 4. Khởi tạo driver ILI9341 chính thức
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_PIN_NUM_LCD_RST,
      .rgb_endian = LCD_RGB_ENDIAN_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));

  // 5. Cài đặt định hướng đứng dọc (Portrait Inverted 240x320)
  // mirror_x = false: sửa triệt để lỗi chữ bị lật ngược ngang (soi gương)
  // mirror_y = true: giữ hướng xoay 180 độ theo thiết kế phần cứng hộp/vỏ (MADCTL = 0x88)
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, false, true));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel_handle, false));

  // 6. Bật hiển thị và đèn nền
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
  gpio_set_level(LCD_PIN_NUM_BK_LIGHT, 1);

  ESP_LOGI(TAG, "Official esp_lcd_ili9341 initialized (Portrait Inverted 240x320)");
  return ESP_OK;
}

static void tft_draw_bitmap_wait(int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end, const void *color_data) {
  if (!s_panel_handle || x_start >= x_end || y_start >= y_end || !color_data) return;
  if (s_refresh_done_sem) {
    xSemaphoreTake(s_refresh_done_sem, 0);
  }
  esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, color_data);
  if (s_refresh_done_sem) {
    xSemaphoreTake(s_refresh_done_sem, pdMS_TO_TICKS(50));
  }
}

static void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT || s_panel_handle == NULL || s_dma_buf == NULL) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > TFT_WIDTH) w = TFT_WIDTH - x;
  if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;
  if (w <= 0 || h <= 0) return;

  uint16_t swapped = swap16(color);
  int16_t max_lines = TFT_DMA_BUF_PIXELS / w;
  if (max_lines > h) max_lines = h;
  if (max_lines < 1) max_lines = 1;

  int total_pixels = w * max_lines;
  for (int i = 0; i < total_pixels; i++) {
    s_dma_buf[i] = swapped;
  }

  int16_t cur_y = y;
  while (cur_y < y + h) {
    int16_t lines = y + h - cur_y;
    if (lines > max_lines) lines = max_lines;
    tft_draw_bitmap_wait(x, cur_y, x + w, cur_y + lines, s_dma_buf);
    cur_y += lines;
  }
}

static void tft_fill_screen(uint16_t color) {
  tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

static void tft_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color) {
  tft_fill_rect(x, y, w, 1, color);
}

static void tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color) {
  tft_fill_rect(x, y, 1, h, color);
}

static void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  tft_draw_fast_h_line(x, y, w, color);
  tft_draw_fast_h_line(x, y + h - 1, w, color);
  tft_draw_fast_v_line(x, y, h, color);
  tft_draw_fast_v_line(x + w - 1, y, h, color);
}

static void tft_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size) {
  if (!str || !s_dma_buf || s_panel_handle == NULL) return;
  if (size < 1) size = 1;

  int16_t char_w = 8 * size;
  int16_t char_h = 8 * size;
  int max_chars_in_buf = TFT_DMA_BUF_PIXELS / (char_w * char_h);
  if (max_chars_in_buf < 1) max_chars_in_buf = 1;

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

    if (cur_x >= TFT_WIDTH) break;

    int chunk_chars = 0;
    const char *p = str;
    while (*p && *p != '\n' && chunk_chars < max_chars_in_buf) {
      if (cur_x + (chunk_chars + 1) * char_w > TFT_WIDTH) {
        break;
      }
      chunk_chars++;
      p++;
    }

    if (chunk_chars == 0) break;

    int16_t chunk_w = chunk_chars * char_w;
    int16_t chunk_h = char_h;

    for (int c_idx = 0; c_idx < chunk_chars; c_idx++) {
      char c = str[c_idx];
      if (c < 32 || c > 127) c = ' ';
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

    if (cur_x < TFT_WIDTH && cur_y < TFT_HEIGHT && cur_x + chunk_w > 0 && cur_y + chunk_h > 0) {
      tft_draw_bitmap_wait(cur_x, cur_y, cur_x + chunk_w, cur_y + chunk_h, s_dma_buf);
    }

    cur_x += chunk_w;
    str += chunk_chars;
  }
}

static void tft_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent,
                                  uint16_t fg_color, uint16_t bg_color, uint16_t border_color) {
  if (percent > 100) percent = 100;
  tft_draw_rect(x, y, w, h, border_color);

  int16_t inner_x = x + 2;
  int16_t inner_y = y + 2;
  int16_t inner_w = w - 4;
  int16_t inner_h = h - 4;
  if (inner_w <= 0 || inner_h <= 0) return;

  int16_t fill_w = (inner_w * percent) / 100;
  if (fill_w > 0) {
    tft_fill_rect(inner_x, inner_y, fill_w, inner_h, fg_color);
  }
  if (fill_w < inner_w) {
    tft_fill_rect(inner_x + fill_w, inner_y, inner_w - fill_w, inner_h, bg_color);
  }
}

static const char *get_weekday_name(uint8_t wday) {
  switch (wday) {
  case 0: return "Sun";
  case 1: return "Mon";
  case 2: return "Tue";
  case 3: return "Wed";
  case 4: return "Thu";
  case 5: return "Fri";
  case 6: return "Sat";
  default: return "---";
  }
}

static const char *s_month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void draw_text_cached(int16_t x, int16_t y, char *cache, size_t cache_sz,
                             const char *new_text, uint16_t fg, uint16_t bg,
                             uint8_t size) {
  if (strcmp(cache, new_text) != 0) {
    tft_draw_string(x, y, new_text, fg, bg, size);
    strncpy(cache, new_text, cache_sz - 1);
    cache[cache_sz - 1] = '\0';
  }
}

static void tft_draw_segmented_bar(int16_t x, int16_t y, int16_t seg_w, int16_t seg_h,
                                   int16_t seg_gap, uint8_t total_segs, uint8_t filled_segs,
                                   uint16_t fill_color, uint16_t empty_color, uint16_t border_color) {
  for (uint8_t i = 0; i < total_segs; i++) {
    int16_t cur_x = x + i * (seg_w + seg_gap);
    uint16_t color = (i < filled_segs) ? fill_color : empty_color;
    tft_fill_rect(cur_x, y, seg_w, seg_h, color);
    if (border_color != 0) {
      tft_draw_rect(cur_x, y, seg_w, seg_h, border_color);
    }
  }
}

static void draw_target_button(websocket_target_t target) {
  bool is_server = (target == WEBSOCKET_TARGET_SERVER);
  uint16_t bg_col = is_server ? 0x0015 : 0x2960;       // Dark Navy Blue vs Dark Olive
  uint16_t border_col = is_server ? TFT_CYAN : TFT_YELLOW;
  uint16_t text_col = is_server ? TFT_CYAN : TFT_YELLOW;
  uint16_t sub_col = is_server ? TFT_ACCENT_GOLD : TFT_WHITE;

  // Nút cảm ứng Card 1: X = 142..234 (rộng 92px), Y = 47..70 (cao 24px)
  tft_fill_rect(142, 47, 92, 24, bg_col);
  tft_draw_rect(142, 47, 92, 24, border_col);
  if (is_server) {
    tft_draw_string(153, 50, "* SERVER *", text_col, bg_col, 1);
  } else {
    tft_draw_string(156, 50, "* LOCAL *", text_col, bg_col, 1);
  }
  tft_draw_string(148, 60, "[TOUCH CHG]", sub_col, bg_col, 1);
}

static void render_static_dashboard(void) {
  tft_fill_screen(TFT_BG_MAIN);

  // 1. Top Header (0,0 to 240, 34)
  tft_fill_rect(0, 0, TFT_WIDTH, 34, TFT_CARD_BG);
  tft_draw_fast_h_line(0, 34, TFT_WIDTH, TFT_CARD_BORDER);
  tft_draw_string(4, 2, "MRKOI GATEWAY", TFT_ACCENT_GOLD, TFT_CARD_BG, 1);

  // 2. Card 1: [GATEWAY & NETWORK] (2, 36 to 238, 92)
  tft_draw_rect(2, 36, 236, 56, TFT_CARD_BORDER);
  tft_draw_string(6, 38, "[GATEWAY & NETWORK]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 3. Card 2: [SYSTEM PERFORMANCE] (2, 94 to 238, 156)
  tft_draw_rect(2, 94, 236, 62, TFT_CARD_BORDER);
  tft_draw_string(6, 96, "[SYSTEM PERFORMANCE]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 4. Card 3: [MESH NETWORK STATUS] (2, 158 to 238, 224)
  tft_draw_rect(2, 158, 236, 66, TFT_CARD_BORDER);
  tft_draw_string(6, 160, "[MESH NETWORK STATUS]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 5. Card 4: [TRAFFIC & THROUGHPUT] (2, 226 to 238, 288)
  tft_draw_rect(2, 226, 236, 62, TFT_CARD_BORDER);
  tft_draw_string(6, 228, "[TRAFFIC & THROUGHPUT]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 6. Footer Status Line (0, 290 to 240, 320)
  tft_draw_fast_h_line(0, 290, TFT_WIDTH, TFT_CARD_BORDER);
  tft_draw_string(6, 292, "SYS STATUS: NORMAL ALL SERVICES OK", TFT_GREEN, TFT_BG_MAIN, 1);
  // Ô nút cảm ứng lớn ở Footer
  tft_fill_rect(4, 303, 232, 15, 0x18E3);
  tft_draw_rect(4, 303, 232, 15, TFT_ACCENT_GOLD);
  tft_draw_string(12, 307, "[TOUCH: TOGGLE SERVER <-> LOCAL]", TFT_ACCENT_GOLD, 0x18E3, 1);
}

static void tft_screen_task(void *arg) {
  screen_ctx_t *ctx = (screen_ctx_t *)arg;
  ui_metrics_t m = {0};

  // Khởi tạo phần cứng màn hình qua driver chính hãng esp_lcd_ili9341
  ESP_ERROR_CHECK(tft_hardware_init());

  // Khởi tạo cảm ứng soft SPI touch xpt2046
  xpt2046_soft_init();

  render_static_dashboard();

  // Cache buffers cho các nhãn động
  char c_wifi[16] = "";
  char c_ws[16] = "";
  char c_time[16] = "";
  char c_date[24] = "";
  char c_bat[16] = "";
  char c_uptime[20] = "";
  char c_vbat[16] = "";
  char c_ver_gw[16] = "";

  char c_ip[28] = "";
  char c_target[20] = "";
  char c_mac[28] = "";
  char c_rec[16] = "";
  char c_softap[32] = "";

  char c_fps[16] = "";
  char c_cpu[16] = "";
  char c_rest_cpu[16] = "";
  char c_ram[16] = "";
  char c_rest_ram[28] = "";
  char c_sd[36] = "";

  char c_nodes[20] = "";
  char c_weak_conn[20] = "";
  char c_weak_bat[32] = "";
  char c_node_ver[20] = "";
  char c_topol[36] = "";
  char c_root_mode[36] = "";

  char c_throughput[32] = "";
  char c_queue[24] = "";
  char c_rx_rate[24] = "";
  char c_tx_rate[24] = "";
  char c_latency[20] = "";
  char c_pipe[36] = "";

  bool ota_overlay_active = false;
  uint8_t last_ota_percent = 255;
  char c_ota_status[96] = "";

  TickType_t last_touch_check = 0;
  TickType_t last_rate_tick = 0;
  uint32_t last_rx = 0;
  uint32_t last_tx = 0;
  uint32_t rx_rate = 0;
  uint32_t tx_rate = 0;

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
    if (s_screen_ota_mutex && xSemaphoreTake(s_screen_ota_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (fota_is_running()) {
        s_screen_ota.active = true;
        s_screen_ota.percent = fota_get_progress_percent();
        if (s_screen_ota.target[0] == '\0') {
          strncpy(s_screen_ota.target, "Gateway", sizeof(s_screen_ota.target) - 1);
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

        tft_fill_screen(TFT_BG_MAIN);

        // Header bar (Maroon / deep red with orange border)
        tft_fill_rect(0, 0, TFT_WIDTH, 36, 0x4800);
        tft_draw_fast_h_line(0, 36, TFT_WIDTH, TFT_ORANGE);
        tft_draw_string(20, 13, "*** FIRMWARE OTA UPGRADE ***", TFT_YELLOW, 0x4800, 1);

        // Center card
        tft_fill_rect(8, 44, 224, 230, TFT_CARD_BG);
        tft_draw_rect(8, 44, 224, 230, TFT_ORANGE);
        tft_draw_rect(9, 45, 222, 228, TFT_CARD_BORDER);

        char target_str[64];
        snprintf(target_str, sizeof(target_str), "Target  : %s",
                 ota_snap.target[0] ? ota_snap.target : "Gateway");
        tft_draw_string(18, 56, target_str, TFT_WHITE, TFT_CARD_BG, 1);

        char ver_str[64];
        snprintf(ver_str, sizeof(ver_str), "Version : %s",
                 ota_snap.version[0] ? ota_snap.version : "---");
        tft_draw_string(18, 72, ver_str, TFT_ACCENT_CYAN, TFT_CARD_BG, 1);

        tft_draw_fast_h_line(15, 88, 210, TFT_CARD_BORDER);

        tft_draw_string(18, 185, "Partition : [ota_1] Active", TFT_TEXT_MUTED, TFT_CARD_BG, 1);
        tft_draw_string(18, 202, "Warning   : DO NOT POWER OFF!", TFT_ORANGE, TFT_CARD_BG, 1);
        tft_draw_string(18, 219, "Free Heap : > 100 KB OK", TFT_GREEN, TFT_CARD_BG, 1);
        tft_draw_string(18, 238, "Network   : WiFi STA / WS Connected", TFT_TEXT_MUTED, TFT_CARD_BG, 1);

        // Footer
        tft_draw_fast_h_line(0, 290, TFT_WIDTH, TFT_CARD_BORDER);
        tft_draw_string(18, 298, "Gateway Firmware Over-The-Air", TFT_TEXT_MUTED, TFT_BG_MAIN, 1);
      }

      if (ota_snap.percent != last_ota_percent) {
        last_ota_percent = ota_snap.percent;
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%3u%%", ota_snap.percent);
        tft_draw_string(84, 104, pct_str, TFT_GREENYELLOW, TFT_CARD_BG, 3);
        tft_draw_progress_bar(18, 140, 204, 18, ota_snap.percent, TFT_GREEN, TFT_BAR_BG, TFT_WHITE);
      }

      char new_status[96];
      snprintf(new_status, sizeof(new_status), "Status  : %-20.20s",
               ota_snap.detail[0] ? ota_snap.detail : "In Progress...");
      if (strcmp(c_ota_status, new_status) != 0) {
        tft_draw_string(18, 166, new_status, TFT_ACCENT_CYAN, TFT_CARD_BG, 1);
        strncpy(c_ota_status, new_status, sizeof(c_ota_status) - 1);
      }

      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (ota_overlay_active) {
      ota_overlay_active = false;
      render_static_dashboard();
      c_wifi[0] = '\0';
      c_ws[0] = '\0';
      c_time[0] = '\0';
      c_date[0] = '\0';
      c_bat[0] = '\0';
      c_uptime[0] = '\0';
      c_vbat[0] = '\0';
      c_ver_gw[0] = '\0';
      c_ip[0] = '\0';
      c_target[0] = '\0';
      c_mac[0] = '\0';
      c_rec[0] = '\0';
      c_softap[0] = '\0';
      c_fps[0] = '\0';
      c_cpu[0] = '\0';
      c_rest_cpu[0] = '\0';
      c_ram[0] = '\0';
      c_rest_ram[0] = '\0';
      c_sd[0] = '\0';
      c_nodes[0] = '\0';
      c_weak_conn[0] = '\0';
      c_weak_bat[0] = '\0';
      c_node_ver[0] = '\0';
      c_topol[0] = '\0';
      c_root_mode[0] = '\0';
      c_throughput[0] = '\0';
      c_queue[0] = '\0';
      c_rx_rate[0] = '\0';
      c_tx_rate[0] = '\0';
      c_latency[0] = '\0';
      c_pipe[0] = '\0';
    }

    TickType_t now_tick = xTaskGetTickCount();

    // Tính toán TX/RX packet rate mỗi giây
    if ((now_tick - last_rate_tick) >= pdMS_TO_TICKS(1000)) {
      last_rate_tick = now_tick;
      if (ctx && ctx->telemetry) {
        uint32_t cur_rx = ctx->telemetry->rx_packet_count;
        uint32_t cur_tx = ctx->telemetry->tx_packet_count;
        rx_rate = cur_rx - last_rx;
        tx_rate = cur_tx - last_tx;
        last_rx = cur_rx;
        last_tx = cur_tx;
      }
    }

    // Xử lý cảm ứng chạm (Touch handler)
    static bool s_touch_was_pressed = false;
    static TickType_t s_last_toggle_tick = 0;

    if ((now_tick - last_touch_check) >= pdMS_TO_TICKS(80)) {
      last_touch_check = now_tick;
      int16_t tx = 0, ty = 0;
      bool is_touched = xpt2046_soft_poll(&tx, &ty);

      if (is_touched) {
        // Vùng nút cảm ứng:
        // 1. Nút góc trên phải Card 1: X = 135..238, Y = 42..76
        // 2. Thanh nút Footer: Y >= 285..320
        bool hit_card1_btn = (tx >= 135 && tx <= 238 && ty >= 42 && ty <= 76);
        bool hit_footer_btn = (ty >= 285 && ty <= 320);

        if (hit_card1_btn || hit_footer_btn) {
          if (!s_touch_was_pressed && (now_tick - s_last_toggle_tick) >= pdMS_TO_TICKS(400)) {
            s_last_toggle_tick = now_tick;
            websocket_target_t cur = websocket_get_selected_target();
            websocket_target_t new_t = (cur == WEBSOCKET_TARGET_SERVER)
                                           ? WEBSOCKET_TARGET_LOCAL
                                           : WEBSOCKET_TARGET_SERVER;
            websocket_select_target(new_t);
            draw_target_button(new_t);
            ESP_LOGI("ScreenTouch",
                     ">>> [TOUCH CLICK] At (%d, %d) [%s] -> Toggled Target to %s",
                     tx, ty, hit_card1_btn ? "Card1 Button" : "Footer Button",
                     (new_t == WEBSOCKET_TARGET_SERVER) ? "SERVER" : "LOCAL");
          }
        } else {
          ESP_LOGI("ScreenTouch", "Touch at Screen(X=%d, Y=%d) - Outside toggle buttons", tx, ty);
        }
        s_touch_was_pressed = true;
      } else {
        s_touch_was_pressed = false;
      }
    }

    char buf[64];

    // ===== 1. TOP HEADER =====
    bool wifi_ok = is_wifi_connected();
    draw_text_cached(118, 2, c_wifi, sizeof(c_wifi), wifi_ok ? "[WIFI:OK]" : "[NO-WIFI]",
                     wifi_ok ? TFT_GREEN : TFT_RED, TFT_CARD_BG, 1);

    bool ws_ok = websocket_is_connected();
    draw_text_cached(190, 2, c_ws, sizeof(c_ws), ws_ok ? "[WS:OK]" : "[WS:--]",
                     ws_ok ? TFT_GREEN : TFT_RED, TFT_CARD_BG, 1);

    if (m.rtc_ok) {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", m.rtc_time.tm_hour,
               m.rtc_time.tm_min, m.rtc_time.tm_sec);
      draw_text_cached(4, 13, c_time, sizeof(c_time), buf, TFT_WHITE, TFT_CARD_BG, 1);

      const char *mon = (m.rtc_time.tm_mon >= 0 && m.rtc_time.tm_mon <= 11)
                            ? s_month_names[m.rtc_time.tm_mon]
                            : "---";
      snprintf(buf, sizeof(buf), "%s,%02d %s",
               get_weekday_name((uint8_t)m.rtc_time.tm_wday),
               m.rtc_time.tm_mday, mon);
      draw_text_cached(74, 13, c_date, sizeof(c_date), buf, TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    } else {
      draw_text_cached(4, 13, c_time, sizeof(c_time), "00:00:00", TFT_WHITE, TFT_CARD_BG, 1);
      draw_text_cached(74, 13, c_date, sizeof(c_date), "Syncing RTC..", TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    }

    int bat_pct = 0;
    if (ctx && ctx->hw) {
      bat_pct = power_manager_battery_get_percent(ctx->hw, NULL, NULL);
    }
    snprintf(buf, sizeof(buf), "BAT:%2d%%", bat_pct);
    draw_text_cached(152, 13, c_bat, sizeof(c_bat), buf,
                     (bat_pct > 20) ? TFT_CYAN : TFT_RED, TFT_CARD_BG, 1);

    uint8_t bat_segs = (bat_pct >= 80) ? 5 : ((bat_pct >= 60) ? 4 : ((bat_pct >= 40) ? 3 : ((bat_pct >= 20) ? 2 : ((bat_pct > 0) ? 1 : 0))));
    tft_draw_segmented_bar(210, 13, 4, 7, 1, 5, bat_segs, (bat_pct > 20) ? TFT_GREEN : TFT_RED, TFT_BAR_BG, TFT_CARD_BORDER);

    float days_f = (float)m.uptime_s / 86400.0f;
    snprintf(buf, sizeof(buf), "Up:%.2fd", (double)days_f);
    draw_text_cached(4, 23, c_uptime, sizeof(c_uptime), buf, TFT_TEXT_MUTED, TFT_CARD_BG, 1);

    float vbat = (float)m.battery_pack_mv / 1000.0f;
    snprintf(buf, sizeof(buf), "Vbat:%.2fV", (double)vbat);
    draw_text_cached(74, 23, c_vbat, sizeof(c_vbat), buf, TFT_TEXT_MUTED, TFT_CARD_BG, 1);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    snprintf(buf, sizeof(buf), "v%-6s", app_desc ? app_desc->version : "0.0.2");
    draw_text_cached(185, 23, c_ver_gw, sizeof(c_ver_gw), buf, TFT_ACCENT_GOLD, TFT_CARD_BG, 1);

    // ===== 2. CARD 1: [GATEWAY & NETWORK] =====
    char ip_buf[20] = "0.0.0.0";
    wifi_manager_get_ip_info(ip_buf, sizeof(ip_buf));
    snprintf(buf, sizeof(buf), "IP : %-15s", ip_buf);
    draw_text_cached(6, 49, c_ip, sizeof(c_ip), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    websocket_target_t target = websocket_get_selected_target();
    const char *target_name = (target == WEBSOCKET_TARGET_SERVER) ? "SERVER" : "LOCAL";
    if (strcmp(c_target, target_name) != 0) {
      draw_target_button(target);
      strncpy(c_target, target_name, sizeof(c_target) - 1);
      c_target[sizeof(c_target) - 1] = '\0';
    }

    char mac_buf[24] = "00:00:00:00:00:00";
    wifi_manager_get_mac_info(mac_buf, sizeof(mac_buf));
    snprintf(buf, sizeof(buf), "MAC: %-17s", mac_buf);
    draw_text_cached(6, 61, c_mac, sizeof(c_mac), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "SoftAP: ROOT_AP (Ch:11)");
    draw_text_cached(6, 75, c_softap, sizeof(c_softap), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "WS Rec:%-3lu", (unsigned long)websocket_get_reconnect_count());
    draw_text_cached(155, 75, c_rec, sizeof(c_rec), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    // ===== 3. CARD 2: [SYSTEM PERFORMANCE] =====
    snprintf(buf, sizeof(buf), "FPS:%-2lu", (unsigned long)m.fps);
    draw_text_cached(180, 96, c_fps, sizeof(c_fps), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    uint32_t cpu_pct = m.cpu_load_permille / 10U;
    uint32_t rest_cpu = 100U - (cpu_pct > 100U ? 100U : cpu_pct);
    snprintf(buf, sizeof(buf), "CPU: %2lu%%", (unsigned long)cpu_pct);
    draw_text_cached(6, 108, c_cpu, sizeof(c_cpu), buf, TFT_WHITE, TFT_BG_MAIN, 1);
    tft_draw_progress_bar(70, 109, 55, 6, (uint8_t)cpu_pct, TFT_CYAN, TFT_BAR_BG, TFT_CARD_BORDER);
    snprintf(buf, sizeof(buf), "Rest:%2lu%%", (unsigned long)rest_cpu);
    draw_text_cached(135, 108, c_rest_cpu, sizeof(c_rest_cpu), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    uint8_t mem_pct = 0;
    uint32_t free_kb = 0;
    uint32_t total_kb = 0;
    memory_manager_get_usage(&free_kb, &total_kb, &mem_pct);
    uint8_t rest_ram = 100U - (mem_pct > 100U ? 100U : mem_pct);
    snprintf(buf, sizeof(buf), "RAM: %2u%%", mem_pct);
    draw_text_cached(6, 121, c_ram, sizeof(c_ram), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    uint8_t ram_segs = (mem_pct >= 95) ? 6 : ((mem_pct >= 85) ? 5 : ((mem_pct >= 65) ? 4 : ((mem_pct >= 45) ? 3 : ((mem_pct >= 25) ? 2 : ((mem_pct >= 10) ? 1 : 0)))));
    tft_draw_segmented_bar(68, 121, 4, 7, 1, 6, ram_segs, TFT_GREEN, TFT_BAR_BG, TFT_CARD_BORDER);

    snprintf(buf, sizeof(buf), "R:%2u%% Free:%luk", rest_ram, (unsigned long)free_kb);
    draw_text_cached(105, 121, c_rest_ram, sizeof(c_rest_ram), buf, TFT_GREENYELLOW, TFT_BG_MAIN, 1);

    if (m.sd_total_kb == 0) {
      snprintf(buf, sizeof(buf), "SD Card: No SD Card Mounted   ");
    } else {
      uint32_t mb = m.sd_total_kb / 1024;
      uint32_t free_mb = m.sd_free_kb / 1024;
      snprintf(buf, sizeof(buf), "SD : %lu.%02luGB (%luMB Free) ", (unsigned long)(mb / 1000),
               (unsigned long)((mb % 1000) / 10), (unsigned long)free_mb);
    }
    draw_text_cached(6, 134, c_sd, sizeof(c_sd), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    // ===== 4. CARD 3: [MESH NETWORK STATUS] =====
    draw_text_cached(155, 160, c_node_ver, sizeof(c_node_ver), "Node:v0.1", TFT_ACCENT_GOLD, TFT_BG_MAIN, 1);

    size_t node_count = link_list_data_get_count();
    snprintf(buf, sizeof(buf), "Connected Nodes : %-2u", (unsigned)node_count);
    draw_text_cached(6, 172, c_nodes, sizeof(c_nodes), buf, TFT_GREEN, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "Weak Conn : 0");
    draw_text_cached(135, 172, c_weak_conn, sizeof(c_weak_conn), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    size_t low_bat_nodes = link_list_data_get_low_battery_count();
    snprintf(buf, sizeof(buf), "Weak Bat Nodes  : %-2u", (unsigned)low_bat_nodes);
    draw_text_cached(6, 185, c_weak_bat, sizeof(c_weak_bat), buf,
                     (low_bat_nodes > 0) ? TFT_RED : TFT_GREEN, TFT_BG_MAIN, 1);

    uint8_t max_level = link_list_data_get_max_level();
    snprintf(buf, sizeof(buf), "Max Mesh Level  : Level %-2u", (unsigned)max_level);
    draw_text_cached(6, 198, c_topol, sizeof(c_topol), buf, TFT_CYAN, TFT_BG_MAIN, 1);

    draw_text_cached(6, 210, c_root_mode, sizeof(c_root_mode), "Root Coordinator: ONLINE (ACTIVE)", TFT_GREEN, TFT_BG_MAIN, 1);

    // ===== 5. CARD 4: [TRAFFIC & THROUGHPUT] =====
    draw_text_cached(145, 228, c_latency, sizeof(c_latency), "Latency:<5ms", TFT_GREEN, TFT_BG_MAIN, 1);

    uint32_t tot_rx = (ctx && ctx->telemetry) ? ctx->telemetry->rx_packet_count : 0;
    snprintf(buf, sizeof(buf), "Throughput: %-6lu pkts", (unsigned long)tot_rx);
    draw_text_cached(6, 240, c_throughput, sizeof(c_throughput), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    uint32_t q_used = 0, q_total = 0;
    uart_to_node_get_queue_status(&q_used, &q_total);
    snprintf(buf, sizeof(buf), "Queue:%lu/%lu", (unsigned long)q_used, (unsigned long)q_total);
    draw_text_cached(155, 240, c_queue, sizeof(c_queue), buf, (q_used > 20) ? TFT_ORANGE : TFT_YELLOW, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "RX Rate   : %4lu pkt/s", (unsigned long)rx_rate);
    draw_text_cached(6, 253, c_rx_rate, sizeof(c_rx_rate), buf, TFT_WHITE, TFT_BG_MAIN, 1);
    uint8_t rx_bar_pct = rx_rate > 50 ? 100 : (rx_rate * 2);
    tft_draw_progress_bar(165, 254, 55, 6, rx_bar_pct, TFT_CYAN, TFT_BAR_BG, TFT_CARD_BORDER);

    snprintf(buf, sizeof(buf), "TX Rate   : %4lu pkt/s", (unsigned long)tx_rate);
    draw_text_cached(6, 265, c_tx_rate, sizeof(c_tx_rate), buf, TFT_WHITE, TFT_BG_MAIN, 1);
    uint8_t tx_bar_pct = tx_rate > 50 ? 100 : (tx_rate * 2);
    tft_draw_progress_bar(165, 266, 55, 6, tx_bar_pct, TFT_GREENYELLOW, TFT_BAR_BG, TFT_CARD_BORDER);

    draw_text_cached(6, 277, c_pipe, sizeof(c_pipe), "Pipe: UART2 (115200) <-> WS Cloud", TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    vTaskDelay(pdMS_TO_TICKS(200));
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
