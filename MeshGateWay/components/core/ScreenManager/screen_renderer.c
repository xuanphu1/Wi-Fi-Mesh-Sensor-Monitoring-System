#include "screen_renderer.h"
#include "screen_driver.h"
#include "font8x8.h"
#include <string.h>

void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color) {
  if (w <= 0 || h <= 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT ||
      screen_internal_panel_handle == NULL || screen_internal_dma_buf == NULL)
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
    screen_internal_dma_buf[i] = swapped;
  }

  int16_t cur_y = y;
  while (cur_y < y + h) {
    int16_t lines = y + h - cur_y;
    if (lines > max_lines)
      lines = max_lines;
    tft_draw_bitmap_wait(x, cur_y, x + w, cur_y + lines, screen_internal_dma_buf);
    cur_y += lines;
  }
}

void tft_fill_screen(uint16_t color) {
  tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void tft_draw_fast_h_line(int16_t x, int16_t y, int16_t w,
                          uint16_t color) {
  tft_fill_rect(x, y, w, 1, color);
}

void tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h,
                          uint16_t color) {
  tft_fill_rect(x, y, 1, h, color);
}

void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color) {
  tft_draw_fast_h_line(x, y, w, color);
  tft_draw_fast_h_line(x, y + h - 1, w, color);
  tft_draw_fast_v_line(x, y, h, color);
  tft_draw_fast_v_line(x + w - 1, y, h, color);
}

void tft_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg,
                     uint16_t bg, uint8_t size) {
  if (!str || !screen_internal_dma_buf || screen_internal_panel_handle == NULL)
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
              screen_internal_dma_buf[py * chunk_w + px] = pixel;
            }
          }
        }
      }
    }

    if (cur_x < TFT_WIDTH && cur_y < TFT_HEIGHT && cur_x + chunk_w > 0 &&
        cur_y + chunk_h > 0) {
      tft_draw_bitmap_wait(cur_x, cur_y, cur_x + chunk_w, cur_y + chunk_h,
                           screen_internal_dma_buf);
    }

    cur_x += chunk_w;
    str += chunk_chars;
  }
}

void tft_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h,
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

void tft_draw_text_box(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char *text, uint16_t fg, uint16_t bg,
                       uint8_t size) {
  if (w <= 0 || h <= 0)
    return;
  tft_fill_rect(x, y, w, h, bg);
  if (text && text[0] != '\0') {
    tft_draw_string(x, y, text, fg, bg, size);
  }
}

void draw_text_cached_box(int16_t x, int16_t y, int16_t w, int16_t h,
                          char *cache, size_t cache_sz,
                          const char *new_text, uint16_t fg, uint16_t bg,
                          uint8_t size) {
  if (strcmp(cache, new_text) != 0) {
    tft_draw_text_box(x, y, w, h, new_text, fg, bg, size);
    strncpy(cache, new_text, cache_sz - 1);
    cache[cache_sz - 1] = '\0';
  }
}

void tft_draw_segmented_bar(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint8_t percent, uint16_t active_color,
                            uint16_t inactive_color) {
  if (w <= 0 || h <= 0 || !screen_internal_dma_buf || screen_internal_panel_handle == NULL)
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
        screen_internal_dma_buf[row_offset + base_x + i] = seg_col;
      }
      for (int g = 0; g < seg_gap; g++) {
        screen_internal_dma_buf[row_offset + base_x + seg_w + g] = bg_sw;
      }
    }
    int filled_w = total_segs * pitch;
    for (int rem = filled_w; rem < w; rem++) {
      screen_internal_dma_buf[row_offset + rem] = bg_sw;
    }
  }

  tft_draw_bitmap_wait(x, y, x + w, y + h, screen_internal_dma_buf);
}

void draw_segmented_bar_cached(int16_t x, int16_t y, int16_t w,
                               int16_t h, uint8_t *cached_pct,
                               uint8_t new_pct, uint16_t active_color,
                               uint16_t inactive_color) {
  if (*cached_pct != new_pct) {
    *cached_pct = new_pct;
    tft_draw_segmented_bar(x, y, w, h, new_pct, active_color, inactive_color);
  }
}

void draw_progress_bar_cached(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint8_t *cached_pct, uint8_t new_pct,
                              uint16_t fg_color, uint16_t bg_color,
                              uint16_t border_color) {
  if (*cached_pct != new_pct) {
    *cached_pct = new_pct;
    tft_draw_progress_bar(x, y, w, h, new_pct, fg_color, bg_color,
                          border_color);
  }
}

void draw_battery_glyph(int16_t x, int16_t y, int bat_pct) {
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
