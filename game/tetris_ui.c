// TETRIS - the screens: menus, the playfield, and everything that lights up on it.
//
// Controls while playing: LEFT / RIGHT move, DOWN soft drop, ENTER or SPACE
// hard drop, UP or X turn clockwise, Z counter-clockwise, C or SHIFT hold,
// CANCEL / TAB / P / a tap of POWER pause. Holding POWER saves the game.
#include "arcade.h"
#include <string.h>

enum { T_MENU, T_OPTIONS, T_SCORES, T_HOWTO, T_READY, T_PLAY, T_PAUSE, T_CONFIRM, T_OVER, T_NAME };
enum { CF_QUIT, CF_RESTART, CF_NEW };

#define FX      105             // the field: 10 x 20 cells of 11 pixels
#define FY      10
#define CS      11
#define FW      (TB_W * CS)
#define FH      (TB_VIS * CS)

static const char *const MODE_NAMES[TM_COUNT] = { "MARATHON", "SPRINT", "ULTRA" };

static int s_st, s_st_t;
static tgame_t s_g;
static int s_mode;
static int s_menu_sel;
static int s_opt_sel, s_reset_confirm;
static int s_tab;
static int s_pause_sel;
static int s_confirm, s_confirm_sel, s_confirm_back;
static int s_ready_len;
static int s_rank;              // where this game goes in the records, -1 nowhere
static bool s_quit;
static int s_name_len;
static char s_name[NAME_LEN + 1];
static int s_new_rec_mode = -1, s_new_rec_rank = -1;
static bool s_level_dirty;      // the start level changed: saved when leaving the menu

// effects
static uint32_t s_shown_score;
static int s_bump;
static int s_lock_t;
static int8_t s_lock_cells[4][2];
static int s_trail_t, s_trail_type;
static int8_t s_trail_top[4][3];     // x, top row, bottom row for each column the drop swept
static int s_trail_n;
static int s_next_slide, s_hold_flash;
static int s_pop_t;
static char s_pop[3][20];
static px_t s_pop_col;
static uint32_t s_pop_pts;
static int s_banner_t;
static char s_banner[2][16];
static px_t s_banner_col;
static int s_over_t;
static int s_logo_t;

// ------------------------------------------------------------------ colour themes

typedef struct { px_t top, bot, glow, accent; } ttheme_t;

static const ttheme_t THEMES[10] = {
    { COL(3, 8, 28), COL(8, 36, 72), COL(30, 120, 230), COL(90, 220, 255) },
    { COL(12, 4, 30), COL(46, 14, 82), COL(140, 60, 240), COL(205, 140, 255) },
    { COL(22, 2, 20), COL(76, 10, 60), COL(230, 50, 160), COL(255, 120, 205) },
    { COL(24, 6, 6), COL(86, 28, 10), COL(240, 110, 30), COL(255, 180, 90) },
    { COL(2, 18, 14), COL(8, 60, 42), COL(30, 200, 120), COL(120, 255, 180) },
    { COL(20, 14, 2), COL(72, 50, 8), COL(230, 180, 30), COL(255, 230, 120) },
    { COL(24, 2, 8), COL(80, 8, 22), COL(240, 40, 70), COL(255, 120, 130) },
    { COL(2, 20, 24), COL(6, 62, 70), COL(30, 210, 210), COL(140, 255, 240) },
    { COL(10, 6, 30), COL(38, 20, 90), COL(110, 90, 250), COL(255, 130, 225) },
    { COL(8, 12, 24), COL(40, 54, 80), COL(150, 190, 240), COL(235, 245, 255) },
};

static ttheme_t s_th;               // what is on screen
static int s_th_from, s_th_to, s_th_k = 32;

static void theme_set(int i, bool now)
{
    i %= 10;
    if(i == s_th_to && !now) return;
    s_th_from = now ? i : s_th_to;
    s_th_to = i;
    s_th_k = now ? 32 : 0;
}

static void theme_step(void)
{
    if(s_th_k < 32) s_th_k++;
    const ttheme_t *a = &THEMES[s_th_from], *b = &THEMES[s_th_to];
    s_th.top = px_mix(b->top, a->top, s_th_k);
    s_th.bot = px_mix(b->bot, a->bot, s_th_k);
    s_th.glow = px_mix(b->glow, a->glow, s_th_k);
    s_th.accent = px_mix(b->accent, a->accent, s_th_k);
}

static int game_theme(void)
{
    return s_g.mode == TM_MARATHON ? s_g.level - 1 : s_g.lines / 10 + (s_g.mode == TM_ULTRA ? 3 : 0);
}

// ------------------------------------------------------------------ small helpers

static void level_flush(void)
{
    // one write for a run of start level changes, not one per step
    if(s_level_dirty) {
        s_level_dirty = false;
        g_events |= EV_SAVE_ARCADE;
    }
}

static void set_state(int st)
{
    if(st != T_MENU) level_flush();
    s_st = st;
    s_st_t = 0;
}

static void fmt_time(char *buf, uint32_t frames, bool cs)
{
    uint32_t c = (uint32_t)((uint64_t)frames * 100 / FPS);
    uint32_t m = c / 6000, s = (c / 100) % 60;
    char t[8];
    fmt_int(buf, (long)m);
    str_cat(buf, ":");
    fmt_score(t, s, 2);
    str_cat(buf, t);
    if(cs) {
        str_cat(buf, ".");
        fmt_score(t, c % 100, 2);
        str_cat(buf, t);
    }
}

static void apply_repeat(tgame_t *g)
{
    static const uint8_t DAS[4] = { 7, 5, 3, 3 }, ARR[4] = { 2, 1, 1, 0 };
    int k = g_arc.das > 3 ? 1 : g_arc.das;
    g->das = DAS[k];
    g->arr = ARR[k];
}

static void fx_reset_all(void)
{
    fx_reset();
    s_bump = s_lock_t = s_trail_t = s_next_slide = s_hold_flash = 0;
    s_pop_t = s_banner_t = 0;
}

static void banner(const char *a, const char *b, px_t col, int frames)
{
    strcpy(s_banner[0], a);
    strcpy(s_banner[1], b ? b : "");
    s_banner_col = col;
    s_banner_t = frames;
}

// ------------------------------------------------------------------ starting and ending

static void start_game(int mode)
{
    s_mode = mode;
    tet_new(&s_g, mode, g_arc.start_level, rng_next(&g_fxrng) ^ (plat_millis() * 2654435761u));
    apply_repeat(&s_g);
    s_shown_score = 0;
    s_quit = false;
    s_rank = -1;
    fx_reset_all();
    theme_set(game_theme(), false);
    s_ready_len = 48;
    set_state(T_READY);
}

static bool suspend_game(void)
{
    // keep the game to CONTINUE: finish a line clear first so a piece is in play.
    // That can end it (no room for the next piece, or the sprint's last line): false then.
    tet_settle(&s_g);
    if(s_g.over) return false;
    g_arc.suspended = tet_pack(&s_g, g_arc.susp, TET_PACK_LEN) == TET_PACK_LEN;
    g_events |= EV_SAVE_ARCADE;
    return true;
}

static bool resume_game(void)
{
    if(!g_arc.suspended || !tet_unpack(&s_g, g_arc.susp, TET_PACK_LEN)) {
        g_arc.suspended = false;
        g_events |= EV_SAVE_ARCADE;
        return false;
    }
    s_mode = s_g.mode;
    apply_repeat(&s_g);
    s_shown_score = s_g.score;
    s_quit = false;
    s_rank = -1;
    fx_reset_all();
    theme_set(game_theme(), true);
    s_ready_len = 48;
    set_state(T_READY);
    return true;
}

static void game_ended(void)
{
    // the saved copy is for carrying on, and there is nothing to carry on with
    g_arc.suspended = false;
    g_arc.plays++;
    g_arc.lines += s_g.lines;
    g_events |= EV_SAVE_ARCADE;
    uint32_t v = 0;
    if(s_mode == TM_MARATHON) v = s_g.score;
    else if(s_g.won && !s_quit) v = s_mode == TM_SPRINT ? s_g.frames : s_g.score;
    s_rank = trec_rank(s_mode, v);
    s_over_t = 0;
    set_state(T_OVER);
}

static uint32_t record_value(void)
{
    return s_mode == TM_SPRINT ? s_g.frames : s_g.score;
}

static void commit_record(const char *name)
{
    if(s_rank < 0) return;
    trec_insert(s_mode, s_rank, name, record_value(), s_g.lines, s_g.level);
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
    if(r & KEYBIT(K_LEFT)) k |= TK_LEFT;
    if(r & KEYBIT(K_RIGHT)) k |= TK_RIGHT;
    if(r & KEYBIT(K_DOWN)) k |= TK_SOFT;
    if(r & KEYBIT(K_UP)) k |= g_arc.upkey ? TK_HARD : TK_CW;
    if(r & KEYBIT(K_X)) k |= TK_CW;
    if(r & KEYBIT(K_Z)) k |= TK_CCW;
    if(r & (KEYBIT(K_ENTER) | KEYBIT(K_SPACE))) k |= TK_HARD;
    if(r & (KEYBIT(K_C) | KEYBIT(K_SHIFT))) k |= TK_HOLD;
    return k;
}

static int cell_sx(int x) { return FX + x * CS + g_shake_x; }
static int cell_sy(int y) { return FY + (y - TB_HIDDEN) * CS + s_bump + g_shake_y; }

static void on_clear(void)
{
    static const char *const PLAIN[5] = { "", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS" };
    int n = s_g.lk_lines, spin = s_g.lk_spin;
    bool tetris = n == 4;

    // sparks fly out of every cleared cell
    for(int i = 0; i < s_g.clear_n; i++) {
        int y = s_g.clear_row[i];
        for(int x = 0; x < TB_W; x++) {
            px_t c = PIECE_COL[s_g.cell[y][x] ? s_g.cell[y][x] : PC_GREY];
            float cx = (float)(cell_sx(x) + CS / 2), cy = (float)(cell_sy(y) + CS / 2);
            float dir = (float)(x - 4) - 0.5f;
            for(int k = 0; k < (tetris ? 3 : 2); k++) {
                fx_spark(cx, cy, dir * rndfxr(0.3f, 0.9f) + rndfxr(-1.0f, 1.0f), rndfxr(-3.0f, 1.0f), c, 10 + rndfx(12));
            }
            if(tetris || spin) fx_ember(cx, cy, rndfxr(-1.5f, 1.5f), rndfxr(-2.5f, -0.5f), c, 24 + rndfx(20), 2);
        }
    }
    float mx = (float)(FX + FW / 2), my = (float)(cell_sy(s_g.clear_row[0]) + n * CS / 2);

    s_pop[0][0] = s_pop[1][0] = s_pop[2][0] = 0;
    if(spin) {
        strcpy(s_pop[0], spin == SPIN_MINI ? "MINI T-SPIN" : "T-SPIN");
        strcpy(s_pop[1], PLAIN[n]);
        s_pop_col = PIECE_COL[PC_T];
        fx_ring(mx, my, 6, 4.0f, PIECE_COL[PC_T], 16, 3);
    } else {
        strcpy(s_pop[0], PLAIN[n]);
        s_pop_col = tetris ? PIECE_COL[PC_I] : C_WHITE;
    }
    if(s_g.lk_b2b) strcpy(s_pop[2], "B2B ");
    if(s_g.lk_combo > 0) {
        fmt_int(s_pop[2] + strlen(s_pop[2]), s_g.lk_combo);
        str_cat(s_pop[2], " COMBO");
    }
    s_pop_pts = s_g.lk_points;
    // a plain single says nothing unless something rides on it
    if(n > 1 || spin || s_pop[2][0]) s_pop_t = 75;

    if(tetris) {
        banner("TETRIS", NULL, PIECE_COL[PC_I], 40);
        fx_ring(mx, my, 8, 6.0f, COL(120, 230, 255), 20, 4);
        fx_ring(mx, my, 4, 3.5f, C_WHITE, 14, 2);
        fx_flash(px_scale(s_th.glow, 16), 6);
        fx_shake(4);
    } else if(spin && n) {
        fx_flash(COL(60, 20, 90), 5);
        fx_shake(2);
    } else if(n >= 2) {
        fx_shake(1);
    }
    if(s_g.lk_pc) {
        banner("PERFECT", "CLEAR", C_GOLD, 60);
        fx_ring(mx, (float)(FY + FH / 2), 10, 7.0f, C_GOLD, 24, 4);
        fx_flash(COL(90, 70, 20), 10);
    }
}

static void on_events(uint32_t ev)
{
    if(ev & TE_LOCK) {
        const int8_t (*c)[2] = TET_SHAPE[s_g.lk_type][s_g.lk_rot];
        for(int i = 0; i < 4; i++) {
            s_lock_cells[i][0] = (int8_t)(s_g.lk_x + c[i][0]);
            s_lock_cells[i][1] = (int8_t)(s_g.lk_y + c[i][1]);
        }
        s_lock_t = 6;
    }
    if(ev & TE_HARDDROP) {
        // light trails down each column the piece swept, dust where it landed
        const int8_t (*c)[2] = TET_SHAPE[s_g.lk_type][s_g.lk_rot];
        s_trail_n = 0;
        for(int i = 0; i < 4; i++) {
            int x = s_g.lk_x + c[i][0];
            int k;
            for(k = 0; k < s_trail_n && s_trail_top[k][0] != x; k++) {
            }
            int top = s_g.hd_y + c[i][1], bot = s_g.lk_y + c[i][1];
            if(k == s_trail_n) {
                s_trail_top[k][0] = (int8_t)x;
                s_trail_top[k][1] = (int8_t)top;
                s_trail_top[k][2] = (int8_t)bot;
                s_trail_n++;
            } else {
                if(top < s_trail_top[k][1]) s_trail_top[k][1] = (int8_t)top;
                if(bot > s_trail_top[k][2]) s_trail_top[k][2] = (int8_t)bot;
            }
        }
        s_trail_type = s_g.lk_type;
        s_trail_t = s_g.lk_y > s_g.hd_y ? 8 : 0;
        s_bump = s_g.lk_y - s_g.hd_y > 2 ? 3 : 1;
        for(int k = 0; k < s_trail_n; k++) {
            float x = (float)(cell_sx(s_trail_top[k][0]) + CS / 2), y = (float)(cell_sy(s_trail_top[k][2]) + CS);
            for(int j = 0; j < 3; j++) {
                fx_spark(x, y, rndfxr(-2.5f, 2.5f), rndfxr(-1.6f, -0.2f), COL(200, 220, 255), 6 + rndfx(6));
            }
        }
    }
    if(ev & TE_HOLD) s_hold_flash = 8;
    if(ev & TE_SPAWN) s_next_slide = 5;
    if(ev & TE_CLEAR) on_clear();
    if(ev & TE_LOCK && !(ev & TE_CLEAR) && s_g.lk_spin) {
        strcpy(s_pop[0], s_g.lk_spin == SPIN_MINI ? "MINI T-SPIN" : "T-SPIN");
        s_pop[1][0] = s_pop[2][0] = 0;
        s_pop_col = PIECE_COL[PC_T];
        s_pop_pts = s_g.lk_points;
        s_pop_t = 60;
    }
    if(ev & TE_LEVELUP) {
        char buf[16] = "LEVEL ";
        fmt_int(buf + 6, s_g.level);
        if(!s_g.lk_pc && s_g.lk_lines < 4) banner(buf, NULL, s_th.accent, 50);
        fx_ring((float)(FX + FW / 2), (float)(FY + FH / 2), 10, 5.0f, s_th.accent, 20, 3);
    }
    if(ev & (TE_LEVELUP | TE_CLEAR)) theme_set(game_theme(), false);
}

static void play_update(void)
{
    if((g_in.pressed & (B_PAUSE | B_B))) {
        s_pause_sel = 0;
        set_state(T_PAUSE);
        return;
    }
    uint32_t ev = tet_step(&s_g, play_keys());
    on_events(ev);
    if(s_g.over) {
        if(ev & TE_GOAL) {
            if(s_mode == TM_SPRINT) {
                banner("FINISH", NULL, C_GOLD, 60);
                for(int i = 0; i < 4; i++) {
                    fx_ring((float)(FX + 20 + rndfx(FW - 40)), (float)(FY + 30 + rndfx(FH - 60)), 4, 3.0f + (float)i,
                            PIECE_COL[PC_I + rndfx(7)], 20 + i * 4, 2);
                }
                fx_flash(COL(80, 70, 30), 10);
            } else {
                banner("TIME UP", NULL, C_GOLD, 60);
                fx_flash(COL(60, 40, 20), 8);
            }
        }
        game_ended();
    }
}

// ------------------------------------------------------------------ drawing the game

static void darken(int x, int y, int w, int h, int k32)
{
    // one pass over the rectangle: every pixel scaled to k32/32 (glass over the backdrop)
    int x1 = x + w, y1 = y + h;
    if(x < g_clip.x0) x = g_clip.x0;
    if(y < g_clip.y0) y = g_clip.y0;
    if(x1 > g_clip.x1) x1 = g_clip.x1;
    if(y1 > g_clip.y1) y1 = g_clip.y1;
    while(x < x1) {
        int se = ((x >> STRIP_SHIFT) + 1) << STRIP_SHIFT;
        if(se > x1) se = x1;
        int n = se - x;
        px_t *row = &g_fb[FBI(x, y)];
        for(int j = y; j < y1; j++, row += STRIP_W) {
            for(int i = 0; i < n; i++) row[i] = px_scale(row[i], k32);
        }
        x = se;
    }
}

static void draw_bg(void)
{
    gfx_vgrad(0, 0, SCR_W, SCR_H, s_th.top, s_th.bot);
    int t = (int)g_arc_t;
    // slow lights drifting behind everything
    for(int i = 0; i < 2; i++) {
        int x = 160 + ((isin256(t * (1 + i) / 2 + i * 85) * 150) >> 14);
        int y = 120 + ((icos256(t * (2 + i) / 3 + i * 50) * 100) >> 14);
        gfx_glow(x, y, 58 + i * 8, px_scale(s_th.glow, 6));
    }
    // motes rising
    for(int i = 0; i < 28; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        int sp = 1 + (int)((h >> 5) & 3);
        int x = (int)(h % SCR_W) + ((isin256(t * 2 + (int)(h >> 7)) * 5) >> 14);
        int y = SCR_H - (int)(((uint32_t)t * (uint32_t)sp / 2 + (h >> 11)) % (SCR_H + 20)) + 10;
        int k = 8 + ((isin256(t * 5 + (int)(h >> 3)) * 7) >> 14);
        gfx_padd(x, y, px_scale(s_th.accent, k));
        if((h & 7) == 0) gfx_padd(x, y + 1, px_scale(s_th.accent, k / 2));
    }
}

static bool row_clearing(int y)
{
    if(s_g.active || !s_g.clear_n) return false;
    for(int i = 0; i < s_g.clear_n; i++) {
        if(s_g.clear_row[i] == y) return true;
    }
    return false;
}

static void draw_field(bool hide)
{
    int x0 = FX + g_shake_x, y0 = FY + s_bump + g_shake_y;
    int h = tet_stack_height(&s_g);
    bool danger = h >= TB_VIS - 3 && !hide && s_st != T_OVER;
    px_t edge = danger ? ((g_arc_t & 4) ? C_RED : C_WHITE) : s_th.accent;

    darken(x0, y0, FW, FH, 9);
    if(danger) ar_add_vgrad(x0, y0, FW, 44, (g_arc_t & 8) ? COL(90, 0, 16) : COL(50, 0, 8), 0);
    if(g_arc.grid) {
        px_t gc = px_scale(s_th.accent, 3);
        for(int c = 1; c < TB_W; c++) gfx_fill_mode(x0 + c * CS, y0, 1, FH, gc, DM_ADD);
        for(int r = 1; r < TB_VIS; r++) gfx_fill_mode(x0, y0 + r * CS, FW, 1, gc, DM_ADD);
    }
    ar_neon_rect(x0 - 2, y0 - 2, FW + 4, FH + 4, edge, danger ? 4 : 3);
    gfx_rect(x0 - 1, y0 - 1, FW + 2, FH + 2, px_scale(edge, 10));
    if(hide) return;

    // everything in the well stays inside it: pieces slide in from behind the top edge
    gfx_clip(x0, y0, x0 + FW, y0 + FH);

    // the stack; rows being cleared flash, then vanish from the middle out
    int phase = CLEAR_FRAMES - s_g.wait;
    for(int y = TB_HIDDEN - 1; y < TB_H; y++) {
        int sy = cell_sy(y);
        bool clearing = row_clearing(y);
        for(int x = 0; x < TB_W; x++) {
            int t = s_g.cell[y][x];
            if(!t) continue;
            int sx = cell_sx(x);
            if(clearing) {
                if(phase < 3) {
                    gfx_fill(sx, sy, CS, CS, C_WHITE);
                } else {
                    int d = x < 5 ? 4 - x : x - 5;          // 0 in the middle, 4 at the walls
                    int gone = (phase - 3) * 5 / (CLEAR_FRAMES - 3);
                    if(d >= gone) {
                        ar_cell(sx, sy, CS, t);
                        gfx_fill_mode(sx, sy, CS, CS, px_scale(C_WHITE, 20 - (phase - 3) * 2), DM_ADD);
                    }
                }
                continue;
            }
            ar_cell(sx, sy, CS, t);
        }
    }

    if(s_g.active) {
        const int8_t (*c)[2] = TET_SHAPE[s_g.type][s_g.rot];
        if(g_arc.ghost) {
            int gy = tet_ghost_y(&s_g);
            for(int i = 0; i < 4; i++) {
                int y = gy + c[i][1];
                if(y >= TB_HIDDEN) ar_ghost_cell(cell_sx(s_g.x + c[i][0]), cell_sy(y), CS, s_g.type);
            }
        }
        // resting on something: it glows brighter as the lock delay runs out
        int glow = s_g.lock_t ? s_g.lock_t * 12 / LOCK_DELAY : 0;
        for(int i = 0; i < 4; i++) {
            int sx = cell_sx(s_g.x + c[i][0]), sy = cell_sy(s_g.y + c[i][1]);
            ar_cell(sx, sy, CS, s_g.type);
            if(glow) gfx_fill_mode(sx, sy, CS, CS, px_scale(C_WHITE, glow), DM_ADD);
        }
    }

    // hard drop trails
    if(s_trail_t > 0) {
        px_t c = PIECE_COL[s_trail_type];
        for(int k = 0; k < s_trail_n; k++) {
            int sx = cell_sx(s_trail_top[k][0]);
            int top = cell_sy(s_trail_top[k][1]), bot = cell_sy(s_trail_top[k][2]);
            if(top < y0) top = y0;
            if(bot > top) ar_add_vgrad(sx + 1, top, CS - 2, bot - top, 0, px_scale(c, s_trail_t * 2));
        }
    }
    // the piece that just locked flashes
    if(s_lock_t > 0) {
        for(int i = 0; i < 4; i++) {
            int y = s_lock_cells[i][1];
            if(y < TB_HIDDEN || row_clearing(y)) continue;
            gfx_fill_mode(cell_sx(s_lock_cells[i][0]), cell_sy(y), CS, CS, px_scale(C_WHITE, s_lock_t * 3), DM_ADD);
        }
    }
    gfx_noclip();
}

static void box(int x, int y, int w, int h, bool flash)
{
    darken(x, y, w, h, 12);
    ar_neon_rect(x, y, w, h, flash ? C_WHITE : px_scale(s_th.accent, 22), flash ? 2 : 1);
}

static void label(int x, int y, const char *s)
{
    text_draw(x, y, s, px_scale(s_th.accent, 24), 1);
}

static void big_value(int x, int y, const char *s, int maxw)
{
    int sc = text_width(s, 2) <= maxw ? 2 : 1;
    text_draw_fx(x, y + (sc == 1 ? 4 : 0), s, C_WHITE, sc, TX_SHADOW, NULL);
}

static void draw_hud(bool hide)
{
    char buf[24];
    // HOLD
    box(14, 12, 78, 48, s_hold_flash > 0);
    label(19, 15, "HOLD");
    if(s_g.hold && !hide) {
        ar_piece(s_g.hold, 53, 40, 9);
        if(s_g.hold_used) gfx_fill_mode(15, 24, 76, 35, 0, DM_SHADOW);
    }
    // NEXT, the first bigger
    int slide = s_next_slide * 5;
    box(228, 12, 78, 48, false);
    label(233, 15, "NEXT");
    box(228, 64, 78, 122, false);
    if(!hide) {
        gfx_clip(229, 23, 305, 59);
        ar_piece(s_g.next[0], 267, 41 + slide, 9);
        gfx_clip(229, 65, 305, 185);
        for(int i = 1; i < NEXT_N; i++) ar_piece(s_g.next[i], 267, 80 + (i - 1) * 29 + slide, 7);
        gfx_noclip();
    }

    // score and progress
    fmt_commas(buf, s_shown_score);
    label(16, 68, "SCORE");
    big_value(16, 78, buf, 88);

    if(s_mode == TM_MARATHON) {
        label(16, 101, "LEVEL");
        fmt_int(buf, s_g.level);
        big_value(16, 111, buf, 88);
        label(16, 134, "LINES");
        fmt_int(buf, s_g.lines);
        big_value(16, 144, buf, 88);
        label(16, 167, "TIME");
        fmt_time(buf, s_g.frames, false);
        text_draw(52, 167, buf, C_WHITE, 1);
    } else if(s_mode == TM_SPRINT) {
        label(16, 101, "LINES LEFT");
        fmt_int(buf, s_g.lines >= SPRINT_LINES ? 0 : SPRINT_LINES - s_g.lines);
        big_value(16, 111, buf, 88);
        label(16, 134, "TIME");
        fmt_time(buf, s_g.frames, true);
        big_value(16, 144, buf, 88);
    } else {
        label(16, 101, "TIME LEFT");
        uint32_t left = s_g.frames >= ULTRA_FRAMES ? 0 : ULTRA_FRAMES - s_g.frames;
        fmt_time(buf, left, true);
        text_draw_fx(16, 111, buf, left < 10 * FPS && (g_arc_t & 8) ? C_RED : C_WHITE, 2, TX_SHADOW, NULL);
        label(16, 134, "LINES");
        fmt_int(buf, s_g.lines);
        big_value(16, 144, buf, 88);
    }

    // stats on the right
    int y = 194;
    label(232, y, MODE_NAMES[s_mode]);
    y += 12;
    uint32_t secs100 = s_g.frames ? (uint32_t)((uint64_t)s_g.pieces * FPS * 100 / s_g.frames) : 0;
    strcpy(buf, "PPS ");
    fmt_int(buf + 4, (long)(secs100 / 100));
    str_cat(buf, ".");
    fmt_score(buf + strlen(buf), secs100 % 100, 2);
    text_draw(232, y, buf, C_GREY, 1);
    y += 11;
    strcpy(buf, "TETRIS ");
    fmt_int(buf + 7, s_g.n_tetris);
    text_draw(232, y, buf, C_GREY, 1);
    y += 11;
    strcpy(buf, "T-SPIN ");
    fmt_int(buf + 7, s_g.n_tspin);
    text_draw(232, y, buf, C_GREY, 1);

    // what the last clear was worth
    if(s_pop_t > 0 && !hide) {
        int age = 75 - s_pop_t;
        int k = s_pop_t < 12 ? s_pop_t * 32 / 12 : 32;
        int ox = age < 6 ? -(6 - age) * (6 - age) * 2 : 0;
        int py = 194;
        px_t main = s_pop_col;
        if(!strcmp(s_pop[0], "TETRIS")) main = PIECE_COL[PC_I + ((g_arc_t >> 1) % 7)];
        int sc = text_width(s_pop[0], 2) <= 88 ? 2 : 1;
        text_draw_fx(16 + ox, py - (sc == 2 ? 4 : 0), s_pop[0], px_scale(main, k), sc, TX_OUTLINE, NULL);
        if(s_pop[1][0]) text_draw_fx(16 + ox, py + 14, s_pop[1], px_scale(C_WHITE, k), 1, TX_SHADOW, NULL);
        if(s_pop[2][0]) text_draw_fx(16 + ox, py + 25, s_pop[2], px_scale(C_GOLD, k), 1, TX_SHADOW, NULL);
        if(s_pop_pts && age > 4) {
            strcpy(buf, "+");
            fmt_commas(buf + 1, s_pop_pts);
            text_right(98 + ox, py + 14, buf, px_scale(C_GREEN, k), 1, TX_SHADOW);
        }
    }
}

static void draw_banner(void)
{
    if(s_banner_t <= 0) return;
    int k = s_banner_t < 10 ? s_banner_t * 32 / 10 : 32;
    int two = s_banner[1][0] != 0;
    int cy = FY + FH / 2 - (two ? 18 : 9);
    px_t c = s_banner_col;
    if(!strcmp(s_banner[0], "TETRIS")) c = PIECE_COL[PC_I + ((g_arc_t >> 1) % 7)];
    // a band just wide enough for the words, never less than the field
    int w = text_width(s_banner[0], 3);
    if(two && text_width(s_banner[1], 3) > w) w = text_width(s_banner[1], 3);
    w = w + 20 > FW + 10 ? w + 20 : FW + 10;
    int bx = 160 - w / 2;
    gfx_fill_alpha(bx, cy - 8, w, two ? 52 : 34, COL(0, 0, 10), k / 2);
    gfx_fill_mode(bx, cy - 9, w, 1, px_scale(c, k / 2), DM_ADD);
    gfx_fill_mode(bx, cy + (two ? 44 : 26), w, 1, px_scale(c, k / 2), DM_ADD);
    text_center(cy, s_banner[0], px_scale(c, k), 3, TX_OUTLINE);
    if(two) text_center(cy + 24, s_banner[1], px_scale(c, k), 3, TX_OUTLINE);
}

static void effects_tick(void)
{
    fx_update();
    if(s_bump > 0) s_bump = s_bump * 2 / 3;
    if(s_lock_t > 0) s_lock_t--;
    if(s_trail_t > 0) s_trail_t--;
    if(s_next_slide > 0) s_next_slide--;
    if(s_hold_flash > 0) s_hold_flash--;
    if(s_pop_t > 0) s_pop_t--;
    if(s_banner_t > 0) s_banner_t--;
    uint32_t target = s_g.score;
    if(s_shown_score < target) s_shown_score += (target - s_shown_score + 5) / 6;
    else s_shown_score = target;
}

// ------------------------------------------------------------------ menu

enum { MN_CONTINUE, MN_MARATHON, MN_SPRINT, MN_ULTRA, MN_SCORES, MN_OPTIONS, MN_HOWTO, MN_HOME, MN_COUNT };

typedef struct { int16_t x, y; uint8_t type, s, speed; } fall_t;
static fall_t s_fall[9];

static bool menu_enabled(int i)
{
    return i != MN_CONTINUE || g_arc.suspended;
}

static void menu_fix(void)
{
    while(!menu_enabled(s_menu_sel)) s_menu_sel = (s_menu_sel + 1) % MN_COUNT;
}

static void falling_reset(fall_t *f, bool anywhere)
{
    f->type = (uint8_t)(PC_I + rndfx(7));
    f->s = (uint8_t)(7 + rndfx(6));
    f->speed = (uint8_t)(1 + rndfx(3));
    f->x = (int16_t)(10 + rndfx(SCR_W - 20));
    f->y = (int16_t)(anywhere ? rndfx(SCR_H) * 4 : -40 * 4);
}

static void menu_update(void)
{
    menu_fix();
    if(g_in.menu & B_UP) {
        do { s_menu_sel = (s_menu_sel + MN_COUNT - 1) % MN_COUNT; } while(!menu_enabled(s_menu_sel));
    }
    if(g_in.menu & B_DOWN) {
        do { s_menu_sel = (s_menu_sel + 1) % MN_COUNT; } while(!menu_enabled(s_menu_sel));
    }
    if(s_menu_sel == MN_MARATHON && (g_in.menu & (B_LEFT | B_RIGHT))) {
        int lv = g_arc.start_level + ((g_in.menu & B_RIGHT) ? 1 : -1);
        if(lv >= 1 && lv <= 15) {
            g_arc.start_level = (uint8_t)lv;
            s_level_dirty = true;
        }
    }
    // the menu shows off the colours
    if(s_st_t % 240 == 0) theme_set(s_st_t / 240, false);

    if(g_in.pressed & B_B) {
        level_flush();
        arcade_go_home(GAME_TETRIS);
        return;
    }
    if(!(g_in.pressed & B_A)) return;
    switch(s_menu_sel) {
        case MN_CONTINUE:
            resume_game();
            break;
        case MN_MARATHON:
        case MN_SPRINT:
        case MN_ULTRA:
            if(g_arc.suspended) {
                s_confirm = CF_NEW;
                s_confirm_sel = 1;
                s_confirm_back = T_MENU;
                s_mode = s_menu_sel - MN_MARATHON;
                set_state(T_CONFIRM);
            } else {
                start_game(s_menu_sel - MN_MARATHON);
            }
            break;
        case MN_SCORES:
            s_tab = TM_MARATHON;
            s_new_rec_rank = -1;
            set_state(T_SCORES);
            break;
        case MN_OPTIONS:
            s_opt_sel = 0;
            s_reset_confirm = 0;
            set_state(T_OPTIONS);
            break;
        case MN_HOWTO:
            set_state(T_HOWTO);
            break;
        case MN_HOME:
            level_flush();
            arcade_go_home(GAME_TETRIS);
            break;
    }
}

static void soft_piece(int type, int cx, int cy, int s)
{
    // a see-through piece for the backdrop
    const int8_t (*c)[2] = TET_SHAPE[type][0];
    int ox = cx - 2 * s, oy = cy - s;
    for(int i = 0; i < 4; i++) {
        int x = ox + c[i][0] * s, y = oy + c[i][1] * s;
        gfx_fill_mode(x, y, s - 1, s - 1, px_scale(PIECE_COL[type], 12), DM_ADD);
        gfx_fill_mode(x, y, s - 1, 1, px_scale(PIECE_COL[type], 10), DM_ADD);
    }
}

static void menu_bg_draw(void)
{
    draw_bg();
    for(int i = 0; i < 9; i++) {
        fall_t *f = &s_fall[i];
        f->y = (int16_t)(f->y + f->speed);
        if(f->y > (SCR_H + 40) * 4) falling_reset(f, false);
        soft_piece(f->type, f->x, f->y / 4, f->s);
    }
}

static void menu_draw(void)
{
    char buf[40];
    menu_bg_draw();
    ar_tetris_logo(160, 14, 9, s_logo_t);
    static const char *const labels[MN_COUNT] = { "CONTINUE", "MARATHON", "SPRINT 40 LINES", "ULTRA 2 MINUTES",
                                                  "HIGH SCORES", "OPTIONS", "HOW TO PLAY", "HOME" };
    int y = 76;
    for(int i = 0; i < MN_COUNT; i++) {
        if(!menu_enabled(i)) continue;
        const char *s = labels[i];
        if(i == MN_CONTINUE) {
            tgame_t peek;
            if(tet_unpack(&peek, g_arc.susp, TET_PACK_LEN)) {
                strcpy(buf, "CONTINUE ");
                str_cat(buf, MODE_NAMES[peek.mode]);
                if(peek.mode == TM_MARATHON) {
                    str_cat(buf, " LV ");
                    fmt_int(buf + strlen(buf), peek.level);
                }
                s = buf;
            }
        } else if(i == MN_MARATHON && s_menu_sel == MN_MARATHON) {
            strcpy(buf, "MARATHON  " G_LEFT " LEVEL ");
            fmt_int(buf + strlen(buf), g_arc.start_level);
            str_cat(buf, " " G_RIGHT);
            s = buf;
        }
        ar_menu_item(160, y, s, i == s_menu_sel, true, s_th.accent);
        y += 17;
    }
    // the best for the selected mode
    int m = s_menu_sel >= MN_MARATHON && s_menu_sel <= MN_ULTRA ? s_menu_sel - MN_MARATHON : -1;
    gfx_fill_mode(0, SCR_H - 13, SCR_W, 13, 0, DM_SHADOW);
    if(m >= 0) {
        const trec_t *r = &g_arc.rec[m][0];
        strcpy(buf, "BEST  ");
        if(m == TM_SPRINT) fmt_time(buf + strlen(buf), r->value, true);
        else fmt_commas(buf + strlen(buf), r->value);
        str_cat(buf, "  ");
        str_cat(buf, r->name);
        text_center(SCR_H - 10, buf, C_GOLD, 1, 0);
    } else {
        text_center(SCR_H - 10, G_UP G_DOWN " SELECT   ENTER OK   CANCEL HOME", COL(120, 120, 160), 1, 0);
    }
    ar_battery(SCR_W - 22, 4);
}

// ------------------------------------------------------------------ options

enum { O_GHOST, O_GRID, O_REPEAT, O_UPKEY, O_RESET, O_BACK, O_COUNT };

static void options_update(void)
{
    if(g_in.menu & B_UP) s_opt_sel = (s_opt_sel + O_COUNT - 1) % O_COUNT;
    if(g_in.menu & B_DOWN) s_opt_sel = (s_opt_sel + 1) % O_COUNT;
    int d = 0;
    if(g_in.menu & B_LEFT) d = -1;
    if(g_in.menu & B_RIGHT) d = 1;
    if((g_in.pressed & B_A) && s_opt_sel < O_RESET) d = 1;
    if(d) {
        switch(s_opt_sel) {
            case O_GHOST:  g_arc.ghost ^= 1; break;
            case O_GRID:   g_arc.grid ^= 1; break;
            case O_REPEAT: g_arc.das = (uint8_t)((g_arc.das + 4 + d) % 4); break;
            case O_UPKEY:  g_arc.upkey ^= 1; break;
            default: break;
        }
        if(s_opt_sel < O_RESET) g_events |= EV_SAVE_ARCADE;
    }
    if(g_in.pressed & B_A) {
        if(s_opt_sel == O_RESET) {
            if(++s_reset_confirm >= 2) {
                arcsave_t keep = g_arc;
                arcsave_defaults();
                memcpy(keep.rec, g_arc.rec, sizeof(keep.rec));
                g_arc = keep;
                s_reset_confirm = 0;
                g_events |= EV_SAVE_ARCADE;
                fx_flash(C_RED, 6);
            }
        } else if(s_opt_sel == O_BACK) {
            set_state(T_MENU);
        }
    }
    if(s_opt_sel != O_RESET) s_reset_confirm = 0;
    if(g_in.pressed & B_B) set_state(T_MENU);
}

static void options_draw(void)
{
    static const char *const labels[O_COUNT] = { "GHOST PIECE", "GRID", "KEY REPEAT", "UP KEY", "RESET RECORDS", "BACK" };
    static const char *const repeat[4] = { "SLOW", "NORMAL", "FAST", "INSTANT" };
    static const char *const hints[O_COUNT] = {
        "SHOWS WHERE THE PIECE WILL LAND",
        "FAINT LINES ACROSS THE FIELD",
        "HOW FAST A HELD LEFT OR RIGHT SLIDES",
        "WHAT THE UP ARROW DOES",
        "CLEARS ALL THREE RECORD TABLES",
        "CHANGES ARE SAVED AS YOU MAKE THEM",
    };
    menu_bg_draw();
    ar_panel(36, 42, 248, 156, s_th.accent);
    text_center(30, "OPTIONS", C_WHITE, 2, TX_OUTLINE);
    for(int i = 0; i < O_COUNT; i++) {
        int y = 58 + i * 21;
        bool sel = i == s_opt_sel;
        if(sel) gfx_fill_alpha(44, y - 5, 232, 17, px_scale(s_th.accent, 14), 12);
        text_draw(54, y, labels[i], sel ? C_WHITE : C_GREY, 1);
        const char *v = "";
        px_t vc = s_th.accent;
        switch(i) {
            case O_GHOST:  v = g_arc.ghost ? "ON" : "OFF"; break;
            case O_GRID:   v = g_arc.grid ? "ON" : "OFF"; break;
            case O_REPEAT: v = repeat[g_arc.das & 3]; break;
            case O_UPKEY:  v = g_arc.upkey ? "HARD DROP" : "ROTATE"; break;
            case O_RESET:  v = s_reset_confirm ? "SURE? ENTER" : ""; vc = C_RED; break;
            default: break;
        }
        if(v[0]) {
            if(sel && i < O_RESET) {
                text_draw(172, y, G_TRI_L, C_DIM, 1);
                text_draw(264, y, G_TRI_R, C_DIM, 1);
            }
            text_center_x(221, y, v, vc, 1, 0);
        }
    }
    const char *hint = (s_opt_sel == O_RESET && s_reset_confirm) ? "PRESS ENTER AGAIN TO CLEAR THEM" : hints[s_opt_sel];
    text_center(186, hint, C_DIM, 1, 0);
}

// ------------------------------------------------------------------ records

static void scores_update(void)
{
    if(g_in.menu & B_LEFT) s_tab = (s_tab + TM_COUNT - 1) % TM_COUNT;
    if(g_in.menu & B_RIGHT) s_tab = (s_tab + 1) % TM_COUNT;
    if(g_in.pressed & (B_A | B_B)) {
        s_new_rec_rank = -1;
        set_state(T_MENU);
    }
}

static void scores_draw(void)
{
    char buf[48];           // room for the totals line at any size ("GAMES 4294967295   LINES 4,294,967,295")
    menu_bg_draw();
    ar_panel(22, 40, 276, 170, s_th.accent);
    text_center(28, "HIGH SCORES", C_WHITE, 2, TX_OUTLINE);
    // tabs
    for(int m = 0; m < TM_COUNT; m++) {
        int cx = 70 + m * 90;
        bool on = m == s_tab;
        if(on) gfx_fill_alpha(cx - 38, 50, 76, 14, px_scale(s_th.accent, 14), 14);
        text_center_x(cx, 53, MODE_NAMES[m], on ? C_WHITE : C_DIM, 1, 0);
        if(on) gfx_hline(cx - 38, cx + 37, 64, s_th.accent);
    }
    text_draw(36, 74, s_tab == TM_SPRINT ? "     NAME        TIME" : "     NAME        SCORE", C_DIM, 1);
    text_right(284, 74, s_tab == TM_MARATHON ? "LEVEL" : (s_tab == TM_SPRINT ? "" : "LINES"), C_DIM, 1, 0);
    for(int i = 0; i < TREC_N; i++) {
        const trec_t *r = &g_arc.rec[s_tab][i];
        int y = 90 + i * 18;
        px_t c = i == 0 ? C_GOLD : (i < 3 ? C_WHITE : C_GREY);
        if(s_tab == s_new_rec_mode && i == s_new_rec_rank && (g_arc_t & 8)) c = s_th.accent;
        ar_cell(38, y - 1, 9, PC_I + (i * 3) % 7);
        fmt_int(buf, i + 1);
        text_draw(52, y, buf, c, 1);
        text_draw(66, y, r->name, c, 1);
        if(s_tab == TM_SPRINT) fmt_time(buf, r->value, true);
        else fmt_commas(buf, r->value);
        text_right(222, y, buf, c, 1, 0);
        if(s_tab != TM_SPRINT) {
            fmt_int(buf, s_tab == TM_MARATHON ? r->level : r->lines);
            text_right(280, y, buf, c, 1, 0);
        }
    }
    strcpy(buf, "GAMES ");
    fmt_int(buf + strlen(buf), (long)g_arc.plays);
    str_cat(buf, "   LINES ");
    fmt_commas(buf + strlen(buf), g_arc.lines);
    text_center(190, buf, C_DIM, 1, 0);
    text_center(SCR_H - 12, G_LEFT G_RIGHT " MODE   ENTER BACK", COL(120, 120, 160), 1, 0);
}

// ------------------------------------------------------------------ how to play

static void howto_update(void)
{
    if(g_in.pressed & (B_A | B_B)) set_state(T_MENU);
}

static void howto_draw(void)
{
    menu_bg_draw();
    ar_panel(12, 36, 296, 196, s_th.accent);
    text_center(24, "HOW TO PLAY", C_WHITE, 2, TX_OUTLINE);
    struct { const char *k, *v; } rows[] = {
        { G_LEFT " " G_RIGHT, "MOVE" },
        { G_DOWN, "SOFT DROP" },
        { "ENTER / SPACE", "HARD DROP" },
        { G_UP " / X", "TURN CLOCKWISE" },
        { "Z", "TURN THE OTHER WAY" },
        { "C / SHIFT", "HOLD THE PIECE FOR LATER" },
        { "CANCEL / TAB", "PAUSE" },
        { "HOLD POWER", "SAVE THE GAME AND SWITCH OFF" },
    };
    int y = 50;
    for(unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        text_draw(26, y, rows[i].k, s_th.accent, 1);
        text_draw(118, y, rows[i].v, C_WHITE, 1);
        y += 13;
    }
    y += 6;
    text_draw(26, y, "SCORING", C_GOLD, 1);
    y += 13;
    text_draw(26, y, "TETRIS 800    T-SPIN DOUBLE 1200", C_GREY, 1);
    y += 11;
    text_draw(26, y, "BACK-TO-BACK x1.5    COMBOS 50 EACH", C_GREY, 1);
    y += 11;
    text_draw(26, y, "ALL TIMES THE LEVEL. EMPTY THE FIELD", C_GREY, 1);
    y += 11;
    text_draw(26, y, "FOR A PERFECT CLEAR BONUS.", C_GREY, 1);
}

// ------------------------------------------------------------------ ready, pause, confirm

static void ready_update(void)
{
    if(s_st_t == s_ready_len - 16) fx_ring((float)(FX + FW / 2), (float)(FY + FH / 2), 6, 5.0f, s_th.accent, 16, 3);
    if(s_st_t >= s_ready_len) {
        s_g.keys_prev = TK_ALL;         // keys held through the count do nothing
        set_state(T_PLAY);
    }
    if(g_in.pressed & (B_PAUSE | B_B)) {
        s_pause_sel = 0;
        set_state(T_PAUSE);
    }
}

static void ready_draw_text(void)
{
    int t = s_st_t, go = s_ready_len - 16;
    if(t < go) {
        int k = t < 6 ? t * 5 : 32;
        text_center(FY + FH / 2 - 12, "READY", px_scale(s_th.accent, k), 3, TX_OUTLINE);
    } else {
        int k = 32 - (t - go) * 2;
        text_center(FY + FH / 2 - 16, "GO!", px_scale(C_WHITE, k), 4, TX_OUTLINE);
    }
}

enum { PM_RESUME, PM_RESTART, PM_SAVEQUIT, PM_QUIT, PM_COUNT };

static void pause_update(void)
{
    if(g_in.menu & B_UP) s_pause_sel = (s_pause_sel + PM_COUNT - 1) % PM_COUNT;
    if(g_in.menu & B_DOWN) s_pause_sel = (s_pause_sel + 1) % PM_COUNT;
    if(g_in.pressed & (B_PAUSE | B_B)) {
        s_ready_len = 30;
        set_state(T_READY);
        return;
    }
    if(!(g_in.pressed & B_A)) return;
    switch(s_pause_sel) {
        case PM_RESUME:
            s_ready_len = 30;
            set_state(T_READY);
            break;
        case PM_RESTART:
        case PM_QUIT:
            s_confirm = s_pause_sel == PM_RESTART ? CF_RESTART : CF_QUIT;
            s_confirm_sel = 1;
            s_confirm_back = T_PAUSE;
            set_state(T_CONFIRM);
            break;
        case PM_SAVEQUIT:
            if(!suspend_game()) {
                game_ended();       // it was over after all: the results, and any record
                break;
            }
            s_menu_sel = MN_CONTINUE;
            set_state(T_MENU);
            break;
    }
}

static void pause_draw(void)
{
    static const char *const items[PM_COUNT] = { "RESUME", "RESTART", "SAVE AND QUIT", "END GAME" };
    char buf[32];
    ar_panel(80, 70, 160, 110, s_th.accent);
    text_center(58, "PAUSED", C_WHITE, 2, TX_OUTLINE);
    for(int i = 0; i < PM_COUNT; i++) ar_menu_item(160, 84 + i * 18, items[i], i == s_pause_sel, true, s_th.accent);
    strcpy(buf, MODE_NAMES[s_mode]);
    if(s_mode == TM_MARATHON) {
        str_cat(buf, "  LEVEL ");
        fmt_int(buf + strlen(buf), s_g.level);
    }
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
            if(g_arc.suspended) g_events |= EV_SAVE_ARCADE;
            g_arc.suspended = false;
            start_game(s_mode);
            break;
        case CF_NEW:
            g_arc.suspended = false;
            g_events |= EV_SAVE_ARCADE;
            start_game(s_mode);
            break;
    }
}

static void confirm_draw(void)
{
    static const char *const heads[3] = { "END GAME?", "RESTART?", "NEW GAME?" };
    static const char *const lines[3] = { "THIS GAME ENDS HERE.", "THIS GAME IS LOST.", "YOUR SAVED GAME WILL BE LOST." };
    if(s_confirm == CF_NEW) menu_bg_draw();
    ar_panel(50, 86, 220, 80, s_th.accent);
    text_center(74, heads[s_confirm], C_WHITE, 2, TX_OUTLINE);
    text_center(100, lines[s_confirm], C_GREY, 1, 0);
    ar_menu_item(160, 124, "YES", s_confirm_sel == 0, true, s_th.accent);
    ar_menu_item(160, 142, "NO", s_confirm_sel == 1, true, s_th.accent);
}

// ------------------------------------------------------------------ game over / results

static int over_panel_at(void)
{
    // after the stack turns grey, or once FINISH / TIME UP has had its moment
    return s_g.won ? 56 : 28;
}

static void over_update(void)
{
    s_over_t++;
    // the stack turns to stone from the bottom up
    if(!s_g.won && s_over_t <= TB_H) {
        int y = TB_H - s_over_t;
        for(int x = 0; x < TB_W; x++) {
            if(s_g.cell[y][x]) s_g.cell[y][x] = PC_GREY;
        }
    }
    if(s_over_t < over_panel_at() + 12) return;
    if(g_in.pressed & (B_A | B_B)) {
        if(s_rank >= 0) {
            strcpy(s_name, g_arc.name);
            s_name_len = (int)strlen(s_name);
            set_state(T_NAME);
        } else {
            s_menu_sel = MN_MARATHON + s_mode;
            set_state(T_MENU);
        }
    } else if((g_in.raw_pressed & KEYBIT(K_R)) && s_rank < 0) {
        start_game(s_mode);
    }
}

static void over_draw(void)
{
    if(s_over_t < over_panel_at()) return;
    char buf[24];
    int k = iclamp((s_over_t - over_panel_at()) * 4, 0, 32);
    int y0 = 44 + (32 - k);
    ar_panel(60, y0, 200, 150, s_g.won ? C_GOLD : s_th.accent);
    const char *head = s_quit ? "GAME ENDED" : (s_g.won ? (s_mode == TM_SPRINT ? "FINISHED!" : "TIME UP!") : "GAME OVER");
    text_center(y0 + 8, head, s_g.won ? C_GOLD : C_WHITE, 2, TX_OUTLINE);
    int y = y0 + 32;
    if(s_mode == TM_SPRINT && s_g.won) {
        fmt_time(buf, s_g.frames, true);
        text_center(y, buf, C_WHITE, 3, TX_OUTLINE);
    } else {
        fmt_commas(buf, s_g.score);
        text_center(y, buf, C_WHITE, 3, TX_OUTLINE);
    }
    y += 30;
    struct { const char *l; char v[16]; } rows[4];
    int n = 0;
    rows[n].l = "LINES";
    fmt_int(rows[n++].v, s_g.lines);
    if(s_mode == TM_MARATHON) {
        rows[n].l = "LEVEL";
        fmt_int(rows[n++].v, s_g.level);
    }
    rows[n].l = "TIME";
    fmt_time(rows[n++].v, s_g.frames, s_mode == TM_SPRINT);
    rows[n].l = "TETRIS / T-SPIN";
    fmt_int(rows[n].v, s_g.n_tetris);
    str_cat(rows[n].v, " / ");
    fmt_int(rows[n].v + strlen(rows[n].v), s_g.n_tspin);
    n++;
    for(int i = 0; i < n; i++, y += 12) {
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
    set_state(T_SCORES);
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
    menu_bg_draw();
    ar_panel(24, 50, 272, 150, s_th.accent);
    text_center(38, "NEW RECORD", C_GOLD, 2, TX_OUTLINE);
    if(s_mode == TM_SPRINT) fmt_time(buf, s_g.frames, true);
    else fmt_commas(buf, s_g.score);
    text_center(66, buf, C_WHITE, 3, TX_OUTLINE);
    strcpy(buf, MODE_NAMES[s_mode]);
    str_cat(buf, "  #");
    fmt_int(buf + strlen(buf), s_rank + 1);
    text_center(96, buf, s_th.accent, 1, 0);
    text_center(114, "TYPE YOUR NAME ON THE KEYBOARD", C_GREY, 1, 0);
    int w = NAME_LEN * 12 + 12;
    gfx_fill(160 - w / 2, 130, w, 22, COL(8, 10, 30));
    ar_neon_rect(160 - w / 2, 130, w, 22, s_th.accent, 1);
    text_draw(160 - w / 2 + 6, 134, s_name, C_WHITE, 2);
    if(s_name_len < NAME_LEN && (g_arc_t & 8)) gfx_fill(160 - w / 2 + 6 + s_name_len * 12, 147, 10, 2, s_th.accent);
    text_center(166, G_CROSS " DEL: ERASE     ENTER: DONE", C_DIM, 1, 0);
}

// ------------------------------------------------------------------ for the desktop build's tests

static tai_t s_bot;

void tetris_debug_start(int mode)
{
    start_game(mode);
    memset(&s_bot, 0, sizeof(s_bot));
}

uint64_t tetris_bot_keys(int pace)
{
    if(s_st != T_PLAY) return 0;
    static const uint8_t KEY[7] = { K_LEFT, K_RIGHT, K_DOWN, K_SPACE, K_X, K_Z, K_C };
    uint32_t k = tet_ai_keys(&s_g, &s_bot, pace, true);
    uint64_t raw = 0;
    for(int i = 0; i < 7; i++) {
        if(k & (1u << i)) raw |= KEYBIT(KEY[i]);
    }
    return raw;
}

const tgame_t *tetris_debug_game(void)
{
    return &s_g;
}

int tetris_debug(uint32_t *score, int *lines, int *level, bool *over)
{
    *score = s_g.score;
    *lines = s_g.lines;
    *level = s_g.level;
    *over = s_g.over;
    return s_st;
}

// ------------------------------------------------------------------ entry points

void tetris_enter(void)
{
    s_menu_sel = g_arc.suspended ? MN_CONTINUE : MN_MARATHON;
    for(int i = 0; i < 9; i++) falling_reset(&s_fall[i], true);
    theme_set(0, true);
    fx_reset_all();
    s_logo_t = 0;
    set_state(T_MENU);
}

bool tetris_in_play(void)
{
    return s_st == T_PLAY || s_st == T_READY;
}

void tetris_power_tap(void)
{
    if(tetris_in_play()) {
        s_pause_sel = 0;
        set_state(T_PAUSE);
    }
}

static void off_mid_game(void)
{
    // keep the game to carry on with, or if that ends it, its record
    if(suspend_game()) return;
    game_ended();
    commit_record(g_arc.name[0] ? g_arc.name : "PLAYER");
}

void tetris_before_off(void)
{
    switch(s_st) {
        case T_READY:
        case T_PLAY:
        case T_PAUSE:
            off_mid_game();
            break;
        case T_CONFIRM:
            if(s_confirm != CF_NEW) off_mid_game();
            break;
        case T_OVER:
            commit_record(g_arc.name[0] ? g_arc.name : "PLAYER");
            break;
        case T_NAME:
            name_commit();
            break;
        default:
            break;
    }
}

void tetris_update(void)
{
    s_st_t++;
    s_logo_t++;
    theme_step();
    switch(s_st) {
        case T_MENU:    menu_update(); break;
        case T_OPTIONS: options_update(); break;
        case T_SCORES:  scores_update(); break;
        case T_HOWTO:   howto_update(); break;
        case T_READY:   ready_update(); break;
        case T_PLAY:    play_update(); break;
        case T_PAUSE:   pause_update(); break;
        case T_CONFIRM: confirm_update(); break;
        case T_OVER:    over_update(); break;
        case T_NAME:    name_update(); break;
    }
    effects_tick();
}

void tetris_draw(void)
{
    switch(s_st) {
        case T_MENU:    menu_draw(); break;
        case T_OPTIONS: options_draw(); break;
        case T_SCORES:  scores_draw(); break;
        case T_HOWTO:   howto_draw(); break;
        case T_NAME:    name_draw(); break;
        case T_CONFIRM:
            if(s_confirm == CF_NEW) {
                confirm_draw();
                break;
            }
            // fall through: over the (hidden) field
        default: {
            bool hide = s_st == T_PAUSE || s_st == T_CONFIRM;
            draw_bg();
            draw_field(hide);
            draw_hud(hide);
            fx_draw();
            draw_banner();
            if(s_st == T_READY) ready_draw_text();
            if(s_st == T_PAUSE) pause_draw();
            if(s_st == T_CONFIRM) confirm_draw();
            if(s_st == T_OVER) over_draw();
            break;
        }
    }
    fx_draw_top();
}
