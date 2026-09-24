// QUASAR - the player ship, its weapons and the Nova bomb.
#include "game.h"
#include <string.h>

player_t g_pl;
pshot_t g_pshots[MAX_PSHOTS];

#define PL_SPEED    3.3f
#define CHARGE_MIN  10      // frames held before charging starts
#define CHARGE_L1   22
#define CHARGE_L2   42
#define CHARGE_MAX  62

void player_reset(bool new_game)
{
    player_t *p = &g_pl;
    if(new_game) {
        memset(p, 0, sizeof(*p));
        static const int lives[3] = { 4, 2, 1 };
        static const int hp[3] = { 4, 3, 2 };
        p->lives = lives[difficulty()];
        p->maxhp = hp[difficulty()];
        p->main_lvl = 1;
        p->sub = 0;
        p->sub_lvl = 0;
        p->options = 0;
    }
    p->hp = p->maxhp;
    p->bombs = difficulty() == 2 ? 2 : 3;
    p->x = -30;
    p->y = 128;
    p->alive = true;
    p->entering = true;
    p->invuln = 90;
    p->dead_timer = 0;
    p->charge = 0;
    p->fire_cd = 0;
    p->sub_cd = 0;
    p->bomb_timer = 0;
    for(int i = 0; i < 24; i++) {
        p->trail_x[i] = p->x;
        p->trail_y[i] = p->y;
    }
    for(int i = 0; i < 2; i++) {
        p->opt_x[i] = p->x;
        p->opt_y[i] = p->y;
    }
    memset(g_pshots, 0, sizeof(g_pshots));
}

bool player_can_be_hit(void)
{
    // only while flying: not under GAME OVER after a quit, nor during the stage tally
    return g_game.state == ST_PLAY && g_pl.alive && !g_pl.entering && g_pl.invuln <= 0 && g_pl.bomb_timer <= 0;
}

static pshot_t *pshot_new(int kind, float x, float y, float vx, float vy, float dmg)
{
    for(int i = 0; i < MAX_PSHOTS; i++) {
        pshot_t *s = &g_pshots[i];
        if(!s->alive) {
            memset(s, 0, sizeof(*s));
            s->alive = 1;
            s->kind = (uint8_t)kind;
            s->x = x;
            s->y = y;
            s->vx = vx;
            s->vy = vy;
            s->dmg = dmg;
            s->life = 90;
            s->target = -1;
            s->pierce = 1;
            return s;
        }
    }
    return NULL;
}

static void fire_main(float x, float y, int lvl, bool option)
{
    static const float offs[5][5] = {
        { 0, 0, 0, 0, 0 },
        { -3, 3, 0, 0, 0 },
        { 0, -4, 4, 0, 0 },
        { -2, 2, -6, 6, 0 },
        { 0, -3, 3, -7, 7 },
    };
    static const float angs[5][5] = {
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, -0.012f, 0.012f, 0, 0 },
        { 0, 0, -0.025f, 0.025f, 0 },
        { 0, -0.012f, 0.012f, -0.035f, 0.035f },
    };
    int n = option ? 1 : lvl;
    int row = option ? 0 : lvl - 1;
    for(int i = 0; i < n; i++) {
        float a = angs[row][i];
        pshot_t *s = pshot_new(option ? PS_OPTION : PS_PULSE, x + 12, y + offs[row][i],
                               fcos_t(a) * 10.5f, fsin_t(a) * 10.5f, option ? 0.8f : 1.0f);
        (void)s;
    }
}

static void fire_sub(void)
{
    player_t *p = &g_pl;
    switch(p->sub) {
        case 1: {       // spread
            static const float a[3] = { 0.06f, 0.12f, 0.18f };
            for(int i = 0; i < p->sub_lvl; i++) {
                pshot_new(PS_SPREAD, p->x + 6, p->y - 3, fcos_t(-a[i]) * 8.5f, fsin_t(-a[i]) * 8.5f, 0.9f);
                pshot_new(PS_SPREAD, p->x + 6, p->y + 3, fcos_t(a[i]) * 8.5f, fsin_t(a[i]) * 8.5f, 0.9f);
            }
            p->sub_cd = 9;
            break;
        }
        case 2: {       // laser
            pshot_t *s = pshot_new(PS_LASER, p->x + 14, p->y, 15.0f, 0, 1.6f + 0.6f * (float)p->sub_lvl);
            if(s) {
                s->pierce = (uint8_t)(2 + p->sub_lvl * 2);
                s->level = (uint8_t)p->sub_lvl;
            }
            p->sub_cd = 13 - p->sub_lvl;
            break;
        }
        case 3: {       // missiles
            int n = p->sub_lvl >= 2 ? 2 : 1;
            for(int i = 0; i < n; i++) {
                float dir = (i == 0) ? -1.0f : 1.0f;
                pshot_t *s = pshot_new(PS_MISSILE, p->x, p->y + dir * 6, 2.0f, dir * 2.5f, 3.5f);
                if(s) s->life = 80;
            }
            p->sub_cd = p->sub_lvl == 3 ? 12 : 17;
            break;
        }
        default:
            break;
    }
}

static void fire_beam(int level)
{
    player_t *p = &g_pl;
    pshot_t *s = pshot_new(PS_BEAM, p->x + 18, p->y, 9.0f, 0, level == 3 ? 2.2f : (level == 2 ? 1.5f : 1.0f));
    if(s) {
        s->level = (uint8_t)level;
        s->pierce = 255;
        s->life = 45;
    }
    fx_flashball(p->x + 18, p->y, 14 + level * 6, COL(120, 220, 255), 8);
    fx_ring(p->x + 18, p->y, 4, 3.0f, COL(140, 230, 255), 12, 2);
    fx_shake(level);
}

void player_bomb(void)
{
    player_t *p = &g_pl;
    if(p->bombs <= 0 || p->bomb_timer > 0 || !p->alive) return;
    p->bombs--;
    p->bomb_timer = 48;
    p->invuln = p->invuln > 90 ? p->invuln : 90;
    p->charge = 0;
    eshots_cancel(true);
    fx_flash(COL(160, 200, 255), 14);
    fx_shake(8);
    for(int i = 0; i < 4; i++) {
        fx_ring(p->x, p->y, 6.0f + (float)i * 4.0f, 7.0f + (float)i * 2.0f, i & 1 ? COL(255, 120, 230) : COL(120, 220, 255), 26 + i * 4, 5);
    }
    for(int i = 0; i < 40; i++) {
        float a = (float)i / 40.0f;
        float sp = rndfxr(4.0f, 9.0f);
        fx_spark(p->x, p->y, fcos_t(a) * sp, fsin_t(a) * sp, i & 1 ? C_CYAN : C_PINK, 20 + rndfx(10));
    }
}

void player_hit(void)
{
    player_t *p = &g_pl;
    if(!player_can_be_hit()) return;

    p->hp--;
    g_stage.damage_taken++;
    chain_break();
    p->charge = 0;
    fx_hitspark(p->x, p->y, C_WHITE);
    fx_flash(COL(255, 60, 60), 6);
    fx_shake(6);
    g_hitstop = 3;

    if(p->hp > 0) {
        p->invuln = 70;
        for(int i = 0; i < 12; i++) {
            float a = rndfxf();
            fx_spark(p->x, p->y, fcos_t(a) * 4.0f, fsin_t(a) * 4.0f, C_CYAN, 14);
        }
        return;
    }

    // ship destroyed
    fx_explode(p->x, p->y, 2);
    fx_ring(p->x, p->y, 4, 5.0f, C_CYAN, 20, 3);
    p->alive = false;
    p->dead_timer = 75;
    // lose some power, drop a little of it back into space
    if(p->main_lvl > 1) {
        p->main_lvl--;
        pickup_spawn(PU_POWER, p->x, p->y);
    }
    if(p->sub_lvl > 1) p->sub_lvl--;
    if(p->options > 0) p->options--;
    eshots_cancel(false);
}

static void player_die_step(void)
{
    player_t *p = &g_pl;
    if(--p->dead_timer > 0) return;

    if(p->lives > 0) {
        p->lives--;
        player_reset(false);
    } else {
        // no more ships
        game_set_state(ST_GAMEOVER);
    }
}

void player_update(void)
{
    player_t *p = &g_pl;

    if(!p->alive) {
        player_die_step();
        return;
    }

    if(p->invuln > 0) p->invuln--;
    if(p->bomb_timer > 0) {
        p->bomb_timer--;
        // the nova keeps hurting everything on screen
        if((p->bomb_timer & 3) == 0) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                enemy_t *e = &g_en[i];
                if(e->alive && e->x < SCR_W + 10) enemy_damage(e, 4.0f, e->x, e->y);
            }
            if(g_boss.active && !g_boss.dying) {
                for(int i = 0; i < g_boss.nparts; i++) {
                    if(g_boss.part[i].alive && g_boss.part[i].vuln) {
                        boss_hit_test(g_boss.part[i].x, g_boss.part[i].y, 1, 2.5f, g_boss.part[i].x, g_boss.part[i].y);
                        break;
                    }
                }
            }
        }
    }

    // trail for the option drones
    p->trail_i = (p->trail_i + 1) % 24;
    p->trail_x[p->trail_i] = p->x;
    p->trail_y[p->trail_i] = p->y;

    if(p->entering) {
        p->x += (60 - p->x) * 0.08f + 0.5f;
        p->y += (120 - p->y) * 0.08f;
        p->bank = 0;
        if(p->x > 56) p->entering = false;
        return;
    }

    float dx = 0, dy = 0;
    if(g_in.held & B_LEFT) dx -= 1;
    if(g_in.held & B_RIGHT) dx += 1;
    if(g_in.held & B_UP) dy -= 1;
    if(g_in.held & B_DOWN) dy += 1;
    float sp = PL_SPEED;
    if(dx != 0 && dy != 0) sp *= 0.7071f;
    if(p->charge > CHARGE_MIN) sp *= 0.8f;
    p->x += dx * sp;
    p->y += dy * sp;
    p->bank = (int)dy;

    int top = 20, bot = 232;
    int tt, tb;
    if(bg_terrain((int)p->x, &tt, &tb)) {
        // squeeze inside the corridor but never trap the ship
        if(tt + 8 > top) top = tt + 8;
        if(tb - 8 < bot) bot = tb - 8;
        if(top > bot - 12) {
            top = (tt + tb) / 2 - 6;
            bot = top + 12;
        }
    }
    p->x = fclampf(p->x, 16, 304);
    p->y = fclampf(p->y, (float)top, (float)bot);

    // guns: auto fire, paused while charging
    if(p->fire_cd > 0) p->fire_cd--;
    if(p->sub_cd > 0) p->sub_cd--;

    bool a_held = (g_in.held & B_A) != 0;
    if(a_held) {
        p->charge++;
        if(p->charge > CHARGE_MAX) p->charge = CHARGE_MAX;
        if(p->charge > CHARGE_MIN && (g_game.frame & 1)) {
            // particles rushing into the charge orb
            float a = rndfxf();
            float r = 22.0f;
            fx_spark(p->x + 18 + fcos_t(a) * r, p->y + fsin_t(a) * r, -fcos_t(a) * 2.2f, -fsin_t(a) * 2.2f,
                     p->charge >= CHARGE_L2 ? C_PINK : C_CYAN, 9);
        }
    } else {
        if(p->charge >= CHARGE_L1) {
            fire_beam(p->charge >= CHARGE_MAX ? 3 : (p->charge >= CHARGE_L2 ? 2 : 1));
        }
        p->charge = 0;
    }

    if(p->charge <= CHARGE_MIN) {
        if(p->fire_cd == 0) {
            fire_main(p->x, p->y, p->main_lvl, false);
            for(int i = 0; i < p->options; i++) fire_main(p->opt_x[i], p->opt_y[i], 1, true);
            p->fire_cd = 4;
        }
        if(p->sub && p->sub_cd == 0) fire_sub();
    }

    if(g_in.pressed & B_B) player_bomb();

    // option drones trail behind
    for(int i = 0; i < p->options; i++) {
        int k = (p->trail_i + 24 - (i + 1) * 9) % 24;
        p->opt_x[i] += (p->trail_x[k] - 6 - p->opt_x[i]) * 0.5f;
        p->opt_y[i] += (p->trail_y[k] + (i ? 10 : -10) - p->opt_y[i]) * 0.5f;
    }

    // touching enemies hurts
    if(player_can_be_hit()) {
        for(int i = 0; i < MAX_ENEMIES; i++) {
            enemy_t *e = &g_en[i];
            if(!e->alive || (e->flags & EF_NOHIT)) continue;
            float r = enemy_radius(e) * 0.75f + 3;
            float ddx = e->x - p->x, ddy = e->y - p->y;
            if(ddx * ddx + ddy * ddy < r * r) {
                player_hit();
                enemy_damage(e, 6, p->x, p->y);
                break;
            }
        }
        if(g_boss.active && !g_boss.dying && boss_touch_player(p->x, p->y, 4)) player_hit();
        int wt, wb;
        if(bg_terrain((int)p->x + 8, &wt, &wb)) {
            if(p->y - 3 < wt || p->y + 3 > wb) player_hit();
        }
    }
}

void player_draw(void)
{
    player_t *p = &g_pl;
    if(!p->alive) return;

    int sx = g_shake_x, sy = g_shake_y;
    int x = (int)p->x + sx, y = (int)p->y + sy;
    bool blink = p->invuln > 0 && !p->entering && ((g_game.frame >> 1) & 1);

    // engine flames
    int fl = 3 + ((g_game.frame * 7) & 3);
    for(int d = -2; d <= 2; d += 4) {
        gfx_glow(x - 18, y + d, fl + 2, COL(255, 120, 30));
        gfx_hline(x - 18 - fl * 2, x - 17, y + d, COL(255, 220, 120));
        gfx_padd(x - 19, y + d, C_WHITE);
    }

    // options
    for(int i = 0; i < p->options; i++) {
        int ox = (int)p->opt_x[i] + sx, oy = (int)p->opt_y[i] + sy;
        gfx_glow(ox, oy, 9, COL(255, 90, 40));
        gfx_disc(ox, oy, 3, COL(255, 200, 120), DM_NORMAL);
        gfx_pset(ox - 1, oy - 1, C_WHITE);
        int a = (g_game.frame * 12 + i * 128) & 255;
        gfx_padd(ox + (icos256(a) * 6 >> 14), oy + (isin256(a) * 6 >> 14), C_GOLD);
    }

    if(!blink) {
        const sprite_t *s = p->bank < 0 ? &SPR_PLAYER_UP : (p->bank > 0 ? &SPR_PLAYER_DN : &SPR_PLAYER);
        gfx_sprite(s, x, y, 0, NULL, DM_NORMAL, 0);
        if(p->invuln > 60 && !p->entering && (g_game.frame & 2)) {
            gfx_sprite(s, x, y, 0, NULL, DM_FLASH, C_WHITE);
        }
    }

    // nova shield
    if(p->bomb_timer > 0) {
        int r = 20 + (48 - p->bomb_timer) / 3;
        gfx_ring(x, y, r - 2, r, COL(90, 160, 255), DM_ADD);
    }

    // charge orb
    if(p->charge > CHARGE_MIN) {
        int c = p->charge;
        int r = 2 + (c - CHARGE_MIN) / 6;
        px_t col = c >= CHARGE_MAX ? ((g_game.frame & 2) ? C_WHITE : C_PINK) : (c >= CHARGE_L2 ? C_PINK : C_CYAN);
        gfx_glow(x + 19, y, r * 2 + 3, px_scale(col, 22));
        gfx_disc(x + 19, y, r / 2 + 1, C_WHITE, DM_NORMAL);
        if(c >= CHARGE_L1) gfx_ring(x + 19, y, r + 2, r + 3, col, DM_ADD);
    }
}

// ------------------------------------------------------------------ shots

static int find_target(float x, float y)
{
    int best = -1;
    float bd = 1e9f;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive || (e->flags & EF_NOHIT) || e->x < x - 20 || e->x > SCR_W + 8) continue;
        float dx = e->x - x, dy = e->y - y;
        float d = dx * dx + dy * dy;
        if(d < bd) {
            bd = d;
            best = i;
        }
    }
    if(best < 0 && g_boss.active && !g_boss.dying) best = 1000;
    return best;
}

static bool shot_hits_enemies(pshot_t *s, float hw, float hh)
{
    // returns true if the shot should die
    for(int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &g_en[i];
        if(!e->alive || (e->flags & EF_NOHIT)) continue;
        float r = enemy_radius(e);
        if(s->x + hw < e->x - r || s->x - hw > e->x + r) continue;
        if(s->y + hh < e->y - r || s->y - hh > e->y + r) continue;
        enemy_damage(e, s->dmg, s->x, s->y);
        if(s->kind == PS_BEAM) continue;
        if(s->pierce > 1) {
            s->pierce--;
            continue;
        }
        return true;
    }
    if(g_boss.active && !g_boss.dying) {
        float r = hw > hh ? hw : hh;
        if(boss_hit_test(s->x, s->y, r, s->dmg, s->x, s->y)) {
            if(s->kind == PS_BEAM) return false;
            return true;
        }
    }
    return false;
}

void pshots_update(void)
{
    for(int i = 0; i < MAX_PSHOTS; i++) {
        pshot_t *s = &g_pshots[i];
        if(!s->alive) continue;

        if(s->kind == PS_MISSILE) {
            if(s->target < 0 || (s->target < MAX_ENEMIES && !g_en[s->target].alive) ||
               (s->target == 1000 && (!g_boss.active || g_boss.dying))) {
                s->target = (int16_t)find_target(s->x, s->y);
            }
            float tx = s->x + 60, ty = s->y;
            if(s->target >= 0 && s->target < MAX_ENEMIES) {
                tx = g_en[s->target].x;
                ty = g_en[s->target].y;
            } else if(s->target == 1000) {
                tx = g_boss.x;
                ty = g_boss.y;
                for(int k = 0; k < g_boss.nparts; k++) {
                    if(g_boss.part[k].alive && g_boss.part[k].vuln) {
                        tx = g_boss.part[k].x;
                        ty = g_boss.part[k].y;
                        break;
                    }
                }
            }
            float cur = fatan2_t(s->vy, s->vx);
            float want = fatan2_t(ty - s->y, tx - s->x);
            float d = want - cur;
            while(d > 0.5f) d -= 1.0f;
            while(d < -0.5f) d += 1.0f;
            float turn = 0.035f;
            if(d > turn) d = turn;
            if(d < -turn) d = -turn;
            float spd = fsqrt(s->vx * s->vx + s->vy * s->vy);
            if(spd < 9.0f) spd += 0.45f;
            cur += d;
            s->vx = fcos_t(cur) * spd;
            s->vy = fsin_t(cur) * spd;
            if(g_game.frame & 1) fx_ember(s->x - s->vx, s->y - s->vy, 0, 0, COL(255, 140, 60), 8, 1);
        }

        s->x += s->vx;
        s->y += s->vy;
        if(--s->life <= 0 || s->x > SCR_W + 30 || s->x < -30 || s->y < -20 || s->y > SCR_H + 20) {
            s->alive = 0;
            continue;
        }

        float hw = 5, hh = 2;
        switch(s->kind) {
            case PS_LASER:   hw = 18; hh = 2; break;
            case PS_MISSILE: hw = 4; hh = 3; break;
            case PS_BEAM:    hw = 14; hh = (float)(8 + s->level * 6); break;
            case PS_SPREAD:  hw = 4; hh = 2; break;
            default: break;
        }

        // stage 3 walls stop ordinary shots
        int tt, tb;
        if(s->kind != PS_BEAM && bg_terrain((int)s->x, &tt, &tb)) {
            if(s->y < tt || s->y > tb) {
                fx_hitspark(s->x, s->y < tt ? (float)tt : (float)tb, C_CYAN);
                s->alive = 0;
                continue;
            }
        }

        if(shot_hits_enemies(s, hw, hh)) {
            fx_hitspark(s->x + hw, s->y, s->kind == PS_SPREAD ? C_PINK : C_CYAN);
            s->alive = 0;
            continue;
        }

        if(s->kind == PS_BEAM) {
            // the beam also erases bullets it sweeps through
            for(int k = 0; k < MAX_ESHOTS; k++) {
                eshot_t *b = &g_eshots[k];
                if(!b->alive) continue;
                if(b->x > s->x - hw && b->x < s->x + hw && b->y > s->y - hh && b->y < s->y + hh) {
                    b->alive = 0;
                    fx_ember(b->x, b->y, 1.0f, 0, C_CYAN, 10, 2);
                }
            }
        }
    }
}

void pshots_draw(void)
{
    int sx = g_shake_x, sy = g_shake_y;
    for(int i = 0; i < MAX_PSHOTS; i++) {
        pshot_t *s = &g_pshots[i];
        if(!s->alive) continue;
        int x = (int)s->x + sx, y = (int)s->y + sy;
        switch(s->kind) {
            case PS_PULSE:
            case PS_OPTION:
                gfx_sprite(&SPR_SHOT_PULSE, x, y, 0, NULL, DM_NORMAL, 0);
                break;
            case PS_SPREAD: {
                int a = (int)(fatan2_t(s->vy, s->vx) * 256.0f);
                gfx_sprite_rot(&SPR_SHOT_SPREAD, x, y, a, 256, NULL, DM_NORMAL, 0);
                break;
            }
            case PS_LASER: {
                px_t c1 = s->level >= 3 ? COL(120, 255, 160) : COL(80, 230, 140);
                gfx_fill(x - 18, y - 1, 36, 3, c1);
                gfx_fill(x - 16, y, 34, 1, C_WHITE);
                gfx_fill_mode(x - 20, y - 3, 40, 7, COL(20, 90, 40), DM_ADD);
                break;
            }
            case PS_MISSILE: {
                int a = (int)(fatan2_t(s->vy, s->vx) * 256.0f);
                gfx_sprite_rot(&SPR_MISSILE, x, y, a, 256, NULL, DM_NORMAL, 0);
                break;
            }
            case PS_BEAM: {
                int h = 8 + s->level * 6;
                int ph = g_game.frame * 3;
                gfx_glow(x, y, h + 8, COL(40, 110, 200));
                for(int k = -h; k <= h; k += 2) {
                    int w = 14 - (iabs(k) * 10) / (h ? h : 1);
                    int wob = (isin256(ph + k * 9) * 3) >> 14;
                    gfx_fill_mode(x - w + wob, y + k, w * 2, 2, COL(60, 160, 255), DM_ADD);
                }
                gfx_fill(x - 10, y - 2, 22, 5, COL(200, 240, 255));
                gfx_fill(x - 12, y - 1, 26, 3, C_WHITE);
                if(s->level >= 2) gfx_ring(x - 4, y, h - 2, h, COL(255, 120, 230), DM_ADD);
                break;
            }
            default:
                break;
        }
    }
}
