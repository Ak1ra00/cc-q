// QUASAR - headless runner: boots the firmware's home screen (or goes straight
// into a game), plays with an autopilot or a key script, and writes frames to
// disk. Used for tests and for the README screenshots.
//
//   quasar_headless [--frames N] [--shot F[,F...]] [--out DIR] [--bot]
//                   [--stage S] [--diff D] [--seed X] [--keys "F:KEY,..."]
//                   [--quasar] [--tetris MODE] [--pac MODE]
//
//   --stage S     QUASAR on its own, as before the home screen, from stage S
//   --quasar      QUASAR on its own, from its title
//   --tetris M    TETRIS, a game of mode M (0 marathon, 1 sprint, 2 ultra; -1 its menu)
//   --pac M       PAC-MAN, a game of mode M (0 classic, 1 neon; -1 its menu)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../game/arcade.h"

static px_t fb[FB_PIX];
static uint32_t s_ms;

uint32_t plat_millis(void) { return s_ms; }

void game_debug_start(int stage, int diff);

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if(!f) return;
    fprintf(f, "P6 %d %d 255\n", SCR_W, SCR_H);
    for(int y = 0; y < SCR_H; y++) {
        for(int x = 0; x < SCR_W; x++) {
            uint8_t r, g, b;
            px_to_rgb(fb[FBI(x, y)], &r, &g, &b);
            fputc(r, f);
            fputc(g, f);
            fputc(b, f);
        }
    }
    fclose(f);
}

static const struct { const char *name; int key; } KEYNAMES[] = {
    { "LEFT", K_LEFT }, { "RIGHT", K_RIGHT }, { "UP", K_UP }, { "DOWN", K_DOWN },
    { "ENTER", K_ENTER }, { "CANCEL", K_CANCEL }, { "TAB", K_TAB }, { "SPACE", K_SPACE },
    { "DEL", K_DEL }, { "POWER", K_POWER }, { "P", K_P }, { "A", K_A }, { "Q", K_Q },
    { "S", K_S }, { "R", K_R }, { "K", K_K }, { "I", K_I }, { "E", K_E }, { "Y", K_Y }, { "N", K_N },
    { "O", K_O }, { "V", K_V }, { "B", K_B }, { "T", K_T }, { "1", K_1 }, { "2", K_2 },
    { "X", K_X }, { "Z", K_Z }, { "C", K_C }, { "SHIFT", K_SHIFT }, { "D", K_D }, { "M", K_M },
};

typedef struct { int frame, len; uint64_t keys; } press_t;
static press_t s_script[256];
static int s_nscript;

static int keycode(const char *n)
{
    for(unsigned i = 0; i < sizeof(KEYNAMES) / sizeof(KEYNAMES[0]); i++) {
        if(!strcmp(KEYNAMES[i].name, n)) return KEYNAMES[i].key;
    }
    return -1;
}

// "F:KEY[+KEY][*LEN],..."  press keys at frame F for LEN frames (default 2)
static void parse_script(const char *s)
{
    char buf[4096];
    strncpy(buf, s, sizeof(buf) - 1);
    for(char *tok = strtok(buf, ","); tok && s_nscript < 256; tok = strtok(NULL, ",")) {
        press_t *p = &s_script[s_nscript];
        char *colon = strchr(tok, ':');
        if(!colon) continue;
        *colon = 0;
        p->frame = atoi(tok);
        p->len = 2;
        char *star = strchr(colon + 1, '*');
        if(star) {
            *star = 0;
            p->len = atoi(star + 1);
        }
        p->keys = 0;
        for(char *k = strtok_r(colon + 1, "+", &star); k; k = strtok_r(NULL, "+", &star)) {
            int code = keycode(k);
            if(code >= 0) p->keys |= KEYBIT(code);
        }
        s_nscript++;
    }
}

// ------------------------------------------------------------------ autopilot

static uint64_t bot_keys(int frame)
{
    player_t *p = &g_pl;
    uint64_t k = 0;
    if(g_game.state != ST_PLAY) {
        // keep things moving through menus and results
        if(g_game.state == ST_STAGECLEAR && g_game.t > 130 && (frame & 7) == 0) k |= KEYBIT(K_ENTER);
        if(g_game.state == ST_GAMEOVER && g_game.t > 20 && (frame & 15) == 0) k |= KEYBIT(K_ENTER);
        if(g_game.state == ST_NAME && g_game.t > 30) {
            if(g_game.name_len < 3 && (frame & 3) == 0) k |= KEYBIT(K_B + (g_game.name_len % 2));
            if(g_game.name_len >= 3 && (frame & 7) == 0) k |= KEYBIT(K_ENTER);
        }
        return k;
    }
    if(!p->alive || p->entering) return 0;

    // pick a target height: nearest enemy or the boss core
    float ty = 120, best = 1e9f;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive || e->t < 0 || e->x < p->x || e->x > SCR_W) continue;
        float d = (e->x - p->x) + (e->y > p->y ? e->y - p->y : p->y - e->y) * 2;
        if(d < best) {
            best = d;
            ty = e->y;
        }
    }
    if(g_boss.active && g_boss.entered) ty = g_boss.part[0].y;

    // danger field from bullets ahead
    float push = 0, pushx = 0;
    int threat = 0;
    for(int i = 0; i < MAX_ESHOTS; i++) {
        eshot_t *b = &g_eshots[i];
        if(!b->alive) continue;
        // predicted position a few frames ahead
        for(int f = 2; f <= 10; f += 4) {
            float bx = b->x + b->vx * (float)f, by = b->y + b->vy * (float)f;
            float dx = bx - p->x, dy = by - p->y;
            if(dx * dx + dy * dy < 22 * 22) {
                push += dy > 0 ? -1.0f : 1.0f;
                pushx += dx > 0 ? -0.5f : 0.5f;
                threat++;
            }
        }
    }
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive) continue;
        float dx = e->x - p->x, dy = e->y - p->y;
        if(dx > -10 && dx < 50 && dy * dy < 400) {
            push += dy > 0 ? -1.5f : 1.5f;
            pushx -= 1;
        }
    }

    float want_y = ty;
    if(threat) want_y = p->y + push * 20;
    if(want_y < p->y - 3) k |= KEYBIT(K_UP);
    if(want_y > p->y + 3) k |= KEYBIT(K_DOWN);
    float want_x = 80 + pushx * 12;
    if(g_boss.active) want_x = 60 + pushx * 10;
    if(p->x < want_x - 6) k |= KEYBIT(K_RIGHT);
    if(p->x > want_x + 6) k |= KEYBIT(K_LEFT);

    if(threat >= 6 && p->bombs > 0 && p->invuln == 0 && (frame % 4) == 0) k |= KEYBIT(K_CANCEL);
    // charge beams on bosses now and then
    if(g_boss.active && (frame % 120) < 70 && !threat) k |= KEYBIT(K_ENTER);
    return k;
}

int main(int argc, char **argv)
{
    int frames = 300, stage = 0, diff = 1, tetris = -2, pac = -2;
    uint32_t seed = 12345;
    bool bot = false, quasar_only = false, trace = false;
    const char *out = ".";
    char shots[4096] = "";

    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--shot") && i + 1 < argc) strncpy(shots, argv[++i], sizeof(shots) - 1);
        else if(!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if(!strcmp(argv[i], "--bot")) bot = true;
        else if(!strcmp(argv[i], "--stage") && i + 1 < argc) stage = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--diff") && i + 1 < argc) diff = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if(!strcmp(argv[i], "--keys") && i + 1 < argc) parse_script(argv[++i]);
        else if(!strcmp(argv[i], "--quasar")) quasar_only = true;
        else if(!strcmp(argv[i], "--tetris") && i + 1 < argc) tetris = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--pac") && i + 1 < argc) pac = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--trace")) trace = true;
    }
    // QUASAR's own frame loop, exactly as it ran before the home screen existed
    bool direct = quasar_only || stage > 0;

    gfx_set_target(fb);
    if(direct) {
        game_init(seed);
        if(stage > 0) game_debug_start(stage, diff);
    } else {
        arcade_init(seed);
        if(tetris >= -1) arcade_debug_start(GAME_TETRIS, tetris);
        if(pac >= -1) arcade_debug_start(GAME_PAC, pac);
    }

    int shot_list[512], nshots = 0;
    for(char *t = strtok(shots, ","); t && nshots < 512; t = strtok(NULL, ",")) shot_list[nshots++] = atoi(t);

    int deaths = 0, last_lives = g_pl.lives, max_eshots = 0;
    long long ev_all = 0;
    clock_t t0 = clock();
    for(int f = 1; f <= frames; f++) {
        s_ms += 33;
        uint64_t keys = 0;
        for(int i = 0; i < s_nscript; i++) {
            if(f >= s_script[i].frame && f < s_script[i].frame + s_script[i].len) keys |= s_script[i].keys;
        }
        if(bot) {
            if(!direct && arcade_app() == APP_TETRIS) keys |= tetris_bot_keys(1);
            else if(!direct && arcade_app() == APP_PAC) keys |= pac_bot_keys();
            else keys |= bot_keys(f);
        }
        int phase0 = !direct && arcade_app() == APP_PAC ? pac_debug_game()->phase : -1;
        uint32_t ev = direct ? game_frame(keys) : arcade_frame(keys);
        if(trace && phase0 >= 0 && arcade_app() == APP_PAC && pac_debug_game()->phase != phase0) {
            const pgame_t *g = pac_debug_game();
            printf("frame %d: phase %d -> %d  level %d lives %d score %u screen %d\n", f, phase0, g->phase, g->level,
                   g->lives, g->score, pac_debug_screen());
        }
        ev_all |= ev;
        int ne = eshots_count();
        if(ne > max_eshots) max_eshots = ne;
        if(g_pl.lives < last_lives) deaths++;
        last_lives = g_pl.lives;
        for(int i = 0; i < nshots; i++) {
            if(shot_list[i] == f) {
                char path[512];
                snprintf(path, sizeof(path), "%s/frame_%05d.ppm", out, f);
                write_ppm(path);
            }
        }
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    if(!direct && arcade_app() != APP_QUASAR) {
        uint32_t score;
        int lines, level;
        bool over;
        int st = arcade_app() == APP_TETRIS ? tetris_debug(&score, &lines, &level, &over) : -1;
        if(arcade_app() == APP_PAC) {
            const pgame_t *g = pac_debug_game();
            st = pac_debug_screen();
            score = g->score;
            lines = g->lives;
            level = g->level;
            over = g->over;
        }
        printf("frames=%d app=%d screen=%d score=%u lines=%d level=%d over=%d events=0x%llx host_ms_per_frame=%.3f\n",
               frames, arcade_app(), st, st >= 0 ? score : 0, st >= 0 ? lines : 0, st >= 0 ? level : 0,
               st >= 0 ? over : 0, ev_all, secs * (double)1000 / (double)frames);
        return 0;
    }
    printf("frames=%d state=%d stage=%d score=%u lives=%d hp=%d deaths=%d max_bullets=%d events=0x%llx host_ms_per_frame=%.3f\n",
           frames, g_game.state, g_stage.num, g_run.score, g_pl.lives, g_pl.hp, deaths, max_eshots, ev_all,
           secs * (double)1000 / (double)frames);
    return 0;
}
