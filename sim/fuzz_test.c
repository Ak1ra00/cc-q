// Q ARCADE - randomised tests: a monkey mashing keys across the home screen,
// QUASAR and TETRIS with the invariants checked every frame, power cuts and
// reboots at random moments, damaged save files (with good checksums, so the
// field checks are what gets tested), and the TETRIS rules under random play.
//
//   fuzz_test [frames] [seed]      default 400000 frames; make fuzz runs longer
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../game/arcade.h"

static px_t fb[FB_PIX];
static uint32_t s_ms;
static int s_fail;
static uint32_t s_rng = 1;

uint32_t plat_millis(void) { return s_ms; }

static uint32_t rnd32(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int rnd_n(int n) { return (int)(rnd32() % (uint32_t)n); }

#define FAIL(...) do { \
    if(s_fail < 20) { printf("[FAIL] "); printf(__VA_ARGS__); printf("  (%s:%d)\n", __FILE__, __LINE__); } \
    s_fail++; \
} while(0)

// ------------------------------------------------------------------ invariants

static bool name_ok(const char *s)
{
    int n = 0;
    for(; n <= NAME_LEN && s[n]; n++) {
        if(s[n] < 32 || s[n] > 126) return false;
    }
    return n <= NAME_LEN;
}

static void check_game(const tgame_t *g, const char *where)
{
    for(int y = 0; y < TB_H; y++) {
        for(int x = 0; x < TB_W; x++) {
            if(g->cell[y][x] >= PC_KINDS) FAIL("%s: cell %d,%d = %d", where, x, y, g->cell[y][x]);
        }
    }
    if(g->active) {
        if(!tet_fits(g, g->type, g->rot, g->x, g->y)) FAIL("%s: the falling piece overlaps", where);
        if(g->clear_n) FAIL("%s: a piece is falling while rows wait to clear", where);
        for(int y = 0; y < TB_H; y++) {
            int k = 0;
            for(int x = 0; x < TB_W; x++) k += g->cell[y][x] != 0;
            if(k == TB_W) FAIL("%s: row %d is full while a piece falls", where, y);
        }
    }
    if(g->level < 1 || g->level > MAX_LEVEL) FAIL("%s: level %d", where, g->level);
    if(g->hold > PC_L) FAIL("%s: hold %d", where, g->hold);
    for(int i = 0; i < NEXT_N; i++) {
        if(g->next[i] < PC_I || g->next[i] > PC_L) FAIL("%s: next[%d] = %d", where, i, g->next[i]);
    }
    if(g->score > 99999999u) FAIL("%s: score %u", where, g->score);
    if(g->clear_n > 4) FAIL("%s: clear_n %d", where, g->clear_n);
}

static void check_saved_state(const char *where)
{
    if(g_arc.last_game >= NUM_GAMES) FAIL("%s: last_game %d", where, g_arc.last_game);
    if(g_arc.das > 3 || g_arc.ghost > 1 || g_arc.grid > 1 || g_arc.upkey > 1) FAIL("%s: options out of range", where);
    if(g_arc.start_level < 1 || g_arc.start_level > 15) FAIL("%s: start level %d", where, g_arc.start_level);
    if(!name_ok(g_arc.name)) FAIL("%s: last name", where);
    for(int m = 0; m < TM_COUNT; m++) {
        for(int i = 0; i < TREC_N; i++) {
            const trec_t *r = &g_arc.rec[m][i];
            if(!name_ok(r->name)) FAIL("%s: record name %d/%d", where, m, i);
            if(r->level > MAX_LEVEL) FAIL("%s: record level", where);
            if(i && m != TM_SPRINT && r->value > g_arc.rec[m][i - 1].value) FAIL("%s: records out of order", where);
        }
    }
    if(g_arc.suspended) {
        tgame_t t;
        if(!tet_unpack(&t, g_arc.susp, TET_PACK_LEN)) FAIL("%s: a saved game that will not continue", where);
        else check_game(&t, "saved game");
    }
    if(g_save.brightness > 4 || g_save.vsync > 2 || g_save.auto_off > 2 || g_save.shake > 1) FAIL("%s: settings", where);
}

static void check_round_trip(void)
{
    // what we write is what we read back, byte for byte
    static uint8_t a[640], b[640];
    arcsave_t keep = g_arc;
    int n = arcade_save_pack(a, sizeof(a));
    if(!n || !arcade_save_unpack(a, n)) {
        FAIL("arcade.sav does not read back");
    } else {
        int n2 = arcade_save_pack(b, sizeof(b));
        if(n2 != n || memcmp(a, b, (size_t)n)) FAIL("arcade.sav changes on a round trip");
        if(g_arc.suspended != keep.suspended) FAIL("the saved game is lost on a round trip");
    }
    g_arc = keep;
    uint8_t q[256];
    savedata_t qs = g_save;
    int qn = save_pack(q, sizeof(q));
    if(!qn || !save_unpack(q, qn) || memcmp(&qs, &g_save, sizeof(qs))) FAIL("quasar.sav changes on a round trip");
    g_save = qs;
}

// ------------------------------------------------------------------ the monkey

static uint64_t s_held;
static int s_hold_left;

static uint64_t monkey_keys(void)
{
    static const uint8_t KEYS[] = {
        K_LEFT, K_RIGHT, K_UP, K_DOWN, K_ENTER, K_ENTER, K_CANCEL, K_SPACE, K_X, K_Z, K_C, K_SHIFT,
        K_TAB, K_P, K_DEL, K_R, K_A, K_S, K_D, K_W, K_1, K_Q, K_M, K_NFC, K_QR, K_LEFT, K_RIGHT, K_DOWN,
    };
    if(s_hold_left > 0) {
        s_hold_left--;
        return s_held;
    }
    int r = rnd_n(100);
    if(r < 35) {
        s_held = 0;                                                 // nothing
        s_hold_left = rnd_n(6);
    } else if(r < 98) {
        s_held = KEYBIT(KEYS[rnd_n((int)sizeof(KEYS))]);           // one key, a tap or a hold
        if(rnd_n(4) == 0) s_held |= KEYBIT(KEYS[rnd_n((int)sizeof(KEYS))]);
        s_hold_left = rnd_n(10) == 0 ? 20 + rnd_n(40) : 1 + rnd_n(4);
    } else {
        s_held = KEYBIT(K_POWER);                                   // power: tap, or a hold that switches off
        s_hold_left = rnd_n(3) ? rnd_n(8) : 45;
    }
    return s_held;
}

static void boot(uint32_t seed, const uint8_t *qb, int qn, const uint8_t *ab, int an)
{
    gfx_set_target(fb);
    arcade_init(seed);
    // the host's order: display up, S check, then the two files
    if(qb && !save_unpack(qb, qn)) FAIL("quasar.sav written by us does not load");
    if(ab && !arcade_save_unpack(ab, an)) FAIL("arcade.sav written by us does not load");
    if(ab) arcade_loaded();
}

static void reboot(uint32_t seed)
{
    static uint8_t q[256], a[640];
    int qn = save_pack(q, sizeof(q));
    int an = arcade_save_pack(a, sizeof(a));
    boot(seed, q, qn, a, an);
}

static void monkey(long frames)
{
    boot(rnd32(), NULL, 0, NULL, 0);
    long reboots = 0, apps[3] = { 0 }, games = 0, played = 0;
    int last_st = -1;
    for(long f = 0; f < frames; f++) {
        s_ms += 33;
        uint64_t k = monkey_keys();
        // now and then, play properly for a while, so games get long and records get set
        if(arcade_app() == APP_TETRIS && (f / 3000) % 3 == 2) k = tetris_bot_keys(1) | (k & KEYBIT(K_POWER));
        uint32_t ev = arcade_frame(k);
        int app = arcade_app();
        if(app < 0 || app > 2) FAIL("app %d", app);
        else apps[app]++;
        if(ev & ~(uint32_t)(EV_SAVE | EV_SYSTEM | EV_POWEROFF | EV_BRIGHT | EV_VSYNC | EV_SAVE_ARCADE)) {
            FAIL("unexpected event bits 0x%x", ev);
        }
        if(app == APP_TETRIS) {
            uint32_t sc;
            int l, lv;
            bool over;
            int st = tetris_debug(&sc, &l, &lv, &over);
            if(st == 5) {
                played++;
                check_game(tetris_debug_game(), "playing");
            }
            if(st == 4 && last_st != 4) games++;
            last_st = st;
        }
        if((f & 255) == 0) {
            check_saved_state("monkey");
            check_round_trip();
        }
        // what main.py does: switch off (saving both files first), and later on again
        if((ev & EV_POWEROFF) || arcade_wants_idle_off() || rnd_n(20000) == 0) {
            reboot(rnd32());
            reboots++;
            check_saved_state("after a reboot");
        }
    }
    printf("      monkey: %ld frames, home %ld / quasar %ld / tetris %ld, %ld tetris games started, "
           "%ld frames of tetris play, %ld reboots, %u records saved\n",
           frames, apps[0], apps[1], apps[2], games, played, reboots, g_arc.plays);
}

// ------------------------------------------------------------------ damaged saves

static uint32_t crc32(const uint8_t *p, int n)
{
    uint32_t c = 0xffffffffu;
    for(int i = 0; i < n; i++) {
        c ^= p[i];
        for(int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void put32(uint8_t *p, uint32_t v)
{
    for(int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void save_fuzz(int rounds)
{
    // a real file with a paused game in it
    boot(77, NULL, 0, NULL, 0);
    arcade_debug_start(GAME_TETRIS, TM_MARATHON);
    for(int i = 0; i < 1500; i++) {
        s_ms += 33;
        arcade_frame(tetris_bot_keys(1));
    }
    for(int i = 0; i < 45; i++) arcade_frame(KEYBIT(K_POWER));
    static uint8_t good[640], bad[2048];
    int n = arcade_save_pack(good, sizeof(good));
    if(!g_arc.suspended) FAIL("fuzz: no paused game to start from");
    int body = 4 + 1 + 2 + 6 + NAME_LEN + 8 + TM_COUNT * TREC_N * (NAME_LEN + 7);
    int susp = body + 4, susp_len = 4 + 1 + TET_PACK_LEN;
    uint8_t q[256];
    int qn = save_pack(q, sizeof(q));
    long loaded = 0, continued = 0;
    for(int r = 0; r < rounds; r++) {
        int len = n;
        memcpy(bad, good, (size_t)n);
        int kind = rnd_n(10);
        int flips = 1 + rnd_n(6);
        for(int i = 0; i < flips; i++) {
            int at = kind < 5 ? susp + 5 + rnd_n(TET_PACK_LEN) : rnd_n(n);      // mostly inside the paused game
            if(rnd_n(2)) bad[at] ^= (uint8_t)(1u << rnd_n(8));
            else bad[at] = (uint8_t)rnd32();
        }
        if(kind == 9) len = rnd_n(n + 1);
        if(kind == 8) {
            len = n + rnd_n(600);
            for(int i = n; i < len; i++) bad[i] = (uint8_t)rnd32();
        }
        // mend the checksums most of the time, so the field checks are what is tested
        if(rnd_n(4)) {
            put32(bad + body, crc32(bad, body));
            put32(bad + susp + susp_len, crc32(bad + susp, susp_len));
        }
        gfx_set_target(fb);
        arcade_init(rnd32());
        save_unpack(q, qn);
        if(arcade_save_unpack(bad, len)) {
            loaded++;
            arcade_loaded();
            check_saved_state("fuzzed file");
            // and play on from it: into TETRIS, CONTINUE if there is one, then mash keys
            bool had = g_arc.suspended;
            arcade_debug_start(GAME_TETRIS, -1);
            for(int i = 0; i < 4; i++) arcade_frame(0);
            for(int i = 0; i < 3; i++) arcade_frame(KEYBIT(K_ENTER));
            if(had) continued++;
            for(int i = 0; i < 300; i++) {
                s_ms += 33;
                uint32_t ev = arcade_frame(monkey_keys());
                if(arcade_app() == APP_TETRIS) {
                    uint32_t sc;
                    int l, lv;
                    bool over;
                    if(tetris_debug(&sc, &l, &lv, &over) == 5) check_game(tetris_debug_game(), "fuzzed game");
                }
                if(ev & EV_POWEROFF) break;
            }
            check_saved_state("fuzzed file, played");
        } else if(!name_ok(g_arc.name) || g_arc.suspended) {
            FAIL("fuzz: a refused file still changed the state");
        }
    }
    // and QUASAR's own file, damaged the same way
    long qloaded = 0;
    for(int r = 0; r < rounds / 4; r++) {
        uint8_t qb[512];
        memcpy(qb, q, (size_t)qn);
        for(int i = 0; i < 1 + rnd_n(6); i++) qb[rnd_n(qn)] = (uint8_t)rnd32();
        int body_q = 4 + 1 + 1 + 8 + 4 + NUM_SCORES * (NAME_LEN + 2 + 4 + 2);
        if(rnd_n(4)) put32(qb + body_q, crc32(qb, body_q));
        gfx_set_target(fb);
        arcade_init(rnd32());
        if(save_unpack(qb, qn)) {
            qloaded++;
            check_saved_state("fuzzed quasar.sav");
            for(int i = 0; i < 200; i++) arcade_frame(monkey_keys());
        }
    }
    printf("      save fuzz: %d damaged arcade.sav files, %ld accepted (%ld with a game to continue), "
           "%d damaged quasar.sav, %ld accepted\n", rounds, loaded, continued, rounds / 4, qloaded);
}

// ------------------------------------------------------------------ the rules under random play

static long s_tetrises, s_tspins;

static void rules_fuzz(long frames)
{
    tgame_t g;
    long games = 0, lines = 0, clears = 0;
    uint32_t last_score = 0;
    tet_new(&g, rnd_n(TM_COUNT), 1 + rnd_n(15), rnd32());
    g.das = (uint8_t)(1 + rnd_n(7));
    g.arr = (uint8_t)rnd_n(3);
    tai_t ai;
    memset(&ai, 0, sizeof(ai));
    for(long f = 0; f < frames; f++) {
        // mostly the demo player (so lines clear), with random keys thrown in
        uint32_t k = (f / 2000) % 4 == 3 ? (uint32_t)rnd_n(128) : tet_ai_keys(&g, &ai, 1 + rnd_n(3), rnd_n(2));
        if(rnd_n(10) == 0) k ^= 1u << rnd_n(7);
        if(rnd_n(3) && (f / 2000) % 4 == 3) k &= (uint32_t)~TK_HARD;
        int lines_before = g.lines;
        uint32_t ev = tet_step(&g, k);
        check_game(&g, "rules");
        if(g.score < last_score) FAIL("rules: the score went down");
        last_score = g.score;
        if(ev & TE_CLEAR) {
            clears++;
            s_tetrises += g.lk_lines == 4;
            s_tspins += g.lk_spin != SPIN_NONE;
            if(g.lines != lines_before + g.lk_lines || g.lk_lines != g.clear_n || !g.clear_n) {
                FAIL("rules: lines counted wrong (%d + %d != %d)", lines_before, g.lk_lines, g.lines);
            }
        }
        if(g.over) {
            if(tet_step(&g, k) != 0) FAIL("rules: a finished game still moves");
            games++;
            lines += g.lines;
            tet_new(&g, rnd_n(TM_COUNT), 1 + rnd_n(15), rnd32());
            g.das = (uint8_t)(1 + rnd_n(7));
            g.arr = (uint8_t)rnd_n(3);
            memset(&ai, 0, sizeof(ai));
            last_score = 0;
        }
        if(rnd_n(5000) == 0 && !g.over) {
            // suspend and resume at a random moment: the game must carry on
            tgame_t t;
            uint8_t buf[TET_PACK_LEN];
            tet_settle(&g);
            if(!g.over) {
                if(tet_pack(&g, buf, sizeof(buf)) != TET_PACK_LEN || !tet_unpack(&t, buf, TET_PACK_LEN)) {
                    FAIL("rules: a game at frame %ld does not suspend", f);
                } else {
                    t.das = g.das;
                    t.arr = g.arr;
                    t.frames = g.frames;
                    g = t;
                }
            }
        }
    }
    printf("      rules fuzz: %ld frames, %ld games, %ld lines, %ld clears (%ld tetrises, %ld t-spins)\n",
           frames, games, lines, clears, s_tetrises, s_tspins);
}

int main(int argc, char **argv)
{
    long frames = argc > 1 ? atol(argv[1]) : 400000;
    s_rng = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 12345;
    if(!s_rng) s_rng = 1;
    monkey(frames);
    save_fuzz((int)(frames / 200 > 200 ? frames / 200 : 200));
    rules_fuzz(frames * 2);
    printf(s_fail ? "\n%d check(s) FAILED\n" : "\nall fuzz checks passed\n", s_fail);
    return s_fail ? 1 : 0;
}
