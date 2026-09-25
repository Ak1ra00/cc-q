# main.py - QUASAR for the Coldcard Q: boot, run the games, handle their events.
#
# By the time this runs, the bootloader has checked the firmware signature and
# shown its warning. Nothing here touches the secure elements: the home screen
# and the games (QUASAR, TETRIS) run in C (the quasar module), and Python only
# starts them, keeps their save files, and hosts the SYSTEM screens. The one
# path that involves the PIN is installing other firmware (system.py), because
# the bootloader requires it.
#
# Hold S while switching on to go straight to the SYSTEM screens.
#
import gc, os, pyb, utime
from micropython import const
import quasar, qhw

SAVE_FILE = '/flash/quasar.sav'         # QUASAR, and the settings every game shares
ARCADE_FILE = '/flash/arcade.sav'       # the home screen and TETRIS

EV_SAVE = const(0x01)
EV_SYSTEM = const(0x02)
EV_POWEROFF = const(0x04)
EV_BRIGHT = const(0x08)
EV_VSYNC = const(0x10)
EV_SAVE_ARCADE = const(0x20)
EV_IDLE_OFF = const(0x100)

K_S = const(31)


def flash_fs():
    # As the stock firmware does: only make a filesystem when there is none at
    # all, so the official firmware's settings in /flash survive a round trip.
    try:
        os.statvfs('/flash')
    except OSError:
        fl = pyb.Flash(start=0)
        os.VfsLfs2.mkfs(fl)
        os.mount(fl, '/flash')
        os.mkdir('/flash/settings')


def load():
    try:
        with open(SAVE_FILE, 'rb') as f:
            # a real save is at most 256 bytes; a damaged file must not stop the boot
            quasar.load_blob(f.read(1024))
    except OSError:
        pass            # first run: defaults
    try:
        with open(ARCADE_FILE, 'rb') as f:
            quasar.arcade_load(f.read(2048))        # a real one is under 640 bytes
    except OSError:
        pass


def write(path, data):
    # write the new data beside the old, then rename over it (atomic on littlefs)
    try:
        tmp = path + '.new'
        with open(tmp, 'wb') as f:
            f.write(data)
        os.rename(tmp, path)
    except OSError as exc:
        print('save: %r' % exc)


def save():
    write(SAVE_FILE, quasar.save_blob())


def save_arcade():
    write(ARCADE_FILE, quasar.arcade_blob())


def apply_settings():
    bright, _vsync, _fast = quasar.settings()
    qhw.backlight(bright)


def power_off():
    save()
    save_arcade()
    qhw.power_off()     # the bootloader clears RAM and cuts power


def system_screens():
    import system
    if system.run() == 'off':
        power_off()
    gc.collect()


def play():
    next_batt = utime.ticks_ms()
    while True:
        if utime.ticks_diff(utime.ticks_ms(), next_batt) >= 0:
            quasar.battery(qhw.battery_level())
            next_batt = utime.ticks_add(utime.ticks_ms(), 10000)

        ev = quasar.run(1000)

        if ev & EV_SAVE:
            save()
        if ev & EV_SAVE_ARCADE:
            save_arcade()
        if ev & (EV_BRIGHT | EV_VSYNC):
            apply_settings()
        if ev & (EV_POWEROFF | EV_IDLE_OFF):
            power_off()
        if ev & EV_SYSTEM:
            system_screens()


def crashed(exc):
    # Never leave anyone stuck: say what happened, and keep the SYSTEM screens
    # (and so the way back to the official firmware) one key away.
    import sys, uio, ui
    buf = uio.StringIO()
    sys.print_exception(exc, buf)
    lines = buf.getvalue().strip().split('\n')
    print('\n'.join(lines))
    while True:
        try:
            quasar.fast(False)
            quasar.init(qhw.display_setup())     # in case we failed before it ran
            k = ui.story('SOMETHING WENT WRONG',
                         '\n'.join(ln.strip()[:ui.COLS] for ln in lines[-6:]),
                         'ENTER: RESTART    S: SYSTEM', warn=True, keys=(ui.K_ENTER, ui.K_S))
        except Exception:
            utime.sleep_ms(3000)
            k = ui.K_ENTER
        if k == ui.K_ENTER:
            import machine
            machine.reset()
        try:
            system_screens()
        except Exception as exc2:
            lines = [repr(exc2)]


def main():
    # Display first, so any later failure can still be shown (see crashed()),
    # and the S check before /flash is touched: the bootloader has no recovery
    # for firmware that is validly signed but fails early, so SYSTEM has to stay
    # reachable even with a damaged filesystem.
    quasar.init(qhw.display_setup())
    if quasar.held(K_S):
        system_screens()
    flash_fs()
    load()
    apply_settings()
    play()


try:
    main()
except Exception as exc:
    crashed(exc)

# EOF
