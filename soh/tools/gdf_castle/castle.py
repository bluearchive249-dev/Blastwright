"""Hyrule Castle interior for "Play as Ganondorf": a castle wing in the style of the courtyard and the Temple of Time,
generated as a scene for the unused SCENE_TESTROOM slot (scenes/shared/testroom_scene). It's built in the courtyard's
own coordinates, so the throne room's east wall holds the courtyard window: the real courtyard (its display lists,
called from oot.o2r) is outside, with Zelda's garden.

Layout (floor y 20, x west negative):
  Throne room   x -1500..-575  z -460..460   throne at the west end facing the window (east)
  Grand hall    x -1100..-820  z  460..1660  library door west, barracks door east
  Library       x -1700..-1100 z  700..1300
  Barracks      x  -820..-260  z 1000..1500
  Entrance hall x -1400..-520  z 1660..2360  front doors south, grand stair up the west side to
  Gallery       y 320 along the entrance hall's north wall, and the
  Royal rooms   x -1700..-1100 z 1300..1660  floor y 320
"""
import math, struct, sys, numpy as np
sys.path.insert(0, '/home/claude/o2r')
from otr import z as OOT, HASH
from matlib import materials, raw_cmds
import o2rgen as G

SCENE = 'testroom'
BASE = f'scenes/shared/{SCENE}_scene/'
FLOOR = 20

# ---- Materials (textures from the Temple of Time and the courtyard) ----

TOT = materials('tokinoma')
NAKA = materials('nakaniwa')
SPOT15 = materials('spot15')
ALL = {**TOT, **NAKA, **SPOT15}


def tex(name):
    for k in ALL:
        if k.endswith(name):
            return k
    raise KeyError(name)


MAT = {
    'marble_check': tex('tokinoma_sceneTex_00D7A0'),
    'marble': tex('tokinoma_sceneTex_010FA0'),
    'marble2': tex('tokinoma_sceneTex_00FFA0'),
    'brick_white': tex('tokinoma_sceneTex_00EFA0'),
    'brick_white2': tex('tokinoma_sceneTex_00DFA0'),
    'stone_court': tex('nakaniwa_room_0Tex_011E18'),
    'brick_court': tex('nakaniwa_room_0Tex_00C618'),
    'tile_court': tex('nakaniwa_room_0Tex_00DE18'),
    'carpet_red': tex('tokinoma_sceneTex_00CFA0'),
    'carpet_blue': tex('nakaniwa_room_0Tex_011618'),
    'lattice': tex('tokinoma_room_0Tex_0151D8'),
    'window': tex('nakaniwa_room_0Tex_010618'),
    'shelves': tex('nakaniwa_room_0Tex_00E618'),
    'ceiling': tex('tokinoma_sceneTex_0107A0'),
    'pillar': tex('nakaniwa_room_0Tex_007A18'),
    'plaster': tex('nakaniwa_room_0Tex_013618'),
    'stone_band': tex('nakaniwa_room_0Tex_013E18'),
    'portrait1': tex('nakaniwa_room_0Tex_008418'),
    'portrait2': tex('nakaniwa_room_0Tex_008218'),
    'portrait3': tex('nakaniwa_room_0Tex_007618'),
    'portrait4': tex('nakaniwa_room_0Tex_007418'),
    'portrait5': tex('nakaniwa_room_0Tex_007218'),
    'hedge': tex('nakaniwa_room_0Tex_012618'),
    'bridge': tex('spot15_sceneTex_00C200'),
    'bridge_edge': tex('spot15_sceneTex_016600'),
}
# World units per texel, per material (how big the texture is on the surface)
DENSITY = {'carpet_red': 2.2, 'carpet_blue': 2.2, 'portrait1': 3.0, 'portrait2': 3.0, 'portrait3': 3.0,
           'portrait4': 3.0, 'portrait5': 3.0, 'lattice': 3.0, 'window': 3.0, 'shelves': 2.5}
TEMPLATE = TOT[tex('tokinoma_sceneTex_011FA0')]['run']  # a full run: its state commands go with the 7-command runs


def material_cmds(texpath):
    run = ALL[texpath]['run']
    ops = [c[3] for c in run]
    cmds = [struct.pack('<II', 0xE7000000, 0)] + list(run)
    if 0xFC not in ops:
        cmds += [c for c in TEMPLATE if c[3] in (0xFC, 0xE2, 0xD9, 0xFA)]
    return cmds


# ---- Geometry ----

L = np.array([0.35, 0.85, 0.4])
L /= np.linalg.norm(L)


class Builder:
    def __init__(self):
        self.faces = {}   # material -> list of (verts[4 or 3] of (x,y,z,u,v,r,g,b,a))
        self.col_verts = []
        self.col_index = {}
        self.col_polys = []

    # A quad p0..p3 (counter-clockwise seen from the front), textured by world position
    def quad(self, mat, pts, collide=None, color=(255, 255, 255), uv=None, light=True):
        P = np.array(pts, float)
        n = np.cross(P[1] - P[0], P[2] - P[0])
        n /= max(np.linalg.norm(n), 1e-9)
        if uv is None:
            d = DENSITY.get(mat, 2.0)
            if abs(n[1]) > 0.7:
                U = P[:, 0] / d
                V = P[:, 2] / d
            elif abs(n[0]) > abs(n[2]):
                U = P[:, 2] / d
                V = -P[:, 1] / d
            else:
                U = P[:, 0] / d
                V = -P[:, 1] / d
            uvs = list(zip(U, V))
        else:
            uvs = uv
        shade = (0.62 + 0.38 * max(0.0, float(np.dot(n, L)))) if light else 1.0
        verts = []
        for (x, y, zz), (u, v) in zip(P, uvs):
            # a little darker high up toward the ceiling, warm light
            k = shade * (1.0 - 0.12 * min(1.0, max(0.0, (y - FLOOR) / 520.0)))
            verts.append((x, y, zz, u, v, int(min(255, color[0] * k)), int(min(255, color[1] * k * 0.97)),
                          int(min(255, color[2] * k * 0.92)), 255))
        self.faces.setdefault(mat, []).append(verts)
        if collide is not None:
            idx = [self.cv(p) for p in P]
            for tri in ((0, 1, 2), (0, 2, 3)) if len(idx) == 4 else ((0, 1, 2),):
                self.col_polys.append((collide, idx[tri[0]], idx[tri[1]], idx[tri[2]], 0, 0))

    def cv(self, p):
        key = tuple(int(round(c)) for c in p)
        if key not in self.col_index:
            self.col_index[key] = len(self.col_verts)
            self.col_verts.append(key)
        return self.col_index[key]

    # Floor rectangle (facing up)
    def floor(self, mat, x0, x1, z0, z1, y=FLOOR, surface='floor', color=(255, 255, 255)):
        self.quad(mat, [(x0, y, z1), (x1, y, z1), (x1, y, z0), (x0, y, z0)], collide=SURF[surface], color=color)

    def ceiling(self, mat, x0, x1, z0, z1, y, color=(235, 235, 245)):
        self.quad(mat, [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], color=color)

    # A wall along one side of a room, facing into it, with openings (u0, u1, y0, y1) cut out of it
    def wall(self, mat, axis, fixed, a, b, y0, y1, facing, openings=(), collide=True, color=(255, 255, 255)):
        pieces = []
        ops = sorted(openings)
        u = a
        for (o0, o1, v0, v1) in ops:
            if o0 > u:
                pieces.append((u, o0, y0, y1))
            if v0 > y0:
                pieces.append((o0, o1, y0, v0))
            if v1 < y1:
                pieces.append((o0, o1, v1, y1))
            u = o1
        if u < b:
            pieces.append((u, b, y0, y1))
        for (u0, u1, v0, v1) in pieces:
            if axis == 'x':  # wall in the plane x = fixed, spanning z
                p = [(fixed, v0, u0), (fixed, v0, u1), (fixed, v1, u1), (fixed, v1, u0)]
                if facing > 0:  # faces +x
                    p = [p[1], p[0], p[3], p[2]]
            else:  # plane z = fixed, spanning x
                p = [(u0, v0, fixed), (u1, v0, fixed), (u1, v1, fixed), (u0, v1, fixed)]
                if facing < 0:
                    p = [p[1], p[0], p[3], p[2]]
            self.quad(mat, p, collide=SURF['wall'] if collide else None, color=color)

    # Box (all sides but the bottom), outward faces
    def box(self, mats, x0, x1, y0, y1, z0, z1, collide=True, color=(255, 255, 255)):
        top, side = (mats, mats) if isinstance(mats, str) else mats
        self.quad(top, [(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], collide=SURF['floor'] if collide else None,
                  color=color)
        c = SURF['wall'] if collide else None
        self.quad(side, [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], collide=c, color=color)
        self.quad(side, [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], collide=c, color=color)
        self.quad(side, [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], collide=c, color=color)
        self.quad(side, [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], collide=c, color=color)

    # An octagonal pillar with a base and a capital
    def pillar(self, x, zc, r, y0, y1, mat='pillar', band='stone_band'):
        self.box(band, x - r - 8, x + r + 8, y0, y0 + 24, zc - r - 8, zc + r + 8)
        self.box(band, x - r - 8, x + r + 8, y1 - 24, y1, zc - r - 8, zc + r + 8, collide=False)
        n = 8
        for i in range(n):
            a0 = 2 * math.pi * i / n + math.pi / 8
            a1 = 2 * math.pi * (i + 1) / n + math.pi / 8
            p0 = (x + r * math.cos(a0), zc + r * math.sin(a0))
            p1 = (x + r * math.cos(a1), zc + r * math.sin(a1))
            circ = 2 * math.pi * r / n
            u0, u1 = i * circ / 2.0, (i + 1) * circ / 2.0
            self.quad(mat, [(p1[0], y0 + 24, p1[1]), (p0[0], y0 + 24, p0[1]), (p0[0], y1 - 24, p0[1]),
                            (p1[0], y1 - 24, p1[1])], collide=SURF['wall'],
                      uv=[(u1, 0), (u0, 0), (u0, -(y1 - y0 - 48) / 2.0), (u1, -(y1 - y0 - 48) / 2.0)])

    # Stairs rising along +z or -z (dz sign) from (x0..x1, z_start) by `rise` over `run`; a ramp for collision
    def stairs(self, mat, x0, x1, z_start, run, rise, steps, dz, side_mat='stone_band'):
        sh = rise / steps
        sd = run / steps
        for i in range(steps):
            za = z_start + dz * i * sd
            zb = z_start + dz * (i + 1) * sd
            y = FLOOR + (i + 1) * sh
            lo, hi = min(za, zb), max(za, zb)
            self.quad(mat, [(x0, y, hi), (x1, y, hi), (x1, y, lo), (x0, y, lo)])
            # riser, facing back down the stair
            zr = za
            if dz < 0:
                self.quad(side_mat, [(x0, y - sh, zr), (x1, y - sh, zr), (x1, y, zr), (x0, y, zr)])
            else:
                self.quad(side_mat, [(x1, y - sh, zr), (x0, y - sh, zr), (x0, y, zr), (x1, y, zr)])
        z_end = z_start + dz * run
        lo, hi = min(z_start, z_end), max(z_start, z_end)
        # the walking surface: one smooth ramp
        if dz < 0:
            ramp = [(x0, FLOOR, hi), (x1, FLOOR, hi), (x1, FLOOR + rise, lo), (x0, FLOOR + rise, lo)]
        else:
            ramp = [(x0, FLOOR + rise, hi), (x1, FLOOR + rise, hi), (x1, FLOOR, lo), (x0, FLOOR, lo)]
        idx = [self.cv(p) for p in ramp]
        self.col_polys.append((SURF['floor'], idx[0], idx[1], idx[2], 0, 0))
        self.col_polys.append((SURF['floor'], idx[0], idx[2], idx[3], 0, 0))

    def railing(self, x0, x1, z0, z1, y, h=60, posts=8):
        self.box('stone_band', x0, x1, y + h - 8, y + h, z0, z1, collide=False)
        for i in range(posts + 1):
            t = i / posts
            x = x0 + (x1 - x0) * t
            zz = z0 + (z1 - z0) * t
            self.box('marble', x - 5, x + 5, y, y + h - 8, zz - 5, zz + 5, collide=False)
        # one wall to stop him walking off
        if abs(x1 - x0) > abs(z1 - z0):
            zc = (z0 + z1) / 2
            idx = [self.cv(p) for p in [(x0, y, zc), (x1, y, zc), (x1, y + h + 40, zc), (x0, y + h + 40, zc)]]
        else:
            xc = (x0 + x1) / 2
            idx = [self.cv(p) for p in [(xc, y, z0), (xc, y, z1), (xc, y + h + 40, z1), (xc, y + h + 40, z0)]]
        for a, b, c in ((0, 1, 2), (0, 2, 3), (0, 2, 1), (0, 3, 2)):
            self.col_polys.append((SURF['wall'], idx[a], idx[b], idx[c], 0, 0))


# Surface types (data[1], data[0]) from the Temple of Time: stone floor, wall; the front door floor carries exit 1
SURFACES = [(0x00004802, 0x00000000), (0x00002FCD, 0x00000000), (0x00004802, 0x00000100)]
SURF = {'floor': 0, 'wall': 1, 'exit': 2}


def build():
    B = Builder()
    F = FLOOR

    # ---- Throne room ----
    x0, x1, z0, z1, top = -1500, -575, -460, 460, 540
    B.floor('marble_check', x0, x1, z0, z1)
    B.ceiling('ceiling', x0, x1, z0, z1, top)
    # The window onto the courtyard (east wall), exactly where the courtyard's window tunnel ends
    B.wall('brick_white', 'x', x1, z0, z1, F, top, -1, openings=[(-20, 20, 113, 198)])
    B.wall('brick_white', 'x', x0, z0, z1, F, top, +1)
    B.wall('brick_white', 'z', z0, x0, x1, F, top, +1)
    B.wall('brick_white', 'z', z1, x0, x1, F, top, -1, openings=[(-1100, -820, F, F + 300)])
    # A stone band around the walls at the top
    for zz, f in ((z0 + 1, 1), (z1 - 1, -1)):
        B.wall('stone_band', 'z', zz, x0, x1, top - 40, top - 10, f, collide=False)
    # Two rows of pillars
    for xx in (-1350, -1150, -950, -750):
        for zz in (-330, 330):
            B.pillar(xx, zz, 30, F, top)
    # The dais and the throne, facing the window
    for i, (dx, dzs) in enumerate(((0, 230), (40, 200), (80, 170))):
        B.box(('marble', 'stone_band'), x0, -1300 - dx, F, F + 15 * (i + 1), -dzs, dzs)
    ty = F + 45
    B.box(('carpet_red', 'stone_band'), -1470, -1410, ty, ty + 40, -40, 40)        # seat
    B.box(('stone_band', 'marble2'), -1490, -1470, ty, ty + 200, -55, 55)          # back
    B.box(('stone_band', 'stone_band'), -1495, -1465, ty + 200, ty + 230, -30, 30, collide=False)  # crest
    B.box(('stone_band', 'marble2'), -1470, -1410, ty, ty + 70, -60, -40)           # arms
    B.box(('stone_band', 'marble2'), -1470, -1410, ty, ty + 70, 40, 60)
    # The red carpet from the dais to the window
    B.quad('carpet_red', [(-1300, F + 1, 70), (-650, F + 1, 70), (-650, F + 1, -70), (-1300, F + 1, -70)],
           uv=[(0, 64), (290, 64), (290, 0), (0, 0)])
    # Stained glass panels along the north and south walls, lit from behind
    for xx in (-1250, -1050, -850):
        for zz, f in ((z0 + 2, 1), (z1 - 2, -1)):
            if f > 0:
                p = [(xx - 50, 200, zz), (xx + 50, 200, zz), (xx + 50, 420, zz), (xx - 50, 420, zz)]
            else:
                p = [(xx + 50, 200, zz), (xx - 50, 200, zz), (xx - 50, 420, zz), (xx + 50, 420, zz)]
            B.quad('lattice', p, uv=[(0, 64), (32, 64), (32, 0), (0, 0)], light=False, color=(255, 250, 220))
    # The window's reveal, through to the courtyard's own window tunnel at x -575
    B.quad('stone_court', [(-575, 113, 20), (-575, 113, -20), (-615, 113, -20), (-615, 113, 20)])

    # ---- Grand hall ----
    gx0, gx1, gz0, gz1, gtop = -1100, -820, 460, 1660, 400
    B.floor('marble', gx0, gx1, gz0, gz1)
    B.ceiling('ceiling', gx0, gx1, gz0, gz1, gtop)
    B.wall('brick_court', 'x', gx0, gz0, gz1, F, gtop, +1, openings=[(900, 1100, F, F + 220)])
    B.wall('brick_court', 'x', gx1, gz0, gz1, F, gtop, -1, openings=[(1150, 1350, F, F + 220)])
    B.quad('carpet_blue', [(-1000, F + 1, gz1), (-920, F + 1, gz1), (-920, F + 1, gz0), (-1000, F + 1, gz0)],
           uv=[(0, 545), (36, 545), (36, 0), (0, 0)])
    for zz in range(560, 1660, 200):
        for xx in (-1070, -850):
            B.pillar(xx, zz, 18, F, gtop)
    # the lintel over the throne room doors
    B.wall('stone_band', 'z', gz0 + 1, gx0, gx1, F + 300, F + 330, 1, collide=False)

    # ---- Library ----
    lx0, lx1, lz0, lz1, ltop = -1700, -1100, 700, 1300, 380
    B.floor('tile_court', lx0, lx1, lz0, lz1)
    B.ceiling('ceiling', lx0, lx1, lz0, lz1, ltop)
    B.wall('plaster', 'x', lx0, lz0, lz1, F, ltop, +1)
    B.wall('plaster', 'x', lx1, lz0, lz1, F, ltop, -1, openings=[(900, 1100, F, F + 220)])
    B.wall('plaster', 'z', lz0, lx0, lx1, F, ltop, +1)
    B.wall('plaster', 'z', lz1, lx0, lx1, F, ltop, -1)
    # Bookshelves round the walls, tables in the middle
    for zz in range(730, 1290, 140):
        B.box(('stone_band', 'shelves'), lx0, lx0 + 40, F, F + 260, zz, zz + 120)
    for xx in range(-1680, -1120, 140):
        B.box(('stone_band', 'shelves'), xx, xx + 120, F, F + 260, lz0, lz0 + 40)
        B.box(('stone_band', 'shelves'), xx, xx + 120, F, F + 260, lz1 - 40, lz1)
    for xx in (-1520, -1320):
        B.box(('marble2', 'stone_band'), xx - 60, xx + 60, F, F + 50, 940, 1060)

    # ---- Barracks ----
    bx0, bx1, bz0, bz1, btop = -820, -260, 1000, 1500, 320
    B.floor('stone_court', bx0, bx1, bz0, bz1)
    B.ceiling('ceiling', bx0, bx1, bz0, bz1, btop)
    B.wall('brick_court', 'x', bx0, bz0, bz1, F, btop, +1, openings=[(1150, 1350, F, F + 220)])
    B.wall('brick_court', 'x', bx1, bz0, bz1, F, btop, -1)
    B.wall('brick_court', 'z', bz0, bx0, bx1, F, btop, +1)
    B.wall('brick_court', 'z', bz1, bx0, bx1, F, btop, -1)
    for xx in (-700, -560, -420):
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz0 + 20, bz0 + 150)   # bunks
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz1 - 150, bz1 - 20)
    B.box(('stone_band', 'brick_court'), bx1 - 50, bx1, F, F + 160, 1180, 1320)                  # weapon rack

    # ---- Entrance hall ----
    ex0, ex1, ez0, ez1, etop = -1400, -520, 1660, 2360, 560
    B.floor('marble_check', ex0, ex1, ez0, ez1)
    # the front doors' threshold carries the exit back out to the drawbridge
    B.floor('marble_check', -1020, -900, ez1 - 40, ez1 - 2, y=F + 0.5, surface='exit')
    B.ceiling('ceiling', ex0, ex1, ez0, ez1, etop)
    B.wall('brick_white2', 'x', ex0, ez0, ez1, F, etop, +1)
    B.wall('brick_white2', 'x', ex1, ez0, ez1, F, etop, -1)
    B.wall('brick_white2', 'z', ez0, ex0, ex1, F, etop, +1,
           openings=[(-1100, -820, F, F + 300), (-1400, -1200, F + 320, F + 520)])
    B.wall('brick_white2', 'z', ez1, ex0, ex1, F, etop, -1, openings=[(-1020, -900, F, F + 240)])
    # the great doors (shut, the way out is their threshold)
    B.quad('window', [(-900, F, ez1 + 20), (-1020, F, ez1 + 20), (-1020, F + 240, ez1 + 20), (-900, F + 240, ez1 + 20)],
           color=(150, 110, 80))
    B.quad('carpet_red', [(-1000, F + 1, ez1), (-920, F + 1, ez1), (-920, F + 1, ez0), (-1000, F + 1, ez0)],
           uv=[(0, 320), (36, 320), (36, 0), (0, 0)])
    for xx in (-1150, -770):
        for zz in (1850, 2150):
            B.pillar(xx, zz, 34, F, etop)
    # The grand stair up the west side to the gallery (north)
    B.stairs('marble', -1400, -1200, 2300, 600, 300, 15, -1)
    B.railing(-1200, -1200, 2300, 1700, F, h=60)
    # The gallery along the north wall
    gy = F + 300
    B.floor('marble', -1400, -700, ez0, ez0 + 110, y=gy)
    B.quad('stone_band', [(-1200, gy - 20, ez0 + 110), (-700, gy - 20, ez0 + 110), (-700, gy, ez0 + 110),
                          (-1200, gy, ez0 + 110)])
    B.railing(-1200, -700, ez0 + 110, ez0 + 110, gy, h=60)
    B.railing(-700, -700, ez0, ez0 + 110, gy, h=60)

    # ---- Royal rooms (upstairs) ----
    rx0, rx1, rz0, rz1, rtop = -1700, -1100, 1300, 1660, gy + 300
    B.floor('carpet_red', rx0, rx1, rz0, rz1, y=gy)
    B.ceiling('ceiling', rx0, rx1, rz0, rz1, rtop)
    B.wall('plaster', 'x', rx0, rz0, rz1, gy, rtop, +1)
    B.wall('plaster', 'x', rx1, rz0, rz1, gy, rtop, -1)
    B.wall('plaster', 'z', rz0, rx0, rx1, gy, rtop, +1)
    B.wall('plaster', 'z', rz1, rx0, rx1, gy, rtop, -1, openings=[(-1400, -1200, gy, gy + 200)])
    # The royal portraits on the north wall
    for i, xx in enumerate(range(-1640, -1140, 100)):
        m = f'portrait{i + 1}'
        B.quad(m, [(xx, gy + 110, rz0 + 2), (xx + 70, gy + 110, rz0 + 2), (xx + 70, gy + 180, rz0 + 2),
                   (xx, gy + 180, rz0 + 2)], uv=[(0, 16), (16, 16), (16, 0), (0, 0)], light=False)
    B.box(('carpet_blue', 'stone_band'), -1660, -1520, gy, gy + 40, 1340, 1460)   # the bed
    B.box(('stone_band', 'marble2'), -1680, -1660, gy, gy + 120, 1330, 1470)      # its headboard
    return B


def emit_dl(B, dl_path, vtx_path, files, obj=False):
    verts = []
    dl = G.DL(dl_path)
    for mat, faces in B.faces.items():
        for c in material_cmds(MAT[mat]):
            dl.raw(c)
        if obj:
            # Drawn by an actor (after an actor's setup, lights on): the shading is baked into the vertex colours,
            # so lighting goes off, and the tint the scene gives this texture goes with it
            dl.w(0xD9FDFFFF, 0x00000004)
            dl.w(0xFA000000, OBJ_PRIM.get(mat, 0xFFFFFFFF))
        batch = []
        def flush():
            if not batch:
                return
            base = len(verts)
            local = []
            for f in batch:
                local.append(list(range(len(verts) - base, len(verts) - base + len(f))))
                verts.extend(f)
            dl.vtx(vtx_path, base, len(verts) - base)
            tris = []
            for idx in local:
                tris.append((idx[0], idx[1], idx[2]))
                if len(idx) == 4:
                    tris.append((idx[0], idx[2], idx[3]))
            for i in range(0, len(tris) - 1, 2):
                dl.tri2(*tris[i], *tris[i + 1])
            if len(tris) % 2:
                dl.tri1(*tris[-1])
            batch.clear()
        count = 0
        for f in faces:
            if count + len(f) > 32:
                flush()
                count = 0
            batch.append(f)
            count += len(f)
        flush()
    dl.pipesync()
    dl.end()
    files[vtx_path] = G.vtx_file(verts)
    files[dl_path] = dl.data()


OBJ_PRIM = {'bridge_edge': 0x4E1722FF}


def build_drawbridge():
    """The working drawbridge (hinge at the origin, lying along +z when down) and the dark gateway behind it"""
    B = Builder()
    w, t, L = 100, 14, 300
    B.quad('bridge', [(-w, 0, L), (w, 0, L), (w, 0, 0), (-w, 0, 0)], collide=SURF['floor'],
           uv=[(0, 0), (64, 0), (64, 96), (0, 96)])
    B.quad('bridge', [(-w, -t, 0), (w, -t, 0), (w, -t, L), (-w, -t, L)], collide=SURF['wall'],
           uv=[(0, 96), (64, 96), (64, 0), (0, 0)])
    B.quad('bridge_edge', [(-w, -t, L), (w, -t, L), (w, 0, L), (-w, 0, L)])
    B.quad('bridge_edge', [(w, -t, L), (w, -t, 0), (w, 0, 0), (w, 0, L)])
    B.quad('bridge_edge', [(-w, -t, 0), (-w, -t, L), (-w, 0, L), (-w, 0, 0)])
    D = Builder()
    dark = (45, 38, 36)
    D.quad('ceiling', [(-101, 0, -70), (101, 0, -70), (101, 296, -70), (-101, 296, -70)], color=dark, light=False)
    D.quad('ceiling', [(-101, 0, 0), (-101, 0, -70), (-101, 296, -70), (-101, 296, 0)], color=dark, light=False)
    D.quad('ceiling', [(101, 0, -70), (101, 0, 0), (101, 296, 0), (101, 296, -70)], color=dark, light=False)
    D.quad('ceiling', [(-101, 296, -70), (101, 296, -70), (101, 296, 0), (-101, 296, 0)], color=dark, light=False)
    D.quad('stone_court', [(-101, 0, 0), (101, 0, 0), (101, 0, -70), (-101, 0, -70)], color=(120, 110, 100))
    return B, D


def emit(B):
    files = {}
    emit_dl(B, BASE + f'{SCENE}_room_0DL_castle', BASE + f'{SCENE}_room_0Vtx_castle', files)
    dl_path = BASE + f'{SCENE}_room_0DL_castle'

    bridge, doorway = build_drawbridge()
    emit_dl(bridge, 'objects/gdf_castle/gGdfDrawbridgeDL', 'objects/gdf_castle/gGdfDrawbridgeVtx', files, obj=True)
    emit_dl(doorway, 'objects/gdf_castle/gGdfDoorwayDL', 'objects/gdf_castle/gGdfDoorwayVtx', files, obj=True)
    files['objects/gdf_castle/gGdfDrawbridgeCol'] = G.collision_file(bridge.col_verts, bridge.col_polys, SURFACES,
                                                                     cam_settings=(1,))

    # The courtyard outside the window: its own display lists, without the little throne room set behind the
    # window (its floor and back wall), which our throne room replaces
    skip = {'nakaniwa_room_0DL_0017B0', 'nakaniwa_room_0DL_004C28'}
    court_opa = G.DL(BASE + f'{SCENE}_room_0DL_court_opa')
    court_xlu = G.DL(BASE + f'{SCENE}_room_0DL_court_xlu')
    for top, out in (('scenes/shared/nakaniwa_scene/nakaniwa_room_0DL_007178', court_opa),
                     ('scenes/shared/nakaniwa_scene/nakaniwa_room_0DL_014E98', court_xlu)):
        for c in raw_cmds(top):
            if c[3] == 0x31:
                h = struct.unpack_from('<II', c, 8)
                sub = HASH[(h[0] << 32) | h[1]]
                if sub.split('/')[-1] not in skip:
                    out.call(sub)
        out.end()
    files[court_opa.path] = court_opa.data()
    files[court_xlu.path] = court_xlu.data()

    col_path = BASE + f'{SCENE}_sceneCollisionHeader_castle'
    files[col_path] = G.collision_file(B.col_verts, B.col_polys, SURFACES, cam_settings=(1, 1))
    room_path = BASE + f'{SCENE}_room_0'
    HEISHI3, ZL4 = 0x0142, 0x01D3
    guards = [(-1300, -170, 0x4000), (-1300, 170, 0x4000), (-1060, 760, 0x4000), (-860, 1160, -0x4000),
              (-600, 1260, -0x8000), (-420, 1300, 0), (-1110, 2250, -0x8000), (-810, 2250, -0x8000),
              (-1450, 1000, 0x4000), (-1250, 1500, 0x4000)]
    actors = [G.actor_entry(HEISHI3, (x, FLOOR if z < 1300 or x > -1100 else FLOOR + 300, z), (0, yaw, 0), 0)
              for x, z, yaw in guards]
    # Zelda at the courtyard window, peering in (her own spot in the courtyard)
    actors.append(G.actor_entry(ZL4, (-485, 84, 0), (0, -0x4000, 0), 0))
    files[room_path] = G.room_file([(dl_path, ''), (court_opa.path, court_xlu.path)], objects=(0x0097, 0x0191),
                                   actors=actors,
                                   echo=10, behavior=(0, 0), skymod=(0, 1))
    spawns = [
        G.actor_entry(0, (-960, FLOOR, 2280), (0, -0x8000, 0), 0x0FFF),   # inside the front doors, facing north
        G.actor_entry(0, (-700, FLOOR, 0), (0, -0x4000, 0), 0x0FFF),      # the throne room, by the window
    ]
    # Lighting from the courtyard (its fog and colours)
    lighting = [(100, 70, 70, -74, 100, -21, -1, -6, -6, -74, -100, 21, 20, 20, 60, 0, 0, 0, 2020, 12800),
                (105, 90, 90, 78, 98, -17, -1, -1, -16, -82, -20, -94, 50, 50, 90, 100, 100, 120, 2020, 12800)]
    files[BASE + f'{SCENE}_scene'] = G.scene_file([room_path], col_path, spawns, [(0, 0), (1, 0)],
                                                   exits=(0x0138,), lighting=lighting, sound=(0, 19, 0x29),
                                                   special=(0, 3), sky=(0, 0, 0, 1), camera=(0, 0))
    return files


if __name__ == '__main__':
    B = build()
    files = emit(B)
    out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/claude-0/-home-claude/7ba8c2c1-f15e-5c38-bc23-64c21735746e/scratchpad/castle.o2r'
    G.pack(files, out)
    print('faces', sum(len(v) for v in B.faces.values()), 'col polys', len(B.col_polys), 'files', len(files))
    for k, v in files.items():
        print(len(v), k)
