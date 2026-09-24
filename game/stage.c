// QUASAR - stage timelines and wave spawning.
#include "game.h"
#include <string.h>

stage_t g_stage;

const char *const STAGE_NAMES[6] = {
    "", "FROST BELT", "CRIMSON NEBULA", "DYSON ARRAY", "SOLAR CORONA", "THE QUASAR",
};

enum {
    W_END = 0, W_DRONE_LINE, W_DRONE_V, W_DARTERS, W_FIGHTERS, W_POD, W_MINES, W_ROCKS,
    W_GUNSHIP, W_CARRIER, W_SPINNERS, W_WEAVERS, W_JELLY, W_TURRETS, W_WISPS, W_EYES,
    W_SEEKERS, W_CARGO, W_DARTER_WALL,
};

typedef struct {
    uint16_t t;
    uint8_t kind, n;
    int16_t y, a, b;        // meaning depends on kind; b is often a capsule to drop
} wave_t;

// ---------------------------------------------------------------- timelines (frames @ 30fps)

static const wave_t STAGE1[] = {
    {   60, W_DRONE_LINE, 5,  70, 22, PU_POWER },
    {  190, W_DRONE_LINE, 5, 170, 22, 0 },
    {  320, W_DARTERS,    4, 120, 0, 0 },
    {  450, W_ROCKS,      3, 0, 0, 0 },
    {  580, W_DRONE_V,    7, 120, 0, PU_SPREAD },
    {  720, W_FIGHTERS,   2,  80, 0, 0 },
    {  780, W_FIGHTERS,   2, 170, 0, 0 },
    {  920, W_CARGO,      1, 120, 0, PU_POWER },
    { 1020, W_MINES,      5, 0, 0, 0 },
    { 1160, W_DARTERS,    6,  60, 0, 0 },
    { 1220, W_DARTERS,    6, 180, 0, 0 },
    { 1370, W_POD,        1,  80, 0, 0 },
    { 1400, W_POD,        1, 170, 0, 0 },
    { 1540, W_ROCKS,      5, 0, 0, 0 },
    { 1700, W_WEAVERS,    8, 120, 60, PU_LASER },
    { 1900, W_FIGHTERS,   3, 120, 0, 0 },
    { 2000, W_DRONE_LINE, 6,  60, 30, 0 },
    { 2060, W_DRONE_LINE, 6, 180, 30, 0 },
    { 2200, W_GUNSHIP,    1, 120, 0, PU_BOMB },
    { 2520, W_ROCKS,      6, 0, 0, 0 },
    { 2680, W_CARGO,      1,  90, 0, PU_OPTION },
    { 2780, W_DARTER_WALL,8, 0, 0, 0 },
    { 2940, W_SPINNERS,   3, 0, 0, 0 },
    { 3080, W_FIGHTERS,   4, 120, 0, 0 },
    { 3260, W_DRONE_V,    9, 120, 0, PU_HEAL },
    { 3420, W_END,        0, 0, 0, 0 },
};

static const wave_t STAGE2[] = {
    {   60, W_WEAVERS,    8,  80, 50, PU_POWER },
    {  220, W_JELLY,      2, 0, 0, 0 },
    {  400, W_DARTERS,    6, 120, 0, 0 },
    {  520, W_SEEKERS,    3, 0, 0, 0 },
    {  680, W_WEAVERS,    8, 160, 50, PU_MISSILE },
    {  850, W_JELLY,      3, 0, 0, 0 },
    { 1000, W_CARGO,      1, 120, 0, PU_POWER },
    { 1080, W_SPINNERS,   4, 0, 0, 0 },
    { 1250, W_FIGHTERS,   4, 120, 0, 0 },
    { 1400, W_DRONE_V,    9, 120, 0, PU_BOMB },
    { 1560, W_SEEKERS,    5, 0, 0, 0 },
    { 1700, W_JELLY,      2, 0, 0, 0 },
    { 1760, W_WEAVERS,   10, 120, 70, PU_OPTION },
    { 1980, W_GUNSHIP,    1,  90, 0, PU_HEAL },
    { 2250, W_DARTER_WALL,10, 0, 0, 0 },
    { 2400, W_MINES,      6, 0, 0, 0 },
    { 2560, W_JELLY,      3, 0, 0, 0 },
    { 2700, W_SEEKERS,    6, 0, 0, 0 },
    { 2880, W_CARGO,      1, 150, 0, PU_POWER },
    { 2950, W_FIGHTERS,   5, 120, 0, 0 },
    { 3150, W_SPINNERS,   5, 0, 0, 0 },
    { 3350, W_END,        0, 0, 0, 0 },
};

static const wave_t STAGE3[] = {
    {   60, W_DRONE_LINE, 6, 120, 40, PU_POWER },
    {  240, W_FIGHTERS,   3, 120, 0, 0 },
    {  500, W_TURRETS,    4, 0, 0, 0 },
    {  700, W_POD,        1, 120, 0, 0 },
    {  820, W_TURRETS,    5, 0, 0, 0 },
    {  980, W_CARGO,      1, 120, 0, PU_LASER },
    { 1100, W_DARTERS,    6, 120, 0, 0 },
    { 1250, W_TURRETS,    6, 0, 0, 0 },
    { 1400, W_MINES,      5, 0, 0, 0 },
    { 1560, W_GUNSHIP,    1, 120, 0, PU_BOMB },
    { 1800, W_TURRETS,    6, 0, 0, 0 },
    { 1950, W_SEEKERS,    4, 0, 0, 0 },
    { 2100, W_DRONE_V,    9, 120, 0, PU_HEAL },
    { 2250, W_TURRETS,    6, 0, 0, 0 },
    { 2400, W_FIGHTERS,   5, 120, 0, 0 },
    { 2560, W_CARRIER,    1, 120, 0, PU_LIFE },
    { 3000, W_TURRETS,    6, 0, 0, 0 },
    { 3120, W_CARGO,      1, 100, 0, PU_POWER },
    { 3250, W_END,        0, 0, 0, 0 },
};

static const wave_t STAGE4[] = {
    {   60, W_WISPS,      4, 0, 0, 0 },
    {  200, W_FIGHTERS,   4, 120, 0, PU_POWER },
    {  380, W_ROCKS,      5, 0, 0, 0 },
    {  540, W_WISPS,      6, 0, 0, 0 },
    {  700, W_SEEKERS,    5, 0, 0, 0 },
    {  860, W_CARGO,      1, 120, 0, PU_MISSILE },
    {  960, W_GUNSHIP,    1,  80, 0, 0 },
    { 1000, W_GUNSHIP,    1, 170, 0, PU_BOMB },
    { 1300, W_WISPS,      8, 0, 0, 0 },
    { 1450, W_DARTER_WALL,10, 0, 0, 0 },
    { 1600, W_ROCKS,      6, 0, 0, 0 },
    { 1760, W_WEAVERS,   10, 120, 80, PU_OPTION },
    { 1950, W_SPINNERS,   5, 0, 0, 0 },
    { 2100, W_CARRIER,    1, 120, 0, PU_HEAL },
    { 2500, W_WISPS,      8, 0, 0, 0 },
    { 2650, W_FIGHTERS,   6, 120, 0, 0 },
    { 2820, W_CARGO,      1, 140, 0, PU_POWER },
    { 2900, W_SEEKERS,    6, 0, 0, 0 },
    { 3100, W_END,        0, 0, 0, 0 },
};

static const wave_t STAGE5[] = {
    {   60, W_EYES,       2, 0, 0, 0 },
    {  250, W_WEAVERS,   10,  70, 50, PU_POWER },
    {  400, W_WEAVERS,   10, 170, 50, 0 },
    {  560, W_SPINNERS,   5, 0, 0, 0 },
    {  720, W_JELLY,      3, 0, 0, 0 },
    {  880, W_CARGO,      1, 120, 0, PU_BOMB },
    {  960, W_SEEKERS,    6, 0, 0, 0 },
    { 1100, W_EYES,       3, 0, 0, 0 },
    { 1300, W_FIGHTERS,   6, 120, 0, 0 },
    { 1460, W_WISPS,      6, 0, 0, 0 },
    { 1600, W_GUNSHIP,    1, 120, 0, PU_HEAL },
    { 1900, W_DRONE_V,   11, 120, 0, PU_OPTION },
    { 2050, W_MINES,      7, 0, 0, 0 },
    { 2200, W_EYES,       3, 0, 0, 0 },
    { 2400, W_DARTER_WALL,12, 0, 0, 0 },
    { 2560, W_CARRIER,    1, 120, 0, PU_POWER },
    { 2950, W_SPINNERS,   6, 0, 0, 0 },
    { 3100, W_CARGO,      1, 120, 0, PU_HEAL },
    { 3200, W_END,        0, 0, 0, 0 },
};

static const wave_t *const TIMELINES[6] = { NULL, STAGE1, STAGE2, STAGE3, STAGE4, STAGE5 };

static int s_group_next;

static int next_group(int count, int drop)
{
    int g = s_group_next;
    s_group_next = (s_group_next + 1) % 16;
    group_start(g, count, drop);
    return g;
}

static void spawn_wave(const wave_t *w)
{
    int n = w->n;
    // a little more pressure on higher difficulty
    if(difficulty() == 2 && n > 2 && w->kind != W_CARGO) n += n / 3;
    if(difficulty() == 0 && n > 3) n -= n / 4;

    switch(w->kind) {
        case W_DRONE_LINE: {
            int g = w->b ? next_group(n, w->b) : -1;
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_DRONE, (float)(SCR_W + 16 + i * 26), (float)w->y);
                if(!e) break;
                e->vx = -1.9f;
                e->p[0] = (float)w->y;
                e->p[1] = (float)w->a;
                e->p[2] = 1.0f / 90.0f;
                e->p[3] = (float)i * 0.08f;
                e->group = (int8_t)g;
            }
            break;
        }
        case W_DRONE_V: {
            int g = w->b ? next_group(n, w->b) : -1;
            for(int i = 0; i < n; i++) {
                int k = i - n / 2;
                enemy_t *e = enemy_spawn(E_DRONE, (float)(SCR_W + 16 + iabs(k) * 18), (float)(w->y + k * 16));
                if(!e) break;
                e->vx = -1.6f;
                e->p[0] = e->y;
                e->p[1] = 6;
                e->p[2] = 1.0f / 60.0f;
                e->p[3] = (float)i * 0.1f;
                e->group = (int8_t)g;
            }
            break;
        }
        case W_DARTERS:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_DARTER, (float)(SCR_W + 10 + i * 22), (float)(w->y + ((i & 1) ? 24 : -24)));
                if(!e) break;
                e->vx = -3.0f;
                e->p[0] = (float)(20 + i * 4);
            }
            break;
        case W_DARTER_WALL:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_DARTER, (float)(SCR_W + 10 + (i % 2) * 20), (float)(30 + (i * 190) / (n > 1 ? n - 1 : 1)));
                if(!e) break;
                e->vx = -2.4f;
                e->p[0] = 25;
            }
            break;
        case W_FIGHTERS: {
            // close up the formation only if it would not fit on screen (big HARD waves)
            int sp = 34;
            while(sp > 16 && (w->y - ((n - 1) / 2) * sp < 22 || w->y + ((n - 1) - (n - 1) / 2) * sp > 226)) sp -= 2;
            for(int i = 0; i < n; i++) {
                float y = (float)w->y + (float)((i - (n - 1) / 2) * sp);
                enemy_t *e = enemy_spawn(E_FIGHTER, (float)(SCR_W + 20 + i * 14), y);
                if(!e) break;
                e->p[0] = (float)(220 - (i % 2) * 40);
                e->p[1] = 2 + (float)difficulty();
                e->p[2] = y < 120 ? -1.0f : 1.0f;
            }
            break;
        }
        case W_POD: {
            enemy_t *e = enemy_spawn(E_POD, SCR_W + 20, (float)w->y);
            if(e) {
                e->p[0] = 240;
                e->p[1] = 260;
                e->drop = (uint8_t)w->b;
            }
            break;
        }
        case W_MINES:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_MINE, (float)(SCR_W + 16 + i * 40), (float)(40 + rnd(170)));
                if(!e) break;
                e->vx = -1.1f;
                e->p[0] = rndf();
            }
            break;
        case W_ROCKS:
            for(int i = 0; i < n; i++) {
                int type = (i % 3 == 0) ? E_ROCK_L : E_ROCK_M;
                enemy_t *e = enemy_spawn(type, (float)(SCR_W + 20 + i * 45), (float)(30 + rnd(180)));
                if(!e) break;
                e->vx = -rndr(1.0f, 2.0f);
                e->vy = rndr(-0.5f, 0.5f);
                e->p[0] = rndr(-3, 3);
            }
            break;
        case W_GUNSHIP: {
            enemy_t *e = enemy_spawn(E_GUNSHIP, SCR_W + 30, (float)w->y);
            if(e) {
                e->p[0] = 250;
                e->p[1] = 420;
                e->drop = (uint8_t)w->b;
            }
            break;
        }
        case W_CARRIER: {
            enemy_t *e = enemy_spawn(E_CARRIER, SCR_W + 40, (float)w->y);
            if(e) {
                e->p[0] = 250;
                e->p[1] = 600;
                e->drop = (uint8_t)w->b;
            }
            break;
        }
        case W_SPINNERS:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_SPINNER, (float)(SCR_W + 14 + i * 30), (float)(40 + rnd(160)));
                if(!e) break;
                e->vx = -rndr(1.2f, 2.0f);
                e->vy = rnd(2) ? 1.6f : -1.6f;
            }
            break;
        case W_WEAVERS: {
            int g = w->b ? next_group(n, w->b) : -1;
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_WEAVER, SCR_W + 40, (float)w->y);
                if(!e) break;
                e->t = (int16_t)(-i * 9);
                e->p[0] = SCR_W + 12;
                e->p[1] = (float)w->y;
                e->p[2] = (float)w->a;
                e->p[3] = 1.7f;
                e->group = (int8_t)g;
            }
            break;
        }
        case W_JELLY:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_JELLY, (float)(SCR_W + 20 + i * 60), (float)(50 + i * 70 % 150));
                if(!e) break;
                e->vx = -0.8f;
                e->p[0] = e->y;
                e->cd = (int16_t)(30 + i * 20);
            }
            break;
        case W_TURRETS:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_TURRET, (float)(SCR_W + 16 + i * 52), 0);
                if(!e) break;
                if(i & 1) e->flags |= EF_FLIPY;
            }
            break;
        case W_WISPS:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_WISP, (float)(SCR_W + 10 + (i % 3) * 20), (float)(30 + rnd(180)));
                if(!e) break;
                e->vx = -2;
                e->vy = 0;
                e->t = (int16_t)(-i * 12);
            }
            break;
        case W_EYES:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_EYE, (float)(SCR_W + 20 + i * 30), (float)(50 + (i * 150) / (n > 1 ? n - 1 : 1)));
                if(!e) break;
                e->p[0] = (float)(250 - i * 20);
            }
            break;
        case W_SEEKERS:
            for(int i = 0; i < n; i++) {
                enemy_t *e = enemy_spawn(E_SEEKER, (float)(SCR_W + 10 + i * 24), (float)(30 + rnd(180)));
                if(!e) break;
                e->vx = -3;
                e->t = (int16_t)(-i * 10);
            }
            break;
        case W_CARGO: {
            enemy_t *e = enemy_spawn(E_CAPSULE_CARRIER, SCR_W + 12, (float)w->y);
            if(e) {
                e->vx = -1.2f;
                e->p[0] = (float)w->y;
                e->drop = (uint8_t)w->b;
            }
            break;
        }
        default:
            break;
    }
}

void stage_start(int num)
{
    memset(&g_stage, 0, sizeof(g_stage));
    g_stage.num = num;
    g_stage.score_at_start = g_run.score;
    s_group_next = 0;
    bg_init(num);
    enemies_clear();
    pickups_clear();
    boss_clear();
    fx_reset();
    player_reset(false);
    if(num > g_save.max_stage && !g_run.practice) {
        g_save.max_stage = (uint8_t)num;
        g_events |= EV_SAVE;
    }
    if(num >= 2 && !g_run.practice) run_checkpoint();
}

void stage_update(void)
{
    stage_t *s = &g_stage;
    s->t++;

    if(!s->boss_phase) {
        const wave_t *tl = TIMELINES[s->num];
        while(tl[s->wave_i].kind != W_END && tl[s->wave_i].t <= s->t) {
            spawn_wave(&tl[s->wave_i]);
            s->wave_i++;
        }
        if(tl[s->wave_i].kind == W_END && s->t >= tl[s->wave_i].t) {
            // wait for the field to clear (or give up waiting), then warn
            if(s->warning_t == 0 && (enemies_alive() == 0 || s->t > tl[s->wave_i].t + 360)) {
                s->warning_t = 1;
            }
        }
        if(s->warning_t > 0) {
            s->warning_t++;
            if(s->warning_t == 2) fx_flash(COL(120, 0, 0), 10);
            if(s->warning_t > 110) {
                s->boss_phase = true;
                boss_start(s->num);
            }
        }
    } else if(!g_boss.active) {
        s->clear_t++;
        if(s->clear_t == 1) {
            // pull remaining gems in
            for(int i = 0; i < MAX_PICKUPS; i++) g_pu[i].magnet = 1;
        }
        if(s->clear_t > 70) game_set_state(ST_STAGECLEAR);
    }
}

void stage_draw_overlay(void)
{
    stage_t *s = &g_stage;
    char buf[32];

    // stage intro banner
    if(s->t < 150 && !s->boss_phase) {
        int t = s->t;
        int slide = t < 20 ? (20 - t) * 16 : (t > 130 ? (t - 130) * 16 : 0);
        int y = 88;
        gfx_fill_mode(0, y - 6, SCR_W, 58, 0, DM_SHADOW);
        gfx_hline(0, SCR_W - 1, y - 6, px_scale(C_CYAN, 18));
        gfx_hline(0, SCR_W - 1, y + 51, px_scale(C_CYAN, 18));
        strcpy(buf, "STAGE ");
        fmt_int(buf + 6, s->num);
        text_draw_fx(SCR_W / 2 - text_width(buf, 2) / 2 - slide, y, buf, C_CYAN, 2, TX_SHADOW, NULL);
        text_draw_fx(SCR_W / 2 - text_width(STAGE_NAMES[s->num], 3) / 2 + slide, y + 22, STAGE_NAMES[s->num], C_WHITE, 3, TX_OUTLINE, NULL);
        if(!g_run.practice && g_save.run.stage == s->num) text_center(y + 60, "RUN SAVED", C_GREY, 1, TX_SHADOW);
    }

    // boss warning
    if(s->warning_t > 0 && s->warning_t < 110) {
        int t = s->warning_t;
        bool on = (t >> 3) & 1;
        int y = 96;
        gfx_fill_alpha(0, y - 10, SCR_W, 56, COL(60, 0, 8), 18);
        for(int x = -(t * 2 % 24); x < SCR_W; x += 24) {
            gfx_fill(x, y - 10, 12, 4, COL(255, 60, 40));
            gfx_fill(x + 12, y + 42, 12, 4, COL(255, 60, 40));
        }
        if(on) text_center(y, "WARNING", COL(255, 70, 60), 4, TX_OUTLINE);
        text_center(y + 34, "MASSIVE SIGNATURE APPROACHING", C_WHITE, 1, TX_SHADOW);
    }
}
