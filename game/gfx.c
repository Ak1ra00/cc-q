// QUASAR - graphics core. See gfx.h for the frame buffer layout.
#include "gfx.h"
#include "gmath.h"

px_t *g_fb;
clip_t g_clip = { 0, 0, SCR_W, SCR_H };

void gfx_set_target(px_t *fb)
{
    g_fb = fb;
}

void gfx_clip(int x0, int y0, int x1, int y1)
{
    g_clip.x0 = (int16_t)iclamp(x0, 0, SCR_W);
    g_clip.y0 = (int16_t)iclamp(y0, 0, SCR_H);
    g_clip.x1 = (int16_t)iclamp(x1, 0, SCR_W);
    g_clip.y1 = (int16_t)iclamp(y1, 0, SCR_H);
}

void gfx_noclip(void)
{
    g_clip.x0 = 0;
    g_clip.y0 = 0;
    g_clip.x1 = SCR_W;
    g_clip.y1 = SCR_H;
}

void gfx_clear(px_t c)
{
    uint32_t v = ((uint32_t)c << 16) | c;
    uint32_t *p = (uint32_t *)g_fb;
    for(int i = 0; i < FB_PIX / 2; i += 4) {
        p[i] = v;
        p[i + 1] = v;
        p[i + 2] = v;
        p[i + 3] = v;
    }
}

// clip a rectangle; returns false if nothing left
static inline bool clip_rect(int *x, int *y, int *w, int *h)
{
    int x0 = *x, y0 = *y, x1 = *x + *w, y1 = *y + *h;
    if(x0 < g_clip.x0) x0 = g_clip.x0;
    if(y0 < g_clip.y0) y0 = g_clip.y0;
    if(x1 > g_clip.x1) x1 = g_clip.x1;
    if(y1 > g_clip.y1) y1 = g_clip.y1;
    if(x0 >= x1 || y0 >= y1) return false;
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
    return true;
}

void gfx_fill(int x, int y, int w, int h, px_t c)
{
    if(!clip_rect(&x, &y, &w, &h)) return;

    int x1 = x + w;
    while(x < x1) {
        int s = x >> STRIP_SHIFT;
        int se = (s + 1) << STRIP_SHIFT;
        if(se > x1) se = x1;
        int n = se - x;
        px_t *row = g_fb + s * STRIP_PIX + (y << STRIP_SHIFT) + (x & (STRIP_W - 1));
        for(int j = 0; j < h; j++, row += STRIP_W) {
            for(int i = 0; i < n; i++) row[i] = c;
        }
        x = se;
    }
}

void gfx_fill_mode(int x, int y, int w, int h, px_t c, int mode)
{
    if(mode == DM_NORMAL || mode == DM_FLASH) {
        gfx_fill(x, y, w, h, c);
        return;
    }
    if(!clip_rect(&x, &y, &w, &h)) return;

    int x1 = x + w;
    while(x < x1) {
        int s = x >> STRIP_SHIFT;
        int se = (s + 1) << STRIP_SHIFT;
        if(se > x1) se = x1;
        int n = se - x;
        px_t *row = g_fb + s * STRIP_PIX + (y << STRIP_SHIFT) + (x & (STRIP_W - 1));
        for(int j = 0; j < h; j++, row += STRIP_W) {
            switch(mode) {
                case DM_ADD:
                    for(int i = 0; i < n; i++) row[i] = px_add(row[i], c);
                    break;
                case DM_HALF:
                    for(int i = 0; i < n; i++) row[i] = px_half(row[i], c);
                    break;
                default:
                    for(int i = 0; i < n; i++) row[i] = px_scale(row[i], 14);
                    break;
            }
        }
        x = se;
    }
}

void gfx_fill_alpha(int x, int y, int w, int h, px_t c, int a32)
{
    if(a32 <= 0) return;
    if(a32 >= 32) {
        gfx_fill(x, y, w, h, c);
        return;
    }
    if(!clip_rect(&x, &y, &w, &h)) return;

    uint16_t fg = SWAP16(c);
    int x1 = x + w;
    while(x < x1) {
        int s = x >> STRIP_SHIFT;
        int se = (s + 1) << STRIP_SHIFT;
        if(se > x1) se = x1;
        int n = se - x;
        px_t *row = g_fb + s * STRIP_PIX + (y << STRIP_SHIFT) + (x & (STRIP_W - 1));
        for(int j = 0; j < h; j++, row += STRIP_W) {
            for(int i = 0; i < n; i++) {
                row[i] = SWAP16(c565_mix(fg, SWAP16(row[i]), a32));
            }
        }
        x = se;
    }
}

void gfx_hline(int x0, int x1, int y, px_t c)
{
    if(x1 < x0) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    gfx_fill(x0, y, x1 - x0 + 1, 1, c);
}

void gfx_vline(int x, int y0, int y1, px_t c)
{
    if(y1 < y0) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    gfx_fill(x, y0, 1, y1 - y0 + 1, c);
}

void gfx_rect(int x, int y, int w, int h, px_t c)
{
    if(w <= 0 || h <= 0) return;
    gfx_fill(x, y, w, 1, c);
    gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y + 1, 1, h - 2, c);
    gfx_fill(x + w - 1, y + 1, 1, h - 2, c);
}

static inline bool in_clip(int x, int y)
{
    return x >= g_clip.x0 && y >= g_clip.y0 && x < g_clip.x1 && y < g_clip.y1;
}

void gfx_pset(int x, int y, px_t c)
{
    if(in_clip(x, y)) g_fb[FBI(x, y)] = c;
}

void gfx_padd(int x, int y, px_t c)
{
    if(in_clip(x, y)) {
        px_t *p = &g_fb[FBI(x, y)];
        *p = px_add(*p, c);
    }
}

static inline void plot_mode(int x, int y, px_t c, int mode)
{
    if(!in_clip(x, y)) return;
    px_t *p = &g_fb[FBI(x, y)];
    switch(mode) {
        case DM_ADD:    *p = px_add(*p, c); break;
        case DM_HALF:   *p = px_half(*p, c); break;
        case DM_SHADOW: *p = px_scale(*p, 14); break;
        default:        *p = c; break;
    }
}

void gfx_line(int x0, int y0, int x1, int y1, px_t c, int mode)
{
    int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    // trivially reject long lines far outside
    for(int guard = 0; guard < 2048; guard++) {
        plot_mode(x0, y0, c, mode);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_circle(int cx, int cy, int r, px_t c, int mode)
{
    if(r <= 0) {
        plot_mode(cx, cy, c, mode);
        return;
    }
    int x = r, y = 0, err = 1 - r;
    while(x >= y) {
        plot_mode(cx + x, cy + y, c, mode);
        plot_mode(cx + y, cy + x, c, mode);
        plot_mode(cx - y, cy + x, c, mode);
        plot_mode(cx - x, cy + y, c, mode);
        if(y) {
            plot_mode(cx - x, cy - y, c, mode);
            plot_mode(cx - y, cy - x, c, mode);
            plot_mode(cx + y, cy - x, c, mode);
            plot_mode(cx + x, cy - y, c, mode);
        }
        y++;
        if(err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_disc(int cx, int cy, int r, px_t c, int mode)
{
    if(r < 0) return;
    int rr = r * r + r;
    for(int dy = -r; dy <= r; dy++) {
        int hw = isqrt((uint32_t)(rr - dy * dy));
        gfx_fill_mode(cx - hw, cy + dy, 2 * hw + 1, 1, c, mode);
    }
}

void gfx_ring(int cx, int cy, int r0, int r1, px_t c, int mode)
{
    // filled annulus between radius r0 (inner) and r1 (outer)
    if(r1 <= 0) return;
    if(r0 < 0) r0 = 0;
    int o2 = r1 * r1 + r1, i2 = r0 * r0 - r0;
    for(int dy = -r1; dy <= r1; dy++) {
        int y = cy + dy;
        if(y < g_clip.y0 || y >= g_clip.y1) continue;
        int ow = isqrt((uint32_t)(o2 - dy * dy));
        int inner = i2 - dy * dy;
        if(inner <= 0 || r0 == 0) {
            gfx_fill_mode(cx - ow, y, 2 * ow + 1, 1, c, mode);
        } else {
            int iw = isqrt((uint32_t)inner);
            gfx_fill_mode(cx - ow, y, ow - iw, 1, c, mode);
            gfx_fill_mode(cx + iw + 1, y, ow - iw, 1, c, mode);
        }
    }
}

void gfx_vgrad(int x, int y, int w, int h, px_t top, px_t bot)
{
    if(h <= 0) return;
    uint16_t a = SWAP16(top), b = SWAP16(bot);
    for(int j = 0; j < h; j++) {
        int k = (h > 1) ? (j * 32) / (h - 1) : 0;
        uint16_t c = c565_mix(b, a, k);
        gfx_fill(x, y + j, w, 1, SWAP16(c));
    }
}

void gfx_hgrad(int x, int y, int w, int h, px_t left, px_t right)
{
    if(w <= 0) return;
    uint16_t a = SWAP16(left), b = SWAP16(right);
    for(int i = 0; i < w; i++) {
        int k = (w > 1) ? (i * 32) / (w - 1) : 0;
        uint16_t c = c565_mix(b, a, k);
        gfx_fill(x + i, y, 1, h, SWAP16(c));
    }
}

// glow falloff lookup, index = d2 * 64 / r2 (0..63), value 0..32
static const uint8_t glow_lut[64] = {
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 20, 19, 18,
    17, 16, 16, 15, 14, 14, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8,
    8, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
};

void gfx_glow(int cx, int cy, int r, px_t c)
{
    if(r <= 0) return;
    if(r == 1) {
        gfx_padd(cx, cy, c);
        return;
    }
    int x0 = cx - r, x1 = cx + r + 1, y0 = cy - r, y1 = cy + r + 1;
    if(x0 < g_clip.x0) x0 = g_clip.x0;
    if(y0 < g_clip.y0) y0 = g_clip.y0;
    if(x1 > g_clip.x1) x1 = g_clip.x1;
    if(y1 > g_clip.y1) y1 = g_clip.y1;
    if(x0 >= x1 || y0 >= y1) return;

    uint16_t base = SWAP16(c);
    uint16_t shade[33];
    for(int k = 0; k <= 32; k++) shade[k] = c565_scale(base, k);

    int r2 = r * r;
    int inv = (64 << 16) / r2;
    for(int y = y0; y < y1; y++) {
        int dy = y - cy, dy2 = dy * dy;
        for(int x = x0; x < x1; x++) {
            int dx = x - cx;
            int d2 = dx * dx + dy2;
            if(d2 >= r2) continue;
            int k = glow_lut[(d2 * inv) >> 16];
            if(!k) continue;
            px_t *p = &g_fb[FBI(x, y)];
            *p = SWAP16(c565_add(SWAP16(*p), shade[k]));
        }
    }
}

// ---- sprites ----

#define SPRITE_LOOP(BODY)                                                                   \
    do {                                                                                   \
        for(int sx = cx0; sx < cx1;) {                                                     \
            int strip = sx >> STRIP_SHIFT;                                                  \
            int se = (strip + 1) << STRIP_SHIFT;                                            \
            if(se > cx1) se = cx1;                                                          \
            int n = se - sx;                                                                \
            px_t *drow = g_fb + strip * STRIP_PIX + (cy0 << STRIP_SHIFT) + (sx & (STRIP_W - 1)); \
            for(int y = cy0; y < cy1; y++, drow += STRIP_W) {                               \
                int ty = (flags & SPR_FLIPY) ? (s->h - 1 - (y - y0)) : (y - y0);            \
                const uint8_t *srow = s->pix + ty * s->w;                                   \
                if(flags & SPR_FLIPX) {                                                     \
                    const uint8_t *sp = srow + (s->w - 1 - (sx - x0));                      \
                    for(int i = 0; i < n; i++) {                                            \
                        uint8_t v = sp[-i];                                                 \
                        if(v) { px_t *d = &drow[i]; BODY; }                                 \
                    }                                                                       \
                } else {                                                                    \
                    const uint8_t *sp = srow + (sx - x0);                                   \
                    for(int i = 0; i < n; i++) {                                            \
                        uint8_t v = sp[i];                                                  \
                        if(v) { px_t *d = &drow[i]; BODY; }                                 \
                    }                                                                       \
                }                                                                           \
            }                                                                               \
            sx = se;                                                                        \
        }                                                                                   \
    } while(0)

void gfx_sprite(const sprite_t *s, int x, int y, int flags, const px_t *pal, int mode, px_t flash)
{
    if(!s) return;
    if(!pal) pal = s->pal;

    int x0 = x - ((flags & SPR_FLIPX) ? (s->w - 1 - s->ox) : s->ox);
    int y0 = y - ((flags & SPR_FLIPY) ? (s->h - 1 - s->oy) : s->oy);
    int cx0 = x0, cy0 = y0, cx1 = x0 + s->w, cy1 = y0 + s->h;
    if(cx0 < g_clip.x0) cx0 = g_clip.x0;
    if(cy0 < g_clip.y0) cy0 = g_clip.y0;
    if(cx1 > g_clip.x1) cx1 = g_clip.x1;
    if(cy1 > g_clip.y1) cy1 = g_clip.y1;
    if(cx0 >= cx1 || cy0 >= cy1) return;

    switch(mode) {
        case DM_FLASH:
            SPRITE_LOOP(*d = flash);
            break;
        case DM_ADD:
            SPRITE_LOOP(*d = px_add(*d, pal[v]));
            break;
        case DM_HALF:
            SPRITE_LOOP(*d = px_half(*d, pal[v]));
            break;
        case DM_SHADOW:
            SPRITE_LOOP(*d = px_scale(*d, 12));
            break;
        default:
            SPRITE_LOOP(*d = pal[v]);
            break;
    }
}

void gfx_sprite_rot(const sprite_t *s, int x, int y, int angle, int scale, const px_t *pal, int mode, px_t flash)
{
    if(!s || scale <= 0) return;
    if(!pal) pal = s->pal;

    // inverse mapping: for each destination pixel find the source texel
    int ca = icos256(angle), sa = isin256(angle);          // Q14
    // radius of the bounding circle, in destination pixels
    int hw = s->w > s->h ? s->w : s->h;
    int rad = ((hw * scale) >> 8) + 2;

    int dx0 = x - rad, dx1 = x + rad + 1, dy0 = y - rad, dy1 = y + rad + 1;
    if(dx0 < g_clip.x0) dx0 = g_clip.x0;
    if(dy0 < g_clip.y0) dy0 = g_clip.y0;
    if(dx1 > g_clip.x1) dx1 = g_clip.x1;
    if(dy1 > g_clip.y1) dy1 = g_clip.y1;
    if(dx0 >= dx1 || dy0 >= dy1) return;

    // step per destination pixel, in source texels, Q16
    int64_t inv = ((int64_t)1 << 24) / scale;       // Q16 of 256/scale
    int32_t ux = (int32_t)(((int64_t)ca * inv) >> 14);
    int32_t uy = (int32_t)(((int64_t)-sa * inv) >> 14);
    int32_t vx = (int32_t)(((int64_t)sa * inv) >> 14);
    int32_t vy = (int32_t)(((int64_t)ca * inv) >> 14);

    int32_t ox = (int32_t)s->ox << 16, oy = (int32_t)s->oy << 16;
    int32_t half = 1 << 15;

    for(int py = dy0; py < dy1; py++) {
        int ry = py - y;
        int rx = dx0 - x;
        int32_t u = ox + half + rx * ux + ry * vx;
        int32_t v = oy + half + rx * uy + ry * vy;
        for(int px = dx0; px < dx1; px++, u += ux, v += uy) {
            int tu = u >> 16, tv = v >> 16;
            if((unsigned)tu >= s->w || (unsigned)tv >= s->h) continue;
            uint8_t idx = s->pix[tv * s->w + tu];
            if(!idx) continue;
            px_t *d = &g_fb[FBI(px, py)];
            switch(mode) {
                case DM_FLASH: *d = flash; break;
                case DM_ADD:   *d = px_add(*d, pal[idx]); break;
                case DM_HALF:  *d = px_half(*d, pal[idx]); break;
                default:       *d = pal[idx]; break;
            }
        }
    }
}
