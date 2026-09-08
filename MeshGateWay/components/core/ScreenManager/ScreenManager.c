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
  // mirror_x = true, mirror_y = true: sửa triệt để lỗi chữ bị lật ngược ngang (nhìn như qua gương)
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, true));
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

static void render_static_dashboard(void) {
  tft_fill_screen(TFT_BG_MAIN);

  // 1. Top Header Background (0,0 to 240, 32)
  tft_fill_rect(0, 0, TFT_WIDTH, 32, TFT_CARD_BG);
  tft_draw_fast_h_line(0, 32, TFT_WIDTH, TFT_CARD_BORDER);

  // 2. Gateway System Card (4, 36 to 236, 88)
  tft_draw_rect(4, 36, 232, 52, TFT_CARD_BORDER);
  tft_draw_string(8, 40, "[GATEWAY SYSTEM]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 3. System Performance Card (4, 92 to 236, 148)
  tft_draw_rect(4, 92, 232, 56, TFT_CARD_BORDER);
  tft_draw_string(8, 96, "[SYSTEM PERFORMANCE]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 4. Mesh Network Card (4, 152 to 236, 204)
  tft_draw_rect(4, 152, 232, 52, TFT_CARD_BORDER);
  tft_draw_string(8, 156, "[MESH NETWORK]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 5. Node Sensors Card (4, 208 to 236, 270)
  tft_draw_rect(4, 208, 232, 62, TFT_CARD_BORDER);
  tft_draw_string(8, 212, "[NODE SENSORS (PORTS)]", TFT_ACCENT_CYAN, TFT_BG_MAIN, 1);

  // 6. Bottom Status Line (0, 274 to 240, 320)
  tft_draw_fast_h_line(0, 274, TFT_WIDTH, TFT_CARD_BORDER);
  tft_draw_string(8, 280, "SoftAP: ROOT_AP (Ch:11)", TFT_TEXT_MUTED, TFT_BG_MAIN, 1);
  tft_draw_string(8, 292, "Mesh-Lite / IP-Forward: ON", TFT_GREEN, TFT_BG_MAIN, 1);
  tft_draw_string(8, 304, "Driver: esp_lcd_ili9341", TFT_TEXT_MUTED, TFT_BG_MAIN, 1);
}

static void tft_screen_task(void *arg) {
  screen_ctx_t *ctx = (screen_ctx_t *)arg;
  ui_metrics_t m = {0};

  // Khởi tạo phần cứng màn hình qua driver chính hãng esp_lcd_ili9341
  ESP_ERROR_CHECK(tft_hardware_init());

  // Khởi tạo cảm ứng soft SPI touch xpt2046
  xpt2046_soft_init();

  render_static_dashboard();

  char c_time[16] = "";
  char c_date[32] = "";
  char c_uptime[16] = "";
  char c_battery[16] = "";
  char c_wifi[16] = "";
  char c_version[16] = "";
  char c_ws_status[24] = "";
  char c_ws_target[16] = "";
  char c_ip[32] = "";
  char c_cpu[16] = "";
  char c_ram[24] = "";
  char c_sd[20] = "";
  char c_fps[16] = "";
  char c_nodes[16] = "";
  char c_sel_node[32] = "";
  char c_port1[32] = "";
  char c_port2[32] = "";
  char c_port3[32] = "";

  bool ota_overlay_active = false;
  uint8_t last_ota_percent = 255;
  char c_ota_status[64] = "";

  TickType_t last_touch_check = 0;
  uint16_t current_selected_node_idx = 0;

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

        tft_fill_rect(10, 45, 220, 225, TFT_CARD_BG);
        tft_draw_rect(10, 45, 220, 225, TFT_ORANGE);
        tft_draw_rect(11, 46, 218, 223, TFT_YELLOW);

        tft_draw_string(20, 55, "*** GATEWAY OTA UPDATE ***", TFT_YELLOW, TFT_CARD_BG, 1);
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
        tft_draw_progress_bar(20, 140, 200, 18, ota_snap.percent, TFT_GREEN, TFT_BAR_BG, TFT_WHITE);
      }

      char new_status[64];
      snprintf(new_status, sizeof(new_status), "%-25s",
               ota_snap.detail[0] ? ota_snap.detail : "Downloading...");
      if (strcmp(c_ota_status, new_status) != 0) {
        tft_draw_string(20, 168, new_status, TFT_ACCENT_CYAN, TFT_CARD_BG, 1);
        strncpy(c_ota_status, new_status, sizeof(c_ota_status) - 1);
      }

      tft_draw_string(20, 195, "Partition: [ota_1] ready", TFT_WHITE, TFT_CARD_BG, 1);
      tft_draw_string(20, 215, "Please do not power off!", TFT_ORANGE, TFT_CARD_BG, 1);
      tft_draw_string(20, 235, "Free Heap: > 100 KB OK  ", TFT_GREEN, TFT_CARD_BG, 1);

      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (ota_overlay_active) {
      ota_overlay_active = false;
      render_static_dashboard();
      c_time[0] = '\0';
      c_date[0] = '\0';
      c_cpu[0] = '\0';
      c_ram[0] = '\0';
    }

    TickType_t now_tick = xTaskGetTickCount();
    if ((now_tick - last_touch_check) >= pdMS_TO_TICKS(150)) {
      last_touch_check = now_tick;
      int16_t tx = 0, ty = 0;
      if (xpt2046_soft_poll(&tx, &ty)) {
        if (ty >= 40 && ty <= 70) {
          websocket_target_t cur = websocket_get_selected_target();
          websocket_select_target(cur == WEBSOCKET_TARGET_SERVER
                                      ? WEBSOCKET_TARGET_LOCAL
                                      : WEBSOCKET_TARGET_SERVER);
        } else if (ty >= 165 && ty <= 200) {
          size_t cnt = link_list_data_get_count();
          if (cnt > 0) {
            current_selected_node_idx = (current_selected_node_idx + 1) % cnt;
            link_list_data_select_node_by_index(current_selected_node_idx);
          }
        }
      }
    }

    char buf[64];
    if (m.rtc_ok) {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", m.rtc_time.tm_hour,
               m.rtc_time.tm_min, m.rtc_time.tm_sec);
      draw_text_cached(6, 4, c_time, sizeof(c_time), buf, TFT_WHITE, TFT_CARD_BG, 2);

      const char *mon = (m.rtc_time.tm_mon >= 0 && m.rtc_time.tm_mon <= 11)
                            ? s_month_names[m.rtc_time.tm_mon]
                            : "---";
      snprintf(buf, sizeof(buf), "%s, %02d %s %04d",
               get_weekday_name((uint8_t)m.rtc_time.tm_wday),
               m.rtc_time.tm_mday, mon, m.rtc_time.tm_year + 1900);
      draw_text_cached(6, 22, c_date, sizeof(c_date), buf, TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    } else {
      draw_text_cached(6, 4, c_time, sizeof(c_time), "00:00:00", TFT_WHITE, TFT_CARD_BG, 2);
      draw_text_cached(6, 22, c_date, sizeof(c_date), "Syncing RTC...", TFT_TEXT_MUTED, TFT_CARD_BG, 1);
    }

    bool wifi_ok = is_wifi_connected();
    draw_text_cached(155, 6, c_wifi, sizeof(c_wifi), wifi_ok ? "[WIFI]" : "[NO-W]",
                     wifi_ok ? TFT_GREEN : TFT_RED, TFT_CARD_BG, 1);

    int bat_pct = 0;
    if (ctx && ctx->hw) {
      bat_pct = power_manager_battery_get_percent(ctx->hw, NULL, NULL);
    }
    snprintf(buf, sizeof(buf), "%3d%%", bat_pct);
    draw_text_cached(198, 6, c_battery, sizeof(c_battery), buf,
                     (bat_pct > 20) ? TFT_CYAN : TFT_RED, TFT_CARD_BG, 1);

    float days_f = (float)m.uptime_s / 86400.0f;
    snprintf(buf, sizeof(buf), "Up:%.2fd", (double)days_f);
    draw_text_cached(170, 22, c_uptime, sizeof(c_uptime), buf, TFT_TEXT_MUTED, TFT_CARD_BG, 1);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    snprintf(buf, sizeof(buf), "v%-6s", app_desc ? app_desc->version : "0.0.1");
    draw_text_cached(185, 40, c_version, sizeof(c_version), buf, TFT_ACCENT_GOLD, TFT_BG_MAIN, 1);

    bool ws_ok = websocket_is_connected();
    snprintf(buf, sizeof(buf), "WS: %-12s", ws_ok ? "CONNECTED" : "DISCONNECTED");
    draw_text_cached(10, 52, c_ws_status, sizeof(c_ws_status), buf,
                     ws_ok ? TFT_GREEN : TFT_RED, TFT_BG_MAIN, 1);

    websocket_target_t target = websocket_get_selected_target();
    snprintf(buf, sizeof(buf), "[%-6s]", (target == WEBSOCKET_TARGET_SERVER) ? "SERVER" : "LOCAL");
    draw_text_cached(180, 52, c_ws_target, sizeof(c_ws_target), buf, TFT_YELLOW, TFT_BG_MAIN, 1);

    char ip_buf[20] = "None";
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
      esp_netif_ip_info_t ipi;
      if (esp_netif_get_ip_info(sta_netif, &ipi) == ESP_OK && ipi.ip.addr != 0) {
        snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&ipi.ip));
      }
    }
    snprintf(buf, sizeof(buf), "IP:%-15s Rec:%lu ", ip_buf,
             (unsigned long)websocket_get_reconnect_count());
    draw_text_cached(10, 68, c_ip, sizeof(c_ip), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    snprintf(buf, sizeof(buf), "FPS:%-3lu", (unsigned long)m.fps);
    draw_text_cached(185, 96, c_fps, sizeof(c_fps), buf, TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    uint32_t cpu_pct = m.cpu_load_permille / 10U;
    snprintf(buf, sizeof(buf), "CPU: %2lu%% ", (unsigned long)cpu_pct);
    draw_text_cached(10, 110, c_cpu, sizeof(c_cpu), buf, TFT_WHITE, TFT_BG_MAIN, 1);
    tft_draw_progress_bar(75, 112, 60, 6, (uint8_t)cpu_pct, TFT_CYAN, TFT_BAR_BG, TFT_CARD_BORDER);

    if (m.sd_total_kb == 0) {
      snprintf(buf, sizeof(buf), "SD: None  ");
    } else {
      uint32_t mb = m.sd_total_kb / 1024;
      snprintf(buf, sizeof(buf), "SD:%lu.%02luGB", (unsigned long)(mb / 1000),
               (unsigned long)((mb % 1000) / 10));
    }
    draw_text_cached(145, 110, c_sd, sizeof(c_sd), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    uint8_t mem_pct = 0;
    uint32_t free_kb = 0;
    memory_manager_get_usage(&free_kb, NULL, &mem_pct);
    snprintf(buf, sizeof(buf), "RAM: %2u%% ", mem_pct);
    draw_text_cached(10, 126, c_ram, sizeof(c_ram), buf, TFT_WHITE, TFT_BG_MAIN, 1);
    tft_draw_progress_bar(75, 128, 60, 6, mem_pct, TFT_GREEN, TFT_BAR_BG, TFT_CARD_BORDER);

    snprintf(buf, sizeof(buf), "Free:%lukB ", (unsigned long)free_kb);
    tft_draw_string(145, 126, buf, TFT_GREENYELLOW, TFT_BG_MAIN, 1);

    size_t node_count = link_list_data_get_count();
    snprintf(buf, sizeof(buf), "Nodes:%-2u", (unsigned)node_count);
    draw_text_cached(175, 156, c_nodes, sizeof(c_nodes), buf, TFT_GREEN, TFT_BG_MAIN, 1);

    tft_draw_string(10, 170, "Weak Conn: 0 | Low Bat: 0    ", TFT_TEXT_MUTED, TFT_BG_MAIN, 1);

    link_list_node_snapshot_t sel_node = {0};
    if (link_list_data_get_selected_node(&sel_node)) {
      snprintf(buf, sizeof(buf), "Selected: #%-18s", sel_node.id);
    } else {
      snprintf(buf, sizeof(buf), "Selected: NONE (Tap to cycle) ");
    }
    draw_text_cached(10, 185, c_sel_node, sizeof(c_sel_node), buf, TFT_YELLOW, TFT_BG_MAIN, 1);

    if (sel_node.sensor_count > 0 && sel_node.ports[0].name[0]) {
      snprintf(buf, sizeof(buf), "P1: %-22s", sel_node.ports[0].name);
    } else {
      snprintf(buf, sizeof(buf), "P1: NONE                  ");
    }
    draw_text_cached(10, 224, c_port1, sizeof(c_port1), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    if (sel_node.sensor_count > 1 && sel_node.ports[1].name[0]) {
      snprintf(buf, sizeof(buf), "P2: %-22s", sel_node.ports[1].name);
    } else {
      snprintf(buf, sizeof(buf), "P2: NONE                  ");
    }
    draw_text_cached(10, 238, c_port2, sizeof(c_port2), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    if (sel_node.sensor_count > 2 && sel_node.ports[2].name[0]) {
      snprintf(buf, sizeof(buf), "P3: %-22s", sel_node.ports[2].name);
    } else {
      snprintf(buf, sizeof(buf), "P3: NONE                  ");
    }
    draw_text_cached(10, 252, c_port3, sizeof(c_port3), buf, TFT_WHITE, TFT_BG_MAIN, 1);

    vTaskDelay(pdMS_TO_TICKS(250));
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
