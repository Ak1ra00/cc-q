// QUASAR - particles, explosions, screen shake and flashes.
#include "game.h"
#include <string.h>

enum { P_NONE = 0, P_SPARK, P_EMBER, P_DEBRIS, P_FIRE, P_SMOKE, P_RING, P_FLASH, P_TEXT };

typedef struct {
    float x, y, vx, vy;
    float r, dr;            // ring radius / growth, or sprite scale
    int16_t life, maxlife;
    int16_t delay;
    px_t col;
    uint8_t kind, size;
    int8_t rot;
    char txt[10];
} particle_t;

#define MAX_PARTICLES 640
static particle_t s_p[MAX_PARTICLES];
static int s_next;

int g_shake_x, g_shake_y;
int g_hitstop;
static int s_shake;
static px_t s_flash_col;
static int s_flash_t, s_flash_len;

void fx_reset(void)
{
    memset(s_p, 0, sizeof(s_p));
    s_next = 0;
    s_shake = 0;
    s_flash_t = 0;
    g_shake_x = g_shake_y = 0;
    g_hitstop = 0;
}

static particle_t *alloc(int kind)
{
    for(int n = 0; n < MAX_PARTICLES; n++) {
        int i = (s_next + n) % MAX_PARTICLES;
        if(s_p[i].kind == P_NONE) {
            s_next = (i + 1) % MAX_PARTICLES;
            particle_t *p = &s_p[i];
            memset(p, 0, sizeof(*p));
            p->kind = (uint8_t)kind;
            return p;
        }
    }
    // full: recycle the next one (visual only)
    particle_t *p = &s_p[s_next];
    s_next = (s_next + 1) % MAX_PARTICLES;
    memset(p, 0, sizeof(*p));
    p->kind = (uint8_t)kind;
    return p;
}

void fx_spark(float x, float y, float vx, float vy, px_t col, int life)
{
    particle_t *p = alloc(P_SPARK);
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->col = col;
    p->life = p->maxlife = (int16_t)life;
}

void fx_ember(float x, float y, float vx, float vy, px_t col, int life, int size)
{
    particle_t *p = alloc(P_EMBER);
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->col = col;
    p->size = (uint8_t)size;
    p->life = p->maxlife = (int16_t)life;
}

void fx_debris(float x, float y, float vx, float vy, px_t col, int life)
{
    particle_t *p = alloc(P_DEBRIS);
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->col = col;
    p->life = p->maxlife = (int16_t)life;
    p->size = (uint8_t)(1 + rndfx(2));
}

void fx_fire(float x, float y, float vx, float vy, int delay, int scale)
{
    particle_t *p = alloc(P_FIRE);
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->delay = (int16_t)delay;
    p->r = (float)scale;            // 1/256 units
    p->rot = (int8_t)rndfx(256);
    p->life = p->maxlife = ANIM_FIRE_N * 3;
}

void fx_smoke(float x, float y, float vx, float vy)
{
    particle_t *p = alloc(P_SMOKE);
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->life = p->maxlife = ANIM_SMOKE_N * 5;
}

void fx_ring(float x, float y, float r0, float speed, px_t col, int life, int thick)
{
    particle_t *p = alloc(P_RING);
    p->x = x;
    p->y = y;
    p->r = r0;
    p->dr = speed;
    p->col = col;
    p->size = (uint8_t)thick;
    p->life = p->maxlife = (int16_t)life;
}

void fx_flashball(float x, float y, int r, px_t col, int life)
{
    particle_t *p = alloc(P_FLASH);
    p->x = x;
    p->y = y;
    p->r = (float)r;
    p->col = col;
    p->life = p->maxlife = (int16_t)life;
}

void fx_text(float x, float y, const char *s, px_t col)
{
    particle_t *p = alloc(P_TEXT);
    p->x = x;
    p->y = y;
    p->vy = -0.6f;
    p->col = col;
    p->life = p->maxlife = 36;
    strncpy(p->txt, s, sizeof(p->txt) - 1);
}

void fx_hitspark(float x, float y, px_t col)
{
    for(int i = 0; i < 3; i++) {
        float a = rndfxf();
        float sp = rndfxr(1.5f, 3.5f);
        fx_spark(x, y, fcos_t(a) * sp, fsin_t(a) * sp, col, 6 + rndfx(5));
    }
    fx_flashball(x, y, 6, col, 4);
}

void fx_explode(float x, float y, int size)
{
    static const px_t spark_cols[4] = { COL(255, 240, 160), COL(255, 170, 60), COL(255, 110, 60), COL(255, 255, 255) };
    int nfire = 1 + size * 3;
    int nspark = 6 + size * 10;
    float spread = 3.0f + (float)size * 7.0f;

    fx_flashball(x, y, 14 + size * 12, COL(255, 200, 120), 6 + size * 2);
    for(int i = 0; i < nfire; i++) {
        float ox = rndfxr(-spread, spread) * 0.7f;
        float oy = rndfxr(-spread, spread) * 0.7f;
        int sc = 110 + size * 45 + rndfx(60);
        fx_fire(x + ox, y + oy, ox * 0.03f - 0.3f, oy * 0.03f, i * (size > 1 ? 2 : 1), sc);
    }
    for(int i = 0; i < nspark; i++) {
        float a = rndfxf();
        float sp = rndfxr(2.0f, 5.0f + (float)size * 2.0f);
        fx_spark(x, y, fcos_t(a) * sp, fsin_t(a) * sp, spark_cols[rndfx(4)], 10 + rndfx(14));
    }
    for(int i = 0; i < 4 + size * 4; i++) {
        float a = rndfxf();
        float sp = rndfxr(0.5f, 2.5f);
        fx_debris(x, y, fcos_t(a) * sp, fsin_t(a) * sp, COL(120 + rndfx(80), 110, 130), 20 + rndfx(30));
    }
    if(size >= 1) {
        fx_ring(x, y, 4.0f, 2.0f + (float)size * 1.2f, COL(255, 190, 120), 10 + size * 3, 2 + size);
        for(int i = 0; i < size * 2; i++) {
            fx_smoke(x + rndfxr(-spread, spread), y + rndfxr(-spread, spread), -0.4f, rndfxr(-0.2f, 0.2f));
        }
    }
    if(size >= 2) {
        fx_ring(x, y, 8.0f, 4.0f, COL(160, 200, 255), 16, 3);
    }
    fx_shake(size == 0 ? 1 : size * 3);
}

void fx_shake(int amount)
{
    if(!g_save.shake) return;
    if(amount > s_shake) s_shake = amount;
}

void fx_flash(px_t col, int frames)
{
    s_flash_col = col;
    s_flash_t = s_flash_len = frames;
}

void fx_update(void)
{
    // shake decays; offsets are random each frame
    if(s_shake > 0) {
        int a = s_shake > 8 ? 8 : s_shake;
        g_shake_x = rndfx(2 * a + 1) - a;
        g_shake_y = rndfx(2 * a + 1) - a;
        s_shake--;
    } else {
        g_shake_x = g_shake_y = 0;
    }
    if(s_flash_t > 0) s_flash_t--;

    for(int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_p[i];
        if(p->kind == P_NONE) continue;
        if(p->delay > 0) {
            p->delay--;
            continue;
        }
        p->x += p->vx;
        p->y += p->vy;
        switch(p->kind) {
            case P_SPARK:
                p->vx *= 0.9f;
                p->vy *= 0.9f;
                break;
            case P_EMBER:
                p->vx *= 0.96f;
                p->vy *= 0.96f;
                break;
            case P_DEBRIS:
                p->vx *= 0.98f;
                p->vy *= 0.98f;
                break;
            case P_FIRE:
                p->vx *= 0.94f;
                p->vy *= 0.94f;
                break;
            case P_RING:
                p->r += p->dr;
                p->dr *= 0.93f;
                break;
            case P_TEXT:
                p->vy *= 0.95f;
                break;
            default:
                break;
        }
        if(--p->life <= 0) p->kind = P_NONE;
    }
}

static void draw_particle(particle_t *p)
{
    int x = (int)p->x + g_shake_x, y = (int)p->y + g_shake_y;
    int life = p->life, ml = p->maxlife;
    switch(p->kind) {
        case P_SPARK: {
            int k = (life * 32) / ml;
            px_t c = px_scale(p->col, k < 8 ? 8 : k);
            int tx = (int)(p->x - p->vx * 2.5f) + g_shake_x;
            int ty = (int)(p->y - p->vy * 2.5f) + g_shake_y;
            gfx_line(tx, ty, x, y, c, DM_ADD);
            gfx_padd(x, y, C_WHITE);
            break;
        }
        case P_EMBER: {
            int k = (life * 32) / ml;
            px_t c = px_scale(p->col, k);
            if(p->size <= 1) gfx_padd(x, y, c);
            else gfx_glow(x, y, p->size, c);
            break;
        }
        case P_DEBRIS:
            if(life < 10 && (life & 1)) break;
            gfx_fill(x, y, p->size, p->size, p->col);
            break;
        case P_FIRE: {
            int f = (ml - life) / 3;
            if(f >= ANIM_FIRE_N) f = ANIM_FIRE_N - 1;
            gfx_sprite_rot(ANIM_FIRE[f], x, y, (int)p->rot * 7, (int)p->r, NULL, DM_NORMAL, 0);
            break;
        }
        case P_SMOKE: {
            int f = (ml - life) / 5;
            if(f >= ANIM_SMOKE_N) f = ANIM_SMOKE_N - 1;
            gfx_sprite(ANIM_SMOKE[f], x, y, 0, NULL, DM_HALF, 0);
            break;
        }
        case P_RING: {
            int k = (life * 32) / ml;
            px_t c = px_scale(p->col, k);
            int th = p->size;
            int r = (int)p->r;
            gfx_ring(x, y, r - th, r, c, DM_ADD);
            break;
        }
        case P_FLASH: {
            int k = (life * 32) / ml;
            int r = (int)(p->r * (0.6f + 0.4f * (float)life / (float)ml));
            gfx_glow(x, y, r, px_scale(p->col, k));
            break;
        }
        case P_TEXT: {
            px_t c = (life < 10 && (life & 2)) ? C_WHITE : p->col;
            text_center_x(x, y, p->txt, c, 1, TX_SHADOW);
            break;
        }
        default:
            break;
    }
}

void fx_draw(void)
{
    // everything except floating text
    for(int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_p[i];
        if(p->kind == P_NONE || p->kind == P_TEXT || p->delay > 0) continue;
        draw_particle(p);
    }
}

void fx_draw_top(void)
{
    for(int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_p[i];
        if(p->kind == P_TEXT && p->delay <= 0) draw_particle(p);
    }
    if(s_flash_t > 0) {
        int k = (s_flash_t * 20) / (s_flash_len ? s_flash_len : 1);
        gfx_fill_mode(0, 0, SCR_W, SCR_H, px_scale(s_flash_col, k), DM_ADD);
    }
}
