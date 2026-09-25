#!/usr/bin/env -S true
# test_main.py - runs main.py (boot, the two save files, the event loop) under
# the real (unix-built, pinned-commit) MicroPython interpreter, against the same
# mocks as test_system.py. main.py starts the device when imported, so its
# source is run here without that last step and its functions are called one
# by one, with the save files pointed at a scratch directory.
#
# Run with: external/micropython/ports/unix/micropython tests/test_main.py
import sys, os

_f = __file__ if '__file__' in dir() else 'test_main.py'
HERE = _f.rsplit('/', 1)[0] if '/' in _f else '.'
sys.path[:0] = [HERE + '/mocks', HERE + '/../python']

import uctypes_mock
sys.modules['uctypes'] = uctypes_mock

import quasar, qhw, machine             # mocks

FAILED = []
TMP = '/tmp/quasar-test-main-%d' % os.getpid() if hasattr(os, 'getpid') else '/tmp/quasar-test-main'


def check(name, cond, detail=''):
    print('[%s] %s%s' % ('ok ' if cond else 'FAIL', name, ('  -- ' + detail) if detail and not cond else ''))
    if not cond:
        FAILED.append(name)


def load_main():
    # main.py without its last lines (the try: main() that starts the device)
    src = open(HERE + '/../python/main.py').read()
    src = src[:src.rindex('\ntry:\n    main()')]
    ns = {'__name__': 'main_under_test'}
    exec(src, ns)
    ns['SAVE_FILE'] = TMP + '/quasar.sav'
    ns['ARCADE_FILE'] = TMP + '/arcade.sav'
    return ns


def exists(path):
    try:
        os.stat(path)
        return True
    except OSError:
        return False


def read(path):
    with open(path, 'rb') as f:
        return f.read()


def clean():
    for n in ('quasar.sav', 'arcade.sav', 'quasar.sav.new', 'arcade.sav.new'):
        try:
            os.remove(TMP + '/' + n)
        except OSError:
            pass


def reset_all():
    quasar.reset()
    del qhw.backlights[:]
    clean()


def test_load_nothing():
    reset_all()
    m = load_main()
    m['load']()
    check('load: no files yet is fine, nothing loaded',
          not [c for c in quasar.calls if isinstance(c, tuple) and c[0] in ('load', 'arcade_load')])


def test_save_and_load():
    reset_all()
    m = load_main()
    m['save']()
    m['save_arcade']()
    check('save: quasar.sav written', exists(TMP + '/quasar.sav') and read(TMP + '/quasar.sav') == b'SAVE-BLOB')
    check('save: arcade.sav written beside it', read(TMP + '/arcade.sav') == b'ARCADE-BLOB')
    check('save: no temporary files left', not exists(TMP + '/quasar.sav.new') and not exists(TMP + '/arcade.sav.new'))
    del quasar.calls[:]
    m['load']()
    check('load: both files reach the game',
          ('load', b'SAVE-BLOB') in quasar.calls and ('arcade_load', b'ARCADE-BLOB') in quasar.calls)


def test_load_is_bounded():
    reset_all()
    m = load_main()
    with open(TMP + '/arcade.sav', 'wb') as f:
        f.write(b'x' * 50000)
    with open(TMP + '/quasar.sav', 'wb') as f:
        f.write(b'y' * 50000)
    m['load']()
    sizes = dict((c[0], len(c[1])) for c in quasar.calls if isinstance(c, tuple) and c[0] in ('load', 'arcade_load'))
    check('load: a huge or damaged file is read only in part', sizes.get('load') == 1024 and sizes.get('arcade_load') == 2048,
          repr(sizes))


def run_events(m, events):
    quasar.events_q[:] = list(events)
    try:
        m['play']()
    except qhw.PowerOff:
        return 'off'
    except IndexError:
        return 'done'           # the scripted events ran out
    return None


def test_events_save_the_right_file():
    reset_all()
    m = load_main()
    run_events(m, [m['EV_SAVE_ARCADE']])
    check('events: EV_SAVE_ARCADE writes arcade.sav only', exists(TMP + '/arcade.sav') and not exists(TMP + '/quasar.sav'))
    reset_all()
    run_events(m, [m['EV_SAVE']])
    check('events: EV_SAVE writes quasar.sav only', exists(TMP + '/quasar.sav') and not exists(TMP + '/arcade.sav'))
    reset_all()
    run_events(m, [m['EV_BRIGHT']])
    check('events: EV_BRIGHT applies the brightness', qhw.backlights == [3])


def test_power_off_saves_both():
    for ev_name in ('EV_POWEROFF', 'EV_IDLE_OFF'):
        reset_all()
        m = load_main()
        rv = run_events(m, [0, m[ev_name]])
        check('%s: switches off' % ev_name, rv == 'off')
        check('%s: both files saved first' % ev_name, exists(TMP + '/quasar.sav') and exists(TMP + '/arcade.sav'))


def test_system_and_back():
    reset_all()
    m = load_main()
    quasar.keys_q[:] = [3 + 2, 3 + 2, 3 + 2, 8]     # DOWN x3 to BACK, ENTER
    rv = run_events(m, [m['EV_SYSTEM'], 0])
    check('events: SYSTEM runs the system screens and comes back to the games', rv == 'done' and not quasar.keys_q)
    check('events: the SYSTEM menu offers BACK', any('BACK' in s for scr in quasar.screens for s in scr))


def test_boot_order():
    # the S check comes before anything touches /flash, so SYSTEM stays reachable
    for held in (False, True):
        reset_all()
        m = load_main()
        order = []
        m['flash_fs'] = lambda: order.append('flash_fs')
        m['load'] = lambda: order.append('load')
        m['system_screens'] = lambda: order.append('system')
        m['apply_settings'] = lambda: order.append('settings')
        m['play'] = lambda: order.append('play')
        orig_held = quasar.held
        quasar.held = lambda k: order.append('held') or held
        try:
            m['main']()
        finally:
            quasar.held = orig_held
        want = ['held'] + (['system'] if held else []) + ['flash_fs', 'load', 'settings', 'play']
        check('boot (S %s): display, S check, then files' % ('held' if held else 'not held'),
              quasar.calls[:1] == ['init'] and order == want, repr(order))


def main():
    try:
        os.mkdir(TMP)
    except OSError:
        pass
    for fn in (test_load_nothing, test_save_and_load, test_load_is_bounded, test_events_save_the_right_file,
               test_power_off_saves_both, test_system_and_back, test_boot_order):
        try:
            fn()
        except Exception as exc:
            FAILED.append(fn.__name__)
            print('[FAIL] %s raised %r' % (fn.__name__, exc))
            sys.print_exception(exc)
    clean()
    try:
        os.rmdir(TMP)
    except OSError:
        pass
    print()
    if FAILED:
        print('%d test(s) FAILED: %s' % (len(FAILED), ', '.join(FAILED)))
        sys.exit(1)
    print('all tests passed')


if __name__ == '__main__':
    main()
