#
# QUASAR - the art. Hand drawn sprites as text plus procedural ones.
#
import math, random
from artlib import *

C = lambda h: hexrgb(h)

OUT = C('0b0d1c')           # universal dark outline

# ---------------------------------------------------------------- player

PLAYER_LEG = {
    'K': OUT,
    '1': C('1a2856'), '2': C('2c4a90'), '3': C('4676cc'), '4': C('78a8f2'), '5': C('c8e0ff'),
    'W': C('ffffff'),
    'c': C('0a4466'), 'C': C('1ec8ff'), 'h': C('b0f4ff'),
    'R': C('ff3d5a'), 'r': C('8e1630'),
    'g': C('343a58'), 'G': C('6a7296'),
    'o': C('ff9a2a'), 'y': C('fff08a'),
}

PLAYER = '''
......KK....................
......K5KK..................
......K44RKK................
......KK443rKK..............
.......K5433rRKKK...........
....KKKK44433332KKKKK.......
..KKGGK54433322222hhCKKK....
KKKGGgK5544333222CCCChh55KKK
..KKggK33222221111cCCcKKK...
....KKKK22211111KKKKK.......
.......K3221rrKKK...........
......KK221rKK..............
......K22rKK................
......K3KK..................
......KK....................
'''


def player_ship(bank=0):
    # procedural top-down fighter, nose to the right. bank: -1 rolling up, +1 down
    W, H = 32, 19
    cy = 9
    img = Img(W, H)
    hull = [C(h) for h in ['101a40', '1c2c62', '2c4a96', '4676cc', '78a8f2', 'c8e0ff']]
    wingc = [C(h) for h in ['141e48', '22346e', '34529e', '5078c8', '86aaee', 'c0d8ff']]
    span_top = 8.4 - (2.6 if bank < 0 else 0) + (0.0 if bank <= 0 else 0.0)
    span_bot = 8.4 - (2.6 if bank > 0 else 0)
    for y in range(H):
        for x in range(W):
            dy = y - cy
            ady = abs(dy)
            top = dy < 0
            part = None
            if 5 <= x <= 29:
                f = 3.4 if x < 17 else 3.4 * (29.5 - x) / 12.5
                if ady <= f:
                    part = ('hull', ady / max(f, 0.5))
            span = span_top if top else span_bot
            if 3 <= ady <= span:
                t = (ady - 3) / 5.0
                lead = 17 - 10 * t
                trail = 10 - 7 * t
                if trail <= x <= lead:
                    part = part or ('wing', t)
            if 3 <= ady <= 5.2:
                t = (ady - 3) / 2.2
                if 21 - t <= x <= 24 - 3 * t:
                    part = part or ('canard', t)
            if 1 <= x <= 5 and 1 <= ady <= 3.4:
                part = ('engine', ady)
            if part is None:
                continue
            kind, v = part
            if kind == 'hull':
                lvl = 5 - int(v * 3.2)
                if not top:
                    lvl -= 1
                if dy == 0:
                    lvl = 5 if x > 8 else 4
                if bank < 0 and not top:
                    lvl -= 1
                if bank > 0 and top:
                    lvl -= 1
                c = hull[clamp(lvl, 0, 5)]
            elif kind == 'wing':
                lvl = 4 - int(v * 2.5)
                if not top:
                    lvl -= 2
                if (bank < 0 and top) or (bank > 0 and not top):
                    lvl += 1
                c = wingc[clamp(lvl, 0, 5)]
            elif kind == 'canard':
                c = wingc[4 if top else 2]
            else:
                c = C('6a7296') if top else C('3a3f60')
                if ady >= 3:
                    c = C('343a58')
            img.set(x, y, c)
    for y in range(H):
        for x in range(W):
            dx = (x - 21.5) / 4.2
            dyy = (y - cy + 0.3) / 1.5
            if dx * dx + dyy * dyy <= 1:
                c = C('1ec8ff')
                if dyy < -0.2 and dx < 0.3:
                    c = C('b0f4ff')
                if dyy > 0.5:
                    c = C('0a5a88')
                img.set(x, y, c)
    img.set(12, cy - 3, C('1a2856'))
    img.set(12, cy + 3, C('101a40'))
    for x in range(9, 15):
        img.set(x, cy, C('a8c8ff'))
    if span_top > 7:
        img.set(4, cy - 8, C('ff3d5a'))
        img.set(3, cy - 8, C('ff3d5a'))
    if span_bot > 7:
        img.set(4, cy + 8, C('8e1630'))
        img.set(3, cy + 8, C('8e1630'))
    for d in (-2, 2):
        img.set(0, cy + d, C('ffe07a'))
        img.set(1, cy + d, C('ff9a2a'))
    return outline(img, OUT)

# ---------------------------------------------------------------- enemies

EN_METAL = {
    'K': OUT,
    'a': C('2a2438'), 'b': C('4a4460'), 'c': C('7a7494'), 'd': C('b8b2d0'), 'D': C('ece8ff'),
    'e': C('ff2a4a'), 'E': C('ffc0cc'), 'f': C('8a0a24'),
}

DRONE = '''
.....KKKKKK.....
...KKddccbbKK...
..KdDdccbbbaaK..
.KdDdccbbKKKaaK.
.KddccbKKfeeKaK.
KddccbbKfeEEeKaK
KdccbbbKfeEEeKaK
KccbbbbKfeeeeKaK
.KcbbbaaKKffKaK.
.KbbbaaaaaKKaaK.
..KbaaaaaaaaaK..
...KKaaaaaaKK...
.....KKKKKK.....
'''

EN_RED = {
    'K': OUT,
    '1': C('4a0c1c'), '2': C('8c1a2a'), '3': C('d23a2e'), '4': C('ff7a3a'), '5': C('ffd08a'),
    'W': C('ffffff'), 'r': C('ff3060'), 'g': C('2a2438'), 'y': C('fff27a'),
}

DARTER = '''
..........KKK...
......KKKK332K..
...KKK33322211K.
KKK44433222r11KK
KW5444332rry1ggK
KKK33322111r11KK
...KKK22211111K.
......KKKK111K..
..........KKK...
'''


def shade_sphere(img, cx, cy, r, ramp_cols, light=(-0.55, -0.65, 0.52), spec=None, ditherit=True, only=None):
    lx, ly, lz = light
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    for y in range(img.h):
        for x in range(img.w):
            dx = (x + 0.5 - cx) / r
            dy = (y + 0.5 - cy) / r
            d2 = dx * dx + dy * dy
            if d2 > 1.0:
                continue
            if only and not only(x, y):
                continue
            nz = math.sqrt(1 - d2)
            lam = max(0.0, dx * lx + dy * ly + nz * lz)
            v = 0.12 + 0.88 * lam
            n = len(ramp_cols)
            i = dither(v, x, y, n) if ditherit else clamp(int(v * (n - 1) + 0.5), 0, n - 1)
            c = ramp_cols[i]
            if spec and lam > 0.93:
                c = spec
            img.set(x, y, c)

def fighter(w, h, pal, nose_left=True, wing_sweep=5, wing_span=None, canopy=True, seed=1):
    # generic enemy fighter, drawn nose-right then mirrored
    cy = h // 2
    img = Img(w, h)
    span = wing_span if wing_span is not None else cy - 1
    hullc, wingc, canc, engc = pal['hull'], pal['wing'], pal['canopy'], pal['engine']
    for y in range(h):
        for x in range(w):
            dy = y - cy
            ady = abs(dy)
            top = dy < 0
            part = None
            fx0 = 2
            fx1 = w - 2
            f = 2.6 if x < w * 0.55 else 2.6 * (fx1 + 0.5 - x) / (fx1 - w * 0.55)
            if fx0 <= x <= fx1 and ady <= f:
                part = ('hull', ady / max(f, 0.5))
            if 2 <= ady <= span:
                t = (ady - 2) / max(1, span - 2)
                lead = w * 0.62 - wing_sweep * t
                trail = w * 0.30 - wing_sweep * 0.8 * t
                if trail <= x <= lead:
                    part = part or ('wing', t)
            if part is None:
                continue
            kind, v = part
            if kind == 'hull':
                lvl = len(hullc) - 1 - int(v * 2.5)
                if not top:
                    lvl -= 1
                c = hullc[clamp(lvl, 0, len(hullc) - 1)]
            else:
                lvl = len(wingc) - 2 - int(v * 2)
                if not top:
                    lvl -= 1
                c = wingc[clamp(lvl, 0, len(wingc) - 1)]
            img.set(x, y, c)
    if canopy:
        for y in range(h):
            for x in range(w):
                dx = (x - w * 0.68) / (w * 0.13)
                dyy = (y - cy + 0.2) / 1.2
                if dx * dx + dyy * dyy <= 1:
                    img.set(x, y, canc[1] if dyy < 0 else canc[0])
    img.set(1, cy, engc)
    img.set(0, cy, engc)
    img = outline(img, OUT)
    return img.flip_x() if nose_left else img

def rock(r, seed, cols, craters=3):
    size = 2 * r + 3
    img = Img(size, size)
    noise, fbm = value_noise(seed)
    cx = cy = size / 2
    rnd = random.Random(seed)
    crater_list = [(rnd.uniform(-0.5, 0.5) * r, rnd.uniform(-0.5, 0.5) * r, rnd.uniform(0.18, 0.32) * r) for _ in range(craters)]
    for y in range(size):
        for x in range(size):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            ang = math.atan2(dy, dx)
            rr = r * (0.8 + 0.35 * fbm(math.cos(ang) * 2.2 + 10, math.sin(ang) * 2.2 + 10, 3))
            d = math.sqrt(dx * dx + dy * dy)
            if d > rr:
                continue
            nx, ny = dx / rr, dy / rr
            nz = math.sqrt(max(0.0, 1 - min(1.0, nx * nx + ny * ny)))
            lam = max(0.0, -0.55 * nx - 0.6 * ny + 0.58 * nz)
            tex = fbm(x * 0.35 + seed, y * 0.35, 3)
            v = 0.1 + 0.8 * lam + (tex - 0.5) * 0.35
            for (ccx, ccy, cr) in crater_list:
                cd = math.sqrt((dx - ccx) ** 2 + (dy - ccy) ** 2)
                if cd < cr:
                    # crater: dark on the lit side, light on the far rim
                    v -= 0.25 * (1 - cd / cr)
                    if (dx - ccx) * 0.6 + (dy - ccy) * 0.6 > cr * 0.4:
                        v += 0.3
            i = dither(clamp(v, 0, 1), x, y, len(cols))
            img.set(x, y, cols[i])
    return outline(img, OUT)

def mine_sprite():
    size = 17
    img = Img(size, size)
    c = size / 2
    red = [C('3a0a14'), C('6e1424'), C('a8263a'), C('e24a5a'), C('ff9aa6')]
    for k in range(8):
        a = k / 8 * math.tau
        for t in range(9):
            rr = 4 + t * 0.5
            x = c + math.cos(a) * rr
            y = c + math.sin(a) * rr
            img.set(int(x), int(y), C('8a8aa8') if t < 7 else C('e8e8ff'))
    shade_sphere(img, c, c, 5.2, red, spec=C('ffffff'))
    img.set(int(c) - 1, int(c) - 2, C('ffe0e8'))
    return outline(img, OUT)

def pod_sprite():
    size = 21
    img = Img(size, size)
    metal = [C('1c1a2c'), C('2e2a44'), C('48446a'), C('6e6a96'), C('a09cc8'), C('d8d4f4')]
    shade_sphere(img, size / 2, size / 2, 9.4, metal, spec=C('ffffff'))
    for x in range(size):
        for y in (8, 12):
            if img.get(x, y) is not None:
                img.set(x, y, C('14121e'))
    for x in (4, 16):
        img.set(x, 10, C('ff3a3a'))
    return outline(img, OUT)

def spinner_sprite():
    size = 19
    img = Img(size, size)
    c = size / 2
    steel = [C('24283a'), C('3c4260'), C('5e668e'), C('8a94c0'), C('c4ccf0')]
    for y in range(size):
        for x in range(size):
            dx, dy = x + 0.5 - c, y + 0.5 - c
            r = math.sqrt(dx * dx + dy * dy)
            a = math.atan2(dy, dx)
            blade = (math.cos(4 * a + r * 0.28) + 1) / 2
            lim = 3 + 6.2 * blade
            if r <= lim or r < 3.2:
                v = 0.3 + 0.7 * blade * (1 - r / 10) + (0.3 if dx + dy < 0 else 0)
                img.set(x, y, steel[dither(clamp(v, 0, 1), x, y, len(steel))])
    for y in range(size):
        for x in range(size):
            dx, dy = x + 0.5 - c, y + 0.5 - c
            if dx * dx + dy * dy < 5:
                img.set(x, y, C('ffcc40') if dx + dy < 0 else C('c07010'))
    return outline(img, OUT)

def weaver_sprite():
    size = 13
    img = Img(size, size)
    g = [C('0c3a24'), C('14683a'), C('22a052'), C('4ad87a'), C('b0ffc8')]
    shade_sphere(img, size / 2, size / 2, 5.6, g, spec=C('ffffff'))
    for a in range(0, 360, 60):
        x = size / 2 + math.cos(math.radians(a)) * 5.8
        y = size / 2 + math.sin(math.radians(a)) * 5.8
        img.set(int(x), int(y), C('d0ffe0'))
    return outline(img, OUT)

def jelly_sprite():
    w, h = 25, 27
    img = Img(w, h)
    body = [C('3a0c3a'), C('6c1a6a'), C('a8309e'), C('e05ad0'), C('ff9cf0'), C('ffe0fa')]
    cx, cy = w / 2, 9.5
    for y in range(h):
        for x in range(w):
            dx, dy = (x + 0.5 - cx) / 11.5, (y + 0.5 - cy) / 9
            if dy <= 0.45 and dx * dx + dy * dy <= 1:
                v = 0.25 + 0.7 * (1 - (dx * dx + dy * dy)) - dy * 0.2
                if dx < -0.2 and dy < -0.3:
                    v += 0.25
                img.set(x, y, body[dither(clamp(v, 0, 1), x, y, len(body))])
    # frilled rim
    for x in range(1, w - 1):
        if (x % 3) != 1:
            img.set(x, 14, body[1])
    # tentacles
    for k, tx in enumerate([5, 9, 12, 15, 19]):
        for y in range(14, h - 1):
            wob = int(round(math.sin((y + k * 2) * 0.55) * 1.2))
            img.set(tx + wob, y, body[2] if y < 20 else body[1])
    # glowing organs
    for (ox, oy) in [(9, 8), (15, 8), (12, 5)]:
        img.set(ox, oy, C('ffffff'))
        img.set(ox + 1, oy, body[4])
    return outline(img, OUT)

def turret_sprite():
    art = """
....KKKKKK....
..KKccccbbKK..
.KccddccbbaaK.
.KcddccbbbaaK.
KKKKKKKKKKKKKK
KeeeeeeeeeeeeK
KffffffffffffK
KKKKKKKKKKKKKK
"""
    return from_ascii(art, {'K': OUT, 'a': C('3a3446'), 'b': C('5a5470'), 'c': C('8e88a8'), 'd': C('cac4e0'),
                           'e': C('a86a24'), 'f': C('5a3414')})

def eye_sprite():
    size = 29
    img = Img(size, size)
    stone = [C('141026'), C('241c40'), C('3a2e62'), C('584890'), C('8474c0')]
    shade_sphere(img, size / 2, size / 2, 13.2, stone, spec=None)
    # socket
    for y in range(size):
        for x in range(size):
            dx, dy = (x + 0.5 - size / 2) / 8.5, (y + 0.5 - size / 2) / 6.5
            if dx * dx + dy * dy <= 1:
                img.set(x, y, C('06040e'))
    # glowing runes around
    for k in range(6):
        a = k / 6 * math.tau + 0.3
        x = size / 2 + math.cos(a) * 11
        y = size / 2 + math.sin(a) * 11
        img.set(int(x), int(y), C('b890ff'))
    return outline(img, OUT)

def seeker_sprite():
    art = """
.........KKK....
.....KKKKyyyK...
..KKKyyKKkkyyK..
KKWykkyyyykkyyKK
KrrWyyyyyyyyyyyK
KKWykkyyyykkyyKK
..KKKyyKKkkyyK..
.....KKKKyyyK...
.........KKK....
"""
    return from_ascii(art, {'K': OUT, 'y': C('ffc828'), 'k': C('2a2018'), 'W': C('ffffff'), 'r': C('ff3030')})

def cargo_sprite():
    w, h = 19, 17
    img = Img(w, h)
    gold = [C('4a2c08'), C('8a5a10'), C('c88a1a'), C('f0b832'), C('ffe27a'), C('fff6c8')]
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            v = 0.8 - (y / h) * 0.6 + (0.15 if x < w / 2 else -0.05)
            if x in (1, w - 2) or y in (1, h - 2):
                v -= 0.3
            img.set(x, y, gold[dither(clamp(v, 0, 1), x, y, len(gold))])
    for x in range(3, w - 3):
        img.set(x, h // 2, C('7a4a0a'))
    for y in range(3, h - 3):
        img.set(w // 2, y, C('7a4a0a'))
    img.set(w // 2, h // 2, C('ffffff'))
    img.set(3, 3, C('ffffff'))
    return outline(img, OUT)

def gunship_sprite():
    w, h = 44, 28
    img = Img(w, h)
    cy = h // 2
    hull = [C('14201c'), C('22382e'), C('365a46'), C('4e8264'), C('7cb890'), C('c0f0cc')]
    for y in range(h):
        for x in range(w):
            dy = y - cy
            ady = abs(dy)
            # thick hull tapering to a blunt nose on the right (drawn nose-right, flipped later)
            f = 8.5 if x < 30 else 8.5 - (x - 30) * 0.55
            if 3 <= x <= w - 2 and ady <= f:
                v = 0.85 - ady / 10.0 - (0.18 if dy > 0 else 0)
                img.set(x, y, hull[dither(clamp(v, 0, 1), x, y, len(hull))])
            # side pods
            if 8 <= x <= 26 and 9 <= ady <= 12:
                v = 0.55 - (0.25 if dy > 0 else 0) + (0.2 if ady == 9 else 0)
                img.set(x, y, hull[dither(clamp(v, 0, 1), x, y, len(hull))])
    # plating lines & windows
    for x in range(6, 38, 6):
        for y in range(cy - 6, cy + 7):
            if img.get(x, y) is not None:
                img.set(x, y, hull[1])
    for x in range(12, 32, 4):
        img.set(x, cy - 3, C('ffb040'))
    # cannons
    for yy in (cy - 11, cy + 11):
        for x in range(26, 34):
            img.set(x, yy, C('8a8aa0'))
    # engines
    for yy in (cy - 4, cy + 4):
        for x in range(0, 4):
            img.set(x, yy, C('ff8a2a') if x < 2 else C('5a5a70'))
    img = outline(img, OUT)
    return img.flip_x()

def carrier_sprite():
    w, h = 64, 40
    img = Img(w, h)
    cy = h // 2
    hull = [C('1a1424'), C('2c2240'), C('46365e'), C('66507e'), C('9078a8'), C('c8b4e0')]
    for y in range(h):
        for x in range(w):
            dy = y - cy
            ady = abs(dy)
            f = 15 if x < 44 else 15 - (x - 44) * 0.75
            if 2 <= x <= w - 2 and ady <= f:
                v = 0.8 - ady / 20.0 - (0.2 if dy > 0 else 0)
                v += 0.08 * math.sin(x * 0.9)
                img.set(x, y, hull[dither(clamp(v, 0, 1), x, y, len(hull))])
            if 10 <= x <= 40 and 15 < ady <= 19 and (x // 5) % 2 == 0:
                img.set(x, y, hull[2] if dy < 0 else hull[1])
    # hangar bays (dark with light)
    for bx in (14, 30):
        for y in range(cy - 4, cy + 5):
            for x in range(bx, bx + 8):
                img.set(x, y, C('08060e'))
        for x in range(bx, bx + 8):
            img.set(x, cy - 4, C('ffa040'))
    # bridge
    for y in range(cy - 9, cy - 5):
        for x in range(46, 54):
            img.set(x, y, hull[4] if y < cy - 7 else hull[3])
    for x in range(47, 53, 2):
        img.set(x, cy - 8, C('7ae8ff'))
    for yy in (cy - 6, cy + 6):
        for x in range(0, 3):
            img.set(x, yy, C('ff8a2a'))
    img = outline(img, OUT)
    return img.flip_x()



def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    l2 = vx * vx + vy * vy
    t = 0 if l2 == 0 else max(0.0, min(1.0, (wx * vx + wy * vy) / l2))
    dx, dy = px - (ax + t * vx), py - (ay + t * vy)
    return math.sqrt(dx * dx + dy * dy)

LOGO_LETTERS = {
    'Q': [[(0, 1), (1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 1)], [(2.4, 4.4), (4.3, 6.5)]],
    'U': [[(0, 0), (0, 5), (1, 6), (3, 6), (4, 5), (4, 0)]],
    'A': [[(0, 6), (0, 1), (1, 0), (3, 0), (4, 1), (4, 6)], [(0, 3.3), (4, 3.3)]],
    'S': [[(4, 1), (3, 0), (1, 0), (0, 1), (0, 2), (1, 3), (3, 3), (4, 4), (4, 5), (3, 6), (1, 6), (0, 5)]],
    'R': [[(0, 6), (0, 0), (3, 0), (4, 1), (4, 2), (3, 3), (0, 3)], [(2.2, 3), (4, 6)]],
}

def logo(text='QUASAR', unit=6.2, stroke=1.25, gap=7):
    lw = int(4 * unit + stroke * unit + 2)
    lh = int(6 * unit + stroke * unit + 2)
    W = len(text) * (lw + gap) + 8
    H = lh + 10
    ss = 3
    face = Img(W, H)
    mask = [[False] * W for _ in range(H)]
    for li, ch in enumerate(text):
        ox = 4 + li * (lw + gap) + stroke * unit / 2
        oy = 3 + stroke * unit / 2
        polys = LOGO_LETTERS[ch]
        for y in range(H):
            for x in range(int(ox - stroke * unit), int(ox + lw)):
                if x < 0 or x >= W:
                    continue
                hits = 0
                for sy in range(ss):
                    for sx in range(ss):
                        px = (x + (sx + 0.5) / ss - ox) / unit
                        py = (y + (sy + 0.5) / ss - oy) / unit
                        best = 9
                        for pl in polys:
                            for k in range(len(pl) - 1):
                                d = seg_dist(px, py, pl[k][0], pl[k][1], pl[k + 1][0], pl[k + 1][1])
                                best = min(best, d)
                        if best <= stroke / 2:
                            hits += 1
                if hits * 2 >= ss * ss:
                    mask[y][x] = True
    # chrome face: bright sky above the horizon line, dark ground below
    top = [C('ffffff'), C('d8f6ff'), C('8ae0ff'), C('3aa8f0')]
    bot = [C('2a1a6a'), C('5a2aa0'), C('a040d0'), C('ff70e0')]
    for y in range(H):
        t = (y - 3) / max(1, lh)
        for x in range(W):
            if not mask[y][x]:
                continue
            if t < 0.52:
                c = ramp(top, t / 0.52)
            else:
                c = ramp(bot, (t - 0.52) / 0.48)
            face.set(x, y, c)
    # highlight the top edge of every stroke
    for y in range(1, H):
        for x in range(W):
            if mask[y][x] and not mask[y - 1][x]:
                face.set(x, y, C('ffffff'))
    # extrusion to the lower right, then outline
    ext = Img(W + 4, H + 4)
    for d in range(3, 0, -1):
        for y in range(H):
            for x in range(W):
                if mask[y][x]:
                    ext.set(x + d, y + d, C('1a0a3a') if d > 1 else C('3a1470'))
    ext.paste(face, 0, 0)
    return outline(ext, C('05030f'), diag=True)

def planet_ice():
    R = 46
    W, H = 150, 112
    cx, cy = 75, 56
    img = Img(W, H)
    noise, fbm = value_noise(77)
    cols = [C('0a1636'), C('14285a'), C('1e4284'), C('2e66a8'), C('4a90c8'), C('7ab8e0'), C('b4dcf2'), C('eaf8ff')]
    ring_cols = [C('3a5a8a'), C('6a90b8'), C('a8c8e4')]

    def ring_pix(front):
        for y in range(H):
            for x in range(W):
                dx, dy = (x - cx) / 70.0, (y - cy) / 14.0
                d = math.sqrt(dx * dx + dy * dy)
                if 0.72 < d < 1.0:
                    if front != (y > cy):
                        continue
                    band = int((d - 0.72) / 0.28 * 7)
                    if band in (2, 5):
                        continue
                    img.set(x, y, ring_cols[dither(0.3 + 0.6 * (1 - abs(d - 0.86) / 0.14), x, y, 3)])
    ring_pix(False)
    for y in range(H):
        for x in range(W):
            dx, dy = (x + 0.5 - cx) / R, (y + 0.5 - cy) / R
            d2 = dx * dx + dy * dy
            if d2 > 1:
                continue
            nz = math.sqrt(1 - d2)
            lam = max(0.0, -0.7 * dx - 0.35 * dy + 0.6 * nz)
            lat = dy * 3.2 + fbm(x * 0.05, y * 0.4, 3) * 1.4
            band = 0.5 + 0.5 * math.sin(lat * 2.4)
            v = 0.05 + lam * (0.72 + 0.28 * band)
            if d2 > 0.86:
                v += 0.25 * lam    # thin bright atmosphere on the lit limb
            img.set(x, y, cols[dither(clamp(v, 0, 1), x, y, len(cols))])
    ring_pix(True)
    return img

def crystal(img, cx, cy, ang, r0, r1, w, cols, tip=0.35):
    ca, sa = math.cos(ang), math.sin(ang)
    L = r1 - r0
    for y in range(img.h):
        for x in range(img.w):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            u = dx * ca + dy * sa - r0          # along the crystal
            v = -dx * sa + dy * ca              # across
            if u < 0 or u > L:
                continue
            half = w
            if u > L * (1 - tip):
                half = w * (L - u) / (L * tip)
            if abs(v) > half:
                continue
            face = 0.75 if v < 0 else 0.35
            face += 0.2 * (u / L)
            if abs(v) > half - 1:
                face -= 0.25
            img.set(x, y, cols[dither(clamp(face, 0, 1), x, y, len(cols))])

def glacier():
    W, H = 78, 86
    img = Img(W, H)
    cols = [C('0c1c3c'), C('1a3a6e'), C('2c62a0'), C('4a92cc'), C('84c4ec'), C('d4f0ff')]
    cx, cy = 42, 43
    rnd = random.Random(4)
    spikes = [(math.pi, 36, 7), (math.pi * 0.82, 30, 6), (math.pi * 1.18, 30, 6), (math.pi * 0.62, 26, 6),
              (math.pi * 1.38, 26, 6), (math.pi * 0.4, 22, 6), (math.pi * 1.6, 22, 6), (0.15, 20, 7), (-0.15, 20, 7),
              (math.pi * 0.25, 18, 5), (math.pi * 1.75, 18, 5)]
    for ang, L, w in sorted(spikes, key=lambda s: -s[1]):
        crystal(img, cx, cy, ang, 4, L + rnd.randint(0, 5), w, cols)
    # dark core socket
    for y in range(H):
        for x in range(W):
            dx, dy = x + 0.5 - (cx - 2), y + 0.5 - cy
            if dx * dx + dy * dy < 13 * 13:
                img.set(x, y, C('06101e') if dx * dx + dy * dy < 11 * 11 else C('2a5a90'))
    return outline(img, OUT)

def shard():
    img = Img(13, 25)
    cols = [C('10244a'), C('24508a'), C('4a8cc8'), C('9ad4f4'), C('eaffff')]
    crystal(img, 6.5, 24.5, -math.pi / 2, 0, 24, 5, cols, tip=0.3)
    # bottom point too
    for y in range(20, 25):
        for x in range(13):
            if img.get(x, y) is not None and abs(x - 6) > (24 - y):
                img.set(x, y, None)
    return outline(img, OUT)

def hydra_head():
    W, H = 38, 30
    img = Img(W, H)
    skin = [C('2a0626'), C('4e0c46'), C('7a1a68'), C('b02c8e'), C('e05ab4'), C('ffa0dc')]
    for y in range(H):
        for x in range(W):
            # skull (right) + snout (left), facing left
            dx1, dy1 = (x - 24) / 13.0, (y - 14) / 11.0
            dx2, dy2 = (x - 11) / 11.0, (y - 16) / 6.5
            in1 = dx1 * dx1 + dy1 * dy1 <= 1
            in2 = dx2 * dx2 + dy2 * dy2 <= 1
            if not (in1 or in2):
                continue
            ny = dy1 if in1 else dy2
            v = 0.75 - ny * 0.45 + (0.1 if x > 20 else 0)
            img.set(x, y, skin[dither(clamp(v, 0, 1), x, y, len(skin))])
    # jaw line & mouth glow
    for x in range(2, 19):
        img.set(x, 18, C('1a0418'))
    for x in range(3, 12):
        img.set(x, 19, C('ff6aa0'))
    # crest spikes
    for k, sx in enumerate([22, 27, 32]):
        for t in range(6):
            img.set(sx + t // 2, 4 - t + (k % 2), C('ffd0f0') if t > 3 else skin[3])
    # eye
    for (ex, ey) in [(17, 11), (18, 11), (17, 12), (18, 12)]:
        img.set(ex, ey, C('fff2a0'))
    img.set(17, 11, C('ffffff'))
    return outline(img, OUT)

def hydra_seg():
    size = 19
    img = Img(size, size)
    skin = [C('240420'), C('440a3c'), C('6e145e'), C('a02488'), C('d850b0'), C('ff9ad8')]
    shade_sphere(img, size / 2, size / 2, 7.5, skin, spec=C('ffe0f4'))
    for k in range(6):
        a = k / 6 * math.tau
        for t in range(3):
            x = size / 2 + math.cos(a) * (7 + t)
            y = size / 2 + math.sin(a) * (7 + t)
            img.set(int(x), int(y), C('ffc0e8') if t == 2 else skin[2])
    return outline(img, OUT)

def satellite():
    size = 17
    img = Img(size, size)
    metal = [C('1c1636'), C('302a5a'), C('4c4488'), C('7a70bc'), C('b8b0f0')]
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        for t in range(9):
            x = size / 2 + math.cos(a) * t
            y = size / 2 + math.sin(a) * t
            img.set(int(x), int(y), metal[3] if t < 7 else C('ff70e0'))
    shade_sphere(img, size / 2, size / 2, 4.6, metal, spec=C('ffffff'))
    img.set(8, 8, C('ff4ad0'))
    return outline(img, OUT)


# ---------------------------------------------------------------- bullets & shots

def orb(r, core, mid, edge, rim=None):
    # glowing orb bullet: white core, coloured body, dark rim
    size = 2 * r + 1
    img = Img(size, size)
    for y in range(size):
        for x in range(size):
            dx, dy = x - r, y - r
            d = math.sqrt(dx * dx + dy * dy) / (r + 0.5)
            if d > 1.0:
                continue
            if d < 0.35:
                c = core
            elif d < 0.7:
                c = mid
            else:
                c = edge
            img.set(x, y, c)
    return img

def shot_bolt(length, h, body, hot, tail):
    img = Img(length, h)
    mid = h // 2
    for x in range(length):
        t = x / (length - 1)
        for y in range(h):
            dy = abs(y - mid)
            if dy == 0:
                c = hot if t > 0.35 else body
            elif dy == 1 and h > 2:
                if t < 0.2:
                    continue
                c = body if t > 0.5 else tail
            else:
                continue
            img.set(x, y, c)
    return img

def needle(length, col, hot):
    img = Img(length, 3)
    for x in range(length):
        img.set(x, 1, hot if x > length // 3 else col)
        if 1 <= x < length - 1:
            img.set(x, 0, col)
            img.set(x, 2, col)
    img.set(length - 1, 1, (255, 255, 255))
    return img

# ---------------------------------------------------------------- pickups

def capsule(body, light, dark):
    art = '''
..KKKKKKKK..
.KllllllllK.
KlWbbbbbbblK
KlbbbbbbbbdK
KlbbbbbbbbdK
KlbbbbbbbbdK
KlbbbbbbbbdK
KlbbbbbbbbdK
KlbbbbbbbbdK
KdbbbbbbbddK
.KddddddddK.
..KKKKKKKK..
'''
    return from_ascii(art, {'K': OUT, 'l': light, 'b': body, 'd': dark, 'W': (255, 255, 255)})

def gem(frame):
    # rotating crystal: 4 frames by narrowing width
    widths = [5, 4, 2, 4]
    w = widths[frame]
    img = Img(9, 11)
    cx = 4
    pal = [C('0a3a5a'), C('1d8fd0'), C('5fe0ff'), C('e0ffff')]
    for y in range(11):
        t = y / 10.0
        half = w * (1 - abs(t - 0.4) / 0.6) if t < 1 else 0
        half = max(0, half)
        for x in range(9):
            dx = x - cx
            if abs(dx) <= half + 0.01:
                shade = 2 if dx <= 0 else 1
                if y < 4 and dx <= 0:
                    shade = 3
                if abs(dx) > half - 1:
                    shade = 0
                img.set(x, y, pal[shade])
    img.set(cx - 1 if w > 2 else cx, 2, (255, 255, 255))
    return img

# ---------------------------------------------------------------- effects

def fireball_frames(n=8, size=32, seed=7):
    noise, fbm = value_noise(seed)
    frames = []
    ramp_cols = [C('ffffff'), C('fff5a0'), C('ffc23a'), C('ff7a1e'), C('d8341e'), C('7a1420'), C('2a0c18')]
    r_max = size / 2 - 1
    for f in range(n):
        t = f / (n - 1)
        img = Img(size, size)
        rad = r_max * (0.35 + 0.65 * math.sqrt(t))
        heat = 1.0 - t
        for y in range(size):
            for x in range(size):
                dx, dy = x - size / 2 + 0.5, y - size / 2 + 0.5
                d = math.sqrt(dx * dx + dy * dy)
                nv = fbm(x * 0.22 + f * 1.7, y * 0.22 + f * 0.9, 3)
                edge = rad * (0.75 + 0.5 * nv)
                if d > edge:
                    continue
                k = d / edge
                # temperature: hot centre fading with time, noise breaks it up
                temp = (1 - k) * (0.4 + 0.9 * heat) + (nv - 0.5) * 0.5
                if t > 0.55 and nv < 0.35 + (t - 0.55) * 1.2:
                    continue        # break into smoke holes late
                idx = clamp(int((1 - temp) * (len(ramp_cols) - 1) + 0.5), 0, len(ramp_cols) - 1)
                img.set(x, y, ramp_cols[idx])
        frames.append(img)
    return frames

def smoke_frames(n=6, size=16, seed=11):
    noise, fbm = value_noise(seed)
    frames = []
    cols = [C('5a5470'), C('433e58'), C('2e2a40'), C('1e1a2c')]
    for f in range(n):
        t = f / (n - 1)
        img = Img(size, size)
        rad = size / 2 * (0.4 + 0.6 * t)
        for y in range(size):
            for x in range(size):
                dx, dy = x - size / 2 + 0.5, y - size / 2 + 0.5
                d = math.sqrt(dx * dx + dy * dy)
                nv = fbm(x * 0.3 + f, y * 0.3, 3)
                if d > rad * (0.7 + 0.5 * nv):
                    continue
                if nv < t * 0.7:
                    continue
                shade = clamp(int((d / rad) * 3 + t * 1.5), 0, 3)
                if dither(nv, x, y, 2) == 0 and shade < 3:
                    shade += 1
                img.set(x, y, cols[shade])
        frames.append(img)
    return frames

# ---------------------------------------------------------------- icons

ICON_SHIP = '''
..KK....
.K43KK..
KKK33hKK
.K21KKK.
..KK....
'''

ICON_BOMB = '''
..KKK..
.KyyyK.
KyWyooK
KyyooRK
KyooRRK
.KoRRK.
..KKK..
'''

def build():
    sl = []
    anims = []

    def add(name, img, ox=None, oy=None, outline_col=None):
        if outline_col is not None:
            img = outline(img, outline_col)
        if ox is None:
            ox = img.w // 2
        if oy is None:
            oy = img.h // 2
        sl.append((name, img, ox, oy))
        return img

    add('player', player_ship(0), 17, 10)
    add('player_up', player_ship(-1), 17, 10)
    add('player_dn', player_ship(1), 17, 10)
    add('drone', from_ascii(DRONE, EN_METAL))
    add('darter', from_ascii(DARTER, EN_RED))
    fighter_pal = {'hull': [C('2a0c2e'), C('4e1656'), C('7a2486'), C('ae3cb8'), C('e070e8'), C('ffc0ff')],
                   'wing': [C('1e0a24'), C('3a1244'), C('5e1e6a'), C('86309a'), C('b458c8')],
                   'canopy': [C('a83a10'), C('ffb040')], 'engine': C('ff8a2a')}
    add('fighter', fighter(24, 17, fighter_pal, wing_sweep=6))
    add('pod', pod_sprite())
    add('mine', mine_sprite())
    ice = [C('0e1430'), C('1c2c5a'), C('32508e'), C('5a86c4'), C('98c4ec'), C('e4f6ff')]
    add('rock_l', rock(14, 3, ice, 4))
    add('rock_m', rock(9, 5, ice, 2))
    add('rock_s', rock(5, 9, ice, 1))
    add('gunship', gunship_sprite())
    add('carrier', carrier_sprite())
    add('spinner', spinner_sprite())
    add('weaver', weaver_sprite())
    add('jelly', jelly_sprite())
    add('turret', turret_sprite(), None, 5)
    add('eye', eye_sprite())
    add('seeker', seeker_sprite())
    add('cargo', cargo_sprite())
    lg = logo()
    add('logo', lg, lg.w // 2, lg.h // 2)
    add('planet_ice', planet_ice())
    add('glacier', glacier(), 44, 44)
    add('shard', shard())
    add('hydra_head', hydra_head(), 19, 15)
    add('hydra_seg', hydra_seg())
    add('satellite', satellite())



    # player shots
    add('shot_pulse', shot_bolt(10, 3, C('36c8ff'), C('ffffff'), C('1860c0')), 5, 1)
    add('shot_spread', shot_bolt(7, 3, C('ff4ad8'), C('ffe0ff'), C('8a1a8a')), 3, 1)
    add('missile', from_ascii('''
.KK.....
KrrKKKK.
KoWWggGK
KrrKKKK.
.KK.....
''', {'K': OUT, 'r': C('ff5a2a'), 'o': C('ffe07a'), 'W': C('e8e8ff'), 'g': C('8a8aa8'), 'G': C('ff3050')}), 4, 2)

    # enemy bullets: pink/orange/blue families
    add('eb_s_pink', orb(3, C('ffffff'), C('ff7ae0'), C('d0209a')))
    add('eb_m_pink', orb(5, C('ffffff'), C('ff7ae0'), C('c01890')))
    add('eb_l_pink', orb(7, C('ffffff'), C('ff8ae8'), C('b0107e')))
    add('eb_s_orng', orb(3, C('ffffff'), C('ffc05a'), C('e0561a')))
    add('eb_m_orng', orb(5, C('ffffff'), C('ffc05a'), C('d8461a')))
    add('eb_s_blue', orb(3, C('ffffff'), C('8ae8ff'), C('1a82e0')))
    add('eb_m_blue', orb(5, C('ffffff'), C('8ae8ff'), C('1870d8')))
    add('eb_needle', needle(9, C('ffb020'), C('fff5b0')), 4, 1)

    # pickups
    add('cap_red', capsule(C('e8263e'), C('ff8a9a'), C('8a0e22')))
    add('cap_blue', capsule(C('1e7ae8'), C('8ac8ff'), C('0e3a8a')))
    add('cap_green', capsule(C('1ec85a'), C('8affb0'), C('0a6a2e')))
    add('cap_gold', capsule(C('f0a818'), C('ffe08a'), C('8a5a0a')))
    add('cap_violet', capsule(C('a040f0'), C('dca8ff'), C('4a1080')))
    for i in range(4):
        add('gem%d' % i, gem(i))
    anims.append(('gem', ['gem%d' % i for i in range(4)]))

    for i, fr in enumerate(fireball_frames()):
        add('fire%d' % i, fr)
    anims.append(('fire', ['fire%d' % i for i in range(8)]))
    for i, fr in enumerate(smoke_frames()):
        add('smoke%d' % i, fr)
    anims.append(('smoke', ['smoke%d' % i for i in range(6)]))

    icon_leg = {'K': OUT, '4': C('78a8f2'), '3': C('4676cc'), '2': C('2c4a90'), '1': C('1a2856'), 'h': C('b0f4ff'),
                'y': C('fff27a'), 'W': C('ffffff'), 'o': C('ff9a2a'), 'R': C('d8341e')}
    add('icon_ship', from_ascii(ICON_SHIP, icon_leg), 0, 0)
    add('icon_bomb', from_ascii(ICON_BOMB, icon_leg), 0, 0)

    return sl, anims
