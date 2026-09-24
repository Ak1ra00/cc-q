#
# QUASAR - tiny pixel art toolkit used by gen_assets.py
#
# Images are lists of rows of (r, g, b, a) tuples; a is 0 or 255.
#
import math, random

def hexrgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

def to565(rgb):
    r, g, b = rgb
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)

def from565(v):
    r = (v >> 11) & 0x1f
    g = (v >> 5) & 0x3f
    b = v & 0x1f
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)

def swap16(v):
    return ((v >> 8) & 0xff) | ((v & 0xff) << 8)

def clamp(v, lo=0, hi=255):
    return lo if v < lo else hi if v > hi else v

def lerp(a, b, t):
    return a + (b - a) * t

def mix(c1, c2, t):
    return tuple(int(round(lerp(c1[i], c2[i], t))) for i in range(3))

def ramp(colors, t):
    # piecewise linear ramp through a list of rgb colours, t in 0..1
    t = max(0.0, min(1.0, t))
    n = len(colors) - 1
    p = t * n
    i = min(int(p), n - 1)
    return mix(colors[i], colors[i + 1], p - i)

class Img:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[None] * w for _ in range(h)]

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y][x]
        return None

    def set(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = rgb

    def copy(self):
        o = Img(self.w, self.h)
        o.px = [list(r) for r in self.px]
        return o

    def flip_x(self):
        o = Img(self.w, self.h)
        o.px = [list(reversed(r)) for r in self.px]
        return o

    def flip_y(self):
        o = Img(self.w, self.h)
        o.px = [list(r) for r in reversed(self.px)]
        return o

    def paste(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                c = other.px[y][x]
                if c is not None:
                    self.set(ox + x, oy + y, c)

    def bbox(self):
        xs = [x for y in range(self.h) for x in range(self.w) if self.px[y][x] is not None]
        ys = [y for y in range(self.h) for x in range(self.w) if self.px[y][x] is not None]
        if not xs:
            return (0, 0, 1, 1)
        return (min(xs), min(ys), max(xs) + 1, max(ys) + 1)

    def crop(self, x0, y0, x1, y1):
        o = Img(x1 - x0, y1 - y0)
        for y in range(y0, y1):
            for x in range(x0, x1):
                o.px[y - y0][x - x0] = self.get(x, y)
        return o

def from_ascii(art, legend):
    rows = [r for r in art.strip('\n').split('\n')]
    # strip common left indentation
    ind = min(len(r) - len(r.lstrip(' ')) for r in rows if r.strip())
    rows = [r[ind:] for r in rows]
    w = max(len(r) for r in rows)
    img = Img(w, len(rows))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch in '. ':
                continue
            if ch not in legend:
                raise KeyError('no colour for %r in %r' % (ch, r))
            img.px[y][x] = legend[ch]
    return img

def outline(img, col, diag=False, pad=1):
    # add an outline around opaque pixels (grows image by pad on all sides)
    o = Img(img.w + 2 * pad, img.h + 2 * pad)
    o.paste(img, pad, pad)
    res = o.copy()
    nb = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diag:
        nb += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
    for y in range(o.h):
        for x in range(o.w):
            if o.px[y][x] is not None:
                continue
            for dx, dy in nb:
                if o.get(x + dx, y + dy) is not None:
                    res.px[y][x] = col
                    break
    return res

def scale_img(img, k):
    o = Img(img.w * k, img.h * k)
    for y in range(img.h):
        for x in range(img.w):
            c = img.px[y][x]
            if c is not None:
                for j in range(k):
                    for i in range(k):
                        o.px[y * k + j][x * k + i] = c
    return o

BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]

def dither(v, x, y, levels):
    # ordered dither of v (0..1) into 0..levels-1
    t = (BAYER4[y & 3][x & 3] + 0.5) / 16.0
    p = v * (levels - 1)
    i = int(math.floor(p))
    if p - i > t:
        i += 1
    return max(0, min(levels - 1, i))

def value_noise(seed):
    rnd = random.Random(seed)
    table = [rnd.random() for _ in range(256 * 256)]

    def smooth(t):
        return t * t * (3 - 2 * t)

    def n(x, y, period=256):
        x0 = int(math.floor(x)) % period
        y0 = int(math.floor(y)) % period
        fx, fy = x - math.floor(x), y - math.floor(y)
        x1, y1 = (x0 + 1) % period, (y0 + 1) % period
        a = table[y0 * 256 + x0]
        b = table[y0 * 256 + x1]
        c = table[y1 * 256 + x0]
        d = table[y1 * 256 + x1]
        sx, sy = smooth(fx), smooth(fy)
        return lerp(lerp(a, b, sx), lerp(c, d, sx), sy)

    def fbm(x, y, octaves=4, period=256):
        v, amp, tot, f = 0.0, 1.0, 0.0, 1.0
        for _ in range(octaves):
            v += amp * n(x * f, y * f, period)
            tot += amp
            amp *= 0.5
            f *= 2.0
        return v / tot

    return n, fbm

def quantize(img, max_colors=255):
    # returns (index rows, palette list of 565 values); palette[0] is transparent
    pal = []
    lut = {}
    idx = []
    for y in range(img.h):
        row = []
        for x in range(img.w):
            c = img.px[y][x]
            if c is None:
                row.append(0)
                continue
            v = to565(c)
            if v not in lut:
                lut[v] = len(pal) + 1
                pal.append(v)
            row.append(lut[v])
        idx.append(row)
    if len(pal) > max_colors:
        raise ValueError('too many colours: %d' % len(pal))
    return idx, [0] + pal
