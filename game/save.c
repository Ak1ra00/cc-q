// QUASAR - persistent settings, the high score table and the saved run.
//
// Serialised as a small versioned blob with a CRC. The host stores it; if it is
// ever damaged we simply fall back to defaults.
#include "game.h"
#include <string.h>

savedata_t g_save;

#define SAVE_MAGIC      0x31525351u     // "QSR1"
#define SAVE_VERSION    1

static const char *const DEFAULT_NAMES[NUM_SCORES] = {
    "NOVA", "ORION", "VEGA", "LYRA", "RIGEL", "ALTAIR", "DENEB", "SIRIUS",
};

void save_defaults(void)
{
    memset(&g_save, 0, sizeof(g_save));
    g_save.difficulty = 1;
    g_save.shake = 1;
    g_save.brightness = 3;
    g_save.show_fps = 0;
    g_save.vsync = 0;
    g_save.max_stage = 1;
    for(int i = 0; i < NUM_SCORES; i++) {
        strcpy(g_save.scores[i].name, DEFAULT_NAMES[i]);
        g_save.scores[i].score = (uint32_t)(200000 - i * 22000);
        g_save.scores[i].stage = (uint8_t)(i < 2 ? 3 : (i < 5 ? 2 : 1));
        g_save.scores[i].diff = 1;
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

// layout: magic(4) version(1) len(1) settings(8) plays(4) scores(8 x 18) crc(4),
// then the saved run as a block of its own: magic(4) len(1) fields crc(4).
// Version 1.1.0 and older read the first part and ignore the rest, so a save
// works in both directions.
#define REC_LEN     (NAME_LEN + 2 + 4 + 2)      // name, stage, diff, score, pad
#define BODY_LEN    (4 + 1 + 1 + 8 + 4 + NUM_SCORES * REC_LEN)
#define RUN_MAGIC   0x31585351u     // "QSX1"
#define RUN_LEN     (4 + 1 + 9 + 5 * 4)
#define TOTAL_LEN   (BODY_LEN + 4 + RUN_LEN + 4)

static uint8_t *pack_run(uint8_t *p)
{
    const savedrun_t *r = &g_save.run;
    uint8_t *start = p;
    put32(p, RUN_MAGIC);
    p += 4;
    *p++ = RUN_LEN;
    *p++ = r->stage;
    *p++ = r->diff;
    *p++ = r->lives;
    *p++ = r->maxhp;
    *p++ = r->main_lvl;
    *p++ = r->sub;
    *p++ = r->sub_lvl;
    *p++ = r->options;
    *p++ = r->continues;
    const uint32_t v[5] = { r->score, r->next_life, r->frames, r->gems, r->best_chain };
    for(int i = 0; i < 5; i++, p += 4) put32(p, v[i]);
    put32(p, crc32(start, RUN_LEN));
    return p + 4;
}

static void unpack_run(const uint8_t *p, savedrun_t *out)
{
    savedrun_t r;
    r.stage = p[0];
    r.diff = p[1];
    r.lives = p[2];
    r.maxhp = p[3];
    r.main_lvl = p[4];
    r.sub = p[5];
    r.sub_lvl = p[6];
    r.options = p[7];
    r.continues = p[8];
    r.score = get32(p + 9);
    r.next_life = get32(p + 13);
    r.frames = get32(p + 17);
    r.gems = get32(p + 21);
    r.best_chain = get32(p + 25);
    // anything out of range means no run to continue, never a strange one
    if(r.stage < 2 || r.stage > 5 || r.diff > 2 || r.lives > 99 || r.maxhp < 1 || r.maxhp > 9 ||
       r.main_lvl < 1 || r.main_lvl > 5 || r.sub > 3 || r.sub_lvl > 3 || r.options > 2 ||
       r.continues > 99 || r.gems > 1000000u || r.best_chain > 1000000u) {
        return;
    }
    *out = r;
}

int save_pack(uint8_t *buf, int max)
{
    if(max < TOTAL_LEN) return 0;
    uint8_t *p = buf;
    put32(p, SAVE_MAGIC);
    p += 4;
    *p++ = SAVE_VERSION;
    *p++ = (uint8_t)BODY_LEN;
    *p++ = g_save.difficulty;
    *p++ = g_save.shake;
    *p++ = g_save.brightness;
    *p++ = g_save.show_fps;
    *p++ = g_save.vsync;
    *p++ = g_save.max_stage;
    *p++ = g_save.clears;
    *p++ = g_save.auto_off;
    put32(p, g_save.plays);
    p += 4;
    for(int i = 0; i < NUM_SCORES; i++) {
        hiscore_t *h = &g_save.scores[i];
        memset(p, 0, REC_LEN);
        memcpy(p, h->name, strlen(h->name) > NAME_LEN ? NAME_LEN : strlen(h->name));
        p[NAME_LEN] = h->stage;
        p[NAME_LEN + 1] = h->diff;
        put32(p + NAME_LEN + 2, h->score);
        p += REC_LEN;
    }
    put32(p, crc32(buf, BODY_LEN));
    pack_run(p + 4);
    return TOTAL_LEN;
}

bool save_unpack(const uint8_t *buf, int len)
{
    if(len < BODY_LEN + 4) return false;
    if(get32(buf) != SAVE_MAGIC) return false;
    if(buf[4] != SAVE_VERSION || buf[5] != BODY_LEN) return false;
    if(get32(buf + BODY_LEN) != crc32(buf, BODY_LEN)) return false;

    const uint8_t *p = buf + 6;
    savedata_t s;
    memset(&s, 0, sizeof(s));
    s.difficulty = p[0] > 2 ? 1 : p[0];
    s.shake = p[1] ? 1 : 0;
    s.brightness = p[2] > 4 ? 3 : p[2];
    s.show_fps = p[3] ? 1 : 0;
    s.vsync = p[4] > 2 ? 0 : p[4];
    s.max_stage = (p[5] < 1 || p[5] > 5) ? 1 : p[5];
    s.clears = p[6];
    s.auto_off = p[7] > 2 ? 0 : p[7];
    p += 8;
    s.plays = get32(p);
    p += 4;
    for(int i = 0; i < NUM_SCORES; i++) {
        hiscore_t *h = &s.scores[i];
        for(int k = 0; k < NAME_LEN; k++) {
            char c = (char)p[k];
            h->name[k] = (c >= 32 && c < 127) ? c : 0;
        }
        h->name[NAME_LEN] = 0;
        h->stage = p[NAME_LEN] > 6 ? 1 : p[NAME_LEN];
        h->diff = p[NAME_LEN + 1] > 2 ? 1 : p[NAME_LEN + 1];
        h->score = get32(p + NAME_LEN + 2);
        p += REC_LEN;
    }
    // a saved run is optional: a missing or damaged one only loses CONTINUE
    const uint8_t *r = buf + BODY_LEN + 4;
    if(len >= TOTAL_LEN && get32(r) == RUN_MAGIC && r[4] == RUN_LEN &&
       get32(r + RUN_LEN) == crc32(r, RUN_LEN)) {
        unpack_run(r + 5, &s.run);
    }
    g_save = s;
    return true;
}

int score_rank(uint32_t score)
{
    for(int i = 0; i < NUM_SCORES; i++) {
        if(score > g_save.scores[i].score) return i;
    }
    return -1;
}

void score_insert(int rank, const char *name, uint32_t score, int stage, int diff)
{
    if(rank < 0 || rank >= NUM_SCORES) return;
    for(int i = NUM_SCORES - 1; i > rank; i--) g_save.scores[i] = g_save.scores[i - 1];
    hiscore_t *h = &g_save.scores[rank];
    memset(h, 0, sizeof(*h));
    strncpy(h->name, name, NAME_LEN);
    h->score = score;
    h->stage = (uint8_t)stage;
    h->diff = (uint8_t)diff;
}
