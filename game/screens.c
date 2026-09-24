// QUASAR - title, menus, pause, results, name entry, ending.
#include "game.h"
#include <string.h>

static const char *const DIFF_NAMES[3] = { "EASY", "NORMAL", "HARD" };
static const px_t DIFF_COLS[3] = { COL(120, 255, 160), COL(120, 220, 255), COL(255, 100, 100) };

// menu model for the title screen
enum { M_START, M_PRACTICE, M_SCORES, M_OPTIONS, M_HOWTO, M_SYSTEM, M_COUNT };
static const char *const TITLE_ITEMS[M_COUNT] = { "START GAME", "PRACTICE", "HIGH SCORES", "OPTIONS", "HOW TO PLAY", "SYSTEM" };

static int s_clear_step;
static uint32_t s_clear_bonus[4];
static int s_cont_t;
static int s_idle;
static int s_opt_sel;
static int s_practice_sel;
static int s_pause_sel;
static int s_reset_confirm;
static bool s_quit;

static bool title_item_enabled(int i)
{
    if(i == M_PRACTICE) return g_save.max_stage > 1;
    return true;
}

void game_start_run(int stage, bool practice)
{
    memset(&g_run, 0, sizeof(g_run));
    g_run.mult = 1;
    g_run.hiscore = g_save.scores[0].score;
    g_run.practice = practice;
    g_run.start_stage = stage;
    g_run.next_life = 50000;
    player_reset(true);
    stage_start(stage);
    if(!practice) {
        g_save.plays++;
        g_events |= EV_SAVE;
    }
    game_set_state(ST_PLAY);
}

void title_bg_draw(void)
{
    bg_draw();
}

static void draw_logo(int y)
{
    int t = g_game.frame;
    gfx_glow(160, y + 20, 80, COL(40, 10, 60));
    gfx_sprite(&SPR_LOGO, 160, y + 20, 0, NULL, DM_NORMAL, 0);
    // light sweep across the letters
    int sx = (t * 6) % 600 - 140;
    for(int k = 0; k < 10; k++) {
        gfx_line(sx + k, y, sx + k - 20, y + 42, COL(60, 60, 70), DM_ADD);
    }
    // repaint the gaps with the logo mask so the sweep only lights the letters
    (void)sx;
}

static void draw_menu_item(int y, const char *s, bool sel, bool enabled)
{
    px_t c = !enabled ? COL(70, 70, 90) : (sel ? C_WHITE : COL(160, 170, 210));
    if(sel) {
        int w = text_width(s, 1) + 26;
        int pulse = 10 + ((isin256(g_game.frame * 8) * 6) >> 14);
        gfx_fill_alpha(160 - w / 2, y - 3, w, 13, COL(40, 110, 200), pulse);
        gfx_hline(160 - w / 2, 160 + w / 2 - 1, y - 3, C_CYAN);
        gfx_hline(160 - w / 2, 160 + w / 2 - 1, y + 9, C_CYAN);
        text_draw(160 - w / 2 + 4, y, G_TRI_R, C_CYAN, 1);
        text_draw(160 + w / 2 - 10, y, G_TRI_L, C_CYAN, 1);
    }
    text_center(y, s, c, 1, TX_SHADOW);
}

static void battery_draw(int x, int y)
{
    int lv = g_game.battery;
    if(lv < 0) {
        text_draw(x - 8, y, "USB", C_DIM, 1);
        return;
    }
    gfx_rect(x, y, 14, 7, C_GREY);
    gfx_fill(x + 14, y + 2, 2, 3, C_GREY);
    px_t c = lv <= 0 ? C_RED : (lv == 1 ? C_ORANGE : C_GREEN);
    gfx_fill(x + 1, y + 1, lv <= 0 ? 2 : lv * 4, 5, c);
}

// ------------------------------------------------------------------ title

static void title_update(void)
{
    if(g_in.menu & B_UP) {
        do { g_game.sel = (g_game.sel + M_COUNT - 1) % M_COUNT; } while(!title_item_enabled(g_game.sel));
    }
    if(g_in.menu & B_DOWN) {
        do { g_game.sel = (g_game.sel + 1) % M_COUNT; } while(!title_item_enabled(g_game.sel));
    }
    if(g_in.raw_pressed) s_idle = 0;
    else s_idle++;

    if(g_in.pressed & B_A) {
        switch(g_game.sel) {
            case M_START:
                game_start_run(1, false);
                break;
            case M_PRACTICE:
                s_practice_sel = 0;
                game_set_state(ST_PRACTICE);
                break;
            case M_SCORES:
                game_set_state(ST_SCORES);
                break;
            case M_OPTIONS:
                s_opt_sel = 0;
                s_reset_confirm = 0;
                game_set_state(ST_OPTIONS);
                break;
            case M_HOWTO:
                game_set_state(ST_HOWTO);
                break;
            case M_SYSTEM:
                g_events |= EV_SYSTEM;
                break;
        }
    }
    // attract: cycle to the score table when idle
    if(s_idle > 30 * 25) {
        s_idle = 0;
        game_set_state(ST_SCORES);
    }
}

static void title_draw(void)
{
    char buf[24];
    title_bg_draw();
    draw_logo(20);
    text_center(70, "RIDE THE LIGHT  " G_DOT "  BREAK THE DARK", COL(200, 160, 255), 1, TX_SHADOW);

    for(int i = 0; i < M_COUNT; i++) {
        draw_menu_item(98 + i * 18, TITLE_ITEMS[i], i == g_game.sel, title_item_enabled(i));
    }

    strcpy(buf, "HI ");
    fmt_score(buf + 3, g_save.scores[0].score, 8);
    text_draw(4, 4, buf, C_GOLD, 1);
    battery_draw(SCR_W - 22, 4);

    gfx_fill_mode(0, SCR_H - 13, SCR_W, 13, 0, DM_SHADOW);
    text_draw(4, SCR_H - 10, "V" QUASAR_VERSION, C_DIM, 1);
    text_right(SCR_W - 4, SCR_H - 10, "FOR COLDCARD Q", C_DIM, 1, 0);
    text_center(SCR_H - 10, G_UP G_DOWN " SELECT   ENTER OK", COL(110, 110, 150), 1, 0);
}

// ------------------------------------------------------------------ options

enum { O_DIFF, O_SHAKE, O_BRIGHT, O_FPS, O_SYNC, O_RESET, O_BACK, O_COUNT };

static void options_update(void)
{
    if(g_in.menu & B_UP) s_opt_sel = (s_opt_sel + O_COUNT - 1) % O_COUNT;
    if(g_in.menu & B_DOWN) s_opt_sel = (s_opt_sel + 1) % O_COUNT;
    int d = 0;
    if(g_in.menu & B_LEFT) d = -1;
    if(g_in.menu & B_RIGHT) d = 1;
    if((g_in.pressed & B_A) && s_opt_sel != O_RESET && s_opt_sel != O_BACK) d = 1;

    if(d) {
        savedata_t was = g_save;
        switch(s_opt_sel) {
            case O_DIFF:
                g_save.difficulty = (uint8_t)((g_save.difficulty + 3 + d) % 3);
                break;
            case O_SHAKE:
                g_save.shake = (uint8_t)!g_save.shake;
                break;
            case O_BRIGHT:
                g_save.brightness = (uint8_t)iclamp(g_save.brightness + d, 0, 4);
                if(g_save.brightness != was.brightness) g_events |= EV_BRIGHT;
                break;
            case O_FPS:
                g_save.show_fps = (uint8_t)!g_save.show_fps;
                break;
            case O_SYNC:
                g_save.vsync = (uint8_t)((g_save.vsync + 3 + d) % 3);
                g_events |= EV_VSYNC;
                break;
            default:
                break;
        }
        // each save rewrites /flash, so skip it when holding a key at a limit
        if(memcmp(&was, &g_save, sizeof(was))) g_events |= EV_SAVE;
    }
    if(g_in.pressed & B_A) {
        if(s_opt_sel == O_RESET) {
            if(++s_reset_confirm >= 2) {
                // just the table: settings and practice progress stay
                savedata_t keep = g_save;
                save_defaults();
                memcpy(keep.scores, g_save.scores, sizeof(keep.scores));
                g_save = keep;
                s_reset_confirm = 0;
                g_events |= EV_SAVE;
                fx_flash(C_RED, 6);
            }
        } else if(s_opt_sel == O_BACK) {
            game_set_state(ST_TITLE);
        }
    }
    if(s_opt_sel != O_RESET) s_reset_confirm = 0;
    if(g_in.pressed & B_B) game_set_state(ST_TITLE);
}

static void panel(int x, int y, int w, int h, const char *title)
{
    gfx_fill_alpha(x, y, w, h, COL(6, 8, 24), 26);
    gfx_rect(x, y, w, h, COL(60, 90, 160));
    gfx_rect(x + 1, y + 1, w - 2, h - 2, COL(20, 30, 70));
    if(title) {
        int tw = text_width(title, 2);
        gfx_fill(160 - tw / 2 - 8, y - 9, tw + 16, 18, COL(10, 14, 40));
        gfx_rect(160 - tw / 2 - 8, y - 9, tw + 16, 18, COL(60, 90, 160));
        text_center(y - 6, title, C_WHITE, 2, TX_SHADOW);
    }
}

static void options_draw(void)
{
    static const char *const labels[O_COUNT] = { "DIFFICULTY", "SCREEN SHAKE", "BRIGHTNESS", "SHOW FPS", "SCREEN SYNC", "RESET SCORES", "BACK" };
    static const char *const sync_names[3] = { "L " G_RIGHT " R", "R " G_RIGHT " L", "OFF" };
    title_bg_draw();
    panel(30, 30, 260, 186, "OPTIONS");
    for(int i = 0; i < O_COUNT; i++) {
        int y = 50 + i * 20;
        bool sel = i == s_opt_sel;
        if(sel) gfx_fill_alpha(38, y - 4, 244, 15, COL(40, 110, 200), 12);
        text_draw(46, y, labels[i], sel ? C_WHITE : C_GREY, 1);
        char buf[20] = "";
        px_t vc = C_CYAN;
        switch(i) {
            case O_DIFF:   strcpy(buf, DIFF_NAMES[g_save.difficulty]); vc = DIFF_COLS[g_save.difficulty]; break;
            case O_SHAKE:  strcpy(buf, g_save.shake ? "ON" : "OFF"); break;
            case O_FPS:    strcpy(buf, g_save.show_fps ? "ON" : "OFF"); break;
            case O_SYNC:   strcpy(buf, sync_names[g_save.vsync % 3]); break;
            case O_RESET:  strcpy(buf, s_reset_confirm ? "SURE? ENTER" : ""); vc = C_RED; break;
            default: break;
        }
        if(i == O_BRIGHT) {
            for(int k = 0; k < 5; k++) {
                gfx_fill(196 + k * 12, y - 1, 9, 9, k <= g_save.brightness ? C_GOLD : COL(50, 50, 70));
            }
        } else if(buf[0]) {
            if(i != O_RESET && sel) {
                text_draw(190, y, G_TRI_L, C_DIM, 1);
                text_draw(270, y, G_TRI_R, C_DIM, 1);
            }
            text_center_x(232, y, buf, vc, 1, 0);
        }
    }
    text_center(SCR_H - 16, "SCREEN SYNC: PICK THE ONE WITH NO TEARING", C_DIM, 1, 0);
}

// ------------------------------------------------------------------ scores

static void scores_update(void)
{
    if(g_in.pressed & (B_A | B_B)) game_set_state(ST_TITLE);
    if(g_game.t > 30 * 12) game_set_state(ST_HOWTO);
}

static void scores_draw(void)
{
    char buf[24];
    title_bg_draw();
    panel(20, 28, 280, 190, "HIGH SCORES");
    text_draw(34, 44, "RANK  PILOT        SCORE    STG", C_DIM, 1);
    for(int i = 0; i < NUM_SCORES; i++) {
        hiscore_t *h = &g_save.scores[i];
        int y = 60 + i * 18;
        px_t c = i == 0 ? C_GOLD : (i < 3 ? C_WHITE : C_GREY);
        if(g_game.name_rank == i && g_game.prev == ST_NAME && (g_game.frame & 8)) c = C_CYAN;
        fmt_int(buf, i + 1);
        text_draw(40, y, buf, c, 1);
        text_draw(70, y, h->name, c, 1);
        fmt_score(buf, h->score, 8);
        text_draw(160, y, buf, c, 1);
        if(h->stage >= 6) strcpy(buf, "ALL");
        else fmt_int(buf, h->stage);
        text_draw(236, y, buf, c, 1);
        gfx_fill(262, y + 1, 5, 5, DIFF_COLS[h->diff % 3]);
    }
}

// ------------------------------------------------------------------ how to play

static void howto_update(void)
{
    if(g_in.pressed & (B_A | B_B)) game_set_state(ST_TITLE);
    if(g_game.t > 30 * 16) game_set_state(ST_TITLE);
}

static void howto_draw(void)
{
    title_bg_draw();
    panel(12, 26, 296, 202, "HOW TO PLAY");
    int y = 42;
    struct { const char *k, *v; } rows[] = {
        { "D-PAD", "FLY  (WASD WORKS TOO)" },
        { "GUNS", "FIRE ALL BY THEMSELVES" },
        { "HOLD ENTER", "CHARGE THE QUASAR BEAM" },
        { "CANCEL", "NOVA BOMB: CLEARS BULLETS" },
        { "TAB / P", "PAUSE" },
        { "HOLD POWER", "SAVE AND SWITCH OFF" },
    };
    for(unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        text_draw(26, y, rows[i].k, C_CYAN, 1);
        text_draw(110, y, rows[i].v, C_WHITE, 1);
        y += 12;
    }
    y += 6;
    text_draw(26, y, "POWER-UPS", C_GOLD, 1);
    y += 16;
    struct { const sprite_t *s; const char *l; const char *n; } caps[8] = {
        { &SPR_CAP_GOLD, "P", "POWER" }, { &SPR_CAP_RED, "S", "SPREAD" }, { &SPR_CAP_GREEN, "L", "LASER" },
        { &SPR_CAP_VIOLET, "M", "MISSILE" }, { &SPR_CAP_BLUE, "B", "NOVA" }, { &SPR_CAP_RED, G_HEART, "REPAIR" },
        { &SPR_CAP_GOLD, "O", "OPTION" }, { &SPR_CAP_BLUE, G_STAR, "1UP" },
    };
    for(int i = 0; i < 8; i++) {
        int cx = 36 + (i % 4) * 70, cy = y + (i / 4) * 20;
        gfx_sprite(caps[i].s, cx, cy, 0, NULL, DM_NORMAL, 0);
        text_draw_fx(cx - 2, cy - 4, caps[i].l, C_WHITE, 1, TX_SHADOW, NULL);
        text_draw(cx + 10, cy - 3, caps[i].n, C_GREY, 1);
    }
    y += 44;
    text_center(y, "KILL FAST TO BUILD THE CHAIN MULTIPLIER.", COL(200, 200, 255), 1, 0);
    text_center(y + 11, "GEMS ARE WORTH MORE WHILE IT IS HIGH.", COL(200, 200, 255), 1, 0);
}

// ------------------------------------------------------------------ practice

static void practice_update(void)
{
    int n = g_save.max_stage < 1 ? 1 : (g_save.max_stage > 5 ? 5 : g_save.max_stage);
    if(g_in.menu & B_UP) s_practice_sel = (s_practice_sel + n - 1) % n;
    if(g_in.menu & B_DOWN) s_practice_sel = (s_practice_sel + 1) % n;
    if(g_in.pressed & B_A) game_start_run(s_practice_sel + 1, true);
    if(g_in.pressed & B_B) game_set_state(ST_TITLE);
}

static void practice_draw(void)
{
    char buf[24];
    title_bg_draw();
    panel(40, 40, 240, 150, "PRACTICE");
    int n = g_save.max_stage < 1 ? 1 : (g_save.max_stage > 5 ? 5 : g_save.max_stage);
    for(int i = 0; i < n; i++) {
        strcpy(buf, "STAGE ");
        fmt_int(buf + 6, i + 1);
        str_cat(buf, "  ");
        str_cat(buf, STAGE_NAMES[i + 1]);
        draw_menu_item(64 + i * 22, buf, i == s_practice_sel, true);
    }
    text_center(174, "NO SCORES ARE SAVED IN PRACTICE", C_DIM, 1, 0);
}

// ------------------------------------------------------------------ pause

static void pause_update(void)
{
    if(g_in.menu & (B_UP | B_DOWN)) s_pause_sel ^= 1;
    if(g_in.pressed & B_PAUSE) {
        game_set_state(ST_PLAY);
        return;
    }
    if(g_in.pressed & B_B) {
        game_set_state(ST_PLAY);
        return;
    }
    if(g_in.pressed & B_A) {
        if(s_pause_sel == 0) game_set_state(ST_PLAY);
        else {
            g_game.confirm_sel = 1;
            game_set_state(ST_CONFIRM_QUIT);
        }
    }
}

static void pause_draw(void)
{
    char buf[24];
    gfx_fill_mode(0, 0, SCR_W, SCR_H, 0, DM_SHADOW);
    panel(70, 70, 180, 100, "PAUSED");
    draw_menu_item(96, "RESUME", s_pause_sel == 0, true);
    draw_menu_item(116, "QUIT TO TITLE", s_pause_sel == 1, true);
    strcpy(buf, "STAGE ");
    fmt_int(buf + 6, g_stage.num);
    str_cat(buf, "  ");
    str_cat(buf, DIFF_NAMES[difficulty()]);
    text_center(142, buf, C_DIM, 1, 0);
    text_center(154, "HOLD POWER TO SWITCH OFF", C_DIM, 1, 0);
    battery_draw(SCR_W - 22, 4);
}

static void confirm_update(void)
{
    if(g_in.menu & (B_UP | B_DOWN | B_LEFT | B_RIGHT)) g_game.confirm_sel ^= 1;
    if(g_in.pressed & B_B) {
        game_set_state(ST_PAUSE);
        return;
    }
    if(g_in.pressed & B_A) {
        if(g_game.confirm_sel == 0) {
            // quitting counts as game over for the score table
            s_quit = true;
            game_set_state(g_run.practice ? ST_TITLE : ST_GAMEOVER);
        } else {
            game_set_state(ST_PAUSE);
        }
    }
}

static void confirm_draw(void)
{
    gfx_fill_mode(0, 0, SCR_W, SCR_H, 0, DM_SHADOW);
    panel(60, 80, 200, 80, "QUIT?");
    text_center(100, "THE RUN ENDS HERE.", C_GREY, 1, 0);
    draw_menu_item(120, "YES, QUIT", g_game.confirm_sel == 0, true);
    draw_menu_item(138, "NO", g_game.confirm_sel == 1, true);
}

// ------------------------------------------------------------------ stage clear

static void stageclear_enter(void)
{
    // nothing left alive or in flight during the tally
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(g_en[i].alive) enemy_kill(&g_en[i], false);
    }
    eshots_cancel(false);
    s_clear_step = 0;
    int pct = g_stage.spawned ? (g_stage.kills * 100) / g_stage.spawned : 100;
    if(pct > 100) pct = 100;
    s_clear_bonus[0] = (uint32_t)pct * 200u;
    s_clear_bonus[1] = g_stage.damage_taken == 0 ? 20000u : 0;
    s_clear_bonus[2] = (uint32_t)g_run.gems * 10u;
    s_clear_bonus[3] = (uint32_t)(pct);
    g_run.gems = 0;
}

static void stageclear_update(void)
{
    int t = g_game.t;
    if(t == 40 || t == 70 || t == 100) {
        int k = (t - 40) / 30;
        if(s_clear_bonus[k]) add_score(s_clear_bonus[k], 160, 120, false);
    }
    if(t > 120 && (g_in.pressed & B_A || t > 30 * 7)) {
        if(g_stage.num >= 5) {
            game_set_state(ST_ENDING);
        } else if(g_run.practice) {
            game_set_state(ST_TITLE);
        } else {
            stage_start(g_stage.num + 1);
            game_set_state(ST_PLAY);
        }
    }
}

static void stageclear_draw(void)
{
    char buf[24];
    int t = g_game.t;
    int y = 60;
    gfx_fill_alpha(0, y - 12, SCR_W, 130, COL(4, 6, 20), 20);
    text_center(y, "STAGE CLEAR", C_GOLD, 3, TX_OUTLINE);
    y += 36;
    if(t > 30) {
        text_draw(60, y, "TARGETS DOWN", C_GREY, 1);
        fmt_int(buf, (int)s_clear_bonus[3]);
        str_cat(buf, "%");
        text_right(260, y, buf, C_WHITE, 1, 0);
    }
    y += 14;
    if(t > 40) {
        text_draw(60, y, "HUNTER BONUS", C_GREY, 1);
        fmt_commas(buf, s_clear_bonus[0]);
        text_right(260, y, buf, C_CYAN, 1, 0);
    }
    y += 14;
    if(t > 70) {
        text_draw(60, y, "NO DAMAGE BONUS", C_GREY, 1);
        fmt_commas(buf, s_clear_bonus[1]);
        text_right(260, y, s_clear_bonus[1] ? buf : "--", s_clear_bonus[1] ? C_GREEN : C_DIM, 1, 0);
    }
    y += 14;
    if(t > 100) {
        text_draw(60, y, "GEM BONUS", C_GREY, 1);
        fmt_commas(buf, s_clear_bonus[2]);
        text_right(260, y, buf, C_CYAN, 1, 0);
    }
    y += 22;
    if(t > 120 && (g_game.frame & 16)) text_center(y, "PRESS ENTER", C_WHITE, 1, 0);
}

// ------------------------------------------------------------------ game over / continue

static void finish_run(void)
{
    // offer the score table, then back to the title
    if(!g_run.practice) {
        int r = score_rank(g_run.score);
        if(r >= 0 && g_run.score > 0) {
            g_game.name_rank = r;
            g_game.name_len = 0;
            g_game.name[0] = 0;
            game_set_state(ST_NAME);
            return;
        }
    }
    game_set_state(ST_TITLE);
}

static void gameover_update(void)
{
    int limit = difficulty() == 0 ? 99 : 3;
    if(g_game.t == 1) {
        bool can_continue = !g_run.practice && g_run.continues < limit && s_cont_t == 0;
        s_cont_t = can_continue ? 10 * 30 : -1;
    }

    // keys still being mashed from the fight must not pick for the player
    bool ready = g_game.t > 20;

    if(s_cont_t > 0) {
        s_cont_t--;
        if(ready && (g_in.pressed & B_A)) {
            // continue: score resets, progress kept
            uint32_t best = g_run.score;
            if(score_rank(best) >= 0 && best > 0) {
                // remember the score reached so far on the table
                score_insert(score_rank(best), "CONTINUE", best, g_stage.num, difficulty());
                g_events |= EV_SAVE;
            }
            g_run.score = 0;
            g_run.continues++;
            g_run.mult = 1;
            g_run.chain = 0;
            player_reset(true);
            g_pl.main_lvl = 2;
            game_set_state(ST_PLAY);
            return;
        }
        if(ready && (g_in.pressed & B_B)) s_cont_t = 0;
        if(s_cont_t == 0) finish_run();
    } else if(g_game.t > 90 || (ready && (g_in.pressed & (B_A | B_B)))) {
        finish_run();
    }
}

static void gameover_draw(void)
{
    char buf[16];
    gfx_fill_alpha(0, 70, SCR_W, 100, COL(20, 0, 10), 22);
    text_center(84, "GAME OVER", COL(255, 80, 90), 4, TX_OUTLINE);
    if(s_cont_t > 0) {
        text_center(128, "CONTINUE?", C_WHITE, 2, TX_SHADOW);
        fmt_int(buf, s_cont_t / 30);
        text_center(148, buf, C_GOLD, 2, TX_SHADOW);
        text_center(170, "ENTER: YES   CANCEL: NO", C_GREY, 1, 0);
    }
}

// ------------------------------------------------------------------ name entry

static void name_commit(void)
{
    const char *nm = g_game.name_len ? g_game.name : "PILOT";
    score_insert(g_game.name_rank, nm, g_run.score, g_run.frames ? g_stage.num : 1, difficulty());
    if(g_game.prev == ST_ENDING || g_game.prev == ST_CREDITS) {
        // stage reached shows ALL for a full clear
        g_save.scores[g_game.name_rank].stage = 6;
    }
    g_events |= EV_SAVE;
    game_set_state(ST_SCORES);
}

static void name_update(void)
{
    // letters come straight from the Q keyboard
    for(int k = 10; k < 60; k++) {
        if(!(g_in.raw_pressed & KEYBIT(k))) continue;
        char c = key_to_char(k, false);
        if(k == K_SPACE) c = ' ';
        if(c && g_game.name_len < NAME_LEN) {
            g_game.name[g_game.name_len++] = c;
            g_game.name[g_game.name_len] = 0;
        }
    }
    if((g_in.raw_pressed & KEYBIT(K_DEL)) && g_game.name_len > 0) {
        g_game.name[--g_game.name_len] = 0;
    }
    // half a second before ENTER counts, so tapping through GAME OVER can't skip this
    if((g_in.raw_pressed & KEYBIT(K_ENTER)) && g_game.t > 15) name_commit();
}

void screens_before_off(void)
{
    // switching off (held POWER, or idle) must not lose a new high score
    if(g_game.state == ST_NAME) name_commit();
}

static void name_draw(void)
{
    char buf[24];
    title_bg_draw();
    panel(24, 40, 272, 160, "NEW HIGH SCORE");
    fmt_commas(buf, g_run.score);
    text_center(62, buf, C_GOLD, 3, TX_OUTLINE);
    strcpy(buf, "RANK ");
    fmt_int(buf + 5, g_game.name_rank + 1);
    text_center(92, buf, C_WHITE, 1, 0);
    text_center(110, "TYPE YOUR NAME ON THE KEYBOARD", C_GREY, 1, 0);
    // field
    int w = NAME_LEN * 12 + 12;
    gfx_fill(160 - w / 2, 128, w, 22, COL(8, 10, 30));
    gfx_rect(160 - w / 2, 128, w, 22, C_CYAN);
    text_draw(160 - w / 2 + 6, 132, g_game.name, C_WHITE, 2);
    if(g_game.name_len < NAME_LEN && (g_game.frame & 8)) {
        gfx_fill(160 - w / 2 + 6 + g_game.name_len * 12, 145, 10, 2, C_CYAN);
    }
    text_center(162, G_CROSS " DEL: ERASE     ENTER: DONE", C_DIM, 1, 0);
}

// ------------------------------------------------------------------ ending

static const char *const ENDING_LINES[] = {
    "THE SINGULARITY FALLS SILENT.",
    "",
    "LIGHT POURS BACK INTO THE DARK",
    "AND THE QUASAR SLEEPS AGAIN.",
    "",
    "SOMEWHERE A SMALL SHIP",
    "TURNS FOR HOME.",
};

static void ending_update(void)
{
    if(g_game.t == 1) {
        uint32_t bonus = difficulty() == 2 ? 300000u : (difficulty() == 1 ? 150000u : 60000u);
        bonus += (uint32_t)g_pl.lives * 25000u;
        add_score(bonus, 160, 120, false);
        if(!g_run.practice) {
            g_save.clears++;
            g_save.max_stage = 5;
            g_events |= EV_SAVE;
        }
    }
    if(g_game.t > 30 * 12 && (g_in.pressed & B_A || g_game.t > 30 * 30)) game_set_state(ST_CREDITS);
}

static void ending_draw(void)
{
    int t = g_game.t;
    bg_draw();
    int shown = t / 40;
    for(int i = 0; i < (int)(sizeof(ENDING_LINES) / sizeof(ENDING_LINES[0])) && i <= shown; i++) {
        text_center(50 + i * 14, ENDING_LINES[i], C_WHITE, 1, TX_SHADOW);
    }
    if(t > 30 * 9) {
        char buf[24];
        text_center(170, "ALL CLEAR", C_GOLD, 3, TX_OUTLINE);
        fmt_commas(buf, g_run.score);
        text_center(202, buf, C_WHITE, 2, TX_SHADOW);
    }
}

static const char *const CREDITS[] = {
    "QUASAR",
    "",
    "A GAME FOR THE COLDCARD Q",
    "",
    "DESIGN, CODE AND PIXELS",
    "AK1RA00",
    "",
    "ENGINE",
    "HAND WRITTEN C, 30 FPS",
    "ON A 120 MHZ CORTEX-M4",
    "",
    "BUILT ON MICROPYTHON",
    "AND THE COLDCARD Q BOARD CODE",
    "",
    "THANK YOU FOR PLAYING",
};

static void credits_update(void)
{
    int n = (int)(sizeof(CREDITS) / sizeof(CREDITS[0]));
    if(g_game.t > n * 16 + 280 || (g_game.t > 60 && (g_in.pressed & B_A))) finish_run();
}

static void credits_draw(void)
{
    bg_draw();
    int n = (int)(sizeof(CREDITS) / sizeof(CREDITS[0]));
    int base = SCR_H - g_game.t / 2;
    for(int i = 0; i < n; i++) {
        int y = base + i * 16;
        if(y < -10 || y > SCR_H) continue;
        bool big = (i == 0);
        text_center(y, CREDITS[i], big ? C_GOLD : (CREDITS[i][0] && i % 2 ? C_CYAN : C_WHITE), big ? 3 : 1, TX_SHADOW);
    }
}

// ------------------------------------------------------------------ dispatch

void screens_update(void)
{
    switch(g_game.state) {
        case ST_TITLE:      title_update(); break;
        case ST_OPTIONS:    options_update(); break;
        case ST_SCORES:     scores_update(); break;
        case ST_HOWTO:      howto_update(); break;
        case ST_PRACTICE:   practice_update(); break;
        case ST_PAUSE:      pause_update(); break;
        case ST_CONFIRM_QUIT: confirm_update(); break;
        case ST_STAGECLEAR: stageclear_update(); break;
        case ST_GAMEOVER:   gameover_update(); break;
        case ST_NAME:       name_update(); break;
        case ST_ENDING:     ending_update(); break;
        case ST_CREDITS:    credits_update(); break;
        default: break;
    }
}

void screens_draw(void)
{
    switch(g_game.state) {
        case ST_TITLE:      title_draw(); break;
        case ST_OPTIONS:    options_draw(); break;
        case ST_SCORES:     scores_draw(); break;
        case ST_HOWTO:      howto_draw(); break;
        case ST_PRACTICE:   practice_draw(); break;
        case ST_PAUSE:      pause_draw(); break;
        case ST_CONFIRM_QUIT: confirm_draw(); break;
        case ST_STAGECLEAR: stageclear_draw(); break;
        case ST_GAMEOVER:   gameover_draw(); break;
        case ST_NAME:       name_draw(); break;
        case ST_ENDING:     ending_draw(); break;
        case ST_CREDITS:    credits_draw(); break;
        default: break;
    }
}

void screens_enter(int st)
{
    if(st == ST_STAGECLEAR) stageclear_enter();
    if(st == ST_PAUSE) {
        s_pause_sel = 0;
        fx_flash(0, 0);         // a flash would otherwise stay frozen over the pause menu
    }
    if(st == ST_TITLE) {
        s_idle = 0;
        bg_init(0);
        if(!title_item_enabled(g_game.sel)) g_game.sel = 0;
    }
    if(st == ST_ENDING || st == ST_CREDITS) bg_init(0);
    if(st == ST_GAMEOVER) {
        s_cont_t = s_quit ? -1 : 0;
        s_quit = false;
    }
    if(st == ST_TITLE) s_quit = false;
}
