#!/usr/bin/env -S true
# test_system.py - runs the QUASAR system-screen Python under the real
# (unix-built, pinned-commit) MicroPython interpreter, against mocks of the
# hardware modules. Exercises install()'s file picking, header checks, PIN
# handling (including brick and blank-device paths) and the menu/story UI,
# the same code path that runs on the device.
#
# Run with: external/micropython/ports/unix/micropython tests/test_system.py
import sys, ustruct, time

# os.path isn't available on this build (same as the device), so find our own
# directory by hand instead of with os.path.dirname.
_f = __file__ if '__file__' in dir() else 'test_system.py'
HERE = _f.rsplit('/', 1)[0] if '/' in _f else '.'
sys.path[:0] = [HERE + '/mocks', HERE + '/../python']

import uctypes_mock
sys.modules['uctypes'] = uctypes_mock

import quasar, qhw, card, ckcc          # mocks
import ui, system                       # code under test
import sigheader

# The real card.py mounts an actual filesystem at c.root, so system.py reads
# staged files with the plain open() builtin. Our mock card has no real
# filesystem, so point system.py's open() at the mock's instead - the same
# shadowing trick used for uctypes above, at the one call site that needs it.
system.open = card.open

FAILED = []


def check(name, cond, detail=''):
    mark = 'ok ' if cond else 'FAIL'
    print('[%s] %s%s' % (mark, name, ('  -- ' + detail) if detail and not cond else ''))
    if not cond:
        FAILED.append(name)


def reset_all():
    quasar.reset()
    ckcc.reset()
    card.slots[0].clear()
    card.slots[1].clear()
    card.present[0] = True
    card.present[1] = False
    uctypes_mock.regions.clear()


def make_header(hw_compat=sigheader.MK_Q1_OK, size=300000, pubkey_num=0,
                magic=sigheader.FW_HEADER_MAGIC, timestamp=b'\x26\x01\x01\x00\x00\x00\x00\x00'):
    hdr = bytearray(sigheader.FW_HEADER_SIZE)
    ustruct.pack_into(sigheader.FWH_PY_FORMAT, hdr, 0,
                      magic, timestamp, b'1.0.0', pubkey_num, size, 0, hw_compat,
                      bytes(8), bytes(4 * sigheader.FWH_NUM_FUTURE), bytes(64))
    return bytes(hdr)


def make_dfu_bytes(hdr, body_size):
    body = bytes(body_size)
    # header sits at FW_HEADER_OFFSET inside the "firmware" region
    fw = bytearray(sigheader.FW_HEADER_OFFSET + sigheader.FW_HEADER_SIZE + len(body))
    fw[sigheader.FW_HEADER_OFFSET:sigheader.FW_HEADER_OFFSET + sigheader.FW_HEADER_SIZE] = hdr
    fw[sigheader.FW_HEADER_OFFSET + sigheader.FW_HEADER_SIZE:] = body
    return b'MOCKDFU!' + bytes(fw), len(fw)


# ------------------------------------------------------------------ check_header

def test_check_header():
    reset_all()
    hdr = make_header(size=1000)
    check('check_header: good header passes', system.check_header(hdr, 1000) is None)

    bad_magic = make_header(magic=0xdeadbeef, size=1000)
    check('check_header: bad magic rejected', system.check_header(bad_magic, 1000) is not None)

    check('check_header: size mismatch rejected',
         system.check_header(hdr, 999) is not None)

    wrong_hw = make_header(hw_compat=sigheader.MK_4_OK, size=1000)
    check('check_header: wrong hw_compat rejected',
         system.check_header(wrong_hw, 1000) is not None)

    ckcc._state['highwater'] = b'\x26\x06\x01\x00\x00\x00\x00\x00'
    old = make_header(timestamp=b'\x26\x01\x01\x00\x00\x00\x00\x00', size=1000)
    check('check_header: downgrade below highwater rejected',
         system.check_header(old, 1000) is not None)
    ckcc._state['highwater'] = bytes(8)


def test_decode_header():
    hdr = make_header(timestamp=bytes((0x26, 0x03, 0x17, 0, 0, 0, 0, 0)))
    date, vers, pk = system.decode_header(hdr)
    check('decode_header: date', date == '2026-03-17', date)
    check('decode_header: version', vers == '1.0.0', vers)
    check('decode_header: pubkey', pk == 0, pk)


# ------------------------------------------------------------------ stage / choose_file

def test_choose_file_and_stage():
    reset_all()
    hdr = make_header(size=50000)
    dfu_bytes, fw_len = make_dfu_bytes(hdr, 50000 - sigheader.FW_HEADER_OFFSET - sigheader.FW_HEADER_SIZE)
    card.slots[0]['firmware.dfu'] = dfu_bytes

    quasar.keys_q[:] = [ui.K_ENTER]     # pick the only file in the menu
    pick = system.choose_file()
    check('choose_file: found the file', pick == (False, 'firmware.dfu'), pick)

    got_hdr, length = system.stage(*pick)
    check('stage: header round-trips', bytes(got_hdr) == hdr)
    check('stage: length matches', length == fw_len, length)

    psram = uctypes_mock.regions[system.PSRAM_BASE]
    check('stage: firmware bytes landed in PSRAM',
         bytes(psram[0:fw_len]) == dfu_bytes[8:8 + fw_len])


def test_choose_file_rejects_bad_size():
    reset_all()
    card.slots[0]['tiny.dfu'] = b'MOCKDFU!' + bytes(10)   # far under MIN_DFU_SIZE
    quasar.keys_q[:] = [ui.K_ENTER]     # would retry -> cancel
    quasar.keys_q.append(ui.K_CANCEL)
    pick = system.choose_file()
    check('choose_file: too-small file is not offered', pick is None)


# ------------------------------------------------------------------ PIN / upgrade flow

def test_upgrade_blank_device():
    # A never-configured Coldcard: the empty PIN logs straight in, no prompt.
    # (Our mock bootloader, unlike the real one, returns from firmware_upgrade()
    # instead of rebooting, so login_and_upgrade then shows a FAILED screen;
    # queue the key that dismisses it.)
    reset_all()
    ckcc.reset(is_blank=True)
    quasar.keys_q[:] = [ui.K_ENTER]
    system.login_and_upgrade(12345)
    check('blank device: no PIN prompt was shown', not shown('PREFIX'))
    check('blank device: firmware_upgrade reached the bootloader',
         ckcc._state['upgrade_started'] == (0, 12345), ckcc._state['upgrade_started'])


def shown(fragment):
    # true if some drawn screen contains this fragment, joining each screen's
    # pieces (one per ui_text() call) so a fragment split across a wrapped
    # line still matches
    return any(fragment in ' '.join(screen) for screen in quasar.screens)


def key_pin(prefix, suffix):
    # the digit/ENTER sequence enter_pin() expects for one PIN
    out = []
    for d in prefix:
        out.append(ui.K_1 + '1234567890'.index(d))
    out.append(ui.K_ENTER)
    for d in suffix:
        out.append(ui.K_1 + '1234567890'.index(d))
    out.append(ui.K_ENTER)
    return out


def test_upgrade_correct_pin():
    reset_all()
    ckcc.reset(correct_pin=b'123456-789', attempts_left=13)
    quasar.keys_q[:] = key_pin('123456', '789') + [ui.K_ENTER]   # + dismiss FAILED
    system.login_and_upgrade(999)
    check('correct PIN: firmware_upgrade reached the bootloader',
         ckcc._state['upgrade_started'] == (0, 999), ckcc._state['upgrade_started'])


def test_upgrade_wrong_then_correct_pin():
    reset_all()
    ckcc.reset(correct_pin=b'11-22', attempts_left=13)
    quasar.keys_q[:] = (key_pin('99', '99') + [ui.K_ENTER]      # dismiss WRONG PIN
                        + key_pin('11', '22') + [ui.K_ENTER])  # dismiss FAILED
    system.login_and_upgrade(42)
    check('wrong-then-right PIN: eventually upgrades',
         ckcc._state['upgrade_started'] == (0, 42))
    check('wrong-then-right PIN: the wrong attempt was reported',
         shown('WRONG PIN') and shown('12 attempts left'))


def test_upgrade_runs_out_of_attempts():
    reset_all()
    ckcc.reset(correct_pin=b'11-22', attempts_left=1)
    quasar.keys_q[:] = key_pin('99', '99') + [ui.K_ENTER]   # dismiss the brick story
    system.login_and_upgrade(1)
    check('brick: no upgrade was started', ckcc._state['upgrade_started'] is None)
    check('brick: the I AM BRICK screen was shown', shown('I AM BRICK'))


def test_low_battery_blocks_install():
    reset_all()
    qhw.level = 0
    quasar.keys_q[:] = [ui.K_ENTER, ui.K_ENTER]      # past the intro story, then LOW BATTERY
    try:
        system.install()
        check('low battery: install() returned without touching the bootloader',
             ckcc._state['upgrade_started'] is None)
    finally:
        qhw.level = 3


# ------------------------------------------------------------------ ui

def test_wrap_respects_width():
    lines = ui.wrap('one two three four five six seven eight nine ten', width=12)
    check('wrap: no line exceeds the width',
         all(len(ln) <= 12 for ln in lines), lines)
    check('wrap: words are not dropped',
         ' '.join(lines).split() == 'one two three four five six seven eight nine ten'.split())


def test_menu_navigation():
    reset_all()
    quasar.keys_q[:] = [ui.K_DOWN, ui.K_DOWN, ui.K_UP, ui.K_ENTER]
    sel = ui.menu('TEST', ['A', 'B', 'C'])
    check('menu: down,down,up lands on B', sel == 1, sel)

    quasar.keys_q[:] = [ui.K_CANCEL]
    sel = ui.menu('TEST', ['A', 'B'])
    check('menu: cancel returns -1', sel == -1, sel)


def test_story_keys():
    quasar.keys_q[:] = [ui.K_LEFT, ui.K_ENTER]     # an unrelated key is ignored
    k = ui.story('HEAD', 'body text')
    check('story: ignores keys not in its set, returns the matching one', k == ui.K_ENTER, k)


def main():
    for fn in (test_check_header, test_decode_header, test_choose_file_and_stage,
              test_choose_file_rejects_bad_size, test_upgrade_blank_device,
              test_upgrade_correct_pin, test_upgrade_wrong_then_correct_pin,
              test_upgrade_runs_out_of_attempts, test_low_battery_blocks_install,
              test_wrap_respects_width, test_menu_navigation, test_story_keys):
        try:
            fn()
        except Exception as exc:
            FAILED.append(fn.__name__)
            print('[FAIL] %s raised %r' % (fn.__name__, exc))
            sys.print_exception(exc)
    print()
    if FAILED:
        print('%d test(s) FAILED: %s' % (len(FAILED), ', '.join(FAILED)))
        sys.exit(1)
    print('all tests passed')


if __name__ == '__main__':
    main()
