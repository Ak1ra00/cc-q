// PAC-MAN - the screens: menus, the maze in neon, and everything that glows in it.
//
// Controls while playing: the arrows or W A S D steer (a tap is kept until the
// next turning comes); in NEON, SPACE, ENTER or SHIFT dash. CANCEL / TAB / P /
// a tap of POWER pause. Holding POWER saves the game.
//
// The walls are light, not paint: tools/gen_pacman.py rendered them once as thin
// glowing tubes, cut into 8x8 tiles of 16 brightness levels. Each frame the tiles
// are added onto the backdrop through a colour table per brightness variant, so
// the maze can breathe, flare where an energizer's shockwave passes, flash on a
// clear or ice over, for the price of a table lookup a pixel.
#include "arcade.h"
#include <string.h>

enum { P_MENU, P_SCORES, P_HOWTO, P_PLAY, P_PAUSE, P_CONFIRM, P_OVER, P_NAME };
enum { CF_QUIT, CF_RESTART, CF_NEW };

#define MX      8               // the maze on screen: 38 x 27 tiles of 8 pixels
#define MY      22
#define MW      (PM_W * 8)
#define MH      (PM_H * 8)
#define NV      4               // wall brightness variants: dim, normal, bright, flare

typedef uint32_t __attribute__((may_alias)) u32a;

static const char *const MODE_NAMES[PM_MODES] = { "CLASSIC", "NEON" };
static const px_t GHOST_COL[GH_N] = { COL(255, 40, 56), COL(255, 128, 214), COL(40, 226, 255), COL(255, 164, 46) };
#define PAC_YELLOW  COL(255, 226, 20)
#define PEACH       COL(255, 190, 160)

// what one game looks like as it plays, beyond the rules: the menu's demo keeps its own
typedef struct {
    int32_t px, py;             // Pac-Man last frame, so he chomps only as he moves
    int chomp;
    int ripple_t, ripple_x, ripple_y;   // an energizer's shockwave running through the walls
    int16_t trail[6][2];        // NEON dash afterimages
    int trail_n;
    int theme_from, theme_to, theme_k;
    uint32_t shown;             // the score, rolling up
    int t;
} view_t;

static int s_st, s_st_t;
static pgame_t s_g;             // the game being played
static view_t s_v;
static pgame_t s_demo;          // the attract demo: the menu's backdrop and the home screen's card
static pai_t s_demo_ai;
static view_t s_dv;
static uint32_t s_demo_seed = 7;
static int s_mode;
static int s_menu_sel, s_tab, s_pause_sel;
static int s_confirm, s_confirm_sel, s_confirm_back;
static int s_rank = -1;
static bool s_quit;
static int s_hold;              // frames of calm after un-pausing
static int s_over_t;
static int s_name_len;
static char s_name[NAME_LEN + 1];
static int s_new_rec_mode = -1, s_new_rec_rank = -1;
static int s_dim = 32;          // the scene's brightness: 32, or less as the menus' backdrop
static int s_banner_t;
static char s_banner[2][36];
static px_t s_banner_col;

// ------------------------------------------------------------------ colour themes

typedef struct { px_t wall, bg0, bg1, dot; } ptheme_t;

static const ptheme_t THEMES[8] = {
    { COL(36, 100, 255), COL(0, 0, 6), COL(4, 6, 28), COL(255, 196, 170) },       // arcade blue
    { COL(235, 50, 200), COL(4, 0, 6), COL(28, 3, 28), COL(255, 222, 170) },       // magenta
    { COL(30, 220, 110), COL(0, 4, 3), COL(2, 26, 16), COL(255, 232, 190) },       // green
    { COL(40, 210, 255), COL(0, 3, 6), COL(2, 20, 30), COL(255, 206, 196) },       // cyan
    { COL(255, 130, 20), COL(6, 2, 0), COL(32, 12, 2), COL(210, 232, 255) },       // orange
    { COL(150, 80, 255), COL(3, 0, 8), COL(18, 6, 38), COL(255, 230, 170) },       // violet
    { COL(255, 44, 70), COL(6, 0, 2), COL(32, 3, 8), COL(220, 240, 255) },         // red
    { COL(255, 206, 40), COL(5, 4, 0), COL(28, 22, 2), COL(200, 220, 255) },       // gold
};

static int level_theme(int level)
{
    if(level <= 2) return 0;
    if(level <= 5) return 1;
    if(level <= 9) return 2;
    return 3 + ((level - 10) / 4) % 5;
}

static px_t view_col(const view_t *v, int what)
{
    const ptheme_t *a = &THEMES[v->theme_from], *b = &THEMES[v->theme_to];
    px_t ca = what == 0 ? a->wall : (what == 1 ? a->bg0 : (what == 2 ? a->bg1 : a->dot));
    px_t cb = what == 0 ? b->wall : (what == 1 ? b->bg0 : (what == 2 ? b->bg1 : b->dot));
    return px_mix(cb, ca, v->theme_k);
}

static void view_reset(view_t *v, const pgame_t *g)
{
    memset(v, 0, sizeof(*v));
    v->px = g->pac.x;
    v->py = g->pac.y;
    v->theme_from = v->theme_to = level_theme(g->level);
    v->theme_k = 32;
    v->shown = g->score;
}

static void view_tick(view_t *v, const pgame_t *g)
{
    v->t++;
    int32_t dx = g->pac.x - v->px, dy = g->pac.y - v->py;
    if(iabs(dx) < PT && iabs(dy) < PT) v->chomp += iabs(dx) + iabs(dy);
    v->px = g->pac.x;
    v->py = g->pac.y;
    if(v->ripple_t > 0) v->ripple_t--;
    int th = level_theme(g->level);
    if(th != v->theme_to) {
        v->theme_from = v->theme_to;
        v->theme_to = th;
        v->theme_k = 0;
    }
    if(v->theme_k < 32) v->theme_k++;
    // the dash leaves afterimages
    if(g->dash_t > 0 && g->phase == PP_PLAY) {
        memmove(&v->trail[1], &v->trail[0], sizeof(v->trail) - sizeof(v->trail[0]));
        v->trail[0][0] = (int16_t)(g->pac.x >> 5);
        v->trail[0][1] = (int16_t)(g->pac.y >> 5);
        if(v->trail_n < 6) v->trail_n++;
    } else if(v->trail_n > 0) {
        v->trail_n--;
    }
    if(v->shown < g->score) v->shown += (g->score - v->shown + 5) / 6;
    else v->shown = g->score;
}

// ------------------------------------------------------------------ the walls

static uint32_t s_lut2[NV][256];        // two pixels' colours for every byte of a tile
static px_t s_lut_col;
static int s_lut_dim = -1;

static void wall_luts(px_t c)
{
    if(s_lut_dim == s_dim && c == s_lut_col) return;
    s_lut_dim = s_dim;
    s_lut_col = c;
    uint8_t r, g, b;
    px_to_rgb(c, &r, &g, &b);
    static const float GAIN[NV] = { 0.62f, 1.0f, 1.4f, 2.0f };
    for(int k = 0; k < NV; k++) {
        uint16_t c16[16];
        for(int v = 0; v < 16; v++) {
            float t = (float)v / 15.0f;
            float hot = t > 0.72f ? (t - 0.72f) * 3.6f : 0.0f;     // the tube's white-hot middle
            float gk = GAIN[k] * (float)s_dim * (1.0f / 32.0f);
            int R = (int)((r * t * 1.2f + (255 - r) * hot * 0.75f) * gk);
            int G = (int)((g * t * 1.2f + (255 - g) * hot * 0.75f) * gk);
            int B = (int)((b * t * 1.2f + (255 - b) * hot * 0.75f) * gk);
            c16[v] = RGB565(iclamp(R, 0, 255), iclamp(G, 0, 255), iclamp(B, 0, 255));
        }
        c16[0] = 0;
        // the high nibble is the left pixel, the lower half of the word
        for(int i = 0; i < 256; i++) s_lut2[k][i] = (((uint32_t)c16[i & 15] << 16) | c16[i >> 4]) & 0xf7def7deu;
    }
}

static void draw_walls(const pgame_t *g, const view_t *v, bool dim)
{
    const uint16_t (*map)[PM_W] = PAC_TILEMAP[g->maze];
    int ptx = g->pac.x >> 8, pty = g->pac.y >> 8;
    int base = dim ? 0 : 1;
    int rr = v->ripple_t > 0 ? (36 - v->ripple_t) * 11 : -99;      // the shockwave's radius
    for(int ty = 0; ty < PM_H; ty++) {
        for(int tx = 0; tx < PM_W; tx++) {
            int idx = map[ty][tx];
            if(!idx) continue;
            int k = base;
            if(((tx + ty * 2 - (v->t >> 1)) & 63) < 2) k++;         // a glint running across
            if(iabs(tx - ptx) <= 2 && iabs(ty - pty) <= 2) k++;     // Pac-Man's own light
            if(rr >= 0) {
                int dx = tx * 8 + 4 - v->ripple_x, dy = ty * 8 + 4 - v->ripple_y;
                int d = isqrt((uint32_t)(dx * dx + dy * dy));
                int e = iabs(d - rr);
                if(e < 7) k = 3;
                else if(e < 16 && k < 2) k = 2;
            }
            const uint32_t *lut = s_lut2[k > 3 ? 3 : k];
            const uint8_t *src = PAC_TILES[idx];
            u32a *d = (u32a *)&g_fb[FBI(MX + tx * 8, MY + ty * 8)];
            for(int r = 0; r < 8; r++, d += STRIP_W / 2, src += 4) {
                for(int q = 0; q < 4; q++) {
                    if(src[q]) d[q] = px_add2(d[q], lut[src[q]]);
                }
            }
        }
    }
}

// ------------------------------------------------------------------ shapes, drawn anti-aliased

static inline float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline void blend(int x, int y, px_t c, float a)
{
    int k = (int)(a * 32.0f + 0.5f);
    if(k <= 0) return;
    px_t *p = &g_fb[FBI(x, y)];
    *p = k >= 32 ? c : px_mix(c, *p, k);
}

static bool box_clip(float cx, float cy, float rx, float ry, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = (int)(cx - rx - 1.0f);
    *y0 = (int)(cy - ry - 1.0f);
    *x1 = (int)(cx + rx + 2.0f);
    *y1 = (int)(cy + ry + 2.0f);
    if(*x0 < g_clip.x0) *x0 = g_clip.x0;
    if(*y0 < g_clip.y0) *y0 = g_clip.y0;
    if(*x1 > g_clip.x1) *x1 = g_clip.x1;
    if(*y1 > g_clip.y1) *y1 = g_clip.y1;
    return *x0 < *x1 && *y0 < *y1;
}

static void aa_disc(float cx, float cy, float r, px_t c, float a)
{
    int x0, y0, x1, y1;
    if(!box_clip(cx, cy, r, r, &x0, &y0, &x1, &y1)) return;
    for(int y = y0; y < y1; y++) {
        float py = (float)y + 0.5f - cy;
        for(int x = x0; x < x1; x++) {
            float px = (float)x + 0.5f - cx;
            float d2 = px * px + py * py;
            if(d2 > (r + 1.0f) * (r + 1.0f)) continue;
            blend(x, y, c, clamp01(r + 0.5f - fsqrt(d2)) * a);
        }
    }
}

// Pac-Man: a glossy disc with a wedge for a mouth. mouth is its half-angle in turns
// (0.5 is all mouth: the end of the death).
static void draw_pac_at(float cx, float cy, float r, int dir, float mouth, px_t body, float a)
{
    static const int8_t FX[5] = { 0, -1, 0, 1, 1 }, FY[5] = { -1, 0, 1, 0, 0 };
    int x0, y0, x1, y1;
    if(!box_clip(cx, cy, r, r, &x0, &y0, &x1, &y1)) return;
    float fx = FX[dir], fy = FY[dir];
    float sa = fsin_t(mouth), ca = fcos_t(mouth);
    px_t hi = ar_lighten(body, 16), lo = px_scale(body, 19);
    float inv = 1.0f / (2.0f * r);
    for(int y = y0; y < y1; y++) {
        float py = (float)y + 0.5f - cy;
        for(int x = x0; x < x1; x++) {
            float px = (float)x + 0.5f - cx;
            float d2 = px * px + py * py;
            if(d2 > (r + 1.0f) * (r + 1.0f)) continue;
            float cov = clamp01(r + 0.5f - fsqrt(d2));
            if(mouth > 0.0f) {
                float u = px * fx + py * fy, w = px * fy - py * fx;
                if(w < 0.0f) w = -w;
                cov *= clamp01(0.5f - (u * sa - w * ca));
            }
            if(cov <= 0.0f) continue;
            // lit from the upper left, a touch of shadow low right
            float l = (px + py) * inv;
            px_t c = l < 0.0f ? px_mix(hi, body, (int)(-l * 30.0f)) : px_mix(lo, body, (int)(l * 22.0f));
            blend(x, y, c, cov * a);
        }
    }
}

enum { GL_NORMAL, GL_BLUE, GL_WHITE, GL_EYES, GL_ICE };

static float skirt(float gx, int feet)
{
    // the hem: three points down, shuffling between two positions
    float ph = (gx + 7.0f) * (3.0f / 14.0f) + (feet ? 0.5f : 0.0f);
    float f = ph - (float)(int)ph;
    float tri = f < 0.5f ? f * 2.0f : (1.0f - f) * 2.0f;
    return 4.6f + 2.4f * tri;
}

// a ghost 14 units across at scale s, centred; look is the way its eyes point
static void draw_ghost_at(float cx, float cy, float s, px_t body, int look, int feet, int mode, float a)
{
    int x0, y0, x1, y1;
    if(!box_clip(cx, cy - 0.5f * s, 7.5f * s, 8.0f * s, &x0, &y0, &x1, &y1)) return;
    float inv = 1.0f / s;
    int lx = look < PD_NONE ? PAC_DX[look] : 0, ly = look < PD_NONE ? PAC_DY[look] : 0;
    px_t top, bot, face = PEACH;
    if(mode == GL_BLUE) {
        top = COL(70, 90, 255);
        bot = COL(20, 30, 170);
    } else if(mode == GL_WHITE) {
        top = C_WHITE;
        bot = COL(190, 200, 230);
        face = COL(255, 40, 40);
    } else if(mode == GL_ICE) {
        top = COL(230, 250, 255);
        bot = px_mix(COL(120, 200, 255), body, 20);
    } else {
        top = ar_lighten(body, 9);
        bot = px_scale(body, 21);
    }
    for(int y = y0; y < y1; y++) {
        float gy = ((float)y + 0.5f - cy) * inv;
        px_t shade = px_mix(bot, top, iclamp((int)((6.0f - gy) * 2.3f), 0, 32));
        for(int x = x0; x < x1; x++) {
            float gx = ((float)x + 0.5f - cx) * inv;
            if(mode != GL_EYES) {
                float cov;
                if(gy < -1.0f) {
                    cov = (7.0f - fsqrt(gx * gx + (gy + 1.0f) * (gy + 1.0f))) * s + 0.5f;
                } else {
                    float ax = gx < 0.0f ? -gx : gx;
                    float c1 = (7.0f - ax) * s + 0.5f, c2 = (skirt(gx, feet) - gy) * s + 0.5f;
                    cov = c1 < c2 ? c1 : c2;
                }
                cov = clamp01(cov);
                if(cov <= 0.0f) continue;
                px_t c = shade;
                // a gleam on the dome
                float hx = gx + 3.0f, hy = gy + 4.6f;
                if(hx * hx + hy * hy < 2.2f) c = ar_lighten(c, 14);
                blend(x, y, c, cov * a);
            }
            if(mode == GL_BLUE || mode == GL_WHITE) {
                // the scared face: two dots and a wobbly mouth
                float ex = (gx < 0.0f ? -gx : gx) - 2.6f, ey = gy + 2.0f;
                float e = clamp01((1.05f - (ex < 0.0f ? -ex : ex)) * s) * clamp01((1.05f - (ey < 0.0f ? -ey : ey)) * s);
                float zig = gx * 1.35f + 20.0f;
                float fz = zig - (float)(int)zig;
                float my = 2.6f + (fz < 0.5f ? fz : 1.0f - fz) * 1.6f - 0.4f;
                float m = (gx > -5.2f && gx < 5.2f) ? clamp01((0.6f - (gy - my < 0.0f ? my - gy : gy - my)) * s * 1.5f) : 0.0f;
                float k = e > m ? e : m;
                if(k > 0.0f) blend(x, y, face, k * a);
                continue;
            }
            // eyes: whites looking the way it goes, blue pupils at the edge
            float eyy = gy - (-2.2f + ly * 1.0f);
            if(eyy < -3.2f || eyy > 3.2f) continue;
            for(int side = -1; side <= 1; side += 2) {
                float ex = gx - (side * 2.9f + lx * 1.0f), ey = eyy;
                if(ex < -2.6f || ex > 2.6f) continue;
                float q = fsqrt((ex * ex) * (1.0f / (2.1f * 2.1f)) + (ey * ey) * (1.0f / (2.6f * 2.6f)));
                float w = clamp01((1.0f - q) * 2.2f * s + 0.5f);
                if(w <= 0.0f) continue;
                blend(x, y, C_WHITE, w * a);
                float px = ex - lx * 1.1f, py = ey - ly * 1.2f;
                float pd = clamp01((1.3f - fsqrt(px * px + py * py)) * s + 0.5f);
                if(pd > 0.0f) blend(x, y, mode == GL_ICE ? COL(40, 150, 255) : COL(30, 40, 200), pd * a);
            }
        }
    }
}

// ------------------------------------------------------------------ fruit and power-ups, as pixel art

enum { ART_MAGNET = FR_COUNT, ART_FREEZE, ART_N };

static const char *const ART[ART_N][12] = {
    { "........G...", ".......G.G..", "......G..G..", ".....G...G..", "....G....G..", "..rr.....G..",
      ".rrrr...G...", "rWrrrr.rrr..", "rrrrrrrrrrr.", ".rrrrrWrrrr.", "..rr.rrrrrr.", "......rrrr.." },
    { ".....gG.....", "...GgggGg...", "..rrrggrrr..", ".rryrrrrWrr.", ".rrrrryrrrr.", ".ryrrrrrryr.",
      "..rrryrrrr..", "..rrrrrryr..", "...ryrrrr...", "....rrrr....", ".....rr.....", "............" },
    { ".....G......", "....Ggg.....", "...oooGg....", "..ooooooo...", ".ooWooooooo.", ".oWoooooooo.",
      ".oooooooooo.", ".oooooooooO.", "..ooooooOO..", "...ooooOO...", "....oooo....", "............" },
    { ".....b.gg...", "......bgg...", "..rrr.b.rr..", ".rrWrrrrrrr.", ".rWrrrrrrrr.", ".rrrrrrrrrr.",
      ".rrrrrrrrrr.", ".rrrrrrrrrR.", "..rrrrrrrR..", "..rrrrrrRR..", "...rr..rr...", "............" },
    { "......b.....", "....GmmmG...", "...mGmmGmm..", "..mmGmmGmmm.", ".mmGmmmGmmm.", ".mWGmmGmmmm.",
      ".mmGmmGmmGm.", ".mmmGmmGmGm.", "..mmGmmGmm..", "...mmGmGm...", ".....mm.....", "............" },
    { ".....rr.....", "....yrry....", "...yyyyyy...", "B..yyyyyy..B", "BB.yyyyyy.BB", "BBByyyyyyBBB",
      "BBByyrryyBBB", "BB..yyyy..BB", "B....yy....B", ".....yy.....", "............", "............" },
    { ".....yy.....", "....yyyy....", "...yWyyyy...", "...yWyyyy...", "..yWyyyyyy..", "..yyyyyyyy..",
      ".yyyyyyyyyy.", ".yyyyyyyyyy.", "yyyyyyyyyyyy", "YYYYYYYYYYYY", ".....cc.....", "............" },
    { "...cccccc...", "..cc.cc.cc..", "..cccccccc..", "...cccccc...", ".....ww.....", ".....ww.....",
      ".....wwkk...", ".....ww.....", ".....wwkk...", ".....ww.....", ".....wwk....", "............" },
    { "............", "...rrrBBB...", "..rrrrBBBB..", ".rrrr..BBBB.", ".rrr....BBB.", ".rrr....BBB.",
      ".rrr....BBB.", ".rrr....BBB.", ".www....www.", ".www....www.", "............", "............" },
    { "......c.....", "..c...w...c.", "...c..w..c..", "....c.w.c...", ".....cwc....", ".cwwwwWwwwwc",
      ".....cwc....", "....c.w.c...", "...c..w..c..", "..c...w...c.", "......c.....", "............" },
};

static px_t art_col(char ch)
{
    switch(ch) {
        case 'r': return COL(240, 24, 44);
        case 'R': return COL(150, 0, 24);
        case 'W': return C_WHITE;
        case 'w': return COL(215, 220, 235);
        case 'k': return COL(120, 120, 150);
        case 'g': return COL(60, 210, 70);
        case 'G': return COL(30, 130, 40);
        case 'b': return COL(160, 90, 30);
        case 'o': return COL(255, 150, 20);
        case 'O': return COL(200, 90, 0);
        case 'y': return COL(255, 222, 40);
        case 'Y': return COL(190, 140, 0);
        case 'm': return COL(150, 235, 100);
        case 'c': return COL(80, 225, 255);
        case 'B': return COL(50, 80, 255);
        default: return 0;
    }
}

static void draw_art(int i, int cx, int cy, int k32)
{
    for(int r = 0; r < 12; r++) {
        const char *row = ART[i][r];
        int y = cy - 6 + r;
        if(y < g_clip.y0 || y >= g_clip.y1) continue;
        for(int c = 0; c < 12; c++) {
            if(row[c] == '.') continue;
            int x = cx - 6 + c;
            if(x < g_clip.x0 || x >= g_clip.x1) continue;
            px_t col = art_col(row[c]);
            g_fb[FBI(x, y)] = k32 >= 32 ? col : px_mix(col, g_fb[FBI(x, y)], k32);
        }
    }
}

// ------------------------------------------------------------------ the logo

static void image_draw(const pac_image_t *im, int cx, int cy, int scale256, int k32)
{
    // nearest-pixel scaling; the letters' fill in a yellow to orange sweep, a white rim,
    // and the glow around them added on top of whatever is there
    if(scale256 <= 0 || k32 <= 0) return;
    int w = im->w * scale256 >> 8, h = im->h * scale256 >> 8;
    int x0 = cx - w / 2, y0 = cy - h / 2;
    int step = (256 << 8) / scale256;           // source 1/256 pixels a screen pixel
    px_t fill_top = px_scale(COL(255, 236, 60), k32), fill_bot = px_scale(COL(255, 132, 10), k32);
    px_t rim = px_scale(COL(255, 252, 215), k32);
    px_t glow[12];
    for(int v = 0; v < 12; v++) glow[v] = px_scale(COL(255, 140, 0), v * 3 * k32 / 32);
    int xa = x0 < g_clip.x0 ? g_clip.x0 : x0, xb = x0 + w > g_clip.x1 ? g_clip.x1 : x0 + w;
    for(int y = 0; y < h; y++) {
        int sy = y0 + y;
        if(sy < g_clip.y0 || sy >= g_clip.y1) continue;
        const uint8_t *row = im->pix + ((y * step) >> 8) * (im->w / 2);
        px_t fill = px_mix(fill_bot, fill_top, 32 - y * 32 / (h > 1 ? h : 1));
        int ix = (xa - x0) * step;
        for(int sx = xa; sx < xb; sx++, ix += step) {
            int i = ix >> 8;
            int v = (row[i >> 1] >> ((i & 1) ? 0 : 4)) & 15;
            if(!v) continue;
            px_t *p = &g_fb[FBI(sx, sy)];
            if(v >= 15) *p = rim;
            else if(v >= 12) *p = k32 >= 32 ? fill : px_mix(fill, *p, k32);
            else *p = px_add(*p, glow[v]);
        }
    }
}

void pac_logo_draw(int cx, int cy, int scale256, int k32)
{
    image_draw(&PAC_LOGO_BIG, cx, cy, scale256, k32);
}

// ------------------------------------------------------------------ the scene

static void draw_bg(const view_t *v)
{
    // the whole screen, a row at a time, two pixels a store
    px_t top = px_scale(view_col(v, 1), s_dim), bot = px_scale(view_col(v, 2), s_dim);
    for(int y = 0; y < SCR_H; y++) {
        px_t c = px_mix(bot, top, y * 32 / (SCR_H - 1));
        uint32_t cc = (uint32_t)c | ((uint32_t)c << 16);
        for(int st = 0; st < NUM_STRIPS; st++) {
            u32a *d = (u32a *)&g_fb[st * STRIP_PIX + (y << STRIP_SHIFT)];
            for(int i = 0; i < STRIP_W / 2; i++) d[i] = cc;
        }
    }
}

static void draw_dots(const pgame_t *g, const view_t *v, int ox, int oy)
{
    px_t col = px_scale(view_col(v, 3), s_dim);
    px_t halo = px_scale(col, 6), white = px_scale(C_WHITE, s_dim);
    for(int ty = 0; ty < PM_H; ty++) {
        for(int tx = 0; tx < PM_W; tx++) {
            int d = g->dot[ty][tx];
            if(!d) continue;
            int x = ox + tx * 8 + 3, y = oy + ty * 8 + 3;
            if(d == 1) {
                px_t c = ((tx * 7 + ty * 11 + v->t) & 63) < 2 ? white : col;         // a twinkle now and then
                px_t *p = &g_fb[FBI(x, y)];
                p[0] = p[1] = p[STRIP_W] = p[STRIP_W + 1] = c;
                p[-1] = px_add(p[-1], halo);
                p[STRIP_W - 1] = px_add(p[STRIP_W - 1], halo);
                p[2] = px_add(p[2], halo);
                p[STRIP_W + 2] = px_add(p[STRIP_W + 2], halo);
                p[-STRIP_W] = px_add(p[-STRIP_W], halo);
                p[-STRIP_W + 1] = px_add(p[-STRIP_W + 1], halo);
                p[2 * STRIP_W] = px_add(p[2 * STRIP_W], halo);
                p[2 * STRIP_W + 1] = px_add(p[2 * STRIP_W + 1], halo);
            } else {
                // energizers pulse
                int pulse = (isin256(v->t * 9 + tx * 20) + 16384) >> 11;     // 0..16
                gfx_glow(x + 1, y + 1, 8 + pulse / 4, px_scale(col, 8 + pulse / 2));
                aa_disc((float)x + 1.0f, (float)y + 1.0f, 3.2f, pulse > 12 ? white : col, 1.0f);
            }
        }
    }
}

static void actor_pos(int32_t ux, int32_t uy, float *sx, float *sy)
{
    *sx = (float)MX + (float)ux * (1.0f / 32.0f);
    *sy = (float)MY + (float)uy * (1.0f / 32.0f);
}

static void ghost_draw(const pgame_t *g, const view_t *v, int i, float s)
{
    const pactor_t *a = &g->gh[i];
    int mode = GL_NORMAL;
    if(a->state == GS_EYES || a->state == GS_ENTERING) mode = GL_EYES;
    else if(a->fright) mode = pac_fright_flash(g) ? GL_WHITE : GL_BLUE;
    else if(g->freeze_t > 0 && a->state == GS_ACTIVE) mode = GL_ICE;
    float sx, sy;
    actor_pos(a->x, a->y, &sx, &sy);
    px_t col = mode == GL_BLUE ? COL(50, 70, 255) : (mode == GL_ICE ? COL(150, 220, 255) : GHOST_COL[i]);
    int feet = mode == GL_ICE ? 0 : (v->t >> 2) & 1;
    for(int w = -1; w <= 1; w++) {
        float x = sx + (float)(w * MW);
        if(x < (float)(MX - 12) || x > (float)(MX + MW + 12)) continue;
        if(mode != GL_EYES) gfx_glow((int)x, (int)sy, 13, px_scale(col, 7 * s_dim / 32));
        draw_ghost_at(x, sy, s, col, a->dir, feet, mode, (float)s_dim * (1.0f / 32.0f));
    }
}

static void pac_draw_actor(const pgame_t *g, const view_t *v)
{
    float sx, sy;
    actor_pos(g->pac.x, g->pac.y, &sx, &sy);
    float mouth;
    int dir = g->pac.dir;
    if(g->phase == PP_DYING) {
        if(g->phase_t < DIE_FREEZE) {
            mouth = 0.06f;
        } else {
            // folds away upwards, as in the arcade
            int k = g->phase_t - DIE_FREEZE;
            if(k >= DIE_ANIM - 12) return;
            mouth = 0.06f + 0.44f * (float)k / (float)(DIE_ANIM - 12);
            dir = PD_UP;
        }
    } else if(g->phase == PP_READY) {
        mouth = 0.0f;
    } else {
        int c = (v->chomp >> 4) & 31;               // opens and shuts every 16 pixels
        mouth = (float)(c < 16 ? c : 31 - c) * (0.13f / 15.0f) + 0.005f;
    }
    px_t body = PAC_YELLOW;
    if(g->dash_t > 0) body = ar_lighten(body, 12 + ((v->t >> 1) & 1) * 8);
    for(int w = -1; w <= 1; w++) {
        float x = sx + (float)(w * MW);
        if(x < (float)(MX - 12) || x > (float)(MX + MW + 12)) continue;
        gfx_glow((int)x, (int)sy, 16, px_scale(COL(255, 190, 0), (g->dash_t > 0 ? 12 : 6) * s_dim / 32));
        if(g->mode == PM_NEON && g->boost >= BOOST_FULL && g->phase == PP_PLAY) {
            // charged: a halo that breathes
            int k = 6 + ((isin256(v->t * 8) + 16384) >> 12);
            gfx_ring((int)x, (int)sy, 9, 10, px_scale(COL(255, 240, 150), k * s_dim / 32), DM_ADD);
        }
        draw_pac_at(x, sy, 6.6f, dir, mouth, body, (float)s_dim * (1.0f / 32.0f));
    }
}

static void draw_extras(const pgame_t *g, const view_t *v)
{
    // the fruit under the house, bobbing in its own light
    if(g->fruit_t > 0 && !(g->fruit_t < 60 && (g->fruit_t & 4))) {
        int32_t fx, fy;
        pac_fruit_pos(&fx, &fy);
        float sx, sy;
        actor_pos(fx, fy, &sx, &sy);
        int bob = (isin256(v->t * 6) * 2) >> 14;
        gfx_glow((int)sx, (int)sy, 14, px_scale(COL(40, 26, 20), s_dim));
        draw_art(pac_fruit_for_level(g->level), (int)sx, (int)sy + bob, s_dim);
    }
    // NEON's power-up
    if(g->item_t > 0 && !(g->item_t < 60 && (g->item_t & 4))) {
        float sx = (float)(MX + g->item_x * 8 + 4), sy = (float)(MY + g->item_y * 8 + 4);
        px_t c = g->item_kind == IT_MAGNET ? COL(255, 60, 160) : COL(80, 220, 255);
        int pulse = (isin256(v->t * 7) + 16384) >> 11;
        gfx_glow((int)sx, (int)sy, 14 + pulse / 3, px_scale(c, (8 + pulse / 2) * s_dim / 32));
        for(int k = 0; k < 3; k++) {
            int ang = v->t * 5 + k * 85;
            gfx_glow((int)sx + (icos256(ang) * 10 >> 14), (int)sy + (isin256(ang) * 10 >> 14), 2, px_scale(c, s_dim));
        }
        draw_art(g->item_kind == IT_MAGNET ? ART_MAGNET : ART_FREEZE, (int)sx, (int)sy, s_dim);
    }
}

static void scene_draw(const pgame_t *g, const view_t *v)
{
    bool clear_flash = g->phase == PP_CLEAR && g->phase_t >= CLEAR_FREEZE;
    px_t wall = view_col(v, 0);
    if(clear_flash && ((g->phase_t - CLEAR_FREEZE) & 8)) wall = COL(230, 235, 255);
    else if(g->freeze_t > 0) wall = px_mix(COL(170, 230, 255), wall, 20);
    wall_luts(wall);

    draw_bg(v);
    draw_walls(g, v, g->fright_t > 0 && !clear_flash);
    // the door of the ghost house
    gfx_fill_mode(MX + 18 * 8, MY + 10 * 8 + 3, 16, 2, px_scale(COL(255, 150, 200), s_dim), DM_ADD);
    draw_dots(g, v, MX, MY);
    draw_extras(g, v);

    gfx_clip(MX, MY, MX + MW, MY + MH);
    bool ghosts = !(g->phase == PP_DYING && g->phase_t >= DIE_FREEZE) && !clear_flash && g->phase != PP_OVER;
    if(ghosts) {
        for(int i = GH_N - 1; i >= 0; i--) {
            if(g->phase == PP_EAT && i == g->ev_ghost) continue;
            ghost_draw(g, v, i, 1.0f);
        }
    }
    // the magnet's reach
    if(g->magnet_t > 0 && g->phase == PP_PLAY) {
        float sx, sy;
        actor_pos(g->pac.x, g->pac.y, &sx, &sy);
        int k = g->magnet_t < 45 ? g->magnet_t * 12 / 45 : 12;
        for(int j = 0; j < 12; j++) {
            int ang = j * 21 + v->t * 3;
            gfx_glow((int)sx + (icos256(ang) * 22 >> 14), (int)sy + (isin256(ang) * 22 >> 14), 2, px_scale(COL(255, 80, 190), k + 8));
        }
    }
    // dash afterimages
    for(int j = v->trail_n - 1; j >= 1; j--) {
        gfx_glow(MX + v->trail[j][0], MY + v->trail[j][1], 7 - j, px_scale(COL(255, 220, 90), 16 - j * 2));
    }
    if(g->phase != PP_EAT && g->phase != PP_OVER) pac_draw_actor(g, v);
    gfx_noclip();

    if(g->phase == PP_EAT) {
        // what the ghost was worth, where it was caught
        float sx, sy;
        actor_pos(g->ev_x, g->ev_y, &sx, &sy);
        char buf[8];
        fmt_int(buf, g->ev_pts);
        gfx_glow((int)sx, (int)sy, 10, COL(0, 60, 80));
        text_center_x((int)sx, (int)sy - 4, buf, (g->ev & PE_SHATTER) || g->ev_pts == 500 ? COL(200, 240, 255) : C_CYAN, 1, TX_OUTLINE);
    }
}

// ------------------------------------------------------------------ the bar across the top

static void draw_hud(const pgame_t *g, const view_t *v)
{
    char buf[24];
    fmt_commas(buf, v->shown);
    text_draw_fx(8, 3, buf, C_WHITE, 2, TX_SHADOW, NULL);
    uint32_t hi = g_arc.pac_rec[g->mode][0].value;
    if(g->score > hi) hi = g->score;
    text_center_x(160, 2, "HIGH SCORE", COL(150, 150, 195), 1, 0);
    fmt_commas(buf, hi);
    text_center_x(160, 12, buf, g->score >= hi && g->score ? C_GOLD : C_WHITE, 1, 0);
    // spare lives, and the level's fruit
    int ly = g->mode == PM_NEON ? 7 : 10;
    for(int i = 0; i < g->lives - 1 && i < 5; i++) draw_pac_at(230.0f + (float)i * 12.0f, (float)ly, 4.6f, PD_LEFT, 0.1f, PAC_YELLOW, 1.0f);
    draw_art(pac_fruit_for_level(g->level), 305, 10, 32);
    if(g->mode == PM_NEON) {
        // the dash: charging as he eats, draining as he runs
        int w = 60, x = 226, y = 16;
        int fill = g->dash_t > 0 ? g->dash_t * w / DASH_FRAMES : g->boost * w / BOOST_FULL;
        bool full = g->boost >= BOOST_FULL;
        gfx_fill(x, y, w, 3, COL(30, 24, 10));
        px_t c = full ? ((v->t & 8) ? C_WHITE : COL(255, 230, 90)) : COL(255, 170, 30);
        if(g->dash_t > 0) c = COL(255, 250, 200);
        gfx_fill(x, y, fill, 3, c);
        if(full) gfx_glow(x + w, y + 1, 5, COL(120, 100, 30));
    }
}

static void copy_str(char *d, const char *s, int n)
{
    int i = 0;
    for(; s && s[i] && i < n - 1; i++) d[i] = s[i];
    d[i] = 0;
}

static void banner(const char *a, const char *b, px_t col, int frames)
{
    copy_str(s_banner[0], a, sizeof(s_banner[0]));
    copy_str(s_banner[1], b, sizeof(s_banner[1]));
    s_banner_col = col;
    s_banner_t = frames;
}

static void draw_banner(void)
{
    if(s_banner_t <= 0) return;
    int k = s_banner_t < 10 ? s_banner_t * 32 / 10 : 32;
    bool two = s_banner[1][0] != 0;
    int cy = MY + 12 * 8 - (two ? 22 : 12);
    int w = text_width(s_banner[0], 2);
    if(two && text_width(s_banner[1], 1) > w) w = text_width(s_banner[1], 1);
    w += 24;
    gfx_fill_alpha(160 - w / 2, cy - 5, w, two ? 36 : 26, COL(0, 0, 8), k * 3 / 4);
    gfx_fill_mode(160 - w / 2, cy - 6, w, 1, px_scale(s_banner_col, k / 2), DM_ADD);
    gfx_fill_mode(160 - w / 2, cy + (two ? 31 : 21), w, 1, px_scale(s_banner_col, k / 2), DM_ADD);
    text_center(cy, s_banner[0], px_scale(s_banner_col, k), 2, TX_OUTLINE);
    if(two) text_center(cy + 21, s_banner[1], px_scale(C_WHITE, k), 1, TX_SHADOW);
}

// ------------------------------------------------------------------ what happened, made to glow

static void on_events(const pgame_t *g, view_t *v, uint32_t ev, bool full)
{
    float px, py;
    actor_pos(g->pac.x, g->pac.y, &px, &py);
    px_t dotc = view_col(v, 3);
    if(ev & PE_DOT) {
        fx_spark(px, py, rndfxr(-0.8f, 0.8f), rndfxr(-0.8f, 0.8f), dotc, 6);
    }
    if(ev & PE_POWER) {
        v->ripple_t = 36;
        v->ripple_x = (int)px - MX;
        v->ripple_y = (int)py - MY;
        fx_ring(px, py, 4, 4.0f, COL(255, 210, 240), 18, 2);
        for(int i = 0; i < 14; i++) {
            float a = (float)i / 14.0f;
            fx_spark(px, py, fcos_t(a) * 2.6f, fsin_t(a) * 2.6f, dotc, 14);
        }
    }
    if(ev & (PE_GHOST | PE_SHATTER)) {
        float gx, gy;
        actor_pos(g->ev_x, g->ev_y, &gx, &gy);
        bool ice = (ev & PE_SHATTER) != 0;
        px_t c = ice ? COL(190, 240, 255) : GHOST_COL[g->ev_ghost & 3];
        fx_ring(gx, gy, 3, 3.5f, ice ? C_WHITE : C_CYAN, 16, 2);
        for(int i = 0; i < (ice ? 22 : 16); i++) {
            float a = rndfxf(), sp = rndfxr(1.0f, ice ? 3.8f : 3.0f);
            fx_spark(gx, gy, fcos_t(a) * sp, fsin_t(a) * sp, i & 1 ? c : C_WHITE, 12 + rndfx(10));
        }
    }
    if(ev & PE_FRUIT) {
        float fx, fy;
        actor_pos(g->ev_x, g->ev_y, &fx, &fy);
        char buf[8];
        fmt_int(buf, g->ev_pts);
        if(full) fx_text(fx - (float)text_width(buf, 1) / 2.0f, fy - 12.0f, buf, COL(255, 150, 210));
        for(int i = 0; i < 12; i++) {
            float a = (float)i / 12.0f;
            fx_spark(fx, fy, fcos_t(a) * 2.0f, fsin_t(a) * 2.0f, COL(255, 120, 160), 12);
        }
    }
    if(ev & PE_ITEM) {
        float ix, iy;
        actor_pos(g->ev_x, g->ev_y, &ix, &iy);
        bool mag = g->magnet_t > 0 && g->magnet_t == MAGNET_FRAMES;
        fx_ring(ix, iy, 4, 5.0f, mag ? COL(255, 80, 190) : COL(120, 230, 255), 20, 3);
        if(full) banner(mag ? "MAGNET!" : "FREEZE!", mag ? "DOTS COME TO YOU" : "SHATTER THE GHOSTS", mag ? COL(255, 90, 200) : COL(120, 230, 255), 50);
    }
    if(g->pulled_n) {
        // the magnet's catch streaks in
        for(int i = 0; i < g->pulled_n; i++) {
            float dx = (float)(MX + g->pulled[i][0] * 8 + 4), dy = (float)(MY + g->pulled[i][1] * 8 + 4);
            fx_spark(dx, dy, (px - dx) * 0.22f, (py - dy) * 0.22f, COL(255, 150, 220), 5);
        }
    }
    if(ev & PE_DASH) {
        fx_ring(px, py, 5, 3.0f, COL(255, 240, 160), 12, 2);
    }
    if(!full) return;
    if(ev & PE_CHARGED) fx_ring(256.0f, 17.0f, 2, 1.5f, COL(255, 230, 90), 12, 1);
    if(ev & PE_ALL4) banner("PERFECT!", "ALL FOUR GHOSTS  +3000", C_GOLD, 60);
    if(ev & PE_EXTRA) {
        banner("1UP!", "AN EXTRA LIFE", COL(255, 230, 60), 50);
        fx_ring(242.0f, 9.0f, 3, 2.0f, PAC_YELLOW, 16, 2);
    }
    if(ev & PE_THAW) fx_ring(160.0f, 130.0f, 10, 8.0f, COL(160, 230, 255), 20, 2);
    if(ev & PE_LEVEL) {
        char buf[12] = "LEVEL ";
        fmt_int(buf + 6, g->level);
        banner(buf, g->maze != pac_maze_for_level(g->level - 1) ? "A NEW MAZE" : NULL, view_col(v, 0), 50);
    }
    if(ev & PE_CLEAR) {
        for(int i = 0; i < 30; i++) {
            fx_spark((float)(MX + rndfx(MW)), (float)(MY + rndfx(MH)), 0.0f, rndfxr(-0.6f, -0.1f), C_WHITE, 20 + rndfx(20));
        }
    }
    if(g->phase == PP_DYING && g->phase_t == DIE_FREEZE + DIE_ANIM - 12) {
        // the last of him, a little pop of light
        for(int i = 0; i < 12; i++) {
            float a = (float)i / 12.0f;
            fx_spark(px, py, fcos_t(a) * 2.2f, fsin_t(a) * 2.2f, PAC_YELLOW, 10);
        }
    }
}

// ------------------------------------------------------------------ the attract demo

static void demo_new(void)
{
    s_demo_seed = s_demo_seed * 1103515245u + 12345u;
    pac_new(&s_demo, PM_NEON, s_demo_seed | 1);
    memset(&s_demo_ai, 0, sizeof(s_demo_ai));
    view_reset(&s_dv, &s_demo);
}

static void demo_step(void)
{
    uint32_t ev = pac_step(&s_demo, pac_ai_keys(&s_demo, &s_demo_ai));
    if(ev & PE_POWER) {
        // no sparks behind the menus, but the walls still flare
        s_dv.ripple_t = 36;
        s_dv.ripple_x = s_demo.pac.x >> 5;
        s_dv.ripple_y = s_demo.pac.y >> 5;
    }
    view_tick(&s_dv, &s_demo);
    if(s_demo.over || s_demo.level > 4) demo_new();
}

void pac_card_update(void)
{
    demo_step();
}

void pac_card_draw(int x, int y, int w, int h, int t)
{
    // the demo at half size: 4 pixels a tile
    const pgame_t *g = &s_demo;
    const view_t *v = &s_dv;
    gfx_vgrad(x, y, w, h, view_col(v, 1), view_col(v, 2));
    int ox = x + (w - PM_W * 4) / 2, oy = y + 1;
    // walls, through a 16-colour table
    px_t lut[16];
    uint8_t r, gg, b;
    bool flash = g->phase == PP_CLEAR && g->phase_t >= CLEAR_FREEZE && ((g->phase_t - CLEAR_FREEZE) & 8);
    px_to_rgb(flash ? COL(230, 235, 255) : view_col(v, 0), &r, &gg, &b);
    for(int i = 0; i < 16; i++) {
        float tt = (float)i / 15.0f, hot = tt > 0.72f ? (tt - 0.72f) * 3.6f : 0.0f;
        lut[i] = COL(iclamp((int)(r * tt * 1.3f + (255 - r) * hot * 0.8f), 0, 255),
                     iclamp((int)(gg * tt * 1.3f + (255 - gg) * hot * 0.8f), 0, 255),
                     iclamp((int)(b * tt * 1.3f + (255 - b) * hot * 0.8f), 0, 255));
    }
    const uint8_t (*map)[PM_W] = PAC_MINI_MAP[g->maze];
    for(int ty = 0; ty < PM_H; ty++) {
        int y0 = oy + ty * 4;
        if(y0 + 4 <= g_clip.y0 || y0 >= g_clip.y1) continue;
        bool rows_in = y0 >= g_clip.y0 && y0 + 4 <= g_clip.y1;
        for(int tx = 0; tx < PM_W; tx++) {
            int idx = map[ty][tx];
            if(!idx) continue;
            int x0 = ox + tx * 4;
            if(x0 + 4 <= g_clip.x0 || x0 >= g_clip.x1) continue;
            const uint8_t *src = PAC_MINI_TILES[idx];
            if(rows_in && x0 >= g_clip.x0 && x0 + 4 <= g_clip.x1 && (x0 & (STRIP_W - 1)) <= STRIP_W - 4) {
                // all of it on screen and in one strip: straight down the rows
                px_t *p = &g_fb[FBI(x0, y0)];
                for(int rr = 0; rr < 4; rr++, p += STRIP_W, src += 2) {
                    if(src[0] >> 4) p[0] = px_add(p[0], lut[src[0] >> 4]);
                    if(src[0] & 15) p[1] = px_add(p[1], lut[src[0] & 15]);
                    if(src[1] >> 4) p[2] = px_add(p[2], lut[src[1] >> 4]);
                    if(src[1] & 15) p[3] = px_add(p[3], lut[src[1] & 15]);
                }
                continue;
            }
            for(int rr = 0; rr < 4; rr++) {
                int yy = y0 + rr;
                if(yy < g_clip.y0 || yy >= g_clip.y1) continue;
                for(int c = 0; c < 4; c++) {
                    int xx = x0 + c;
                    int vv = (src[rr * 2 + (c >> 1)] >> ((c & 1) ? 0 : 4)) & 15;
                    if(!vv || xx < g_clip.x0 || xx >= g_clip.x1) continue;
                    px_t *p = &g_fb[FBI(xx, yy)];
                    *p = px_add(*p, lut[vv]);
                }
            }
        }
    }
    // dots, one pixel each; energizers a little glow
    px_t dc = view_col(v, 3);
    for(int ty = 0; ty < PM_H; ty++) {
        for(int tx = 0; tx < PM_W; tx++) {
            int d = g->dot[ty][tx];
            if(!d) continue;
            int xx = ox + tx * 4 + 2, yy = oy + ty * 4 + 2;
            if(d == 1) gfx_pset(xx, yy, dc);
            else if(t & 8) gfx_glow(xx, yy, 3, dc);
        }
    }
    // the cast, at half size
    float sc = 0.5f;
    bool ghosts = !(g->phase == PP_DYING && g->phase_t >= DIE_FREEZE) && g->phase != PP_CLEAR;
    int cx0 = g_clip.x0, cy0 = g_clip.y0, cx1 = g_clip.x1, cy1 = g_clip.y1;
    gfx_clip(iclamp(ox, cx0, cx1), iclamp(oy, cy0, cy1), iclamp(ox + PM_W * 4, cx0, cx1), iclamp(oy + PM_H * 4, cy0, cy1));
    if(ghosts) {
        for(int i = GH_N - 1; i >= 0; i--) {
            const pactor_t *a = &g->gh[i];
            if(g->phase == PP_EAT && i == g->ev_ghost) continue;
            int mode = GL_NORMAL;
            if(a->state == GS_EYES || a->state == GS_ENTERING) mode = GL_EYES;
            else if(a->fright) mode = pac_fright_flash(g) ? GL_WHITE : GL_BLUE;
            else if(g->freeze_t > 0 && a->state == GS_ACTIVE) mode = GL_ICE;
            float gx = (float)ox + (float)a->x * (1.0f / 64.0f), gy = (float)oy + (float)a->y * (1.0f / 64.0f);
            px_t c = mode == GL_BLUE ? COL(50, 70, 255) : (mode == GL_ICE ? COL(150, 220, 255) : GHOST_COL[i]);
            if(mode != GL_EYES) gfx_glow((int)gx, (int)gy, 6, px_scale(c, 8));
            draw_ghost_at(gx, gy, sc, c, a->dir, (t >> 2) & 1, mode, 1.0f);
        }
    }
    if(g->phase != PP_EAT && !(g->phase == PP_DYING && g->phase_t >= DIE_FREEZE + DIE_ANIM - 12)) {
        float pxx = (float)ox + (float)g->pac.x * (1.0f / 64.0f), pyy = (float)oy + (float)g->pac.y * (1.0f / 64.0f);
        int c = (v->chomp >> 4) & 31;
        float mouth = g->phase == PP_DYING && g->phase_t >= DIE_FREEZE
                          ? 0.06f + 0.44f * (float)(g->phase_t - DIE_FREEZE) / (float)(DIE_ANIM - 12)
                          : (float)(c < 16 ? c : 31 - c) * (0.13f / 15.0f) + 0.005f;
        gfx_glow((int)pxx, (int)pyy, 8, COL(50, 38, 0));
        draw_pac_at(pxx, pyy, 3.3f, g->phase == PP_DYING && g->phase_t >= DIE_FREEZE ? PD_UP : g->pac.dir, mouth, PAC_YELLOW, 1.0f);
    }
    gfx_clip(cx0, cy0, cx1, cy1);
    // the name across the top, and the demo's score
    gfx_fill_alpha(x, y, w, 20, COL(0, 0, 6), 14);
    image_draw(&PAC_LOGO_SMALL, x + w / 2, y + 10, 200, 32);
    (void)h;
}

// ------------------------------------------------------------------ starting and ending

static void set_state(int st)
{
    s_st = st;
    s_st_t = 0;
}

static void start_game(int mode)
{
    s_mode = mode;
    pac_new(&s_g, mode, rng_next(&g_fxrng) ^ (plat_millis() * 2654435761u));
    view_reset(&s_v, &s_g);
    s_quit = false;
    s_rank = -1;
    s_hold = 0;
    fx_reset();
    s_banner_t = 0;
    banner(mode == PM_NEON ? "NEON" : "CLASSIC", mode == PM_NEON ? "EAT 40 DOTS, THEN SPACE TO DASH" : "PLAYER ONE", mode == PM_NEON ? COL(255, 90, 220) : C_CYAN, 60);
    set_state(P_PLAY);
}

static bool suspend_game(void)
{
    // keep the level to CONTINUE from its READY. A caught Pac-Man has lost that life;
    // if it was his last, there is nothing to keep (false).
    if(s_g.phase == PP_OVER) return false;
    if(s_g.phase == PP_DYING) {
        if(s_g.lives <= 1) {
            s_g.lives = 0;
            s_g.over = true;
            s_g.phase = PP_OVER;
            return false;
        }
        s_g.lives--;
    }
    for(int i = 0; i < 200 && s_g.phase == PP_CLEAR; i++) pac_step(&s_g, 0);
    g_arc.pac_suspended = pac_pack(&s_g, g_arc.pac_susp, PAC_PACK_LEN) == PAC_PACK_LEN;
    g_events |= EV_SAVE_ARCADE;
    return true;
}

static bool resume_game(void)
{
    if(!g_arc.pac_suspended || !pac_unpack(&s_g, g_arc.pac_susp, PAC_PACK_LEN)) {
        g_arc.pac_suspended = false;
        g_events |= EV_SAVE_ARCADE;
        return false;
    }
    s_mode = s_g.mode;
    view_reset(&s_v, &s_g);
    s_quit = false;
    s_rank = -1;
    s_hold = 0;
    fx_reset();
    char buf[12] = "LEVEL ";
    fmt_int(buf + 6, s_g.level);
    banner("CONTINUE", buf, view_col(&s_v, 0), 50);
    set_state(P_PLAY);
    return true;
}

static void game_ended(void)
{
    g_arc.pac_suspended = false;
    g_arc.pac_plays++;
    g_events |= EV_SAVE_ARCADE;
    s_rank = prec_rank(s_mode, s_g.score);
    s_over_t = 0;
    set_state(P_OVER);
}

static void commit_record(const char *name)
{
    if(s_rank < 0) return;
    prec_insert(s_mode, s_rank, name, s_g.score, s_g.level);
    s_new_rec_mode = s_mode;
    s_new_rec_rank = s_rank;
    s_rank = -1;
    g_events |= EV_SAVE_ARCADE;
}

// ------------------------------------------------------------------ playing

static uint32_t play_keys(void)
{
    uint64_t r = g_in.raw;
    uint32_t k = 0;
    if(r & (KEYBIT(K_UP) | KEYBIT(K_W))) k |= PK_UP;
    if(r & (KEYBIT(K_LEFT) | KEYBIT(K_A))) k |= PK_LEFT;
    if(r & (KEYBIT(K_DOWN) | KEYBIT(K_S))) k |= PK_DOWN;
    if(r & (KEYBIT(K_RIGHT) | KEYBIT(K_D))) k |= PK_RIGHT;
    if(r & (KEYBIT(K_SPACE) | KEYBIT(K_ENTER) | KEYBIT(K_SHIFT))) k |= PK_DASH;
    return k;
}

static void play_update(void)
{
    if(g_in.pressed & (B_PAUSE | B_B)) {
        s_pause_sel = 0;
        set_state(P_PAUSE);
        return;
    }
    if(s_hold > 0) {
        s_hold--;
        s_g.keys_prev = (uint8_t)play_keys();       // what is held through the pause does nothing
        return;
    }
    uint32_t ev = pac_step(&s_g, play_keys());
    on_events(&s_g, &s_v, ev, true);
    if(s_g.over) game_ended();
}

// ------------------------------------------------------------------ menu

enum { MN_CONTINUE, MN_CLASSIC, MN_NEON, MN_SCORES, MN_HOWTO, MN_HOME, MN_COUNT };

static bool menu_enabled(int i)
{
    return i != MN_CONTINUE || g_arc.pac_suspended;
}

static void menu_update(void)
{
    while(!menu_enabled(s_menu_sel)) s_menu_sel = (s_menu_sel + 1) % MN_COUNT;
    if(g_in.menu & B_UP) {
        do { s_menu_sel = (s_menu_sel + MN_COUNT - 1) % MN_COUNT; } while(!menu_enabled(s_menu_sel));
    }
    if(g_in.menu & B_DOWN) {
        do { s_menu_sel = (s_menu_sel + 1) % MN_COUNT; } while(!menu_enabled(s_menu_sel));
    }
    if(g_in.pressed & B_B) {
        arcade_go_home(GAME_PAC);
        return;
    }
    if(!(g_in.pressed & B_A)) return;
    switch(s_menu_sel) {
        case MN_CONTINUE:
            resume_game();
            break;
        case MN_CLASSIC:
        case MN_NEON:
            s_mode = s_menu_sel == MN_NEON ? PM_NEON : PM_CLASSIC;
            if(g_arc.pac_suspended) {
                s_confirm = CF_NEW;
                s_confirm_sel = 1;
                s_confirm_back = P_MENU;
                set_state(P_CONFIRM);
            } else {
                start_game(s_mode);
            }
            break;
        case MN_SCORES:
            s_tab = s_mode;
            s_new_rec_rank = -1;
            set_state(P_SCORES);
            break;
        case MN_HOWTO:
            set_state(P_HOWTO);
            break;
        case MN_HOME:
            arcade_go_home(GAME_PAC);
            break;
    }
}

static void backdrop_draw(int dim)
{
    // the demo game, drawn dim so what is in front reads
    s_dim = 32 - dim;
    scene_draw(&s_demo, &s_dv);
    s_dim = 32;
}

static void panel(int x, int y, int w, int h, px_t edge)
{
    // ar_panel's look, solid: nothing behind it needs to show through
    gfx_fill(x, y, w, h, COL(4, 6, 20));
    ar_neon_rect(x, y, w, h, edge, 2);
    gfx_rect(x + 1, y + 1, w - 2, h - 2, px_scale(edge, 9));
}

static void menu_draw(void)
{
    char buf[40];
    backdrop_draw(19);
    // the logo drops in, then glows
    int drop = s_st_t < 14 ? (14 - s_st_t) * (14 - s_st_t) / 3 : 0;
    pac_logo_draw(160, 38 - drop, 256, 32);
    static const char *const labels[MN_COUNT] = { "CONTINUE", "CLASSIC", "NEON", "HIGH SCORES", "HOW TO PLAY", "HOME" };
    int n = 0;
    for(int i = 0; i < MN_COUNT; i++) n += menu_enabled(i);
    int y = 84 + (MN_COUNT - n) * 9;
    gfx_fill_alpha(70, y - 8, 180, n * 18 + 8, COL(0, 0, 8), 22);
    ar_neon_rect(70, y - 8, 180, n * 18 + 8, px_scale(COL(255, 210, 40), 20), 1);
    for(int i = 0; i < MN_COUNT; i++) {
        if(!menu_enabled(i)) continue;
        const char *s = labels[i];
        if(i == MN_CONTINUE) {
            pgame_t peek;
            if(pac_unpack(&peek, g_arc.pac_susp, PAC_PACK_LEN)) {
                strcpy(buf, "CONTINUE ");
                str_cat(buf, MODE_NAMES[peek.mode]);
                str_cat(buf, " LV ");
                fmt_int(buf + strlen(buf), peek.level);
                s = buf;
            }
        }
        ar_menu_item(160, y, s, i == s_menu_sel, true, COL(255, 210, 40));
        y += 18;
    }
    // what the mode is
    gfx_fill_mode(0, SCR_H - 25, SCR_W, 25, 0, DM_SHADOW);
    static const char *const about[MN_COUNT] = {
        "PICK UP WHERE YOU LEFT OFF",
        "THE ARCADE, GHOST FOR GHOST",
        "DASH, MAGNET AND FREEZE. GO WILD",
        "THE BEST OF BOTH",
        "KEYS, POINTS AND POWERS",
        "BACK TO THE ARCADE",
    };
    text_center(SCR_H - 22, about[s_menu_sel], COL(200, 190, 150), 1, 0);
    int m = s_menu_sel == MN_NEON ? PM_NEON : (s_menu_sel == MN_CLASSIC ? PM_CLASSIC : -1);
    if(m >= 0) {
        const trec_t *r = &g_arc.pac_rec[m][0];
        strcpy(buf, "BEST  ");
        fmt_commas(buf + strlen(buf), r->value);
        str_cat(buf, "  ");
        str_cat(buf, r->name);
        text_center(SCR_H - 11, buf, C_GOLD, 1, 0);
    } else {
        text_center(SCR_H - 11, G_UP G_DOWN " SELECT   ENTER OK   CANCEL HOME", COL(120, 120, 160), 1, 0);
    }
    ar_battery(SCR_W - 22, 4);
}

// ------------------------------------------------------------------ records, how to play

static void scores_update(void)
{
    if(g_in.menu & (B_LEFT | B_RIGHT)) s_tab ^= 1;
    if(g_in.pressed & (B_A | B_B)) {
        s_new_rec_rank = -1;
        set_state(P_MENU);
    }
}

static void scores_draw(void)
{
    char buf[48];
    backdrop_draw(22);
    px_t acc = COL(255, 210, 40);
    panel(22, 40, 276, 170, acc);
    text_center(28, "HIGH SCORES", C_WHITE, 2, TX_OUTLINE);
    for(int m = 0; m < PM_MODES; m++) {
        int cx = 110 + m * 100;
        bool on = m == s_tab;
        if(on) gfx_fill_alpha(cx - 44, 50, 88, 14, px_scale(acc, 14), 14);
        text_center_x(cx, 53, MODE_NAMES[m], on ? C_WHITE : C_DIM, 1, 0);
        if(on) gfx_hline(cx - 44, cx + 43, 64, acc);
    }
    text_draw(36, 74, "     NAME        SCORE", C_DIM, 1);
    text_right(284, 74, "LEVEL", C_DIM, 1, 0);
    for(int i = 0; i < TREC_N; i++) {
        const trec_t *r = &g_arc.pac_rec[s_tab][i];
        int y = 90 + i * 18;
        px_t c = i == 0 ? C_GOLD : (i < 3 ? C_WHITE : C_GREY);
        if(s_tab == s_new_rec_mode && i == s_new_rec_rank && (g_arc_t & 8)) c = acc;
        draw_ghost_at(41.0f, (float)y + 3.0f, 0.6f, GHOST_COL[i & 3], PD_RIGHT, (g_arc_t >> 3) & 1, GL_NORMAL, 1.0f);
        fmt_int(buf, i + 1);
        text_draw(52, y, buf, c, 1);
        text_draw(66, y, r->name, c, 1);
        fmt_commas(buf, r->value);
        text_right(222, y, buf, c, 1, 0);
        fmt_int(buf, r->level);
        text_right(280, y, buf, c, 1, 0);
    }
    strcpy(buf, "GAMES ");
    fmt_int(buf + strlen(buf), (long)g_arc.pac_plays);
    text_center(190, buf, C_DIM, 1, 0);
    text_center(SCR_H - 12, G_LEFT G_RIGHT " MODE   ENTER BACK", COL(120, 120, 160), 1, 0);
}

static void howto_update(void)
{
    if(g_in.pressed & (B_A | B_B)) set_state(P_MENU);
}

static void howto_draw(void)
{
    static const char *const names[GH_N] = { "BLINKY", "PINKY", "INKY", "CLYDE" };
    backdrop_draw(24);
    px_t acc = COL(255, 210, 40);
    panel(10, 30, 300, 204, acc);
    text_center(18, "HOW TO PLAY", C_WHITE, 2, TX_OUTLINE);
    // the cast
    for(int i = 0; i < GH_N; i++) {
        int cx = 52 + i * 72;
        int look = (g_arc_t / 40 + i) & 3;
        gfx_glow(cx, 50, 12, px_scale(GHOST_COL[i], 7));
        draw_ghost_at((float)cx, 50.0f, 1.0f, GHOST_COL[i], look, (g_arc_t >> 3) & 1, GL_NORMAL, 1.0f);
        text_center_x(cx, 62, names[i], GHOST_COL[i], 1, 0);
    }
    struct { const char *k, *v; } rows[] = {
        { G_LEFT G_UP G_DOWN G_RIGHT " / WASD", "STEER. A TAP WAITS FOR THE TURN" },
        { "CANCEL / TAB", "PAUSE" },
        { "HOLD POWER", "SAVE THE GAME AND SWITCH OFF" },
    };
    int y = 78;
    for(unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        text_draw(22, y, rows[i].k, acc, 1);
        text_draw(116, y, rows[i].v, C_WHITE, 1);
        y += 12;
    }
    y += 3;
    text_draw(22, y, "DOT 10  ENERGIZER 50  GHOSTS 200-1600", C_GREY, 1);
    y += 11;
    text_draw(22, y, "FRUIT 100-5000   EXTRA LIFE AT 10,000", C_GREY, 1);
    y += 11;
    text_draw(22, y, "EVERY FEW LEVELS A NEW MAZE AND COLOUR", C_GREY, 1);
    y += 15;
    text_draw(22, y, "NEON", COL(255, 90, 220), 1);
    y += 12;
    text_draw(22, y, "SPACE: DASH, THROUGH GHOSTS. 40 DOTS", C_WHITE, 1);
    y += 11;
    text_draw(22, y, "CHARGE IT. MAGNET PULLS DOTS IN.", C_WHITE, 1);
    y += 11;
    text_draw(22, y, "FREEZE: TOUCH A GHOST TO SHATTER IT.", C_WHITE, 1);
    y += 11;
    text_draw(22, y, "ALL FOUR ON ONE ENERGIZER: +3000", C_WHITE, 1);
    draw_art(ART_MAGNET, 282, y - 30, 32);
    draw_art(ART_FREEZE, 282, y - 12, 32);
}

// ------------------------------------------------------------------ pause, confirm

enum { PA_RESUME, PA_RESTART, PA_SAVEQUIT, PA_QUIT, PA_COUNT };

static void pause_update(void)
{
    if(g_in.menu & B_UP) s_pause_sel = (s_pause_sel + PA_COUNT - 1) % PA_COUNT;
    if(g_in.menu & B_DOWN) s_pause_sel = (s_pause_sel + 1) % PA_COUNT;
    if(g_in.pressed & (B_PAUSE | B_B)) {
        s_hold = 20;
        set_state(P_PLAY);
        return;
    }
    if(!(g_in.pressed & B_A)) return;
    switch(s_pause_sel) {
        case PA_RESUME:
            s_hold = 20;
            set_state(P_PLAY);
            break;
        case PA_RESTART:
        case PA_QUIT:
            s_confirm = s_pause_sel == PA_RESTART ? CF_RESTART : CF_QUIT;
            s_confirm_sel = 1;
            s_confirm_back = P_PAUSE;
            set_state(P_CONFIRM);
            break;
        case PA_SAVEQUIT:
            if(!suspend_game()) {
                game_ended();
                break;
            }
            s_menu_sel = MN_CONTINUE;
            set_state(P_MENU);
            break;
    }
}

static void pause_draw(void)
{
    static const char *const items[PA_COUNT] = { "RESUME", "RESTART", "SAVE AND QUIT", "END GAME" };
    char buf[32];
    px_t acc = view_col(&s_v, 0);
    gfx_fill_alpha(0, MY, SCR_W, MH, COL(0, 0, 6), 16);
    ar_panel(80, 70, 160, 110, acc);
    text_center(58, "PAUSED", C_WHITE, 2, TX_OUTLINE);
    for(int i = 0; i < PA_COUNT; i++) ar_menu_item(160, 84 + i * 18, items[i], i == s_pause_sel, true, acc);
    strcpy(buf, MODE_NAMES[s_mode]);
    str_cat(buf, "  LEVEL ");
    fmt_int(buf + strlen(buf), s_g.level);
    text_center(160, buf, C_DIM, 1, 0);
}

static void confirm_update(void)
{
    if(g_in.menu & (B_UP | B_DOWN | B_LEFT | B_RIGHT)) s_confirm_sel ^= 1;
    if(g_in.pressed & B_B) {
        set_state(s_confirm_back);
        return;
    }
    if(!(g_in.pressed & B_A)) return;
    if(s_confirm_sel == 1) {
        set_state(s_confirm_back);
        return;
    }
    switch(s_confirm) {
        case CF_QUIT:
            s_quit = true;
            s_g.over = true;
            game_ended();
            break;
        case CF_RESTART:
            if(g_arc.pac_suspended) g_events |= EV_SAVE_ARCADE;
            g_arc.pac_suspended = false;
            start_game(s_mode);
            break;
        case CF_NEW:
            g_arc.pac_suspended = false;
            g_events |= EV_SAVE_ARCADE;
            start_game(s_mode);
            break;
    }
}

static void confirm_draw(void)
{
    static const char *const heads[3] = { "END GAME?", "RESTART?", "NEW GAME?" };
    static const char *const lines[3] = { "THIS GAME ENDS HERE.", "THIS GAME IS LOST.", "YOUR SAVED GAME WILL BE LOST." };
    px_t acc = COL(255, 210, 40);
    if(s_confirm == CF_NEW) backdrop_draw(22);
    else gfx_fill_alpha(0, MY, SCR_W, MH, COL(0, 0, 6), 16);
    panel(50, 86, 220, 80, acc);
    text_center(74, heads[s_confirm], C_WHITE, 2, TX_OUTLINE);
    text_center(100, lines[s_confirm], C_GREY, 1, 0);
    ar_menu_item(160, 124, "YES", s_confirm_sel == 0, true, acc);
    ar_menu_item(160, 142, "NO", s_confirm_sel == 1, true, acc);
}

// ------------------------------------------------------------------ game over, results, names

#define OVER_PANEL  50          // GAME OVER in the maze first, then the results

static void over_update(void)
{
    s_over_t++;
    if(s_over_t < OVER_PANEL + 12) return;
    if(g_in.pressed & (B_A | B_B)) {
        if(s_rank >= 0) {
            strcpy(s_name, g_arc.name);
            s_name_len = (int)strlen(s_name);
            set_state(P_NAME);
        } else {
            s_menu_sel = s_mode == PM_NEON ? MN_NEON : MN_CLASSIC;
            set_state(P_MENU);
        }
    } else if((g_in.raw_pressed & KEYBIT(K_R)) && s_rank < 0) {
        start_game(s_mode);
    }
}

static void over_draw(void)
{
    if(s_over_t < OVER_PANEL) {
        int k = s_over_t < 8 ? s_over_t * 4 : 32;
        text_center(MY + 15 * 8 - 4, s_quit ? "GAME ENDED" : "GAME  OVER", px_scale(COL(255, 40, 40), k), 2, TX_OUTLINE);
        return;
    }
    char buf[24];
    int k = iclamp((s_over_t - OVER_PANEL) * 4, 0, 32);
    int y0 = 40 + (32 - k);
    px_t acc = s_rank >= 0 ? C_GOLD : COL(255, 210, 40);
    ar_panel(60, y0, 200, 156, acc);
    text_center(y0 + 8, s_quit ? "GAME ENDED" : "GAME OVER", s_rank >= 0 ? C_GOLD : C_WHITE, 2, TX_OUTLINE);
    int y = y0 + 32;
    fmt_commas(buf, s_g.score);
    text_center(y, buf, C_WHITE, 3, TX_OUTLINE);
    y += 30;
    struct { const char *l; char v[16]; } rows[4];
    rows[0].l = "LEVEL";
    fmt_int(rows[0].v, s_g.level);
    rows[1].l = "GHOSTS EATEN";
    fmt_int(rows[1].v, s_g.ghosts_eaten);
    rows[2].l = "FRUIT";
    fmt_int(rows[2].v, s_g.fruits_eaten);
    rows[3].l = "TIME";
    uint32_t secs = s_g.frames / FPS;
    fmt_int(rows[3].v, (long)(secs / 60));
    str_cat(rows[3].v, ":");
    fmt_score(rows[3].v + strlen(rows[3].v), secs % 60, 2);
    for(int i = 0; i < 4; i++, y += 12) {
        text_draw(76, y, rows[i].l, C_GREY, 1);
        text_right(244, y, rows[i].v, C_WHITE, 1, 0);
    }
    y += 4;
    if(s_rank >= 0) {
        strcpy(buf, "NEW RECORD!  #");
        fmt_int(buf + strlen(buf), s_rank + 1);
        text_center(y, buf, (g_arc_t & 8) ? C_GOLD : C_WHITE, 1, TX_SHADOW);
    } else {
        text_center(y, "ENTER: MENU    R: PLAY AGAIN", C_DIM, 1, 0);
    }
}

static void name_commit(void)
{
    const char *nm = s_name_len ? s_name : "PLAYER";
    strcpy(g_arc.name, s_name);
    commit_record(nm);
    s_tab = s_mode;
    set_state(P_SCORES);
}

static void name_update(void)
{
    for(int k = 10; k < 60; k++) {
        if(!(g_in.raw_pressed & KEYBIT(k))) continue;
        char c = key_to_char(k, false);
        if(k == K_SPACE) c = ' ';
        if(c && s_name_len < NAME_LEN) {
            s_name[s_name_len++] = c;
            s_name[s_name_len] = 0;
        }
    }
    if((g_in.raw_pressed & KEYBIT(K_DEL)) && s_name_len > 0) s_name[--s_name_len] = 0;
    if((g_in.raw_pressed & KEYBIT(K_ENTER)) && s_st_t > 15) name_commit();
}

static void name_draw(void)
{
    char buf[24];
    px_t acc = COL(255, 210, 40);
    backdrop_draw(22);
    panel(24, 50, 272, 150, acc);
    text_center(38, "NEW RECORD", C_GOLD, 2, TX_OUTLINE);
    fmt_commas(buf, s_g.score);
    text_center(66, buf, C_WHITE, 3, TX_OUTLINE);
    strcpy(buf, MODE_NAMES[s_mode]);
    str_cat(buf, "  #");
    fmt_int(buf + strlen(buf), s_rank + 1);
    text_center(96, buf, acc, 1, 0);
    text_center(114, "TYPE YOUR NAME ON THE KEYBOARD", C_GREY, 1, 0);
    int w = NAME_LEN * 12 + 12;
    gfx_fill(160 - w / 2, 130, w, 22, COL(8, 10, 30));
    ar_neon_rect(160 - w / 2, 130, w, 22, acc, 1);
    text_draw(160 - w / 2 + 6, 134, s_name, C_WHITE, 2);
    if(s_name_len < NAME_LEN && (g_arc_t & 8)) gfx_fill(160 - w / 2 + 6 + s_name_len * 12, 147, 10, 2, acc);
    text_center(166, G_CROSS " DEL: ERASE     ENTER: DONE", C_DIM, 1, 0);
}

// ------------------------------------------------------------------ for the desktop build's tests

static pai_t s_bot;

void pac_debug_start(int mode)
{
    start_game(mode);
    memset(&s_bot, 0, sizeof(s_bot));
}

uint64_t pac_bot_keys(void)
{
    if(s_st != P_PLAY) return 0;
    uint32_t k = pac_ai_keys(&s_g, &s_bot);
    uint64_t raw = 0;
    if(k & PK_UP) raw |= KEYBIT(K_UP);
    if(k & PK_LEFT) raw |= KEYBIT(K_LEFT);
    if(k & PK_DOWN) raw |= KEYBIT(K_DOWN);
    if(k & PK_RIGHT) raw |= KEYBIT(K_RIGHT);
    if(k & PK_DASH) raw |= KEYBIT(K_SPACE);
    return raw;
}

const pgame_t *pac_debug_game(void)
{
    return &s_g;
}

int pac_debug_screen(void)
{
    return s_st;
}

// ------------------------------------------------------------------ entry points

void pac_ui_init(void)
{
    demo_new();
}

void pac_enter(void)
{
    s_menu_sel = g_arc.pac_suspended ? MN_CONTINUE : MN_CLASSIC;
    fx_reset();
    s_banner_t = 0;
    set_state(P_MENU);
}

bool pac_in_play(void)
{
    return s_st == P_PLAY;
}

void pac_power_tap(void)
{
    if(s_st == P_PLAY) {
        s_pause_sel = 0;
        set_state(P_PAUSE);
    }
}

static void off_mid_game(void)
{
    if(suspend_game()) return;
    game_ended();
    commit_record(g_arc.name[0] ? g_arc.name : "PLAYER");
}

void pac_before_off(void)
{
    switch(s_st) {
        case P_PLAY:
        case P_PAUSE:
            off_mid_game();
            break;
        case P_CONFIRM:
            if(s_confirm != CF_NEW) off_mid_game();
            break;
        case P_OVER:
            commit_record(g_arc.name[0] ? g_arc.name : "PLAYER");
            break;
        case P_NAME:
            name_commit();
            break;
        default:
            break;
    }
}

void pac_update(void)
{
    s_st_t++;
    bool menus = s_st == P_MENU || s_st == P_SCORES || s_st == P_HOWTO || (s_st == P_CONFIRM && s_confirm == CF_NEW) ||
                 s_st == P_NAME;
    if(menus) demo_step();
    switch(s_st) {
        case P_MENU:    menu_update(); break;
        case P_SCORES:  scores_update(); break;
        case P_HOWTO:   howto_update(); break;
        case P_PLAY:    play_update(); break;
        case P_PAUSE:   pause_update(); break;
        case P_CONFIRM: confirm_update(); break;
        case P_OVER:    over_update(); break;
        case P_NAME:    name_update(); break;
    }
    if(s_st == P_PLAY || s_st == P_OVER) view_tick(&s_v, &s_g);
    fx_update();
    if(s_banner_t > 0 && s_st == P_PLAY && !s_hold) s_banner_t--;
}

void pac_draw(void)
{
    switch(s_st) {
        case P_MENU:    menu_draw(); break;
        case P_SCORES:  scores_draw(); break;
        case P_HOWTO:   howto_draw(); break;
        case P_NAME:    name_draw(); break;
        case P_CONFIRM:
            if(s_confirm == CF_NEW) {
                confirm_draw();
                break;
            }
            // fall through: over the game
        default:
            scene_draw(&s_g, &s_v);
            draw_hud(&s_g, &s_v);
            fx_draw();
            draw_banner();
            if(s_st == P_PLAY && s_g.phase == PP_READY) {
                text_center(MY + 15 * 8 - 4, "READY!", PAC_YELLOW, 2, TX_OUTLINE);
            }
            if(s_st == P_PLAY && s_hold > 0 && (s_hold & 4)) text_center(MY + 15 * 8 - 4, "GET READY", PAC_YELLOW, 2, TX_OUTLINE);
            if(s_st == P_PAUSE) pause_draw();
            if(s_st == P_CONFIRM) confirm_draw();
            if(s_st == P_OVER) over_draw();
            break;
    }
    fx_draw_top();
}
