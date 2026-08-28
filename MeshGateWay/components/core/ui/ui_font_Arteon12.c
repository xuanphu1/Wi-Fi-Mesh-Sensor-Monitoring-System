/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --font C:/Users/MrKoi/SquareLine/assets/Arteon-m2vP2.otf -o C:/Users/MrKoi/SquareLine/assets\ui_font_Arteon12.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_ARTEON12
#define UI_FONT_ARTEON12 1
#endif

#if UI_FONT_ARTEON12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf3, 0xd0,

    /* U+0022 "\"" */
    0xf5, 0x80,

    /* U+0023 "#" */
    0xc, 0xcf, 0xfc, 0xff, 0x8c, 0xc1, 0x99, 0xff,
    0x9f, 0xf1, 0x98, 0x77, 0x0,

    /* U+0024 "$" */
    0xc, 0x3, 0x7, 0xfb, 0xfe, 0xc0, 0x30, 0xf,
    0xf9, 0xff, 0x0, 0xdf, 0xf7, 0xf8, 0x30, 0xc,
    0x0,

    /* U+0026 "&" */
    0x18, 0x18, 0x7e, 0xfe, 0xc0, 0xc0, 0x7e, 0xfe,
    0xc0, 0xfe, 0x7e, 0x18, 0x18,

    /* U+0027 "'" */
    0xdb, 0x0,

    /* U+0028 "(" */
    0x7f, 0x6d, 0xb6, 0xec,

    /* U+0029 ")" */
    0xef, 0x33, 0x33, 0x33, 0xfe,

    /* U+002C "," */
    0xdb, 0x0,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x13, 0x33, 0x26, 0x66, 0x4c, 0xc0,

    /* U+0030 "0" */
    0x7f, 0x7f, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0xff, 0x7f, 0x0,

    /* U+0031 "1" */
    0x7f, 0x3, 0x33, 0x33, 0x30,

    /* U+0032 "2" */
    0x7f, 0x3f, 0xc0, 0x6f, 0xff, 0xf6, 0x3, 0x1,
    0xfe, 0x7f, 0x0,

    /* U+0033 "3" */
    0xfd, 0xfc, 0x1f, 0xff, 0xc0, 0xc1, 0xff, 0xfc,

    /* U+0034 "4" */
    0x7, 0x7, 0x86, 0xc6, 0x66, 0x37, 0xff, 0xfe,
    0x6, 0x3, 0x0,

    /* U+0035 "5" */
    0xff, 0x7f, 0xb0, 0x19, 0xec, 0xf8, 0xf, 0x7,
    0xff, 0x7f, 0x0,

    /* U+0036 "6" */
    0x7f, 0x7f, 0xb0, 0x19, 0xec, 0xfe, 0xf, 0x7,
    0xff, 0x7f, 0x0,

    /* U+0037 "7" */
    0x7f, 0xdf, 0xf0, 0x0, 0xe, 0x7, 0x1, 0x80,
    0xe0, 0x70, 0x38, 0x0,

    /* U+0038 "8" */
    0x7f, 0x7f, 0xf0, 0x7f, 0xff, 0xf6, 0xf, 0x7,
    0xff, 0x7f, 0x0,

    /* U+0039 "9" */
    0x7f, 0x7f, 0xf0, 0x78, 0x3f, 0x9b, 0xcc, 0x6,
    0xff, 0x7f, 0x0,

    /* U+003A ":" */
    0xf0, 0xf,

    /* U+003B ";" */
    0xf0, 0xf, 0x80,

    /* U+003F "?" */
    0xff, 0x3f, 0xc0, 0x60, 0x33, 0xf9, 0xf8, 0xc0,
    0x60, 0x30, 0x18, 0x0,

    /* U+0041 "A" */
    0xe, 0x1, 0xc0, 0x7c, 0xd, 0x83, 0x80, 0x60,
    0x1f, 0xf3, 0xfe, 0xe0, 0xe0,

    /* U+0042 "B" */
    0xff, 0xdf, 0xff, 0x1, 0xff, 0xff, 0xfc, 0x0,
    0xc0, 0x1f, 0xff, 0xff, 0xc0,

    /* U+0043 "C" */
    0x7f, 0xff, 0xfc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x3f, 0x4f, 0xc0,

    /* U+0044 "D" */
    0xff, 0xbf, 0xfc, 0xf, 0x3, 0xc0, 0xf0, 0x30,
    0xf, 0xff, 0xff, 0x80,

    /* U+0045 "E" */
    0x7f, 0xff, 0xfc, 0x3, 0x3e, 0xcf, 0xb0, 0xc,
    0x3, 0x3f, 0xcf, 0xc0,

    /* U+0046 "F" */
    0x7f, 0xff, 0xfc, 0x3, 0x3e, 0xcf, 0xb0, 0xc,
    0x3, 0x0, 0xc0, 0x0,

    /* U+0047 "G" */
    0x7f, 0xff, 0xfc, 0x3, 0x3f, 0xc7, 0xf0, 0x3c,
    0xf, 0xf0, 0x7c, 0x0,

    /* U+0048 "H" */
    0xc0, 0x78, 0xc, 0x1, 0xff, 0xff, 0xff, 0x80,
    0x30, 0x6, 0x3, 0xc0, 0x60,

    /* U+0049 "I" */
    0xff, 0xff, 0xc0,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0xc3, 0xff,
    0xfe,

    /* U+004B "K" */
    0xc7, 0x67, 0x37, 0x1f, 0xf, 0x7, 0x3, 0x91,
    0x8c, 0xc3, 0x0,

    /* U+004C "L" */
    0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1,
    0xff, 0xff, 0x80,

    /* U+004D "M" */
    0xc0, 0x3e, 0x3, 0xf0, 0xbf, 0x8b, 0xf9, 0xbd,
    0xfb, 0xcf, 0x3c, 0x63, 0xc0, 0x30,

    /* U+004E "N" */
    0xc1, 0xf0, 0xfc, 0x7f, 0x3b, 0xcc, 0xf2, 0x3d,
    0xf, 0x3, 0xc0, 0xc0,

    /* U+004F "O" */
    0x7f, 0x5f, 0xef, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0xff, 0x5f, 0xc0,

    /* U+0050 "P" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0xff, 0xff, 0xec,
    0x3, 0x0, 0xc0, 0x0,

    /* U+0051 "Q" */
    0x7f, 0x5f, 0xef, 0x1, 0xe0, 0x3c, 0x7, 0x8c,
    0xf1, 0x9f, 0x3f, 0x67, 0xc0, 0xc0,

    /* U+0052 "R" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0xcf, 0xfb, 0xec,
    0x73, 0xe, 0xc1, 0xc0,

    /* U+0053 "S" */
    0x7f, 0xff, 0xff, 0x0, 0x7f, 0xe7, 0xfe, 0x0,
    0xc0, 0x1f, 0xff, 0xff, 0xc0,

    /* U+0054 "T" */
    0xff, 0xff, 0xf0, 0x0, 0x0, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x0,

    /* U+0055 "U" */
    0xc0, 0x78, 0xf, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1f, 0xff, 0x7f, 0xc0,

    /* U+0056 "V" */
    0xe0, 0xec, 0x19, 0xc7, 0x18, 0xc3, 0xb8, 0x26,
    0x5, 0xc0, 0x30, 0xe, 0x0,

    /* U+0057 "W" */
    0xee, 0xee, 0xcd, 0x99, 0xdf, 0x71, 0x9c, 0xc3,
    0xbb, 0x82, 0x66, 0x5, 0xdc, 0x3, 0x30, 0xe,
    0xe0,

    /* U+0058 "X" */
    0x61, 0xdc, 0xe3, 0xf0, 0x78, 0xe, 0x7, 0x43,
    0xb1, 0xce, 0xe1, 0xc0,

    /* U+0059 "Y" */
    0xe1, 0xdc, 0xe3, 0x70, 0x38, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x0,

    /* U+005A "Z" */
    0xff, 0xdf, 0xf0, 0x38, 0x3c, 0x1c, 0xe, 0x0,
    0x3, 0xfe, 0xff, 0x80,

    /* U+0061 "a" */
    0xe, 0x1, 0xc0, 0x7c, 0xd, 0x83, 0x80, 0x60,
    0x1f, 0xf3, 0xfe, 0xe0, 0xe0,

    /* U+0062 "b" */
    0xff, 0xdf, 0xff, 0x1, 0xff, 0xff, 0xfc, 0x0,
    0xc0, 0x1f, 0xff, 0xff, 0xc0,

    /* U+0063 "c" */
    0x7f, 0xff, 0xfc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x3f, 0x4f, 0xc0,

    /* U+0064 "d" */
    0xff, 0xbf, 0xfc, 0xf, 0x3, 0xc0, 0xf0, 0x30,
    0xf, 0xff, 0xff, 0x80,

    /* U+0065 "e" */
    0x7f, 0xff, 0xfc, 0x3, 0x3e, 0xcf, 0xb0, 0xc,
    0x3, 0x3f, 0xcf, 0xc0,

    /* U+0066 "f" */
    0x7f, 0xff, 0xfc, 0x3, 0x3e, 0xcf, 0xb0, 0xc,
    0x3, 0x0, 0xc0, 0x0,

    /* U+0067 "g" */
    0x7f, 0xff, 0xfc, 0x3, 0x3f, 0xc7, 0xf0, 0x3c,
    0xf, 0xf0, 0x7c, 0x0,

    /* U+0068 "h" */
    0xc0, 0x78, 0xc, 0x1, 0xff, 0xff, 0xff, 0x80,
    0x30, 0x6, 0x3, 0xc0, 0x60,

    /* U+0069 "i" */
    0xff, 0xff, 0xc0,

    /* U+006A "j" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0xc3, 0xff,
    0xfe,

    /* U+006B "k" */
    0xc7, 0x67, 0x37, 0x1f, 0xf, 0x7, 0x3, 0x91,
    0x8c, 0xc3, 0x0,

    /* U+006C "l" */
    0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1,
    0xff, 0xff, 0x80,

    /* U+006D "m" */
    0xc0, 0x3e, 0x3, 0xf0, 0xbf, 0x8b, 0xf9, 0xbd,
    0xfb, 0xcf, 0x3c, 0x63, 0xc0, 0x30,

    /* U+006E "n" */
    0xc1, 0xf0, 0xfc, 0x7f, 0x3b, 0xcc, 0xf2, 0x3d,
    0xf, 0x3, 0xc0, 0xc0,

    /* U+006F "o" */
    0x7f, 0x5f, 0xef, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0xff, 0x5f, 0xc0,

    /* U+0070 "p" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0xff, 0xff, 0xec,
    0x3, 0x0, 0xc0, 0x0,

    /* U+0071 "q" */
    0x7f, 0x5f, 0xef, 0x1, 0xe0, 0x3c, 0x7, 0x8c,
    0xf1, 0x9f, 0x3f, 0x67, 0xc0, 0xc0,

    /* U+0072 "r" */
    0xff, 0xbf, 0xf0, 0xc, 0x3, 0xcf, 0xfb, 0xec,
    0x73, 0xe, 0xc1, 0xc0,

    /* U+0073 "s" */
    0x7f, 0xff, 0xff, 0x0, 0x7f, 0xe7, 0xfe, 0x0,
    0xc0, 0x1f, 0xff, 0xff, 0xc0,

    /* U+0074 "t" */
    0xff, 0xff, 0xf0, 0x0, 0x0, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x0,

    /* U+0075 "u" */
    0xc0, 0x78, 0xf, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1f, 0xff, 0x7f, 0xc0,

    /* U+0076 "v" */
    0xe0, 0xec, 0x19, 0xc7, 0x18, 0xc3, 0xb8, 0x26,
    0x5, 0xc0, 0x30, 0xe, 0x0,

    /* U+0077 "w" */
    0xee, 0xee, 0xcd, 0x99, 0xdf, 0x71, 0x9c, 0xc3,
    0xbb, 0x82, 0x66, 0x5, 0xdc, 0x3, 0x30, 0xe,
    0xe0,

    /* U+0078 "x" */
    0x61, 0xdc, 0xe3, 0xf0, 0x78, 0xe, 0x7, 0x43,
    0xb1, 0xce, 0xe1, 0xc0,

    /* U+0079 "y" */
    0xe1, 0xdc, 0xe3, 0x70, 0x38, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x0,

    /* U+007A "z" */
    0xff, 0xdf, 0xf0, 0x38, 0x3c, 0x1c, 0xe, 0x0,
    0x3, 0xfe, 0xff, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 102, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 55, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 88, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 191, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 19, .adv_w = 170, .box_w = 10, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 36, .adv_w = 129, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 49, .adv_w = 51, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 51, .adv_w = 63, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 64, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 51, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 62, .adv_w = 110, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 64, .adv_w = 53, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 65, .adv_w = 81, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 154, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 82, .adv_w = 81, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 87, .adv_w = 157, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 98, .adv_w = 129, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 106, .adv_w = 161, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 117, .adv_w = 152, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 128, .adv_w = 152, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 139, .adv_w = 164, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 151, .adv_w = 167, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 162, .adv_w = 154, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 173, .adv_w = 55, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 175, .adv_w = 54, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 178, .adv_w = 158, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 190, .adv_w = 187, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 203, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 216, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 228, .adv_w = 173, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 240, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 252, .adv_w = 173, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 264, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 276, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 289, .adv_w = 49, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 292, .adv_w = 136, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 301, .adv_w = 154, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 312, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 323, .adv_w = 204, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 337, .adv_w = 170, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 349, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 362, .adv_w = 172, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 374, .adv_w = 194, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 388, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 400, .adv_w = 185, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 413, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 425, .adv_w = 193, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 438, .adv_w = 186, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 451, .adv_w = 249, .box_w = 15, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 468, .adv_w = 175, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 480, .adv_w = 171, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 492, .adv_w = 166, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 504, .adv_w = 187, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 517, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 530, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 542, .adv_w = 173, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 554, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 566, .adv_w = 173, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 578, .adv_w = 174, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 590, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 603, .adv_w = 49, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 606, .adv_w = 136, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 615, .adv_w = 154, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 626, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 637, .adv_w = 204, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 651, .adv_w = 170, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 663, .adv_w = 194, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 676, .adv_w = 172, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 688, .adv_w = 194, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 714, .adv_w = 185, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 727, .adv_w = 178, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 739, .adv_w = 193, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 752, .adv_w = 186, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 765, .adv_w = 249, .box_w = 15, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 782, .adv_w = 175, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 794, .adv_w = 171, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 806, .adv_w = 166, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 1}
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
    -25, -4, -3, -23, -16, -19, -6, -14,
    -9, -20, -8, -12, -5, -18, -12, -18,
    -5, -7, -10, -14, -13, -12, -8, -23,
    -23, -12, -13, -5, -15, -7, -12, -18,
    0, -19, -6, -22, -5, -16, -21, -16,
    -17, -17, -4, -18, -24, -6, -6, -19,
    -4, -17, -23, -3, -6, -19, -11, -14,
    -15, -5, -4, -12, -6, -12, -9, -13,
    -3, -8, -3, -7, -4, -17, -21, -6,
    -21, -12, -8, -11, -4, -3, -16, -17,
    -16, -29, -12, -13, -10, -30, -34, -9,
    -13, -11, -14, -4, -21, -11, -2, -21,
    -8, -20, -8, -8, -8, -8, -8, -48,
    -7, -60, -60, -65, -8, -8, -8, -8,
    -48, -7, -60, -60, -65, -11, -8, -8,
    -13, -17, -11, -8, -8, -13, -17, -12,
    -12, -11, -16, -22, -12, -12, -11, -16,
    -22, -38, -22, -38, -22, -7, -7, -13,
    -13, -16, -16, -16, -16, -5, -6, -5,
    -7, -16, -16, -16, -16, -5, -6, -5,
    -7, -55, -49, -56, -48, -61, -57, -52,
    -53, -12, -12, -12, -16, -23, -12, -12,
    -12, -16, -23, -40, -54, -9, -9, -22,
    -19, -13, -40, -54, -9, -9, -22, -19,
    -13, -12, -12, -12, -16, -23, -12, -12,
    -12, -16, -23, -10, -10, -21, -10, -10,
    -21, -9, -4, -4, -7, -8, -9, -4,
    -4, -7, -8, -46, -62, -45, -62, -12,
    -12, -54, -7, -7, -7, -7, -56, -7,
    -7, -6, -54, -7, -7, -7, -7, -56,
    -7, -7, -6, -54, -7, -7, -7, -7,
    -42, -7, -7, -6, -54, -7, -7, -7,
    -7, -42, -7, -7, -6, -12, -12, -12,
    -12, -3, -12, -12, -12, -12, -3, -62,
    -18, -18, -18, -18, -63, -18, -18, -16,
    -62, -18, -18, -18, -18, -63, -18, -18,
    -16, -8, -8, -8, -8, -39, -7, -60,
    -60, -65, -8, -8, -8, -8, -40, -7,
    -60, -60, -65, -11, -8, -8, -13, -17,
    -11, -8, -8, -13, -17, -12, -12, -11,
    -16, -22, -12, -12, -11, -16, -22, -42,
    -25, -42, -29, -24, -24, -13, -13, -16,
    -16, -16, -16, -5, -6, -5, -7, -16,
    -16, -16, -16, -5, -6, -5, -7, -62,
    -53, -56, -52, -62, -48, -54, -62, -12,
    -12, -12, -16, -23, -12, -12, -12, -16,
    -23, -40, -14, -9, -9, -22, -19, -13,
    -40, -12, -9, -9, -22, -19, -13, -12,
    -12, -12, -16, -23, -12, -12, -12, -16,
    -23, -10, -10, -21, -10, -10, -21, -9,
    -4, -4, -7, -8, -9, -4, -4, -7,
    -8, -52, -62, -55, -62, -12, -12, -54,
    -7, -7, -7, -7, -39, -7, -7, -6,
    -54, -7, -7, -7, -7, -39, -7, -7,
    -6, -54, -7, -7, -7, -7, -42, -7,
    -7, -6, -54, -7, -7, -7, -7, -42,
    -7, -7, -6, -12, -12, -12, -12, -3,
    -12, -12, -12, -12, -3, -62, -18, -18,
    -18, -18, -63, -18, -18, -16, -62, -18,
    -18, -18, -18, -63, -18, -18, -16
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
const lv_font_t ui_font_Arteon12 = {
#else
lv_font_t ui_font_Arteon12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
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



#endif /*#if UI_FONT_ARTEON12*/

