#ifndef SCREEN_RENDERER_H
#define SCREEN_RENDERER_H

#include "screen_internal.h"
#include <stddef.h>
#include <stdint.h>

void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_fill_screen(uint16_t color);
void tft_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color);
void tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color);
void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);
void tft_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t fg_color, uint16_t bg_color, uint16_t border_color);
void tft_draw_text_box(int16_t x, int16_t y, int16_t w, int16_t h, const char *text, uint16_t fg, uint16_t bg, uint8_t size);
void draw_text_cached_box(int16_t x, int16_t y, int16_t w, int16_t h, char *cache, size_t cache_sz, const char *new_text, uint16_t fg, uint16_t bg, uint8_t size);
void tft_draw_segmented_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t active_color, uint16_t inactive_color);
void draw_segmented_bar_cached(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t *cached_pct, uint8_t new_pct, uint16_t active_color, uint16_t inactive_color);
void draw_progress_bar_cached(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t *cached_pct, uint8_t new_pct, uint16_t fg_color, uint16_t bg_color, uint16_t border_color);
void draw_battery_glyph(int16_t x, int16_t y, int bat_pct);

#endif // SCREEN_RENDERER_H
