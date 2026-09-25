// Q ARCADE - the home screen, and the games it starts.
//
// The device boots into the home screen, which shows each game as a live card.
// QUASAR runs exactly as before (game.c and friends); TETRIS lives in
// tetris.c (the rules) and tetris_ui.c (its screens). arcade.c sits on top of
// them all: it owns the frame loop, switches between them, and keeps the
// settings, scores and a paused game that the home screen and TETRIS save in a
// file of their own (arcade.sav), so QUASAR's save stays exactly as it was.
//
#pragma once
#include "game.h"
#include "tetris.h"

enum { APP_HOME = 0, APP_QUASAR, APP_TETRIS };
enum { GAME_QUASAR = 0, GAME_TETRIS, NUM_GAMES };

#define EV_SAVE_ARCADE  0x20        // arcade.sav changed, please store it

// ---------------------------------------------------------------- saved data
#define TREC_N      5

typedef struct {
    char name[NAME_LEN + 1];
    uint32_t value;                 // score, or a sprint time in frames
    uint16_t lines;
    uint8_t level;
} trec_t;

typedef struct {
    uint8_t last_game;              // the card the home screen opens on
    // TETRIS options
    uint8_t ghost, grid, das, upkey, start_level;
    char name[NAME_LEN + 1];        // the last name typed, offered again
    uint32_t plays, lines;
    trec_t rec[TM_COUNT][TREC_N];   // best marathon and ultra scores, sprint times
    bool suspended;                 // a TETRIS game saved to carry on with
    uint8_t susp[TET_PACK_LEN];
} arcsave_t;

extern arcsave_t g_arc;
extern uint32_t g_arc_t;            // frames since boot, for animation

void arcsave_defaults(void);
int arcade_save_pack(uint8_t *buf, int max);
bool arcade_save_unpack(const uint8_t *buf, int len);
int trec_rank(int mode, uint32_t value);
void trec_insert(int mode, int rank, const char *name, uint32_t value, int lines, int level);

// ---------------------------------------------------------------- top level
void arcade_init(uint32_t seed);
uint32_t arcade_frame(uint64_t keys);       // one frame of whatever is running
bool arcade_wants_idle_off(void);
void arcade_loaded(void);                   // the host has loaded arcade.sav
void arcade_start(int game);                // from home: the game is on screen from the next frame
void arcade_go_home(int from_game);         // from a game: back to the home screen

// ---------------------------------------------------------------- home.c
void home_enter(int from_game);
void home_update(void);
void home_draw(void);
bool home_launching(void);

// ---------------------------------------------------------------- tetris_ui.c
void tetris_enter(void);
void tetris_update(void);
void tetris_draw(void);
bool tetris_in_play(void);          // a game is running (not paused, not on a menu)
void tetris_before_off(void);       // keep a game or a new record before switching off
void tetris_power_tap(void);

// ---------------------------------------------------------------- for the desktop build's tests
int arcade_app(void);
void arcade_debug_start(int game, int mode);        // straight in, no animation; mode < 0: TETRIS menu
uint64_t tetris_bot_keys(int pace);                 // the demo player's keys, as key matrix bits
void tetris_debug_start(int mode);
int tetris_debug(uint32_t *score, int *lines, int *level, bool *over);     // returns the screen

// ---------------------------------------------------------------- shared drawing
extern const px_t PIECE_COL[PC_KINDS];
extern const px_t GAME_COL[NUM_GAMES];      // each game's colour, for the change of scene

void ar_tiles_init(void);
void ar_cell(int x, int y, int s, int type);            // a bevelled block, s from 3 to 12 pixels
void ar_ghost_cell(int x, int y, int s, int type);
void ar_piece(int type, int cx, int cy, int s);         // a whole piece, centred
void ar_tetris_logo(int cx, int y, int s, int t);       // the block letters; t animates, <0 still
int ar_tetris_logo_w(int s);
void ar_battery(int x, int y);
void ar_panel(int x, int y, int w, int h, px_t edge);
void ar_neon_rect(int x, int y, int w, int h, px_t c, int glow);
void ar_menu_item(int cx, int y, const char *s, bool sel, bool enabled, px_t accent);
px_t ar_lighten(px_t c, int a32);
void ar_add_vgrad(int x, int y, int w, int h, px_t top, px_t bot);  // additive gradient
