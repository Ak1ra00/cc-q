// QUASAR - in-game heads up display.
#include "game.h"

static const px_t MULT_COL[9] = {
    0, COL(170, 170, 200), COL(120, 220, 255), COL(120, 255, 170), COL(255, 230, 90),
    COL(255, 160, 60), COL(255, 90, 90), COL(255, 90, 220), COL(255, 255, 255),
};

static void battery_icon(int x, int y, int level)
{
    gfx_rect(x, y, 14, 7, C_GREY);
    gfx_fill(x + 14, y + 2, 2, 3, C_GREY);
    px_t c = level <= 0 ? ((g_game.frame & 8) ? C_RED : COL(90, 20, 20)) : (level == 1 ? C_ORANGE : C_GREEN);
    int w = level <= 0 ? 2 : level * 4;
    gfx_fill(x + 1, y + 1, w, 5, c);
}

void hud_boss_bar(void)
{
    boss_t *b = &g_boss;
    if(!b->active || !b->entered) return;
    int x = 60, y = 16, w = 200;
    float f = b->maxhp > 0 ? b->hp / b->maxhp : 0;
    int fill = (int)((float)(w - 2) * f);
    gfx_fill_mode(x - 2, y - 1, w + 4, 10, 0, DM_SHADOW);
    gfx_rect(x, y, w, 8, COL(90, 60, 110));
    gfx_hgrad(x + 1, y + 1, fill, 6, COL(255, 60, 90), COL(255, 180, 60));
    if(b->flash) gfx_fill_mode(x + 1, y + 1, fill, 6, COL(80, 80, 80), DM_ADD);
    text_center_x(160, y + 10, b->name, COL(255, 180, 200), 1, TX_SHADOW);
}

void hud_draw(void)
{
    char buf[24];
    player_t *p = &g_pl;

    // top strip
    gfx_fill_mode(0, 0, SCR_W, 12, 0, DM_SHADOW);
    gfx_hline(0, SCR_W - 1, 12, COL(40, 40, 70));

    fmt_score(buf, g_run.score, 8);
    text_draw(3, 2, "SC", C_DIM, 1);
    text_draw(17, 2, buf, C_WHITE, 1);

    fmt_score(buf, g_run.hiscore > g_run.score ? g_run.hiscore : g_run.score, 8);
    text_right(SCR_W - 3, 2, buf, C_GOLD, 1, 0);
    text_right(SCR_W - 3 - 50, 2, "HI", C_DIM, 1, 0);

    // chain multiplier with its countdown bar
    int m = g_run.mult > 8 ? 8 : g_run.mult;
    if(m < 1) m = 1;
    buf[0] = 'x';
    fmt_int(buf + 1, g_run.mult);
    text_center_x(160, 2, buf, MULT_COL[m], 1, 0);
    if(g_run.chain > 1) {
        int w = (g_run.chain_timer * 36) / 45;
        gfx_fill(160 - 18, 10, w, 1, MULT_COL[m]);
    }

    // bottom left: ships and hull
    int y = SCR_H - 10;
    int x = 3;
    for(int i = 0; i < p->lives && i < 6; i++) {
        gfx_sprite(&SPR_ICON_SHIP, x, y + 1, 0, NULL, DM_NORMAL, 0);
        x += 10;
    }
    if(p->lives > 6) {
        buf[0] = '+';
        fmt_int(buf + 1, p->lives - 6);
        text_draw(x, y, buf, C_WHITE, 1);
        x += 16;
    }
    x += 4;
    for(int i = 0; i < p->maxhp; i++) {
        px_t c = i < p->hp ? C_RED : COL(60, 30, 40);
        if(i < p->hp && p->hp == 1 && (g_game.frame & 8)) c = C_WHITE;
        text_draw_fx(x, y, G_HEART, c, 1, TX_SHADOW, NULL);
        x += 7;
    }

    // bottom right: bombs and weapons
    x = SCR_W - 4;
    for(int i = 0; i < p->bombs && i < 6; i++) {
        x -= 9;
        gfx_sprite(&SPR_ICON_BOMB, x, y, 0, NULL, DM_NORMAL, 0);
    }
    x -= 8;
    // sub weapon
    if(p->sub) {
        static const char *const SUB[4] = { "", "S", "L", "M" };
        static const px_t SUBC[4] = { 0, COL(255, 90, 110), COL(90, 255, 150), COL(200, 130, 255) };
        for(int i = 0; i < 3; i++) gfx_fill(x - 2, y + 7 - i * 3, 3, 2, i < p->sub_lvl ? SUBC[p->sub] : COL(50, 50, 70));
        x -= 8;
        text_draw_fx(x, y, SUB[p->sub], SUBC[p->sub], 1, TX_SHADOW, NULL);
        x -= 8;
    }
    // main gun level
    for(int i = 4; i >= 0; i--) {
        x -= 4;
        gfx_fill(x, y + 7 - i, 3, i + 2, i < p->main_lvl ? C_CYAN : COL(40, 50, 70));
    }
    text_right(x - 2, y, "P", C_CYAN, 1, 0);

    if(g_game.battery == 0) battery_icon(SCR_W / 2 - 8, SCR_H - 10, 0);

    if(g_save.show_fps) {
        fmt_int(buf, g_game.fps10 / 10);
        str_cat(buf, " FPS");
        text_draw(3, 16, buf, C_GREEN, 1);
    }

    hud_boss_bar();
}
