#include "screen_dashboard.h"
#include "screen_renderer.h"
#include "esp_system.h"
#include <string.h>

const char *screen_dashboard_get_weekday_name(uint8_t wday) {
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

const char *screen_dashboard_month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

const char *screen_dashboard_get_reset_reason_name(void) {
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

void screen_dashboard_render_target_button(websocket_target_t target) {
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

void screen_dashboard_render_static(void) {
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
