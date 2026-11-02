# Original 16x16 pixel-art icons for Ganondorf's Dark Arts (made from scratch, no game assets).
# Writes a C++ header with each icon as 16 strings of palette letters, and a preview PNG.
import math, sys
from PIL import Image

N = 16
PAL = {
    '.': None,
    'k': (12, 8, 18), 'K': (40, 34, 52),            # outline, dark grey
    'g': (120, 120, 132), 'G': (190, 190, 200),     # grey, light grey
    'w': (245, 245, 250),                           # white
    'r': (200, 30, 30), 'R': (255, 90, 60),         # red, light red
    'o': (240, 130, 20), 'y': (255, 220, 60),       # orange, yellow
    'b': (40, 90, 210), 'B': (120, 190, 255),       # blue, light blue
    'c': (180, 240, 255),                           # ice
    'p': (110, 40, 160), 'P': (190, 110, 240),      # purple, light purple
    'n': (110, 70, 40), 'N': (170, 120, 70),        # brown, tan
    's': (220, 190, 120),                           # sand
    'e': (40, 150, 60), 'E': (120, 210, 90),        # green, light green
    'm': (70, 50, 80),                              # murky purple-grey
    'h': (230, 200, 170),                           # pale skin
}


class Icon:
    def __init__(self):
        self.a = [['.'] * N for _ in range(N)]

    def px(self, x, y, c):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < N and 0 <= y < N:
            self.a[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)

    def disc(self, cx, cy, r, c):
        for y in range(N):
            for x in range(N):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r + 0.3:
                    self.px(x, y, c)

    def ring(self, cx, cy, r, c, w=1.0):
        for y in range(N):
            for x in range(N):
                d = math.hypot(x - cx, y - cy)
                if r - w <= d <= r + 0.4:
                    self.px(x, y, c)

    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / max(n, 1)
            self.px(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c)

    def poly(self, pts, c):
        for y in range(N):
            for x in range(N):
                inside = False
                j = len(pts) - 1
                for i in range(len(pts)):
                    xi, yi = pts[i]
                    xj, yj = pts[j]
                    if ((yi > y + 0.5) != (yj > y + 0.5)) and (x + 0.5 < (xj - xi) * (y + 0.5 - yi) / (yj - yi) + xi):
                        inside = not inside
                    j = i
                if inside:
                    self.px(x, y, c)

    def outline(self, c='k'):
        b = [row[:] for row in self.a]
        for y in range(N):
            for x in range(N):
                if self.a[y][x] != '.':
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < N and 0 <= yy < N and self.a[yy][xx] not in ('.', c):
                        b[y][x] = c
                        break
        self.a = b
        return self


def flame(i, cx, base, h, w=2.5):
    i.poly([(cx - w, base), (cx - w * 0.8, base - h * 0.5), (cx, base - h), (cx + w * 0.8, base - h * 0.5), (cx + w, base)], 'r')
    i.poly([(cx - w * 0.6, base), (cx - w * 0.4, base - h * 0.45), (cx, base - h * 0.75), (cx + w * 0.4, base - h * 0.45), (cx + w * 0.6, base)], 'o')
    i.poly([(cx - w * 0.3, base), (cx, base - h * 0.4), (cx + w * 0.3, base)], 'y')


def bomb(i, cx, cy, r=3.5):
    i.disc(cx, cy, r, 'b')
    i.px(cx - r * 0.4, cy - r * 0.4, 'B')
    i.px(cx - r * 0.4 + 1, cy - r * 0.4, 'B')
    i.rect(int(cx) - 1, int(cy - r) - 1, int(cx), int(cy - r) - 1, 'g')
    i.px(cx + 1, cy - r - 2, 'y')
    i.px(cx + 2, cy - r - 3, 'o')


def hand(i, ox, oy, col='m', flip=False):
    # a big grasping hand reaching down: palm, four spread fingers and a thumb
    i.rect(ox + 3, oy, ox + 11, oy + 4, col)
    for f, h in ((0, 4), (3, 6), (6, 6), (9, 4)):
        i.rect(ox + 3 + f, oy + 5, ox + 4 + f, oy + 5 + h, col)
        i.px(ox + 3 + f, oy + 5 + h, 'G')
    i.line(ox + 2, oy + 2, ox, oy + 6, col)
    i.line(ox + 3, oy + 2, ox + 1, oy + 6, col)
    i.px(ox + 5, oy + 1, 'G'); i.px(ox + 6, oy + 1, 'G')


def skull(i, cx, cy, col='w'):
    i.disc(cx, cy, 3.2, col)
    i.rect(int(cx) - 2, int(cy) + 2, int(cx) + 2, int(cy) + 4, col)
    i.px(cx - 1, cy, 'k'); i.px(cx + 1, cy, 'k')
    i.px(cx - 1, cy + 4, 'k'); i.px(cx + 1, cy + 4, 'k')


ICONS = []


def icon(name):
    def deco(fn):
        i = Icon()
        fn(i)
        ICONS.append((name, i))
        return fn
    return deco


@icon('Beamos Watch')
def _(i):
    i.rect(4, 9, 11, 14, 'g'); i.rect(5, 9, 6, 14, 'G')
    i.disc(7.5, 7, 4.5, 'g'); i.disc(6.5, 6, 2, 'G')
    i.disc(7.5, 7, 2, 'r'); i.px(7, 6, 'R'); i.px(8, 7, 'y')
    i.line(12, 4, 15, 1, 'R'); i.line(12, 5, 15, 2, 'r')


@icon('Wall of Flame')
def _(i):
    for cx, h in ((3, 9), (8, 12), (13, 9)):
        flame(i, cx, 14.5, h, 2.4)
    i.rect(0, 15, 15, 15, 'n')


@icon('Rock Fall')
def _(i):
    for cx, cy in ((4, 6), (11, 10)):
        i.line(cx + 2, cy - 2, cx + 4, cy - 5, 'o'); i.line(cx + 3, cy - 2, cx + 5, cy - 5, 'y')
        i.disc(cx, cy, 2.7, 'n'); i.px(cx - 1, cy - 1, 'N'); i.px(cx + 1, cy + 1, 'r')
    i.rect(0, 15, 15, 15, 'n')


@icon('Bomb Rain')
def _(i):
    bomb(i, 4, 5, 2.6); bomb(i, 11, 4, 2.6); bomb(i, 8, 11, 3.2)


@icon('Sealed Door')
def _(i):
    i.rect(3, 2, 12, 15, 'n'); i.rect(4, 3, 11, 15, 'N'); i.rect(7, 3, 8, 15, 'n')
    i.px(10, 9, 'y')
    for t in range(-1, 2):
        i.line(2, 4 + t, 13, 13 + t, 'c'); i.line(13, 4 + t, 2, 13 + t, 'B')


@icon('Quicksand')
def _(i):
    i.disc(7.5, 8.5, 7, 's')
    for k in range(30):
        a = k * 0.5
        r = 6.5 - k * 0.2
        i.px(7.5 + math.cos(a) * r, 8.5 + math.sin(a) * r * 0.7, 'N')
    i.px(7, 8, 'n'); i.px(8, 8, 'n')


@icon('Ambush Ring')
def _(i):
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        x, y = 7.5 + math.cos(a) * 5, 7.5 + math.sin(a) * 5
        i.disc(x, y, 2.2, 'e'); i.px(x, y, 'r'); i.px(x - 1, y - 1, 'E')
    i.disc(7.5, 7.5, 1.6, 'h')


@icon('Floormasters')
def _(i):
    hand(i, 1, 1, 'e')
    i.px(5, 2, 'E'); i.px(7, 3, 'E')


@icon('Swarm')
def _(i):
    for cx, cy, s in ((4, 5, 1), (11, 4, 1), (8, 11, 1)):
        i.poly([(cx - 4, cy - 1), (cx - 1, cy + 1), (cx, cy - 2), (cx + 1, cy + 1), (cx + 4, cy - 1), (cx + 2, cy + 2), (cx - 2, cy + 2)], 'p')
        i.disc(cx, cy, 1.1, 'K'); i.px(cx - 1, cy, 'y'); i.px(cx + 1, cy, 'y')


@icon('Waves')
def _(i):
    skull(i, 4, 6); skull(i, 11.5, 4.5); skull(i, 8, 11, 'G')


@icon('Sentinel')
def _(i):
    i.rect(4, 3, 11, 14, 'E'); i.rect(4, 3, 5, 14, 'e'); i.rect(10, 3, 11, 14, 'e')
    for y in (5, 8, 11):
        i.rect(4, y, 11, y, 'e')
    i.disc(7.5, 3, 3, 'r'); i.disc(7.5, 3, 1.5, 'k')


@icon('Duel')
def _(i):
    i.line(2, 13, 13, 2, 'G'); i.line(3, 13, 13, 3, 'w')
    i.line(2, 2, 13, 13, 'G'); i.line(2, 3, 12, 13, 'w')
    i.line(1, 11, 4, 14, 'n'); i.line(11, 14, 14, 11, 'n')
    i.px(7, 7, 'y'); i.px(8, 8, 'y')


@icon('Backwards Path')
def _(i):
    i.poly([(1, 5), (6, 1), (6, 3), (14, 3), (14, 7), (6, 7), (6, 9)], 'R')
    i.poly([(14, 11), (9, 7), (9, 9), (1, 9), (1, 13), (9, 13), (9, 15)], 'b')


@icon('Frozen Ground')
def _(i):
    for a in range(3):
        ang = a * math.pi / 3
        dx, dy = math.cos(ang) * 6.5, math.sin(ang) * 6.5
        i.line(7.5 - dx, 7.5 - dy, 7.5 + dx, 7.5 + dy, 'c')
        for s in (1, -1):
            bx, by = 7.5 + dx * 0.6 * s, 7.5 + dy * 0.6 * s
            px, py = -dy * 0.3, dx * 0.3
            i.line(bx, by, bx + px + dx * 0.25 * s, by + py + dy * 0.25 * s, 'B')
            i.line(bx, by, bx - px + dx * 0.25 * s, by - py + dy * 0.25 * s, 'B')
    i.disc(7.5, 7.5, 1, 'w')


@icon('Iron Weight')
def _(i):
    i.poly([(3, 6), (12, 6), (14, 14), (1, 14)], 'K')
    i.poly([(4, 7), (6, 7), (4, 13), (3, 13)], 'g')
    i.ring(7.5, 4, 2.6, 'K', 1.2)
    i.rect(5, 10, 10, 10, 'G')


@icon('Shrink')
def _(i):
    i.disc(7.5, 10, 1.6, 'h'); i.rect(6, 12, 9, 14, 'e')
    for x in (3, 12):
        i.line(x, 1, x, 6, 'w'); i.line(x - 2, 4, x, 6, 'w'); i.line(x + 2, 4, x, 6, 'w')


@icon('Sealed Hand')
def _(i):
    i.disc(7.5, 7.5, 6, 'y'); i.disc(7.5, 7.5, 4.5, 'o')
    i.poly([(5, 9), (7.5, 4), (10, 9)], 'y')
    for t in (0, 1):
        i.line(2 + t, 2, 14 + t, 14, 'r'); i.line(14 - t, 2, 2 - t, 14, 'r')


@icon('Withering')
def _(i):
    i.disc(5, 6, 3.3, 'r'); i.disc(10, 6, 3.3, 'r')
    i.poly([(1.8, 7), (13.2, 7), (7.5, 14)], 'r')
    i.px(4, 5, 'R'); i.px(5, 4, 'R')
    i.line(8, 3, 7, 6, 'k'); i.line(7, 6, 9, 9, 'k'); i.line(9, 9, 7, 13, 'k')
    for x, y in ((2, 14), (12, 13)):
        i.px(x, y, 'm')


@icon('Lost Bearings')
def _(i):
    i.ring(7.5, 7.5, 6, 'y', 1.2); i.ring(7.5, 7.5, 2.5, 'y', 1)
    i.rect(7, 0, 8, 3, 'y'); i.rect(7, 12, 8, 15, 'y'); i.rect(0, 7, 3, 8, 'y'); i.rect(12, 7, 15, 8, 'y')
    i.line(1, 14, 14, 1, 'r'); i.line(2, 14, 14, 2, 'r')


@icon('Ghostly Gale')
def _(i):
    for y, l in ((3, 11), (7, 13), (11, 9)):
        i.line(1, y, l, y, 'G')
        i.ring(l, y - 1.2, 1.6, 'w', 1)
    i.px(3, 5, 'P'); i.px(6, 9, 'P'); i.px(4, 13, 'P')


@icon('Phantom Step')
def _(i):
    for x, y in ((3, 11), (8, 7), (12, 3)):
        i.disc(x, y, 1.8, 'P'); i.rect(x - 1, y + 2, x, y + 3, 'p')
    i.px(1, 14, 'p'); i.px(14, 1, 'P')


@icon('Eye of Ganon')
def _(i):
    i.poly([(0, 8), (4, 4), (11, 4), (15, 8), (11, 12), (4, 12)], 'y')
    i.disc(7.5, 8, 3, 'r'); i.rect(7, 5, 8, 11, 'k'); i.px(9, 6, 'R')


@icon('Shadow Veil')
def _(i):
    i.poly([(7.5, 1), (12, 5), (13, 15), (2, 15), (3, 5)], 'p')
    i.poly([(7.5, 3), (10, 6), (10, 10), (5, 10), (5, 6)], 'k')
    i.px(6, 7, 'P'); i.px(9, 7, 'P')


@icon('Rupee Tithe')
def _(i):
    i.poly([(5, 1), (10, 1), (12, 5), (12, 10), (7.5, 15), (3, 10), (3, 5)], 'e')
    i.poly([(6, 3), (9, 3), (9, 9), (7.5, 12), (6, 9)], 'E')
    i.rect(10, 12, 15, 13, 'r')


@icon('Moblin Guard')
def _(i):
    i.line(2, 14, 12, 4, 'N'); i.line(3, 14, 13, 4, 'n')
    i.poly([(11, 5), (14, 1), (14, 6)], 'G'); i.px(13, 3, 'w')
    i.rect(5, 10, 7, 11, 'r')


@icon('Disarm')
def _(i):
    # his sword knocked spinning out of his open hand
    i.line(4, 2, 11, 9, 'G'); i.line(5, 2, 12, 9, 'w')
    i.line(10, 11, 13, 8, 'n'); i.line(12, 10, 13, 11, 'n')
    i.rect(1, 12, 5, 14, 'h'); i.px(1, 11, 'h'); i.px(3, 11, 'h'); i.px(5, 11, 'h')
    for x, y in ((14, 2), (13, 4), (2, 7)):
        i.px(x, y, 'y')
    i.line(13, 13, 15, 15, 'r'); i.line(15, 13, 13, 15, 'r')


@icon('Dark Pact')
def _(i):
    for (x, y) in ((7.5, 1), (3.5, 8), (11.5, 8)):
        i.poly([(x, y), (x + 4, y + 7), (x - 4, y + 7)], 'y')
    i.poly([(7.5, 14.5), (5.6, 9.2), (9.4, 9.2)], 'p')
    i.disc(7.5, 10.5, 0.8, 'P')


@icon('Dark Link')
def _(i):
    i.disc(7.5, 4, 2.6, 'K'); i.poly([(4, 2), (11, 2), (13, 0)], 'K')
    i.rect(5, 7, 10, 12, 'K'); i.rect(5, 13, 6, 15, 'K'); i.rect(9, 13, 10, 15, 'K')
    i.px(6, 4, 'r'); i.px(9, 4, 'r')
    i.line(11, 8, 15, 4, 'G')


@icon('Wallmaster')
def _(i):
    hand(i, 1, 0, 'm')
    i.rect(2, 15, 13, 15, 'K')


@icon('Iron Knuckle')
def _(i):
    i.line(4, 15, 11, 2, 'n')
    i.poly([(8, 1), (14, 3), (14, 9), (10, 7)], 'G'); i.poly([(9, 2), (13, 4), (13, 7)], 'w')
    i.poly([(8, 1), (3, 2), (6, 6)], 'g')


@icon('Fire Pillar')
def _(i):
    for k in range(6):
        a = k * math.pi / 3
        flame(i, 7.5 + math.cos(a) * 5.5, 9 + math.sin(a) * 3.8 + 2, 5, 1.6)
    i.disc(7.5, 10, 1.3, 'h'); i.rect(7, 11, 8, 13, 'e')


@icon('Aero Alchemy')
def _(i):
    for cx, cy, r in ((5, 10, 3.5), (11, 5, 2.7), (11, 12, 1.8), (4, 3, 1.5)):
        i.ring(cx, cy, r, 'B', 1); i.px(cx - r * 0.4, cy - r * 0.4, 'w')
    i.line(0, 15, 15, 15, 'b')


@icon('The Haunting')
def _(i):
    i.poly([(3, 2), (12, 2), (13, 8), (11, 15), (4, 15), (2, 8)], 'h')
    i.poly([(4, 5), (7, 5), (6, 8), (4, 8)], 'k'); i.poly([(8, 5), (11, 5), (11, 8), (9, 8)], 'k')
    i.poly([(5, 11), (10, 11), (9, 14), (6, 14)], 'k')
    i.px(5, 6, 'r'); i.px(10, 6, 'r')


@icon('Lava Floor')
def _(i):
    i.rect(0, 8, 15, 15, 'r')
    for x in range(16):
        y = 8 + int(round(math.sin(x * 0.9) * 1.3))
        i.px(x, y, 'o'); i.px(x, y + 1, 'o')
    for x, y in ((3, 12), (9, 13), (12, 11)):
        i.px(x, y, 'y'); i.px(x + 1, y, 'o')
    i.disc(6, 5, 1.2, 'o'); i.px(11, 3, 'y')


@icon('Bomb Flower Hat')
def _(i):
    i.disc(7.5, 12, 3.2, 'h'); i.rect(5, 15, 10, 15, 'e')
    bomb(i, 7.5, 6, 3.4)
    for x in (3, 12):
        i.poly([(x - 1, 9), (x + 1, 9), (x, 5)], 'E')


@icon('Earthquake')
def _(i):
    i.rect(0, 11, 15, 15, 'n'); i.rect(0, 11, 15, 11, 'N')
    i.line(7, 11, 5, 13, 'k'); i.line(5, 13, 8, 15, 'k'); i.line(9, 11, 11, 14, 'k')
    for cx, cy in ((3, 3), (11, 2), (7, 6)):
        i.disc(cx, cy, 1.6, 'g'); i.px(cx - 1, cy - 1, 'G')
    for y in (4, 7):
        i.px(0, y, 'y'); i.px(15, y + 1, 'y')


def main():
    names = [n for n, _ in ICONS]
    sheet = Image.new('RGBA', (len(ICONS) * 17 * 6, 17 * 6), (60, 50, 70, 255))
    for k, (name, ic) in enumerate(ICONS):
        ic.outline('k')
        for y in range(N):
            for x in range(N):
                c = PAL[ic.a[y][x]]
                if c:
                    for dy in range(6):
                        for dx in range(6):
                            sheet.putpixel((k * 102 + x * 6 + dx + 3, y * 6 + dy + 3), c + (255,))
    sheet.save(sys.argv[2])

    out = ['// Ganondorf: Invasion: the Dark Arts icons, 16x16 pixel art drawn for this mod (no game assets).',
           '// Generated by make_icons.py: each icon is 16 rows of palette letters (\'.\' is clear).', '#pragma once', '',
           'struct GdfIconColor {', '    char key;', '    unsigned char r, g, b;', '};', '',
           'static const GdfIconColor sGdfIconPalette[] = {']
    for kch, c in PAL.items():
        if c:
            out.append('    { \'%s\', %d, %d, %d },' % (kch, c[0], c[1], c[2]))
    out.append('};')
    out.append('')
    out.append('static const char* const sGdfArtIcons[][16] = {')
    for name, ic in ICONS:
        out.append('    { // %s' % name)
        for y in range(N):
            out.append('      "%s",' % ''.join(ic.a[y]))
        out.append('    },')
    out.append('};')
    open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
    print(len(ICONS), 'icons')


main()
