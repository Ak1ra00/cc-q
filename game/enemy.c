// QUASAR - enemies, enemy bullets and pickups.
#include "game.h"
#include <string.h>

enemy_t g_en[MAX_ENEMIES];
eshot_t g_eshots[MAX_ESHOTS];
pickup_t g_pu[MAX_PICKUPS];

typedef struct {
    const sprite_t *spr;
    float hp;
    uint16_t score;
    uint8_t radius;
    uint8_t expl;           // explosion size
    uint8_t gems;
} etype_t;

static const etype_t ETYPES[E_KINDS] = {
    [E_DRONE]    = { &SPR_DRONE,    1.6f,  100,  7, 0, 1 },
    [E_DARTER]   = { &SPR_DARTER,   1.0f,  150,  6, 0, 1 },
    [E_FIGHTER]  = { &SPR_FIGHTER,  5.0f,  300,  9, 1, 2 },
    [E_POD]      = { &SPR_POD,     10.0f,  500, 10, 1, 3 },
    [E_MINE]     = { &SPR_MINE,     3.0f,  200,  7, 1, 1 },
    [E_ROCK_L]   = { &SPR_ROCK_L,  14.0f,  300, 14, 1, 2 },
    [E_ROCK_M]   = { &SPR_ROCK_M,   5.0f,  150,  9, 0, 1 },
    [E_ROCK_S]   = { &SPR_ROCK_S,   2.0f,   80,  5, 0, 1 },
    [E_GUNSHIP]  = { &SPR_GUNSHIP, 30.0f, 1500, 16, 2, 5 },
    [E_CARRIER]  = { &SPR_CARRIER, 70.0f, 3000, 22, 2, 8 },
    [E_SPINNER]  = { &SPR_SPINNER,  5.0f,  250,  8, 0, 1 },
    [E_WEAVER]   = { &SPR_WEAVER,   1.6f,  100,  6, 0, 1 },
    [E_JELLY]    = { &SPR_JELLY,   12.0f,  600, 12, 1, 3 },
    [E_TURRET]   = { &SPR_TURRET,   7.0f,  400,  8, 1, 2 },
    [E_WISP]     = { NULL,          2.0f,  200,  6, 0, 1 },
    [E_EYE]      = { &SPR_EYE,     22.0f, 1200, 12, 2, 4 },
    [E_SEEKER]   = { &SPR_SEEKER,   3.0f,  250,  7, 0, 1 },
    [E_CAPSULE_CARRIER] = { &SPR_CARGO, 4.0f, 500, 9, 1, 2 },
};

// formation tracking
static struct { int8_t left; uint8_t drop; bool failed; } s_groups[16];

void group_start(int id, int count, int drop)
{
    if(id < 0 || id >= 16) return;
    s_groups[id].left = (int8_t)count;
    s_groups[id].drop = (uint8_t)drop;
    s_groups[id].failed = false;
}

void enemies_clear(void)
{
    memset(g_en, 0, sizeof(g_en));
    memset(g_eshots, 0, sizeof(g_eshots));
    memset(s_groups, 0, sizeof(s_groups));
}

int enemies_alive(void)
{
    int n = 0;
    for(int i = 0; i < MAX_ENEMIES; i++) n += g_en[i].alive ? 1 : 0;
    return n;
}

float enemy_radius(const enemy_t *e)
{
    return (float)ETYPES[e->type].radius;
}

enemy_t *enemy_spawn(int type, float x, float y)
{
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive) {
            memset(e, 0, sizeof(*e));
            e->alive = 1;
            e->type = (uint8_t)type;
            e->x = x;
            e->y = y;
            e->hp = ETYPES[type].hp * (difficulty() == 2 ? 1.25f : 1.0f) * (1.0f + 0.08f * (float)(g_stage.num - 1));
            e->group = -1;
            e->vx = -1.5f;
            e->cd = (int16_t)(20 + rnd(40));
            g_stage.spawned++;
            return e;
        }
    }
    return NULL;
}

static float fire_rate(void)
{
    static const float d[3] = { 0.55f, 1.0f, 1.45f };
    return d[difficulty()] * (1.0f + 0.12f * (float)(g_stage.num - 1));
}

static float bullet_speed(float base)
{
    static const float d[3] = { 0.8f, 1.0f, 1.18f };
    return base * d[difficulty()] * (1.0f + 0.04f * (float)(g_stage.num - 1));
}

float aim_at_player(float x, float y)
{
    return fatan2_t(g_pl.y - y, g_pl.x - x);
}

// ------------------------------------------------------------------ bullets

eshot_t *eshot_fire(float x, float y, float speed, float turns, int kind)
{
    if(x < -4 || x > SCR_W + 4) return NULL;
    for(int i = 0; i < MAX_ESHOTS; i++) {
        eshot_t *b = &g_eshots[i];
        if(!b->alive) {
            memset(b, 0, sizeof(*b));
            b->alive = 1;
            b->kind = (uint8_t)kind;
            b->x = x;
            b->y = y;
            b->vx = fcos_t(turns) * speed;
            b->vy = fsin_t(turns) * speed;
            b->life = 600;
            return b;
        }
    }
    return NULL;
}

eshot_t *eshot_aimed(float x, float y, float speed, int kind, float spread)
{
    float a = aim_at_player(x, y);
    if(spread > 0) a += (rndf() - 0.5f) * spread;
    return eshot_fire(x, y, speed, a, kind);
}

void eshot_ring(float x, float y, int n, float speed, float off, int kind)
{
    for(int i = 0; i < n; i++) eshot_fire(x, y, speed, off + (float)i / (float)n, kind);
}

void eshot_fan(float x, float y, int n, float speed, float center, float spread, int kind)
{
    if(n <= 1) {
        eshot_fire(x, y, speed, center, kind);
        return;
    }
    for(int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1) - 0.5f;
        eshot_fire(x, y, speed, center + t * spread, kind);
    }
}

int eshots_count(void)
{
    int n = 0;
    for(int i = 0; i < MAX_ESHOTS; i++) n += g_eshots[i].alive ? 1 : 0;
    return n;
}

void eshots_cancel(bool to_gems)
{
    int made = 0;
    for(int i = 0; i < MAX_ESHOTS; i++) {
        eshot_t *b = &g_eshots[i];
        if(!b->alive) continue;
        b->alive = 0;
        fx_ember(b->x, b->y, rndfxr(-1, 1), rndfxr(-1, 1), C_PINK, 14, 3);
        if(to_gems && made < 40 && (i & 1)) {
            pickup_spawn(PU_GEM, b->x, b->y);
            made++;
        }
    }
}

static const uint8_t EB_RADIUS[EB_KINDS] = { 2, 4, 6, 2, 4, 2, 4, 2 };
static const sprite_t *const EB_SPR[EB_KINDS] = {
    &SPR_EB_S_PINK, &SPR_EB_M_PINK, &SPR_EB_L_PINK, &SPR_EB_S_ORNG,
    &SPR_EB_M_ORNG, &SPR_EB_S_BLUE, &SPR_EB_M_BLUE, &SPR_EB_NEEDLE,
};

void eshots_update(void)
{
    player_t *p = &g_pl;
    bool vuln = player_can_be_hit();
    for(int i = 0; i < MAX_ESHOTS; i++) {
        eshot_t *b = &g_eshots[i];
        if(!b->alive) continue;
        if(b->delay > 0) {
            b->delay--;
            continue;
        }
        if(b->turn != 0) {
            float a = fatan2_t(b->vy, b->vx) + b->turn;
            float s = fsqrt(b->vx * b->vx + b->vy * b->vy);
            b->vx = fcos_t(a) * s;
            b->vy = fsin_t(a) * s;
        }
        b->vx += b->ax;
        b->vy += b->ay;
        b->x += b->vx;
        b->y += b->vy;
        if(--b->life <= 0 || b->x < -16 || b->x > SCR_W + 24 || b->y < -16 || b->y > SCR_H + 16) {
            b->alive = 0;
            continue;
        }
        if(vuln) {
            float r = (float)EB_RADIUS[b->kind] + 2.2f;
            float dx = b->x - (p->x + 2), dy = b->y - p->y;
            if(dx * dx + dy * dy < r * r) {
                b->alive = 0;
                player_hit();
                vuln = player_can_be_hit();
            }
        }
    }
}

void eshots_draw(void)
{
    int sx = g_shake_x, sy = g_shake_y;
    for(int i = 0; i < MAX_ESHOTS; i++) {
        eshot_t *b = &g_eshots[i];
        if(!b->alive) continue;
        int x = (int)b->x + sx, y = (int)b->y + sy;
        if(b->kind == EB_NEEDLE) {
            int a = (int)(fatan2_t(b->vy, b->vx) * 256.0f);
            gfx_sprite_rot(&SPR_EB_NEEDLE, x, y, a, 256, NULL, DM_NORMAL, 0);
        } else {
            gfx_sprite(EB_SPR[b->kind], x, y, 0, NULL, DM_NORMAL, 0);
        }
    }
}

// ------------------------------------------------------------------ pickups

const char *pickup_name(int kind)
{
    switch(kind) {
        case PU_POWER:   return "POWER UP";
        case PU_SPREAD:  return "SPREAD";
        case PU_LASER:   return "LASER";
        case PU_MISSILE: return "MISSILES";
        case PU_BOMB:    return "NOVA +1";
        case PU_HEAL:    return "REPAIR";
        case PU_OPTION:  return "OPTION";
        case PU_LIFE:    return "1UP";
        default:         return "";
    }
}

void pickups_clear(void)
{
    memset(g_pu, 0, sizeof(g_pu));
}

void pickup_spawn(int kind, float x, float y)
{
    for(int i = 0; i < MAX_PICKUPS; i++) {
        pickup_t *u = &g_pu[i];
        if(!u->alive) {
            memset(u, 0, sizeof(*u));
            u->alive = 1;
            u->kind = (uint8_t)kind;
            u->x = x;
            u->y = y;
            if(kind == PU_GEM) {
                float a = rndf();
                float s = rndr(0.5f, 2.2f);
                u->vx = fcos_t(a) * s;
                u->vy = fsin_t(a) * s;
                u->life = 300;
            } else {
                u->vx = -0.6f;
                u->vy = 0;
                u->life = 900;
            }
            return;
        }
    }
}

static void collect(pickup_t *u)
{
    player_t *p = &g_pl;
    char buf[16];
    const char *msg = pickup_name(u->kind);
    bool maxed = false;

    switch(u->kind) {
        case PU_GEM:
            g_run.gems++;
            add_score(50u * (uint32_t)g_run.mult, u->x, u->y, false);
            if(g_run.chain_timer > 0 && g_run.chain_timer < 20) g_run.chain_timer = 20;
            fx_ember(u->x, u->y, 0, -0.5f, C_CYAN, 10, 3);
            return;
        case PU_POWER:
            if(p->main_lvl < 5) p->main_lvl++;
            else maxed = true;
            break;
        case PU_SPREAD:
        case PU_LASER:
        case PU_MISSILE: {
            int w = u->kind - PU_SPREAD + 1;
            if(p->sub == w) {
                if(p->sub_lvl < 3) p->sub_lvl++;
                else maxed = true;
            } else {
                p->sub = w;
                if(p->sub_lvl < 1) p->sub_lvl = 1;
            }
            break;
        }
        case PU_BOMB:
            if(p->bombs < 6) p->bombs++;
            else maxed = true;
            break;
        case PU_HEAL:
            if(p->hp < p->maxhp) p->hp++;
            else maxed = true;
            break;
        case PU_OPTION:
            if(p->options < 2) {
                p->options++;
                p->opt_x[p->options - 1] = p->x;
                p->opt_y[p->options - 1] = p->y;
            } else {
                maxed = true;
            }
            break;
        case PU_LIFE:
            p->lives++;
            break;
        default:
            break;
    }
    if(maxed) {
        add_score(2000, u->x, u->y, true);
        msg = "MAX";
    }
    strcpy(buf, msg);
    fx_text(u->x, u->y - 12, buf, C_GOLD);
    fx_ring(u->x, u->y, 3, 2.5f, C_GOLD, 10, 2);
    fx_flashball(u->x, u->y, 16, COL(255, 220, 120), 6);
}

void pickups_update(void)
{
    player_t *p = &g_pl;
    for(int i = 0; i < MAX_PICKUPS; i++) {
        pickup_t *u = &g_pu[i];
        if(!u->alive) continue;
        float dx = p->x - u->x, dy = p->y - u->y;
        float d2 = dx * dx + dy * dy;
        if(u->kind == PU_GEM) {
            u->vx *= 0.94f;
            u->vy *= 0.94f;
            if(u->magnet || (p->alive && d2 < 70 * 70)) {
                u->magnet = 1;
                float d = fsqrt(d2) + 0.01f;
                u->vx += dx / d * 1.1f;
                u->vy += dy / d * 1.1f;
            } else {
                u->vx -= 0.03f;
            }
        } else {
            u->vy = fsin_t((float)(u->life) / 60.0f) * 0.4f;
            if(p->alive && d2 < 26 * 26) {
                float d = fsqrt(d2) + 0.01f;
                u->x += dx / d * 1.5f;
                u->y += dy / d * 1.5f;
            }
        }
        u->x += u->vx;
        u->y += u->vy;
        if(--u->life <= 0 || u->x < -20) {
            u->alive = 0;
            continue;
        }
        if(u->kind != PU_GEM) u->y = fclampf(u->y, 26, 226);
        if(p->alive && !p->entering && d2 < 13 * 13) {
            collect(u);
            u->alive = 0;
        }
    }
}

void pickups_draw(void)
{
    int sx = g_shake_x, sy = g_shake_y;
    for(int i = 0; i < MAX_PICKUPS; i++) {
        pickup_t *u = &g_pu[i];
        if(!u->alive) continue;
        if(u->life < 60 && (u->life & 2)) continue;
        int x = (int)u->x + sx, y = (int)u->y + sy;
        if(u->kind == PU_GEM) {
            int f = ((g_game.frame + i) >> 2) & 3;
            gfx_sprite(ANIM_GEM[f], x, y, 0, NULL, DM_NORMAL, 0);
            continue;
        }
        const sprite_t *s = &SPR_CAP_GOLD;
        const char *letter = "P";
        switch(u->kind) {
            case PU_SPREAD:  s = &SPR_CAP_RED;    letter = "S"; break;
            case PU_LASER:   s = &SPR_CAP_GREEN;  letter = "L"; break;
            case PU_MISSILE: s = &SPR_CAP_VIOLET; letter = "M"; break;
            case PU_BOMB:    s = &SPR_CAP_BLUE;   letter = "B"; break;
            case PU_HEAL:    s = &SPR_CAP_RED;    letter = G_HEART; break;
            case PU_OPTION:  s = &SPR_CAP_GOLD;   letter = "O"; break;
            case PU_LIFE:    s = &SPR_CAP_BLUE;   letter = G_STAR; break;
            default: break;
        }
        gfx_glow(x, y, 13, px_scale(C_WHITE, 6 + ((g_game.frame >> 1) & 7)));
        gfx_sprite(s, x, y, 0, NULL, DM_NORMAL, 0);
        text_draw_fx(x - 2, y - 4, letter, C_WHITE, 1, TX_SHADOW, NULL);
    }
}

// ------------------------------------------------------------------ damage & death

static void drop_gems(enemy_t *e, int n)
{
    for(int i = 0; i < n; i++) pickup_spawn(PU_GEM, e->x + rndr(-4, 4), e->y + rndr(-4, 4));
}

void enemy_kill(enemy_t *e, bool give_score)
{
    const etype_t *t = &ETYPES[e->type];
    e->alive = 0;
    fx_explode(e->x, e->y, t->expl);
    if(!give_score) return;

    g_stage.kills++;
    chain_kill();
    add_score((uint32_t)t->score * (uint32_t)g_run.mult, e->x, e->y, t->score >= 300);
    drop_gems(e, t->gems);

    if(e->drop) pickup_spawn(e->drop, e->x, e->y);

    if(e->group >= 0 && e->group < 16 && s_groups[e->group].left > 0) {
        if(--s_groups[e->group].left == 0 && !s_groups[e->group].failed && s_groups[e->group].drop) {
            pickup_spawn(s_groups[e->group].drop, e->x, e->y);
            fx_text(e->x, e->y - 14, "BONUS", C_GOLD);
            add_score(1000u * (uint32_t)g_run.mult, e->x, e->y, false);
        }
    }

    // splitting rocks
    if(e->type == E_ROCK_L || e->type == E_ROCK_M) {
        int child = e->type == E_ROCK_L ? E_ROCK_M : E_ROCK_S;
        int n = e->type == E_ROCK_L ? 3 : 2;
        for(int i = 0; i < n; i++) {
            enemy_t *c = enemy_spawn(child, e->x, e->y);
            if(!c) break;
            float a = 0.25f + (float)i / (float)n + rndr(-0.08f, 0.08f);
            float sp = rndr(1.0f, 2.0f);
            c->vx = fcos_t(a) * sp - 0.9f;
            c->vy = fsin_t(a) * sp;
            c->p[0] = rndr(-3, 3);
            g_stage.spawned--;      // children don't count against the kill ratio
        }
    }

    // mines burst
    if(e->type == E_MINE && difficulty() > 0) {
        eshot_ring(e->x, e->y, difficulty() == 2 ? 12 : 8, bullet_speed(1.6f), rndf(), EB_S_ORNG);
    }
}

void enemy_damage(enemy_t *e, float dmg, float hx, float hy)
{
    (void)hx;
    (void)hy;
    if(!e->alive || (e->flags & EF_NOHIT)) return;
    e->hp -= dmg;
    e->flash = 3;
    if(e->hp <= 0) enemy_kill(e, true);
}

static void leave_if_offscreen(enemy_t *e)
{
    if(e->x < -48 || e->x > SCR_W + 120 || e->y < -80 || e->y > SCR_H + 80) {
        e->alive = 0;
        if(e->group >= 0 && e->group < 16) s_groups[e->group].failed = true;
    }
}

// ------------------------------------------------------------------ behaviours

static void b_drone(enemy_t *e)
{
    // p0 base y, p1 amplitude, p2 frequency, p3 phase
    e->x += e->vx;
    e->y = e->p[0] + e->p[1] * fsin_t(e->p[3] + (float)e->t * e->p[2]);
    if(--e->cd <= 0) {
        e->cd = (int16_t)(90 + rnd(90));
        if(e->x > 60 && e->x < SCR_W - 10 && rndf() < 0.35f * fire_rate()) {
            eshot_aimed(e->x - 6, e->y, bullet_speed(2.2f), EB_S_PINK, 0.02f);
        }
    }
}

static void b_darter(enemy_t *e)
{
    // p0 frames of straight flight, p1 turn strength
    if(e->state == 0) {
        e->x += e->vx;
        e->y += e->vy;
        if(e->t > (int)e->p[0] || e->x < 150) {
            e->state = 1;
            float dy = g_pl.y - e->y;
            e->vy = fclampf(dy * 0.03f, -2.2f, 2.2f);
        }
    } else {
        e->vx -= 0.18f;
        if(e->vx < -7.0f) e->vx = -7.0f;
        e->vy *= 0.97f;
        e->x += e->vx;
        e->y += e->vy;
        if(e->st == 0 && difficulty() == 2 && rnd(3) == 0) eshot_aimed(e->x, e->y, bullet_speed(2.6f), EB_S_ORNG, 0);
    }
}

static void b_fighter(enemy_t *e)
{
    // p0 stop x, p1 bursts, p2 exit direction (-1 up, 1 down, 0 left)
    switch(e->state) {
        case 0:
            e->vx = (e->p[0] - e->x) * 0.06f;
            if(e->vx > -0.4f) e->vx = -0.4f;
            e->x += e->vx;
            e->y += e->vy;
            e->vy *= 0.95f;
            if(e->x <= e->p[0] + 2) {
                e->state = 1;
                e->st = 0;
                e->cd = 20;
            }
            break;
        case 1:
            e->y += fsin_t((float)e->t / 90.0f) * 0.4f;
            if(--e->cd <= 0) {
                int n = difficulty() == 0 ? 1 : 3;
                eshot_fan(e->x - 8, e->y, n, bullet_speed(2.6f), aim_at_player(e->x, e->y), 0.08f, EB_S_PINK);
                e->cd = (int16_t)(46 / fire_rate());
                e->p[1] -= 1;
                if(e->p[1] <= 0) {
                    e->state = 2;
                    e->st = 0;
                }
            }
            break;
        default:
            e->vx -= 0.08f;
            e->vy += e->p[2] * 0.1f;
            e->x += e->vx;
            e->y += e->vy;
            break;
    }
}

static void b_pod(enemy_t *e)
{
    // p0 stop x, p1 frames to stay
    e->ang = aim_at_player(e->x, e->y);
    if(e->state == 0) {
        e->x += (e->p[0] - e->x) * 0.05f - 0.2f;
        if(e->x <= e->p[0] + 3) e->state = 1;
    } else if(e->state == 1) {
        e->y += fsin_t((float)e->t / 120.0f) * 0.5f;
        if(--e->cd <= 0) {
            e->cd = (int16_t)(40 / fire_rate());
            float a = e->ang;
            float bx = e->x + fcos_t(a) * 12, by = e->y + fsin_t(a) * 12;
            if(difficulty() == 0) eshot_fire(bx, by, bullet_speed(2.4f), a, EB_M_ORNG);
            else eshot_fan(bx, by, 3, bullet_speed(2.6f), a, 0.06f, EB_S_ORNG);
            fx_flashball(bx, by, 8, C_ORANGE, 4);
        }
        if(e->st > (int)e->p[1]) e->state = 2;
    } else {
        e->vx -= 0.06f;
        e->x += e->vx;
    }
}

static void b_mine(enemy_t *e)
{
    e->x += e->vx;
    e->y += fsin_t((float)e->t / 70.0f + e->p[0]) * 0.5f;
    e->ang += 0.004f;
    float dx = g_pl.x - e->x, dy = g_pl.y - e->y;
    if(g_pl.alive && dx * dx + dy * dy < 34 * 34 && e->t > 20) {
        // proximity burst
        eshot_ring(e->x, e->y, difficulty() == 2 ? 12 : 8, bullet_speed(1.8f), rndf(), EB_S_ORNG);
        enemy_kill(e, false);
    }
}

static void b_rock(enemy_t *e)
{
    // p0 spin speed (1/256 turns per frame)
    e->x += e->vx;
    e->y += e->vy;
    e->ang += e->p[0] / 256.0f;
    if(e->y < 10 && e->vy < 0) e->vy = -e->vy;
    if(e->y > SCR_H - 6 && e->vy > 0) e->vy = -e->vy;
}

static void b_gunship(enemy_t *e)
{
    // p0 stop x, p1 lifetime
    if(e->state == 0) {
        e->x += (e->p[0] - e->x) * 0.03f - 0.2f;
        if(e->x <= e->p[0] + 4) e->state = 1;
    } else if(e->state == 1) {
        e->y += fsin_t((float)e->t / 160.0f) * 0.7f;
        if(--e->cd <= 0) {
            e->cd = (int16_t)(58 / fire_rate());
            if((e->t / 60) & 1) {
                eshot_fan(e->x - 18, e->y, 5 + difficulty(), bullet_speed(2.3f), 0.5f, 0.28f, EB_M_PINK);
            } else {
                for(int k = 0; k < 3; k++) {
                    eshot_t *b = eshot_aimed(e->x - 18, e->y + (float)(k - 1) * 8, bullet_speed(3.2f), EB_NEEDLE, 0);
                    if(b) b->delay = (int16_t)(k * 4);
                }
            }
        }
        if(e->st > (int)e->p[1]) e->state = 2;
    } else {
        e->vx -= 0.05f;
        e->x += e->vx;
    }
}

static void b_carrier(enemy_t *e)
{
    if(e->state == 0) {
        e->x += (e->p[0] - e->x) * 0.02f - 0.15f;
        if(e->x <= e->p[0] + 4) e->state = 1;
    } else if(e->state == 1) {
        e->y += fsin_t((float)e->t / 200.0f) * 0.4f;
        if(--e->cd <= 0) {
            e->cd = (int16_t)(80 / fire_rate());
            for(int k = -1; k <= 1; k += 2) {
                enemy_t *d = enemy_spawn(E_DARTER, e->x - 10, e->y + (float)k * 14);
                if(d) {
                    d->vx = -2.5f;
                    d->vy = (float)k * 1.4f;
                    d->p[0] = 18;
                    g_stage.spawned--;
                }
            }
            if(difficulty() > 0) eshot_ring(e->x, e->y, 10, bullet_speed(1.5f), rndf(), EB_S_BLUE);
        }
        if(e->st > (int)e->p[1]) e->state = 2;
    } else {
        e->vx -= 0.04f;
        e->x += e->vx;
    }
}

static void b_spinner(enemy_t *e)
{
    e->x += e->vx;
    e->y += e->vy;
    e->ang += 0.03f;
    if((e->y < 26 && e->vy < 0) || (e->y > SCR_H - 12 && e->vy > 0)) e->vy = -e->vy;
    if(e->t < 40 && e->x < SCR_W - 20) e->vx *= 0.99f;
}

static void b_weaver(enemy_t *e)
{
    // p0 start x, p1 base y, p2 amplitude, p3 speed; t starts negative (queued)
    if(e->t < 0) {
        e->flags |= EF_NOHIT;
        e->x = SCR_W + 40;
        return;
    }
    e->flags &= (uint8_t)~EF_NOHIT;
    float tt = (float)e->t;
    e->x = e->p[0] - tt * e->p[3];
    e->y = e->p[1] + e->p[2] * fsin_t(tt / 75.0f);
    e->ang = tt / 40.0f;
}

static void b_jelly(enemy_t *e)
{
    e->x += e->vx;
    e->y = e->p[0] + fsin_t((float)e->t / 110.0f) * 20.0f;
    if(e->x < SCR_W - 30 && --e->cd <= 0) {
        e->cd = (int16_t)(70 / fire_rate());
        eshot_ring(e->x, e->y + 4, 8 + difficulty() * 3, bullet_speed(1.4f), (float)e->t * 0.013f, EB_M_PINK);
    }
}

static void b_turret(enemy_t *e)
{
    // lives on the terrain; flagged EF_FLIPY when hanging from the ceiling
    e->x -= bg_speed();
    int top, bot;
    if(bg_terrain((int)e->x, &top, &bot)) {
        e->y = (e->flags & EF_FLIPY) ? (float)top + 6 : (float)bot - 6;
    }
    e->ang = aim_at_player(e->x, e->y);
    if(e->x < SCR_W - 16 && e->x > 30 && --e->cd <= 0) {
        e->cd = (int16_t)(55 / fire_rate());
        eshot_aimed(e->x + fcos_t(e->ang) * 8, e->y + fsin_t(e->ang) * 8, bullet_speed(2.5f), EB_S_ORNG, 0.02f);
    }
}

static void b_wisp(enemy_t *e)
{
    float want = aim_at_player(e->x, e->y);
    float cur = fatan2_t(e->vy, e->vx);
    float d = want - cur;
    while(d > 0.5f) d -= 1.0f;
    while(d < -0.5f) d += 1.0f;
    float turn = 0.006f + 0.002f * (float)difficulty();
    if(e->t > 150) turn = 0;
    if(d > turn) d = turn;
    if(d < -turn) d = -turn;
    float sp = 2.2f + 0.3f * (float)difficulty();
    cur += d;
    e->vx = fcos_t(cur) * sp;
    e->vy = fsin_t(cur) * sp;
    e->x += e->vx;
    e->y += e->vy;
    if((g_game.frame & 1) == 0) fx_ember(e->x, e->y, -e->vx * 0.3f, -e->vy * 0.3f, COL(255, 150, 40), 16, 2);
    if(e->t > 330) enemy_kill(e, false);
}

static void b_eye(enemy_t *e)
{
    // p0 stop x
    switch(e->state) {
        case 0:
            e->x += (e->p[0] - e->x) * 0.05f - 0.2f;
            if(e->x <= e->p[0] + 3) {
                e->state = 1;
                e->st = 0;
            }
            break;
        case 1:
            if(e->st > 20 && (e->st % (int)(9 / fire_rate() + 3)) == 0) {
                float a = (float)e->st * 0.021f;
                eshot_fire(e->x - 6, e->y, bullet_speed(2.0f), a, EB_S_BLUE);
                eshot_fire(e->x - 6, e->y, bullet_speed(2.0f), a + 0.5f, EB_S_BLUE);
                if(difficulty() == 2) eshot_fire(e->x - 6, e->y, bullet_speed(2.0f), a + 0.25f, EB_S_PINK);
            }
            if(e->st > 150) {
                e->state = 2;
                e->st = 0;
            }
            break;
        default:
            e->vx -= 0.05f;
            e->x += e->vx;
            break;
    }
}

static void b_seeker(enemy_t *e)
{
    // dash at the player's position every so often
    if(e->state == 0) {
        e->vx *= 0.92f;
        e->vy *= 0.92f;
        if(e->st > 26) {
            float a = aim_at_player(e->x, e->y);
            e->vx = fcos_t(a) * 4.5f;
            e->vy = fsin_t(a) * 4.5f;
            e->ang = a;
            e->state = 1;
            e->st = 0;
        }
    } else if(e->st > 16) {
        e->state = 0;
        e->st = 0;
        if(e->t > 200) {
            e->state = 2;
        }
    }
    if(e->state == 2) e->vx -= 0.2f;
    e->x += e->vx;
    e->y += e->vy;
}

static void b_cargo(enemy_t *e)
{
    e->x += e->vx;
    e->y = e->p[0] + fsin_t((float)e->t / 90.0f) * 30.0f;
}

void enemies_update(void)
{
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive) continue;
        if(e->flash) e->flash--;
        if(e->t < 0) {
            // queued member of a formation: wait off-screen, untouchable
            e->t++;
            if(e->t < 0) e->flags |= EF_NOHIT;
            else e->flags &= (uint8_t)~EF_NOHIT;
            continue;
        }
        switch(e->type) {
            case E_DRONE:   b_drone(e); break;
            case E_DARTER:  b_darter(e); break;
            case E_FIGHTER: b_fighter(e); break;
            case E_POD:     b_pod(e); break;
            case E_MINE:    b_mine(e); break;
            case E_ROCK_L:
            case E_ROCK_M:
            case E_ROCK_S:  b_rock(e); break;
            case E_GUNSHIP: b_gunship(e); break;
            case E_CARRIER: b_carrier(e); break;
            case E_SPINNER: b_spinner(e); break;
            case E_WEAVER:  b_weaver(e); break;
            case E_JELLY:   b_jelly(e); break;
            case E_TURRET:  b_turret(e); break;
            case E_WISP:    b_wisp(e); break;
            case E_EYE:     b_eye(e); break;
            case E_SEEKER:  b_seeker(e); break;
            case E_CAPSULE_CARRIER: b_cargo(e); break;
            default: break;
        }
        e->t++;
        e->st++;
        if(e->alive && e->t > 0) leave_if_offscreen(e);
    }
}

static void draw_hpbar(enemy_t *e, int x, int y)
{
    float mhp = ETYPES[e->type].hp;
    if(mhp < 20 || e->hp >= mhp * 0.99f) return;
    int w = 24;
    int fill = (int)((float)w * e->hp / mhp);
    gfx_fill(x - w / 2, y, w, 2, COL(40, 20, 30));
    gfx_fill(x - w / 2, y, fill, 2, C_RED);
}

void enemies_draw(void)
{
    int sx = g_shake_x, sy = g_shake_y;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive || (e->t < 0)) continue;
        const etype_t *t = &ETYPES[e->type];
        int x = (int)e->x + sx, y = (int)e->y + sy;
        int mode = e->flash ? DM_FLASH : DM_NORMAL;
        px_t fc = C_WHITE;

        switch(e->type) {
            case E_ROCK_L:
            case E_ROCK_M:
            case E_ROCK_S:
            case E_SPINNER:
            case E_MINE:
                gfx_sprite_rot(t->spr, x, y, (int)(e->ang * 256.0f), 256, NULL, mode, fc);
                break;
            case E_WEAVER:
                gfx_glow(x, y, 10, COL(40, 90, 60));
                gfx_sprite_rot(t->spr, x, y, (int)(e->ang * 256.0f), 256, NULL, mode, fc);
                break;
            case E_WISP: {
                int r = 7 + ((g_game.frame + i) & 3);
                gfx_glow(x, y, r + 6, COL(255, 110, 20));
                gfx_disc(x, y, 4, e->flash ? C_WHITE : COL(255, 220, 120), DM_NORMAL);
                gfx_disc(x, y, 2, C_WHITE, DM_NORMAL);
                break;
            }
            case E_POD: {
                gfx_sprite(t->spr, x, y, 0, NULL, mode, fc);
                int bx = x + (icos256((int)(e->ang * 256.0f)) * 11 >> 14);
                int by = y + (isin256((int)(e->ang * 256.0f)) * 11 >> 14);
                gfx_line(x, y, bx, by, COL(40, 40, 60), DM_NORMAL);
                gfx_line(x, y + 1, bx, by + 1, COL(160, 150, 190), DM_NORMAL);
                gfx_disc(x, y, 3, e->flash ? C_WHITE : COL(255, 90, 60), DM_NORMAL);
                break;
            }
            case E_TURRET: {
                int flip = (e->flags & EF_FLIPY) ? SPR_FLIPY : 0;
                int bx = x + (icos256((int)(e->ang * 256.0f)) * 10 >> 14);
                int by = y + (isin256((int)(e->ang * 256.0f)) * 10 >> 14);
                gfx_line(x, y, bx, by, COL(30, 30, 40), DM_NORMAL);
                gfx_line(x + 1, y, bx + 1, by, COL(200, 190, 170), DM_NORMAL);
                gfx_sprite(t->spr, x, y, flip, NULL, mode, fc);
                break;
            }
            case E_JELLY: {
                int pulse = (isin256(e->t * 5) * 20) >> 14;
                gfx_glow(x, y, 18, COL(80, 20, 70));
                gfx_sprite_rot(t->spr, x, y, 0, 256 + pulse, NULL, mode, fc);
                break;
            }
            case E_EYE: {
                gfx_glow(x, y, 20, COL(40, 30, 90));
                gfx_sprite(t->spr, x, y, 0, NULL, mode, fc);
                int open = e->state == 1 ? iclamp(e->st / 3, 0, 5) : iclamp(5 - e->st / 3, 0, 5);
                if(e->state == 0) open = 0;
                gfx_fill(x - 6, y - open, 12, open * 2 + 1, COL(255, 240, 250));
                if(open > 1) {
                    int px = x - 2 + (int)((g_pl.x - e->x) * 0.02f);
                    int py = y + (int)((g_pl.y - e->y) * 0.02f);
                    gfx_disc(px, py, open > 3 ? 3 : 2, COL(60, 20, 140), DM_NORMAL);
                    gfx_pset(px - 1, py - 1, C_WHITE);
                }
                break;
            }
            case E_SEEKER: {
                int flip = e->vx > 0 ? SPR_FLIPX : 0;
                gfx_sprite(t->spr, x, y, flip, NULL, mode, fc);
                if(e->state == 1) gfx_glow(x + (flip ? -8 : 8), y, 6, C_ORANGE);
                break;
            }
            case E_CAPSULE_CARRIER:
                gfx_glow(x, y, 14, px_scale(C_GOLD, 8 + (g_game.frame & 7)));
                gfx_sprite(t->spr, x, y, 0, NULL, mode, fc);
                break;
            case E_DARTER: {
                gfx_sprite(t->spr, x, y, 0, NULL, mode, fc);
                gfx_glow(x + 9, y, 4 + (g_game.frame & 1), COL(255, 120, 40));
                break;
            }
            default:
                gfx_sprite(t->spr, x, y, 0, NULL, mode, fc);
                break;
        }
        draw_hpbar(e, x, y - t->radius - 5);
    }
}
