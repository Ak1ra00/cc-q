// QUASAR - shared game declarations.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gfx.h"
#include "gmath.h"
#include "text.h"
#include "assets_gen.h"

#define QUASAR_VERSION  "1.2.0"
#define FPS             30

// ---------------------------------------------------------------- keys
// Key numbers match the Coldcard Q keyboard matrix: row * 10 + column.
enum {
    K_NFC = 0, K_TAB = 1, K_QR = 2, K_LEFT = 3, K_UP = 4, K_DOWN = 5, K_RIGHT = 6,
    K_CANCEL = 7, K_ENTER = 8,
    K_1 = 10, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9, K_0,
    K_Q = 20, K_W, K_E, K_R, K_T, K_Y, K_U, K_I, K_O, K_P,
    K_A = 30, K_S, K_D, K_F, K_G, K_H, K_J, K_K, K_L, K_APOS,
    K_Z = 40, K_X, K_C, K_V, K_B, K_N, K_M, K_COMMA, K_DOT, K_SLASH,
    K_LAMP = 50, K_SHIFT = 51, K_SPACE = 52, K_SYM = 53, K_DEL = 54,
    K_POWER = 63,
};

#define KEYBIT(k)   ((uint64_t)1 << (k))

// abstract buttons
enum {
    B_LEFT  = 0x0001,
    B_RIGHT = 0x0002,
    B_UP    = 0x0004,
    B_DOWN  = 0x0008,
    B_A     = 0x0010,       // fire/charge, confirm
    B_B     = 0x0020,       // bomb, back
    B_PAUSE = 0x0040,
    B_POWER = 0x0080,
};

typedef struct {
    uint64_t raw, raw_pressed;          // key matrix state
    uint32_t held, pressed, released;   // game buttons (with WASD during play)
    uint32_t menu;                      // pressed + auto-repeat, for menus
    int power_hold;                     // frames the power key has been held
} input_t;

extern input_t g_in;
void input_update(uint64_t raw);
char key_to_char(int k, bool shift);    // for name entry; 0 if none

// ---------------------------------------------------------------- events to host
enum {
    EV_SAVE     = 0x01,     // persistent data changed, please store it
    EV_SYSTEM   = 0x02,     // user picked SYSTEM from the title menu
    EV_POWEROFF = 0x04,     // user asked to power down
    EV_BRIGHT   = 0x08,     // brightness setting changed
    EV_VSYNC    = 0x10,     // display sync setting changed
};
extern uint32_t g_events;

// ---------------------------------------------------------------- save data
#define NUM_SCORES  8
#define NAME_LEN    10

typedef struct {
    char name[NAME_LEN + 1];
    uint8_t stage;          // stage reached (6 = cleared)
    uint8_t diff;
    uint32_t score;
} hiscore_t;

// a run as it stood at the start of a stage, so CONTINUE can pick it up there
typedef struct {
    uint8_t stage;          // 2..5, or 0 when no run is saved
    uint8_t diff;
    uint8_t lives, maxhp, main_lvl, sub, sub_lvl, options;
    uint8_t continues;
    uint32_t score, next_life, frames, gems, best_chain;
} savedrun_t;

typedef struct {
    uint8_t difficulty;     // 0 easy 1 normal 2 hard
    uint8_t shake;          // screen shake on/off
    uint8_t brightness;     // 0..4
    uint8_t show_fps;
    uint8_t vsync;          // 0 = strips left->right, 1 = right->left, 2 = off
    uint8_t max_stage;      // highest stage reached, for practice
    uint8_t clears;         // number of full clears
    uint8_t auto_off;       // idle power-off on menus: 0 = 10 min, 1 = 30 min, 2 = never
    uint32_t plays;
    hiscore_t scores[NUM_SCORES];
    savedrun_t run;         // what CONTINUE picks up
} savedata_t;

extern savedata_t g_save;
void save_defaults(void);
int save_pack(uint8_t *buf, int max);
bool save_unpack(const uint8_t *buf, int len);
int score_rank(uint32_t score);         // 0..NUM_SCORES-1 or -1
void score_insert(int rank, const char *name, uint32_t score, int stage, int diff);

// ---------------------------------------------------------------- effects
void fx_reset(void);
void fx_update(void);
void fx_draw(void);
void fx_draw_top(void);
void fx_spark(float x, float y, float vx, float vy, px_t col, int life);
void fx_ember(float x, float y, float vx, float vy, px_t col, int life, int size);
void fx_debris(float x, float y, float vx, float vy, px_t col, int life);
void fx_fire(float x, float y, float vx, float vy, int delay, int scale);
void fx_smoke(float x, float y, float vx, float vy);
void fx_ring(float x, float y, float r0, float speed, px_t col, int life, int thick);
void fx_flashball(float x, float y, int r, px_t col, int life);
void fx_text(float x, float y, const char *s, px_t col);
void fx_explode(float x, float y, int size);        // size 0 small .. 3 huge
void fx_hitspark(float x, float y, px_t col);
void fx_shake(int amount);
void fx_flash(px_t col, int frames);
extern int g_shake_x, g_shake_y;
extern int g_hitstop;

// ---------------------------------------------------------------- backgrounds
void bg_init(int stage);
void bg_update(void);
void bg_draw(void);
void bg_draw_front(void);
float bg_speed(void);
void bg_set_speed(float s);
// stage 3 terrain: returns top and bottom solid limits at screen x
bool bg_terrain(int x, int *top, int *bot);

// ---------------------------------------------------------------- player
typedef struct {
    float x, y;
    int hp, maxhp;
    int lives;
    int bombs;
    int main_lvl;           // 1..5
    int sub;                // 0 none 1 spread 2 laser 3 missiles
    int sub_lvl;            // 1..3
    int invuln;
    int dead_timer;         // counts while respawning
    int fire_cd, sub_cd;
    int charge;             // frames held
    int bank;               // -1, 0, 1 visual
    int bomb_timer;
    int options;            // 0..2 option drones
    bool alive;
    bool entering;          // flying in at stage start
    float opt_x[2], opt_y[2];
    int trail_i;
    float trail_x[24], trail_y[24];
} player_t;

extern player_t g_pl;
void player_reset(bool new_game);
void player_update(void);
void player_draw(void);
void player_hit(void);
void player_bomb(void);
bool player_can_be_hit(void);

// player shots
enum { PS_PULSE = 1, PS_SPREAD, PS_LASER, PS_MISSILE, PS_BEAM, PS_OPTION };
typedef struct {
    float x, y, vx, vy;
    float dmg;
    int16_t life;
    uint8_t kind, alive;
    int16_t target;         // missile target enemy index or -1
    uint8_t pierce;         // hits remaining for piercing shots
    uint8_t level;
} pshot_t;

#define MAX_PSHOTS  96
extern pshot_t g_pshots[MAX_PSHOTS];
void pshots_update(void);
void pshots_draw(void);

// ---------------------------------------------------------------- enemy bullets
enum {
    EB_S_PINK, EB_M_PINK, EB_L_PINK, EB_S_ORNG, EB_M_ORNG, EB_S_BLUE, EB_M_BLUE, EB_NEEDLE,
    EB_KINDS
};
typedef struct {
    float x, y, vx, vy;
    float ax, ay;           // acceleration
    float turn;             // curving, turns per frame
    int16_t life;
    int16_t delay;          // frames before it starts moving
    uint8_t kind, alive;
} eshot_t;

#define MAX_ESHOTS  320
extern eshot_t g_eshots[MAX_ESHOTS];
eshot_t *eshot_fire(float x, float y, float speed, float turns, int kind);
eshot_t *eshot_aimed(float x, float y, float speed, int kind, float spread_turns);
void eshot_ring(float x, float y, int n, float speed, float offset_turns, int kind);
void eshot_fan(float x, float y, int n, float speed, float center_turns, float spread_turns, int kind);
void eshots_update(void);
void eshots_draw(void);
void eshots_cancel(bool to_gems);
int eshots_count(void);
float aim_at_player(float x, float y);

// ---------------------------------------------------------------- enemies
enum {
    E_NONE = 0,
    E_DRONE, E_DARTER, E_FIGHTER, E_POD, E_MINE, E_ROCK_L, E_ROCK_M, E_ROCK_S,
    E_GUNSHIP, E_CARRIER, E_SPINNER, E_WEAVER, E_JELLY, E_TURRET, E_WISP, E_EYE,
    E_SEEKER, E_CAPSULE_CARRIER,
    E_KINDS
};

typedef struct {
    float x, y, vx, vy;
    float hp;
    float p[4];             // behaviour parameters
    int16_t t;              // age in frames
    int16_t state, st;      // state and time in state
    int16_t cd;             // weapon cooldown
    uint8_t type, alive;
    uint8_t flash;          // hit flash frames
    uint8_t drop;           // capsule to drop on death, 0 none
    int8_t group;           // formation id, -1 none
    uint8_t flags;
    float ang;              // facing / rotation, turns
} enemy_t;

#define EF_GROUND   0x01    // on terrain (stage 3)
#define EF_NOHIT    0x02    // not collidable yet
#define EF_FLIPY    0x04

#define MAX_ENEMIES 56
extern enemy_t g_en[MAX_ENEMIES];
enemy_t *enemy_spawn(int type, float x, float y);
void enemies_update(void);
void enemies_draw(void);
void enemies_clear(void);
int enemies_alive(void);
void enemy_damage(enemy_t *e, float dmg, float hx, float hy);
float enemy_radius(const enemy_t *e);
void enemy_kill(enemy_t *e, bool give_score);

// formations: a group gets a bonus capsule when fully destroyed
void group_start(int id, int count, int drop);

// ---------------------------------------------------------------- pickups
enum { PU_GEM = 1, PU_POWER, PU_SPREAD, PU_LASER, PU_MISSILE, PU_BOMB, PU_HEAL, PU_OPTION, PU_LIFE };
typedef struct {
    float x, y, vx, vy;
    int16_t life;
    uint8_t kind, alive;
    uint8_t magnet;
} pickup_t;
#define MAX_PICKUPS 64
extern pickup_t g_pu[MAX_PICKUPS];
void pickup_spawn(int kind, float x, float y);
void pickups_update(void);
void pickups_draw(void);
void pickups_clear(void);
const char *pickup_name(int kind);

// ---------------------------------------------------------------- boss
typedef struct {
    bool active;
    int kind;               // 1..5
    float x, y, vx, vy;
    float hp, maxhp;
    int t, state, st;
    int phase;
    int flash;
    bool dying;
    int death_t;
    bool entered;
    float p[8];
    // parts
    int nparts;
    struct {
        float x, y, r;
        float hp, maxhp;
        bool alive, vuln;
        int flash;
    } part[12];
    char name[24];
} boss_t;

extern boss_t g_boss;
void boss_start(int kind);
void boss_update(void);
void boss_draw(void);
bool boss_hit_test(float x, float y, float r, float dmg, float hx, float hy);
bool boss_touch_player(float x, float y, float r);
void boss_clear(void);

// ---------------------------------------------------------------- stage
typedef struct {
    int num;                // 1..5
    int t;                  // stage frame counter
    int wave_i;
    bool boss_phase;
    int warning_t;
    int clear_t;
    int kills, spawned;
    int damage_taken;
    uint32_t score_at_start;
} stage_t;

extern stage_t g_stage;
extern const char *const STAGE_NAMES[6];
void stage_start(int num);
void stage_update(void);
void stage_draw_overlay(void);

// ---------------------------------------------------------------- scoring & run
typedef struct {
    uint32_t score;
    uint32_t hiscore;
    int chain, chain_timer, best_chain;
    int mult;
    int continues;
    uint32_t next_life;
    bool practice;
    int start_stage;
    int diff;               // difficulty this run is played at
    uint32_t frames;        // play time
    int gems;
} run_t;
extern run_t g_run;
void add_score(uint32_t pts, float x, float y, bool show);
void chain_kill(void);
void chain_break(void);
int difficulty(void);       // 0..2

// ---------------------------------------------------------------- hud & screens
void hud_draw(void);
void hud_boss_bar(void);

enum {
    ST_TITLE = 0, ST_OPTIONS, ST_SCORES, ST_HOWTO, ST_PRACTICE,
    ST_INTRO, ST_PLAY, ST_PAUSE, ST_STAGECLEAR, ST_GAMEOVER, ST_NAME,
    ST_ENDING, ST_CREDITS, ST_CONFIRM_QUIT, ST_CONFIRM_NEW,
};

typedef struct {
    int state, prev;
    int t;                  // frames in current state
    int sel;                // menu selection
    int frame;              // global frame counter
    int battery;            // -1 usb, 0..3
    int fps10;              // measured fps x10
    int fade;               // 0..32 fade to black
    bool demo;
    int name_len;
    char name[NAME_LEN + 1];
    int name_rank;
    int confirm_sel;
    uint32_t last_ms;
} game_t;
extern game_t g_game;
void game_set_state(int st);
void game_start_run(int stage, bool practice);
void game_debug_start(int stage, int diff);

void screens_update(void);
void screens_draw(void);
void screens_before_off(void);      // commit anything pending before power-off
void screens_save_loaded(void);     // the host loaded a save: point the title at CONTINUE
void run_checkpoint(void);          // save the run as it stands at this stage's start
void run_checkpoint_clear(void);    // the run is over: nothing to continue
void title_bg_draw(void);

// ---------------------------------------------------------------- platform
// Provided by the host.
uint32_t plat_millis(void);

// ---------------------------------------------------------------- top level API
void game_init(uint32_t seed);
uint32_t game_frame(uint64_t keys);         // update + render into g_fb; returns EV_*
void game_set_battery(int level);
void game_set_fps(int fps10);
bool game_wants_idle_off(void);
int game_brightness(void);
int game_vsync(void);

// colours used everywhere
#define C_WHITE     COL(255, 255, 255)
#define C_BLACK     COL(0, 0, 0)
#define C_CYAN      COL(90, 220, 255)
#define C_GOLD      COL(255, 214, 90)
#define C_PINK      COL(255, 90, 200)
#define C_RED       COL(255, 60, 80)
#define C_GREEN     COL(90, 255, 140)
#define C_GREY      COL(140, 140, 170)
#define C_DIM       COL(80, 80, 110)
#define C_ORANGE    COL(255, 150, 50)
#define C_VIOLET    COL(190, 120, 255)
