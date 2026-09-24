# system.py - QUASAR: the SYSTEM screens.
#
# Device info, power off, and the way back to the official firmware: install
# any signed .dfu file from a microSD card. The install follows the stock
# firmware's steps (actions.py, auth.py, login.py in the Coldcard sources):
# check the file's header, stage the image in PSRAM, get the main PIN, and
# hand over to the bootloader, which verifies the signature itself before
# anything is written.
#
# Adapted from those files, (c) Coinkite Inc.; covered by COPYING-CC.
#
import gc, ustruct, uctypes
from micropython import const
import quasar, ui, qhw, callgate
from ui import K_ENTER, K_CANCEL, K_DEL, K_LEFT

PSRAM_BASE = const(0x90000000)     # mapped by the bootloader (psram.py)
PSRAM_LEN = const(0x400000)        # lower 4MB: where firmware gets staged

MIN_DFU_SIZE = const(0x7800)       # same limits as the stock file picker


def run():
    # returns 'off' if the user asked to power down, else None
    sel = 0
    while True:
        sel = ui.menu('SYSTEM', ['INSTALL FIRMWARE', 'DEVICE INFO', 'POWER OFF', 'BACK TO GAME'], sel)
        if sel == 0:
            install()
        elif sel == 1:
            info()
        elif sel == 2:
            if ui.story('POWER OFF', 'Switch the Coldcard off now?',
                        'ENTER: OFF    CANCEL: BACK') == K_ENTER:
                return 'off'
        else:
            ui.flush()
            return None
        gc.collect()


# ------------------------------------------------------------------ header


def decode_header(hdr):
    # (date, version, pubkey number) from a firmware header
    from sigheader import FWH_PY_FORMAT
    _magic, ts, vers, pk = ustruct.unpack_from(FWH_PY_FORMAT, hdr)[0:4]
    parts = ['%02x' % i for i in ts]
    date = '20' + '-'.join(parts[0:3])
    return date, bytes(vers).rstrip(b'\0').decode(), pk


def check_header(hdr, size):
    # basic checks, as in the stock firmware; returns an error message or None
    from sigheader import FW_HEADER_SIZE, FW_HEADER_MAGIC, FWH_PY_FORMAT, MK_Q1_OK
    try:
        assert len(hdr) >= FW_HEADER_SIZE
        magic, ts, _v, _pk, fw_size, _fl, hw_compat = ustruct.unpack_from(FWH_PY_FORMAT, hdr)[0:7]
        assert magic == FW_HEADER_MAGIC, 'bad magic'
        # stricter than stock: the bootloader refuses anything but an exact match, after the PIN
        assert fw_size == size, 'size problem'
    except Exception as exc:
        return 'That does not look like a firmware file we would want to use: %s' % exc

    if hw_compat and not (hw_compat & MK_Q1_OK):
        return "That firmware is not for the Coldcard Q."

    water = callgate.get_highwater()
    if water[0] and ts < water:
        return 'That downgrade is not supported.'
    return None


def own_header():
    # the header of the firmware running now, as the bootloader saw it
    from sigheader import FLASH_HEADER_BASE_MK4, FW_HEADER_SIZE
    return uctypes.bytes_at(FLASH_HEADER_BASE_MK4, FW_HEADER_SIZE)


# ------------------------------------------------------------------ info


def info():
    import card
    rows = []
    try:
        date, vers, _pk = decode_header(own_header())
        rows.append(('FIRMWARE', 'QUASAR %s (%s)' % (vers, date)))
    except Exception:
        rows.append(('FIRMWARE', 'QUASAR'))
    try:
        rows.append(('BOOTLOADER', callgate.get_bl_version()[0]))
    except Exception:
        pass
    try:
        v = qhw.battery_volts()
        rows.append(('POWER', 'USB' if v is None else ('BATTERY %.1fV' % v)))
    except Exception:
        pass
    try:
        a, b = card.inserted()
        rows.append(('MICROSD', '%s / %s' % ('TOP: CARD' if a else 'TOP: EMPTY',
                                           'BOTTOM: CARD' if b else 'BOTTOM: EMPTY')))
    except Exception:
        pass
    gc.collect()
    rows.append(('FREE RAM', '%d KB' % (gc.mem_free() // 1024)))

    ui.clear()
    ui.title('DEVICE INFO')
    y = 56
    for label, value in rows:
        ui.text(20, y, label, ui.GREY)
        ui.text(110, y, value, ui.WHITE)
        y += 20
    ui.footer('ANY KEY: BACK')
    ui.show()
    ui.flush()
    ui.key()


# ------------------------------------------------------------------ install


def install():
    k = ui.story('INSTALL FIRMWARE',
                 'Installs a signed firmware file (.dfu) from a microSD card.\n\n'
                 'To go back to the official Coldcard firmware, download the '
                 'latest Q release from coldcard.com/downloads and put it on '
                 'a card.\n\n'
                 'The bootloader checks the signature, and it will only '
                 'install firmware once your main PIN has been entered.',
                 'ENTER: CONTINUE    CANCEL: BACK')
    if k != K_ENTER:
        return

    if qhw.battery_level() in (0, 1):
        ui.story('LOW BATTERY', 'Battery power is low right now. Connect USB power or '
                 'fit fresh batteries before installing firmware.', warn=True)
        return

    try:
        bricked = callgate.get_is_bricked()
    except Exception:
        bricked = False
    if bricked:
        ui.story('LOCKED', 'The secure element on this Coldcard is locked for good, '
                 'so the bootloader will not install firmware.', warn=True)
        return

    pick = choose_file()
    if not pick:
        return

    try:
        hdr, length = stage(*pick)
    except Exception as exc:
        ui.story('CANNOT USE FILE', str(exc), warn=True)
        return

    date, vers, pk = decode_header(hdr)
    if pk == 0:
        who = 'Signed with the public developer key (not an official release).'
    else:
        who = 'Claims Coinkite signing key #%d.' % pk
    k = ui.story('INSTALL THIS?',
                 '  %s\n  %s\n\n%s\n\nThe checksum and signature are verified by the '
                 'bootloader before any changes are made. Next: your main PIN.' % (vers, date, who),
                 'ENTER: YES    CANCEL: NO')
    if k != K_ENTER:
        return

    login_and_upgrade(length)


def choose_file():
    # (slot_b, name) of a .dfu file on either card, or None
    import card
    from sigheader import FW_MAX_DFU_SIZE_MK4
    while True:
        a, b = card.inserted()
        if not (a or b):
            if ui.story('MICROSD', 'Insert a microSD card holding the firmware (.dfu) '
                        'into either slot.', 'ENTER: RETRY    CANCEL: BACK') != K_ENTER:
                return None
            continue

        ui.message('MICROSD', 'Reading card...')
        files = []
        for slot_b, present in ((False, a), (True, b)):
            if not present:
                continue
            try:
                with card.Card(slot_b) as c:
                    for name, size in c.list('.dfu'):
                        if MIN_DFU_SIZE <= size <= FW_MAX_DFU_SIZE_MK4:
                            files.append((slot_b, name))
            except Exception:
                pass

        if not files:
            if ui.story('NO FIRMWARE FOUND', 'No .dfu file at the top level of the card. '
                        'Use the normal firmware file, not the -factory one.',
                        'ENTER: RETRY    CANCEL: BACK') != K_ENTER:
                return None
            continue

        labels = [('B ' if s else 'A ') + n[:44] for s, n in files]
        i = ui.menu('CHOOSE FIRMWARE', labels)
        return files[i] if i >= 0 else None


def stage(slot_b, name):
    # check the file and copy its firmware into PSRAM: returns (header, length)
    import card
    from sigheader import FW_HEADER_OFFSET, FW_HEADER_SIZE

    psram = uctypes.bytearray_at(PSRAM_BASE, PSRAM_LEN)
    buf = bytearray(0x4000)
    with card.Card(slot_b) as c:
        with open(c.root + '/' + name, 'rb') as fp:
            offset, size = card.dfu_parse(fp)
            if size > PSRAM_LEN:
                raise ValueError('File is too big.')

            hdr = bytearray(FW_HEADER_SIZE)
            fp.seek(offset + FW_HEADER_OFFSET)
            if fp.readinto(hdr) != FW_HEADER_SIZE:
                raise ValueError('File is too short.')
            failed = check_header(hdr, size)
            if failed:
                raise ValueError(failed)

            fp.seek(offset)
            pos = 0
            while pos < size:
                if (pos & 0x1ffff) == 0:
                    ui.progress('LOADING', name[:40], pos / size)
                here = fp.readinto(buf)
                if not here:
                    break
                # PSRAM takes word-aligned writes only (see stock psram.py)
                n = min(len(buf), (here + 3) & ~3, PSRAM_LEN - pos)
                memoryview(psram)[pos:pos + n] = memoryview(buf)[0:n]
                pos += here
            if pos < size:
                raise ValueError('File is truncated.')
    return hdr, size


# ------------------------------------------------------------------ PIN


def enter_pin(pa):
    # the main PIN as b'prefix-suffix', or None if cancelled
    parts = ['', '']
    part = 0
    ui.flush()
    while True:
        ui.clear()
        ui.title('MAIN PIN')
        ui.center(50, 'Your MAIN PIN. Never enter a trick PIN here.', ui.GREY)
        for i, label in enumerate(('PREFIX', 'SUFFIX')):
            x = 40 + i * 130
            live = (i == part)
            ui.text(x, 80, label, ui.CYAN if live else ui.DIM)
            quasar.ui_rect(x - 4, 94, 114, 26, ui.CYAN if live else ui.DIM, 1)
            quasar.ui_text(x + 8, 100, parts[i] + ('_' if live else ''), ui.WHITE, 2, ui.SHADOW)
        ui.center(104, '-', ui.GREY, 2)
        if pa.num_fails:
            ui.center(140, '%d FAILURES, %d TRIES LEFT' % (pa.num_fails, pa.attempts_left),
                      ui.RED if pa.attempts_left <= 5 else ui.GOLD)
        ui.center(160, 'Each part is 2 to 6 digits.', ui.GREY)
        ui.footer('0-9 DIGITS   DEL ERASE   ENTER NEXT   CANCEL BACK')
        ui.show()

        k = ui.key()
        d = ui.digit(k)
        if d is not None:
            if len(parts[part]) < 6:
                parts[part] += d
        elif k in (K_DEL, K_LEFT):
            if parts[part]:
                parts[part] = parts[part][:-1]
            elif part:
                part = 0
        elif k == K_ENTER:
            if len(parts[part]) >= 2:
                if part == 0:
                    part = 1
                else:
                    return (parts[0] + '-' + parts[1]).encode()
        elif k == K_CANCEL:
            if part and not parts[1]:
                part = 0
            else:
                return None


def err_code(exc):
    # the bootloader's error number from a pinattempt error, if it has one
    return exc.args[1] if len(exc.args) > 1 else None


def locked_forever(num_fails):
    ui.story('I AM BRICK!', 'After %d failed PIN attempts this Coldcard is locked forever. '
             'By design, there is no way to reset or recover the secure element, and its '
             'contents are now forever inaccessible.\n\nThe game still works, but no '
             'other firmware can be installed.' % num_fails, warn=True)


def login_and_upgrade(length):
    import pinattempt
    pa = pinattempt.PinAttempt()
    try:
        pa.setup(b'')           # where do we stand?
    except Exception as exc:
        ui.story('BOOTLOADER', 'Could not reach the bootloader: %s' % exc, warn=True)
        return

    # A blank Coldcard (no PIN ever set) is already logged in by that setup(),
    # as in the stock firmware; the bootloader refuses a login() on top of it
    # (EPIN_WRONG_SUCCESS), so go straight to the upgrade.
    pin = None
    while not pa.is_blank():
        if pin is None:
            if not pa.attempts_left:
                locked_forever(pa.num_fails)
                return
            pin = enter_pin(pa)
            if pin is None:
                return
            if pa.num_fails > 3:
                # they are getting close: warn on every attempt, like stock
                k = ui.story('WARNING', 'You have %d attempts left before this Coldcard '
                             'BRICKS ITSELF FOREVER.\n\nCheck and double-check your entry:'
                             '\n\n  %s\n\nMaybe even take a break and come back later.'
                             % (pa.attempts_left, pin.decode()),
                             'ENTER: CONTINUE    CANCEL: STOP', warn=True)
                if k != K_ENTER:
                    return

        ui.message('MAIN PIN', 'Checking...')
        unchecked = None
        try:
            pa.setup(pin)
            ok = pa.login()
        except RuntimeError as exc:
            ok = False
            if err_code(exc) == pinattempt.EPIN_I_AM_BRICK:
                locked_forever(pa.num_fails)
                return
            if err_code(exc) != pinattempt.EPIN_AUTH_FAIL:
                # No verdict on the PIN: the bootloader can answer AE_FAIL even
                # after a correct one, so this must never read as a wrong PIN.
                unchecked = exc.args[0]
        if ok:
            break

        pin = None
        # the secure element keeps the real counters: read them back, don't guess
        try:
            pa.setup(b'')
        except RuntimeError as exc:
            if err_code(exc) == pinattempt.EPIN_I_AM_BRICK:
                locked_forever(pa.num_fails + 1)
            else:
                ui.story('BOOTLOADER', 'Could not read the PIN counters: %s' % exc, warn=True)
            return
        if not pa.attempts_left:
            locked_forever(pa.num_fails)
            return
        if unchecked:
            ui.story('PIN NOT CHECKED', 'The bootloader stopped before giving an answer (%s), '
                     'so this was not necessarily a wrong PIN. Nothing was installed.\n\n'
                     '%d attempts left.' % (unchecked, pa.attempts_left), warn=True)
            return
        ui.story('WRONG PIN', '%d attempts left.\n\nPlease check all digits carefully, and '
                 'that the prefix versus suffix break point is correct.' % pa.attempts_left,
                 warn=True)

    # The bootloader takes it from here: it verifies the image in PSRAM, writes
    # it to flash with its own progress screen, and restarts. Not reached.
    ui.message('INSTALLING', 'Do not remove power.')
    try:
        pa.firmware_upgrade(0, length)
        err = 'The bootloader did not start the install.'
    except Exception as exc:
        err = 'The bootloader refused the install: %s' % exc
    ui.story('FAILED', err, warn=True)

# EOF
