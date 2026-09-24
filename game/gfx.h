// QUASAR - graphics core.
//
// The frame buffer is 320x240 RGB565, stored byte-swapped so it can be streamed
// to the ST7789 as-is, and laid out as ten 32-pixel-wide vertical strips. Each strip
// is contiguous in memory, which lets the LCD driver send the frame strip by strip
// in the same direction the panel refreshes (no tearing) with one DMA per strip.
//
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SCR_W       320
#define SCR_H       240
#define STRIP_W     32
#define STRIP_SHIFT 5
#define NUM_STRIPS  (SCR_W / STRIP_W)
#define STRIP_PIX   (STRIP_W * SCR_H)
#define FB_PIX      (SCR_W * SCR_H)

typedef uint16_t px_t;

// index of pixel (x,y) inside a frame buffer
#define FBI(x, y)   ((((x) >> STRIP_SHIFT) * STRIP_PIX) + ((y) << STRIP_SHIFT) + ((x) & (STRIP_W - 1)))

// colour construction: 8-bit channels in, swapped RGB565 out
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xff) >> 3)))
#define SWAP16(v)       ((uint16_t)((((v) >> 8) & 0xff) | (((v) & 0xff) << 8)))
#define COL(r, g, b)    SWAP16(RGB565(r, g, b))
#define HEXCOL(h)       COL(((h) >> 16) & 0xff, ((h) >> 8) & 0xff, (h) & 0xff)

static inline uint16_t col_unswap(px_t c) { return SWAP16(c); }

// sprite: 8-bit palette indices, 0 = transparent
typedef struct {
    uint16_t w, h;
    int16_t ox, oy;             // pivot (drawn position is the pivot)
    const uint8_t *pix;
    const px_t *pal;            // default palette (swapped colours), entry 0 unused
} sprite_t;

// blend / draw modes
enum {
    DM_NORMAL = 0,
    DM_FLASH,                   // every opaque pixel in one colour
    DM_ADD,                     // additive
    DM_HALF,                    // 50% mix
    DM_SHADOW,                  // darken what is under
};

typedef struct {
    int16_t x0, y0, x1, y1;     // clip rectangle, x1/y1 exclusive
} clip_t;

extern px_t *g_fb;
extern clip_t g_clip;

void gfx_set_target(px_t *fb);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_noclip(void);

void gfx_clear(px_t c);
void gfx_fill(int x, int y, int w, int h, px_t c);
void gfx_fill_mode(int x, int y, int w, int h, px_t c, int mode);
void gfx_fill_alpha(int x, int y, int w, int h, px_t c, int alpha32);
void gfx_hline(int x0, int x1, int y, px_t c);
void gfx_vline(int x, int y0, int y1, px_t c);
void gfx_rect(int x, int y, int w, int h, px_t c);
void gfx_line(int x0, int y0, int x1, int y1, px_t c, int mode);
void gfx_pset(int x, int y, px_t c);
void gfx_padd(int x, int y, px_t c);
void gfx_circle(int cx, int cy, int r, px_t c, int mode);
void gfx_disc(int cx, int cy, int r, px_t c, int mode);
void gfx_ring(int cx, int cy, int r0, int r1, px_t c, int mode);
void gfx_vgrad(int x, int y, int w, int h, px_t top, px_t bot);
void gfx_hgrad(int x, int y, int w, int h, px_t left, px_t right);

// glow: soft additive blob with given radius and colour at full intensity
void gfx_glow(int cx, int cy, int r, px_t c);

void gfx_sprite(const sprite_t *s, int x, int y, int flags, const px_t *pal, int mode, px_t flash);
// rotated + scaled sprite; angle in 1/256 turns, scale in 1/256
void gfx_sprite_rot(const sprite_t *s, int x, int y, int angle, int scale, const px_t *pal, int mode, px_t flash);

#define SPR_FLIPX   0x01
#define SPR_FLIPY   0x02

// colour math, native (unswapped) RGB565
static inline uint16_t c565_add(uint16_t a, uint16_t b)
{
    uint32_t s = (uint32_t)(a & 0xf7de) + (uint32_t)(b & 0xf7de);
    uint32_t carry = s & 0x10820;
    s |= carry - (carry >> 5);
    return (uint16_t)s;
}

static inline uint16_t c565_mix(uint16_t fg, uint16_t bg, int alpha32)
{
    uint32_t f = ((uint32_t)fg | ((uint32_t)fg << 16)) & 0x07e0f81f;
    uint32_t b = ((uint32_t)bg | ((uint32_t)bg << 16)) & 0x07e0f81f;
    // mask only after adding bg back: per-channel borrows from (f - b) must cancel first
    b = ((((f - b) * (uint32_t)alpha32) >> 5) + b) & 0x07e0f81f;
    return (uint16_t)(b | (b >> 16));
}

static inline uint16_t c565_scale(uint16_t c, int k32)
{
    uint32_t f = ((uint32_t)c | ((uint32_t)c << 16)) & 0x07e0f81f;
    f = ((f * (uint32_t)k32) >> 5) & 0x07e0f81f;
    return (uint16_t)(f | (f >> 16));
}

// same, for swapped frame-buffer pixels
static inline px_t px_add(px_t a, px_t b) { return SWAP16(c565_add(SWAP16(a), SWAP16(b))); }

// swap the bytes of both halves of a word: one instruction on the Cortex-M4
static inline uint32_t rev16x2(uint32_t v)
{
#if defined(__arm__) && defined(__GNUC__)
    uint32_t r;
    __asm__("rev16 %0, %1" : "=r"(r) : "r"(v));
    return r;
#else
    return ((v >> 8) & 0x00ff00ffu) | ((v << 8) & 0xff00ff00u);
#endif
}

// px_add on two frame-buffer pixels in one 32-bit word, bit for bit the same result.
// cm is the native addend in both halves, masked with 0xf7def7de.
static inline uint32_t px_add2(uint32_t pp, uint32_t cm)
{
    uint32_t a = rev16x2(pp) & 0xf7def7deu;
    uint32_t s = a + cm;
    uint32_t top = ((a & cm) | ((a | cm) & ~s)) >> 31;     // the upper pixel's red carry leaves the word
    uint32_t carry = s & 0x08210820u;
    // bit 16 holds the lower pixel's red carry, which c565_add drops: it is not the upper blue's bit
    s = (s & ~0x00010000u) | (carry - (carry >> 5)) | ((0u - top) & 0xf8000000u);
    return rev16x2(s);
}
static inline px_t px_mix(px_t fg, px_t bg, int a32) { return SWAP16(c565_mix(SWAP16(fg), SWAP16(bg), a32)); }
static inline px_t px_scale(px_t c, int k32) { return SWAP16(c565_scale(SWAP16(c), k32)); }
static inline px_t px_half(px_t a, px_t b)
{
    uint16_t x = SWAP16(a), y = SWAP16(b);
    return SWAP16((uint16_t)(((x & 0xf7de) >> 1) + ((y & 0xf7de) >> 1)));
}

// convert swapped pixel to 8-bit channels (for desktop output)
static inline void px_to_rgb(px_t p, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t c = SWAP16(p);
    *r = (uint8_t)(((c >> 11) & 0x1f) * 255 / 31);
    *g = (uint8_t)(((c >> 5) & 0x3f) * 255 / 63);
    *b = (uint8_t)((c & 0x1f) * 255 / 31);
}
