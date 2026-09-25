// PAC-MAN - the rules. See pac.h.
#include "pac.h"
#include <string.h>

const int8_t PAC_DX[5] = { 0, -1, 0, 1, 0 };
const int8_t PAC_DY[5] = { -1, 0, 1, 0, 0 };
const uint16_t PAC_FRUIT_PTS[FR_COUNT] = { 100, 300, 500, 700, 1000, 2000, 3000, 5000 };

#define MAZE_W      (PM_W * PT)
#define DOOR_X      (19 * PT)               // the line down the middle of the maze
#define EXIT_Y      (9 * PT + PT / 2)       // the row over the ghost house
#define HOUSE_Y     (12 * PT + PT / 2)      // the middle of the house
#define START_Y     (17 * PT + PT / 2)      // Pac-Man's row
#define FRUIT_Y     (15 * PT + PT / 2)
#define BOB         80                      // how far the ghosts in the house bob
#define TURN_WIN    104                     // how far off a tile's middle a corner can still be cut

static const int32_t HOME_X[GH_N] = { DOOR_X, DOOR_X, 17 * PT, 21 * PT };
static const uint8_t KEY_DIR[4] = { PK_UP, PK_LEFT, PK_DOWN, PK_RIGHT };

// ------------------------------------------------------------------ the arcade's tables

typedef struct {
    uint8_t pac, pac_fr, gh, gh_fr, tun, el1, el2;     // speeds, percent of 75.76 pixels a second
    uint8_t fright_s, flashes;
    uint8_t elroy;          // dots left when Blinky speeds up, in the arcade's 244-dot maze
} plevel_t;

static plevel_t level_params(const pgame_t *g)
{
    static const uint8_t FRIGHT[19] = { 6, 5, 4, 3, 2, 5, 2, 2, 1, 5, 2, 1, 1, 3, 1, 1, 0, 1, 0 };
    static const uint8_t FLASHES[19] = { 5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 3, 3, 5, 3, 3, 0, 3, 0 };
    static const uint8_t ELROY[19] = { 20, 30, 40, 40, 40, 50, 50, 50, 60, 60, 60, 80, 80, 80, 100, 100, 100, 100, 120 };
    static const uint8_t SPEED[4][7] = {
        { 80, 90, 75, 50, 40, 80, 85 },
        { 90, 95, 85, 55, 45, 90, 95 },
        { 100, 100, 95, 60, 50, 100, 105 },
        { 90, 90, 95, 60, 50, 100, 105 },
    };
    int lv = g->level;
    int i = lv > 19 ? 18 : lv - 1;
    const uint8_t *s = SPEED[lv == 1 ? 0 : (lv <= 4 ? 1 : (lv <= 20 ? 2 : 3))];
    plevel_t p = { s[0], s[1], s[2], s[3], s[4], s[5], s[6], FRIGHT[i], FLASHES[i], ELROY[i] };
    if(g->mode == PM_NEON && p.fright_s < 2) {
        // NEON never takes the energizers away
        p.fright_s = 2;
        p.flashes = 3;
    }
    return p;
}

// scatter, chase, scatter, ... in frames; after the last chase goes on for good
static const int16_t WAVES[3][7] = {
    { 7 * FPS, 20 * FPS, 7 * FPS, 20 * FPS, 5 * FPS, 20 * FPS, 5 * FPS },
    { 7 * FPS, 20 * FPS, 7 * FPS, 20 * FPS, 5 * FPS, 1033 * FPS, 1 },
    { 5 * FPS, 20 * FPS, 5 * FPS, 20 * FPS, 5 * FPS, 1037 * FPS, 1 },
};

static int wave_set(const pgame_t *g)
{
    return g->level == 1 ? 0 : (g->level <= 4 ? 1 : 2);
}

// dots each ghost waits for in the house, levels 1, 2, and 3 on
static const uint8_t HOUSE_LIMIT[3][GH_N] = { { 0, 0, 30, 60 }, { 0, 0, 0, 50 }, { 0, 0, 0, 0 } };

int pac_maze_for_level(int level)
{
    if(level <= 2) return 0;
    if(level <= 5) return 1;
    if(level <= 9) return 2;
    return ((level - 10) / 4) % PAC_MAZES;
}

int pac_fruit_for_level(int level)
{
    static const uint8_t F[13] = { FR_CHERRY, FR_STRAWBERRY, FR_ORANGE, FR_ORANGE, FR_APPLE, FR_APPLE, FR_MELON,
                                   FR_MELON, FR_GALAXIAN, FR_GALAXIAN, FR_BELL, FR_BELL, FR_KEY };
    return F[level > 13 ? 12 : (level < 1 ? 0 : level - 1)];
}

void pac_fruit_pos(int32_t *x, int32_t *y)
{
    *x = DOOR_X;
    *y = FRUIT_Y;
}

// ------------------------------------------------------------------ the maze

static inline int wrapx(int tx)
{
    return tx < 0 ? tx + PM_W : (tx >= PM_W ? tx - PM_W : tx);
}

static inline bool open_cell(char c)
{
    // corridors: dots, energizers, empty, the tunnels
    static const uint8_t OPEN[128] = { [' '] = 1, ['.'] = 1, ['o'] = 1, ['t'] = 1 };
    return OPEN[c & 127] != 0;
}

bool pac_open(const pgame_t *g, int tx, int ty)
{
    if(ty < 0 || ty >= PM_H || tx < -1 || tx > PM_W) return false;
    return open_cell(PAC_MAZE[g->maze][ty][wrapx(tx)]);
}

static void wrap(pactor_t *a)
{
    if(a->x < 0) a->x += MAZE_W;
    else if(a->x >= MAZE_W) a->x -= MAZE_W;
}

static int advance(uint16_t *acc, int pct)
{
    // 100% is 80.8 units a frame; the thousandths carry over
    uint32_t a = *acc + (uint32_t)pct * 808u;
    *acc = (uint16_t)(a % 1000u);
    return (int)(a / 1000u);
}

static bool toward(int32_t *v, int32_t target, int step)
{
    if(*v < target) *v = *v + step >= target ? target : *v + step;
    else if(*v > target) *v = *v - step <= target ? target : *v - step;
    return *v == target;
}

// ------------------------------------------------------------------ starting over

static void reset_actors(pgame_t *g)
{
    memset(&g->pac, 0, sizeof(g->pac));
    g->pac.x = DOOR_X;
    g->pac.y = START_Y;
    g->pac.dir = PD_LEFT;
    g->want = PD_LEFT;
    g->moving = true;
    g->stall = 0;
    for(int i = 0; i < GH_N; i++) {
        pactor_t *a = &g->gh[i];
        uint8_t dots = a->dots;
        memset(a, 0, sizeof(*a));
        a->dots = dots;
        a->x = HOME_X[i];
        a->y = i == GH_BLINKY ? EXIT_Y : HOUSE_Y;
        a->state = i == GH_BLINKY ? GS_ACTIVE : GS_HOUSE;
        a->dir = i == GH_BLINKY ? PD_LEFT : (i == GH_PINKY ? PD_DOWN : PD_UP);
        a->came = a->dir;
    }
    g->wave = 0;
    g->wave_t = WAVES[wave_set(g)][0];
    g->chase = false;
    g->fright_t = g->fright_len = 0;
    g->fright_eaten = 0;
    g->idle_t = 0;
    g->fruit_t = 0;
    g->item_t = 0;
    g->dash_t = g->magnet_t = g->freeze_t = 0;
    g->pulled_n = 0;
}

static void level_start(pgame_t *g)
{
    g->maze = (uint8_t)pac_maze_for_level(g->level);
    int n = 0;
    for(int y = 0; y < PM_H; y++) {
        for(int x = 0; x < PM_W; x++) {
            char c = PAC_MAZE[g->maze][y][x];
            g->dot[y][x] = c == '.' ? 1 : (c == 'o' ? 2 : 0);
            n += g->dot[y][x] != 0;
        }
    }
    g->dots_total = g->dots_left = (uint16_t)n;
    g->fruit_n = 0;
    g->items_n = 0;
    g->global_on = false;
    g->global_dots = 0;
    for(int i = 0; i < GH_N; i++) g->gh[i].dots = 0;
    reset_actors(g);
}

void pac_new(pgame_t *g, int mode, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->mode = (uint8_t)(mode == PM_NEON ? PM_NEON : PM_CLASSIC);
    g->level = 1;
    g->lives = PAC_LIVES;
    g->rng.s = seed ? seed : 1;
    g->next_extra = 10000;
    g->item = IT_NONE;
    level_start(g);
    g->phase = PP_READY;
    g->ready_len = 66;
}

// ------------------------------------------------------------------ scoring

static void score_add(pgame_t *g, uint32_t pts)
{
    g->score += pts;
    if(g->score > 999999990u) g->score = 999999990u;
    while(g->next_extra && g->score >= g->next_extra) {
        if(g->lives < PAC_MAX_LIVES) g->lives++;
        g->ev |= PE_EXTRA;
        // CLASSIC gives one, at 10,000; NEON one more every 30,000 after
        g->next_extra = g->mode == PM_NEON ? g->next_extra + 30000u : 0;
    }
}

// ------------------------------------------------------------------ the ghosts

static int elroy(const pgame_t *g)
{
    // Blinky's burst of speed near the end of a level, held back after a death
    // until Clyde is out again
    if(g->global_on && g->gh[GH_CLYDE].state == GS_HOUSE) return 0;
    plevel_t p = level_params(g);
    int e1 = p.elroy * g->dots_total / 244;
    if(g->dots_left <= e1 / 2) return 2;
    if(g->dots_left <= e1) return 1;
    return 0;
}

static void ghost_target(const pgame_t *g, int i, int *tx, int *ty)
{
    static const int8_t SX[GH_N] = { PM_W - 3, 2, PM_W - 1, 0 };
    static const int8_t SY[GH_N] = { -3, -3, PM_H, PM_H };
    const pactor_t *a = &g->gh[i];
    if(a->state == GS_EYES) {
        *tx = 18;
        *ty = 9;
        return;
    }
    int px = g->pac.x >> 8, py = g->pac.y >> 8, pd = g->pac.dir;
    bool chase = g->chase || (i == GH_BLINKY && elroy(g));
    if(chase) {
        switch(i) {
            case GH_BLINKY:
                *tx = px;
                *ty = py;
                return;
            case GH_PINKY:
                *tx = px + PAC_DX[pd] * 4;
                *ty = py + PAC_DY[pd] * 4;
                return;
            case GH_INKY: {
                int ax = px + PAC_DX[pd] * 2, ay = py + PAC_DY[pd] * 2;
                *tx = 2 * ax - (g->gh[GH_BLINKY].x >> 8);
                *ty = 2 * ay - (g->gh[GH_BLINKY].y >> 8);
                return;
            }
            default: {
                int dx = (a->x >> 8) - px, dy = (a->y >> 8) - py;
                if(dx * dx + dy * dy > 64) {
                    *tx = px;
                    *ty = py;
                    return;
                }
                break;
            }
        }
    }
    *tx = SX[i];
    *ty = SY[i];
}

static bool no_up(int tx, int ty)
{
    // the junctions over the house and over the start, as in the arcade
    return (ty == 9 || ty == 17) && (tx == 15 || tx == 22);
}

static int ghost_choose(pgame_t *g, int i)
{
    pactor_t *a = &g->gh[i];
    int tx = a->x >> 8, ty = a->y >> 8;
    int rev = a->dir < PD_NONE ? (a->dir ^ 2) : PD_NONE;
    if(a->fright && a->state == GS_ACTIVE) {
        // a random way, and if that is a wall the next one round
        int d = (int)(rng_next(&g->rng) & 3);
        for(int k = 0; k < 4; k++, d = (d + 1) & 3) {
            if(d != rev && pac_open(g, tx + PAC_DX[d], ty + PAC_DY[d])) return d;
        }
        return rev;
    }
    int gx, gy;
    ghost_target(g, i, &gx, &gy);
    int best = PD_NONE;
    int32_t bestd = 0x7fffffff;
    for(int d = 0; d < 4; d++) {
        if(d == rev) continue;
        int nx = tx + PAC_DX[d], ny = ty + PAC_DY[d];
        if(!pac_open(g, nx, ny)) continue;
        if(d == PD_UP && a->state == GS_ACTIVE && no_up(tx, ty)) continue;
        int32_t dx = nx - gx, dy = ny - gy;
        int32_t dist = dx * dx + dy * dy;
        if(dist < bestd) {
            bestd = dist;
            best = d;
        }
    }
    return best == PD_NONE ? rev : best;
}

static void ghost_at_centre(pgame_t *g, int i)
{
    pactor_t *a = &g->gh[i];
    if(a->state == GS_EYES && (a->y >> 8) == 9 && ((a->x >> 8) == 18 || (a->x >> 8) == 19)) {
        a->state = GS_ENTERING;
        return;
    }
    a->came = a->dir;
    a->dir = (uint8_t)ghost_choose(g, i);
}

static void ghost_walk(pgame_t *g, int i, int step)
{
    // along the corridors, choosing a way at the middle of every tile
    pactor_t *a = &g->gh[i];
    while(step > 0 && (a->state == GS_ACTIVE || a->state == GS_EYES) && a->dir < PD_NONE) {
        bool h = a->dir == PD_LEFT || a->dir == PD_RIGHT;
        int32_t *p = h ? &a->x : &a->y;
        int s = (a->dir == PD_RIGHT || a->dir == PD_DOWN) ? 1 : -1;
        int32_t c = (*p & ~(int32_t)(PT - 1)) + PT / 2;
        int32_t d = s > 0 ? (*p < c ? c - *p : c + PT - *p) : (*p > c ? *p - c : *p - c + PT);
        if(step < d) {
            *p += s * step;
            wrap(a);
            break;
        }
        *p += s * d;
        step -= (int)d;
        wrap(a);
        ghost_at_centre(g, i);
    }
}

static void ghost_house(pgame_t *g, int i, int step)
{
    pactor_t *a = &g->gh[i];
    switch(a->state) {
        case GS_HOUSE:
            // bob up and down until it is their turn
            if(a->dir == PD_UP) {
                a->y -= step;
                if(a->y <= HOUSE_Y - BOB) {
                    a->y = HOUSE_Y - BOB;
                    a->dir = PD_DOWN;
                }
            } else {
                a->y += step;
                if(a->y >= HOUSE_Y + BOB) {
                    a->y = HOUSE_Y + BOB;
                    a->dir = PD_UP;
                }
            }
            break;
        case GS_LEAVING:
            if(a->x != DOOR_X) {
                // to the middle of the house, then over to the door
                if(!toward(&a->y, HOUSE_Y, step)) {
                    a->dir = a->y < HOUSE_Y ? PD_DOWN : PD_UP;
                    break;
                }
                a->dir = a->x < DOOR_X ? PD_RIGHT : PD_LEFT;
                toward(&a->x, DOOR_X, step);
            } else {
                a->dir = PD_UP;
                if(toward(&a->y, EXIT_Y, step)) {
                    a->state = GS_ACTIVE;
                    a->dir = a->came = PD_LEFT;
                }
            }
            break;
        case GS_ENTERING:
            if(a->y == EXIT_Y && a->x != DOOR_X) {
                a->dir = a->x < DOOR_X ? PD_RIGHT : PD_LEFT;
                toward(&a->x, DOOR_X, step);
            } else if(a->y < HOUSE_Y) {
                a->dir = PD_DOWN;
                toward(&a->y, HOUSE_Y, step);
            } else if(a->x != HOME_X[i]) {
                a->dir = a->x < HOME_X[i] ? PD_RIGHT : PD_LEFT;
                toward(&a->x, HOME_X[i], step);
            } else {
                // back in one piece, and straight out again
                a->state = GS_LEAVING;
                a->fright = false;
            }
            break;
        default:
            break;
    }
}

static int ghost_pct(const pgame_t *g, int i, const plevel_t *p)
{
    const pactor_t *a = &g->gh[i];
    if(a->state == GS_EYES || a->state == GS_ENTERING) return 200;
    if(a->state == GS_HOUSE || a->state == GS_LEAVING) return 45;
    if(g->freeze_t > 0) return 0;
    if(PAC_MAZE[g->maze][a->y >> 8][wrapx(a->x >> 8)] == 't') return p->tun;
    if(a->fright) return p->gh_fr;
    if(i == GH_BLINKY) {
        int e = elroy(g);
        if(e) return e == 2 ? p->el2 : p->el1;
    }
    return p->gh;
}

static void reverse_all(pgame_t *g)
{
    for(int i = 0; i < GH_N; i++) {
        pactor_t *a = &g->gh[i];
        if(a->state != GS_ACTIVE) continue;
        // right on a tile's middle it has only chosen its next way, not gone there:
        // back the way it came, which is never a wall
        bool middle = (a->x & (PT - 1)) == PT / 2 && (a->y & (PT - 1)) == PT / 2;
        a->dir = (uint8_t)((middle ? a->came : a->dir) ^ 2);
    }
}

static int house_next(const pgame_t *g)
{
    for(int i = GH_PINKY; i <= GH_CLYDE; i++) {
        if(g->gh[i].state == GS_HOUSE) return i;
    }
    return -1;
}

static void release(pgame_t *g, int i)
{
    if(i >= 0 && g->gh[i].state == GS_HOUSE) g->gh[i].state = GS_LEAVING;
}

static void count_dot(pgame_t *g)
{
    if(g->global_on) {
        // after Pac-Man is caught the house counts for everyone
        g->global_dots++;
        if(g->global_dots == 7) release(g, GH_PINKY);
        else if(g->global_dots == 17) release(g, GH_INKY);
        else if(g->global_dots >= 32) {
            release(g, GH_CLYDE);
            g->global_on = false;
        }
    } else {
        int n = house_next(g);
        if(n >= 0 && g->gh[n].dots < 255) g->gh[n].dots++;
    }
}

// ------------------------------------------------------------------ Pac-Man

static void start_fright(pgame_t *g)
{
    plevel_t p = level_params(g);
    int f = p.fright_s * FPS;
    g->fright_eaten = 0;
    g->fright_len = g->fright_t = (int16_t)f;
    reverse_all(g);
    for(int i = 0; i < GH_N; i++) {
        pactor_t *a = &g->gh[i];
        if(f && (a->state == GS_ACTIVE || a->state == GS_HOUSE || a->state == GS_LEAVING)) a->fright = true;
    }
}

bool pac_fright_flash(const pgame_t *g)
{
    if(g->fright_t <= 0) return false;
    plevel_t p = level_params(g);
    return g->fright_t <= p.flashes * 12 && (g->fright_t % 12) < 6;
}

static void spawn_item(pgame_t *g)
{
    g->items_n++;
    int ptx = g->pac.x >> 8, pty = g->pac.y >> 8;
    for(int tries = 0; tries < 80; tries++) {
        uint32_t r = rng_next(&g->rng);
        int tx = (int)(r % PM_W), ty = (int)((r >> 8) % PM_H);
        char c = PAC_MAZE[g->maze][ty][tx];
        if(c != '.' && c != ' ') continue;
        if(iabs(tx - ptx) + iabs(ty - pty) < 10) continue;
        if(ty == 15 && (tx == 18 || tx == 19)) continue;
        g->item_x = (int8_t)tx;
        g->item_y = (int8_t)ty;
        // the first is a toss, then they take turns
        g->item_kind = g->item == IT_NONE ? (uint8_t)((r >> 20) & 1 ? IT_MAGNET : IT_FREEZE)
                                          : (uint8_t)(g->item == IT_MAGNET ? IT_FREEZE : IT_MAGNET);
        g->item = g->item_kind;
        g->item_t = ITEM_FRAMES;
        g->ev |= PE_ITEM_ON;
        return;
    }
}

static void eat_dot(pgame_t *g, int tx, int ty, bool pulled)
{
    bool power = g->dot[ty][tx] == 2;
    g->dot[ty][tx] = 0;
    g->dots_left--;
    g->idle_t = 0;
    if(!pulled) g->stall = (uint8_t)(g->stall + (power ? 3 : 1));
    score_add(g, power ? 50 : 10);
    g->ev |= power ? PE_POWER : PE_DOT;
    count_dot(g);
    if(g->mode == PM_NEON && g->dash_t == 0 && g->boost < BOOST_FULL) {
        if(++g->boost == BOOST_FULL) g->ev |= PE_CHARGED;
    }
    if(power) start_fright(g);
    int eaten = g->dots_total - g->dots_left;
    if((g->fruit_n == 0 && eaten >= g->dots_total * 70 / 244) || (g->fruit_n == 1 && eaten >= g->dots_total * 170 / 244)) {
        g->fruit_n++;
        g->fruit_t = FRUIT_FRAMES;
        g->ev |= PE_FRUIT_ON;
    }
    if(g->mode == PM_NEON && g->items_n < 3 && eaten >= g->dots_total * (g->items_n + 1) / 4) spawn_item(g);
}

static void pac_move(pgame_t *g, int step)
{
    pactor_t *p = &g->pac;
    int tx = p->x >> 8, ty = p->y >> 8;
    int w = g->want;
    if(w < PD_NONE && w != p->dir) {
        if(w == (p->dir ^ 2)) {
            p->dir = (uint8_t)w;
        } else if(pac_open(g, tx + PAC_DX[w], ty + PAC_DY[w])) {
            // near enough the middle of this tile to cut the corner
            int off = (w == PD_LEFT || w == PD_RIGHT) ? (p->y & (PT - 1)) - PT / 2 : (p->x & (PT - 1)) - PT / 2;
            if(iabs(off) <= TURN_WIN) p->dir = (uint8_t)w;
        }
    }
    if(step <= 0) return;
    bool h = p->dir == PD_LEFT || p->dir == PD_RIGHT;
    // cutting a corner: close in on the middle of the new corridor while moving along it
    int32_t *q = h ? &p->y : &p->x;
    toward(q, (*q & ~(int32_t)(PT - 1)) + PT / 2, step);
    int32_t *a = h ? &p->x : &p->y;
    int s = (p->dir == PD_RIGHT || p->dir == PD_DOWN) ? 1 : -1;
    int32_t c = (*a & ~(int32_t)(PT - 1)) + PT / 2;
    int32_t na = *a + s * step;
    g->moving = true;
    if(!pac_open(g, tx + PAC_DX[p->dir], ty + PAC_DY[p->dir]) &&
       ((s > 0 && *a <= c && na >= c) || (s < 0 && *a >= c && na <= c))) {
        // a wall ahead: stop in the middle of the tile
        g->moving = *a != c;
        na = c;
    }
    *a = na;
    wrap(p);
}

static void pac_eat(pgame_t *g)
{
    int tx = g->pac.x >> 8, ty = g->pac.y >> 8;
    if(g->dot[ty][tx]) eat_dot(g, tx, ty, false);
    if(g->fruit_t > 0 && ty == 15 && iabs(g->pac.x - DOOR_X) < PT / 2 + 32) {
        int f = pac_fruit_for_level(g->level);
        g->fruit_t = 0;
        g->fruits_eaten++;
        score_add(g, PAC_FRUIT_PTS[f]);
        g->ev |= PE_FRUIT;
        g->ev_pts = PAC_FRUIT_PTS[f];
        g->ev_x = DOOR_X;
        g->ev_y = FRUIT_Y;
    }
    if(g->item_t > 0 && tx == g->item_x && ty == g->item_y) {
        g->item_t = 0;
        if(g->item_kind == IT_MAGNET) g->magnet_t = MAGNET_FRAMES;
        else g->freeze_t = FREEZE_FRAMES;
        score_add(g, 200);
        g->ev |= PE_ITEM;
        g->ev_pts = 200;
        g->ev_x = tx * PT + PT / 2;
        g->ev_y = ty * PT + PT / 2;
    }
    g->pulled_n = 0;
    if(g->magnet_t > 0) {
        // the magnet takes the dots around (not the energizers)
        for(int dy = -2; dy <= 2; dy++) {
            int y = ty + dy;
            if(y < 0 || y >= PM_H) continue;
            for(int dx = -3; dx <= 3; dx++) {
                if(dx * dx + dy * dy > 9) continue;
                int x = wrapx(tx + dx);
                if(g->dot[y][x] != 1 || g->pulled_n >= 8) continue;
                g->pulled[g->pulled_n][0] = (int8_t)x;
                g->pulled[g->pulled_n][1] = (int8_t)y;
                g->pulled_n++;
                eat_dot(g, x, y, true);
            }
        }
    }
}

static void eat_ghost(pgame_t *g, int i, bool shatter)
{
    pactor_t *a = &g->gh[i];
    uint16_t pts;
    if(shatter) {
        pts = 500;
        g->ev |= PE_SHATTER;
    } else {
        pts = (uint16_t)(200u << (g->fright_eaten > 3 ? 3 : g->fright_eaten));
        g->fright_eaten++;
        g->ev |= PE_GHOST;
    }
    g->ghosts_eaten++;
    a->state = GS_EYES;
    a->fright = false;
    score_add(g, pts);
    g->ev_ghost = (uint8_t)i;
    g->ev_pts = pts;
    g->ev_x = a->x;
    g->ev_y = a->y;
    if(!shatter && g->fright_eaten == 4 && g->mode == PM_NEON) {
        score_add(g, 3000);
        g->ev |= PE_ALL4;
    }
    g->phase = PP_EAT;
    g->phase_t = EAT_FRAMES;
}

static bool collide(pgame_t *g)
{
    for(int i = 0; i < GH_N; i++) {
        pactor_t *a = &g->gh[i];
        if(a->state != GS_ACTIVE) continue;
        int32_t dx = a->x - g->pac.x, dy = a->y - g->pac.y;
        if(dx > MAZE_W / 2) dx -= MAZE_W;
        else if(dx < -MAZE_W / 2) dx += MAZE_W;
        if(iabs(dx) >= PT / 2 || iabs(dy) >= PT / 2) continue;
        if(a->fright) {
            eat_ghost(g, i, false);
            return true;
        }
        if(g->freeze_t > 0) {
            eat_ghost(g, i, true);
            return true;
        }
        if(g->dash_t > 0) continue;         // dashing straight through
        g->phase = PP_DYING;
        g->phase_t = 0;
        g->dash_t = g->magnet_t = 0;
        g->ev |= PE_CAUGHT;
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ a frame

static void play_tick(pgame_t *g)
{
    plevel_t p = level_params(g);
    g->frames++;

    // energizer, then the scatter / chase clock (which waits while the ghosts are blue)
    if(g->fright_t > 0) {
        if(--g->fright_t == 0) {
            for(int i = 0; i < GH_N; i++) g->gh[i].fright = false;
            g->ev |= PE_FRIGHT_END;
        }
    } else if(g->wave < 7 && --g->wave_t <= 0) {
        g->wave++;
        g->chase = (g->wave & 1) != 0;
        if(g->wave < 7) g->wave_t = WAVES[wave_set(g)][g->wave];
        reverse_all(g);
        g->ev |= PE_WAVE;
    }
    if(g->fruit_t > 0) g->fruit_t--;
    if(g->item_t > 0) g->item_t--;
    if(g->dash_t > 0) g->dash_t--;
    if(g->magnet_t > 0) g->magnet_t--;
    if(g->freeze_t > 0 && --g->freeze_t == 0) g->ev |= PE_THAW;

    // the ghost house lets them out by dots eaten, or when Pac-Man stops eating
    if(!g->global_on) {
        int n = house_next(g);
        int lv = g->level > 3 ? 2 : g->level - 1;
        if(n >= 0 && g->gh[n].dots >= HOUSE_LIMIT[lv][n]) release(g, n);
    }
    if(++g->idle_t >= (g->level < 5 ? 4 : 3) * FPS) {
        g->idle_t = 0;
        release(g, house_next(g));
    }

    // Pac-Man
    int pct = g->fright_t > 0 ? p.pac_fr : p.pac;
    if(g->dash_t > 0) pct = pct * 3 / 2;
    int step = advance(&g->pac.acc, pct);
    if(g->dash_t > 0) {
        g->stall = 0;
    } else if(g->stall >= 2) {
        step = 0;
        g->stall -= 2;
    } else if(g->stall == 1) {
        step /= 2;
        g->stall = 0;
    }
    pac_move(g, step);
    pac_eat(g);
    if(g->dots_left == 0) {
        g->phase = PP_CLEAR;
        g->phase_t = 0;
        g->fright_t = 0;
        g->dash_t = g->magnet_t = g->freeze_t = 0;
        for(int i = 0; i < GH_N; i++) g->gh[i].fright = false;
        g->ev |= PE_CLEAR;
        return;
    }
    if(collide(g)) return;

    // the ghosts
    for(int i = 0; i < GH_N; i++) {
        int s = advance(&g->gh[i].acc, ghost_pct(g, i, &p));
        if(g->gh[i].state == GS_ACTIVE || g->gh[i].state == GS_EYES) ghost_walk(g, i, s);
        else ghost_house(g, i, s);
    }
    collide(g);
}

uint32_t pac_step(pgame_t *g, uint32_t keys)
{
    g->ev = 0;
    g->pulled_n = 0;
    if(g->phase == PP_OVER) return 0;
    uint8_t k = (uint8_t)(keys & PK_ALL);
    uint8_t pressed = k & (uint8_t)~g->keys_prev;
    g->keys_prev = k;
    // a tap is remembered until the corner comes; a key held keeps asking
    int held = -1, nheld = 0;
    for(int d = 0; d < 4; d++) {
        if(pressed & KEY_DIR[d]) g->want = (uint8_t)d;
        if(k & KEY_DIR[d]) {
            held = d;
            nheld++;
        }
    }
    if(nheld == 1 && !(pressed & (PK_UP | PK_LEFT | PK_DOWN | PK_RIGHT))) g->want = (uint8_t)held;
    if((pressed & PK_DASH) && g->mode == PM_NEON && g->phase == PP_PLAY && g->boost >= BOOST_FULL && g->dash_t == 0) {
        g->dash_t = DASH_FRAMES;
        g->boost = 0;
        g->ev |= PE_DASH;
    }

    switch(g->phase) {
        case PP_READY:
            if(++g->phase_t >= g->ready_len) {
                g->phase = PP_PLAY;
                g->phase_t = 0;
                g->ev |= PE_GO;
            }
            break;
        case PP_PLAY:
            play_tick(g);
            break;
        case PP_EAT:
            if(--g->phase_t <= 0) g->phase = PP_PLAY;
            break;
        case PP_DYING:
            g->phase_t++;
            if(g->phase_t == DIE_FREEZE) g->ev |= PE_DEATH;
            if(g->phase_t >= DIE_FREEZE + DIE_ANIM) {
                g->lives--;
                g->ev |= PE_LIFE_LOST;
                if(g->lives == 0) {
                    g->over = true;
                    g->phase = PP_OVER;
                    g->ev |= PE_OVER;
                } else {
                    reset_actors(g);
                    g->global_on = true;
                    g->global_dots = 0;
                    g->phase = PP_READY;
                    g->phase_t = 0;
                    g->ready_len = 45;
                }
            }
            break;
        case PP_CLEAR:
            if(++g->phase_t >= CLEAR_FREEZE + CLEAR_FLASH) {
                if(g->level < PAC_MAX_LEVEL) g->level++;
                level_start(g);
                g->phase = PP_READY;
                g->phase_t = 0;
                g->ready_len = 45;
                g->ev |= PE_LEVEL;
            }
            break;
        default:
            break;
    }
    return g->ev;
}

// ------------------------------------------------------------------ suspend / resume
//
// version(1) mode(1) level(1) lives(1) fruit_n(1) items_n(1) boost(1) item(1)
// score(4) next_extra(4) frames(4) rng(4) ghosts(2) fruits(1) reserved(4)
// then a bit for every tile, set where a dot or energizer is left (129 bytes)

#define PACK_HEAD   31

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int pac_pack(const pgame_t *g, uint8_t *buf, int max)
{
    if(max < PAC_PACK_LEN) return 0;
    memset(buf, 0, PAC_PACK_LEN);
    buf[0] = 1;
    buf[1] = g->mode;
    buf[2] = g->level;
    buf[3] = g->lives;
    buf[4] = g->fruit_n;
    buf[5] = g->items_n;
    buf[6] = g->boost;
    buf[7] = g->item;
    put32(buf + 8, g->score);
    put32(buf + 12, g->next_extra);
    put32(buf + 16, g->frames);
    put32(buf + 20, g->rng.s);
    buf[24] = (uint8_t)g->ghosts_eaten;
    buf[25] = (uint8_t)(g->ghosts_eaten >> 8);
    buf[26] = g->fruits_eaten;
    for(int i = 0; i < PM_W * PM_H; i++) {
        if(g->dot[i / PM_W][i % PM_W]) buf[PACK_HEAD + (i >> 3)] |= (uint8_t)(1u << (i & 7));
    }
    return PAC_PACK_LEN;
}

bool pac_unpack(pgame_t *g, const uint8_t *buf, int len)
{
    if(len < PAC_PACK_LEN || buf[0] != 1 || buf[1] >= PM_MODES) return false;
    int level = buf[2], lives = buf[3];
    if(level < 1 || level > PAC_MAX_LEVEL || lives < 1 || lives > PAC_MAX_LIVES) return false;
    if(buf[4] > 2 || buf[5] > 3 || buf[6] > BOOST_FULL || buf[7] > IT_FREEZE) return false;
    if(buf[27] | buf[28] | buf[29] | buf[30]) return false;
    uint32_t score = get32(buf + 8), extra = get32(buf + 12), rs = get32(buf + 20);
    if(score > 999999990u || score % 10 || !rs) return false;
    if(extra == 0 ? buf[1] != PM_CLASSIC : (extra <= score || extra - score > 40000u || extra % 10000u)) return false;

    pgame_t t;
    memset(&t, 0, sizeof(t));
    t.mode = buf[1];
    t.level = (uint8_t)level;
    level_start(&t);
    // only dots the maze has, and at least one of them
    int left = 0;
    for(int i = 0; i < PM_W * PM_H + 6; i++) {
        bool on = (buf[PACK_HEAD + (i >> 3)] >> (i & 7)) & 1;
        if(i >= PM_W * PM_H) {
            if(on) return false;
            continue;
        }
        uint8_t *d = &t.dot[i / PM_W][i % PM_W];
        if(on && !*d) return false;
        if(!on) *d = 0;
        left += on;
    }
    if(!left) return false;
    t.dots_left = (uint16_t)left;
    t.lives = (uint8_t)lives;
    t.fruit_n = buf[4];
    t.items_n = buf[5];
    t.boost = buf[6];
    t.item = buf[7];
    t.score = score;
    t.next_extra = extra;
    t.frames = get32(buf + 16);
    t.rng.s = rs;
    t.ghosts_eaten = (uint16_t)(buf[24] | (buf[25] << 8));
    t.fruits_eaten = buf[26];
    // the ghosts come out one by one, as after a death
    t.global_on = true;
    t.phase = PP_READY;
    t.ready_len = 60;
    *g = t;
    return true;
}

// ------------------------------------------------------------------ demo player
//
// A flood fill from the ghosts says how soon one could reach each tile. Then for
// each way Pac-Man could go, a second fill walks only where he would get there
// well first: how much room that leaves him, and how near the closest thing worth
// eating is. He takes the way with room to spare and food close by; when none
// has room, the way that keeps him furthest from the ghosts.

#define NT      (PM_W * PM_H)
#define FAR     9999
#define AI_CAP      160         // the most room worth counting
#define AI_ROOM     60          // ... when no ghost is near
#define AI_MARGIN   2           // tiles he must be ahead of a ghost to call a tile safe
#define AI_SLOW     6           // and one more for every six, as eating slows him down
#define AI_FOODW    6
#define AI_HORIZON  72          // frames each way is played forward
#define AI_GAINW    2
#define AI_NEAR     14          // how close a ghost must be to bother playing it forward
static int16_t s_gd[NT], s_pd[NT], s_q[NT];

static void fill_ghosts(const pgame_t *g)
{
    int qh = 0, qt = 0;
    for(int i = 0; i < NT; i++) s_gd[i] = FAR;
    for(int i = 0; i < GH_N; i++) {
        const pactor_t *a = &g->gh[i];
        int tx, ty, d0 = 0;
        if(a->state == GS_LEAVING || a->state == GS_HOUSE) {
            // about to come out of the door
            tx = 18;
            ty = 9;
            d0 = a->state == GS_LEAVING ? 2 : 6;
        } else if(a->state == GS_ACTIVE && g->freeze_t <= 20 && !(a->fright && g->fright_t > 45)) {
            tx = a->x >> 8;
            ty = a->y >> 8;
        } else {
            continue;
        }
        int t = ty * PM_W + tx;
        if(s_gd[t] > d0) {
            s_gd[t] = (int16_t)d0;
            s_q[qt++] = (int16_t)t;
        }
        // and the tile it is heading into
        if(a->state == GS_ACTIVE && a->dir < PD_NONE && pac_open(g, tx + PAC_DX[a->dir], ty + PAC_DY[a->dir])) {
            int n = (ty + PAC_DY[a->dir]) * PM_W + wrapx(tx + PAC_DX[a->dir]);
            if(s_gd[n] > 0) {
                s_gd[n] = 0;
                s_q[qt++] = (int16_t)n;
            }
        }
    }
    while(qh < qt) {
        int t = s_q[qh++];
        int tx = t % PM_W, ty = t / PM_W;
        for(int d = 0; d < 4; d++) {
            if(!pac_open(g, tx + PAC_DX[d], ty + PAC_DY[d])) continue;
            int n = (ty + PAC_DY[d]) * PM_W + wrapx(tx + PAC_DX[d]);
            if(s_gd[n] > s_gd[t] + 1 && qt < NT) {
                s_gd[n] = (int16_t)(s_gd[t] + 1);
                s_q[qt++] = (int16_t)n;
            }
        }
    }
}

static bool ai_wants(const pgame_t *g, int t, int dist, bool threatened, int power_left)
{
    int tx = t % PM_W, ty = t / PM_W;
    uint8_t d = g->dot[ty][tx];
    if(d == 1) return true;
    // energizers are for when a ghost is close, or when they are all that is left
    if(d == 2 && (threatened || power_left == g->dots_left)) return true;
    if(g->item_t > dist * 4 + 10 && tx == g->item_x && ty == g->item_y) return true;
    if(g->fruit_t > dist * 4 + 10 && ty == 15 && (tx == 18 || tx == 19)) return true;
    for(int i = 0; i < GH_N; i++) {
        const pactor_t *a = &g->gh[i];
        if(a->state != GS_ACTIVE) continue;
        bool edible = (a->fright && g->fright_t > dist * 5 + 20) || g->freeze_t > dist * 4 + 15;
        if(edible && (a->x >> 8) == tx && (a->y >> 8) == ty) return true;
    }
    return false;
}

// room (tiles he can reach safely, up to a limit) and the distance to food, going first to n
static void ai_explore(const pgame_t *g, int pt, int n, bool threatened, int power_left, int *room, int *food)
{
    for(int i = 0; i < NT; i++) s_pd[i] = FAR;
    int qh = 0, qt = 0;
    s_pd[pt] = 0;
    s_pd[n] = 1;
    s_q[qt++] = (int16_t)n;
    *food = FAR;
    while(qh < qt && qt < AI_CAP) {
        int t = s_q[qh++];
        if(*food == FAR && ai_wants(g, t, s_pd[t], threatened, power_left)) *food = s_pd[t];
        int tx = t % PM_W, ty = t / PM_W;
        for(int d = 0; d < 4; d++) {
            if(!pac_open(g, tx + PAC_DX[d], ty + PAC_DY[d])) continue;
            int m = (ty + PAC_DY[d]) * PM_W + wrapx(tx + PAC_DX[d]);
            if(s_pd[m] != FAR || s_pd[t] + AI_MARGIN + s_pd[t] / AI_SLOW >= s_gd[m]) continue;
            s_pd[m] = (int16_t)(s_pd[t] + 1);
            s_q[qt++] = (int16_t)m;
        }
    }
    *room = qt;
    // food further on, past the edge of the room counted
    for(int i = qh; i < qt && *food == FAR; i++) {
        if(ai_wants(g, s_q[i], s_pd[s_q[i]], threatened, power_left)) *food = s_pd[s_q[i]];
    }
}

static int ai_rollout_dir(const pgame_t *g)
{
    // quick and cheap: keep going, and at a turning take the way furthest from a ghost
    int ptx = g->pac.x >> 8, pty = g->pac.y >> 8;
    int rev = g->pac.dir ^ 2, go = g->pac.dir, best = -1;
    for(int d = 0; d < 4; d++) {
        if(d == rev || !pac_open(g, ptx + PAC_DX[d], pty + PAC_DY[d])) continue;
        int nx = wrapx(ptx + PAC_DX[d]), ny = pty + PAC_DY[d];
        int near = 99;
        for(int i = 0; i < GH_N; i++) {
            const pactor_t *a = &g->gh[i];
            if(a->state != GS_ACTIVE || (a->fright && g->fright_t > 30) || g->freeze_t > 10) continue;
            int dx = iabs((a->x >> 8) - nx), dy = iabs((a->y >> 8) - ny);
            if(dx > PM_W / 2) dx = PM_W - dx;
            if(dx + dy < near) near = dx + dy;
        }
        int v = near * 4 + (g->dot[ny][nx] != 0) * 2 + (d == g->pac.dir);
        if(v > best) {
            best = v;
            go = d;
        }
    }
    return go;
}

static int ai_rollout(const pgame_t *g, int d, int *gain)
{
    // play it forward: how long does he last going this way?
    static pgame_t sim;
    sim = *g;
    for(int f = 0; f < AI_HORIZON; f++) {
        int w = f < 4 ? d : ai_rollout_dir(&sim);
        pac_step(&sim, KEY_DIR[w]);
        if(sim.phase == PP_DYING || sim.phase == PP_OVER) {
            *gain = 0;
            return f;
        }
        if(sim.phase == PP_CLEAR || sim.phase == PP_READY) break;
    }
    *gain = (int)(sim.score - g->score);
    return AI_HORIZON;
}

uint32_t pac_ai_keys(pgame_t *g, pai_t *ai)
{
    if(g->phase != PP_PLAY) return g->phase == PP_READY ? KEY_DIR[PD_LEFT] : 0;
    int ptx0 = g->pac.x >> 8, pty0 = g->pac.y >> 8;
    if(ptx0 + pty0 * PM_W == ai->tile && ai->age < 4 && ai->dir < PD_NONE) {
        ai->age++;
        return KEY_DIR[ai->dir] | ai->dash;
    }
    fill_ghosts(g);
    int ptx = g->pac.x >> 8, pty = g->pac.y >> 8;
    int pt = pty * PM_W + ptx;
    bool threatened = s_gd[pt] <= 7;
    int power_left = 0;
    for(int y = 0; y < PM_H; y++) {
        for(int x = 0; x < PM_W; x++) power_left += g->dot[y][x] == 2;
    }
    int go = PD_NONE, best = -0x7fffffff;
    for(int d = 0; d < 4; d++) {
        if(!pac_open(g, ptx + PAC_DX[d], pty + PAC_DY[d])) continue;
        int n = (pty + PAC_DY[d]) * PM_W + wrapx(ptx + PAC_DX[d]);
        int score;
        if(s_gd[n] <= 2) {
            // a ghost is right there: only if there is nowhere else
            score = -100000 + s_gd[n] * 100;
        } else {
            int room, food;
            ai_explore(g, pt, n, threatened, power_left, &room, &food);
            int cap = threatened ? AI_CAP : AI_ROOM;
            score = (room >= cap ? cap : room) * 10 - (food == FAR ? 400 : food * AI_FOODW) + s_gd[n];
        }
        if(s_gd[pt] <= AI_NEAR) {
            // a ghost within reach: try it out before committing
            int gain;
            int lasted = ai_rollout(g, d, &gain);
            if(lasted < AI_HORIZON) score = -1000000 + lasted * 1000 + score / 1000;
            else score += gain * AI_GAINW;
        }
        if(d == g->pac.dir) score += 5;
        if(score > best) {
            best = score;
            go = d;
        }
    }
    uint32_t keys = go < PD_NONE ? KEY_DIR[go] : 0;
    // NEON: dash out of trouble
    ai->dash = 0;
    if(g->mode == PM_NEON && g->boost >= BOOST_FULL && (s_gd[pt] <= 3 || best < -500000) && !(ai->last & PK_DASH)) {
        keys |= PK_DASH;
    }
    ai->last = (uint8_t)keys;
    ai->tile = (int16_t)pt;
    ai->dir = (uint8_t)go;
    ai->age = 0;
    return keys;
}
