//
// modquasar.c - QUASAR on the Coldcard Q: keyboard matrix, LCD streaming and
// the MicroPython bindings for the games (the home screen, QUASAR and TETRIS,
// all run by arcade.c) and the system screens.
//
// The frame buffer layout (ten 32px vertical strips) lets each strip go out as one
// DMA transfer. Strips are sent in the direction the panel refreshes, starting on
// the LCD's tearing-effect pulse, so a whole frame lands without a visible tear.
// A second buffer is rendered while the first one is being sent.
//
// Two ways to reach the panel:
//  - fast: interrupt-chained DMA per strip, used while playing.
//  - safe: plain blocking writes, same calls as the stock Coldcard display code;
//          used by every system screen (firmware reinstall, errors), and as an
//          automatic fallback if the fast path ever reports an error.
//
// The LCD primitives mirror modlcd.c from the Coldcard firmware, (c) Coinkite
// Inc.; covered by COPYING-CC.
//
#include <string.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "irq.h"
#include "spi.h"
#include "dma.h"
#include "pin.h"
#include "rng.h"

#include "game.h"
#include "arcade.h"

#define PIN_LCD_TEAR        pin_B11
#define PIN_LCD_CS          pin_A4
#define PIN_LCD_DATA_CMD    pin_A8
#define PIN_PWR_BTN         pin_B12

#define CASET   0x2a
#define RASET   0x2b
#define RAMWR   0x2c
#define TEON    0x35

// ------------------------------------------------------------------ state

static px_t s_fb[2][FB_PIX];
static int s_back;                      // buffer being drawn
static const spi_t *s_spi;
static bool s_ready;

static DMA_HandleTypeDef s_dma;
static bool s_dma_inited;
static volatile int s_strip = -1;       // next strip index, -1 when idle
static volatile bool s_dma_err;
static const px_t *s_send_fb;
static int s_dir;                       // 0: strips left to right, 1: right to left
static bool s_fast = true;              // may be turned off after an error
static int s_vsync;                     // 0 L->R, 1 R->L, 2 no sync
static uint32_t s_frame_ms;             // when the last frame started going out

uint32_t plat_millis(void)
{
    return mp_hal_ticks_ms();
}

// ------------------------------------------------------------------ keyboard

static const pin_obj_t *const ROWS[6] = { pin_D8, pin_D9, pin_D10, pin_D11, pin_D12, pin_D7 };
static const pin_obj_t *const COLS[10] = { pin_B0, pin_B1, pin_B2, pin_B5, pin_B8, pin_B9, pin_B10,
                                          pin_D13, pin_D14, pin_D15 };

static void keys_setup(void)
{
    for(int r = 0; r < 6; r++) {
        mp_hal_pin_high(ROWS[r]);               // released
        mp_hal_pin_config(ROWS[r], MP_HAL_PIN_MODE_OPEN_DRAIN, MP_HAL_PIN_PULL_NONE, 0);
    }
    for(int c = 0; c < 10; c++) {
        mp_hal_pin_config(COLS[c], MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_UP, 0);
    }
    mp_hal_pin_config(PIN_PWR_BTN, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_UP, 0);
    mp_hal_pin_config(PIN_LCD_TEAR, MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_NONE, 0);
}

static uint64_t keys_scan(void)
{
    uint64_t m = 0;
    for(int r = 0; r < 6; r++) {
        mp_hal_pin_low(ROWS[r]);
        mp_hal_delay_us(10);
        for(int c = 0; c < 10; c++) {
            if(!mp_hal_pin_read(COLS[c])) m |= (uint64_t)1 << (r * 10 + c);
        }
        mp_hal_pin_high(ROWS[r]);
    }
    if(!mp_hal_pin_read(PIN_PWR_BTN)) m |= (uint64_t)1 << K_POWER;
    return m;
}

// ------------------------------------------------------------------ lcd primitives
// These mirror modlcd.c from the stock firmware.

static void lcd_cmd(uint8_t cmd)
{
    mp_hal_pin_write(PIN_LCD_CS, 1);
    mp_hal_pin_write(PIN_LCD_DATA_CMD, 0);
    mp_hal_pin_write(PIN_LCD_CS, 0);
    HAL_SPI_Transmit(s_spi->spi, &cmd, 1, SPI_TRANSFER_TIMEOUT(1));
    mp_hal_pin_write(PIN_LCD_CS, 1);
}

static void lcd_cmd_args(uint8_t cmd, const uint8_t *args, int n)
{
    mp_hal_pin_write(PIN_LCD_CS, 1);
    mp_hal_pin_write(PIN_LCD_DATA_CMD, 0);
    mp_hal_pin_write(PIN_LCD_CS, 0);
    HAL_SPI_Transmit(s_spi->spi, &cmd, 1, SPI_TRANSFER_TIMEOUT(1));
    if(n) {
        mp_hal_pin_write(PIN_LCD_DATA_CMD, 1);
        HAL_SPI_Transmit(s_spi->spi, (uint8_t *)args, (uint16_t)n, SPI_TRANSFER_TIMEOUT(n));
    }
    mp_hal_pin_write(PIN_LCD_CS, 1);
}

static void set_window(int x, int y, int w, int h)
{
    uint8_t a[4];
    a[0] = (uint8_t)(x >> 8);
    a[1] = (uint8_t)x;
    a[2] = (uint8_t)((x + w - 1) >> 8);
    a[3] = (uint8_t)(x + w - 1);
    lcd_cmd_args(CASET, a, 4);
    a[0] = (uint8_t)(y >> 8);
    a[1] = (uint8_t)y;
    a[2] = (uint8_t)((y + h - 1) >> 8);
    a[3] = (uint8_t)(y + h - 1);
    lcd_cmd_args(RASET, a, 4);
    lcd_cmd(RAMWR);
}

static void begin_data(void)
{
    mp_hal_pin_write(PIN_LCD_CS, 1);
    mp_hal_pin_write(PIN_LCD_DATA_CMD, 1);
    mp_hal_pin_write(PIN_LCD_CS, 0);
}

// ------------------------------------------------------------------ fast path

static void start_strip(int i)
{
    int s = s_dir ? (NUM_STRIPS - 1 - i) : i;
    set_window(s * STRIP_W, 0, STRIP_W, SCR_H);
    begin_data();
    if(HAL_SPI_Transmit_DMA(s_spi->spi, (uint8_t *)(s_send_fb + s * STRIP_PIX), STRIP_PIX * 2) != HAL_OK) {
        mp_hal_pin_write(PIN_LCD_CS, 1);
        s_dma_err = true;
        s_strip = -1;
    }
}

// Called by the HAL from the DMA interrupt when a strip has fully left the SPI.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(!s_spi || hspi != s_spi->spi || s_strip < 0) return;
    mp_hal_pin_write(PIN_LCD_CS, 1);
    int next = s_strip + 1;
    if(next < NUM_STRIPS) {
        s_strip = next;
        start_strip(next);
    } else {
        s_strip = -1;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if(!s_spi || hspi != s_spi->spi) return;
    mp_hal_pin_write(PIN_LCD_CS, 1);
    s_dma_err = true;
    s_strip = -1;
}

// Wait for an in-flight frame. Leaves the SPI idle and the DMA released.
static void dma_wait(void)
{
    uint32_t t0 = mp_hal_ticks_ms();
    while(s_strip >= 0) {
        if(mp_hal_ticks_ms() - t0 > 120) {
            // should never happen; give up on the fast path for good
            HAL_SPI_Abort(s_spi->spi);
            mp_hal_pin_write(PIN_LCD_CS, 1);
            s_strip = -1;
            s_dma_err = true;
            break;
        }
    }
    if(s_dma_inited) {
        dma_deinit(s_spi->tx_dma_descr);
        s_spi->spi->hdmatx = NULL;
        s_dma_inited = false;
    }
    if(s_dma_err) s_fast = false;
}

static void wait_te(void)
{
    // rising edge of the tearing-effect pulse = start of vertical blanking
    uint32_t t0 = mp_hal_ticks_ms();
    while(mp_hal_pin_read(PIN_LCD_TEAR)) {
        if(mp_hal_ticks_ms() - t0 > 40) return;
    }
    while(!mp_hal_pin_read(PIN_LCD_TEAR)) {
        if(mp_hal_ticks_ms() - t0 > 40) return;
    }
}

static void present_safe(const px_t *fb)
{
    dma_wait();
    for(int s = 0; s < NUM_STRIPS; s++) {
        set_window(s * STRIP_W, 0, STRIP_W, SCR_H);
        begin_data();
        spi_transfer(s_spi, STRIP_PIX * 2, (const uint8_t *)(fb + s * STRIP_PIX), NULL,
                     SPI_TRANSFER_TIMEOUT(STRIP_PIX * 2));
        mp_hal_pin_write(PIN_LCD_CS, 1);
    }
}

static void present_fast(const px_t *fb)
{
    dma_wait();
    if(!s_fast) {
        present_safe(fb);
        return;
    }
    if(s_vsync != 2) wait_te();

    s_send_fb = fb;
    s_dir = (s_vsync == 1);
    s_dma_err = false;
    dma_init(&s_dma, s_spi->tx_dma_descr, DMA_MEMORY_TO_PERIPH, s_spi->spi);
    s_spi->spi->hdmatx = &s_dma;
    s_spi->spi->hdmarx = NULL;
    s_dma_inited = true;
    s_strip = 0;
    start_strip(0);
}

// ------------------------------------------------------------------ bindings

static void check_ready(void)
{
    if(!s_ready) mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("quasar.init first"));
}

// init(spi): take the display over and set up the game
STATIC mp_obj_t q_init(mp_obj_t spi_in)
{
    s_spi = spi_from_mp_obj(spi_in);
    keys_setup();
    // tearing-effect output on (vblank only); harmless if already on
    uint8_t zero = 0;
    lcd_cmd_args(TEON, &zero, 1);
    s_back = 0;
    gfx_set_target(s_fb[s_back]);
    if(!s_ready) arcade_init(rng_get() ^ mp_hal_ticks_ms());
    s_ready = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_init_obj, q_init);

// run(max_ms): play frames until a game raises an event or time is up
STATIC mp_obj_t q_run(mp_obj_t ms_in)
{
    check_ready();
    uint32_t budget = (uint32_t)mp_obj_get_int(ms_in);
    uint32_t t0 = mp_hal_ticks_ms();
    uint32_t ev;
    do {
        uint64_t keys = keys_scan();
        gfx_set_target(s_fb[s_back]);
        ev = arcade_frame(keys);
        // With tearing sync a frame (~21ms of SPI) lands on every other 60Hz
        // pulse, which is the game's 30fps. Without it, hold frames to 33ms.
        if(s_vsync == 2) {
            while(mp_hal_ticks_ms() - s_frame_ms < 33) {
            }
        }
        s_frame_ms = mp_hal_ticks_ms();
        present_fast(s_fb[s_back]);
        s_back ^= 1;
        if(arcade_wants_idle_off()) ev |= 0x100;
    } while(!ev && (mp_hal_ticks_ms() - t0) < budget);
    dma_wait();
    return mp_obj_new_int_from_uint(ev);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_run_obj, q_run);

STATIC mp_obj_t q_keys(void)
{
    uint64_t k = keys_scan();
    return mp_obj_new_int_from_ull(k);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(q_keys_obj, q_keys);

// getkey(timeout_ms): next newly pressed key number (0..63), or -1
static uint64_t s_last_keys;
STATIC mp_obj_t q_getkey(mp_obj_t ms_in)
{
    check_ready();
    int32_t budget = mp_obj_get_int(ms_in);
    uint32_t t0 = mp_hal_ticks_ms();
    for(;;) {
        uint64_t a = keys_scan();
        mp_hal_delay_ms(6);
        uint64_t b = keys_scan();
        uint64_t stable = a & b;
        uint64_t fresh = stable & ~s_last_keys;
        // only forget keys that are really up
        s_last_keys = (s_last_keys & (a | b)) | stable;
        if(fresh) {
            for(int k = 0; k < 64; k++) {
                if(fresh & ((uint64_t)1 << k)) return MP_OBJ_NEW_SMALL_INT(k);
            }
        }
        if(budget >= 0 && (int32_t)(mp_hal_ticks_ms() - t0) >= budget) break;
    }
    return MP_OBJ_NEW_SMALL_INT(-1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_getkey_obj, q_getkey);

STATIC mp_obj_t q_held(mp_obj_t key_in)
{
    int k = mp_obj_get_int(key_in);
    uint64_t m = keys_scan();
    return mp_obj_new_bool(k >= 0 && k < 64 && (m & ((uint64_t)1 << k)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_held_obj, q_held);

// show(): blocking present of the drawing buffer (system screens)
STATIC mp_obj_t q_show(void)
{
    check_ready();
    present_safe(s_fb[s_back]);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(q_show_obj, q_show);

STATIC mp_obj_t q_save_blob(void)
{
    uint8_t buf[256];
    int n = save_pack(buf, sizeof(buf));
    return mp_obj_new_bytes(buf, n);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(q_save_blob_obj, q_save_blob);

STATIC mp_obj_t q_load_blob(mp_obj_t b_in)
{
    mp_buffer_info_t bi;
    mp_get_buffer_raise(b_in, &bi, MP_BUFFER_READ);
    bool ok = save_unpack(bi.buf, (int)bi.len);
    if(ok) screens_save_loaded();
    return mp_obj_new_bool(ok);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_load_blob_obj, q_load_blob);

// the home screen's, TETRIS's and PAC-MAN's own save (arcade.sav), apart from QUASAR's
STATIC mp_obj_t q_arcade_blob(void)
{
    static uint8_t buf[1024];
    int n = arcade_save_pack(buf, sizeof(buf));
    return mp_obj_new_bytes(buf, n);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(q_arcade_blob_obj, q_arcade_blob);

STATIC mp_obj_t q_arcade_load(mp_obj_t b_in)
{
    mp_buffer_info_t bi;
    mp_get_buffer_raise(b_in, &bi, MP_BUFFER_READ);
    bool ok = arcade_save_unpack(bi.buf, (int)bi.len);
    if(ok) arcade_loaded();
    return mp_obj_new_bool(ok);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_arcade_load_obj, q_arcade_load);

STATIC mp_obj_t q_battery(mp_obj_t lv)
{
    game_set_battery(mp_obj_get_int(lv));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_battery_obj, q_battery);

STATIC mp_obj_t q_settings(void)
{
    s_vsync = game_vsync();
    mp_obj_t t[3] = { MP_OBJ_NEW_SMALL_INT(game_brightness()), MP_OBJ_NEW_SMALL_INT(s_vsync),
                      mp_obj_new_bool(s_fast) };
    return mp_obj_new_tuple(3, t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(q_settings_obj, q_settings);

STATIC mp_obj_t q_fast(mp_obj_t en)
{
    dma_wait();
    s_fast = mp_obj_is_true(en);
    s_dma_err = false;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_fast_obj, q_fast);

// ---- drawing for the system screens (all colours are 0xRRGGBB)

static px_t rgb(mp_obj_t o)
{
    uint32_t v = (uint32_t)mp_obj_get_int(o);
    return COL((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}

STATIC mp_obj_t q_ui_clear(mp_obj_t style_in)
{
    check_ready();
    gfx_set_target(s_fb[s_back]);
    gfx_noclip();
    int style = mp_obj_get_int(style_in);
    if(style == 1) {
        gfx_vgrad(0, 0, SCR_W, SCR_H, COL(34, 4, 10), COL(8, 2, 6));        // warning red
    } else {
        gfx_vgrad(0, 0, SCR_W, SCR_H, COL(6, 8, 30), COL(2, 2, 10));
    }
    for(int i = 0; i < 70; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        gfx_pset((int)(h % SCR_W), (int)((h >> 12) % SCR_H), COL(70, 80, 130));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_ui_clear_obj, q_ui_clear);

// text(x, y, s, colour, scale=1, flags=0) -> width
STATIC mp_obj_t q_ui_text(size_t n_args, const mp_obj_t *args)
{
    int x = mp_obj_get_int(args[0]);
    int y = mp_obj_get_int(args[1]);
    const char *s = mp_obj_str_get_str(args[2]);
    px_t c = rgb(args[3]);
    int scale = n_args > 4 ? mp_obj_get_int(args[4]) : 1;
    int flags = n_args > 5 ? mp_obj_get_int(args[5]) : 0;
    if(x < 0) {
        // negative x centres the text on the screen
        int w = text_width(s, scale);
        x = (SCR_W - w) / 2;
    }
    return MP_OBJ_NEW_SMALL_INT(text_draw_fx(x, y, s, c, scale, flags, NULL));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(q_ui_text_obj, 4, 6, q_ui_text);

STATIC mp_obj_t q_ui_width(mp_obj_t s_in, mp_obj_t sc_in)
{
    return MP_OBJ_NEW_SMALL_INT(text_width(mp_obj_str_get_str(s_in), mp_obj_get_int(sc_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(q_ui_width_obj, q_ui_width);

// rect(x, y, w, h, colour, mode) mode: 0 fill, 1 outline, 2 half-mix, 3 darken
STATIC mp_obj_t q_ui_rect(size_t n_args, const mp_obj_t *args)
{
    int x = mp_obj_get_int(args[0]), y = mp_obj_get_int(args[1]);
    int w = mp_obj_get_int(args[2]), h = mp_obj_get_int(args[3]);
    px_t c = rgb(args[4]);
    int mode = n_args > 5 ? mp_obj_get_int(args[5]) : 0;
    switch(mode) {
        case 1: gfx_rect(x, y, w, h, c); break;
        case 2: gfx_fill_mode(x, y, w, h, c, DM_HALF); break;
        case 3: gfx_fill_mode(x, y, w, h, c, DM_SHADOW); break;
        default: gfx_fill(x, y, w, h, c); break;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(q_ui_rect_obj, 5, 6, q_ui_rect);

STATIC mp_obj_t q_ui_logo(mp_obj_t y_in)
{
    int y = mp_obj_get_int(y_in);
    gfx_glow(SCR_W / 2, y, 70, COL(40, 12, 60));
    gfx_sprite(&SPR_LOGO, SCR_W / 2, y, 0, NULL, DM_NORMAL, 0);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(q_ui_logo_obj, q_ui_logo);

STATIC const mp_rom_map_elem_t quasar_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_quasar) },
    { MP_ROM_QSTR(MP_QSTR_init),        MP_ROM_PTR(&q_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_run),         MP_ROM_PTR(&q_run_obj) },
    { MP_ROM_QSTR(MP_QSTR_keys),        MP_ROM_PTR(&q_keys_obj) },
    { MP_ROM_QSTR(MP_QSTR_getkey),      MP_ROM_PTR(&q_getkey_obj) },
    { MP_ROM_QSTR(MP_QSTR_held),        MP_ROM_PTR(&q_held_obj) },
    { MP_ROM_QSTR(MP_QSTR_show),        MP_ROM_PTR(&q_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_save_blob),   MP_ROM_PTR(&q_save_blob_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_blob),   MP_ROM_PTR(&q_load_blob_obj) },
    { MP_ROM_QSTR(MP_QSTR_arcade_blob), MP_ROM_PTR(&q_arcade_blob_obj) },
    { MP_ROM_QSTR(MP_QSTR_arcade_load), MP_ROM_PTR(&q_arcade_load_obj) },
    { MP_ROM_QSTR(MP_QSTR_battery),     MP_ROM_PTR(&q_battery_obj) },
    { MP_ROM_QSTR(MP_QSTR_settings),    MP_ROM_PTR(&q_settings_obj) },
    { MP_ROM_QSTR(MP_QSTR_fast),        MP_ROM_PTR(&q_fast_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_clear),    MP_ROM_PTR(&q_ui_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_text),     MP_ROM_PTR(&q_ui_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_width),    MP_ROM_PTR(&q_ui_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_rect),     MP_ROM_PTR(&q_ui_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_logo),     MP_ROM_PTR(&q_ui_logo_obj) },
};
STATIC MP_DEFINE_CONST_DICT(quasar_globals, quasar_globals_table);

const mp_obj_module_t quasar_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&quasar_globals,
};

MP_REGISTER_MODULE(MP_QSTR_quasar, quasar_module, 1);

// EOF
