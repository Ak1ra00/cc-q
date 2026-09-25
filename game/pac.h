// PAC-MAN - the rules: three mazes, Pac-Man, four ghosts with their arcade minds,
// and what NEON mode adds on top.
//
// CLASSIC plays by the arcade's rules: the ghosts' targets (Blinky chases, Pinky
// cuts ahead, Inky flanks off Blinky, Clyde loses his nerve up close), scatter and
// chase waves, the speed tables, energizer times, Elroy, the ghost house's dot
// counters and the fruit. NEON adds a dash that charges as you eat, and two
// power-ups: MAGNET pulls in the dots around you, FREEZE turns the ghosts to ice
// you can shatter. Everything lives in one pgame_t, so the home screen and the
// menu can run their own copies as live demos.
//
#pragma once
#include "game.h"
#include "pac_gen.h"

#define PT              256         // position units in a tile (8 pixels, so 32 a pixel)
#define PAC_LIVES       3
#define PAC_MAX_LIVES   9
#define PAC_MAX_LEVEL   255
#define BOOST_FULL      40          // dots it takes to charge a dash
#define DASH_FRAMES     45
#define ITEM_FRAMES     (10 * FPS)
#define MAGNET_FRAMES   (8 * FPS)
#define FREEZE_FRAMES   (4 * FPS)
#define FRUIT_FRAMES    (285)       // nine and a half seconds
#define EAT_FRAMES      18          // the pause while a ghost's points show
#define DIE_FREEZE      30          // caught: everything stops...
#define DIE_ANIM        48          // ...then Pac-Man folds away
#define CLEAR_FREEZE    30
#define CLEAR_FLASH     64

// directions, in the arcade's order of preference when two ways tie
enum { PD_UP = 0, PD_LEFT, PD_DOWN, PD_RIGHT, PD_NONE };
enum { PM_CLASSIC = 0, PM_NEON, PM_MODES };
enum { GH_BLINKY = 0, GH_PINKY, GH_INKY, GH_CLYDE, GH_N };
enum { GS_HOUSE = 0, GS_LEAVING, GS_ACTIVE, GS_EYES, GS_ENTERING };
enum { PP_READY = 0, PP_PLAY, PP_EAT, PP_DYING, PP_CLEAR, PP_OVER };
enum { IT_NONE = 0, IT_MAGNET, IT_FREEZE };
enum { FR_CHERRY = 0, FR_STRAWBERRY, FR_ORANGE, FR_APPLE, FR_MELON, FR_GALAXIAN, FR_BELL, FR_KEY, FR_COUNT };

// buttons, sampled once a frame (held state)
enum { PK_UP = 0x01, PK_LEFT = 0x02, PK_DOWN = 0x04, PK_RIGHT = 0x08, PK_DASH = 0x10, PK_ALL = 0x1f };

// what happened during a step, for the screens to animate
enum {
    PE_DOT = 0x00001, PE_POWER = 0x00002, PE_GHOST = 0x00004, PE_FRUIT = 0x00008,
    PE_FRUIT_ON = 0x00010, PE_CAUGHT = 0x00020, PE_DEATH = 0x00040, PE_CLEAR = 0x00080,
    PE_LEVEL = 0x00100, PE_EXTRA = 0x00200, PE_FRIGHT_END = 0x00400, PE_ITEM_ON = 0x00800,
    PE_ITEM = 0x01000, PE_DASH = 0x02000, PE_OVER = 0x04000, PE_GO = 0x08000,
    PE_ALL4 = 0x10000, PE_SHATTER = 0x20000, PE_WAVE = 0x40000, PE_LIFE_LOST = 0x80000,
    PE_CHARGED = 0x100000, PE_THAW = 0x200000,
};

typedef struct {
    int32_t x, y;               // position units; a tile's centre is at tile * PT + PT / 2
    uint8_t dir;                // PD_*
    uint8_t came;               // the way it came into the tile whose middle it last chose at (ghosts)
    uint8_t state;              // GS_* (ghosts)
    bool fright;                // blue (ghosts)
    uint8_t dots;               // dots counted in the house (ghosts)
    uint16_t acc;               // fraction of a unit carried to the next frame
} pactor_t;

typedef struct {
    uint8_t mode, level, lives, maze;
    uint8_t phase;
    int16_t phase_t, ready_len;
    uint8_t dot[PM_H][PM_W];    // 1 a dot, 2 an energizer
    uint16_t dots_total, dots_left;
    pactor_t pac;
    pactor_t gh[GH_N];
    uint8_t want;               // the way the player last asked for, kept until it can be taken
    uint8_t stall;              // half frames Pac-Man loses to eating
    bool moving;
    uint8_t keys_prev;
    // scatter and chase
    uint8_t wave;
    int32_t wave_t;
    bool chase;
    // energizers
    int16_t fright_t, fright_len;
    uint8_t fright_eaten;
    // the ghost house
    bool global_on;
    uint8_t global_dots;
    int16_t idle_t;
    // fruit
    uint8_t fruit_n;            // how many have come out this level
    int16_t fruit_t;            // frames left on the board
    // NEON
    uint8_t boost;
    int16_t dash_t;
    uint8_t item, items_n;
    int16_t item_t;
    int8_t item_x, item_y;
    uint8_t item_kind;          // what the item on the board is
    int16_t magnet_t, freeze_t;
    int8_t pulled[8][2];        // dots the magnet took this frame
    uint8_t pulled_n;
    // progress
    uint32_t score, next_extra;
    uint32_t frames;
    uint16_t ghosts_eaten;
    uint8_t fruits_eaten;
    bool over;
    rng_t rng;
    // the last thing that scored, for the screens
    uint32_t ev;
    uint8_t ev_ghost;
    uint16_t ev_pts;
    int32_t ev_x, ev_y;
} pgame_t;

extern const int8_t PAC_DX[5], PAC_DY[5];
extern const uint16_t PAC_FRUIT_PTS[FR_COUNT];

void pac_new(pgame_t *g, int mode, uint32_t seed);
uint32_t pac_step(pgame_t *g, uint32_t keys);
bool pac_open(const pgame_t *g, int tx, int ty);
int pac_maze_for_level(int level);
int pac_fruit_for_level(int level);
bool pac_fright_flash(const pgame_t *g);    // frightened ghosts show white this frame
void pac_fruit_pos(int32_t *x, int32_t *y);

// suspend / resume: the level as it stands, dots and all, picked up from a READY
#define PAC_PACK_LEN    160
int pac_pack(const pgame_t *g, uint8_t *buf, int max);
bool pac_unpack(pgame_t *g, const uint8_t *buf, int len);

// demo player: keeps ahead of the ghosts, eats what is safe, runs when it must
typedef struct {
    int16_t tile;               // where it last planned, and what
    uint8_t dir, age, last, dash;
} pai_t;
uint32_t pac_ai_keys(pgame_t *g, pai_t *ai);
