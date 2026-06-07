/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --font C:/Users/MrKoi/SquareLine/assets/Arteon-m2vP2.otf -o C:/Users/MrKoi/SquareLine/assets\ui_font_Arteon16.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_ARTEON16
#define UI_FONT_ARTEON16 1
#endif

#if UI_FONT_ARTEON16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xff, 0x1f, 0xfe, 0x0,

    /* U+0022 "\"" */
    0xff, 0x7d, 0x80,

    /* U+0023 "#" */
    0x7, 0x1c, 0xe, 0x38, 0xff, 0xf1, 0xff, 0xf3,
    0xff, 0xe1, 0xce, 0x7, 0x1c, 0x7f, 0xfc, 0xff,
    0xf8, 0xff, 0xf0, 0xe3, 0x81, 0xce, 0x0,

    /* U+0024 "$" */
    0x7, 0x0, 0x38, 0x1f, 0xfc, 0xff, 0xef, 0xfe,
    0x70, 0x3, 0x80, 0x1f, 0xfe, 0x7f, 0xf3, 0xff,
    0xc0, 0xe, 0x7f, 0xf7, 0xff, 0xbf, 0xf8, 0x1c,
    0x0, 0xe0,

    /* U+0026 "&" */
    0xe, 0x3, 0x87, 0xff, 0xfe, 0xff, 0xb8, 0xe,
    0x1, 0xfe, 0x7f, 0xbf, 0xee, 0x3, 0xfe, 0xff,
    0xdf, 0xf0, 0xe0, 0x38,

    /* U+0027 "'" */
    0xff, 0xe0,

    /* U+0028 "(" */
    0x3b, 0xff, 0xce, 0x73, 0x9c, 0xe7, 0x39, 0xf7,
    0x9c,

    /* U+0029 ")" */
    0xe7, 0xbe, 0x73, 0x9c, 0xe7, 0x39, 0xcf, 0xff,
    0x70,

    /* U+002C "," */
    0xff, 0xe0,

    /* U+002D "-" */
    0xff, 0xff, 0xff,

    /* U+002E "." */
    0xff, 0x80,

    /* U+002F "/" */
    0xc, 0x71, 0x86, 0x38, 0xe3, 0xc, 0x71, 0xc6,
    0x18, 0xe3, 0x80,

    /* U+0030 "0" */
    0x3f, 0xc7, 0xfe, 0xff, 0xfe, 0x7, 0xe0, 0x7e,
    0x7, 0xe0, 0x7e, 0x7, 0xe0, 0x7f, 0xff, 0x7f,
    0xe3, 0xfc,

    /* U+0031 "1" */
    0x7d, 0xf7, 0xc0, 0x0, 0x71, 0xc7, 0x1c, 0x71,
    0xc7,

    /* U+0032 "2" */
    0x7f, 0xc7, 0xfe, 0x7f, 0xf0, 0x7, 0x3f, 0xf7,
    0xfe, 0xff, 0xce, 0x0, 0xe0, 0xf, 0xfe, 0x7f,
    0xe3, 0xfe,

    /* U+0033 "3" */
    0xff, 0x9f, 0xf7, 0xfc, 0x7, 0x7f, 0xdf, 0xe7,
    0xf8, 0x7, 0x1, 0xdf, 0xf7, 0xfb, 0xfc,

    /* U+0034 "4" */
    0x3, 0xe0, 0x3f, 0x3, 0xf8, 0x1d, 0xc1, 0xce,
    0x1c, 0x71, 0xc3, 0x9f, 0xff, 0xff, 0xff, 0xff,
    0xc0, 0x38, 0x1, 0xc0,

    /* U+0035 "5" */
    0xff, 0xef, 0xfe, 0xff, 0xce, 0x0, 0xe7, 0xce,
    0x7e, 0xe7, 0xf0, 0x7, 0xe0, 0x7f, 0xff, 0x7f,
    0xe3, 0xfc,

    /* U+0036 "6" */
    0x3f, 0xe7, 0xfc, 0xff, 0xce, 0x0, 0xe7, 0xce,
    0x7e, 0xe7, 0xfe, 0x7, 0xe0, 0x7f, 0xff, 0x7f,
    0xe3, 0xfc,

    /* U+0037 "7" */
    0x7f, 0xfb, 0xff, 0xdf, 0xfe, 0x0, 0x0, 0x0,
    0x0, 0xf0, 0x7, 0x0, 0x70, 0x7, 0x80, 0x78,
    0x7, 0x80, 0x38, 0x0,

    /* U+0038 "8" */
    0x7f, 0xe7, 0xff, 0xff, 0xff, 0xc0, 0x7f, 0xff,
    0xbf, 0xfb, 0xff, 0xdc, 0x7, 0xe0, 0x3f, 0xff,
    0xdf, 0xfe, 0xff, 0xc0,

    /* U+0039 "9" */
    0x3f, 0xc7, 0xfe, 0xff, 0xfe, 0x7, 0xe0, 0x7f,
    0xe7, 0x7e, 0x73, 0xe7, 0x0, 0x73, 0xff, 0x3f,
    0xe7, 0xfc,

    /* U+003A ":" */
    0xff, 0x80, 0x7, 0xfc,

    /* U+003B ";" */
    0xff, 0x80, 0x0, 0xff, 0x60,

    /* U+003F "?" */
    0xff, 0xe3, 0xff, 0x9f, 0xfe, 0x0, 0x70, 0x3,
    0x8f, 0xfc, 0x7f, 0xc3, 0xfc, 0x1c, 0x0, 0x0,
    0x7, 0x0, 0x38, 0x1, 0xc0, 0x6, 0x0,

    /* U+0041 "A" */
    0x3, 0x80, 0xf, 0x0, 0x1f, 0x0, 0x7f, 0x0,
    0xee, 0x3, 0x80, 0x7, 0x0, 0x1f, 0xfc, 0x3f,
    0xf8, 0xff, 0xf9, 0xc0, 0x77, 0x0, 0xf0,

    /* U+0042 "B" */
    0xff, 0xf9, 0xff, 0xfb, 0xff, 0xff, 0x0, 0x7f,
    0xff, 0xff, 0xff, 0xbf, 0xff, 0x0, 0x7, 0x0,
    0xf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0,

    /* U+0043 "C" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe0,
    0x3, 0x80, 0xe, 0x0, 0x38, 0x0, 0xe0, 0x3,
    0x9f, 0xf6, 0x7f, 0xc9, 0xff,

    /* U+0044 "D" */
    0xff, 0xe7, 0xff, 0xbf, 0xff, 0xc0, 0x7e, 0x3,
    0xf0, 0x1f, 0x80, 0xe0, 0x7, 0x0, 0x3f, 0xff,
    0xff, 0xfd, 0xff, 0xc0,

    /* U+0045 "E" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe7,
    0xf3, 0x9f, 0xce, 0x7f, 0x38, 0x0, 0xe0, 0x3,
    0x9f, 0xfe, 0x7f, 0xf9, 0xff,

    /* U+0046 "F" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe7,
    0xf3, 0x9f, 0xce, 0x7f, 0x38, 0x0, 0xe0, 0x3,
    0x80, 0xe, 0x0, 0x38, 0x0,

    /* U+0047 "G" */
    0x3f, 0xfb, 0xff, 0xff, 0xff, 0xc0, 0xe, 0x7f,
    0xf1, 0xff, 0x8f, 0xfc, 0x7, 0xe0, 0x3f, 0xf8,
    0x1f, 0xc0, 0x7e, 0x0,

    /* U+0048 "H" */
    0xe0, 0xf, 0xc0, 0x1f, 0x80, 0x38, 0x0, 0x7f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x0, 0xe0,
    0x1, 0xc0, 0x1f, 0x80, 0x3f, 0x0, 0x70,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x1, 0xc0, 0x70, 0x1c, 0x7, 0x1, 0xc0, 0x70,
    0x1f, 0x87, 0xe1, 0xff, 0xff, 0xfb, 0xfc,

    /* U+004B "K" */
    0xe0, 0xee, 0x1c, 0xe3, 0xce, 0x78, 0xef, 0xf,
    0xe0, 0xfc, 0xf, 0x80, 0xf1, 0x8e, 0x3c, 0xe1,
    0xee, 0xf,

    /* U+004C "L" */
    0xe0, 0xe, 0x0, 0xe0, 0xe, 0x0, 0xe0, 0xe,
    0x0, 0xe0, 0xe, 0x0, 0xe0, 0xf, 0xff, 0xff,
    0xff, 0xff,

    /* U+004D "M" */
    0xe0, 0x7, 0xf0, 0x7, 0xf8, 0x7, 0xf8, 0x7,
    0xfc, 0x27, 0xfe, 0x67, 0xee, 0x67, 0xe7, 0xe7,
    0xe7, 0xc7, 0xe3, 0xc7, 0xe1, 0x87, 0xe0, 0x7,

    /* U+004E "N" */
    0xe0, 0x3f, 0x3, 0xf8, 0x3f, 0xc3, 0xfe, 0x3b,
    0xf3, 0xdf, 0x9c, 0xfc, 0xc7, 0xe4, 0x3f, 0x1,
    0xf8, 0xf, 0xc0, 0x70,

    /* U+004F "O" */
    0x3f, 0xc8, 0xff, 0x9b, 0xff, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf0, 0x7, 0xe0,
    0xf, 0xcf, 0xfd, 0x9f, 0xf1, 0x3f, 0xc0,

    /* U+0050 "P" */
    0xff, 0xe7, 0xff, 0xbf, 0xfe, 0x0, 0x70, 0x3,
    0xff, 0xff, 0xff, 0xdf, 0xfc, 0xe0, 0x7, 0x0,
    0x38, 0x1, 0xc0, 0x0,

    /* U+0051 "Q" */
    0x3f, 0xc8, 0xff, 0x9b, 0xff, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf1, 0xc7, 0xe3,
    0x8f, 0xe7, 0xfd, 0xcf, 0xf1, 0x9f, 0xc0, 0x38,
    0x0,

    /* U+0052 "R" */
    0xff, 0xf3, 0xff, 0xef, 0xff, 0xc0, 0x7, 0x0,
    0x1f, 0x9f, 0xff, 0x3f, 0xbe, 0x7c, 0xe1, 0xe3,
    0x83, 0xce, 0x7, 0xb8, 0xf,

    /* U+0053 "S" */
    0x3f, 0xfc, 0xff, 0xfb, 0xff, 0xf7, 0x0, 0xf,
    0xff, 0x8f, 0xff, 0x8f, 0xff, 0x80, 0x7, 0x0,
    0xe, 0xff, 0xfd, 0xff, 0xf3, 0xff, 0xc0,

    /* U+0054 "T" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x0, 0x0,
    0x0, 0x1c, 0x0, 0x70, 0x1, 0xc0, 0x7, 0x0,
    0x1c, 0x0, 0x70, 0x1, 0xc0,

    /* U+0055 "U" */
    0xe0, 0xf, 0xc0, 0x1f, 0x80, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf0, 0x7, 0xe0,
    0xf, 0xff, 0xfd, 0xff, 0xf1, 0xff, 0xc0,

    /* U+0056 "V" */
    0xe0, 0x1c, 0xe0, 0x39, 0xc0, 0xe1, 0xc1, 0xc3,
    0x87, 0x3, 0x8e, 0x7, 0x38, 0x6, 0x70, 0x9,
    0xc0, 0x3, 0x80, 0xe, 0x0, 0x1c, 0x0,

    /* U+0057 "W" */
    0xe7, 0x9c, 0x77, 0x39, 0xce, 0x71, 0xf8, 0xe3,
    0x9f, 0x9c, 0x38, 0xf1, 0xc1, 0xcf, 0x38, 0x1c,
    0xe3, 0x80, 0xce, 0x70, 0x9, 0xcf, 0x0, 0x1c,
    0xe0, 0x3, 0x9e, 0x0, 0x39, 0xc0,

    /* U+0058 "X" */
    0x70, 0x39, 0xe1, 0xc3, 0xcf, 0x7, 0xf8, 0xf,
    0xc0, 0x1e, 0x0, 0x70, 0x3, 0x90, 0x1e, 0xe0,
    0xf3, 0xc7, 0x87, 0xbc, 0xe,

    /* U+0059 "Y" */
    0xf0, 0x7b, 0xc7, 0x8e, 0x38, 0x73, 0x81, 0x3c,
    0x3, 0xc0, 0x1c, 0x0, 0xe0, 0x7, 0x0, 0x38,
    0x1, 0xc0, 0xe, 0x0,

    /* U+005A "Z" */
    0xff, 0xfb, 0xff, 0xdf, 0xfe, 0x3, 0xe0, 0x3c,
    0x3, 0xc0, 0x7c, 0x0, 0x0, 0x0, 0x7, 0xff,
    0xbf, 0xfd, 0xff, 0xf0,

    /* U+0061 "a" */
    0x3, 0x80, 0xf, 0x0, 0x1f, 0x0, 0x7f, 0x0,
    0xee, 0x3, 0x80, 0x7, 0x0, 0x1f, 0xfc, 0x3f,
    0xf8, 0xff, 0xf9, 0xc0, 0x77, 0x0, 0xf0,

    /* U+0062 "b" */
    0xff, 0xf9, 0xff, 0xfb, 0xff, 0xff, 0x0, 0x7f,
    0xff, 0xff, 0xff, 0xbf, 0xff, 0x0, 0x7, 0x0,
    0xf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0,

    /* U+0063 "c" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe0,
    0x3, 0x80, 0xe, 0x0, 0x38, 0x0, 0xe0, 0x3,
    0x9f, 0xf6, 0x7f, 0xc9, 0xff,

    /* U+0064 "d" */
    0xff, 0xe7, 0xff, 0xbf, 0xff, 0xc0, 0x7e, 0x3,
    0xf0, 0x1f, 0x80, 0xe0, 0x7, 0x0, 0x3f, 0xff,
    0xff, 0xfd, 0xff, 0xc0,

    /* U+0065 "e" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe7,
    0xf3, 0x9f, 0xce, 0x7f, 0x38, 0x0, 0xe0, 0x3,
    0x9f, 0xfe, 0x7f, 0xf9, 0xff,

    /* U+0066 "f" */
    0x3f, 0xfd, 0xff, 0xff, 0xff, 0xf8, 0x0, 0xe7,
    0xf3, 0x9f, 0xce, 0x7f, 0x38, 0x0, 0xe0, 0x3,
    0x80, 0xe, 0x0, 0x38, 0x0,

    /* U+0067 "g" */
    0x3f, 0xfb, 0xff, 0xff, 0xff, 0xc0, 0xe, 0x7f,
    0xf1, 0xff, 0x8f, 0xfc, 0x7, 0xe0, 0x3f, 0xf8,
    0x1f, 0xc0, 0x7e, 0x0,

    /* U+0068 "h" */
    0xe0, 0xf, 0xc0, 0x1f, 0x80, 0x38, 0x0, 0x7f,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x0, 0xe0,
    0x1, 0xc0, 0x1f, 0x80, 0x3f, 0x0, 0x70,

    /* U+0069 "i" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+006A "j" */
    0x1, 0xc0, 0x70, 0x1c, 0x7, 0x1, 0xc0, 0x70,
    0x1f, 0x87, 0xe1, 0xff, 0xff, 0xfb, 0xfc,

    /* U+006B "k" */
    0xe0, 0xee, 0x1c, 0xe3, 0xce, 0x78, 0xef, 0xf,
    0xe0, 0xfc, 0xf, 0x80, 0xf1, 0x8e, 0x3c, 0xe1,
    0xee, 0xf,

    /* U+006C "l" */
    0xe0, 0xe, 0x0, 0xe0, 0xe, 0x0, 0xe0, 0xe,
    0x0, 0xe0, 0xe, 0x0, 0xe0, 0xf, 0xff, 0xff,
    0xff, 0xff,

    /* U+006D "m" */
    0xe0, 0x7, 0xf0, 0x7, 0xf8, 0x7, 0xf8, 0x7,
    0xfc, 0x27, 0xfe, 0x67, 0xee, 0x67, 0xe7, 0xe7,
    0xe7, 0xc7, 0xe3, 0xc7, 0xe1, 0x87, 0xe0, 0x7,

    /* U+006E "n" */
    0xe0, 0x3f, 0x3, 0xf8, 0x3f, 0xc3, 0xfe, 0x3b,
    0xf3, 0xdf, 0x9c, 0xfc, 0xc7, 0xe4, 0x3f, 0x1,
    0xf8, 0xf, 0xc0, 0x70,

    /* U+006F "o" */
    0x3f, 0xc8, 0xff, 0x9b, 0xff, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf0, 0x7, 0xe0,
    0xf, 0xcf, 0xfd, 0x9f, 0xf1, 0x3f, 0xc0,

    /* U+0070 "p" */
    0xff, 0xe7, 0xff, 0xbf, 0xfe, 0x0, 0x70, 0x3,
    0xff, 0xff, 0xff, 0xdf, 0xfc, 0xe0, 0x7, 0x0,
    0x38, 0x1, 0xc0, 0x0,

    /* U+0071 "q" */
    0x3f, 0xc8, 0xff, 0x9b, 0xff, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf1, 0xc7, 0xe3,
    0x8f, 0xe7, 0xfd, 0xcf, 0xf1, 0x9f, 0xc0, 0x38,
    0x0,

    /* U+0072 "r" */
    0xff, 0xf3, 0xff, 0xef, 0xff, 0xc0, 0x7, 0x0,
    0x1f, 0x9f, 0xff, 0x3f, 0xbe, 0x7c, 0xe1, 0xe3,
    0x83, 0xce, 0x7, 0xb8, 0xf,

    /* U+0073 "s" */
    0x3f, 0xfc, 0xff, 0xfb, 0xff, 0xf7, 0x0, 0xf,
    0xff, 0x8f, 0xff, 0x8f, 0xff, 0x80, 0x7, 0x0,
    0xe, 0xff, 0xfd, 0xff, 0xf3, 0xff, 0xc0,

    /* U+0074 "t" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x0, 0x0,
    0x0, 0x1c, 0x0, 0x70, 0x1, 0xc0, 0x7, 0x0,
    0x1c, 0x0, 0x70, 0x1, 0xc0,

    /* U+0075 "u" */
    0xe0, 0xf, 0xc0, 0x1f, 0x80, 0x3f, 0x0, 0x7e,
    0x0, 0xfc, 0x1, 0xf8, 0x3, 0xf0, 0x7, 0xe0,
    0xf, 0xff, 0xfd, 0xff, 0xf1, 0xff, 0xc0,

    /* U+0076 "v" */
    0xe0, 0x1c, 0xe0, 0x39, 0xc0, 0xe1, 0xc1, 0xc3,
    0x87, 0x3, 0x8e, 0x7, 0x38, 0x6, 0x70, 0x9,
    0xc0, 0x3, 0x80, 0xe, 0x0, 0x1c, 0x0,

    /* U+0077 "w" */
    0xe7, 0x9c, 0x77, 0x39, 0xce, 0x71, 0xf8, 0xe3,
    0x9f, 0x9c, 0x38, 0xf1, 0xc1, 0xcf, 0x38, 0x1c,
    0xe3, 0x80, 0xce, 0x70, 0x9, 0xcf, 0x0, 0x1c,
    0xe0, 0x3, 0x9e, 0x0, 0x39, 0xc0,

    /* U+0078 "x" */
    0x70, 0x39, 0xe1, 0xc3, 0xcf, 0x7, 0xf8, 0xf,
    0xc0, 0x1e, 0x0, 0x70, 0x3, 0x90, 0x1e, 0xe0,
    0xf3, 0xc7, 0x87, 0xbc, 0xe,

    /* U+0079 "y" */
    0xf0, 0x7b, 0xc7, 0x8e, 0x38, 0x73, 0x81, 0x3c,
    0x3, 0xc0, 0x1c, 0x0, 0xe0, 0x7, 0x0, 0x38,
    0x1, 0xc0, 0xe, 0x0,

    /* U+007A "z" */
    0xff, 0xfb, 0xff, 0xdf, 0xfe, 0x3, 0xe0, 0x3c,
    0x3, 0xc0, 0x7c, 0x0, 0x0, 0x0, 0x7, 0xff,
    0xbf, 0xfd, 0xff, 0xf0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 136, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 73, .box_w = 3, .box_h = 14, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 7, .adv_w = 117, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 10, .adv_w = 255, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 33, .adv_w = 227, .box_w = 13, .box_h = 16, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 59, .adv_w = 172, .box_w = 10, .box_h = 16, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 79, .adv_w = 69, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 9},
    {.bitmap_index = 81, .adv_w = 83, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 90, .adv_w = 86, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 99, .adv_w = 69, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 101, .adv_w = 147, .box_w = 8, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 104, .adv_w = 71, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 106, .adv_w = 109, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 205, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 135, .adv_w = 108, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 144, .adv_w = 210, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 162, .adv_w = 172, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 177, .adv_w = 215, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 197, .adv_w = 203, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 215, .adv_w = 203, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 233, .adv_w = 219, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 253, .adv_w = 223, .box_w = 13, .box_h = 12, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 273, .adv_w = 206, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 291, .adv_w = 73, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 295, .adv_w = 72, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 300, .adv_w = 210, .box_w = 13, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 249, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 346, .adv_w = 259, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 369, .adv_w = 232, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 390, .adv_w = 230, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 410, .adv_w = 232, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 431, .adv_w = 230, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 452, .adv_w = 232, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 472, .adv_w = 259, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 495, .adv_w = 65, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 500, .adv_w = 181, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 515, .adv_w = 205, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 533, .adv_w = 211, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 551, .adv_w = 271, .box_w = 16, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 575, .adv_w = 226, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 595, .adv_w = 258, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 618, .adv_w = 229, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 638, .adv_w = 258, .box_w = 15, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 238, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 684, .adv_w = 247, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 707, .adv_w = 237, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 728, .adv_w = 257, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 751, .adv_w = 249, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 774, .adv_w = 332, .box_w = 20, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 804, .adv_w = 233, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 825, .adv_w = 229, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 845, .adv_w = 222, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 865, .adv_w = 249, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 888, .adv_w = 259, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 911, .adv_w = 232, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 932, .adv_w = 230, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 952, .adv_w = 232, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 973, .adv_w = 230, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 994, .adv_w = 232, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1014, .adv_w = 259, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1037, .adv_w = 65, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1042, .adv_w = 181, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1057, .adv_w = 205, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1075, .adv_w = 211, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1093, .adv_w = 271, .box_w = 16, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1117, .adv_w = 226, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1137, .adv_w = 258, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1160, .adv_w = 229, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1180, .adv_w = 258, .box_w = 15, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1205, .adv_w = 238, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1226, .adv_w = 247, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1249, .adv_w = 237, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1270, .adv_w = 257, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1293, .adv_w = 249, .box_w = 15, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1316, .adv_w = 332, .box_w = 20, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1346, .adv_w = 233, .box_w = 14, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1367, .adv_w = 229, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1387, .adv_w = 222, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 1, 2, 3, 4, 0, 5, 6,
    7, 8, 0, 0, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 0, 0, 0, 25
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 32, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 32, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    },
    {
        .range_start = 65, .range_length = 26, .glyph_id_start = 27,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 97, .range_length = 26, .glyph_id_start = 53,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    3, 4,
    3, 5,
    3, 6,
    3, 13,
    3, 18,
    4, 4,
    4, 9,
    4, 10,
    4, 12,
    4, 13,
    4, 15,
    4, 16,
    4, 17,
    4, 18,
    4, 21,
    4, 26,
    5, 5,
    5, 15,
    5, 21,
    5, 26,
    6, 4,
    6, 18,
    6, 26,
    7, 4,
    7, 13,
    7, 18,
    8, 4,
    8, 11,
    8, 18,
    9, 21,
    9, 26,
    10, 15,
    10, 18,
    10, 21,
    10, 23,
    10, 26,
    11, 9,
    11, 15,
    11, 21,
    11, 26,
    12, 15,
    12, 21,
    12, 23,
    12, 26,
    13, 4,
    13, 5,
    13, 6,
    13, 10,
    13, 11,
    13, 12,
    13, 13,
    13, 15,
    13, 16,
    13, 18,
    14, 21,
    14, 26,
    16, 4,
    16, 13,
    16, 16,
    16, 18,
    16, 21,
    16, 26,
    17, 21,
    17, 26,
    18, 3,
    18, 5,
    18, 7,
    18, 9,
    18, 10,
    18, 15,
    18, 21,
    18, 23,
    18, 26,
    19, 15,
    19, 21,
    19, 26,
    20, 7,
    20, 9,
    20, 15,
    20, 21,
    20, 26,
    21, 4,
    21, 10,
    21, 11,
    21, 12,
    21, 13,
    21, 18,
    22, 21,
    22, 26,
    23, 21,
    23, 26,
    25, 13,
    26, 4,
    26, 10,
    26, 12,
    26, 13,
    26, 16,
    26, 18,
    26, 26,
    27, 29,
    27, 33,
    27, 41,
    27, 43,
    27, 46,
    27, 47,
    27, 48,
    27, 49,
    27, 51,
    27, 55,
    27, 59,
    27, 67,
    27, 69,
    27, 72,
    27, 73,
    27, 74,
    27, 75,
    27, 77,
    28, 27,
    28, 48,
    28, 49,
    28, 50,
    28, 51,
    28, 53,
    28, 74,
    28, 75,
    28, 76,
    28, 77,
    30, 27,
    30, 48,
    30, 49,
    30, 50,
    30, 51,
    30, 53,
    30, 74,
    30, 75,
    30, 76,
    30, 77,
    32, 27,
    32, 36,
    32, 53,
    32, 62,
    33, 27,
    33, 53,
    36, 27,
    36, 53,
    37, 29,
    37, 33,
    37, 41,
    37, 43,
    37, 45,
    37, 48,
    37, 49,
    37, 51,
    37, 55,
    37, 59,
    37, 67,
    37, 69,
    37, 71,
    37, 74,
    37, 75,
    37, 77,
    38, 46,
    38, 48,
    38, 49,
    38, 51,
    38, 72,
    38, 74,
    38, 75,
    38, 77,
    41, 27,
    41, 48,
    41, 49,
    41, 50,
    41, 51,
    41, 53,
    41, 74,
    41, 75,
    41, 76,
    41, 77,
    42, 27,
    42, 36,
    42, 48,
    42, 49,
    42, 50,
    42, 51,
    42, 52,
    42, 53,
    42, 62,
    42, 74,
    42, 75,
    42, 76,
    42, 77,
    42, 78,
    43, 27,
    43, 48,
    43, 49,
    43, 50,
    43, 51,
    43, 53,
    43, 74,
    43, 75,
    43, 76,
    43, 77,
    44, 48,
    44, 49,
    44, 51,
    44, 74,
    44, 75,
    44, 77,
    45, 27,
    45, 48,
    45, 49,
    45, 50,
    45, 51,
    45, 53,
    45, 74,
    45, 75,
    45, 76,
    45, 77,
    46, 27,
    46, 36,
    46, 53,
    46, 62,
    47, 27,
    47, 53,
    48, 27,
    48, 29,
    48, 31,
    48, 32,
    48, 33,
    48, 36,
    48, 41,
    48, 43,
    48, 45,
    48, 53,
    48, 55,
    48, 57,
    48, 58,
    48, 59,
    48, 62,
    48, 67,
    48, 69,
    48, 71,
    49, 27,
    49, 29,
    49, 31,
    49, 32,
    49, 33,
    49, 36,
    49, 41,
    49, 43,
    49, 45,
    49, 53,
    49, 55,
    49, 57,
    49, 58,
    49, 59,
    49, 62,
    49, 67,
    49, 69,
    49, 71,
    50, 29,
    50, 33,
    50, 41,
    50, 43,
    50, 45,
    50, 55,
    50, 59,
    50, 67,
    50, 69,
    50, 71,
    51, 27,
    51, 29,
    51, 31,
    51, 32,
    51, 33,
    51, 36,
    51, 41,
    51, 43,
    51, 45,
    51, 53,
    51, 55,
    51, 57,
    51, 58,
    51, 59,
    51, 62,
    51, 67,
    51, 69,
    51, 71,
    53, 29,
    53, 33,
    53, 41,
    53, 43,
    53, 46,
    53, 47,
    53, 48,
    53, 49,
    53, 51,
    53, 55,
    53, 59,
    53, 67,
    53, 69,
    53, 72,
    53, 73,
    53, 74,
    53, 75,
    53, 77,
    54, 27,
    54, 48,
    54, 49,
    54, 50,
    54, 51,
    54, 53,
    54, 74,
    54, 75,
    54, 76,
    54, 77,
    56, 27,
    56, 48,
    56, 49,
    56, 50,
    56, 51,
    56, 53,
    56, 74,
    56, 75,
    56, 76,
    56, 77,
    58, 27,
    58, 36,
    58, 53,
    58, 62,
    59, 27,
    59, 53,
    62, 27,
    62, 53,
    63, 29,
    63, 33,
    63, 41,
    63, 43,
    63, 45,
    63, 48,
    63, 49,
    63, 51,
    63, 55,
    63, 59,
    63, 67,
    63, 69,
    63, 71,
    63, 74,
    63, 75,
    63, 77,
    64, 46,
    64, 48,
    64, 49,
    64, 51,
    64, 72,
    64, 74,
    64, 75,
    64, 77,
    67, 27,
    67, 48,
    67, 49,
    67, 50,
    67, 51,
    67, 53,
    67, 74,
    67, 75,
    67, 76,
    67, 77,
    68, 27,
    68, 36,
    68, 48,
    68, 49,
    68, 50,
    68, 51,
    68, 52,
    68, 53,
    68, 62,
    68, 74,
    68, 75,
    68, 76,
    68, 77,
    68, 78,
    69, 27,
    69, 48,
    69, 49,
    69, 50,
    69, 51,
    69, 53,
    69, 74,
    69, 75,
    69, 76,
    69, 77,
    70, 48,
    70, 49,
    70, 51,
    70, 74,
    70, 75,
    70, 77,
    71, 27,
    71, 48,
    71, 49,
    71, 50,
    71, 51,
    71, 53,
    71, 74,
    71, 75,
    71, 76,
    71, 77,
    72, 27,
    72, 36,
    72, 53,
    72, 62,
    73, 27,
    73, 53,
    74, 27,
    74, 29,
    74, 31,
    74, 32,
    74, 33,
    74, 36,
    74, 41,
    74, 43,
    74, 45,
    74, 53,
    74, 55,
    74, 57,
    74, 58,
    74, 59,
    74, 62,
    74, 67,
    74, 69,
    74, 71,
    75, 27,
    75, 29,
    75, 31,
    75, 32,
    75, 33,
    75, 36,
    75, 41,
    75, 43,
    75, 45,
    75, 53,
    75, 55,
    75, 57,
    75, 58,
    75, 59,
    75, 62,
    75, 67,
    75, 69,
    75, 71,
    76, 29,
    76, 33,
    76, 41,
    76, 43,
    76, 45,
    76, 55,
    76, 59,
    76, 67,
    76, 69,
    76, 71,
    77, 27,
    77, 29,
    77, 31,
    77, 32,
    77, 33,
    77, 36,
    77, 41,
    77, 43,
    77, 45,
    77, 53,
    77, 55,
    77, 57,
    77, 58,
    77, 59,
    77, 62,
    77, 67,
    77, 69,
    77, 71
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -33, -5, -4, -30, -21, -25, -7, -18,
    -12, -27, -10, -17, -6, -24, -17, -24,
    -6, -9, -14, -19, -17, -15, -10, -31,
    -31, -16, -18, -7, -20, -9, -16, -24,
    0, -25, -8, -29, -6, -21, -28, -22,
    -22, -22, -5, -25, -33, -8, -8, -26,
    -5, -23, -31, -5, -8, -26, -14, -18,
    -20, -6, -6, -15, -8, -16, -12, -18,
    -4, -11, -4, -9, -5, -22, -28, -8,
    -28, -16, -10, -14, -5, -4, -21, -23,
    -22, -39, -16, -17, -13, -39, -46, -12,
    -18, -14, -19, -5, -28, -14, -3, -28,
    -10, -27, -10, -11, -11, -11, -11, -64,
    -9, -79, -79, -87, -11, -11, -11, -11,
    -64, -9, -79, -79, -87, -15, -10, -10,
    -17, -23, -15, -10, -10, -17, -23, -15,
    -15, -15, -21, -30, -15, -15, -15, -21,
    -30, -51, -29, -51, -29, -9, -9, -17,
    -17, -22, -22, -22, -22, -6, -7, -7,
    -10, -22, -22, -22, -22, -6, -7, -7,
    -10, -73, -66, -74, -64, -81, -76, -69,
    -70, -16, -16, -15, -22, -30, -16, -16,
    -15, -22, -30, -53, -72, -12, -12, -29,
    -26, -17, -53, -72, -12, -12, -29, -26,
    -17, -16, -16, -15, -22, -30, -16, -16,
    -15, -22, -30, -14, -13, -28, -14, -13,
    -28, -12, -5, -5, -10, -10, -12, -5,
    -5, -10, -10, -61, -83, -59, -83, -16,
    -16, -72, -10, -10, -10, -10, -74, -10,
    -10, -8, -72, -10, -10, -10, -10, -74,
    -10, -10, -8, -72, -10, -10, -10, -10,
    -56, -10, -10, -8, -72, -10, -10, -10,
    -10, -56, -10, -10, -8, -16, -15, -16,
    -16, -4, -16, -15, -16, -16, -4, -83,
    -24, -24, -24, -24, -84, -24, -24, -21,
    -83, -24, -24, -24, -24, -84, -24, -24,
    -21, -11, -11, -11, -11, -52, -9, -79,
    -79, -87, -11, -11, -11, -11, -54, -9,
    -79, -79, -87, -15, -10, -10, -17, -23,
    -15, -10, -10, -17, -23, -15, -15, -15,
    -21, -30, -15, -15, -15, -21, -30, -56,
    -34, -56, -39, -32, -32, -17, -17, -22,
    -22, -22, -22, -6, -7, -7, -10, -22,
    -22, -22, -22, -6, -7, -7, -10, -83,
    -71, -74, -69, -83, -64, -72, -82, -16,
    -16, -15, -22, -30, -16, -16, -15, -22,
    -30, -53, -19, -12, -12, -29, -26, -17,
    -53, -16, -12, -12, -29, -26, -17, -16,
    -16, -15, -22, -30, -16, -16, -15, -22,
    -30, -14, -13, -28, -14, -13, -28, -12,
    -5, -5, -10, -10, -12, -5, -5, -10,
    -10, -70, -83, -73, -83, -16, -16, -72,
    -10, -10, -10, -10, -52, -10, -10, -8,
    -72, -10, -10, -10, -10, -52, -10, -10,
    -8, -72, -10, -10, -10, -10, -56, -10,
    -10, -8, -72, -10, -10, -10, -10, -56,
    -10, -10, -8, -16, -15, -16, -16, -4,
    -16, -15, -16, -16, -4, -83, -24, -24,
    -24, -24, -84, -24, -24, -21, -83, -24,
    -24, -24, -24, -84, -24, -24, -21
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 479,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_Arteon16 = {
#else
lv_font_t ui_font_Arteon16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_ARTEON16*/

