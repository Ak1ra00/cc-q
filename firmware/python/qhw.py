# qhw.py - QUASAR: the few bits of Coldcard Q hardware handled from Python.
#
# Display bring-up follows st7788.py / gpu.py from the Coldcard firmware exactly;
# battery sensing follows battery.py. (c) Coinkite Inc. for those parts,
# see COPYING-CC.
#
from machine import Pin, SPI
import pyb

spi = None
_backlight = None

def display_setup():
    # The bootloader already initialised the panel; take the bus and go.
    global spi, _backlight
    spi = SPI(1, baudrate=60_000_000, polarity=0, phase=0)

    # The small co-processor (GPU) shares the LCD bus: G_CTRL high = we drive it.
    g_ctrl = Pin('G_CTRL')
    g_reset = Pin('G_RESET')
    g_busy = Pin('G_BUSY', Pin.IN, pull=Pin.PULL_DOWN)
    if g_ctrl() != 1:
        g_ctrl(1)
        for _ in range(200000):
            if g_busy() == 0:
                break
    g_reset(1)
    Pin('LCD_MOSI').init(mode=Pin.ALT, pull=Pin.PULL_DOWN, af=Pin.AF5_SPI1)
    Pin('LCD_SCLK').init(mode=Pin.ALT, pull=Pin.PULL_DOWN, af=Pin.AF5_SPI1)

    _backlight = pyb.LED(1)
    _backlight.on()
    return spi

BRIGHT = (70, 120, 170, 215, 255)

def backlight(level):
    if _backlight is not None:
        _backlight.intensity(BRIGHT[max(0, min(4, level))])

# ---- battery (same method as the stock firmware)

_nbat = None

def _nbat_pin():
    global _nbat
    if _nbat is None:
        rev_d_later = not Pin('REV_D', mode=Pin.IN, pull=Pin.PULL_UP).value()
        _nbat = Pin('NOT_BATTERY_OLD' if not rev_d_later else 'NOT_BATTERY', mode=Pin.IN, pull=Pin.PULL_UP)
    return _nbat

def battery_volts():
    # None when running from USB
    from machine import ADC
    if _nbat_pin()() == 1:
        return None
    adc = ADC('VIN_SENSE')
    vals = [adc.read_u16() for i in range(5)]       # errata: skip the first reading
    avg = sum(vals[1:]) / 4.0
    return round((avg / 65535.0) * 3.3 * 2, 1)

def battery_level():
    # -1 USB, 0 empty, 1 low, 2 ok, 3 full
    try:
        v = battery_volts()
    except Exception:
        return -1
    if v is None:
        return -1
    if v <= 2.9:
        return 0
    if v <= 3.5:
        return 1
    if v <= 4.0:
        return 2
    return 3

def power_off():
    # the bootloader wipes RAM and cuts power (on batteries)
    import callgate
    callgate.show_logout(3)

# EOF
