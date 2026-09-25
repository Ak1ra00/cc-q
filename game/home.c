// Q ARCADE - the home screen: every game as a live card on a neon horizon.
//
// LEFT / RIGHT slide between the cards, ENTER launches the one in front (it
// zooms up to fill the screen, then the game opens behind a wave of blocks).
// DOWN reaches SETTINGS (brightness, sync, auto off and shake, shared with
// every game) and SYSTEM. Adding a game means a theme, a card and a line in
// arcade.c's app_of().
#include "arcade.h"
#include <string.h>

enum { H_CARDS, H_BUTTONS, H_SETTINGS, H_LAUNCH };

typedef struct {
    px_t sky0, sky1, floor, grid, accent, glow;
    const char *name, *tag;
} theme_t;

static const theme_t THEMES[NUM_GAMES] = {
    { COL(6, 2, 22), COL(64, 12, 88), COL(10, 2, 20), COL(255, 60, 200), COL(205, 125, 255), COL(110, 30, 150),
      "QUASAR", "SHOOT-'EM-UP  " G_DOT "  FIVE STAGES  " G_DOT "  FIVE BOSSES" },
    { COL(2, 8, 26), COL(8, 52, 100), COL(2, 8, 22), COL(40, 190, 255), COL(95, 225, 255), COL(20, 100, 190),
      "TETRIS", "MARATHON  " G_DOT "  SPRINT 40  " G_DOT "  ULTRA 2:00" },
    { COL(14, 6, 2), COL(84, 44, 4), COL(12, 5, 1), COL(255, 190, 30), COL(255, 222, 70), COL(170, 100, 10),
      "PAC-MAN", "CLASSIC  " G_DOT "  NEON  " G_DOT "  THREE MAZES" },
};

#define CARD_W      188
#define CARD_H      110
#define CARD_Y      38
#define CARD_GAP    206
#define HORIZON     132
#define LAUNCH_ZOOM 11
#define LAUNCH_END  18

static int s_state;
static int s_sel;
static int s_scroll256;         // carousel position, 256 per card
static int s_btn;
static int s_set_sel;
static int s_t;
static int s_launch_t;
static int s_bump;
static tgame_t s_demo;
static tai_t s_demo_ai;
static int s_cover[4];          // the launching card's rectangle: whatever it covers is not drawn
static uint32_t s_demo_seed = 1;

// ------------------------------------------------------------------ helpers

static px_t mixc(px_t a, px_t b, int k32)
{
    // k32 = 0 gives a, 32 gives b
    return px_mix(b, a, iclamp(k32, 0, 32));
}

static theme_t cur_theme(void)
{
    // blend the two themes either side of the carousel position
    int i = iclamp(s_scroll256 >> 8, 0, NUM_GAMES - 1);
    int j = i + 1 < NUM_GAMES ? i + 1 : i;
    int k = (s_scroll256 - (i << 8)) >> 3;
    const theme_t *a = &THEMES[i], *b = &THEMES[j];
    theme_t t = *a;
    t.sky0 = mixc(a->sky0, b->sky0, k);
    t.sky1 = mixc(a->sky1, b->sky1, k);
    t.floor = mixc(a->floor, b->floor, k);
    t.grid = mixc(a->grid, b->grid, k);
    t.accent = mixc(a->accent, b->accent, k);
    t.glow = mixc(a->glow, b->glow, k);
    return t;
}

static int ease(int t, int n)
{
    // smoothstep, 0..256
    if(t <= 0) return 0;
    if(t >= n) return 256;
    int x = (t << 8) / n;
    return (x * x * (768 - 2 * x)) >> 16;
}

static uint32_t hash(uint32_t v)
{
    v = (v ^ 61u) ^ (v >> 16);
    v *= 9u;
    v ^= v >> 4;
    v *= 0x27d4eb2du;
    return v ^ (v >> 15);
}

static void demo_reset(void)
{
    s_demo_seed = s_demo_seed * 1103515245u + 12345u;
    tet_new(&s_demo, TM_MARATHON, 1, s_demo_seed | 1);
    memset(&s_demo_ai, 0, sizeof(s_demo_ai));
}

// ------------------------------------------------------------------ the backdrop

static void draw_backdrop(const theme_t *th)
{
    // sky, with the horizon glowing
    gfx_vgrad(0, 0, SCR_W, HORIZON, th->sky0, th->sky1);
    ar_add_vgrad(0, HORIZON - 34, SCR_W, 34, 0, px_scale(th->glow, 14));
    gfx_vgrad(0, HORIZON, SCR_W, SCR_H - HORIZON, th->floor, mixc(th->floor, th->sky1, 10));

    // stars, twinkling and drifting
    for(int i = 0; i < 46; i++) {
        uint32_t h = hash((uint32_t)i * 7919u);
        int x = (int)((h % 340) + 340 - ((uint32_t)s_t * (1 + (h >> 29)) / 4) % 340) % 340 - 10;
        int y = (int)((h >> 9) % (HORIZON - 8));
        int tw = isin256((int)(h >> 3) + s_t * (2 + (int)(h & 3)));
        px_t c = px_scale(COL(200, 210, 255), 12 + ((tw * 12) >> 14));
        gfx_padd(x, y, c);
        if((h & 15) == 0) {
            gfx_padd(x - 1, y, px_scale(c, 12));
            gfx_padd(x + 1, y, px_scale(c, 12));
            gfx_padd(x, y - 1, px_scale(c, 12));
            gfx_padd(x, y + 1, px_scale(c, 12));
        }
    }

    // embers rising off the horizon
    for(int i = 0; i < 18; i++) {
        uint32_t h = hash((uint32_t)i * 104729u + 7u);
        int life = 120 + (int)(h % 90);
        int p = (s_t + (int)(h >> 8)) % life;
        int x = (int)(h % SCR_W) + ((isin256(p * 3 + (int)h) * 6) >> 14);
        int y = HORIZON - p * HORIZON / life;
        int k = p < 20 ? p : (life - p) * 20 / (life / 2 + 1);
        gfx_glow(x, y, 2, px_scale(th->accent, iclamp(k, 0, 20)));
    }

    // the neon floor, rushing towards us
    int phase = (s_t * 8) & 255;
    for(int i = 0; i < 12; i++) {
        int z = (i << 8) + 256 - phase;         // 1/256 units, 1..12
        int y = HORIZON + (108 * 256) / (z + 60) - 18;
        if(y <= HORIZON || y >= SCR_H) continue;
        int k = iclamp((y - HORIZON) * 32 / (SCR_H - HORIZON), 3, 32);
        gfx_fill_mode(0, y, SCR_W, 1, px_scale(th->grid, k * 3 / 4), DM_ADD);
    }
    for(int k = -9; k <= 9; k++) {
        int xb = 160 + k * 46, xt = 160 + k * 8;
        gfx_line(xt, HORIZON, xb, SCR_H - 1, px_scale(th->grid, 9), DM_ADD);
    }
    gfx_fill_mode(0, HORIZON, SCR_W, 1, th->grid, DM_ADD);
}

// ------------------------------------------------------------------ cards

static void card_quasar(int x, int y, int w, int h, int t)
{
    gfx_vgrad(x, y, w, h, COL(10, 4, 30), COL(38, 8, 56));
    // the quasar glowing off to the right
    gfx_glow(x + w - 36, y + h / 2 + 6, 56, COL(60, 18, 96));
    gfx_glow(x + w - 36, y + h / 2 + 6, 14, COL(220, 170, 255));
    for(int k = 0; k < 2; k++) {
        gfx_ring(x + w - 36, y + h / 2 + 6, 20 + k * 9 + ((t >> 1) % 9), 21 + k * 9 + ((t >> 1) % 9),
                 COL(40, 16, 60), DM_ADD);
    }
    // stars streaming past
    for(int i = 0; i < 34; i++) {
        uint32_t hh = hash((uint32_t)i * 31u + 5u);
        int layer = i % 3;
        int sx = x + w - 1 - (int)(((uint32_t)t * (uint32_t)(1 + layer * 3) + (hh >> 8)) % (uint32_t)(w + 12));
        int sy = y + (int)(hh % (uint32_t)h);
        if(layer == 2) gfx_fill_mode(sx, sy, 5, 1, COL(110, 120, 200), DM_ADD);
        else gfx_padd(sx, sy, layer ? COL(150, 150, 210) : COL(80, 80, 130));
    }
    // three drones weave in and burst
    for(int e = 0; e < 3; e++) {
        int ph = (t + e * 37) % 110;
        int ex = x + w + 14 - ph * 2;
        int ey = y + 30 + e * 26 + ((isin256(ph * 5 + e * 80) * 7) >> 14);
        if(ph < 62) {
            gfx_sprite(&SPR_DRONE, ex, ey, 0, NULL, DM_NORMAL, 0);
        } else if(ph < 76) {
            int q = ph - 62;
            gfx_glow(ex, ey, 14 - q / 2, px_scale(COL(255, 190, 110), 32 - q * 2));
            gfx_ring(ex, ey, q * 2, q * 2 + 2, px_scale(COL(255, 160, 90), 28 - q * 2), DM_ADD);
        }
    }
    // our ship, bobbing, guns going
    int bob = (isin256(t * 5) * 7) >> 14;
    int sx = x + 34, sy = y + h / 2 + 12 + bob;
    for(int k = 0; k < 6; k++) {
        int age = (t % 5) + k * 5;
        int by = y + h / 2 + 12 + ((isin256((t - age) * 5) * 7) >> 14);
        int bx = sx + 16 + age * 7;
        if(bx < x + w) gfx_sprite(&SPR_SHOT_PULSE, bx, by, 0, NULL, DM_ADD, 0);
    }
    gfx_glow(sx - 17, sy, 5 + ((t >> 1) & 1), COL(255, 140, 60));
    gfx_sprite(&SPR_PLAYER, sx, sy, 0, NULL, DM_NORMAL, 0);
    // the name
    gfx_glow(x + w / 2, y + 22, 30, COL(40, 14, 60));
    gfx_sprite_rot(&SPR_LOGO, x + w / 2, y + 22, 0, 150, NULL, DM_NORMAL, 0);
}

static void draw_demo_board(int wx, int wy, int cs)
{
    const tgame_t *g = &s_demo;
    gfx_fill_mode(wx - 1, wy - 1, TB_W * cs + 2, TB_VIS * cs + 2, 0, DM_SHADOW);
    ar_neon_rect(wx - 2, wy - 2, TB_W * cs + 4, TB_VIS * cs + 4, COL(60, 170, 255), 1);
    for(int r = 0; r < TB_VIS; r++) {
        int y = r + TB_HIDDEN;
        bool clearing = false;
        for(int i = 0; i < g->clear_n; i++) clearing |= g->clear_row[i] == y;
        for(int x = 0; x < TB_W; x++) {
            if(!g->cell[y][x]) continue;
            if(clearing) gfx_fill(wx + x * cs, wy + r * cs, cs, cs, (g->wait & 2) ? C_WHITE : COL(200, 240, 255));
            else ar_cell(wx + x * cs, wy + r * cs, cs, g->cell[y][x]);
        }
    }
    if(g->active) {
        const int8_t (*c)[2] = TET_SHAPE[g->type][g->rot];
        int gy = tet_ghost_y(g);
        for(int i = 0; i < 4; i++) {
            int cx = g->x + c[i][0];
            int cy = gy + c[i][1] - TB_HIDDEN;
            if(cy >= 0) ar_ghost_cell(wx + cx * cs, wy + cy * cs, cs, g->type);
        }
        for(int i = 0; i < 4; i++) {
            int cx = g->x + c[i][0];
            int cy = g->y + c[i][1] - TB_HIDDEN;
            if(cy >= 0) ar_cell(wx + cx * cs, wy + cy * cs, cs, g->type);
        }
    }
}

static void card_tetris(int x, int y, int w, int h, int t)
{
    gfx_vgrad(x, y, w, h, COL(4, 10, 36), COL(10, 36, 74));
    gfx_glow(x + 50, y + 30, 50, COL(10, 40, 80));
    // blocks falling behind the name
    for(int i = 0; i < 6; i++) {
        uint32_t hh = hash((uint32_t)i * 977u + 3u);
        int sp = 1 + (int)(hh % 2);
        int py = y - 20 + (int)(((uint32_t)t * (uint32_t)sp / 2 + (hh >> 8)) % (uint32_t)(h + 40));
        int px = x + 10 + (int)((hh >> 4) % (uint32_t)(w - 90));
        ar_piece(PC_I + (int)(hh % 7), px, py, 4);
    }
    gfx_fill_mode(x, y, w - 64, h, 0, DM_SHADOW);
    draw_demo_board(x + w - 60, y + 5, 5);
    // the name, and the demo's running score
    int cx = (w - 64) / 2 + x;
    ar_tetris_logo(cx, y + 14, 4, t + 100);
    char buf[16];
    text_center_x(cx, y + 44, "SCORE", COL(120, 170, 220), 1, 0);
    fmt_commas(buf, s_demo.score);
    text_center_x(cx, y + 55, buf, C_WHITE, 1, TX_SHADOW);
    text_center_x(cx, y + 70, "NEXT", COL(120, 170, 220), 1, 0);
    for(int i = 0; i < 3; i++) ar_piece(s_demo.next[i], cx - 30 + i * 30, y + 86, 5);
}

static void card_caption(int x, int y, int w, int h, int game)
{
    char buf[24], left[32];
    gfx_fill_alpha(x, y + h - 14, w, 14, COL(0, 0, 8), 22);
    const char *badge = NULL;
    if(game == GAME_QUASAR) {
        strcpy(left, "HI ");
        fmt_commas(buf, g_save.scores[0].score);
        str_cat(left, buf);
        if(g_save.run.stage) {
            strcpy(buf, "SAVED: STAGE ");
            fmt_int(buf + strlen(buf), g_save.run.stage);
            badge = buf;
        }
    } else if(game == GAME_TETRIS) {
        strcpy(left, "BEST ");
        fmt_commas(buf, g_arc.rec[TM_MARATHON][0].value);
        str_cat(left, buf);
        if(g_arc.suspended) badge = "PAUSED GAME";
    } else {
        strcpy(left, "BEST ");
        uint32_t b = g_arc.pac_rec[PM_CLASSIC][0].value;
        if(g_arc.pac_rec[PM_NEON][0].value > b) b = g_arc.pac_rec[PM_NEON][0].value;
        fmt_commas(buf, b);
        str_cat(left, buf);
        if(g_arc.pac_suspended) badge = "PAUSED GAME";
    }
    text_draw(x + 5, y + h - 11, left, COL(190, 200, 230), 1);
    if(badge) text_right(x + w - 5, y + h - 11, badge, (g_arc_t & 16) ? C_GOLD : COL(200, 160, 60), 1, 0);
}

static void draw_card(int game, int x, int y, int w, int h, int t)
{
    gfx_clip(x, y, x + w, y + h);
    if(game == GAME_QUASAR) card_quasar(x, y, w, h, t);
    else if(game == GAME_TETRIS) card_tetris(x, y, w, h, t);
    else pac_card_draw(x, y, w, h, t);
    card_caption(x, y, w, h, game);
    gfx_noclip();
}

static void draw_cards(const theme_t *th, int rise)
{
    for(int i = 0; i < NUM_GAMES; i++) {
        int off = (i << 8) - s_scroll256;               // 256 per card
        if(off < -400 || off > 400) continue;
        int cx = 160 + (off * CARD_GAP) / 256;
        if(i == s_sel) cx += s_bump;
        int x = cx - CARD_W / 2, y = CARD_Y + rise;
        int d = iabs(off);
        if(x - 6 >= s_cover[0] && y - 6 >= s_cover[1] && x + CARD_W + 6 <= s_cover[2] && y + CARD_H + 6 <= s_cover[3]) {
            continue;
        }
        // shadow and floor reflection
        gfx_fill_mode(x + 5, y + 6, CARD_W, CARD_H, 0, DM_SHADOW);
        ar_add_vgrad(x + 6, y + CARD_H + 2, CARD_W - 12, 14, px_scale(THEMES[i].accent, d < 128 ? 6 : 2), 0);
        draw_card(i, x, y, CARD_W, CARD_H, s_t);
        // the one in front glows; the others sit back in the dark
        if(d < 128 && s_state != H_BUTTONS && s_state != H_SETTINGS) {
            int pulse = 2 + ((isin256(s_t * 6) + 16384) >> 14);
            ar_neon_rect(x - 1, y - 1, CARD_W + 2, CARD_H + 2, THEMES[i].accent, pulse);
        } else {
            gfx_rect(x - 1, y - 1, CARD_W + 2, CARD_H + 2, px_scale(THEMES[i].accent, 14));
        }
        int dim = d * 22 / 256;
        if(s_state == H_BUTTONS || s_state == H_SETTINGS) dim += 10;
        if(dim > 0) gfx_fill_alpha(x - 1, y - 1, CARD_W + 2, CARD_H + 2, COL(0, 0, 6), iclamp(dim, 0, 26));
    }
    (void)th;
}

// ------------------------------------------------------------------ header, footer, buttons

static void draw_header(const theme_t *th, int drop)
{
    static const char TITLE[] = "Q ARCADE";
    px_t grad[9];
    for(int i = 0; i < 9; i++) grad[i] = mixc(C_WHITE, th->accent, i * 4);
    int y = 7 - drop;
    gfx_glow(160, y + 9, 46, px_scale(th->glow, 12));
    int w = text_width(TITLE, 2);
    text_draw_fx(160 - w / 2, y, TITLE, C_WHITE, 2, TX_OUTLINE | TX_GRAD, grad);
    // a spark runs along the line under the title
    int lx = 160 - w / 2 - 20, lw = w + 40;
    gfx_fill_mode(lx, y + 21, lw, 1, px_scale(th->accent, 12), DM_ADD);
    int sx = lx + (s_t * 3) % (lw + 60) - 30;
    gfx_glow(sx, y + 21, 5, th->accent);
    ar_battery(SCR_W - 24, 5 - drop);
}

static void draw_names(const theme_t *th, int rise)
{
    // the name under the card follows the carousel, fading between games
    for(int i = 0; i < NUM_GAMES; i++) {
        int off = (i << 8) - s_scroll256;
        if(off <= -256 || off >= 256) continue;
        int k = 32 - iabs(off) * 32 / 100;
        if(k <= 0) continue;
        if(s_t < 30) k = k * s_t / 30;
        int cx = 160 + off * 60 / 256;
        text_center_x(cx, 156 + rise, THEMES[i].name, px_scale(C_WHITE, k), 2, TX_OUTLINE);
        text_center_x(cx, 177 + rise, THEMES[i].tag, px_scale(THEMES[i].accent, k), 1, TX_SHADOW);
    }
    // page dots
    for(int i = 0; i < NUM_GAMES; i++) {
        int x = 160 - (NUM_GAMES - 1) * 7 + i * 14;
        if(i == s_sel) gfx_fill(x - 3, 190 + rise, 7, 3, th->accent);
        else gfx_fill(x - 1, 190 + rise, 3, 3, COL(80, 80, 110));
    }
}

static void draw_buttons(const theme_t *th, int rise)
{
    static const char *const labels[2] = { "SETTINGS", "SYSTEM" };
    for(int i = 0; i < 2; i++) {
        int w = 74, x = 160 - w - 4 + i * (w + 8), y = 201 + rise;
        bool sel = s_state == H_BUTTONS && s_btn == i;
        if(sel) {
            gfx_fill_alpha(x, y, w, 15, px_scale(th->accent, 14), 12);
            ar_neon_rect(x, y, w, 15, th->accent, 2);
        } else {
            gfx_fill_alpha(x, y, w, 15, COL(0, 0, 10), 16);
            gfx_rect(x, y, w, 15, px_scale(th->accent, 12));
        }
        text_center_x(x + w / 2, y + 4, labels[i], sel ? C_WHITE : COL(150, 160, 200), 1, TX_SHADOW);
    }
}

static void draw_footer(int rise)
{
    gfx_fill_mode(0, SCR_H - 12 + rise, SCR_W, 12, 0, DM_SHADOW);
    const char *hint = s_state == H_BUTTONS ? G_LEFT G_RIGHT " CHOOSE   ENTER OPEN   " G_UP " GAMES"
                                            : G_LEFT G_RIGHT " CHOOSE   ENTER PLAY   " G_DOWN " MORE";
    text_center(SCR_H - 10 + rise, hint, COL(120, 120, 160), 1, 0);
    text_draw(4, SCR_H - 10 + rise, "V" QUASAR_VERSION, C_DIM, 1);
}

// ------------------------------------------------------------------ settings

enum { S_BRIGHT, S_SYNC, S_AUTOOFF, S_SHAKE, S_BACK, S_COUNT };

static void settings_update(void)
{
    if(g_in.menu & B_UP) s_set_sel = (s_set_sel + S_COUNT - 1) % S_COUNT;
    if(g_in.menu & B_DOWN) s_set_sel = (s_set_sel + 1) % S_COUNT;
    int d = 0;
    if(g_in.menu & B_LEFT) d = -1;
    if(g_in.menu & B_RIGHT) d = 1;
    if((g_in.pressed & B_A) && s_set_sel != S_BACK) d = 1;
    if(d) {
        savedata_t was = g_save;
        switch(s_set_sel) {
            case S_BRIGHT:
                g_save.brightness = (uint8_t)iclamp(g_save.brightness + d, 0, 4);
                if(g_save.brightness != was.brightness) g_events |= EV_BRIGHT;
                break;
            case S_SYNC:
                g_save.vsync = (uint8_t)((g_save.vsync + 3 + d) % 3);
                g_events |= EV_VSYNC;
                break;
            case S_AUTOOFF:
                g_save.auto_off = (uint8_t)((g_save.auto_off + 3 + d) % 3);
                break;
            case S_SHAKE:
                g_save.shake = (uint8_t)!g_save.shake;
                break;
            default:
                break;
        }
        // these live in QUASAR's save, which its own OPTIONS also change
        if(memcmp(&was, &g_save, sizeof(was))) g_events |= EV_SAVE;
    }
    if(((g_in.pressed & B_A) && s_set_sel == S_BACK) || (g_in.pressed & B_B)) {
        s_state = H_BUTTONS;
    }
}

static void settings_draw(const theme_t *th)
{
    static const char *const labels[S_COUNT] = { "BRIGHTNESS", "SCREEN SYNC", "AUTO OFF", "SCREEN SHAKE", "BACK" };
    static const char *const sync_names[3] = { "L " G_RIGHT " R", "R " G_RIGHT " L", "OFF" };
    static const char *const off_names[3] = { "10 MIN", "30 MIN", "NEVER" };
    static const char *const hints[S_COUNT] = {
        "LOWER LASTS LONGER ON BATTERY",
        "PICK THE ONE WITH NO TEARING",
        "IDLE TIME ON A MENU BEFORE SWITCHING OFF",
        "FOR EXPLOSIONS AND BIG LINE CLEARS",
        "SETTINGS APPLY TO EVERY GAME",
    };
    gfx_fill_alpha(0, 0, SCR_W, SCR_H, COL(0, 0, 8), 18);
    gfx_fill(40, 44, 240, 148, COL(4, 6, 20));
    ar_panel(40, 44, 240, 148, th->accent);
    text_center(34, "SETTINGS", C_WHITE, 2, TX_OUTLINE);
    for(int i = 0; i < S_COUNT; i++) {
        int y = 62 + i * 22;
        bool sel = i == s_set_sel;
        if(sel) gfx_fill_alpha(48, y - 5, 224, 17, px_scale(th->accent, 14), 12);
        text_draw(58, y, labels[i], sel ? C_WHITE : C_GREY, 1);
        char buf[16] = "";
        switch(i) {
            case S_SYNC:    strcpy(buf, sync_names[g_save.vsync % 3]); break;
            case S_AUTOOFF: strcpy(buf, off_names[g_save.auto_off % 3]); break;
            case S_SHAKE:   strcpy(buf, g_save.shake ? "ON" : "OFF"); break;
            default: break;
        }
        if(i == S_BRIGHT) {
            for(int k = 0; k < 5; k++) {
                gfx_fill(186 + k * 15, y - 1 + (4 - k), 11, 5 + k, k <= g_save.brightness ? th->accent : COL(50, 50, 70));
            }
        } else if(buf[0]) {
            if(sel) {
                text_draw(178, y, G_TRI_L, C_DIM, 1);
                text_draw(258, y, G_TRI_R, C_DIM, 1);
            }
            text_center_x(221, y, buf, th->accent, 1, 0);
        }
    }
    text_center(174, hints[s_set_sel], C_DIM, 1, 0);
}

// ------------------------------------------------------------------ launch

static void launch_rect(int *x, int *y, int *w, int *h)
{
    int e = ease(s_launch_t, LAUNCH_ZOOM);
    int cx0 = 160 - CARD_W / 2, cy0 = CARD_Y;
    *x = cx0 - (cx0 * e >> 8);
    *y = cy0 - (cy0 * e >> 8);
    *w = CARD_W + ((SCR_W - CARD_W) * e >> 8);
    *h = CARD_H + ((SCR_H - CARD_H) * e >> 8);
}

static void launch_draw(void)
{
    // the chosen card grows to fill the screen, then melts into the game's colour
    int game = s_sel;
    const theme_t *th = &THEMES[game];
    int e = ease(s_launch_t, LAUNCH_ZOOM);
    int x, y, w, h;
    launch_rect(&x, &y, &w, &h);
    gfx_vgrad(x, y, w, h, th->sky0, th->sky1);
    gfx_glow(x + w / 2, y + h / 2, 30 + w / 5, px_scale(th->glow, 20));
    if(game == GAME_QUASAR) {
        gfx_sprite_rot(&SPR_LOGO, x + w / 2, y + h / 2, 0, 150 + (106 * e >> 8), NULL, DM_NORMAL, 0);
    } else if(game == GAME_TETRIS) {
        int s = 4 + (5 * e >> 8);
        ar_tetris_logo(x + w / 2, y + h / 2 - 5 * s / 2, s, -1);
    } else {
        pac_logo_draw(x + w / 2, y + h / 2, 110 + (146 * e >> 8), 32);
    }
    ar_neon_rect(x, y, w, h, th->accent, 3);
    if(s_launch_t > LAUNCH_ZOOM) {
        int a = (s_launch_t - LAUNCH_ZOOM) * 32 / (LAUNCH_END - LAUNCH_ZOOM);
        gfx_fill_alpha(0, 0, SCR_W, SCR_H, GAME_COL[game], a);
    }
}

bool home_launching(void)
{
    return s_state == H_LAUNCH;
}

// ------------------------------------------------------------------ entry points

void home_enter(int from_game)
{
    s_state = H_CARDS;
    s_sel = from_game >= 0 ? from_game : iclamp(g_arc.last_game, 0, NUM_GAMES - 1);
    s_scroll256 = s_sel << 8;
    s_btn = 0;
    s_bump = 0;
    s_launch_t = 0;
    // the intro only when switching on
    s_t = from_game >= 0 ? 60 : 0;
    if(!s_demo.frames) demo_reset();
}

void home_update(void)
{
    s_t++;
    // the TETRIS and PAC-MAN cards play themselves
    tet_step(&s_demo, tet_ai_keys(&s_demo, &s_demo_ai, 3, false));
    if(s_demo.over || s_demo.lines >= 60) demo_reset();
    if(iabs((GAME_PAC << 8) - s_scroll256) <= 400 || s_state == H_LAUNCH) pac_card_update();

    int target = s_sel << 8;
    s_scroll256 += (target - s_scroll256) / 4;
    if(iabs(target - s_scroll256) < 4) s_scroll256 = target;
    if(s_bump) s_bump = s_bump * -3 / 4;

    switch(s_state) {
        case H_CARDS:
            if(s_t < 20) break;
            if(g_in.menu & B_LEFT) {
                if(s_sel > 0) s_sel--;
                else s_bump = -8;
            }
            if(g_in.menu & B_RIGHT) {
                if(s_sel < NUM_GAMES - 1) s_sel++;
                else s_bump = 8;
            }
            if(g_in.pressed & B_DOWN) s_state = H_BUTTONS;
            if(g_in.pressed & B_A) {
                s_scroll256 = s_sel << 8;
                s_launch_t = 0;
                s_state = H_LAUNCH;
            }
            break;
        case H_BUTTONS:
            if(g_in.menu & (B_LEFT | B_RIGHT)) s_btn ^= 1;
            if(g_in.pressed & (B_UP | B_B)) s_state = H_CARDS;
            if(g_in.pressed & B_A) {
                if(s_btn == 0) {
                    s_set_sel = 0;
                    s_state = H_SETTINGS;
                } else {
                    g_events |= EV_SYSTEM;
                }
            }
            break;
        case H_SETTINGS:
            settings_update();
            break;
        case H_LAUNCH:
            if(++s_launch_t >= LAUNCH_END) arcade_start(s_sel);
            break;
    }
}

void home_draw(void)
{
    // once the chosen card fills the screen, nothing behind it shows
    if(s_state == H_LAUNCH && s_launch_t >= LAUNCH_ZOOM) {
        launch_draw();
        return;
    }
    theme_t th = cur_theme();
    memset(s_cover, 0, sizeof(s_cover));
    if(s_state == H_LAUNCH) {
        int x, y, w, h;
        launch_rect(&x, &y, &w, &h);
        s_cover[0] = x;
        s_cover[1] = y;
        s_cover[2] = x + w;
        s_cover[3] = y + h;
    }
    // switching on: title drops in, the cards rise, the rest follows
    int drop = s_t < 16 ? (16 - s_t) * (16 - s_t) / 6 : 0;
    int rise = s_t < 24 ? ((24 - s_t) * (24 - s_t) * 200) / 576 : 0;
    int rise2 = s_t < 32 ? ((32 - s_t) * (32 - s_t) * 60) / 1024 : 0;
    draw_backdrop(&th);
    draw_cards(&th, rise);
    draw_names(&th, rise2);
    draw_buttons(&th, rise2);
    draw_header(&th, drop);
    draw_footer(rise2);
    if(s_state == H_SETTINGS) settings_draw(&th);
    if(s_state == H_LAUNCH) launch_draw();
}
