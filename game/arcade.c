// Q ARCADE - the frame loop over the home screen and the games, the change of
// scene between them, arcade.sav, and drawing shared by home and TETRIS.
#include "arcade.h"
#include <string.h>

arcsave_t g_arc;
uint32_t g_arc_t;

static int s_app;
static int s_pending = -1;          // APP_* to switch to after this frame
static int s_from_game;
static int s_idle;
static int s_reveal_t = -1;         // the block wipe that opens each scene
static px_t s_reveal_col;

// ------------------------------------------------------------------ arcade.sav
//
// layout: magic(4) version(1) len(2) options(6) name(10) plays(4) lines(4)
// records(3 x 5 x 17) crc(4), then a paused game as a block of its own:
// magic(4) len(1) game crc(4). A damaged paused game only loses CONTINUE.

#define ARC_MAGIC       0x31524151u     // "QAR1"
#define ARC_VERSION     1
#define REC_LEN         (NAME_LEN + 4 + 2 + 1)
#define BODY_LEN        (4 + 1 + 2 + 6 + NAME_LEN + 8 + TM_COUNT * TREC_N * REC_LEN)
#define SUSP_MAGIC      0x31535451u     // "QTS1"
#define SUSP_LEN        (4 + 1 + TET_PACK_LEN)
#define ARC_TOTAL       (BODY_LEN + 4 + SUSP_LEN + 4)

static const char *const REC_NAMES[TREC_N] = { "STACKER", "SPINNER", "DROPPER", "HOLDER", "ROOKIE" };
static const uint32_t REC_DEFAULT[TM_COUNT][TREC_N] = {
    { 60000, 40000, 25000, 12000, 5000 },
    { 120 * FPS, 150 * FPS, 180 * FPS, 240 * FPS, 300 * FPS },
    { 30000, 22000, 15000, 9000, 4000 },
};

void arcsave_defaults(void)
{
    memset(&g_arc, 0, sizeof(g_arc));
    g_arc.ghost = 1;
    g_arc.grid = 1;
    g_arc.das = 1;
    g_arc.start_level = 1;
    for(int m = 0; m < TM_COUNT; m++) {
        for(int i = 0; i < TREC_N; i++) {
            trec_t *r = &g_arc.rec[m][i];
            strcpy(r->name, REC_NAMES[i]);
            r->value = REC_DEFAULT[m][i];
            r->lines = (uint16_t)(m == TM_SPRINT ? SPRINT_LINES : 60 - i * 10);
            r->level = (uint8_t)(m == TM_MARATHON ? 7 - i : 1);
        }
    }
}

static uint32_t crc32(const uint8_t *p, int n)
{
    uint32_t c = 0xffffffffu;
    for(int i = 0; i < n; i++) {
        c ^= p[i];
        for(int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

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

static void put_name(uint8_t *p, const char *s)
{
    memset(p, 0, NAME_LEN);
    size_t n = strlen(s);
    memcpy(p, s, n > NAME_LEN ? NAME_LEN : n);
}

static void get_name(char *dst, const uint8_t *p)
{
    for(int k = 0; k < NAME_LEN; k++) {
        char c = (char)p[k];
        dst[k] = (c >= 32 && c < 127) ? c : 0;
    }
    dst[NAME_LEN] = 0;
}

int arcade_save_pack(uint8_t *buf, int max)
{
    if(max < ARC_TOTAL) return 0;
    uint8_t *p = buf;
    put32(p, ARC_MAGIC);
    p += 4;
    *p++ = ARC_VERSION;
    *p++ = (uint8_t)BODY_LEN;
    *p++ = (uint8_t)(BODY_LEN >> 8);
    *p++ = g_arc.last_game;
    *p++ = g_arc.ghost;
    *p++ = g_arc.grid;
    *p++ = g_arc.das;
    *p++ = g_arc.upkey;
    *p++ = g_arc.start_level;
    put_name(p, g_arc.name);
    p += NAME_LEN;
    put32(p, g_arc.plays);
    put32(p + 4, g_arc.lines);
    p += 8;
    for(int m = 0; m < TM_COUNT; m++) {
        for(int i = 0; i < TREC_N; i++) {
            const trec_t *r = &g_arc.rec[m][i];
            put_name(p, r->name);
            put32(p + NAME_LEN, r->value);
            p[NAME_LEN + 4] = (uint8_t)r->lines;
            p[NAME_LEN + 5] = (uint8_t)(r->lines >> 8);
            p[NAME_LEN + 6] = r->level;
            p += REC_LEN;
        }
    }
    put32(p, crc32(buf, BODY_LEN));
    p += 4;
    uint8_t *s = p;
    memset(s, 0, SUSP_LEN + 4);
    if(g_arc.suspended) {
        put32(s, SUSP_MAGIC);
        s[4] = TET_PACK_LEN;
        memcpy(s + 5, g_arc.susp, TET_PACK_LEN);
        put32(s + SUSP_LEN, crc32(s, SUSP_LEN));
    }
    return ARC_TOTAL;
}

bool arcade_save_unpack(const uint8_t *buf, int len)
{
    if(len < BODY_LEN + 4 || get32(buf) != ARC_MAGIC || buf[4] != ARC_VERSION) return false;
    if((buf[5] | (buf[6] << 8)) != BODY_LEN || get32(buf + BODY_LEN) != crc32(buf, BODY_LEN)) return false;
    arcsave_t a;
    memset(&a, 0, sizeof(a));
    const uint8_t *p = buf + 7;
    a.last_game = p[0] < NUM_GAMES ? p[0] : 0;
    a.ghost = p[1] ? 1 : 0;
    a.grid = p[2] ? 1 : 0;
    a.das = p[3] > 3 ? 1 : p[3];
    a.upkey = p[4] ? 1 : 0;
    a.start_level = (p[5] < 1 || p[5] > 15) ? 1 : p[5];
    p += 6;
    get_name(a.name, p);
    p += NAME_LEN;
    a.plays = get32(p);
    a.lines = get32(p + 4);
    p += 8;
    for(int m = 0; m < TM_COUNT; m++) {
        for(int i = 0; i < TREC_N; i++) {
            trec_t *r = &a.rec[m][i];
            get_name(r->name, p);
            r->value = get32(p + NAME_LEN);
            r->lines = (uint16_t)(p[NAME_LEN + 4] | (p[NAME_LEN + 5] << 8));
            r->level = p[NAME_LEN + 6] > MAX_LEVEL ? 1 : p[NAME_LEN + 6];
            p += REC_LEN;
        }
    }
    // the paused game is checked again in full when it is continued
    const uint8_t *s = buf + BODY_LEN + 4;
    if(len >= ARC_TOTAL && get32(s) == SUSP_MAGIC && s[4] == TET_PACK_LEN &&
       get32(s + SUSP_LEN) == crc32(s, SUSP_LEN)) {
        tgame_t probe;
        if(tet_unpack(&probe, s + 5, TET_PACK_LEN)) {
            memcpy(a.susp, s + 5, TET_PACK_LEN);
            a.suspended = true;
        }
    }
    g_arc = a;
    return true;
}

int trec_rank(int mode, uint32_t value)
{
    if(!value) return -1;
    for(int i = 0; i < TREC_N; i++) {
        uint32_t v = g_arc.rec[mode][i].value;
        if(mode == TM_SPRINT ? (v == 0 || value < v) : value > v) return i;
    }
    return -1;
}

void trec_insert(int mode, int rank, const char *name, uint32_t value, int lines, int level)
{
    if(rank < 0 || rank >= TREC_N) return;
    trec_t *t = g_arc.rec[mode];
    for(int i = TREC_N - 1; i > rank; i--) t[i] = t[i - 1];
    memset(&t[rank], 0, sizeof(t[rank]));
    strncpy(t[rank].name, name, NAME_LEN);
    t[rank].value = value;
    t[rank].lines = (uint16_t)lines;
    t[rank].level = (uint8_t)level;
}

// ------------------------------------------------------------------ scene changes

const px_t GAME_COL[NUM_GAMES] = { COL(190, 90, 255), COL(40, 190, 255) };

static void reveal_start(px_t col)
{
    s_reveal_t = 0;
    s_reveal_col = col;
}

static void reveal_draw(void)
{
    // the screen opens from the middle as a wave of shrinking blocks
    if(s_reveal_t < 0) return;
    int t = s_reveal_t++;
    bool any = false;
    for(int cy = 0; cy < SCR_H / 16; cy++) {
        for(int cx = 0; cx < SCR_W / 16; cx++) {
            int dx = cx * 2 - 19, dy = cy * 2 - 14;
            int d = isqrt((uint32_t)(dx * dx + dy * dy));          // 0..24 half cells
            int sz = 16 - (t - d * 10 / 24) * 3;
            if(sz <= 0) continue;
            if(sz > 16) sz = 16;
            any = true;
            px_t c = sz < 16 ? ar_lighten(s_reveal_col, (16 - sz) + 4) : s_reveal_col;
            int o = (16 - sz) / 2;
            gfx_fill(cx * 16 + o, cy * 16 + o, sz, sz, c);
        }
    }
    if(!any) s_reveal_t = -1;
}

void arcade_start(int game)
{
    s_pending = game == GAME_TETRIS ? APP_TETRIS : APP_QUASAR;
    if(g_arc.last_game != game) {
        g_arc.last_game = (uint8_t)game;
        g_events |= EV_SAVE_ARCADE;
    }
}

void arcade_go_home(int from_game)
{
    s_pending = APP_HOME;
    s_from_game = from_game;
}

static void switch_now(void)
{
    int to = s_pending;
    s_pending = -1;
    s_idle = 0;
    fx_reset();
    s_app = to;
    if(to == APP_HOME) {
        home_enter(s_from_game);
        reveal_start(GAME_COL[s_from_game]);
    } else if(to == APP_QUASAR) {
        game_set_state(ST_TITLE);
        reveal_start(GAME_COL[GAME_QUASAR]);
    } else {
        tetris_enter();
        reveal_start(GAME_COL[GAME_TETRIS]);
    }
}

int arcade_app(void)
{
    return s_app;
}

void arcade_debug_start(int game, int mode)
{
    g_arc.last_game = (uint8_t)game;
    s_pending = game == GAME_TETRIS ? APP_TETRIS : APP_QUASAR;
    switch_now();
    s_reveal_t = -1;
    if(game == GAME_TETRIS && mode >= 0) tetris_debug_start(mode);
}

// ------------------------------------------------------------------ frame loop

void arcade_init(uint32_t seed)
{
    game_init(seed);
    arcsave_defaults();
    ar_tiles_init();
    s_app = APP_HOME;
    s_pending = -1;
    s_reveal_t = -1;
    home_enter(-1);
}

void arcade_loaded(void)
{
    if(s_app == APP_HOME) home_enter(-1);
}

bool arcade_wants_idle_off(void)
{
    if(s_app == APP_QUASAR) return game_wants_idle_off();
    // the AUTO OFF time without a key press, never in the middle of a game
    static const int minutes[3] = { 10, 30, 0 };
    int m = minutes[g_save.auto_off > 2 ? 0 : g_save.auto_off];
    return m && s_idle > 30 * 60 * m && !(s_app == APP_TETRIS && tetris_in_play());
}

uint32_t arcade_frame(uint64_t keys)
{
    uint32_t ev;
    g_arc_t++;
    if(s_app == APP_QUASAR) {
        ev = game_frame(keys);
        if(ev & EV_HOME) {
            ev &= ~(uint32_t)EV_HOME;
            arcade_go_home(GAME_QUASAR);
        }
    } else {
        g_events = 0;
        input_update(keys);
        if(g_in.raw_pressed) s_idle = 0;
        else if(s_idle < 0x7fffffff) s_idle++;

        // power key: tap pauses a game, hold switches off
        if(g_in.power_hold == 1 && s_app == APP_TETRIS) tetris_power_tap();
        if(g_in.power_hold == 40) {
            if(s_app == APP_TETRIS) tetris_before_off();
            g_events |= EV_POWEROFF | EV_SAVE | EV_SAVE_ARCADE;
        }

        if(s_app == APP_HOME) {
            home_update();
            home_draw();
        } else {
            tetris_update();
            tetris_draw();
        }
        // the host switches off after this frame: keep a game or a new record
        if(arcade_wants_idle_off() && s_app == APP_TETRIS) {
            tetris_before_off();
            g_events |= EV_SAVE_ARCADE;
        }
        ev = g_events;
    }
    reveal_draw();
    if(s_pending >= 0) switch_now();
    return ev;
}

// ------------------------------------------------------------------ shared drawing

const px_t PIECE_COL[PC_KINDS] = {
    COL(0, 0, 0),
    COL(40, 215, 255),      // I
    COL(255, 210, 40),      // O
    COL(185, 80, 255),      // T
    COL(70, 225, 90),       // S
    COL(255, 60, 85),       // Z
    COL(50, 110, 255),      // J
    COL(255, 140, 30),      // L
    COL(120, 124, 150),     // grey
};

px_t ar_lighten(px_t c, int a32)
{
    return px_mix(C_WHITE, c, iclamp(a32, 0, 32));
}

// block tiles, every size from 3 to 12 pixels, drawn once at start
#define TILE_MIN    3
#define TILE_MAX    12
#define TILE_POOL   645         // sum of s*s for s = 3..12
static px_t s_tile[PC_KINDS][TILE_POOL];
static uint16_t s_tile_off[TILE_MAX + 1];

static void make_tile(px_t *d, int s, px_t c)
{
    px_t top = ar_lighten(c, 7), bot = px_scale(c, 21);
    px_t hi = ar_lighten(c, 15), lo = px_scale(c, 12);
    for(int y = 0; y < s; y++) {
        int k = s > 3 ? ((y - 1) * 32) / (s - 3) : 16;
        px_t body = px_mix(bot, top, iclamp(k, 0, 32));
        for(int x = 0; x < s; x++) {
            px_t p = body;
            if(x == s - 1 || y == s - 1) p = lo;
            else if(x == 0 || y == 0) p = hi;
            else if(s >= 7 && y <= 1 + s / 7 && x >= 1 && x <= s / 2) p = ar_lighten(c, 11);   // gloss
            d[y * s + x] = p;
        }
    }
    if(s >= 9) {
        // a little shine in the corner
        d[1 * s + 1] = ar_lighten(c, 22);
        d[2 * s + 1] = ar_lighten(c, 16);
    }
}

void ar_tiles_init(void)
{
    int off = 0;
    for(int s = TILE_MIN; s <= TILE_MAX; s++) {
        s_tile_off[s] = (uint16_t)off;
        for(int t = 1; t < PC_KINDS; t++) make_tile(&s_tile[t][off], s, PIECE_COL[t]);
        off += s * s;
    }
}

void ar_cell(int x, int y, int s, int type)
{
    if(type <= 0 || type >= PC_KINDS) return;
    if(s < TILE_MIN) {
        gfx_fill(x, y, s, s, PIECE_COL[type]);
        return;
    }
    if(s > TILE_MAX) s = TILE_MAX;
    const px_t *src = &s_tile[type][s_tile_off[s]];
    int x0 = x, y0 = y, x1 = x + s, y1 = y + s;
    if(x0 < g_clip.x0) x0 = g_clip.x0;
    if(y0 < g_clip.y0) y0 = g_clip.y0;
    if(x1 > g_clip.x1) x1 = g_clip.x1;
    if(y1 > g_clip.y1) y1 = g_clip.y1;
    for(int px = x0; px < x1;) {
        int se = ((px >> STRIP_SHIFT) + 1) << STRIP_SHIFT;
        if(se > x1) se = x1;
        px_t *d = &g_fb[FBI(px, y0)];
        const px_t *sp = src + (y0 - y) * s + (px - x);
        int n = se - px;
        for(int yy = y0; yy < y1; yy++, d += STRIP_W, sp += s) {
            for(int i = 0; i < n; i++) d[i] = sp[i];
        }
        px = se;
    }
}

void ar_ghost_cell(int x, int y, int s, int type)
{
    if(type <= 0 || type >= PC_KINDS) return;
    px_t c = PIECE_COL[type];
    gfx_fill_mode(x + 1, y + 1, s - 2, s - 2, px_scale(c, 5), DM_ADD);
    gfx_rect(x, y, s, s, px_scale(c, 17));
}

void ar_piece(int type, int cx, int cy, int s)
{
    if(type < PC_I || type > PC_L) return;
    const int8_t (*c)[2] = TET_SHAPE[type][0];
    int minx = 9, maxx = -9, miny = 9, maxy = -9;
    for(int i = 0; i < 4; i++) {
        if(c[i][0] < minx) minx = c[i][0];
        if(c[i][0] > maxx) maxx = c[i][0];
        if(c[i][1] < miny) miny = c[i][1];
        if(c[i][1] > maxy) maxy = c[i][1];
    }
    int w = (maxx - minx + 1) * s, h = (maxy - miny + 1) * s;
    int ox = cx - w / 2 - minx * s, oy = cy - h / 2 - miny * s;
    for(int i = 0; i < 4; i++) ar_cell(ox + c[i][0] * s, oy + c[i][1] * s, s, type);
}

// T E T R I S in blocks, three columns and five rows a letter
static const char *const LOGO_ROWS[6][5] = {
    { "###", ".#.", ".#.", ".#.", ".#." },
    { "###", "#..", "##.", "#..", "###" },
    { "###", ".#.", ".#.", ".#.", ".#." },
    { "##.", "#.#", "##.", "#.#", "#.#" },
    { "###", ".#.", ".#.", ".#.", "###" },
    { "###", "#..", "###", "..#", "###" },
};
static const uint8_t LOGO_TYPE[6] = { PC_Z, PC_L, PC_O, PC_S, PC_I, PC_T };

int ar_tetris_logo_w(int s)
{
    return 23 * s;
}

void ar_tetris_logo(int cx, int y, int s, int t)
{
    int x0 = cx - ar_tetris_logo_w(s) / 2;
    // a light sweeps across every few seconds
    int sweep = t < 0 ? -99 : (t % 150) - 30;
    for(int l = 0; l < 6; l++) {
        int oy = 0;
        if(t >= 0) {
            // drop in one by one, bounce, then bob gently
            int p = t - l * 4;
            if(p < 0) continue;
            if(p < 10) oy = -(10 - p) * (10 - p) * s / 12;
            else if(p < 18) oy = -((isin256((p - 10) * 16) * s) >> 15);
            else oy = (isin256(t * 3 + l * 40) * 3) >> 15;
        }
        for(int r = 0; r < 5; r++) {
            for(int c = 0; c < 3; c++) {
                if(LOGO_ROWS[l][r][c] != '#') continue;
                int bx = x0 + (l * 4 + c) * s, by = y + r * s + oy;
                ar_cell(bx, by, s, LOGO_TYPE[l]);
                int d = (l * 4 + c + r) - sweep / 2;
                if(d >= 0 && d < 3) gfx_fill_mode(bx, by, s, s, COL(90, 90, 90), DM_ADD);
            }
        }
    }
}

void ar_battery(int x, int y)
{
    int lv = g_game.battery;
    if(lv < 0) {
        text_draw(x - 8, y, "USB", C_DIM, 1);
        return;
    }
    gfx_rect(x, y, 14, 7, C_GREY);
    gfx_fill(x + 14, y + 2, 2, 3, C_GREY);
    px_t c = lv <= 0 ? ((g_arc_t & 8) ? C_RED : COL(90, 20, 20)) : (lv == 1 ? C_ORANGE : C_GREEN);
    gfx_fill(x + 1, y + 1, lv <= 0 ? 2 : lv * 4, 5, c);
}

void ar_add_vgrad(int x, int y, int w, int h, px_t top, px_t bot)
{
    for(int j = 0; j < h; j++) {
        px_t c = px_mix(bot, top, h > 1 ? (j * 32) / (h - 1) : 0);
        gfx_fill_mode(x, y + j, w, 1, c, DM_ADD);
    }
}

void ar_neon_rect(int x, int y, int w, int h, px_t c, int glow)
{
    for(int k = glow; k >= 1; k--) {
        px_t g = px_scale(c, 12 / k);
        gfx_fill_mode(x - k, y - k, w + 2 * k, 1, g, DM_ADD);
        gfx_fill_mode(x - k, y + h - 1 + k, w + 2 * k, 1, g, DM_ADD);
        gfx_fill_mode(x - k, y - k + 1, 1, h + 2 * k - 2, g, DM_ADD);
        gfx_fill_mode(x + w - 1 + k, y - k + 1, 1, h + 2 * k - 2, g, DM_ADD);
    }
    gfx_rect(x, y, w, h, c);
}

void ar_panel(int x, int y, int w, int h, px_t edge)
{
    gfx_fill_alpha(x, y, w, h, COL(4, 6, 20), 25);
    ar_neon_rect(x, y, w, h, edge, 2);
    gfx_rect(x + 1, y + 1, w - 2, h - 2, px_scale(edge, 9));
}

void ar_menu_item(int cx, int y, const char *s, bool sel, bool enabled, px_t accent)
{
    px_t c = !enabled ? COL(70, 70, 90) : (sel ? C_WHITE : COL(165, 175, 215));
    if(sel) {
        int w = text_width(s, 1) + 30;
        int pulse = 9 + ((isin256((int)g_arc_t * 8) * 5) >> 14);
        gfx_fill_alpha(cx - w / 2, y - 3, w, 13, px_scale(accent, 16), pulse);
        gfx_hline(cx - w / 2, cx + w / 2 - 1, y - 3, accent);
        gfx_hline(cx - w / 2, cx + w / 2 - 1, y + 9, accent);
        int nudge = (g_arc_t >> 3) & 1;
        text_draw(cx - w / 2 + 4 + nudge, y, G_TRI_R, accent, 1);
        text_draw(cx + w / 2 - 10 - nudge, y, G_TRI_L, accent, 1);
    }
    text_center_x(cx, y, s, c, 1, TX_SHADOW);
}
