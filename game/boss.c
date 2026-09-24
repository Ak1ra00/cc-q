// QUASAR - the five bosses.
#include "game.h"
#include <string.h>

boss_t g_boss;

static float s_hist_x[160], s_hist_y[160];     // hydra body trail
static int s_hist_i;
static int s_beam_t;                            // warden main cannon
static float s_beam_y;

static float hp_scale(void)
{
    static const float d[3] = { 0.75f, 1.0f, 1.3f };
    return d[difficulty()];
}

static float bspeed(float v)
{
    static const float d[3] = { 0.82f, 1.0f, 1.15f };
    return v * d[difficulty()];
}

static float brate(void)
{
    static const float d[3] = { 0.6f, 1.0f, 1.4f };
    return d[difficulty()];
}

void boss_clear(void)
{
    memset(&g_boss, 0, sizeof(g_boss));
    s_beam_t = 0;
}

static void set_part(int i, float r, float hp, bool vuln)
{
    g_boss.part[i].r = r;
    g_boss.part[i].hp = g_boss.part[i].maxhp = hp * hp_scale();
    g_boss.part[i].alive = true;
    g_boss.part[i].vuln = vuln;
    if(i >= g_boss.nparts) g_boss.nparts = i + 1;
}

void boss_start(int kind)
{
    boss_t *b = &g_boss;
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->kind = kind;
    b->x = SCR_W + 90;
    b->y = 124;
    b->t = 0;
    s_beam_t = 0;
    switch(kind) {
        case 1:
            strcpy(b->name, "GLACIER MAW");
            set_part(0, 11, 230, true);
            for(int i = 1; i <= 4; i++) set_part(i, 9, 18, true);
            break;
        case 2:
            strcpy(b->name, "HYDRA");
            set_part(0, 13, 270, true);
            for(int i = 1; i <= 10; i++) set_part(i, (float)(9 - i / 3), 9999, false);
            for(int i = 0; i < 160; i++) {
                s_hist_x[i] = b->x;
                s_hist_y[i] = b->y;
            }
            break;
        case 3:
            strcpy(b->name, "WARDEN");
            set_part(0, 12, 200, false);        // reactor opens later
            set_part(1, 10, 55, true);          // top turret
            set_part(2, 10, 55, true);          // bottom turret
            set_part(3, 12, 70, true);          // missile bay
            set_part(4, 12, 80, true);          // main cannon
            break;
        case 4:
            strcpy(b->name, "HELIOS");
            set_part(0, 30, 340, true);
            break;
        default:
            strcpy(b->name, "SINGULARITY");
            set_part(0, 12, 480, false);
            for(int i = 1; i <= 4; i++) set_part(i, 8, 40, true);
            break;
    }
    b->hp = b->maxhp = g_boss.part[0].maxhp;
}

// ------------------------------------------------------------------ hit tests

bool boss_hit_test(float x, float y, float r, float dmg, float hx, float hy)
{
    boss_t *b = &g_boss;
    if(!b->active || b->dying || !b->entered) return false;
    for(int i = b->nparts - 1; i >= 0; i--) {
        if(!b->part[i].alive) continue;
        float dx = x - b->part[i].x, dy = y - b->part[i].y;
        float rr = r + b->part[i].r;
        if(dx * dx + dy * dy > rr * rr) continue;
        if(!b->part[i].vuln) {
            if(rndfx(3) == 0) fx_spark(hx, hy, -2, rndfxr(-2, 2), C_GREY, 6);
            return true;
        }
        b->part[i].hp -= dmg;
        b->part[i].flash = 2;
        if(i == 0) {
            b->hp = b->part[0].hp;
            b->flash = 2;
            if(b->hp <= 0) {
                b->hp = 0;
                b->dying = true;
                b->death_t = 0;
                g_hitstop = 6;
                eshots_cancel(true);
            }
        } else if(b->part[i].hp <= 0) {
            b->part[i].alive = false;
            fx_explode(b->part[i].x, b->part[i].y, 2);
            add_score(5000u * (uint32_t)g_run.mult, b->part[i].x, b->part[i].y, true);
            for(int k = 0; k < 6; k++) pickup_spawn(PU_GEM, b->part[i].x, b->part[i].y);
        }
        return true;
    }
    return false;
}

bool boss_touch_player(float x, float y, float r)
{
    boss_t *b = &g_boss;
    if(!b->entered) return false;
    for(int i = 0; i < b->nparts; i++) {
        if(!b->part[i].alive) continue;
        float dx = x - b->part[i].x, dy = y - b->part[i].y;
        float rr = r + b->part[i].r * 0.8f;
        if(dx * dx + dy * dy < rr * rr) return true;
    }
    if(b->kind == 3 && x > b->x - 44 && y > b->y - 40 && y < b->y + 40) return true;
    if(s_beam_t > 60 && s_beam_t < 100) {
        if(y > s_beam_y - 9 && y < s_beam_y + 9 && x < b->x - 40) return true;
    }
    return false;
}

static int alive_parts(int from, int to)
{
    int n = 0;
    for(int i = from; i <= to; i++) n += g_boss.part[i].alive ? 1 : 0;
    return n;
}

// ------------------------------------------------------------------ 1: glacier maw

static void boss1(void)
{
    boss_t *b = &g_boss;
    float f = b->hp / b->maxhp;
    b->phase = f < 0.5f ? 2 : 1;

    // hover, with occasional lunges in phase 2
    float hx = 238, hy = 124 + fsin_t((float)b->t / 240.0f) * 50;
    if(b->phase == 2 && (b->t % 360) > 300) hx = 170;
    b->x += (hx - b->x) * 0.03f;
    b->y += (hy - b->y) * 0.05f;

    b->part[0].x = b->x;
    b->part[0].y = b->y;
    float spin = (float)b->t * (b->phase == 2 ? 0.006f : 0.004f);
    for(int i = 1; i <= 4; i++) {
        float a = spin + (float)(i - 1) * 0.25f;
        b->part[i].x = b->x + fcos_t(a) * 36;
        b->part[i].y = b->y + fsin_t(a) * 30;
        if(b->part[i].alive && ((b->t + i * 23) % (int)(70 / brate() + 20)) == 0 && b->part[i].x < b->x) {
            eshot_aimed(b->part[i].x, b->part[i].y, bspeed(2.6f), EB_S_BLUE, 0.05f);
        }
    }
    // shards grow back in phase 2
    if(b->phase == 2 && (b->t % 600) == 0) {
        for(int i = 1; i <= 4; i++) {
            if(!b->part[i].alive) {
                b->part[i].alive = true;
                b->part[i].hp = b->part[i].maxhp * 0.6f;
                fx_ring(b->part[i].x, b->part[i].y, 2, 2, C_CYAN, 12, 2);
            }
        }
    }

    int cyc = b->t % 300;
    if(cyc == 60 || cyc == 90 || (b->phase == 2 && cyc == 120)) {
        eshot_fan(b->x - 16, b->y, 5 + difficulty() * 2, bspeed(3.0f), aim_at_player(b->x - 16, b->y), 0.22f, EB_NEEDLE);
    }
    if(cyc == 180 || (difficulty() > 0 && cyc == 200)) {
        eshot_ring(b->x, b->y, 16 + difficulty() * 4, bspeed(1.8f), (float)b->t * 0.01f, EB_M_BLUE);
    }
    if(b->phase == 2 && cyc > 220 && cyc < 290 && (cyc % (int)(8 / brate() + 2)) == 0) {
        float x = 20.0f + (float)rnd(220);
        bool top = rnd(2);
        eshot_t *s = eshot_fire(x, top ? 2.0f : (float)SCR_H - 2, bspeed(2.4f), top ? 0.25f - 0.03f : -0.25f + 0.03f, EB_NEEDLE);
        if(s) s->ay = top ? 0.02f : -0.02f;
    }
}

// ------------------------------------------------------------------ 2: hydra

static void boss2(void)
{
    boss_t *b = &g_boss;
    float f = b->hp / b->maxhp;
    b->phase = f < 0.5f ? 2 : 1;
    float t = (float)b->t;

    if(b->phase == 2 && (b->t % 420) > 330) {
        // dive at the player's height and come back
        float k = (float)((b->t % 420) - 330) / 90.0f;
        float tx = 60 + 190 * (1 - fsin_t(k * 0.5f));
        b->x += (tx - b->x) * 0.08f;
        b->y += (g_pl.y - b->y) * 0.04f;
    } else {
        float tx = 205 + fsin_t(t / 400.0f) * 75;
        float ty = 122 + fsin_t(t / 250.0f + 0.1f) * 82;
        b->x += (tx - b->x) * 0.05f;
        b->y += (ty - b->y) * 0.05f;
    }

    s_hist_i = (s_hist_i + 1) % 160;
    s_hist_x[s_hist_i] = b->x;
    s_hist_y[s_hist_i] = b->y;
    b->part[0].x = b->x;
    b->part[0].y = b->y;
    for(int i = 1; i <= 10; i++) {
        int k = (s_hist_i + 160 - i * 7) % 160;
        b->part[i].x = s_hist_x[k] + 6;
        b->part[i].y = s_hist_y[k];
    }

    int cyc = b->t % 240;
    if((cyc % (int)(50 / brate())) == 10) {
        eshot_fan(b->x - 14, b->y, 3, bspeed(2.8f), aim_at_player(b->x - 14, b->y), 0.1f, EB_M_PINK);
    }
    if(cyc >= 120 && cyc < 180 && (cyc % 6) == 0) {
        int i = 1 + (cyc - 120) / 6;
        if(i <= 10) eshot_aimed(b->part[i].x, b->part[i].y, bspeed(1.7f), EB_S_PINK, 0.1f);
    }
    if(b->phase == 2 && (b->t % 5) == 0 && (b->t % 420) < 200) {
        float a = t * 0.017f;
        eshot_fire(b->x, b->y, bspeed(2.2f), a, EB_S_ORNG);
        if(difficulty() > 0) eshot_fire(b->x, b->y, bspeed(2.2f), a + 0.5f, EB_S_ORNG);
    }
}

// ------------------------------------------------------------------ 3: warden

static void boss3(void)
{
    boss_t *b = &g_boss;
    b->x += (262 - b->x) * 0.02f;
    b->y = 124 + fsin_t((float)b->t / 300.0f) * 18;

    b->part[0].x = b->x - 20;  b->part[0].y = b->y;
    b->part[1].x = b->x - 18;  b->part[1].y = b->y - 34;
    b->part[2].x = b->x - 18;  b->part[2].y = b->y + 34;
    b->part[3].x = b->x + 20;  b->part[3].y = b->y - 22;
    b->part[4].x = b->x - 40;  b->part[4].y = b->y + 12;

    int left = alive_parts(1, 4);
    b->phase = left == 0 ? 2 : 1;
    b->part[0].vuln = left <= 1;

    for(int i = 1; i <= 2; i++) {
        if(b->part[i].alive && ((b->t + i * 40) % (int)(60 / brate())) == 0) {
            eshot_fan(b->part[i].x - 8, b->part[i].y, 3, bspeed(2.7f), aim_at_player(b->part[i].x, b->part[i].y), 0.07f, EB_S_ORNG);
        }
    }
    if(b->part[3].alive && (b->t % (int)(150 / brate())) == 70) {
        for(int k = 0; k < 2; k++) {
            enemy_t *e = enemy_spawn(E_SEEKER, b->part[3].x, b->part[3].y - 6);
            if(e) {
                e->vx = -1.5f;
                e->vy = k ? 2.0f : -2.0f;
                g_stage.spawned--;
            }
        }
    }
    // main cannon: telegraph, then a screen-wide beam
    if(b->part[4].alive) {
        if(s_beam_t == 0 && (b->t % 330) == 200) {
            s_beam_t = 1;
            s_beam_y = b->part[4].y;
        }
    }
    if(s_beam_t > 0) {
        s_beam_t++;
        if(s_beam_t < 60) s_beam_y += (b->part[4].y - s_beam_y) * 0.2f;
        if(s_beam_t == 60) {
            fx_shake(6);
            fx_flash(COL(255, 200, 120), 5);
        }
        if(s_beam_t > 100 || !b->part[4].alive) s_beam_t = 0;
    }
    if(b->part[0].vuln && (b->t % (int)(80 / brate())) == 0) {
        eshot_ring(b->part[0].x, b->part[0].y, 14 + difficulty() * 4, bspeed(2.0f), (float)b->t * 0.013f, EB_M_ORNG);
    }
    if(b->phase == 2 && (b->t % 9) == 0) {
        eshot_fire(b->part[0].x, b->part[0].y, bspeed(2.3f), 0.5f + fsin_t((float)b->t / 90.0f) * 0.15f, EB_NEEDLE);
    }
}

// ------------------------------------------------------------------ 4: helios

static void boss4(void)
{
    boss_t *b = &g_boss;
    float f = b->hp / b->maxhp;
    b->phase = f < 0.45f ? 2 : 1;
    b->x += (228 - b->x) * 0.03f;
    b->y = 122 + fsin_t((float)b->t / 330.0f) * 40;
    b->part[0].x = b->x;
    b->part[0].y = b->y;

    int cyc = b->t % 480;
    if(cyc < 180) {
        int every = (int)(4 / brate()) + 2;
        if((cyc % every) == 0) {
            float a = (float)b->t * 0.013f;
            int arms = b->phase == 2 ? 4 : 3;
            for(int k = 0; k < arms; k++) eshot_fire(b->x, b->y, bspeed(2.2f), a + (float)k / (float)arms, EB_S_ORNG);
            if(b->phase == 2) {
                for(int k = 0; k < 2; k++) eshot_fire(b->x, b->y, bspeed(1.7f), -a * 1.3f + (float)k * 0.5f, EB_S_PINK);
            }
        }
    } else if(cyc < 300) {
        if((cyc % 40) == 0) {
            eshot_fan(b->x - 20, b->y, 5 + difficulty(), bspeed(3.0f), aim_at_player(b->x, b->y), 0.3f, EB_L_PINK);
        }
    } else if(cyc == 330 || (b->phase == 2 && cyc == 390)) {
        for(int k = 0; k < 2 + difficulty(); k++) {
            enemy_t *e = enemy_spawn(E_WISP, b->x - 10, b->y + (float)(k * 20 - 20));
            if(e) {
                e->vx = -2;
                e->vy = (float)(k - 1);
                g_stage.spawned--;
            }
        }
    } else if(cyc > 400 && (cyc % 20) == 0) {
        eshot_ring(b->x, b->y, 20, bspeed(1.6f), (float)cyc * 0.01f, EB_M_ORNG);
    }
}

// ------------------------------------------------------------------ 5: singularity

static void boss5(void)
{
    boss_t *b = &g_boss;
    float f = b->hp / b->maxhp;
    b->phase = f < 0.33f ? 3 : (f < 0.66f ? 2 : 1);
    b->x += (230 - b->x) * 0.02f;
    b->y = 120 + fsin_t((float)b->t / 400.0f) * 30;
    b->part[0].x = b->x;
    b->part[0].y = b->y;

    // the eye opens in a rhythm; satellites guard it
    int cyc = b->t % 300;
    bool open = cyc > 150 || alive_parts(1, 4) == 0;
    b->part[0].vuln = open;
    b->p[0] = open ? 1 : 0;

    float spin = (float)b->t * 0.005f;
    for(int i = 1; i <= 4; i++) {
        float a = spin + (float)(i - 1) * 0.25f;
        b->part[i].x = b->x + fcos_t(a) * 52;
        b->part[i].y = b->y + fsin_t(a) * 40;
        if(b->part[i].alive && ((b->t + i * 17) % (int)(55 / brate())) == 0) {
            eshot_aimed(b->part[i].x, b->part[i].y, bspeed(2.4f), EB_S_BLUE, 0.03f);
        }
    }
    // satellites come back in later phases
    if(b->phase >= 2 && (b->t % 700) == 0) {
        for(int i = 1; i <= 4; i++) {
            if(!b->part[i].alive) {
                b->part[i].alive = true;
                b->part[i].hp = b->part[i].maxhp * 0.5f;
            }
        }
    }

    if(open && (cyc % 30) == 0) {
        eshot_ring(b->x, b->y, 12 + difficulty() * 4 + b->phase * 2, bspeed(1.8f), (float)b->t * 0.007f, EB_M_PINK);
    }
    if(b->phase >= 2 && (b->t % (int)(6 / brate() + 2)) == 0) {
        float a = (float)b->t * 0.011f;
        for(int k = 0; k < 4; k++) {
            eshot_t *s = eshot_fire(b->x, b->y, bspeed(1.9f), a + (float)k * 0.25f, EB_S_PINK);
            if(s && b->phase == 3) s->turn = 0.0015f;
        }
    }
    if(b->phase == 3 && g_pl.alive && !g_pl.entering) {
        // gravity well
        float dx = b->x - g_pl.x, dy = b->y - g_pl.y;
        float d = fsqrt(dx * dx + dy * dy) + 1;
        g_pl.x += dx / d * 0.45f;
        g_pl.y += dy / d * 0.45f;
    }
}

// ------------------------------------------------------------------ update & death

static void boss_die_step(void)
{
    boss_t *b = &g_boss;
    b->death_t++;
    bg_set_speed(0.4f);
    if(b->death_t < 120) {
        if((b->death_t % 6) == 0) {
            float ox = rndr(-40, 40), oy = rndr(-36, 36);
            fx_explode(b->x + ox, b->y + oy, 1 + rnd(2));
            g_hitstop = 1;
        }
        b->x += rndr(-1, 1);
        b->y += rndr(-1, 1);
    } else if(b->death_t == 120) {
        fx_explode(b->x, b->y, 3);
        fx_explode(b->x - 20, b->y + 10, 3);
        fx_flash(C_WHITE, 18);
        fx_shake(14);
        for(int k = 0; k < 3; k++) fx_ring(b->x, b->y, 10, 6.0f + (float)k * 2.5f, k == 1 ? C_PINK : C_CYAN, 30, 4);
        for(int k = 0; k < 30; k++) pickup_spawn(PU_GEM, b->x + rndr(-30, 30), b->y + rndr(-30, 30));
        uint32_t bonus = 20000u * (uint32_t)g_stage.num;
        add_score(bonus, b->x, b->y - 20, true);
    } else if(b->death_t > 150) {
        b->active = false;
        bg_set_speed(1.0f);
    }
}

void boss_update(void)
{
    boss_t *b = &g_boss;
    if(!b->active) return;
    if(b->flash) b->flash--;
    for(int i = 0; i < b->nparts; i++) {
        if(b->part[i].flash) b->part[i].flash--;
    }
    if(b->dying) {
        boss_die_step();
        return;
    }
    b->t++;
    if(!b->entered) {
        if(b->x < 290) b->entered = true;
    }
    switch(b->kind) {
        case 1: boss1(); break;
        case 2: boss2(); break;
        case 3: boss3(); break;
        case 4: boss4(); break;
        default: boss5(); break;
    }
    if(!b->entered) b->x -= 1.2f;

    // smoke when hurt
    if(b->hp < b->maxhp * 0.35f && (b->t % 7) == 0) {
        fx_smoke(b->x + rndr(-20, 20), b->y + rndr(-20, 20), -0.6f, -0.2f);
    }
}

// ------------------------------------------------------------------ drawing

static void draw_warden_hull(int x, int y)
{
    // armoured battleship, built from layered plates
    static const px_t plate[5] = { COL(34, 32, 48), COL(52, 48, 72), COL(76, 70, 102), COL(108, 100, 140), COL(150, 142, 186) };
    // main hull, stepped front
    for(int k = 0; k < 5; k++) {
        int inset = k * 6;
        gfx_fill(x - 46 + inset, y - 42 + inset, 110 - inset, 84 - inset * 2, plate[k]);
    }
    gfx_fill(x - 60, y - 16, 20, 32, plate[1]);
    gfx_fill(x - 60, y - 16, 20, 3, plate[3]);
    // trim lights
    for(int i = 0; i < 8; i++) {
        px_t c = ((g_game.frame >> 3) + i) & 1 ? COL(255, 170, 60) : COL(120, 60, 20);
        gfx_fill(x - 30 + i * 10, y - 44, 4, 2, c);
        gfx_fill(x - 30 + i * 10, y + 42, 4, 2, c);
    }
    gfx_rect(x - 46, y - 42, 110, 84, COL(12, 10, 20));
    // engines at the back
    for(int d = -24; d <= 24; d += 24) {
        gfx_glow(x + 66, y + d, 8 + (g_game.frame & 3), COL(255, 120, 40));
        gfx_fill(x + 60, y + d - 5, 6, 10, plate[0]);
    }
}

static void draw_turret_part(int i, int dx)
{
    boss_t *b = &g_boss;
    if(!b->part[i].alive) {
        gfx_disc((int)b->part[i].x + dx, (int)b->part[i].y + g_shake_y, 6, COL(30, 20, 20), DM_NORMAL);
        return;
    }
    int x = (int)b->part[i].x + dx, y = (int)b->part[i].y + g_shake_y;
    float a = aim_at_player(b->part[i].x, b->part[i].y);
    int bx = x + (int)(fcos_t(a) * 14), by = y + (int)(fsin_t(a) * 14);
    gfx_line(x, y, bx, by, COL(40, 36, 50), DM_NORMAL);
    gfx_line(x, y + 1, bx, by + 1, COL(190, 180, 210), DM_NORMAL);
    gfx_disc(x, y, 8, b->part[i].flash ? C_WHITE : COL(96, 90, 124), DM_NORMAL);
    gfx_disc(x - 2, y - 2, 4, b->part[i].flash ? C_WHITE : COL(150, 144, 186), DM_NORMAL);
    gfx_disc(x, y, 3, COL(255, 80, 60), DM_NORMAL);
}

void boss_draw(void)
{
    boss_t *b = &g_boss;
    if(!b->active) return;
    int sx = g_shake_x, sy = g_shake_y;
    int x = (int)b->x + sx, y = (int)b->y + sy;
    int t = b->t;
    bool fl = b->flash > 0;

    switch(b->kind) {
        case 1: {
            gfx_glow(x, y, 50, COL(20, 60, 110));
            gfx_sprite(&SPR_GLACIER, x, y, 0, NULL, fl ? DM_FLASH : DM_NORMAL, C_WHITE);
            int pr = 8 + ((isin256(t * 6) * 2) >> 14);
            gfx_glow(x - 2, y, pr * 2 + 6, COL(60, 190, 255));
            gfx_disc(x - 2, y, pr, fl ? C_WHITE : COL(150, 240, 255), DM_NORMAL);
            gfx_disc(x - 4, y - 2, pr / 2, C_WHITE, DM_NORMAL);
            for(int i = 1; i <= 4; i++) {
                if(!b->part[i].alive) continue;
                int ang = (int)((float)t * 1.2f) + i * 64;
                gfx_sprite_rot(&SPR_SHARD, (int)b->part[i].x + sx, (int)b->part[i].y + sy, ang, 256, NULL,
                               b->part[i].flash ? DM_FLASH : DM_NORMAL, C_WHITE);
            }
            break;
        }
        case 2: {
            for(int i = 10; i >= 1; i--) {
                int sc = 256 - i * 12;
                gfx_sprite_rot(&SPR_HYDRA_SEG, (int)b->part[i].x + sx, (int)b->part[i].y + sy, t * 2 + i * 20, sc, NULL, DM_NORMAL, 0);
            }
            gfx_glow(x, y, 22, COL(120, 20, 90));
            gfx_sprite(&SPR_HYDRA_HEAD, x, y, 0, NULL, fl ? DM_FLASH : DM_NORMAL, C_WHITE);
            if((t % 60) > 50) gfx_glow(x - 16, y + 2, 10, C_PINK);
            break;
        }
        case 3: {
            draw_warden_hull(x, y);
            // reactor
            px_t rc = b->part[0].vuln ? (fl ? C_WHITE : COL(90, 230, 255)) : COL(60, 60, 90);
            if(b->part[0].vuln) gfx_glow(x - 20, y, 24 + (t & 3), COL(40, 120, 200));
            gfx_disc(x - 20, y, 11, COL(20, 20, 30), DM_NORMAL);
            gfx_disc(x - 20, y, 8, rc, DM_NORMAL);
            if(!b->part[0].vuln) {
                gfx_fill(x - 32, y - 2, 24, 4, COL(90, 86, 120));
                gfx_fill(x - 22, y - 12, 4, 24, COL(90, 86, 120));
            }
            draw_turret_part(1, sx);
            draw_turret_part(2, sx);
            // missile bay
            if(b->part[3].alive) {
                int bx = (int)b->part[3].x + sx, by = (int)b->part[3].y + sy;
                gfx_fill(bx - 12, by - 8, 24, 16, b->part[3].flash ? C_WHITE : COL(70, 60, 90));
                for(int k = 0; k < 3; k++) gfx_fill(bx - 9 + k * 7, by - 5, 4, 10, COL(20, 16, 30));
                gfx_fill(bx - 12, by - 8, 24, 2, COL(255, 140, 50));
            }
            // main cannon
            if(b->part[4].alive) {
                int cx = (int)b->part[4].x + sx, cy = (int)b->part[4].y + sy;
                gfx_fill(cx - 22, cy - 6, 34, 12, b->part[4].flash ? C_WHITE : COL(100, 92, 130));
                gfx_fill(cx - 30, cy - 3, 10, 6, COL(60, 56, 80));
                if(s_beam_t > 0 && s_beam_t < 60) gfx_glow(cx - 30, cy, 4 + s_beam_t / 5, COL(255, 120, 60));
            }
            // beam
            if(s_beam_t > 0) {
                int by = (int)s_beam_y + sy;
                if(s_beam_t < 60) {
                    if((s_beam_t >> 2) & 1) gfx_hline(0, x - 50, by, COL(255, 80, 60));
                } else if(s_beam_t < 100) {
                    int w = s_beam_t < 70 ? (s_beam_t - 58) : (s_beam_t > 90 ? (100 - s_beam_t) : 10);
                    gfx_fill_mode(0, by - w - 4, x - 40, w * 2 + 8, COL(120, 30, 10), DM_ADD);
                    gfx_fill(0, by - w, x - 40, w * 2, COL(255, 200, 120));
                    gfx_fill(0, by - w / 2, x - 40, w, C_WHITE);
                }
            }
            break;
        }
        case 4: {
            int r = 30;
            gfx_glow(x, y, 70, COL(120, 50, 10));
            // flares
            for(int k = 0; k < 6; k++) {
                int a = t * 2 + k * 43;
                int len = 40 + ((isin256(t * 4 + k * 50) * 10) >> 14);
                int ex = x + (icos256(a) * len >> 14), ey = y + (isin256(a) * len >> 14);
                gfx_line(x, y, ex, ey, COL(255, 140, 40), DM_ADD);
                gfx_glow(ex, ey, 6, COL(255, 120, 30));
            }
            gfx_disc(x, y, r + 3, COL(255, 110, 20), DM_ADD);
            gfx_disc(x, y, r, fl ? C_WHITE : COL(255, 170, 60), DM_NORMAL);
            // boiling surface
            for(int k = 0; k < 18; k++) {
                int a = k * 37 + t * (k & 1 ? 1 : -1);
                int rr = (k * 7) % (r - 4);
                int px = x + (icos256(a) * rr >> 14), py = y + (isin256(a) * rr >> 14);
                gfx_disc(px, py, 3 + (k % 3), COL(255, 230, 140), DM_NORMAL);
            }
            gfx_glow(x - 8, y - 8, 14, COL(255, 255, 200));
            break;
        }
        default: {
            // accretion ring: far half behind the hole, near half in front
            for(int pass = 0; pass < 2; pass++) {
                for(int k = 0; k < 96; k++) {
                    int a = (k * 256) / 96 + t * 3;
                    int band = k % 3;
                    int rx = 44 + band * 5, ry = 11 + band;
                    int px = x + (icos256(a) * rx >> 14), py = y + (isin256(a) * ry >> 14);
                    bool front = isin256(a) >= 0;
                    if(front != (pass == 1)) continue;
                    px_t c = band == 0 ? COL(255, 200, 255) : (band == 1 ? COL(255, 120, 220) : COL(140, 110, 255));
                    gfx_fill_mode(px - 1, py, 3, 1, px_scale(c, 20), DM_ADD);
                    gfx_padd(px, py, c);
                }
                if(pass == 0) {
                    gfx_glow(x, y, 46, COL(90, 40, 140));
                    gfx_disc(x, y, 27, COL(255, 220, 255), DM_NORMAL);
                    gfx_disc(x, y, 25, C_BLACK, DM_NORMAL);
                    if(b->p[0] > 0) {
                        // almond shaped eye
                        for(int dy = -8; dy <= 8; dy++) {
                            int hw = 14 - (dy * dy * 14) / 64;
                            gfx_fill(x - hw, y + dy, hw * 2, 1, fl ? C_WHITE : COL(240, 220, 240));
                        }
                        int px = x - 2 + (int)((g_pl.x - b->x) * 0.03f);
                        int py = y + (int)((g_pl.y - b->y) * 0.03f);
                        gfx_disc(px, py, 6, COL(190, 20, 70), DM_NORMAL);
                        gfx_disc(px, py, 3, C_BLACK, DM_NORMAL);
                        gfx_pset(px - 2, py - 2, C_WHITE);
                    }
                }
            }
            for(int i = 1; i <= 4; i++) {
                if(!b->part[i].alive) continue;
                int px = (int)b->part[i].x + sx, py = (int)b->part[i].y + sy;
                gfx_glow(px, py, 12, COL(70, 40, 140));
                gfx_sprite(&SPR_SATELLITE, px, py, 0, NULL, b->part[i].flash ? DM_FLASH : DM_NORMAL, C_WHITE);
            }
            break;
        }
    }
}
