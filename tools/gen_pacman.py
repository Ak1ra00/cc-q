#!/usr/bin/env python3
#
# QUASAR - builds game/pac_gen.c and game/pac_gen.h for PAC-MAN:
# the mazes, their neon walls, and the neon logo.
#
# The mazes are drawn below as text, left half only (they are mirrored). The walls
# are rendered here once, as light: a thin tube a little inside every wall edge,
# anti-aliased, with rounded corners and a soft glow around it. The result is cut
# into 8x8 tiles of 16 brightness levels and the tiles are de-duplicated, so the
# device only stamps tiles each frame (the colour comes from a table at run time).
#
#     python3 tools/gen_pacman.py [--preview sheet.png]
#
import sys, os, math, argparse
sys.path.insert(0, os.path.dirname(__file__))
import gen_font

W, H = 38, 27           # tiles
TILE = 8

# '#' wall  '.' dot  'o' power pellet  ' ' empty corridor  't' tunnel (no dots, ghosts slow)
# '-' ghost house door  'h' inside the ghost house. Every maze shares the house and the
# ring of empty corridor around it (rows 9-15), the start row (17) and the tunnel ends.
MAZES = [
    [
        "###################",
        "#...........#.....#",
        "#.###.#####.#.###.#",
        "#o###.#####.#.###.#",
        "#..................",
        "#.###.##.######.###",
        "#.###.##.######.###",
        "#.....##...........",
        "#####.#####.###.###",
        "#####.#####.##     ",
        "#####.#####.## ###-",
        "#####.#####.## #hhh",
        "ttttt.......   #hhh",
        "#####.#####.## #hhh",
        "#####.#####.## ####",
        "#####.#####.##     ",
        "#####.#####.###.###",
        "#..................",
        "#.###.##.######.###",
        "#.###.##.######.###",
        "#o.................",
        "###.###.####.###.##",
        "###.###.####.###.##",
        "#..................",
        "#.######.###.####.#",
        "#..................",
        "###################",
    ],
    [
        "###################",
        "#......#..........#",
        "#.####.#.########.#",
        "#o####...########.#",
        "ttt....#...........",
        "#.###.##.######.###",
        "#.###.##.######.###",
        "#.....##...........",
        "#.###.#####.###.###",
        "#.###.#####.##     ",
        "#.....#####.## ###-",
        "#.###.#####.## #hhh",
        "#.###.......   #hhh",
        "#.###.#####.## #hhh",
        "#.....#####.## ####",
        "#.###.#####.##     ",
        "#.###.#####.###.###",
        "#..................",
        "#.####.#.#####.#.##",
        "#.####.#.#####.#.##",
        "ttt.......#........",
        "#o###.###.#.####.##",
        "#.###.###.#.####.##",
        "#..................",
        "#.######.###.####.#",
        "#..................",
        "###################",
    ],
    [
        "###################",
        "#....#......#.....#",
        "#.##.#.####.#.###.#",
        "#o##.#.####.#.###.#",
        "#.##.#.####.#.###.#",
        "#..................",
        "###.####.##.###.###",
        "###.####.##.###.###",
        "#...........###.###",
        "#.###.###.#.##     ",
        "#.###.###.#.## ###-",
        "#.###.###.#.## #hhh",
        "ttt.........   #hhh",
        "#.###.###.#.## #hhh",
        "#.###.###.#.## ####",
        "#.###.###.#.##     ",
        "#.###.###.#.###.###",
        "#..................",
        "#.##.####.#.####.##",
        "#.##.####.#.####.##",
        "#o.................",
        "###.#.##.####.##.##",
        "###.#.##.####.##.##",
        "#...#..............",
        "#.#####.####.####.#",
        "#..................",
        "###################",
    ],
]


def full(left):
    rows = []
    for r in left:
        assert len(r) == 19, r
        rows.append(r + r[::-1])
    assert len(rows) == H
    return rows


def check(rows, name):
    # corridors one tile wide, every one connected, no dead ends
    walk = lambda c: c in '. ot'
    g = rows
    for y in range(H):
        for x in range(W):
            if not walk(g[y][x]):
                continue
            n = sum(walk(g[(y + dy)][(x + dx) % W]) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= y + dy < H)
            assert n >= 2, '%s: dead end at %d,%d' % (name, x, y)
            if x + 1 < W and y + 1 < H:
                assert not all(walk(g[y + a][x + b]) for a in (0, 1) for b in (0, 1)), '%s: open 2x2 at %d,%d' % (name, x, y)
    cells = [(x, y) for y in range(H) for x in range(W) if walk(g[y][x])]
    seen, todo = {cells[0]}, [cells[0]]
    while todo:
        x, y = todo.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            p = ((x + dx) % W, y + dy)
            if 0 <= p[1] < H and walk(g[p[1]][p[0]]) and p not in seen:
                seen.add(p)
                todo.append(p)
    assert len(seen) == len(cells), '%s: not all connected' % name
    assert g[17][18] in '. ' and g[17][19] in '. ', name + ': the start row'
    assert g[10][18] == '-' and g[10][19] == '-', name + ': the house door'


# ------------------------------------------------------------------ light

def gauss_kernel(sigma):
    r = int(math.ceil(sigma * 3))
    k = [math.exp(-(i * i) / (2 * sigma * sigma)) for i in range(-r, r + 1)]
    s = sum(k)
    return r, [v / s for v in k]


def blur(img, w, h, sigma):
    # separable gaussian, edges repeated
    r, k = gauss_kernel(sigma)
    tmp = [0.0] * (w * h)
    for y in range(h):
        row = img[y * w:(y + 1) * w]
        for x in range(w):
            acc = 0.0
            for i in range(-r, r + 1):
                xx = min(w - 1, max(0, x + i))
                acc += row[xx] * k[i + r]
            tmp[y * w + x] = acc
    out = [0.0] * (w * h)
    for x in range(w):
        for y in range(h):
            acc = 0.0
            for i in range(-r, r + 1):
                yy = min(h - 1, max(0, y + i))
                acc += tmp[yy * w + x] * k[i + r]
            out[y * w + x] = acc
    return out


def render_maze(rows):
    pw, ph = W * TILE, H * TILE
    mask = [0.0] * (pw * ph)
    for y in range(ph):
        for x in range(pw):
            if rows[y // TILE][x // TILE] == '#':
                mask[y * pw + x] = 1.0
    b = blur(mask, pw, ph, 1.6)
    # the tube: where the blurred wall reaches 0.9, about a pixel and a half in
    tube = [max(0.0, 1.0 - abs(v - 0.9) / 0.07) if m else 0.0 for v, m in zip(b, mask)]
    glow = blur(tube, pw, ph, 2.2)
    out = []
    for t, g in zip(tube, glow):
        v = min(1.0, t + 3.4 * g)
        out.append(int(round(15 * (v ** 0.85))))
    return out


def text_mask(text, scale, pad):
    glyphs = gen_font.parse(gen_font.GLYPHS)
    cw = 6 * scale
    w = len(text) * cw - scale + 2 * pad
    h = 7 * scale + 2 * pad
    m = [0.0] * (w * h)
    for i, ch in enumerate(text):
        rows = glyphs[ord(ch)] + ['.....'] * 7
        for r in range(7):
            for c in range(5):
                if rows[r][c] == '#':
                    for yy in range(scale):
                        for xx in range(scale):
                            m[(pad + r * scale + yy) * w + pad + i * cw + c * scale + xx] = 1.0
    return m, w, h


def render_logo(text, scale):
    # filled letters with rounded corners, a bright rim, and a glow around
    pad = scale * 2 + 4
    m, w, h = text_mask(text, scale, pad)
    soft = blur(m, w, h, scale * 0.45)
    fill = [max(0.0, min(1.0, (v - 0.5) * 6 + 0.5)) for v in soft]
    rim = [max(0.0, 1.0 - abs(v - 0.5) / 0.12) for v in soft]
    glow = blur(fill, w, h, scale * 0.9 + 1.0)
    out = []
    for f, r, g in zip(fill, rim, glow):
        if r > 0.45:
            v = 15
        elif f > 0.5:
            v = 12 + int(round(2 * f))
        else:
            v = int(round(9 * min(1.0, g * 1.8) ** 0.8))
        out.append(min(15, v))
    # rows 12-14 are the letters' fill: shade them top to bottom
    return out, w, h


# ------------------------------------------------------------------ output

def pack4(vals):
    return [(vals[i] << 4) | vals[i + 1] for i in range(0, len(vals), 2)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--preview')
    args = ap.parse_args()
    root = os.path.join(os.path.dirname(__file__), '..', 'game')

    mazes = [full(m) for m in MAZES]
    for i, m in enumerate(mazes):
        check(m, 'maze %d' % i)

    tiles = {(0,) * 64: 0}
    tile_list = [(0,) * 64]
    maps = []
    images = []
    for m in mazes:
        img = render_maze(m)
        images.append(img)
        pw = W * TILE
        tm = []
        for ty in range(H):
            row = []
            for tx in range(W):
                t = tuple(img[(ty * TILE + y) * pw + tx * TILE + x] for y in range(TILE) for x in range(TILE))
                if t not in tiles:
                    tiles[t] = len(tile_list)
                    tile_list.append(t)
                row.append(tiles[t])
            tm.append(row)
        maps.append(tm)
    assert len(tile_list) < 65536

    # the same walls at half size, for the home screen's card: each 2x2 block
    # keeps most of its brightest pixel, so the thin tubes stay lit
    mini = {(0,) * 16: 0}
    mini_list = [(0,) * 16]
    mini_maps = []
    for img in images:
        pw = W * TILE
        tm = []
        for ty in range(H):
            row = []
            for tx in range(W):
                t = []
                for y in range(4):
                    for x in range(4):
                        px, py = tx * TILE + x * 2, ty * TILE + y * 2
                        blk = [img[(py + a) * pw + px + b] for a in (0, 1) for b in (0, 1)]
                        t.append(min(15, int(round(max(blk) * 0.75 + sum(blk) / 4 * 0.35))))
                t = tuple(t)
                if t not in mini:
                    mini[t] = len(mini_list)
                    mini_list.append(t)
                row.append(mini[t])
            tm.append(row)
        mini_maps.append(tm)
    assert len(mini_list) < 256

    logos = [('big', 'PAC-MAN', 5), ('small', 'PAC-MAN', 2)]
    logo_imgs = [(name, render_logo(text, s)) for name, text, s in logos]

    c = ['// generated by tools/gen_pacman.py -- do not edit', '#include "pac_gen.h"', '']
    c.append('const char PAC_MAZE[PAC_MAZES][PM_H][PM_W + 1] = {')
    for m in mazes:
        c.append('    {')
        for r in m:
            c.append('        "%s",' % r)
        c.append('    },')
    c.append('};')
    c.append('')
    c.append('// %d wall tiles, 8x8, two pixels a byte, high nibble first' % len(tile_list))
    c.append('const uint8_t PAC_TILES[%d][32] = {' % len(tile_list))
    for t in tile_list:
        c.append('    { %s },' % ', '.join('0x%02x' % v for v in pack4(list(t))))
    c.append('};')
    c.append('')
    c.append('const uint16_t PAC_TILEMAP[PAC_MAZES][PM_H][PM_W] = {')
    for tm in maps:
        c.append('    {')
        for row in tm:
            c.append('        { %s },' % ', '.join(str(v) for v in row))
        c.append('    },')
    c.append('};')
    c.append('')
    c.append('// the half-size walls: %d tiles, 4x4' % len(mini_list))
    c.append('const uint8_t PAC_MINI_TILES[%d][8] = {' % len(mini_list))
    for t in mini_list:
        c.append('    { %s },' % ', '.join('0x%02x' % v for v in pack4(list(t))))
    c.append('};')
    c.append('')
    c.append('const uint8_t PAC_MINI_MAP[PAC_MAZES][PM_H][PM_W] = {')
    for tm in mini_maps:
        c.append('    {')
        for row in tm:
            c.append('        { %s },' % ', '.join(str(v) for v in row))
        c.append('    },')
    c.append('};')
    for name, (img, w, h) in logo_imgs:
        if w % 2:
            img = [v for y in range(h) for v in img[y * w:(y + 1) * w] + [0]]
            w += 1
        c.append('')
        c.append('static const uint8_t logo_%s[%d] = {' % (name, w * h // 2))
        data = pack4(img)
        for i in range(0, len(data), 24):
            c.append('    %s,' % ', '.join('0x%02x' % v for v in data[i:i + 24]))
        c.append('};')
        c.append('const pac_image_t PAC_LOGO_%s = { %d, %d, logo_%s };' % (name.upper(), w, h, name))
    open(os.path.join(root, 'pac_gen.c'), 'w').write('\n'.join(c) + '\n')

    hdr = ['// generated by tools/gen_pacman.py -- do not edit', '#pragma once', '#include <stdint.h>', '',
           '#define PAC_MAZES   %d' % len(mazes), '#define PM_W        %d' % W, '#define PM_H        %d' % H,
           '#define PAC_TILE_N  %d' % len(tile_list), '#define PAC_MINI_N  %d' % len(mini_list), '',
           '// a 4-bit brightness image, two pixels a byte, high nibble first',
           'typedef struct { uint16_t w, h; const uint8_t *pix; } pac_image_t;', '',
           'extern const char PAC_MAZE[PAC_MAZES][PM_H][PM_W + 1];',
           'extern const uint8_t PAC_TILES[PAC_TILE_N][32];',
           'extern const uint16_t PAC_TILEMAP[PAC_MAZES][PM_H][PM_W];',
           'extern const uint8_t PAC_MINI_TILES[PAC_MINI_N][8];',
           'extern const uint8_t PAC_MINI_MAP[PAC_MAZES][PM_H][PM_W];',
           'extern const pac_image_t PAC_LOGO_BIG, PAC_LOGO_SMALL;']
    open(os.path.join(root, 'pac_gen.h'), 'w').write('\n'.join(hdr) + '\n')
    print('mini tiles %d' % len(mini_list))
    print('mazes %d, unique tiles %d (%d bytes), logos %s' % (len(mazes), len(tile_list), len(tile_list) * 32,
          ', '.join('%dx%d' % (w, h) for _, (img, w, h) in logo_imgs)))

    if args.preview:
        from PIL import Image
        cols = [(40, 110, 255), (220, 60, 190), (40, 210, 110)]
        pw, ph = W * TILE, H * TILE
        sheet = Image.new('RGB', (pw * 3 + 20, ph + 80), (4, 4, 12))
        for i, img in enumerate(images):
            core = cols[i]
            for y in range(ph):
                for x in range(pw):
                    v = img[y * pw + x] / 15.0
                    col = tuple(min(255, int(core[k] * v * 1.3 + (255 - core[k]) * max(0.0, v - 0.75) * 3)) for k in range(3))
                    sheet.putpixel((i * (pw + 10) + x, y), col)
        for y in range(H * 4):
            for x in range(W * 4):
                t = mini_list[mini_maps[0][y // 4][x // 4]][(y % 4) * 4 + x % 4] / 15.0
                core = cols[0]
                col = tuple(min(255, int(core[k] * t * 1.3 + (255 - core[k]) * max(0.0, t - 0.75) * 3)) for k in range(3))
                sheet.putpixel((300 + x, ph + 8 + y), col) if ph + 8 + y < ph + 80 else None
        img, w, h = logo_imgs[0][1]
        for y in range(h):
            for x in range(w):
                v = img[y * w + x] / 15.0
                sheet.putpixel((10 + x, ph + 8 + y), (int(255 * v), int(215 * v), int(40 * v)))
        sheet.save(args.preview)


if __name__ == '__main__':
    main()
