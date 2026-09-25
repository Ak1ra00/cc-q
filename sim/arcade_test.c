// Q ARCADE - tests of the home screen and the way the games hang together,
// driven with real key presses: launching, going home, SETTINGS, switching off
// in the middle of a game and carrying on after, damaged saves, auto off.
// Built and run by `make test` in sim/.
#include <stdio.h>
#include <string.h>
#include "../game/arcade.h"

static px_t fb[FB_PIX];
static uint32_t s_ms;
static int s_fail;

uint32_t plat_millis(void) { return s_ms; }

#define CHECK(name, cond) do { \
    if(cond) printf("[ok ] %s\n", name); \
    else { printf("[FAIL] %s  (%s:%d)\n", name, __FILE__, __LINE__); s_fail++; } \
} while(0)

// T_* screens in tetris_ui.c
enum { T_MENU, T_OPTIONS, T_SCORES, T_HOWTO, T_READY, T_PLAY, T_PAUSE, T_CONFIRM, T_OVER, T_NAME };

static uint32_t frames(int n, uint64_t keys)
{
    uint32_t ev = 0;
    for(int i = 0; i < n; i++) {
        s_ms += 33;
        ev |= arcade_frame(keys);
    }
    return ev;
}

static uint32_t tap(int key)
{
    uint32_t ev = frames(2, KEYBIT(key));
    return ev | frames(2, 0);
}

static int tstate(void)
{
    uint32_t sc;
    int l, lv;
    bool over;
    return tetris_debug(&sc, &l, &lv, &over);
}

static uint32_t play_bot(int n)
{
    uint32_t ev = 0;
    for(int i = 0; i < n; i++) {
        s_ms += 33;
        ev |= arcade_frame(tetris_bot_keys(1));
    }
    return ev;
}

static void boot(uint32_t seed)
{
    memset(fb, 0, sizeof(fb));
    gfx_set_target(fb);
    arcade_init(seed);
}

static void test_home_and_back(void)
{
    boot(1);
    CHECK("boot: the home screen comes up", arcade_app() == APP_HOME);
    frames(30, 0);
    tap(K_RIGHT);
    uint32_t ev = tap(K_ENTER);
    ev |= frames(40, 0);
    CHECK("home: RIGHT, ENTER opens TETRIS", arcade_app() == APP_TETRIS && tstate() == T_MENU);
    CHECK("home: remembers TETRIS as the last game", g_arc.last_game == GAME_TETRIS && (ev & EV_SAVE_ARCADE));

    tap(K_CANCEL);
    frames(30, 0);
    CHECK("tetris: CANCEL on its menu goes home", arcade_app() == APP_HOME);

    tap(K_LEFT);
    frames(10, 0);
    tap(K_ENTER);
    frames(40, 0);
    CHECK("home: LEFT, ENTER opens QUASAR at its title", arcade_app() == APP_QUASAR && g_game.state == ST_TITLE);
    // up from START GAME wraps round to HOME, the last item of QUASAR's title menu
    tap(K_UP);
    ev = tap(K_ENTER);
    ev |= frames(20, 0);
    CHECK("quasar: HOME goes home", arcade_app() == APP_HOME);
    CHECK("quasar: its HOME event never reaches the host", !(ev & EV_HOME));

    tap(K_ENTER);
    frames(40, 0);
    ev = tap(K_CANCEL);
    frames(20, 0);
    CHECK("quasar: CANCEL on its title goes home too", arcade_app() == APP_HOME && !(ev & EV_HOME));
}

static void test_settings_and_system(void)
{
    boot(2);
    frames(30, 0);
    tap(K_DOWN);
    uint32_t ev = tap(K_ENTER);             // SETTINGS
    int b = g_save.brightness;
    ev = tap(K_LEFT);                       // brightness down
    CHECK("settings: brightness changes and asks to be applied and saved",
          g_save.brightness == b - 1 && (ev & EV_BRIGHT) && (ev & EV_SAVE));
    tap(K_DOWN);
    ev = tap(K_RIGHT);
    CHECK("settings: screen sync changes and is applied", (ev & EV_VSYNC) && g_save.vsync == 1);
    tap(K_CANCEL);
    tap(K_RIGHT);                           // to SYSTEM
    ev = tap(K_ENTER);
    CHECK("home: SYSTEM asks the host for the system screens", (ev & EV_SYSTEM) != 0);
}

static void test_suspend_resume(void)
{
    boot(3);
    arcade_debug_start(GAME_TETRIS, TM_MARATHON);
    frames(60, 0);
    CHECK("tetris: READY, then play", tstate() == T_PLAY);
    play_bot(2500);
    uint32_t sc0;
    int l0, lv0;
    bool o0;
    tetris_debug(&sc0, &l0, &lv0, &o0);
    CHECK("tetris: the demo player is scoring", sc0 > 1000 && l0 > 10 && !o0);

    // hold POWER: a tap pauses, the hold saves and switches off
    uint32_t ev = frames(45, KEYBIT(K_POWER));
    CHECK("power: holding it switches off and saves both files",
          (ev & EV_POWEROFF) && (ev & EV_SAVE) && (ev & EV_SAVE_ARCADE));
    CHECK("power: the game was kept to continue", g_arc.suspended);

    uint8_t blob[640];
    int n = arcade_save_pack(blob, sizeof(blob));
    CHECK("save: fits the firmware's buffer", n > 0 && n <= (int)sizeof(blob));
    uint8_t qblob[256];
    int qn = save_pack(qblob, sizeof(qblob));

    // switch on again
    boot(99);
    CHECK("load: QUASAR's save still loads", save_unpack(qblob, qn));
    CHECK("load: arcade.sav loads", arcade_save_unpack(blob, n));
    arcade_loaded();
    CHECK("load: the paused game is there", g_arc.suspended && g_arc.last_game == GAME_TETRIS);
    frames(30, 0);
    tap(K_ENTER);
    frames(40, 0);
    CHECK("home: opens on TETRIS, the last game played", arcade_app() == APP_TETRIS);
    tap(K_ENTER);                           // CONTINUE is selected first
    frames(60, 0);
    uint32_t sc1;
    int l1, lv1;
    bool o1;
    int st = tetris_debug(&sc1, &l1, &lv1, &o1);
    CHECK("continue: the same game, same score and lines", st == T_PLAY && sc1 == sc0 && l1 == l0 && lv1 == lv0);

    // finish it: the saved copy goes away
    for(int i = 0; i < 40 && tstate() != T_OVER; i++) {
        tap(K_TAB);
        tap(K_DOWN);
        tap(K_DOWN);
        tap(K_DOWN);
        tap(K_ENTER);                       // END GAME
        tap(K_UP);
        tap(K_ENTER);                       // YES
    }
    CHECK("end game: results screen", tstate() == T_OVER);
    CHECK("end game: nothing left to continue", !g_arc.suspended);
}

static void test_damaged_saves(void)
{
    boot(4);
    g_arc.suspended = false;
    arcade_debug_start(GAME_TETRIS, TM_SPRINT);
    frames(60, 0);
    play_bot(600);
    frames(45, KEYBIT(K_POWER));
    uint8_t blob[640];
    int n = arcade_save_pack(blob, sizeof(blob));
    uint8_t bad[640];

    memcpy(bad, blob, (size_t)n);
    bad[20] ^= 0x40;
    boot(5);
    CHECK("damaged: a bad record block is refused, defaults stay", !arcade_save_unpack(bad, n) && !g_arc.suspended);

    memcpy(bad, blob, (size_t)n);
    bad[n - 30] ^= 0x01;
    boot(6);
    CHECK("damaged: a bad paused game only loses CONTINUE", arcade_save_unpack(bad, n) && !g_arc.suspended);

    boot(7);
    CHECK("short: an old or cut-off file is refused", !arcade_save_unpack(blob, 40));
    CHECK("short: without the paused game it still loads", arcade_save_unpack(blob, n - 60) && !g_arc.suspended);
}

static void test_idle_keeps_record(void)
{
    // left on the results with a record: auto off puts it on the table under the last name
    boot(12);
    arcade_debug_start(GAME_TETRIS, TM_SPRINT);
    frames(60, 0);
    for(int i = 0; i < 20000 && tstate() == T_PLAY; i++) play_bot(1);
    frames(30 * 60 * 10 + 30, 0);
    CHECK("idle: a record left on the results screen is kept before switching off",
          !strcmp(g_arc.rec[TM_SPRINT][0].name, "PLAYER") && arcade_wants_idle_off());
}

static void test_idle(void)
{
    boot(8);
    g_save.auto_off = 0;                    // 10 minutes
    frames(30 * 60 * 10 + 30, 0);
    CHECK("idle: home switches off after 10 minutes", arcade_wants_idle_off());

    boot(9);
    g_save.auto_off = 0;
    arcade_debug_start(GAME_TETRIS, TM_MARATHON);
    frames(60, 0);
    // keys held (not pressed) during play: no new presses, but play is never idle
    frames(30 * 60 * 10 + 30, 0);
    CHECK("idle: never in the middle of a game", tstate() != T_PLAY || !arcade_wants_idle_off());

    boot(10);
    g_save.auto_off = 2;
    frames(30 * 60 * 31, 0);
    CHECK("idle: NEVER means never", !arcade_wants_idle_off());
}

static void test_sprint_record(void)
{
    boot(11);
    arcade_debug_start(GAME_TETRIS, TM_SPRINT);
    frames(60, 0);
    // (well under the 10 minutes after which auto off would take the record itself)
    for(int i = 0; i < 20000 && tstate() == T_PLAY; i++) play_bot(1);
    frames(80, 0);
    CHECK("sprint: 40 lines ends on the results", tstate() == T_OVER);
    tap(K_ENTER);
    CHECK("sprint: a good time asks for a name", tstate() == T_NAME);
    frames(20, 0);
    tap(K_A);
    tap(K_B);
    tap(K_ENTER);
    CHECK("sprint: the name goes on the table", tstate() == T_SCORES && !strcmp(g_arc.name, "AB"));
    bool found = false;
    for(int i = 0; i < TREC_N; i++) found |= !strcmp(g_arc.rec[TM_SPRINT][i].name, "AB");
    CHECK("sprint: the record is in the sprint table", found);
}

int main(void)
{
    test_home_and_back();
    test_settings_and_system();
    test_suspend_resume();
    test_damaged_saves();
    test_idle();
    test_idle_keeps_record();
    test_sprint_record();
    printf(s_fail ? "\n%d test(s) FAILED\n" : "\nall arcade tests passed\n", s_fail);
    return s_fail ? 1 : 0;
}
