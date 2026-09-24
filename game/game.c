// QUASAR - top level: state machine, scoring and the per-frame entry point.
#include "game.h"
#include <string.h>

game_t g_game;
run_t g_run;
uint32_t g_events;
static int s_idle_frames;
static uint32_t s_ms_hist[8];
static int s_ms_i;

void screens_enter(int st);

int difficulty(void)
{
    return g_save.difficulty > 2 ? 1 : g_save.difficulty;
}

void game_set_state(int st)
{
    g_game.prev = g_game.state;
    g_game.state = st;
    g_game.t = 0;
    if(st == ST_TITLE && g_game.prev != ST_OPTIONS && g_game.prev != ST_SCORES &&
       g_game.prev != ST_HOWTO && g_game.prev != ST_PRACTICE) {
        g_game.sel = 0;
    }
    screens_enter(st);
}

void add_score(uint32_t pts, float x, float y, bool show)
{
    g_run.score += pts;
    if(g_run.score > 99999999u) g_run.score = 99999999u;
    if(show) {
        char buf[12];
        fmt_int(buf, (long)pts);
        fx_text(x, y - 6, buf, g_run.mult >= 4 ? C_PINK : C_WHITE);
    }
    if(!g_run.practice && g_run.score >= g_run.next_life) {
        g_run.next_life += g_run.next_life < 150000 ? 100000 : 200000;
        g_pl.lives++;
        fx_text(g_pl.x, g_pl.y - 20, "EXTEND!", C_GREEN);
        fx_ring(g_pl.x, g_pl.y, 6, 3.0f, C_GREEN, 16, 3);
    }
}

void chain_kill(void)
{
    g_run.chain++;
    g_run.chain_timer = 45;
    int m = 1 + g_run.chain / 10;
    if(m > 8) m = 8;
    if(m > g_run.mult) {
        char buf[8] = "x";
        fmt_int(buf + 1, m);
        fx_text(g_pl.x + 20, g_pl.y - 16, buf, C_GOLD);
    }
    g_run.mult = m;
    if(g_run.chain > g_run.best_chain) g_run.best_chain = g_run.chain;
}

void chain_break(void)
{
    g_run.chain = 0;
    g_run.chain_timer = 0;
    g_run.mult = 1;
}

void game_set_battery(int level)
{
    g_game.battery = level;
}

void game_set_fps(int fps10)
{
    g_game.fps10 = fps10;
}

int game_brightness(void)
{
    return g_save.brightness;
}

int game_vsync(void)
{
    return g_save.vsync;
}

bool game_wants_idle_off(void)
{
    // ten minutes without a key press, and not in the middle of a fight
    return s_idle_frames > 30 * 60 * 10 && g_game.state != ST_PLAY;
}

void game_init(uint32_t seed)
{
    memset(&g_game, 0, sizeof(g_game));
    memset(&g_run, 0, sizeof(g_run));
    g_rng.s = seed ? seed : 0x2545f491u;
    g_fxrng.s = (seed * 2654435761u) ^ 0x9e3779b9u;
    if(!g_fxrng.s) g_fxrng.s = 1;
    save_defaults();
    g_game.battery = -1;
    g_run.mult = 1;
    fx_reset();
    enemies_clear();
    pickups_clear();
    boss_clear();
    g_game.state = ST_TITLE;
    screens_enter(ST_TITLE);
}

void game_debug_start(int stage, int diff)
{
    g_save.difficulty = (uint8_t)iclamp(diff, 0, 2);
    game_start_run(stage < 1 ? 1 : (stage > 5 ? 5 : stage), false);
}

static void world_update(bool with_player)
{
    bg_update();
    if(g_game.state == ST_PLAY) stage_update();
    if(with_player) player_update();
    pshots_update();
    enemies_update();
    boss_update();
    eshots_update();
    pickups_update();
    fx_update();
}

static void world_draw(void)
{
    bg_draw();
    pickups_draw();
    enemies_draw();
    boss_draw();
    pshots_draw();
    player_draw();
    fx_draw();
    eshots_draw();
    bg_draw_front();
    fx_draw_top();
    stage_draw_overlay();
    hud_draw();
}

static bool world_state(int st)
{
    return st == ST_PLAY || st == ST_PAUSE || st == ST_GAMEOVER || st == ST_STAGECLEAR || st == ST_CONFIRM_QUIT;
}

uint32_t game_frame(uint64_t keys)
{
    g_events = 0;
    g_game.frame++;
    g_game.t++;
    input_update(keys);

    if(g_in.raw_pressed) s_idle_frames = 0;
    else s_idle_frames++;

    // power key: tap pauses, hold switches off
    if(g_in.power_hold == 1 && g_game.state == ST_PLAY) game_set_state(ST_PAUSE);
    if(g_in.power_hold == 40) {
        screens_before_off();
        g_events |= EV_POWEROFF | EV_SAVE;
    }

    switch(g_game.state) {
        case ST_PLAY:
            g_run.frames++;
            if(g_in.pressed & B_PAUSE) {
                game_set_state(ST_PAUSE);
                break;
            }
            if(g_hitstop > 0) {
                g_hitstop--;
                break;
            }
            if(g_run.chain_timer > 0 && --g_run.chain_timer == 0) chain_break();
            world_update(true);
            break;
        case ST_GAMEOVER:
            world_update(false);
            screens_update();
            break;
        case ST_STAGECLEAR:
            bg_update();
            pickups_update();
            fx_update();
            player_update();
            pshots_update();
            screens_update();
            break;
        case ST_PAUSE:
        case ST_CONFIRM_QUIT:
            screens_update();
            break;
        default:
            bg_update();
            fx_update();
            screens_update();
            break;
    }

    if(world_state(g_game.state)) {
        world_draw();
        if(g_game.state != ST_PLAY) screens_draw();
    } else {
        screens_draw();
        fx_draw_top();
    }

    // the host switches off after this frame: keep a pending high score
    if(game_wants_idle_off()) screens_before_off();

    // frame rate, averaged over 8 frames
    uint32_t now = plat_millis();
    s_ms_hist[s_ms_i++ & 7] = now;
    uint32_t span = now - s_ms_hist[s_ms_i & 7];
    if(span > 0 && g_game.frame > 8) g_game.fps10 = (int)(70000u / span);

    return g_events;
}
