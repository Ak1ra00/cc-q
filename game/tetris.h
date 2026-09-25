// TETRIS - the rules: playfield, pieces, rotation, scoring, and a demo player.
//
// Modern guideline play: SRS rotation with wall kicks, a 7-piece bag, hold,
// five-piece preview, ghost piece, lock delay with move reset, T-spins (with
// minis), back-to-back, combos and perfect clears. Everything lives in one
// tgame_t, so the home screen can run its own copy as a live preview.
//
#pragma once
#include "game.h"

#define TB_W        10
#define TB_H        24          // 20 visible rows under 4 hidden ones
#define TB_HIDDEN   4
#define TB_VIS      20
#define NEXT_N      5

enum { PC_NONE = 0, PC_I, PC_O, PC_T, PC_S, PC_Z, PC_J, PC_L, PC_GREY, PC_KINDS };

// buttons, sampled once a frame (held state)
enum {
    TK_LEFT = 0x01, TK_RIGHT = 0x02, TK_SOFT = 0x04, TK_HARD = 0x08,
    TK_CW = 0x10, TK_CCW = 0x20, TK_HOLD = 0x40,
    TK_ALL = 0x7f,
};

// what happened during a step, for the screens to animate
enum {
    TE_MOVE = 0x0001, TE_ROTATE = 0x0002, TE_LOCK = 0x0004, TE_CLEAR = 0x0008,
    TE_HARDDROP = 0x0010, TE_HOLD = 0x0020, TE_SPAWN = 0x0040, TE_TOPOUT = 0x0080,
    TE_LEVELUP = 0x0100, TE_GOAL = 0x0200, TE_COLLAPSE = 0x0400, TE_LAND = 0x0800,
    TE_SOFTDROP = 0x1000,
};

enum { TM_MARATHON = 0, TM_SPRINT, TM_ULTRA, TM_COUNT };
enum { SPIN_NONE = 0, SPIN_MINI, SPIN_FULL };

#define SPRINT_LINES    40
#define ULTRA_FRAMES    (120 * FPS)
#define CLEAR_FRAMES    12
#define LOCK_DELAY      15          // half a second
#define LOCK_MOVES      15          // move resets before it locks anyway
#define MAX_LEVEL       99

typedef struct {
    uint8_t cell[TB_H][TB_W];
    rng_t rng;
    uint8_t mode;
    // the falling piece: its box position, rows counting down from the top
    int8_t type, rot, x, y;
    bool active;
    int32_t fall;               // gravity collected, 16.16 rows
    int16_t lock_t;
    uint8_t lock_moves;
    int8_t lowest;
    bool last_rot;              // the last thing that moved it was a rotation
    int8_t last_kick;
    // what comes next
    uint8_t bag[7];
    uint8_t bag_n;
    uint8_t next[NEXT_N];
    uint8_t hold;
    bool hold_used;
    // input
    uint8_t keys_prev;
    uint8_t buf;                // turn / hold pressed between pieces
    int8_t buf_dir;             // and a sideways tap
    int8_t das_dir;
    int16_t das_t;
    uint8_t das, arr;           // auto-repeat delay and rate, frames
    // between pieces
    int16_t wait;
    uint8_t clear_row[4];
    uint8_t clear_n;
    // progress
    uint32_t score;
    uint16_t lines;
    uint8_t level, start_level;
    int16_t combo;              // -1: no combo running
    bool b2b;                   // the last line clear was a difficult one
    uint32_t frames;
    uint32_t pieces;
    uint16_t n_tetris, n_tspin, best_combo;
    bool over, won;
    // the last lock, for the screens
    int8_t lk_type, lk_rot, lk_x, lk_y, hd_y;
    uint8_t lk_lines, lk_spin;
    bool lk_b2b, lk_pc;
    int16_t lk_combo;
    uint32_t lk_points;
} tgame_t;

// piece cells inside their box: [type][rot][cell] = { x, y }
extern const int8_t TET_SHAPE[PC_KINDS][4][4][2];

void tet_new(tgame_t *g, int mode, int level, uint32_t seed);
uint32_t tet_step(tgame_t *g, uint32_t keys);
bool tet_fits(const tgame_t *g, int type, int rot, int x, int y);
int tet_ghost_y(const tgame_t *g);
int32_t tet_gravity(int level);
void tet_settle(tgame_t *g);        // finish a line clear and bring in the next piece (may end the game)
int tet_stack_height(const tgame_t *g);

// suspend / resume
#define TET_PACK_LEN    172
int tet_pack(const tgame_t *g, uint8_t *buf, int max);
bool tet_unpack(tgame_t *g, const uint8_t *buf, int len);

// demo player: picks a placement per piece and presses the keys to get there
typedef struct {
    uint32_t piece;             // g->pieces when planned, +1 if it held
    bool planned, hold;
    int8_t rot, x;
    uint8_t last;               // keys returned last frame
    int16_t tries;
} tai_t;
uint32_t tet_ai_keys(tgame_t *g, tai_t *ai, int pace, bool hard);
