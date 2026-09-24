# ui.py - QUASAR: drawing and input for the system screens.
#
# Everything here is plain and blocking. System screens redraw only when
# something changes, and always reach the panel through the safe (non-DMA)
# path, so they keep working even if the game's fast path is ever disabled.
#
import quasar
from micropython import const

# colours, 0xRRGGBB (the game's palette)
WHITE = const(0xffffff)
BLACK = const(0x000000)
CYAN = const(0x5adcff)
GOLD = const(0xffd65a)
RED = const(0xff3c50)
GREEN = const(0x5aff8c)
GREY = const(0x8c8caa)
DIM = const(0x50506e)

# text flags (text.h)
SHADOW = const(0x01)
OUTLINE = const(0x02)

# key numbers in the Q keyboard matrix (game.h)
K_LEFT = const(3)
K_UP = const(4)
K_DOWN = const(5)
K_RIGHT = const(6)
K_CANCEL = const(7)
K_ENTER = const(8)
K_1 = const(10)             # the number row runs 1..9 then 0
K_0 = const(19)
K_S = const(31)
K_DEL = const(54)
K_POWER = const(63)

# special glyphs in the font (text.h)
G_UP = '\x01'
G_DOWN = '\x02'
G_TRI_R = '\x08'
G_TRI_L = '\x0b'

W = const(320)
H = const(240)
LINE = const(12)
COLS = const(48)            # characters per line at scale 1, with margins
MAX_ROWS = const(7)         # menu rows that fit on one screen


def digit(k):
    # '0'..'9' for a key on the number row, else None
    if K_1 <= k <= K_0:
        return '1234567890'[k - K_1]
    return None


def key(timeout=-1):
    # next newly pressed key, or -1 after timeout ms (-1 = wait forever)
    return quasar.getkey(timeout)


def flush():
    # wait for every key to be released, so a key still held from the
    # previous screen (or from the game) is not taken as a new press
    while quasar.keys():
        pass
    quasar.getkey(0)


def clear(warn=False):
    quasar.ui_clear(1 if warn else 0)


def title(s, warn=False):
    quasar.ui_text(-1, 12, s, RED if warn else GOLD, 2, OUTLINE)
    quasar.ui_rect(20, 36, W - 40, 1, RED if warn else CYAN, 2)


def text(x, y, s, col=WHITE):
    return quasar.ui_text(x, y, s, col, 1, SHADOW)


def center(y, s, col=WHITE, scale=1):
    quasar.ui_text(-1, y, s, col, scale, SHADOW)


def footer(s):
    quasar.ui_rect(0, H - 18, W, 18, BLACK, 3)
    center(H - 13, s, GREY)


def show():
    quasar.show()


def wrap(s, width=COLS):
    # split text into lines of at most `width` characters, on spaces
    out = []
    for para in s.split('\n'):
        line = ''
        for word in para.split(' '):
            if line and len(line) + 1 + len(word) > width:
                out.append(line)
                line = word
            else:
                line = (line + ' ' + word) if line else word
        out.append(line)
    return out


def story(head, body, foot='ENTER: OK', warn=False, keys=(K_ENTER, K_CANCEL)):
    # a page of text; returns the key that closed it
    clear(warn)
    title(head, warn)
    y = 48
    for ln in wrap(body):
        text(14, y, ln)
        y += LINE
    footer(foot)
    show()
    flush()
    while True:
        k = key()
        if k in keys:
            return k


def message(head, body, warn=False):
    # a page with no keys, for work in progress
    clear(warn)
    title(head, warn)
    y = 100 - 6 * len(wrap(body))
    for ln in wrap(body):
        center(y, ln)
        y += LINE
    show()


def menu(head, items, sel=0, foot=None):
    # pick one item: returns its index, or -1 if cancelled
    flush()
    while True:
        clear()
        title(head)
        top = max(0, min(sel - MAX_ROWS // 2, len(items) - MAX_ROWS))
        y = 52
        for i in range(top, min(len(items), top + MAX_ROWS)):
            if i == sel:
                quasar.ui_rect(24, y - 4, W - 48, 17, CYAN, 2)
                center(y, G_TRI_R + ' ' + items[i] + ' ' + G_TRI_L, WHITE)
            else:
                center(y, items[i], GREY)
            y += 22
        if top > 0:
            center(42, G_UP, DIM)
        if top + MAX_ROWS < len(items):
            center(y - 6, G_DOWN, DIM)
        footer(foot or (G_UP + G_DOWN + ' SELECT    ENTER OK    CANCEL BACK'))
        show()
        k = key()
        if k == K_UP:
            sel = (sel - 1) % len(items)
        elif k == K_DOWN:
            sel = (sel + 1) % len(items)
        elif k == K_ENTER:
            return sel
        elif k == K_CANCEL:
            return -1


def progress(head, msg, frac):
    clear()
    title(head)
    center(96, msg)
    x, y, w, h = 40, 124, W - 80, 12
    quasar.ui_rect(x, y, w, h, GREY, 1)
    fill = int((w - 4) * max(0.0, min(1.0, frac)))
    if fill > 0:
        quasar.ui_rect(x + 2, y + 2, fill, h - 4, CYAN, 0)
    show()

# EOF
