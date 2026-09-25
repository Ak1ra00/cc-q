// PAC-MAN - rules tests: the mazes, moving and cornering, the ghosts' targets,
// the house, energizers, dying, clearing a level, fruit, extra lives, NEON's
// dash and power-ups, suspend and resume, and long runs of the demo player.
// Built and run by `make test` in sim/.
#include <stdio.h>
#include <string.h>
#include "../game/pac.c"

static int s_fail;

#define CHECK(name, cond) do { \
    if(cond) printf("[ok ] %s\n", name); \
    else { printf("[FAIL] %s  (%s:%d)\n", name, __FILE__, __LINE__); s_fail++; } \
} while(0)

static void play(pgame_t *g)
{
    while(g->phase == PP_READY) pac_step(g, 0);
}

static void clear_dots(pgame_t *g)
{
    // leave one dot far away, so the level does not end
    memset(g->dot, 0, sizeof(g->dot));
    g->dot[1][1] = 1;
    g->dots_left = 1;
}

static void park_ghosts(pgame_t *g)
{
    for(int i = 0; i < GH_N; i++) {
        g->gh[i].state = GS_HOUSE;
        g->gh[i].x = HOME_X[i];
        g->gh[i].y = HOUSE_Y;
        g->gh[i].dots = 0;
    }
    g->idle_t = -30000;
    g->global_on = true;
    g->global_dots = 0;
}

static void test_mazes(void)
{
    bool ok = true;
    for(int m = 0; m < PAC_MAZES; m++) {
        int dots = 0, power = 0;
        for(int y = 0; y < PM_H; y++) {
            for(int x = 0; x < PM_W; x++) {
                char c = PAC_MAZE[m][y][x];
                ok &= c == PAC_MAZE[m][y][PM_W - 1 - x];       // mirrored
                dots += c == '.';
                power += c == 'o';
                if(y == 0 || y == PM_H - 1) ok &= !open_cell(c);
            }
        }
        ok &= dots > 300 && power == 4;
        ok &= open_cell(PAC_MAZE[m][17][18]) && open_cell(PAC_MAZE[m][17][19]);
        ok &= PAC_MAZE[m][10][18] == '-' && PAC_MAZE[m][10][19] == '-';
        ok &= open_cell(PAC_MAZE[m][9][18]) && open_cell(PAC_MAZE[m][15][18]);
        ok &= !open_cell(PAC_MAZE[m][8][18]) && !open_cell(PAC_MAZE[m][16][18]);
    }
    CHECK("mazes: mirrored, closed top and bottom, 4 energizers, the house where it belongs", ok);
    ok = pac_maze_for_level(1) == 0 && pac_maze_for_level(2) == 0 && pac_maze_for_level(3) == 1 &&
         pac_maze_for_level(6) == 2 && pac_maze_for_level(10) == 0 && pac_maze_for_level(14) == 1 &&
         pac_maze_for_level(255) < PAC_MAZES;
    CHECK("mazes: which maze each level plays", ok);
    ok = pac_fruit_for_level(1) == FR_CHERRY && pac_fruit_for_level(2) == FR_STRAWBERRY &&
         pac_fruit_for_level(4) == FR_ORANGE && pac_fruit_for_level(13) == FR_KEY && pac_fruit_for_level(200) == FR_KEY;
    CHECK("fruit: cherry, strawberry, orange ... key", ok);
}

static void test_moving(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 5);
    CHECK("start: READY, three lives, Pac-Man in the middle of his row",
          g.phase == PP_READY && g.lives == 3 && g.pac.x == DOOR_X && g.pac.y == START_Y);
    play(&g);
    park_ghosts(&g);
    clear_dots(&g);
    int32_t x0 = g.pac.x;
    for(int i = 0; i < 30; i++) pac_step(&g, 0);
    int32_t moved = x0 - g.pac.x;
    CHECK("speed: level 1 is 80% of 75.76 px/s (60.6 px, 1939 units in a second)", moved >= 1935 && moved <= 1942);

    // along row 17 to the left wall, where he stops in the middle of tile 1
    for(int i = 0; i < 200; i++) pac_step(&g, 0);
    CHECK("walls: stops in the middle of the last tile", g.pac.x == 1 * PT + PT / 2 && !g.moving);

    // reverse at once
    pac_new(&g, PM_CLASSIC, 5);
    play(&g);
    park_ghosts(&g);
    clear_dots(&g);
    for(int i = 0; i < 5; i++) pac_step(&g, 0);
    int32_t xa = g.pac.x;
    pac_step(&g, PK_RIGHT);
    CHECK("reverse: turns round the same frame", g.pac.dir == PD_RIGHT && g.pac.x > xa);

    // a tap is kept until the corner comes: row 17 meets col 15's corridor going up
    pac_new(&g, PM_CLASSIC, 5);
    play(&g);
    park_ghosts(&g);
    clear_dots(&g);
    pac_step(&g, PK_UP);
    pac_step(&g, 0);
    bool turned = false;
    int32_t off = 0;
    for(int i = 0; i < 60 && !turned; i++) {
        off = g.pac.x - (15 * PT + PT / 2);
        pac_step(&g, 0);
        turned = g.pac.dir == PD_UP;
    }
    CHECK("buffered turn: a tap of UP is taken at the next opening (col 15)", turned && (g.pac.x >> 8) == 15);
    // cutting the corner: he turned before reaching the middle, and slid onto it
    for(int i = 0; i < 6; i++) pac_step(&g, 0);
    CHECK("cornering: turns a little before the middle, then lines up", off > 0 && off <= TURN_WIN &&
          g.pac.x == 15 * PT + PT / 2 && g.pac.y < START_Y);

    // no way through walls
    pac_new(&g, PM_CLASSIC, 5);
    play(&g);
    park_ghosts(&g);
    clear_dots(&g);
    bool inside = true;
    uint32_t seed = 99;
    for(int i = 0; i < 20000; i++) {
        seed = seed * 1103515245u + 12345u;
        uint32_t k = (seed >> 16) % 5 == 0 ? (1u << ((seed >> 20) & 3)) : 0;
        pac_step(&g, k);
        int tx = g.pac.x >> 8, ty = g.pac.y >> 8;
        inside &= pac_open(&g, tx, ty);
        // on a corridor's middle line, or cutting a corner onto one
        inside &= iabs((g.pac.x & 255) - 128) <= TURN_WIN || iabs((g.pac.y & 255) - 128) <= TURN_WIN;
    }
    CHECK("random steering for 20,000 frames: never in a wall, always on or cornering onto a corridor's line", inside);
}

static void test_tunnel(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 3);
    play(&g);
    park_ghosts(&g);
    clear_dots(&g);
    // put him in the tunnel row heading left
    g.pac.x = 2 * PT + PT / 2;
    g.pac.y = 12 * PT + PT / 2;
    g.pac.dir = PD_LEFT;
    g.want = PD_LEFT;
    bool wrapped = false;
    for(int i = 0; i < 30; i++) {
        pac_step(&g, 0);
        if((g.pac.x >> 8) >= PM_W - 3) wrapped = true;
    }
    CHECK("tunnel: out of the left side, in on the right", wrapped && g.pac.x >= 0 && g.pac.x < MAZE_W);
}

static void test_targets(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 1);
    play(&g);
    g.chase = true;
    g.pac.x = 10 * PT + PT / 2;
    g.pac.y = 17 * PT + PT / 2;
    g.pac.dir = PD_RIGHT;
    g.gh[GH_BLINKY].x = 5 * PT + PT / 2;
    g.gh[GH_BLINKY].y = 7 * PT + PT / 2;
    g.gh[GH_BLINKY].state = GS_ACTIVE;
    int tx, ty;
    ghost_target(&g, GH_BLINKY, &tx, &ty);
    CHECK("Blinky: Pac-Man's tile", tx == 10 && ty == 17);
    ghost_target(&g, GH_PINKY, &tx, &ty);
    CHECK("Pinky: four tiles ahead of him", tx == 14 && ty == 17);
    ghost_target(&g, GH_INKY, &tx, &ty);
    CHECK("Inky: Blinky's line through two ahead, doubled", tx == 2 * 12 - 5 && ty == 2 * 17 - 7);
    g.gh[GH_CLYDE].x = 30 * PT + PT / 2;
    g.gh[GH_CLYDE].y = 4 * PT + PT / 2;
    g.gh[GH_CLYDE].state = GS_ACTIVE;
    ghost_target(&g, GH_CLYDE, &tx, &ty);
    bool far = tx == 10 && ty == 17;
    g.gh[GH_CLYDE].x = 12 * PT + PT / 2;
    g.gh[GH_CLYDE].y = 17 * PT + PT / 2;
    ghost_target(&g, GH_CLYDE, &tx, &ty);
    CHECK("Clyde: chases from afar, turns for his corner up close", far && tx == 0 && ty == PM_H);
    g.chase = false;
    ghost_target(&g, GH_BLINKY, &tx, &ty);
    bool b = tx == PM_W - 3 && ty == -3;
    ghost_target(&g, GH_PINKY, &tx, &ty);
    CHECK("scatter: each to a corner", b && tx == 2 && ty == -3);
    // Elroy: Blinky chases even in scatter near the end
    g.dots_left = 5;
    g.gh[GH_CLYDE].state = GS_ACTIVE;
    ghost_target(&g, GH_BLINKY, &tx, &ty);
    CHECK("Elroy: with few dots left Blinky chases through scatter", tx == 10 && ty == 17 && elroy(&g) == 2);
}

static void test_house(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 11);
    play(&g);
    pac_step(&g, 0);
    CHECK("house: Pinky leaves at once on level 1", g.gh[GH_PINKY].state == GS_LEAVING);
    bool out = false;
    for(int i = 0; i < 90 && !out; i++) {
        pac_step(&g, 0);
        out = g.gh[GH_PINKY].state == GS_ACTIVE;
    }
    CHECK("house: Pinky comes out over the door, heading left",
          out && g.gh[GH_PINKY].y == EXIT_Y && g.gh[GH_PINKY].x <= DOOR_X);
    CHECK("house: Inky waits for his 30 dots", g.gh[GH_INKY].state == GS_HOUSE && g.gh[GH_INKY].dots < 30);
    // the idle timer lets him out when nothing is eaten for four seconds
    pac_new(&g, PM_CLASSIC, 11);
    play(&g);
    clear_dots(&g);
    g.gh[GH_BLINKY].state = GS_HOUSE;
    g.gh[GH_BLINKY].y = HOUSE_Y;
    int t = 0;
    while(g.gh[GH_INKY].state == GS_HOUSE && t < 400) {
        pac_step(&g, 0);
        t++;
    }
    CHECK("house: four seconds with no dot lets the next one out", t >= 4 * FPS && t <= 4 * FPS + 2);
    // dot counts
    pac_new(&g, PM_CLASSIC, 11);
    play(&g);
    pac_step(&g, 0);
    g.gh[GH_INKY].dots = 0;
    for(int i = 0; i < 29; i++) count_dot(&g);
    play_tick(&g);
    bool wait = g.gh[GH_INKY].state == GS_HOUSE;
    count_dot(&g);
    g.idle_t = 0;
    play_tick(&g);
    CHECK("house: Inky's 30th dot opens the door", wait && g.gh[GH_INKY].state == GS_LEAVING);
    // after a death the house counts for everyone: 7, 17, 32
    g.global_on = true;
    g.global_dots = 0;
    for(int i = GH_PINKY; i <= GH_CLYDE; i++) g.gh[i].state = GS_HOUSE;
    for(int i = 0; i < 7; i++) count_dot(&g);
    bool p = g.gh[GH_PINKY].state == GS_LEAVING && g.gh[GH_INKY].state == GS_HOUSE;
    for(int i = 0; i < 10; i++) count_dot(&g);
    bool q = g.gh[GH_INKY].state == GS_LEAVING && g.gh[GH_CLYDE].state == GS_HOUSE;
    for(int i = 0; i < 15; i++) count_dot(&g);
    CHECK("house: after a death, out at 7, 17 and 32 dots", p && q && g.gh[GH_CLYDE].state == GS_LEAVING && !g.global_on);
}

static void test_energizer(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 21);
    play(&g);
    for(int i = 0; i < 100; i++) pac_step(&g, 0);
    uint8_t before = g.gh[GH_BLINKY].dir;
    clear_dots(&g);
    g.dot[g.pac.y >> 8][g.pac.x >> 8] = 2;
    g.dots_left = 2;
    uint32_t ev = pac_step(&g, 0);
    CHECK("energizer: 50 points and the ghosts turn blue and round",
          (ev & PE_POWER) && g.gh[GH_BLINKY].fright && g.fright_t == 6 * FPS && g.gh[GH_BLINKY].dir != before);
    // eat all four, one after another: 200, 400, 800, 1600
    uint32_t pts[4] = { 0 };
    for(int i = 0; i < GH_N; i++) {
        g.gh[i].state = GS_ACTIVE;
        g.gh[i].fright = true;
        g.gh[i].x = g.pac.x;
        g.gh[i].y = g.pac.y;
        uint32_t s0 = g.score;
        g.phase = PP_PLAY;
        collide(&g);
        pts[i] = g.score - s0;
        for(int j = i + 1; j < GH_N; j++) g.gh[j].x = 0, g.gh[j].state = GS_HOUSE;
    }
    CHECK("ghosts: 200, 400, 800, 1600", pts[0] == 200 && pts[1] == 400 && pts[2] == 800 && pts[3] == 1600);
    CHECK("ghosts: eaten ones are eyes, the game holds still for the points", g.gh[0].state == GS_EYES && g.phase == PP_EAT);
    // the eyes find their way home and come back out
    for(int i = 0; i < GH_N; i++) {
        g.gh[i].state = GS_ACTIVE;
        g.gh[i].fright = false;
    }
    g.gh[0].state = GS_EYES;
    g.gh[0].x = 3 * PT + PT / 2;
    g.gh[0].y = 25 * PT + PT / 2;
    g.gh[0].dir = PD_RIGHT;
    for(int i = 1; i < GH_N; i++) g.gh[i].state = GS_HOUSE;
    g.phase = PP_PLAY;
    clear_dots(&g);
    g.pac.x = 36 * PT + PT / 2;
    g.pac.y = 1 * PT + PT / 2;
    g.want = PD_NONE;
    int t = 0;
    bool home = false;
    while(t < 600 && g.gh[0].state != GS_ACTIVE) {
        pac_step(&g, 0);
        if(g.gh[0].state == GS_ENTERING || g.gh[0].state == GS_LEAVING) home = true;
        t++;
    }
    CHECK("eyes: back to the house, and out again", home && g.gh[0].state == GS_ACTIVE && t < 300);

    // NEON: all four on one energizer is worth 3000 more
    pac_new(&g, PM_NEON, 21);
    play(&g);
    start_fright(&g);
    uint32_t s0 = g.score;
    for(int i = 0; i < GH_N; i++) {
        g.gh[i].state = GS_ACTIVE;
        g.gh[i].fright = true;
        g.gh[i].x = g.pac.x;
        g.gh[i].y = g.pac.y;
        g.phase = PP_PLAY;
        collide(&g);
    }
    CHECK("NEON: all four ghosts on one energizer, +3000", g.score - s0 == 3000 + 3000 && (g.ev & PE_ALL4));
}

static void test_reverse_at_a_turn(void)
{
    // found by the long fuzz: a ghost that has just chosen a new way at a tile's middle,
    // then is turned round by an energizer, must go back the way it came, not into a wall
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 91);
    play(&g);
    clear_dots(&g);
    park_ghosts(&g);
    pactor_t *a = &g.gh[GH_CLYDE];
    a->state = GS_ACTIVE;
    a->x = 23 * PT + PT / 2;
    a->y = 9 * PT + PT / 2;
    a->came = PD_RIGHT;
    a->dir = PD_DOWN;               // chosen here, not yet taken; UP from here is a wall
    start_fright(&g);
    bool ok = a->dir == PD_LEFT;
    for(int i = 0; i < 60; i++) {
        pac_step(&g, 0);
        ok &= pac_open(&g, a->x >> 8, a->y >> 8);
    }
    CHECK("ghosts: turned round at a turning, back the way they came", ok);
}

static void test_dying(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 31);
    play(&g);
    g.gh[GH_BLINKY].x = g.pac.x - 60;
    g.gh[GH_BLINKY].y = g.pac.y;
    g.gh[GH_BLINKY].dir = PD_RIGHT;
    uint32_t ev = pac_step(&g, 0);
    CHECK("caught: a ghost on him ends the life", (ev & PE_CAUGHT) && g.phase == PP_DYING);
    uint32_t all = 0;
    for(int i = 0; i < DIE_FREEZE + DIE_ANIM; i++) all |= pac_step(&g, 0);
    CHECK("caught: the fold-away, one life gone, READY again with everyone home",
          (all & PE_DEATH) && (all & PE_LIFE_LOST) && g.lives == 2 && g.phase == PP_READY && g.pac.x == DOOR_X &&
          g.gh[GH_PINKY].state == GS_HOUSE && g.global_on);
    g.lives = 1;
    play(&g);
    g.gh[GH_BLINKY].x = g.pac.x;
    g.gh[GH_BLINKY].y = g.pac.y;
    pac_step(&g, 0);
    all = 0;
    for(int i = 0; i < DIE_FREEZE + DIE_ANIM; i++) all |= pac_step(&g, 0);
    CHECK("game over after the last life", (all & PE_OVER) && g.over && g.phase == PP_OVER && pac_step(&g, PK_ALL) == 0);
}

static void test_clear(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 41);
    play(&g);
    park_ghosts(&g);
    memset(g.dot, 0, sizeof(g.dot));
    g.dot[17][17] = 1;
    g.dots_left = 1;
    uint32_t all = 0;
    for(int i = 0; i < 20 && g.phase == PP_PLAY; i++) all |= pac_step(&g, 0);
    CHECK("clear: the last dot ends the level", (all & PE_CLEAR) && g.phase == PP_CLEAR);
    all = 0;
    for(int i = 0; i < CLEAR_FREEZE + CLEAR_FLASH; i++) all |= pac_step(&g, 0);
    CHECK("clear: level 2, all the dots back, READY", (all & PE_LEVEL) && g.level == 2 && g.dots_left == g.dots_total &&
          g.phase == PP_READY && g.maze == 0);
    g.level = 2;
    memset(g.dot, 0, sizeof(g.dot));
    g.dot[g.pac.y >> 8][(g.pac.x >> 8) - 1] = 1;
    g.dots_left = 1;
    play(&g);
    for(int i = 0; i < 200 && g.phase != PP_READY; i++) pac_step(&g, 0);
    CHECK("clear: level 3 is the second maze", g.level == 3 && g.maze == 1);
}

static void test_fruit_and_lives(void)
{
    pgame_t g;
    pac_new(&g, PM_CLASSIC, 51);
    play(&g);
    park_ghosts(&g);
    int t1 = g.dots_total * 70 / 244;
    uint32_t all = 0;
    for(int n = 0; n < t1; n++) {
        // eat dots one at a time wherever they are
        for(int y = 0; y < PM_H; y++) {
            for(int x = 0; x < PM_W; x++) {
                if(g.dot[y][x] == 1) {
                    eat_dot(&g, x, y, false);
                    all |= g.ev;
                    y = PM_H;
                    break;
                }
            }
        }
    }
    CHECK("fruit: the first comes out at 70 of 244 dots", (all & PE_FRUIT_ON) && g.fruit_n == 1 && g.fruit_t == FRUIT_FRAMES);
    g.pac.x = DOOR_X + 40;
    g.pac.y = FRUIT_Y;
    uint32_t s0 = g.score;
    g.ev = 0;
    pac_eat(&g);
    CHECK("fruit: the cherry is 100", (g.ev & PE_FRUIT) && g.score - s0 == 100 && g.fruit_t == 0);

    pac_new(&g, PM_CLASSIC, 52);
    g.ev = 0;
    score_add(&g, 9990);
    bool none = g.lives == 3;
    score_add(&g, 10);
    bool one = g.lives == 4 && (g.ev & PE_EXTRA);
    score_add(&g, 100000);
    CHECK("CLASSIC: one extra life, at 10,000", none && one && g.lives == 4);
    pac_new(&g, PM_NEON, 53);
    score_add(&g, 10000);
    score_add(&g, 30000);
    score_add(&g, 29990);
    bool two = g.lives == 5;
    score_add(&g, 10);
    CHECK("NEON: at 10,000 and every 30,000 after", two && g.lives == 6);
    for(int i = 0; i < 40; i++) score_add(&g, 30000);
    CHECK("lives stop at nine", g.lives == PAC_MAX_LIVES);
}

static void test_neon(void)
{
    pgame_t g;
    pac_new(&g, PM_NEON, 61);
    play(&g);
    park_ghosts(&g);
    for(int i = 0; i < BOOST_FULL; i++) {
        for(int y = 0; y < PM_H; y++) {
            for(int x = 0; x < PM_W; x++) {
                if(g.dot[y][x] == 1 && g.boost < BOOST_FULL) {
                    eat_dot(&g, x, y, false);
                    y = PM_H;
                    break;
                }
            }
        }
    }
    CHECK("NEON: 40 dots charge the dash", g.boost == BOOST_FULL);
    g.stall = 0;
    uint32_t ev = pac_step(&g, PK_DASH);
    CHECK("NEON: the dash key spends it", (ev & PE_DASH) && g.dash_t == DASH_FRAMES - 1 && g.boost == 0);
    // through a ghost while dashing
    g.gh[GH_BLINKY].state = GS_ACTIVE;
    g.gh[GH_BLINKY].x = g.pac.x - 200;
    g.gh[GH_BLINKY].y = g.pac.y;
    g.gh[GH_BLINKY].dir = PD_RIGHT;
    bool alive = true;
    for(int i = 0; i < 10; i++) {
        pac_step(&g, 0);
        alive &= g.phase == PP_PLAY;
    }
    CHECK("NEON: dashing goes straight through a ghost", alive);
    int32_t x0 = g.pac.x;
    pac_step(&g, 0);
    CHECK("NEON: dashing is half as fast again (120% on level 1)", x0 - g.pac.x >= 96 && x0 - g.pac.x <= 98);

    // items come at a quarter, half and three quarters
    pac_new(&g, PM_NEON, 62);
    play(&g);
    park_ghosts(&g);
    uint32_t all = 0;
    int eaten = 0;
    while(g.items_n < 1 && eaten < 400) {
        for(int y = 0; y < PM_H; y++) {
            for(int x = 0; x < PM_W; x++) {
                if(g.dot[y][x] == 1) {
                    g.ev = 0;
                    eat_dot(&g, x, y, false);
                    all |= g.ev;
                    y = PM_H;
                    break;
                }
            }
        }
        eaten++;
    }
    CHECK("NEON: the first power-up at a quarter of the dots", (all & PE_ITEM_ON) && g.item_t == ITEM_FRAMES &&
          eaten == g.dots_total / 4 && g.item_kind != IT_NONE);
    int ix = g.item_x, iy = g.item_y;
    CHECK("NEON: it is on a corridor, well away from Pac-Man",
          pac_open(&g, ix, iy) && iabs(ix - (g.pac.x >> 8)) + iabs(iy - (g.pac.y >> 8)) >= 10);
    // magnet
    g.item_kind = IT_MAGNET;
    g.pac.x = ix * PT + PT / 2;
    g.pac.y = iy * PT + PT / 2;
    g.ev = 0;
    pac_eat(&g);
    CHECK("NEON: MAGNET taken", (g.ev & PE_ITEM) && g.magnet_t == MAGNET_FRAMES);
    // freeze, and shattering a frozen ghost
    g.freeze_t = FREEZE_FRAMES;
    g.gh[GH_INKY].state = GS_ACTIVE;
    g.gh[GH_INKY].x = g.pac.x;
    g.gh[GH_INKY].y = g.pac.y;
    uint32_t s0 = g.score;
    g.ev = 0;
    collide(&g);
    CHECK("NEON: a frozen ghost shatters for 500", (g.ev & PE_SHATTER) && g.score - s0 == 500 && g.gh[GH_INKY].state == GS_EYES);
    CHECK("NEON: frozen ghosts do not move", ghost_pct(&g, GH_BLINKY, &(plevel_t){ 0 }) == 0 || g.gh[GH_BLINKY].state != GS_ACTIVE);

    // the magnet pulls dots in around him
    pac_new(&g, PM_NEON, 63);
    play(&g);
    park_ghosts(&g);
    g.magnet_t = 100;
    int before = g.dots_left;
    pac_step(&g, 0);
    CHECK("NEON: MAGNET pulls in the dots around", before - g.dots_left >= 3 && g.pulled_n >= 2);
}

static void test_pack(void)
{
    pgame_t g, h;
    pac_new(&g, PM_NEON, 71);
    play(&g);
    pai_t ai = { 0 };
    for(int i = 0; i < 900; i++) pac_step(&g, pac_ai_keys(&g, &ai));
    uint8_t buf[PAC_PACK_LEN];
    int n = pac_pack(&g, buf, sizeof(buf));
    bool ok = n == PAC_PACK_LEN && pac_unpack(&h, buf, n);
    ok &= h.mode == g.mode && h.level == g.level && h.lives == g.lives && h.score == g.score &&
          h.dots_left == g.dots_left && !memcmp(h.dot, g.dot, sizeof(g.dot)) && h.boost == g.boost &&
          h.phase == PP_READY && h.next_extra == g.next_extra && h.frames == g.frames;
    CHECK("suspend: packs and unpacks to the same level, dots and all, at READY", ok);
    uint8_t bad[PAC_PACK_LEN];
    int rejected = 0, tried = 0;
    for(int i = 0; i < PAC_PACK_LEN * 8; i++) {
        memcpy(bad, buf, sizeof(bad));
        bad[i >> 3] ^= (uint8_t)(1u << (i & 7));
        tried++;
        if(!pac_unpack(&h, bad, sizeof(bad))) rejected++;
        else {
            // anything accepted must still be a playable game
            ok &= h.dots_left > 0 && h.level >= 1 && h.lives >= 1 && h.lives <= PAC_MAX_LIVES;
        }
    }
    printf("      single bit flips rejected: %d of %d\n", rejected, tried);
    CHECK("suspend: whatever a flipped bit gives is still a sound game", ok);
    memset(bad, 0, sizeof(bad));
    CHECK("suspend: zeros are refused", !pac_unpack(&h, bad, sizeof(bad)));
    memcpy(bad, buf, sizeof(bad));
    memset(bad + 31, 0, PAC_PACK_LEN - 31);
    CHECK("suspend: a level with no dots left is refused", !pac_unpack(&h, bad, sizeof(bad)));
    CHECK("suspend: short is refused", !pac_unpack(&h, buf, PAC_PACK_LEN - 1));
}

static void test_determinism(void)
{
    pgame_t a, b;
    pai_t x = { 0 }, y = { 0 };
    pac_new(&a, PM_NEON, 81);
    pac_new(&b, PM_NEON, 81);
    bool same = true;
    for(int i = 0; i < 5000; i++) {
        pac_step(&a, pac_ai_keys(&a, &x));
        pac_step(&b, pac_ai_keys(&b, &y));
    }
    same = !memcmp(&a, &b, sizeof(a));
    CHECK("the same seed and keys play the same game", same);
}

static bool sane(const pgame_t *g)
{
    int tx = g->pac.x >> 8, ty = g->pac.y >> 8;
    if(g->pac.x < 0 || g->pac.x >= MAZE_W || !pac_open(g, tx, ty)) return false;
    for(int i = 0; i < GH_N; i++) {
        const pactor_t *a = &g->gh[i];
        if(a->x < 0 || a->x >= MAZE_W || a->y < 0 || a->y >= PM_H * PT) return false;
        if(a->state == GS_ACTIVE || a->state == GS_EYES) {
            if(!pac_open(g, a->x >> 8, a->y >> 8)) return false;
            if((a->x & 255) != 128 && (a->y & 255) != 128 && a->x != DOOR_X) return false;
        } else {
            // in the house or its door, on the middle line or a home spot
            if(a->y < EXIT_Y || a->y > HOUSE_Y + BOB || a->x < 16 * PT || a->x > 22 * PT) return false;
        }
    }
    int n = 0;
    for(int y = 0; y < PM_H; y++) {
        for(int x = 0; x < PM_W; x++) n += g->dot[y][x] != 0;
    }
    return n == g->dots_left;
}

static void test_bot(int mode, int games, int max_frames)
{
    uint64_t total = 0;
    int best_level = 0, levels = 0;
    uint32_t best = 0;
    bool ok = true;
    uint32_t all = 0;
    long frames = 0;
    for(int n = 0; n < games; n++) {
        pgame_t g;
        pai_t ai = { 0 };
        pac_new(&g, mode, 1000u + (uint32_t)n * 7919u);
        for(int f = 0; f < max_frames && !g.over; f++) {
            all |= pac_step(&g, pac_ai_keys(&g, &ai));
            if(!sane(&g)) {
                if(ok) printf("      insane at game %d frame %d phase %d\n", n, f, g.phase);
                ok = false;
            }
            frames++;
        }
        total += g.score;
        if(g.score > best) best = g.score;
        if(g.level > best_level) best_level = g.level;
        levels += g.level - 1;
    }
    char name[96];
    printf("      %s: %d games, average %llu, best %u, levels cleared %d, highest level %d (%ld frames)\n",
           mode == PM_NEON ? "NEON" : "CLASSIC", games, (unsigned long long)(total / (uint64_t)games), best, levels,
           best_level, frames);
    snprintf(name, sizeof(name), "demo player (%s): every frame sound, levels cleared", mode == PM_NEON ? "NEON" : "CLASSIC");
    CHECK(name, ok && levels > 0);
    uint32_t want = PE_DOT | PE_POWER | PE_GHOST | PE_FRUIT_ON | PE_CLEAR | PE_LEVEL | PE_CAUGHT | PE_DEATH | PE_WAVE |
                    PE_FRIGHT_END | PE_EXTRA;
    if(mode == PM_NEON) want |= PE_ITEM_ON | PE_ITEM | PE_DASH | PE_CHARGED;
    snprintf(name, sizeof(name), "demo player (%s): saw everything happen", mode == PM_NEON ? "NEON" : "CLASSIC");
    CHECK(name, (all & want) == want);
    if((all & want) != want) printf("      missing events %#x\n", want & ~all);
}

int main(void)
{
    test_mazes();
    test_moving();
    test_tunnel();
    test_targets();
    test_house();
    test_energizer();
    test_reverse_at_a_turn();
    test_dying();
    test_clear();
    test_fruit_and_lives();
    test_neon();
    test_pack();
    test_determinism();
    test_bot(PM_CLASSIC, 40, 30 * 60 * 8);
    test_bot(PM_NEON, 40, 30 * 60 * 8);
    printf(s_fail ? "\n%d FAILED\n" : "\nall passed\n", s_fail);
    return s_fail ? 1 : 0;
}
