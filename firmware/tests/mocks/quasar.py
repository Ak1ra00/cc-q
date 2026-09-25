# Mock of the quasar C module: records what the screens draw, and plays back
# scripted keys and game events.
screens = []            # text drawn on each show(), newest last
calls = []
keys_q = []             # key numbers returned by getkey()
events_q = []           # values returned by run(); callables are called
held_keys = []
_cur = []


class OutOfKeys(Exception):
    pass


def reset():
    del screens[:], calls[:], keys_q[:], events_q[:], held_keys[:], _cur[:]


def init(spi):
    calls.append('init')


def run(ms):
    ev = events_q.pop(0)
    return ev() if callable(ev) else ev


def keys():
    return 0


def getkey(timeout):
    if timeout == 0:
        return -1
    if not keys_q:
        raise OutOfKeys('UI wants a key; last screen: %r' % (screens[-1:],))
    return keys_q.pop(0)


def held(k):
    return k in held_keys


def show():
    screens.append(list(_cur))


def save_blob():
    return b'SAVE-BLOB'


def load_blob(b):
    calls.append(('load', bytes(b)))
    return True


def arcade_blob():
    return b'ARCADE-BLOB'


def arcade_load(b):
    calls.append(('arcade_load', bytes(b)))
    return True


def battery(level):
    calls.append(('battery', level))


def settings():
    return (3, 0, True)


def fast(en):
    calls.append(('fast', en))


def ui_clear(style):
    del _cur[:]


def ui_text(x, y, s, col, scale=1, flags=0):
    _cur.append(s)
    return len(s) * 6 * scale


def ui_width(s, scale):
    return len(s) * 6 * scale


def ui_rect(x, y, w, h, col, mode=0):
    pass


def ui_logo(y):
    pass
