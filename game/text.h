// QUASAR - bitmap text.
#pragma once
#include "gfx.h"

#define FONT_W      6       // advance per character at scale 1
#define FONT_H      10      // line height at scale 1

// special glyphs
#define G_UP        "\x01"
#define G_DOWN      "\x02"
#define G_LEFT      "\x03"
#define G_RIGHT     "\x04"
#define G_HEART     "\x05"
#define G_STAR      "\x06"
#define G_DOT       "\x07"
#define G_TRI_R     "\x08"
#define G_TRI_L     "\x0b"
#define G_CHECK     "\x0e"
#define G_CROSS     "\x0f"

enum {
    TX_SHADOW   = 0x01,     // dark drop shadow
    TX_OUTLINE  = 0x02,     // dark 1px outline all around
    TX_GRAD     = 0x04,     // vertical gradient using the palette passed in
    TX_ADD      = 0x08,     // additive
};

int text_width(const char *s, int scale);
int text_draw(int x, int y, const char *s, px_t col, int scale);
int text_draw_fx(int x, int y, const char *s, px_t col, int scale, int flags, const px_t *grad);
void text_center(int y, const char *s, px_t col, int scale, int flags);
void text_center_x(int cx, int y, const char *s, px_t col, int scale, int flags);
void text_right(int rx, int y, const char *s, px_t col, int scale, int flags);

// small formatting helpers (no printf on device)
char *fmt_int(char *buf, long v);
char *fmt_score(char *buf, unsigned long v, int min_digits);   // zero padded
char *fmt_commas(char *buf, unsigned long v);
char *str_cat(char *dst, const char *src);
