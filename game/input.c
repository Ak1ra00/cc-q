// QUASAR - input mapping from the Q keyboard matrix to game buttons.
#include "game.h"

input_t g_in;

static uint64_t s_prev_raw;
static uint32_t s_prev_held;
static int s_repeat_t;

// keys that act as each button
static const uint8_t K_LEFTS[] = { K_LEFT };
static const uint8_t K_RIGHTS[] = { K_RIGHT };
static const uint8_t K_UPS[] = { K_UP };
static const uint8_t K_DOWNS[] = { K_DOWN };

static bool any(uint64_t raw, const uint8_t *ks, int n)
{
    for(int i = 0; i < n; i++) {
        if(raw & KEYBIT(ks[i])) return true;
    }
    return false;
}

void input_update(uint64_t raw)
{
    uint32_t b = 0;
    bool playing = (g_game.state == ST_PLAY);

    if(any(raw, K_LEFTS, 1)) b |= B_LEFT;
    if(any(raw, K_RIGHTS, 1)) b |= B_RIGHT;
    if(any(raw, K_UPS, 1)) b |= B_UP;
    if(any(raw, K_DOWNS, 1)) b |= B_DOWN;

    if(playing) {
        // WASD also moves while flying
        if(raw & KEYBIT(K_A)) b |= B_LEFT;
        if(raw & KEYBIT(K_D)) b |= B_RIGHT;
        if(raw & KEYBIT(K_W)) b |= B_UP;
        if(raw & KEYBIT(K_S)) b |= B_DOWN;
        if(raw & (KEYBIT(K_K) | KEYBIT(K_J))) b |= B_A;
        if(raw & KEYBIT(K_L)) b |= B_B;
        if(raw & (KEYBIT(K_P) | KEYBIT(K_TAB) | KEYBIT(K_QR) | KEYBIT(K_NFC))) b |= B_PAUSE;
    } else {
        if(raw & (KEYBIT(K_TAB) | KEYBIT(K_QR) | KEYBIT(K_NFC))) b |= B_PAUSE;
    }
    if(raw & (KEYBIT(K_ENTER) | KEYBIT(K_SPACE))) b |= B_A;
    if(raw & (KEYBIT(K_CANCEL) | KEYBIT(K_DEL))) b |= B_B;
    if(raw & KEYBIT(K_POWER)) b |= B_POWER;

    // opposite directions cancel
    if((b & (B_LEFT | B_RIGHT)) == (B_LEFT | B_RIGHT)) b &= ~(uint32_t)(B_LEFT | B_RIGHT);
    if((b & (B_UP | B_DOWN)) == (B_UP | B_DOWN)) b &= ~(uint32_t)(B_UP | B_DOWN);

    g_in.raw = raw;
    g_in.raw_pressed = raw & ~s_prev_raw;
    g_in.held = b;
    g_in.pressed = b & ~s_prev_held;
    g_in.released = s_prev_held & ~b;

    // menu auto-repeat for the direction buttons
    uint32_t dirs = b & (B_LEFT | B_RIGHT | B_UP | B_DOWN);
    g_in.menu = g_in.pressed;
    if(dirs && dirs == (s_prev_held & (B_LEFT | B_RIGHT | B_UP | B_DOWN))) {
        s_repeat_t++;
        if(s_repeat_t > 10 && (s_repeat_t % 3) == 0) g_in.menu |= dirs;
    } else {
        s_repeat_t = 0;
    }

    if(b & B_POWER) g_in.power_hold++;
    else g_in.power_hold = 0;

    s_prev_raw = raw;
    s_prev_held = b;
}

static const char CHARS[60] =
    "\0\0\0\0\0\0\0\0\0\0"
    "1234567890"
    "QWERTYUIOP"
    "ASDFGHJKL'"
    "ZXCVBNM,./"
    "\0\0 \0\0\0\0\0\0\0";

char key_to_char(int k, bool shift)
{
    (void)shift;
    if(k < 0 || k >= 60) return 0;
    char c = CHARS[k];
    if(c == '\'' || c == ',' || c == '/') return 0;
    return c;
}
