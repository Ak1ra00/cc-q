// TETRIS - rules tests: kicks, T-spins, scoring, the bag, lock delay, suspend
// and resume, and the demo player. Built and run by `make test` in sim/.
#include <stdio.h>
#include <string.h>
#include "../game/tetris.c"

uint32_t plat_millis(void) { return 0; }

static int s_fail;

#define CHECK(name, cond) do { \
    if(cond) printf("[ok ] %s\n", name); \
    else { printf("[FAIL] %s  (%s:%d)\n", name, __FILE__, __LINE__); s_fail++; } \
} while(0)

static void fill_row(tgame_t *g, int y, const char *pat)
{
    for(int x = 0; x < TB_W; x++) g->cell[y][x] = pat[x] == 'X' ? PC_GREY : 0;
}

static void place(tgame_t *g, int type, int rot, int x, int y)
{
    g->type = (int8_t)type;
    g->rot = (int8_t)rot;
    g->x = (int8_t)x;
    g->y = (int8_t)y;
    g->active = true;
    g->lowest = (int8_t)y;
    g->lock_t = 0;
    g->lock_moves = 0;
    g->last_rot = false;
    g->keys_prev = 0;
    g->wait = 0;
}

static uint32_t press(tgame_t *g, uint32_t k)
{
    g->keys_prev = 0;
    uint32_t ev = tet_step(g, k);
    return ev;
}

static void test_kick_symmetry(void)
{
    // every kick back is the kick there, reversed
    bool ok = true;
    for(int from = 0; from < 4; from++) {
        int to = (from + 1) & 3;
        for(int i = 0; i < 5; i++) {
            ok &= KICK_JLSTZ[from][0][i][0] == -KICK_JLSTZ[to][1][i][0];
            ok &= KICK_JLSTZ[from][0][i][1] == -KICK_JLSTZ[to][1][i][1];
            ok &= KICK_I[from][0][i][0] == -KICK_I[to][1][i][0];
            ok &= KICK_I[from][0][i][1] == -KICK_I[to][1][i][1];
        }
    }
    CHECK("kicks: each rotation's kicks undo the reverse rotation's", ok);
}

static void test_rotation_table(void)
{
    // four clockwise turns bring every piece home; each state has four distinct cells
    bool ok = true;
    for(int t = PC_I; t <= PC_L; t++) {
        for(int r = 0; r < 4; r++) {
            for(int i = 0; i < 4; i++) {
                for(int j = i + 1; j < 4; j++) {
                    ok &= !(TET_SHAPE[t][r][i][0] == TET_SHAPE[t][r][j][0] && TET_SHAPE[t][r][i][1] == TET_SHAPE[t][r][j][1]);
                }
            }
        }
    }
    CHECK("shapes: four distinct cells in every state", ok);
}

static void test_tspin_double(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 1);
    fill_row(&g, 21, "...X......");
    fill_row(&g, 22, "XXX...XXXX");
    fill_row(&g, 23, "XXXX.XXXXX");
    // the T drops in standing on its side, then turns into the slot
    place(&g, PC_T, 1, 3, 10);
    press(&g, TK_HARD);
    CHECK("tsd: T hard-dropped on its side locks without a spin", g.lk_spin == SPIN_NONE);

    tet_new(&g, TM_MARATHON, 1, 1);
    fill_row(&g, 21, "...X......");
    fill_row(&g, 22, "XXX...XXXX");
    fill_row(&g, 23, "XXXX.XXXXX");
    place(&g, PC_T, 1, 3, 21);
    uint32_t ev = press(&g, TK_CW);
    CHECK("tsd: the turn into the slot works", (ev & TE_ROTATE) && g.rot == 2 && g.x == 3 && g.y == 21);
    ev = press(&g, TK_HARD);
    CHECK("tsd: counts as a full T-spin double", g.lk_spin == SPIN_FULL && g.lk_lines == 2);
    CHECK("tsd: scores 1200 at level 1", g.lk_points == 1200 && g.score == 1200);
    CHECK("tsd: sets back-to-back", g.b2b);
}

static void test_wall_kick(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 1);
    // an upright I against the left wall turns flat by kicking right
    place(&g, PC_I, 3, -1, 10);
    uint32_t ev = press(&g, TK_CW);
    CHECK("kick: upright I at the left wall turns flat", (ev & TE_ROTATE) && g.rot == 0 && g.x >= 0);
    // O never turns
    place(&g, PC_O, 0, 4, 10);
    ev = press(&g, TK_CW);
    CHECK("kick: O does not turn", !(ev & TE_ROTATE) && g.rot == 0);
}

static void test_perfect_clear(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 1);
    fill_row(&g, 23, "XXXXXX....");
    place(&g, PC_I, 0, 6, 5);
    press(&g, TK_HARD);
    CHECK("pc: a single that empties the field is a perfect clear", g.lk_pc && g.lk_lines == 1);
    CHECK("pc: scores 100 + 800", g.lk_points == 900);
}

static void test_b2b_combo(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 2, 1);
    for(int y = 16; y < 24; y++) fill_row(&g, y, "XXXXXXXXX.");
    fill_row(&g, 15, "X.........");
    place(&g, PC_I, 1, 7, 3);
    press(&g, TK_HARD);
    CHECK("b2b: first tetris scores 800 x level 2", g.lk_lines == 4 && g.lk_points == 1600 && !g.lk_b2b);
    tet_settle(&g);
    place(&g, PC_I, 1, 7, 3);
    press(&g, TK_HARD);
    CHECK("b2b: second tetris is back-to-back, x1.5, plus combo 1", g.lk_b2b && g.lk_combo == 1 &&
          g.lk_points == 1600 * 3 / 2 + 50 * 1 * 2);
    CHECK("b2b: counts two tetrises", g.n_tetris == 2);
}

static void test_bag(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 99);
    g.bag_n = 0;
    bool ok = true;
    for(int round = 0; round < 50; round++) {
        int seen = 0;
        for(int i = 0; i < 7; i++) seen |= 1 << bag_take(&g);
        ok &= seen == 0xfe;
    }
    CHECK("bag: every run of seven has each piece once", ok);
}

static void test_lock_delay(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 5);
    place(&g, PC_T, 0, 3, 22);
    int n = 0;
    uint32_t ev = 0;
    while(!(ev & TE_LOCK) && n < 100) {
        ev = tet_step(&g, 0);
        n++;
    }
    CHECK("lock: a resting piece locks after half a second", n == LOCK_DELAY);

    // moving it keeps it alive, but only fifteen times
    tet_new(&g, TM_MARATHON, 1, 5);
    place(&g, PC_T, 0, 3, 22);
    n = 0;
    ev = 0;
    while(!(ev & TE_LOCK) && n < 400) {
        ev = tet_step(&g, (n & 1) ? TK_LEFT : TK_RIGHT);
        n++;
    }
    CHECK("lock: move reset is limited", (ev & TE_LOCK) && n <= LOCK_MOVES + 2);
}

static void test_topout(void)
{
    tgame_t g;
    tet_new(&g, TM_MARATHON, 1, 7);
    for(int y = TB_HIDDEN - 2; y < TB_H; y++) fill_row(&g, y, "XXXX.XXXXX");
    uint32_t ev = 0;
    for(int i = 0; i < 5 && !g.over; i++) ev |= tet_step(&g, 0);
    CHECK("topout: no room for the next piece ends the game", g.over && (ev & TE_TOPOUT));
}

static void test_suspend_resume(void)
{
    tgame_t a, b;
    tai_t ai;
    memset(&ai, 0, sizeof(ai));
    tet_new(&a, TM_MARATHON, 3, 1234);
    for(int f = 0; f < 3000; f++) tet_step(&a, tet_ai_keys(&a, &ai, 3, true));
    tet_settle(&a);
    uint8_t buf[256];
    int n = tet_pack(&a, buf, sizeof(buf));
    CHECK("suspend: packs to the fixed length", n == TET_PACK_LEN);
    CHECK("suspend: unpacks", tet_unpack(&b, buf, n));
    // both carry on identically
    a.keys_prev = TK_ALL;
    tai_t ai2 = ai;
    ai.planned = ai2.planned = false;
    bool same = true;
    for(int f = 0; f < 3000 && same; f++) {
        uint32_t ka = tet_ai_keys(&a, &ai, 3, true), kb = tet_ai_keys(&b, &ai2, 3, true);
        tet_step(&a, ka);
        tet_step(&b, kb);
        same = !memcmp(a.cell, b.cell, sizeof(a.cell)) && a.score == b.score && a.lines == b.lines;
    }
    CHECK("suspend: a resumed game plays on exactly like the original", same);
    buf[40] ^= 0x7f;
    buf[0] = 9;
    CHECK("suspend: a different version is refused", !tet_unpack(&b, buf, n));
}

static void test_ai(void)
{
    tgame_t g;
    tai_t ai;
    memset(&ai, 0, sizeof(ai));
    tet_new(&g, TM_MARATHON, 1, 42);
    int frames = 0, lines = 0;
    while(!g.over && g.pieces < 2000 && frames < 400000) {
        tet_step(&g, tet_ai_keys(&g, &ai, 1, true));
        // hold it at level 1: at 20G nothing moves sideways without a soft-drop tuck
        lines += g.lines;
        g.lines = 0;
        g.level = 1;
        frames++;
    }
    printf("      ai: %u pieces, %d lines, score %u, %u tetrises, %u t-spins, over=%d\n",
           g.pieces, lines, g.score, g.n_tetris, g.n_tspin, g.over);
    CHECK("ai: survives 2000 pieces at level 1", g.pieces >= 2000 && !g.over);
    CHECK("ai: builds for tetrises (a fifth of its lines or more)", g.n_tetris * 4 * 5 >= (uint32_t)lines);

    tet_new(&g, TM_SPRINT, 1, 43);
    memset(&ai, 0, sizeof(ai));
    frames = 0;
    uint32_t ev = 0;
    while(!g.over && frames < 100000) {
        ev |= tet_step(&g, tet_ai_keys(&g, &ai, 1, true));
        frames++;
    }
    CHECK("sprint: ends, won, at 40 lines", g.over && g.won && g.lines >= 40 && (ev & TE_GOAL));

    tet_new(&g, TM_ULTRA, 1, 44);
    memset(&ai, 0, sizeof(ai));
    frames = 0;
    while(!g.over && frames < 100000) {
        tet_step(&g, tet_ai_keys(&g, &ai, 2, true));
        frames++;
    }
    CHECK("ultra: ends after two minutes of play", g.over && g.won && g.frames == ULTRA_FRAMES);
}

int main(void)
{
    test_kick_symmetry();
    test_rotation_table();
    test_tspin_double();
    test_wall_kick();
    test_perfect_clear();
    test_b2b_combo();
    test_bag();
    test_lock_delay();
    test_topout();
    test_suspend_resume();
    test_ai();
    printf(s_fail ? "\n%d test(s) FAILED\n" : "\nall tetris tests passed\n", s_fail);
    return s_fail ? 1 : 0;
}
