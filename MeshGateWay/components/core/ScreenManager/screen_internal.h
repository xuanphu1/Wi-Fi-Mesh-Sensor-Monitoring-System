#ifndef SCREEN_INTERNAL_H
#define SCREEN_INTERNAL_H

#include "ScreenManager.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

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
#define TFT_DMA_BUF_PIXELS (TFT_WIDTH * TFT_DMA_BUF_LINES) // 240 * 16 = 3840 pixels
#define TFT_DMA_BUF_SIZE (TFT_DMA_BUF_PIXELS * sizeof(uint16_t)) // 7680 bytes

static inline uint16_t swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

typedef struct {
    dm_metrics_t *metrics;
    dm_lvgl_t *lvgl;
    dm_telemetry_t *telemetry;
    dm_hw_t *hw;
} screen_ctx_t;

typedef struct {
    bool active;
    uint8_t percent;
    char target[32];
    char version[32];
    char detail[64];
    TickType_t hide_after_tick;
} screen_ota_state_t;

// State dùng chung nội bộ giữa driver, renderer và dashboard
extern esp_lcd_panel_handle_t screen_internal_panel_handle;
extern SemaphoreHandle_t screen_internal_refresh_done_sem;
extern uint16_t *screen_internal_dma_buf;
extern screen_ota_state_t screen_internal_ota_state;
extern SemaphoreHandle_t screen_internal_ota_mutex;

#endif // SCREEN_INTERNAL_H
