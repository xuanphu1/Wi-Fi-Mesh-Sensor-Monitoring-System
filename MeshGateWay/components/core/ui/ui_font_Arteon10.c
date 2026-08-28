/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --bpp 1 --size 10 --font C:/Users/MrKoi/SquareLine/assets/Arteon-m2vP2.otf -o C:/Users/MrKoi/SquareLine/assets\ui_font_Arteon10.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_ARTEON10
#define UI_FONT_ARTEON10 1
#endif

#if UI_FONT_ARTEON10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xcf, 0x0,

    /* U+0022 "\"" */
    0xba,

    /* U+0023 "#" */
    0x19, 0x9f, 0xe7, 0xf8, 0xcc, 0x32, 0x3f, 0xcf,
    0xf1, 0x98,

    /* U+0024 "$" */
    0xc, 0x3f, 0xdf, 0xd8, 0x7, 0xf3, 0xfd, 0xfe,
    0xfe, 0xc, 0x0,

    /* U+0026 "&" */
    0x18, 0x67, 0xff, 0xc1, 0xff, 0xff, 0x7c, 0x61,
    0x80,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7f, 0x6d, 0xb7, 0x60,

    /* U+0029 ")" */
    0xdd, 0xb6, 0xdf, 0xc0,

    /* U+002C "," */
    0xe0,

    /* U+002D "-" */
    0xff, 0xc0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x32, 0x26, 0x64, 0x4c, 0xc0,

    /* U+0030 "0" */
    0x7d, 0xff, 0x1e, 0x3c, 0x78, 0xff, 0xbe,

    /* U+0031 "1" */
    0x77, 0x0, 0x33, 0x33,

    /* U+0032 "2" */
    0x7e, 0x7e, 0x3, 0x7e, 0xfe, 0xc0, 0xfe, 0x7e,

    /* U+0033 "3" */
    0xfb, 0xe0, 0xbe, 0xf8, 0x2f, 0xbe,

    /* U+0034 "4" */
    0xe, 0x1e, 0x3e, 0x76, 0xe6, 0xff, 0xff, 0x6,

    /* U+0035 "5" */
    0xfd, 0xfb, 0x6, 0xed, 0xe0, 0xff, 0xbe,

    /* U+0036 "6" */
    0x7d, 0xfb, 0x6, 0xed, 0xf8, 0xff, 0xbe,

    /* U+0037 "7" */
    0xff, 0x7f, 0x0, 0x6, 0xc, 0x18, 0x38, 0x30,

    /* U+0038 "8" */
    0x7e, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 0x7e,

    /* U+0039 "9" */
    0x7d, 0xff, 0x1e, 0x3f, 0x6e, 0xdf, 0xbe,

    /* U+003A ":" */
    0xf0, 0xf0,

    /* U+003B ";" */
    0xf0, 0x38,

    /* U+003F "?" */
    0xfe, 0x7f, 0x3, 0x3f, 0x3e, 0x30, 0x30, 0x30,
    0x10,

    /* U+0041 "A" */
    0xc, 0xe, 0x5, 0x86, 0x3, 0x3, 0xf9, 0xfd,
    0x83,

    /* U+0042 "B" */
    0xff, 0xbf, 0xec, 0xb, 0xfe, 0xff, 0x80, 0x2f,
    0xfb, 0xfe,

    /* U+0043 "C" */
    0x7f, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xdf, 0x5f,

    /* U+0044 "D" */
    0xfe, 0xff, 0xc3, 0xc3, 0xc3, 0x3, 0xff, 0xfe,

    /* U+0045 "E" */
    0x7f, 0xff, 0xc0, 0xde, 0xde, 0xc0, 0xdf, 0xdf,

    /* U+0046 "F" */
    0x7f, 0xff, 0xf0, 0x1b, 0xcd, 0xe6, 0x3, 0x1,
    0x80,

    /* U+0047 "G" */
    0x7f, 0xff, 0xc0, 0xdf, 0xcf, 0xc3, 0xf8, 0x78,

    /* U+0048 "H" */
    0xc1, 0xe0, 0xc0, 0x7f, 0xff, 0xfe, 0x3, 0x7,
    0x83,

    /* U+0049 "I" */
    0xff, 0xff,

    /* U+004A "J" */
    0xc, 0x30, 0xc3, 0xf, 0x3f, 0xfe,

    /* U+004B "K" */
    0xc6, 0xcc, 0xd8, 0xf0, 0xf0, 0xe8, 0xcc, 0xc6,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff,

    /* U+004D "M" */
    0xc0, 0xf8, 0x3f, 0xf, 0xcb, 0xde, 0xf7, 0xbc,
    0xcf, 0x3,

    /* U+004E "N" */
    0xc3, 0xc7, 0xcf, 0xdb, 0xdb, 0xd3, 0xc3, 0xc3,

    /* U+004F "O" */
    0x7d, 0x7e, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7e,
    0xbe,

    /* U+0050 "P" */
    0xfe, 0xff, 0x3, 0x3, 0xff, 0xfe, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x7d, 0x7e, 0xf0, 0x78, 0x3c, 0x1e, 0x6f, 0xbe,
    0xde, 0xc, 0x0,

    /* U+0052 "R" */
    0xff, 0x7f, 0xc0, 0x60, 0x3d, 0xff, 0x7b, 0x19,
    0x86,

    /* U+0053 "S" */
    0x7f, 0xff, 0xf0, 0x1f, 0xe7, 0xf8, 0xf, 0xff,
    0xfe,

    /* U+0054 "T" */
    0xff, 0xff, 0xc0, 0x0, 0x1, 0x80, 0xc0, 0x60,
    0x30,

    /* U+0055 "U" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0xfe,
    0xfe,

    /* U+0056 "V" */
    0xc1, 0xb0, 0xd8, 0xc6, 0x63, 0x61, 0x30, 0x30,
    0x18,

    /* U+0057 "W" */
    0xd9, 0xb6, 0xdb, 0x6f, 0x63, 0x76, 0x36, 0xc2,
    0x6c, 0xd, 0x80, 0xd8,

    /* U+0058 "X" */
    0xe3, 0x3b, 0xf, 0x83, 0x81, 0x81, 0xb1, 0xd9,
    0xc6,

    /* U+0059 "Y" */
    0xc3, 0x66, 0x2c, 0x1c, 0x18, 0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0x7f, 0x6, 0x1c, 0x38, 0x0, 0xfe, 0xff,

    /* U+0061 "a" */
    0xc, 0xe, 0x5, 0x86, 0x3, 0x3, 0xf9, 0xfd,
    0x83,

    /* U+0062 "b" */
    0xff, 0xbf, 0xec, 0xb, 0xfe, 0xff, 0x80, 0x2f,
    0xfb, 0xfe,

    /* U+0063 "c" */
    0x7f, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xdf, 0x5f,

    /* U+0064 "d" */
    0xfe, 0xff, 0xc3, 0xc3, 0xc3, 0x3, 0xff, 0xfe,

    /* U+0065 "e" */
    0x7f, 0xff, 0xc0, 0xde, 0xde, 0xc0, 0xdf, 0xdf,

    /* U+0066 "f" */
    0x7f, 0xff, 0xf0, 0x1b, 0xcd, 0xe6, 0x3, 0x1,
    0x80,

    /* U+0067 "g" */
    0x7f, 0xff, 0xc0, 0xdf, 0xcf, 0xc3, 0xf8, 0x78,

    /* U+0068 "h" */
    0xc1, 0xe0, 0xc0, 0x7f, 0xff, 0xfe, 0x3, 0x7,
    0x83,

    /* U+0069 "i" */
    0xff, 0xff,

    /* U+006A "j" */
    0xc, 0x30, 0xc3, 0xf, 0x3f, 0xfe,

    /* U+006B "k" */
    0xc6, 0xcc, 0xd8, 0xf0, 0xf0, 0xe8, 0xcc, 0xc6,

    /* U+006C "l" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff,

    /* U+006D "m" */
    0xc0, 0xf8, 0x3f, 0xf, 0xcb, 0xde, 0xf7, 0xbc,
    0xcf, 0x3,

    /* U+006E "n" */
    0xc3, 0xc7, 0xcf, 0xdb, 0xdb, 0xd3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x7d, 0x7e, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7e,
    0xbe,

    /* U+0070 "p" */
    0xfe, 0xff, 0x3, 0x3, 0xff, 0xfe, 0xc0, 0xc0,

    /* U+0071 "q" */
    0x7d, 0x7e, 0xf0, 0x78, 0x3c, 0x1e, 0x6f, 0xbe,
    0xde, 0xc, 0x0,

    /* U+0072 "r" */
    0xff, 0x7f, 0xc0, 0x60, 0x3d, 0xff, 0x7b, 0x19,
    0x86,

    /* U+0073 "s" */
    0x7f, 0xff, 0xf0, 0x1f, 0xe7, 0xf8, 0xf, 0xff,
    0xfe,

    /* U+0074 "t" */
    0xff, 0xff, 0xc0, 0x0, 0x1, 0x80, 0xc0, 0x60,
    0x30,

    /* U+0075 "u" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0xfe,
    0xfe,

    /* U+0076 "v" */
    0xc1, 0xb0, 0xd8, 0xc6, 0x63, 0x61, 0x30, 0x30,
    0x18,

    /* U+0077 "w" */
    0xd9, 0xb6, 0xdb, 0x6f, 0x63, 0x76, 0x36, 0xc2,
    0x6c, 0xd, 0x80, 0xd8,

    /* U+0078 "x" */
    0xe3, 0x3b, 0xf, 0x83, 0x81, 0x81, 0xb1, 0xd9,
    0xc6,

    /* U+0079 "y" */
    0xc3, 0x66, 0x2c, 0x1c, 0x18, 0x18, 0x18, 0x18,

    /* U+007A "z" */
    0xff, 0x7f, 0x6, 0x1c, 0x38, 0x0, 0xfe, 0xff
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 85, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 46, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 4, .adv_w = 73, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 5, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 142, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 26, .adv_w = 107, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 35, .adv_w = 43, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 36, .adv_w = 52, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 40, .adv_w = 53, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 44, .adv_w = 43, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 45, .adv_w = 92, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 47, .adv_w = 44, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 68, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 53, .adv_w = 128, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 67, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 131, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 108, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 134, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 127, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 127, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 137, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 140, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 129, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 46, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 125, .adv_w = 45, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 131, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 156, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 162, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 144, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 144, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 162, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 40, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 113, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 128, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 132, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 170, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 141, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 161, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 143, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 264, .adv_w = 161, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 275, .adv_w = 149, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 155, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 148, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 161, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 155, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 208, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 332, .adv_w = 146, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 143, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 349, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 156, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 162, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 376, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 144, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 400, .adv_w = 144, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 162, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 40, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 113, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 128, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 132, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 170, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 460, .adv_w = 141, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 161, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 477, .adv_w = 143, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 161, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 496, .adv_w = 149, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 505, .adv_w = 155, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 148, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 523, .adv_w = 161, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 532, .adv_w = 155, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 208, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 146, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 562, .adv_w = 143, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 570, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0}
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
    -21, -3, -3, -19, -13, -16, -5, -12,
    -7, -17, -7, -10, -4, -15, -10, -15,
    -4, -6, -9, -12, -11, -10, -7, -19,
    -19, -10, -11, -4, -13, -6, -10, -15,
    0, -16, -5, -18, -4, -13, -18, -13,
    -14, -14, -3, -15, -20, -5, -5, -16,
    -3, -14, -19, -3, -5, -16, -9, -12,
    -13, -4, -4, -10, -5, -10, -8, -11,
    -3, -7, -2, -6, -3, -14, -17, -5,
    -17, -10, -7, -9, -3, -3, -13, -14,
    -14, -24, -10, -11, -8, -25, -28, -8,
    -11, -9, -12, -3, -17, -9, -2, -18,
    -6, -17, -7, -7, -7, -7, -7, -40,
    -6, -50, -50, -54, -7, -7, -7, -7,
    -40, -6, -50, -50, -54, -9, -7, -6,
    -11, -15, -9, -7, -6, -11, -15, -10,
    -10, -9, -13, -19, -10, -10, -9, -13,
    -19, -32, -18, -32, -18, -6, -6, -11,
    -11, -14, -13, -14, -13, -4, -5, -4,
    -6, -14, -13, -14, -13, -4, -5, -4,
    -6, -45, -41, -46, -40, -51, -47, -43,
    -44, -10, -10, -10, -13, -19, -10, -10,
    -10, -13, -19, -33, -45, -8, -7, -18,
    -16, -11, -33, -45, -8, -7, -18, -16,
    -11, -10, -10, -10, -13, -19, -10, -10,
    -10, -13, -19, -8, -8, -17, -8, -8,
    -17, -7, -3, -3, -6, -7, -7, -3,
    -3, -6, -7, -38, -52, -37, -52, -10,
    -10, -45, -6, -6, -6, -6, -47, -6,
    -6, -5, -45, -6, -6, -6, -6, -47,
    -6, -6, -5, -45, -6, -6, -6, -6,
    -35, -6, -6, -5, -45, -6, -6, -6,
    -6, -35, -6, -6, -5, -10, -10, -10,
    -10, -3, -10, -10, -10, -10, -3, -52,
    -15, -15, -15, -15, -52, -15, -15, -13,
    -52, -15, -15, -15, -15, -52, -15, -15,
    -13, -7, -7, -7, -7, -32, -6, -50,
    -50, -54, -7, -7, -7, -7, -33, -6,
    -50, -50, -54, -9, -7, -6, -11, -15,
    -9, -7, -6, -11, -15, -10, -10, -9,
    -13, -19, -10, -10, -9, -13, -19, -35,
    -21, -35, -24, -20, -20, -11, -11, -14,
    -13, -14, -13, -4, -5, -4, -6, -14,
    -13, -14, -13, -4, -5, -4, -6, -52,
    -44, -46, -43, -52, -40, -45, -52, -10,
    -10, -10, -13, -19, -10, -10, -10, -13,
    -19, -33, -12, -8, -7, -18, -16, -11,
    -33, -10, -8, -7, -18, -16, -11, -10,
    -10, -10, -13, -19, -10, -10, -10, -13,
    -19, -8, -8, -17, -8, -8, -17, -7,
    -3, -3, -6, -7, -7, -3, -3, -6,
    -7, -44, -52, -46, -52, -10, -10, -45,
    -6, -6, -6, -6, -33, -6, -6, -5,
    -45, -6, -6, -6, -6, -33, -6, -6,
    -5, -45, -6, -6, -6, -6, -35, -6,
    -6, -5, -45, -6, -6, -6, -6, -35,
    -6, -6, -5, -10, -10, -10, -10, -3,
    -10, -10, -10, -10, -3, -52, -15, -15,
    -15, -15, -52, -15, -15, -13, -52, -15,
    -15, -15, -15, -52, -15, -15, -13
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
const lv_font_t ui_font_Arteon10 = {
#else
lv_font_t ui_font_Arteon10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if UI_FONT_ARTEON10*/

