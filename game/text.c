// QUASAR - bitmap text rendering.
#include "text.h"
#include <string.h>

extern const uint8_t g_font5x9[128 * 9];

static px_t s_shadow = COL(8, 6, 20);

int text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * FONT_W - 1) * scale : 0;
}

static void glyph(int x, int y, unsigned char ch, px_t col, int scale, int mode, const px_t *grad)
{
    if(ch >= 128) ch = '?';
    const uint8_t *g = &g_font5x9[ch * 9];

    for(int r = 0; r < 9; r++) {
        uint8_t bits = g[r];
        if(!bits) continue;
        px_t c = grad ? grad[r] : col;
        for(int i = 0; i < 5; i++) {
            if(!(bits & (0x10 >> i))) continue;
            if(scale == 1) {
                if(mode == DM_ADD) gfx_padd(x + i, y + r, c);
                else gfx_pset(x + i, y + r, c);
            } else {
                gfx_fill_mode(x + i * scale, y + r * scale, scale, scale, c, mode);
            }
        }
    }
}

int text_draw_fx(int x, int y, const char *s, px_t col, int scale, int flags, const px_t *grad)
{
    int x0 = x;
    int mode = (flags & TX_ADD) ? DM_ADD : DM_NORMAL;
    const px_t *gp = (flags & TX_GRAD) ? grad : NULL;

    if(flags & (TX_OUTLINE | TX_SHADOW)) {
        int cx = x;
        for(const char *p = s; *p; p++, cx += FONT_W * scale) {
            unsigned char ch = (unsigned char)*p;
            if(ch == ' ') continue;
            if(flags & TX_OUTLINE) {
                for(int oy = -1; oy <= 1; oy++) {
                    for(int ox = -1; ox <= 1; ox++) {
                        if(ox || oy) glyph(cx + ox * scale, y + oy * scale, ch, s_shadow, scale, DM_NORMAL, NULL);
                    }
                }
                glyph(cx + scale, y + 2 * scale, ch, s_shadow, scale, DM_NORMAL, NULL);
            } else {
                glyph(cx + scale, y + scale, ch, s_shadow, scale, DM_NORMAL, NULL);
            }
        }
    }

    for(const char *p = s; *p; p++, x += FONT_W * scale) {
        unsigned char ch = (unsigned char)*p;
        if(ch != ' ') glyph(x, y, ch, col, scale, mode, gp);
    }
    return x - x0;
}

int text_draw(int x, int y, const char *s, px_t col, int scale)
{
    return text_draw_fx(x, y, s, col, scale, 0, NULL);
}

void text_center(int y, const char *s, px_t col, int scale, int flags)
{
    text_center_x(SCR_W / 2, y, s, col, scale, flags);
}

void text_center_x(int cx, int y, const char *s, px_t col, int scale, int flags)
{
    int w = text_width(s, scale);
    text_draw_fx(cx - w / 2, y, s, col, scale, flags, NULL);
}

void text_right(int rx, int y, const char *s, px_t col, int scale, int flags)
{
    int w = text_width(s, scale);
    text_draw_fx(rx - w, y, s, col, scale, flags, NULL);
}

char *fmt_int(char *buf, long v)
{
    char tmp[16];
    int n = 0;
    bool neg = v < 0;
    unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;
    do {
        tmp[n++] = (char)('0' + (u % 10));
        u /= 10;
    } while(u && n < 15);
    int k = 0;
    if(neg) buf[k++] = '-';
    while(n) buf[k++] = tmp[--n];
    buf[k] = 0;
    return buf;
}

char *fmt_score(char *buf, unsigned long v, int min_digits)
{
    char tmp[16];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while(v && n < 15);
    while(n < min_digits && n < 15) tmp[n++] = '0';
    int k = 0;
    while(n) buf[k++] = tmp[--n];
    buf[k] = 0;
    return buf;
}

char *fmt_commas(char *buf, unsigned long v)
{
    char tmp[24];
    int n = 0, d = 0;
    do {
        if(d && d % 3 == 0) tmp[n++] = ',';
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
        d++;
    } while(v && n < 22);
    int k = 0;
    while(n) buf[k++] = tmp[--n];
    buf[k] = 0;
    return buf;
}

char *str_cat(char *dst, const char *src)
{
    char *e = dst + strlen(dst);
    while((*e++ = *src++))
        ;
    return dst;
}
