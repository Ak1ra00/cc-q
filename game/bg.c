// QUASAR - parallax backgrounds for each stage and the title screen.
#include "game.h"
#include <string.h>

#define NSTARS 110

typedef struct {
    float x, y;
    uint8_t layer;      // 0 far, 1 mid, 2 near
    uint8_t tw;         // twinkle phase
} star_t;

static star_t s_stars[NSTARS];
static int s_stage;
static float s_speed = 1.0f;
static float s_scroll;              // total scrolled pixels (world x of the left edge)
static int s_t;
static int s_lightning;
static int s_bolt_x;

// shared scratch for textures: stage 5 uses a polar map, others a noise tile
#define TEX_N 128
static union {
    uint8_t tex[TEX_N][TEX_N];
    struct {
        uint8_t ang[120][160];
        uint8_t rad[120][160];
    } polar;
} s_buf;

static px_t s_lut[16][32];          // [row band][intensity]

float bg_speed(void) { return s_speed; }
void bg_set_speed(float s) { s_speed = s; }

static uint8_t hash8(int x, int y, int seed)
{
    // unsigned on purpose: signed overflow here would be undefined behaviour
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (uint8_t)(h >> 24);
}

// tileable value noise with the given cell size (must divide TEX_N)
static int tnoise(int x, int y, int cell, int seed)
{
    int n = TEX_N / cell;
    int xi = x / cell, yi = y / cell;
    int fx = ((x % cell) * 256) / cell, fy = ((y % cell) * 256) / cell;
    // smoothstep
    fx = (fx * fx * (768 - 2 * fx)) >> 16;
    fy = (fy * fy * (768 - 2 * fy)) >> 16;
    int x1 = (xi + 1) % n, y1 = (yi + 1) % n;
    int a = hash8(xi, yi, seed), b = hash8(x1, yi, seed);
    int c = hash8(xi, y1, seed), d = hash8(x1, y1, seed);
    int top = a + (((b - a) * fx) >> 8);
    int bot = c + (((d - c) * fx) >> 8);
    return top + (((bot - top) * fy) >> 8);
}

static void make_nebula_tex(int seed, int sharp)
{
    for(int y = 0; y < TEX_N; y++) {
        for(int x = 0; x < TEX_N; x++) {
            int v = tnoise(x, y, 32, seed) * 8 + tnoise(x, y, 16, seed + 1) * 4 + tnoise(x, y, 8, seed + 2) * 2 + tnoise(x, y, 4, seed + 3);
            v /= 15;                        // 0..255
            // ridge-ish contrast
            v = v - 90;
            if(v < 0) v = 0;
            v = (v * sharp) >> 4;
            if(v > 255) v = 255;
            s_buf.tex[y][x] = (uint8_t)(v >> 3);        // 0..31
        }
    }
}

static void make_polar(void)
{
    // half-resolution polar coordinates around the quasar centre (160, 120 at full res)
    for(int y = 0; y < 120; y++) {
        for(int x = 0; x < 160; x++) {
            float dx = (float)(x - 80) + 0.5f, dy = ((float)(y - 60) + 0.5f) * 1.35f;
            float a = fatan2_t(dy, dx);
            float r = fsqrt(dx * dx + dy * dy);
            s_buf.polar.ang[y][x] = (uint8_t)((int)(a * 256.0f) & 255);
            int rr = (int)(r * 2.2f);
            s_buf.polar.rad[y][x] = (uint8_t)(rr > 255 ? 255 : rr);
        }
    }
}

// build a 16 band x 32 intensity colour table: gradient from top to bottom, plus tint
static void make_lut(px_t top, px_t bot, px_t tint_lo, px_t tint_hi)
{
    for(int b = 0; b < 16; b++) {
        uint16_t base = c565_mix(SWAP16(bot), SWAP16(top), (b * 32) / 15);
        for(int i = 0; i < 32; i++) {
            uint16_t tint = c565_mix(SWAP16(tint_hi), SWAP16(tint_lo), i);
            uint16_t add = c565_scale(tint, i);
            s_lut[b][i] = SWAP16(c565_add(base, add));
        }
    }
}

void bg_init(int stage)
{
    s_stage = stage;
    s_scroll = 0;
    s_t = 0;
    s_speed = 1.0f;
    s_lightning = 0;
    for(int i = 0; i < NSTARS; i++) {
        s_stars[i].x = (float)rndfx(SCR_W);
        s_stars[i].y = (float)rndfx(SCR_H);
        s_stars[i].layer = (uint8_t)(i < 60 ? 0 : (i < 95 ? 1 : 2));
        s_stars[i].tw = (uint8_t)rndfx(256);
    }
    switch(stage) {
        case 1:
            make_nebula_tex(11, 26);
            make_lut(COL(4, 6, 22), COL(10, 20, 56), COL(20, 50, 120), COL(120, 200, 255));
            break;
        case 2:
            make_nebula_tex(23, 34);
            make_lut(COL(18, 2, 16), COL(34, 6, 30), COL(110, 10, 60), COL(255, 120, 200));
            break;
        case 3:
            make_nebula_tex(5, 14);
            make_lut(COL(6, 6, 12), COL(14, 12, 24), COL(30, 26, 50), COL(90, 80, 130));
            break;
        case 4:
            make_nebula_tex(41, 30);
            make_lut(COL(26, 6, 4), COL(60, 16, 6), COL(120, 40, 10), COL(255, 190, 80));
            break;
        default:
            make_polar();
            break;
    }
}

bool bg_terrain(int x, int *top, int *bot)
{
    if(s_stage != 3 || g_stage.boss_phase) return false;
    // world position of this column
    int wx = (int)s_scroll + x;
    if(wx < 520) return false;
    int seg = (wx - 520) / 640;
    int local = (wx - 520) % 640;
    // ramps in at the start of each segment so walls never pop in
    int ramp = local < 60 ? local : (local > 580 ? 640 - local : 60);
    int k = seg % 4;
    int t = 0, b = 0;
    switch(k) {
        case 0:     // gentle corridor
            t = 30 + ((isin256(wx >> 1) * 12) >> 14);
            b = 214 + ((isin256((wx >> 1) + 90) * 12) >> 14);
            break;
        case 1:     // stepped blocks
            t = 26 + (((wx >> 5) & 3) == 1 ? 34 : 0);
            b = 216 - (((wx >> 5) & 3) == 3 ? 36 : 0);
            break;
        case 2:     // narrow pass with a bend
            t = 40 + ((isin256(wx) * 24) >> 14);
            b = t + 150;
            break;
        default:    // open with pillars
            t = 22 + ((((wx >> 4) % 12) == 0) ? 50 : 0);
            b = 222 - ((((wx >> 4) % 12) == 6) ? 50 : 0);
            break;
    }
    // scale depth by ramp
    t = (t * ramp) / 60;
    b = SCR_H - ((SCR_H - b) * ramp) / 60;
    if(t < 1 && b > SCR_H - 2) return false;
    *top = t;
    *bot = b;
    return true;
}

void bg_update(void)
{
    s_t++;
    s_scroll += s_speed;
    static const float lay_speed[3] = { 0.18f, 0.5f, 2.2f };
    for(int i = 0; i < NSTARS; i++) {
        star_t *st = &s_stars[i];
        float sp = lay_speed[st->layer] * s_speed;
        if(s_stage == 5 || s_stage == 0) sp *= 0.7f;
        st->x -= sp;
        if(st->x < -4) {
            st->x += SCR_W + 8;
            st->y = (float)rndfx(SCR_H);
        }
    }
    if(s_stage == 2) {
        if(s_lightning > 0) s_lightning--;
        else if(rndfx(240) == 0) {
            s_lightning = 8;
            s_bolt_x = 60 + rndfx(200);
            fx_flash(COL(90, 40, 90), 5);
        }
    }
}

static void draw_texture_layer(int scroll_x, int scroll_y, int wobble)
{
    // full screen: lut[band][tex] with scrolling and optional heat wobble
    for(int s = 0; s < NUM_STRIPS; s++) {
        px_t *col0 = g_fb + s * STRIP_PIX;
        int xs = s * STRIP_W;
        for(int y = 0; y < SCR_H; y++) {
            const px_t *lut = s_lut[y >> 4];
            int ty = (y + scroll_y) & (TEX_N - 1);
            int off = scroll_x;
            if(wobble) off += (isin256(y * 4 + s_t * 3) * wobble) >> 14;
            const uint8_t *row = s_buf.tex[ty];
            px_t *d = col0 + (y << STRIP_SHIFT);
            int tx = (xs + off) & (TEX_N - 1);
            for(int i = 0; i < STRIP_W; i++) {
                d[i] = lut[row[tx]];
                tx = (tx + 1) & (TEX_N - 1);
            }
        }
    }
}

static void draw_stars(int bright)
{
    static const px_t far_c[4] = { COL(90, 100, 150), COL(120, 120, 170), COL(150, 140, 200), COL(200, 200, 255) };
    for(int i = 0; i < NSTARS; i++) {
        star_t *st = &s_stars[i];
        int x = (int)st->x, y = (int)st->y;
        switch(st->layer) {
            case 0: {
                int k = ((st->tw + s_t * 2) >> 5) & 3;
                gfx_padd(x, y, px_scale(far_c[k], bright));
                break;
            }
            case 1:
                gfx_padd(x, y, px_scale(COL(200, 210, 255), bright));
                if((st->tw & 7) == 0) {
                    gfx_padd(x - 1, y, px_scale(COL(90, 90, 140), bright));
                    gfx_padd(x + 1, y, px_scale(COL(90, 90, 140), bright));
                    gfx_padd(x, y - 1, px_scale(COL(90, 90, 140), bright));
                    gfx_padd(x, y + 1, px_scale(COL(90, 90, 140), bright));
                }
                break;
            default: {
                int len = 3 + (int)(s_speed * 4.0f);
                gfx_hline(x, x + len, y, px_scale(COL(140, 170, 255), bright * 3 / 4));
                gfx_padd(x, y, C_WHITE);
                break;
            }
        }
    }
}

static void draw_quasar_spiral(bool title)
{
    // swirling accretion disk: polar lookup at half resolution, doubled
    static const px_t pal[] = {
        COL(4, 2, 12), COL(10, 4, 26), COL(20, 8, 44), COL(34, 12, 70), COL(54, 18, 100), COL(80, 26, 130),
        COL(110, 40, 160), COL(150, 60, 190), COL(190, 90, 220), COL(230, 140, 240), COL(255, 200, 250), COL(255, 245, 255),
    };
    int rot = s_t * (title ? 1 : 2);
    for(int s = 0; s < NUM_STRIPS; s++) {
        px_t *col0 = g_fb + s * STRIP_PIX;
        for(int hy = 0; hy < 120; hy++) {
            const uint8_t *ar = s_buf.polar.ang[hy];
            const uint8_t *rr = s_buf.polar.rad[hy];
            px_t *d0 = col0 + ((hy * 2) << STRIP_SHIFT);
            px_t *d1 = d0 + STRIP_W;
            int hx0 = (s * STRIP_W) >> 1;
            for(int i = 0; i < STRIP_W / 2; i++) {
                int a = ar[hx0 + i];
                int r = rr[hx0 + i];
                // two spiral arms, twisting with radius
                int arm = (a * 2 + (r * 3 >> 1) - rot) & 255;
                int v = (isin256(arm) >> 10) + 16;         // 0..32
                int fall = r < 24 ? 32 : (r > 230 ? 0 : 32 - ((r - 24) * 32) / 206);
                int k = (v * fall) >> 5;
                if(r < 20) k = 11 - (r >> 2);               // bright core
                k = k * 11 / 32;
                if(k < 0) k = 0;
                if(k > 11) k = 11;
                // dither between neighbouring palette entries
                px_t c = pal[k];
                d0[i * 2] = c;
                d0[i * 2 + 1] = ((i + hy) & 1) && k > 0 ? pal[k - 1] : c;
                d1[i * 2] = ((i + hy) & 1) && k > 0 ? pal[k - 1] : c;
                d1[i * 2 + 1] = c;
            }
        }
    }
    // the core glows
    gfx_glow(160, 120, 40, COL(120, 70, 160));
    gfx_glow(160, 120, 14, COL(255, 230, 255));
    // relativistic jets
    int jl = 60 + ((isin256(s_t * 3) * 8) >> 14);
    for(int k = 0; k < 3; k++) {
        gfx_fill_mode(158 + k, 120 - jl - 40, 1, jl + 30, COL(40, 30, 90), DM_ADD);
        gfx_fill_mode(158 + k, 130, 1, jl + 30, COL(40, 30, 90), DM_ADD);
    }
}

static void draw_sun(void)
{
    // big star on the right edge with a boiling surface
    int cx = 400, cy = 120, r = 150;
    for(int y = 0; y < SCR_H; y++) {
        int dy = y - cy;
        int w2 = r * r - dy * dy;
        if(w2 <= 0) continue;
        int hw = isqrt((uint32_t)w2);
        int x0 = cx - hw;
        if(x0 >= SCR_W) continue;
        if(x0 < 0) x0 = 0;
        int ty = (y + s_t / 2) & (TEX_N - 1);
        for(int x = x0; x < SCR_W; x++) {
            int v = s_buf.tex[ty][(x * 2 + s_t) & (TEX_N - 1)];
            int edge = (x - (cx - hw));
            int k = v + (edge < 24 ? edge - 24 : 0) / 2 + 8;
            if(k < 0) k = 0;
            if(k > 31) k = 31;
            static const px_t sun[8] = { COL(160, 40, 10), COL(210, 70, 16), COL(240, 110, 20), COL(255, 150, 40),
                                         COL(255, 190, 70), COL(255, 220, 120), COL(255, 240, 180), COL(255, 255, 230) };
            g_fb[FBI(x, y)] = sun[k >> 2];
        }
    }
    // corona
    for(int k = 0; k < 3; k++) gfx_ring(cx, cy, r + k * 6, r + k * 6 + 5, px_scale(COL(255, 120, 30), 10 - k * 3), DM_ADD);
}

static void draw_station_backdrop(void)
{
    // far megastructure: rows of lit windows and girders drifting slowly
    int off = (int)(s_scroll * 0.35f);
    for(int x = 0; x < SCR_W; x += 2) {
        int wx = x + off;
        if(((wx >> 6) & 3) == 0) {
            gfx_fill(x, 40, 2, 160, COL(20, 18, 34));
            if(((wx >> 1) & 7) == 0) {
                for(int y = 50; y < 196; y += 12) {
                    if(hash8(wx >> 4, y, 3) > 150) gfx_fill(x, y, 2, 2, COL(255, 180, 90));
                }
            }
        }
    }
    int off2 = (int)(s_scroll * 0.6f);
    for(int y = 60; y < 190; y += 64) {
        gfx_fill(0, y, SCR_W, 3, COL(26, 24, 44));
        for(int x = -((off2) & 31); x < SCR_W; x += 32) {
            gfx_line(x, y + 3, x + 16, y + 20, COL(24, 22, 40), DM_NORMAL);
        }
    }
}

static void draw_terrain(void)
{
    for(int x = 0; x < SCR_W; x++) {
        int top, bot;
        if(!bg_terrain(x, &top, &bot)) continue;
        int wx = (int)s_scroll + x;
        bool seam = (wx & 31) == 0;
        // ceiling
        for(int y = 0; y < top; y++) {
            int d = top - y;
            px_t c;
            if(d <= 2) c = COL(210, 190, 160);
            else if(d <= 4) c = COL(140, 110, 90);
            else c = ((y >> 3) + (wx >> 4)) & 1 ? COL(56, 50, 70) : COL(48, 42, 62);
            if(seam && d > 4) c = COL(24, 20, 34);
            if(d > 5 && ((wx & 31) == 16) && ((y & 15) == 8)) c = COL(255, 150, 60);
            g_fb[FBI(x, y)] = c;
        }
        // floor
        for(int y = bot; y < SCR_H; y++) {
            int d = y - bot;
            px_t c;
            if(d <= 1) c = COL(230, 210, 180);
            else if(d <= 4) c = COL(120, 96, 80);
            else c = ((y >> 3) + (wx >> 4)) & 1 ? COL(48, 42, 62) : COL(40, 36, 54);
            if(seam && d > 4) c = COL(20, 18, 30);
            if(d > 5 && ((wx & 31) == 16) && ((y & 15) == 8)) c = COL(90, 220, 255);
            g_fb[FBI(x, y)] = c;
        }
    }
}

void bg_draw(void)
{
    switch(s_stage) {
        case 0:
            draw_quasar_spiral(true);
            draw_stars(24);
            break;
        case 1:
            draw_texture_layer((int)(s_scroll * 0.3f), 0, 0);
            draw_stars(32);
            {
                // the ice giant drifts by
                int px = 250 - (int)(s_scroll * 0.06f);
                if(px > -80) gfx_sprite(&SPR_PLANET_ICE, px, 70, 0, NULL, DM_NORMAL, 0);
            }
            break;
        case 2:
            draw_texture_layer((int)(s_scroll * 0.55f), s_t / 8, 0);
            draw_stars(26);
            if(s_lightning > 0) {
                int x = s_bolt_x, y = 0;
                px_t c = px_scale(COL(255, 200, 255), s_lightning * 4);
                while(y < SCR_H) {
                    int nx = x + rndfx(17) - 8, ny = y + 8 + rndfx(10);
                    gfx_line(x, y, nx, ny, c, DM_ADD);
                    gfx_line(x + 1, y, nx + 1, ny, c, DM_ADD);
                    x = nx;
                    y = ny;
                }
            }
            break;
        case 3:
            draw_texture_layer((int)(s_scroll * 0.2f), 0, 0);
            draw_station_backdrop();
            draw_stars(18);
            draw_terrain();
            break;
        case 4:
            draw_texture_layer((int)(s_scroll * 0.4f), s_t / 4, 3);
            draw_stars(20);
            draw_sun();
            break;
        default:
            draw_quasar_spiral(false);
            draw_stars(30);
            break;
    }
}

void bg_draw_front(void)
{
    // foreground dust for depth
    if(s_stage == 1) {
        for(int i = 0; i < 6; i++) {
            int x = SCR_W - ((s_t * 5 + i * 97) % (SCR_W + 40));
            int y = (i * 53 + (s_t >> 3)) % SCR_H;
            gfx_line(x, y, x + 10, y, COL(40, 60, 90), DM_ADD);
        }
    } else if(s_stage == 4) {
        for(int i = 0; i < 8; i++) {
            int x = SCR_W - ((s_t * 3 + i * 83) % (SCR_W + 20));
            int y = (i * 37 + s_t) % SCR_H;
            gfx_glow(x, y, 2, COL(255, 140, 40));
        }
    }
}
