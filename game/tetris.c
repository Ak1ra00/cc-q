// TETRIS - the rules. See tetris.h.
#include "tetris.h"
#include <string.h>

// SRS pieces in their boxes, state 0 (spawn), R, 2, L; rows count down
const int8_t TET_SHAPE[PC_KINDS][4][4][2] = {
    { { { 0 } } },
    { // I
        { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 } },
        { { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 } },
        { { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 } },
        { { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 } },
    },
    { // O
        { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
        { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
        { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
        { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } },
    },
    { // T
        { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
        { { 1, 0 }, { 1, 1 }, { 2, 1 }, { 1, 2 } },
        { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 1, 2 } },
        { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 } },
    },
    { // S
        { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 } },
        { { 1, 0 }, { 1, 1 }, { 2, 1 }, { 2, 2 } },
        { { 1, 1 }, { 2, 1 }, { 0, 2 }, { 1, 2 } },
        { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 } },
    },
    { // Z
        { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } },
        { { 2, 0 }, { 1, 1 }, { 2, 1 }, { 1, 2 } },
        { { 0, 1 }, { 1, 1 }, { 1, 2 }, { 2, 2 } },
        { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 0, 2 } },
    },
    { // J
        { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
        { { 1, 0 }, { 2, 0 }, { 1, 1 }, { 1, 2 } },
        { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 2, 2 } },
        { { 1, 0 }, { 1, 1 }, { 0, 2 }, { 1, 2 } },
    },
    { // L
        { { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
        { { 1, 0 }, { 1, 1 }, { 1, 2 }, { 2, 2 } },
        { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 0, 2 } },
        { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 1, 2 } },
    },
    { { { 0 } } },
};

// SRS wall kicks, [from state][0 = clockwise, 1 = counter-clockwise][test] = { x, y },
// written as in the guideline with y pointing up
static const int8_t KICK_JLSTZ[4][2][5][2] = {
    { { { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } },       // 0 -> R
      { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } } },        // 0 -> L
    { { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } },           // R -> 2
      { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } } },         // R -> 0
    { { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } },          // 2 -> L
      { { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } } },     // 2 -> R
    { { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } },        // L -> 0
      { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } } },      // L -> 2
};

static const int8_t KICK_I[4][2][5][2] = {
    { { { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 } },         // 0 -> R
      { { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 } } },       // 0 -> L
    { { { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 } },         // R -> 2
      { { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 } } },       // R -> 0
    { { { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 } },         // 2 -> L
      { { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 } } },       // 2 -> R
    { { { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 } },         // L -> 0
      { { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 } } },       // L -> 2
};

// guideline gravity, (0.8 - (level - 1) * 0.007) ^ (level - 1) seconds a row,
// as rows a frame at 30 fps in 16.16; level 18 and up is 20G
static const int32_t GRAVITY[20] = {
    2185, 2755, 3536, 4621, 6150, 8338, 11517, 16214, 23269, 34053,
    50831, 77417, 120338, 190967, 309485, 512373, 866849, 1310720, 1310720, 1310720,
};

#define ONE_ROW         65536
#define SOFT_MIN        (ONE_ROW * 2 / 3)       // soft drop: at least 20 rows a second
#define ENTRY_DELAY     1

int32_t tet_gravity(int level)
{
    return GRAVITY[iclamp(level, 1, 20) - 1];
}

bool tet_fits(const tgame_t *g, int type, int rot, int x, int y)
{
    const int8_t (*c)[2] = TET_SHAPE[type][rot & 3];
    for(int i = 0; i < 4; i++) {
        int cx = x + c[i][0], cy = y + c[i][1];
        if(cx < 0 || cx >= TB_W || cy >= TB_H) return false;
        if(cy >= 0 && g->cell[cy][cx]) return false;
    }
    return true;
}

int tet_ghost_y(const tgame_t *g)
{
    int y = g->y;
    while(tet_fits(g, g->type, g->rot, g->x, y + 1)) y++;
    return y;
}

int tet_stack_height(const tgame_t *g)
{
    for(int y = 0; y < TB_H; y++) {
        for(int x = 0; x < TB_W; x++) {
            if(g->cell[y][x]) return TB_H - y;
        }
    }
    return 0;
}

// ------------------------------------------------------------------ the queue

static void bag_refill(tgame_t *g)
{
    for(int i = 0; i < 7; i++) g->bag[i] = (uint8_t)(PC_I + i);
    for(int i = 6; i > 0; i--) {
        int j = (int)(rng_next(&g->rng) % (uint32_t)(i + 1));
        uint8_t t = g->bag[i];
        g->bag[i] = g->bag[j];
        g->bag[j] = t;
    }
    g->bag_n = 7;
}

static uint8_t bag_take(tgame_t *g)
{
    if(!g->bag_n) bag_refill(g);
    return g->bag[--g->bag_n];
}

static uint8_t queue_pop(tgame_t *g)
{
    uint8_t t = g->next[0];
    memmove(g->next, g->next + 1, NEXT_N - 1);
    g->next[NEXT_N - 1] = bag_take(g);
    return t;
}

// ------------------------------------------------------------------ the piece

static bool on_ground(const tgame_t *g)
{
    return !tet_fits(g, g->type, g->rot, g->x, g->y + 1);
}

static uint32_t spawn(tgame_t *g, int type)
{
    g->type = (int8_t)type;
    g->rot = 0;
    g->x = (int8_t)(type == PC_O ? 4 : 3);
    // just above the visible field: the I lies flat in its box's second row
    g->y = (int8_t)(type == PC_I ? TB_HIDDEN - 3 : TB_HIDDEN - 2);
    g->fall = 0;
    g->lock_t = 0;
    g->lock_moves = 0;
    g->last_rot = false;
    g->last_kick = 0;
    g->active = true;
    if(!tet_fits(g, type, 0, g->x, g->y)) {
        // block out: no room for the next piece
        g->active = false;
        g->over = true;
        return TE_TOPOUT;
    }
    // and it steps down into view straight away if it can
    if(tet_fits(g, type, 0, g->x, g->y + 1)) g->y++;
    g->lowest = g->y;
    return TE_SPAWN;
}

static void moved(tgame_t *g, bool was_on_ground)
{
    // move reset: every move or turn on the ground buys time, fifteen times
    if(was_on_ground || g->lock_t) {
        g->lock_t = 0;
        if(g->lock_moves < 255) g->lock_moves++;
    }
}

static bool shift(tgame_t *g, int dx)
{
    if(!tet_fits(g, g->type, g->rot, g->x + dx, g->y)) return false;
    bool ground = on_ground(g);
    g->x = (int8_t)(g->x + dx);
    g->last_rot = false;
    moved(g, ground);
    return true;
}

static bool rotate(tgame_t *g, int dir)
{
    if(g->type == PC_O) return false;
    int from = g->rot, to = (from + (dir > 0 ? 1 : 3)) & 3;
    const int8_t (*k)[2] = g->type == PC_I ? KICK_I[from][dir > 0 ? 0 : 1] : KICK_JLSTZ[from][dir > 0 ? 0 : 1];
    for(int i = 0; i < 5; i++) {
        int nx = g->x + k[i][0], ny = g->y - k[i][1];
        if(tet_fits(g, g->type, to, nx, ny)) {
            bool ground = on_ground(g);
            g->x = (int8_t)nx;
            g->y = (int8_t)ny;
            g->rot = (int8_t)to;
            g->last_rot = true;
            g->last_kick = (int8_t)i;
            moved(g, ground);
            if(g->y > g->lowest) {
                g->lowest = g->y;
                g->lock_moves = 0;
            }
            return true;
        }
    }
    return false;
}

static bool solid(const tgame_t *g, int x, int y)
{
    if(x < 0 || x >= TB_W || y >= TB_H) return true;
    return y >= 0 && g->cell[y][x];
}

static int tspin_kind(const tgame_t *g)
{
    // three of the four corners around the T's centre, and it got there by turning
    if(g->type != PC_T || !g->last_rot) return SPIN_NONE;
    static const int8_t corner[4][2] = { { 0, 0 }, { 2, 0 }, { 2, 2 }, { 0, 2 } };
    bool c[4];
    int n = 0;
    for(int i = 0; i < 4; i++) {
        c[i] = solid(g, g->x + corner[i][0], g->y + corner[i][1]);
        n += c[i];
    }
    if(n < 3) return SPIN_NONE;
    // the two corners either side of where the T points
    bool front = c[g->rot] && c[(g->rot + 1) & 3];
    return (front || g->last_kick == 4) ? SPIN_FULL : SPIN_MINI;
}

static uint32_t lock_piece(tgame_t *g)
{
    uint32_t ev = TE_LOCK;
    int spin = tspin_kind(g);
    const int8_t (*c)[2] = TET_SHAPE[g->type][g->rot];
    bool visible = false;
    g->lk_type = g->type;
    g->lk_rot = g->rot;
    g->lk_x = g->x;
    g->lk_y = g->y;
    g->active = false;
    g->pieces++;
    for(int i = 0; i < 4; i++) {
        int cx = g->x + c[i][0], cy = g->y + c[i][1];
        if(cy < 0) continue;
        g->cell[cy][cx] = (uint8_t)g->type;
        if(cy >= TB_HIDDEN) visible = true;
    }
    if(!visible) {
        // lock out: it came to rest entirely above the field
        g->over = true;
        return ev | TE_TOPOUT;
    }

    // full rows, top to bottom
    int n = 0;
    int left = 0;
    for(int y = 0; y < TB_H; y++) {
        int k = 0;
        for(int x = 0; x < TB_W; x++) k += g->cell[y][x] != 0;
        if(k == TB_W && n < 4) g->clear_row[n++] = (uint8_t)y;
        else left += k;
    }
    g->clear_n = (uint8_t)n;

    // score it, guideline style
    static const uint16_t PLAIN[5] = { 0, 100, 300, 500, 800 };
    static const uint16_t MINI[3] = { 100, 200, 400 };
    static const uint16_t FULL[4] = { 400, 800, 1200, 1600 };
    static const uint16_t PERFECT[5] = { 0, 800, 1200, 1800, 2000 };
    uint32_t lv = g->mode == TM_MARATHON ? g->level : 1;
    uint32_t pts;
    bool hard;
    if(spin == SPIN_FULL) {
        pts = FULL[n > 3 ? 3 : n];
        hard = n > 0;
    } else if(spin == SPIN_MINI) {
        pts = MINI[n > 2 ? 2 : n];
        hard = n > 0;
    } else {
        pts = PLAIN[n];
        hard = n == 4;
    }
    pts *= lv;
    g->lk_b2b = false;
    g->lk_pc = false;
    if(n) {
        if(hard && g->b2b) {
            pts = pts * 3 / 2;
            g->lk_b2b = true;
        }
        g->b2b = hard;
        g->combo++;
        if(g->combo > 0) pts += 50u * (uint32_t)g->combo * lv;
        if((uint16_t)g->combo > g->best_combo) g->best_combo = (uint16_t)g->combo;
        if(!left) {
            g->lk_pc = true;
            pts += (n == 4 && g->lk_b2b ? 3200u : PERFECT[n]) * lv;
        }
        if(n == 4) g->n_tetris++;
        g->lines = (uint16_t)(g->lines + n);
        g->wait = CLEAR_FRAMES;
        ev |= TE_CLEAR;
    } else {
        g->combo = -1;
        g->wait = ENTRY_DELAY;
    }
    if(spin) g->n_tspin++;
    g->lk_lines = (uint8_t)n;
    g->lk_spin = (uint8_t)spin;
    g->lk_combo = g->combo;
    g->lk_points = pts;
    g->score += pts;
    if(g->score > 99999999u) g->score = 99999999u;

    if(g->mode == TM_MARATHON) {
        int lvl = g->start_level + g->lines / 10;
        if(lvl > MAX_LEVEL) lvl = MAX_LEVEL;
        if(lvl > g->level) {
            g->level = (uint8_t)lvl;
            ev |= TE_LEVELUP;
        }
    }
    return ev;
}

static void collapse(tgame_t *g)
{
    int w = TB_H - 1;
    for(int y = TB_H - 1; y >= 0; y--) {
        bool gone = false;
        for(int i = 0; i < g->clear_n; i++) gone |= g->clear_row[i] == y;
        if(gone) continue;
        if(w != y) memcpy(g->cell[w], g->cell[y], TB_W);
        w--;
    }
    for(; w >= 0; w--) memset(g->cell[w], 0, TB_W);
    g->clear_n = 0;
}

void tet_new(tgame_t *g, int mode, int level, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->rng.s = seed ? seed : 0x7e7215u;
    g->mode = (uint8_t)mode;
    g->start_level = g->level = (uint8_t)(mode == TM_MARATHON ? iclamp(level, 1, 15) : 1);
    g->combo = -1;
    g->hold = PC_NONE;
    g->das = 5;
    g->arr = 1;
    g->keys_prev = TK_ALL;
    g->lk_type = PC_NONE;
    for(int i = 0; i < NEXT_N; i++) g->next[i] = bag_take(g);
    g->wait = 0;
}

static uint32_t next_piece(tgame_t *g)
{
    g->hold_used = false;
    return spawn(g, queue_pop(g));
}

void tet_settle(tgame_t *g)
{
    if(g->over || g->active) return;
    if(g->clear_n) collapse(g);
    g->wait = 0;
    g->buf = 0;
    g->buf_dir = 0;
    next_piece(g);
}

uint32_t tet_step(tgame_t *g, uint32_t keys)
{
    if(g->over) return 0;
    uint32_t ev = 0;
    uint32_t pressed = keys & ~(uint32_t)g->keys_prev;
    g->keys_prev = (uint8_t)keys;
    g->frames++;
    if(g->mode == TM_ULTRA && g->frames >= ULTRA_FRAMES) {
        g->over = g->won = true;
        g->active = false;
        return TE_GOAL;
    }

    // sideways auto-repeat keeps charging between pieces; with both held, the newer wins
    int dir = 0;
    if(keys & TK_LEFT) dir = -1;
    if(keys & TK_RIGHT) dir = 1;
    if((keys & (TK_LEFT | TK_RIGHT)) == (TK_LEFT | TK_RIGHT)) {
        dir = (pressed & TK_LEFT) ? -1 : ((pressed & TK_RIGHT) ? 1 : g->das_dir);
    }
    int tap = 0;
    if(dir != g->das_dir) {
        g->das_dir = (int8_t)dir;
        g->das_t = 0;
        tap = dir;
    } else if(dir && g->das_t < 1000) {
        g->das_t++;
    }

    if(!g->active) {
        // turns, holds and taps made between pieces are kept for the next one
        g->buf |= (uint8_t)(pressed & (TK_CW | TK_CCW | TK_HOLD));
        if(tap) g->buf_dir = (int8_t)tap;
        if(g->wait > 0 && --g->wait > 0) return ev;
        if(g->clear_n) {
            collapse(g);
            ev |= TE_COLLAPSE;
        }
        if(g->mode == TM_SPRINT && g->lines >= SPRINT_LINES) {
            g->over = g->won = true;
            return ev | TE_GOAL;
        }
        ev |= next_piece(g);
        if(g->over) return ev;
        pressed |= g->buf;
        if(!tap) tap = g->buf_dir;
        g->buf = 0;
        g->buf_dir = 0;
        // a charged auto-repeat carries straight into the new piece
        if(dir && g->das_t >= g->das && g->arr == 0) {
            while(shift(g, dir)) ev |= TE_MOVE;
        }
    }

    if((pressed & TK_HOLD) && !g->hold_used) {
        int t = g->hold;
        g->hold = (uint8_t)g->type;
        ev |= TE_HOLD | spawn(g, t ? t : queue_pop(g));
        g->hold_used = true;
        if(g->over) return ev;
    }
    if(pressed & TK_CW) {
        if(rotate(g, 1)) ev |= TE_ROTATE;
    }
    if(pressed & TK_CCW) {
        if(rotate(g, -1)) ev |= TE_ROTATE;
    }
    if(tap) {
        if(shift(g, tap)) ev |= TE_MOVE;
    } else if(dir && g->das_t >= g->das) {
        if(g->arr == 0) {
            while(shift(g, dir)) ev |= TE_MOVE;
        } else if((g->das_t - g->das) % g->arr == 0) {
            if(shift(g, dir)) ev |= TE_MOVE;
        }
    }

    if(pressed & TK_HARD) {
        int y0 = g->y;
        int y1 = tet_ghost_y(g);
        g->hd_y = (int8_t)y0;
        if(y1 > y0) g->last_rot = false;
        g->y = (int8_t)y1;
        g->score += 2u * (uint32_t)(y1 - y0);
        return ev | TE_HARDDROP | lock_piece(g);
    }

    int32_t grav = tet_gravity(g->level);
    bool soft = (keys & TK_SOFT) != 0;
    if(soft) {
        grav = grav * 20 > SOFT_MIN ? grav * 20 : SOFT_MIN;
        if(grav > 20 * ONE_ROW) grav = 20 * ONE_ROW;
    }
    g->fall += grav;
    while(g->fall >= ONE_ROW) {
        g->fall -= ONE_ROW;
        if(!tet_fits(g, g->type, g->rot, g->x, g->y + 1)) {
            g->fall = 0;
            break;
        }
        g->y++;
        g->last_rot = false;
        if(soft) {
            g->score++;
            ev |= TE_SOFTDROP;
        }
        if(g->y > g->lowest) {
            g->lowest = g->y;
            g->lock_moves = 0;
            g->lock_t = 0;
        }
    }

    if(on_ground(g)) {
        if(g->lock_t == 0) ev |= TE_LAND;
        g->lock_t++;
        if(g->lock_t >= LOCK_DELAY || g->lock_moves >= LOCK_MOVES) {
            g->hd_y = g->y;
            ev |= lock_piece(g);
        }
    } else {
        g->lock_t = 0;
    }
    return ev;
}

// ------------------------------------------------------------------ suspend / resume

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static uint32_t get16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
    return get16(p) | (get16(p + 2) << 16);
}

#define PACK_VERSION    1

int tet_pack(const tgame_t *g, uint8_t *buf, int max)
{
    // only a game with a piece in play (see tet_settle)
    if(max < TET_PACK_LEN || g->over || !g->active) return 0;
    uint8_t *p = buf;
    *p++ = PACK_VERSION;
    *p++ = g->mode;
    *p++ = (uint8_t)g->type;
    *p++ = (uint8_t)g->rot;
    *p++ = (uint8_t)g->x;
    *p++ = (uint8_t)g->y;
    *p++ = (uint8_t)g->lowest;
    *p++ = g->lock_moves;
    memcpy(p, g->bag, 7);
    p += 7;
    *p++ = g->bag_n;
    memcpy(p, g->next, NEXT_N);
    p += NEXT_N;
    *p++ = g->hold;
    *p++ = g->hold_used;
    put32(p, g->rng.s);
    put32(p + 4, g->score);
    put16(p + 8, g->lines);
    p += 10;
    *p++ = g->level;
    *p++ = g->start_level;
    put16(p, (uint16_t)g->combo);
    p += 2;
    *p++ = g->b2b;
    put32(p, g->frames);
    put32(p + 4, g->pieces);
    put16(p + 8, g->n_tetris);
    put16(p + 10, g->n_tspin);
    put16(p + 12, g->best_combo);
    p += 14;
    for(int y = 0; y < TB_H; y++) {
        for(int x = 0; x < TB_W; x += 2) *p++ = (uint8_t)(g->cell[y][x] | (g->cell[y][x + 1] << 4));
    }
    return (int)(p - buf);
}

bool tet_unpack(tgame_t *g, const uint8_t *buf, int len)
{
    if(len < TET_PACK_LEN || buf[0] != PACK_VERSION) return false;
    tgame_t t;
    memset(&t, 0, sizeof(t));
    const uint8_t *p = buf + 1;
    t.mode = *p++;
    t.type = (int8_t)*p++;
    t.rot = (int8_t)*p++;
    t.x = (int8_t)*p++;
    t.y = (int8_t)*p++;
    t.lowest = (int8_t)*p++;
    t.lock_moves = *p++;
    memcpy(t.bag, p, 7);
    p += 7;
    t.bag_n = *p++;
    memcpy(t.next, p, NEXT_N);
    p += NEXT_N;
    t.hold = *p++;
    t.hold_used = *p++ != 0;
    t.rng.s = get32(p);
    t.score = get32(p + 4);
    t.lines = (uint16_t)get16(p + 8);
    p += 10;
    t.level = *p++;
    t.start_level = *p++;
    t.combo = (int16_t)get16(p);
    p += 2;
    t.b2b = *p++ != 0;
    t.frames = get32(p);
    t.pieces = get32(p + 4);
    t.n_tetris = (uint16_t)get16(p + 8);
    t.n_tspin = (uint16_t)get16(p + 10);
    t.best_combo = (uint16_t)get16(p + 12);
    p += 14;
    for(int y = 0; y < TB_H; y++) {
        for(int x = 0; x < TB_W; x += 2, p++) {
            t.cell[y][x] = *p & 15;
            t.cell[y][x + 1] = *p >> 4;
            if(t.cell[y][x] >= PC_KINDS || t.cell[y][x + 1] >= PC_KINDS) return false;
        }
    }
    // anything out of range means nothing to continue, never a strange game
    if(t.mode >= TM_COUNT || t.type < PC_I || t.type > PC_L || t.rot < 0 || t.rot > 3 ||
       t.bag_n > 7 || t.hold > PC_L || t.level < 1 || t.level > MAX_LEVEL || t.start_level < 1 ||
       t.start_level > 15 || t.combo < -1 || t.score > 99999999u || t.lowest < t.y ||
       (t.mode == TM_ULTRA && t.frames >= ULTRA_FRAMES) || (t.mode == TM_SPRINT && t.lines >= SPRINT_LINES)) {
        return false;
    }
    for(int i = 0; i < 7; i++) {
        if(i < t.bag_n && (t.bag[i] < PC_I || t.bag[i] > PC_L)) return false;
    }
    for(int i = 0; i < NEXT_N; i++) {
        if(t.next[i] < PC_I || t.next[i] > PC_L) return false;
    }
    if(!tet_fits(&t, t.type, t.rot, t.x, t.y)) return false;
    t.active = true;
    t.keys_prev = TK_ALL;
    t.das = 5;
    t.arr = 1;
    t.lk_type = PC_NONE;
    *g = t;
    return true;
}

// ------------------------------------------------------------------ demo player

typedef struct {
    int8_t h[TB_W];
} colh_t;

// Pierre Dellacherie's weights: it keeps going for a very long time
static int32_t evaluate(const uint8_t (*b)[TB_W], int land_y, int lines, int eroded, int open_col)
{
    int32_t rt = 0, ct = 0, holes = 0, wells = 0;
    for(int y = 0; y < TB_H; y++) {
        int prev = 1;
        for(int x = 0; x < TB_W; x++) {
            int f = b[y][x] != 0;
            rt += f != prev;
            prev = f;
        }
        rt += prev == 0;
    }
    for(int x = 0; x < TB_W; x++) {
        int prev = 0;
        for(int y = 0; y < TB_H; y++) {
            int f = b[y][x] != 0;
            ct += f != prev;
            if(!f && prev) holes++;
            prev = f;
        }
        ct += prev == 0;
        // wells: empty cells with both sides filled, deeper ones count more
        if(x == open_col) continue;
        int depth = 0;
        for(int y = 0; y < TB_H; y++) {
            bool l = x == 0 || b[y][x - 1];
            bool r = x == TB_W - 1 || b[y][x + 1];
            if(!b[y][x] && l && r) {
                depth++;
                wells += depth;
            } else if(b[y][x]) {
                break;
            } else {
                depth = 0;
            }
        }
    }
    int height = TB_H - land_y;
    return -450 * height + 340 * lines * eroded - 320 * rt - 930 * ct - 790 * holes - 340 * wells;
}

static bool plan_piece(const tgame_t *g, int type, int *best_rot, int *best_x, int32_t *best_v)
{
    static uint8_t b[TB_H][TB_W];
    bool found = false;
    int stack = tet_stack_height(g);
    int nrot = type == PC_O ? 1 : ((type == PC_I || type == PC_S || type == PC_Z) ? 2 : 4);
    int sy = type == PC_I ? TB_HIDDEN - 3 : TB_HIDDEN - 2;
    for(int r = 0; r < nrot; r++) {
        for(int x = -2; x < TB_W; x++) {
            if(!tet_fits(g, type, r, x, sy)) continue;
            int y = sy;
            while(tet_fits(g, type, r, x, y + 1)) y++;
            memcpy(b, g->cell, sizeof(b));
            const int8_t (*c)[2] = TET_SHAPE[type][r];
            int top = TB_H;
            for(int i = 0; i < 4; i++) {
                int cy = y + c[i][1];
                if(cy >= 0) b[cy][x + c[i][0]] = (uint8_t)type;
                if(cy < top) top = cy;
            }
            // clear lines, counting the piece's own cells that went with them
            int lines = 0, eroded = 0;
            for(int yy = 0; yy < TB_H; yy++) {
                int k = 0;
                for(int xx = 0; xx < TB_W; xx++) k += b[yy][xx] != 0;
                if(k < TB_W) continue;
                lines++;
                for(int i = 0; i < 4; i++) eroded += y + c[i][1] == yy;
                memmove(b[1], b[0], (size_t)yy * TB_W);
                memset(b[0], 0, TB_W);
            }
            int32_t v = evaluate((const uint8_t (*)[TB_W])b, top, lines, eroded, stack < 12 ? TB_W - 1 : -1);
            if(top < TB_HIDDEN) v -= 100000;
            if(stack < 12) {
                // while it is safe, build for tetrises: keep the right column open for an I
                if(lines == 4) v += 30000;
                else if(lines) v -= 2500 * lines;
                for(int i = 0; i < 4; i++) v -= x + c[i][0] == TB_W - 1 ? 4000 : 0;
            }
            if(!found || v > *best_v) {
                found = true;
                *best_v = v;
                *best_rot = r;
                *best_x = x;
            }
        }
    }
    return found;
}

uint32_t tet_ai_keys(tgame_t *g, tai_t *ai, int pace, bool hard)
{
    if(g->over || !g->active) {
        ai->planned = false;
        ai->last = 0;
        return 0;
    }
    uint32_t id = g->pieces * 2 + (g->hold_used ? 1 : 0);
    if(!ai->planned || ai->piece != id) {
        int r = 0, x = g->x, r2 = 0, x2 = 0;
        int32_t v = 0, v2 = 0;
        plan_piece(g, g->type, &r, &x, &v);
        ai->hold = false;
        if(!g->hold_used) {
            int alt = g->hold ? g->hold : g->next[0];
            if(plan_piece(g, alt, &r2, &x2, &v2) && v2 > v + 400) ai->hold = true;
        }
        ai->rot = (int8_t)r;
        ai->x = (int8_t)x;
        ai->planned = true;
        ai->piece = id;
        ai->tries = 0;
    }
    // one key at a time, released in between, at the given pace
    if(ai->last || (pace > 1 && (g->frames % (uint32_t)pace) != 0)) {
        ai->last = 0;
        return 0;
    }
    uint32_t k;
    if(ai->hold) {
        k = TK_HOLD;
    } else if(g->rot != ai->rot && ai->tries < 12) {
        k = ((g->rot + 3) & 3) == ai->rot ? TK_CCW : TK_CW;
    } else if(g->x != ai->x && ai->tries < 30) {
        k = g->x < ai->x ? TK_RIGHT : TK_LEFT;
    } else {
        k = hard ? TK_HARD : TK_SOFT;
    }
    ai->tries++;
    ai->last = (uint8_t)k;
    return k;
}
