# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""Hyrule Castle interior for "Play as Ganondorf": a castle wing in the style of the courtyard and the Temple of Time,
generated as a scene for the unused SCENE_TESTROOM slot (scenes/shared/testroom_scene). It's built in the courtyard's
own coordinates, so the throne room's east wall holds the courtyard window: the real courtyard (its display lists,
called from oot.o2r) is outside, with Zelda's garden.

Layout (floor y 20, x west negative):
  Throne room   x -1150..-575  z -460..460   floor y 84 (the courtyard's), throne on the south side; the window
                in the east wall looks across the carpet where Ganondorf kneels
  Grand hall    x -1100..-820  z  460..1660  library door west, barracks door east
  Library       x -1700..-1100 z  700..1300
  Barracks      x  -820..-260  z 1000..1500
  Entrance hall x -1400..-520  z 1660..2360  front doors south, grand stair up the west side to
  Gallery       y 320 along the entrance hall's north wall, and the
  Royal rooms   x -1700..-1100 z 1300..1660  floor y 320
  Garden court  x  -520..0     z 1500..2300  glass roof, fountain, flowerbeds, hedges; doors from the entrance hall
                (east wall) and the barracks (south wall); sunlit windows east and south
  Long gallery  x -1950..-1700 z  450..1660  door from the library's west wall, wall fountain at the south end
  Bedrooms      x -2350..-1950 z  450..850, 850..1250, 1250..1660 (doors off the gallery's west wall)

Lit like the Temple of Time (LitBuilder): lamps, chandeliers and windows baked into the vertex colours over a bright
ambient, two passes (the first finds the lights). Windows carry the Temple of Time's sunbeam (Demo_Kankyo light plane,
scaled by GanondorfCastle.cpp); the water is drawn translucent with the courtyard's own animated water.
"""
import math, struct, sys, numpy as np
sys.path.insert(0, '/home/claude/o2r')
from otr import z as OOT, HASH
from matlib import materials, raw_cmds
import o2rgen as G

SCENE = 'testroom'
BASE = f'scenes/shared/{SCENE}_scene/'
FLOOR = 20
THRONE_FLOOR = 84  # the courtyard's height: the room Zelda's window looks into

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
    # greenery and water, from the courtyard and the castle grounds
    'plant': tex('nakaniwa_room_0Tex_008618'),    # a leafy potted plant (cut out)
    'flowers': tex('nakaniwa_room_0Tex_008E18'),  # yellow flowers (cut out)
    'leaves': tex('nakaniwa_room_0Tex_00CE18'),   # dense dark leaves (flowerbeds, soil)
    'grass': tex('spot15_sceneTex_00CA00'),       # tufts of grass (cut out)
    'water': tex('nakaniwa_room_0Tex_0166C0'),    # the courtyard's water (translucent, animated)
    'ivy': tex('spot15_sceneTex_012400'),         # the castle wall's ivy (cut out)
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
        self.fades = []

    def fade_to_black(self, start, end, box):
        """A passage that fades into darkness (the way OoT's corridors look as they lead out of a room): inside
        box (x0, x1, y0, y1, z0, z1) the colours fall to black from the point start to the point end."""
        self.fades.append((np.asarray(start, float), np.asarray(end, float), box))

    def dim(self, p):
        k = 1.0
        for a, b, (x0, x1, y0, y1, z0, z1) in self.fades:
            if x0 - 1 <= p[0] <= x1 + 1 and y0 - 1 <= p[1] <= y1 + 1 and z0 - 1 <= p[2] <= z1 + 1:
                d = b - a
                f = min(1.0, max(0.0, float(np.dot(np.asarray(p, float) - a, d) / np.dot(d, d))))
                k *= (1.0 - f) ** 1.6
        return k

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
            k = shade * (1.0 - 0.12 * min(1.0, max(0.0, (y - FLOOR) / 520.0))) * self.dim((x, y, zz))
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


# ---- Baked lighting and sunlit windows (shared with sacred.py) ----

GOLD = (255, 205, 110)
WARM = (1.0, 0.9, 0.72)
# The Temple of Time's own sunbeam (Demo_Kankyo's light plane, object_toki_objects_DL_008390, drawn by the actor):
# a pointed window of light at its back (local z -68, y 100..661) and rays falling from its origin, down and forward
# (to y -939 some 600..1040 ahead). The game code scales it to the room.
DEMO_KANKYO, LIGHT_PLANE, OBJECT_TOKI_OBJECTS = 0x008C, 0x000E, 0x005E


class LitBuilder(Builder):
    """The castle's builder, lit the way the Temple of Time is: its surfaces cut into small pieces, each corner lit by
    the room's lights (pools of warm light, the mirror's glow, the Sages' colours over their pillars) over a dim
    ambient, so the rooms fall off into shadow between the lights instead of being one flat brightness.
    lights: (position, radius, intensity, (r, g, b) tint)."""
    CELL = 90.0
    AMBIENT = 0.30

    def __init__(self, lights=None, height=420.0):
        super().__init__()
        self.lights = lights
        self.height = height

    def lum(self, p, n):
        k = np.full(3, self.AMBIENT)
        for pos, radius, inten, tint in self.lights:
            d = np.asarray(pos, float) - p
            dist = np.linalg.norm(d)
            fall = max(0.0, 1.0 - dist / radius) ** 2
            if fall <= 0.0:
                continue
            facing = max(0.25, float(np.dot(n, d / max(dist, 1e-6))))
            k = k + inten * fall * facing * np.asarray(tint, float)
        k *= 1.0 - 0.3 * min(1.0, max(0.0, p[1] / self.height))  # dimmer up toward the ceiling
        return np.minimum(k, 1.35)

    def quad(self, mat, pts, collide=None, color=(255, 255, 255), uv=None, light=True):
        if self.lights is None or (not light and not self.fades):
            return super().quad(mat, pts, collide=collide, color=color, uv=uv, light=light)
        P = np.array(pts, float)
        n = np.cross(P[1] - P[0], P[2] - P[0])
        n /= max(np.linalg.norm(n), 1e-9)
        # The texture coordinates as the castle's builder gives them, for the whole face
        probe = Builder()
        probe.quad(mat, pts, color=(255, 255, 255), uv=uv, light=False)
        UV = np.array([(v[3], v[4]) for v in probe.faces[mat][0]], float)
        if collide is not None:
            idx = [self.cv(q) for q in P]
            for tri in ((0, 1, 2), (0, 2, 3)) if len(idx) == 4 else ((0, 1, 2),):
                self.col_polys.append((collide, idx[tri[0]], idx[tri[1]], idx[tri[2]], 0, 0))
        def vert(q, t):
            c = np.asarray(color, float) * (self.lum(q, n) if light else 1.0) * self.dim(q)
            return (q[0], q[1], q[2], t[0], t[1], int(min(255, c[0])), int(min(255, c[1])), int(min(255, c[2])), 255)
        if len(P) != 4:
            self.faces.setdefault(mat, []).append([vert(P[i], UV[i]) for i in range(len(P))])
            return
        nu = max(1, int(np.ceil(np.linalg.norm(P[1] - P[0]) / self.CELL)))
        nv = max(1, int(np.ceil(np.linalg.norm(P[3] - P[0]) / self.CELL)))
        def at(a, b):  # bilinear across the quad (0,0)=p0, (1,0)=p1, (1,1)=p2, (0,1)=p3
            q = (1 - a) * (1 - b) * P[0] + a * (1 - b) * P[1] + a * b * P[2] + (1 - a) * b * P[3]
            t = (1 - a) * (1 - b) * UV[0] + a * (1 - b) * UV[1] + a * b * UV[2] + (1 - a) * b * UV[3]
            return q, t
        for i in range(nu):
            for j in range(nv):
                cs = [at(i / nu, j / nv), at((i + 1) / nu, j / nv), at((i + 1) / nu, (j + 1) / nv),
                      at(i / nu, (j + 1) / nv)]
                self.faces.setdefault(mat, []).append([vert(q, t) for q, t in cs])

def tot_window(B, wall, pos, w, y_lo, y_hi):
    """The Temple of Time's window (tokinoma_room_0DL_003988): a pointed arch of its white tracery (the lattice
    texture, mapped as the temple maps it: two repeats across, the arch's point at the top) over pale glass, in a
    slim marble surround. Drawn on the wall's face, where the sunbeam's own window of light falls."""
    x, z = pos
    if isinstance(wall, str):
        inward = np.array({'W': (1, 0, 0), 'E': (-1, 0, 0), 'N': (0, 0, 1), 'S': (0, 0, -1)}[wall], float)
    else:  # any wall: its inward direction (x, z)
        inward = np.array([wall[0], 0, wall[1]], float)
    along = np.array([-inward[2], 0, inward[0]])  # u runs along the wall

    def P(u, y, d):
        return tuple(np.array([x, 0, z]) + along * u + inward * d + np.array([0, y, 0]))

    def face(mat, pts, uv=None, color=(255, 255, 255), light=False):
        p = np.array(pts, float)
        n = np.cross(p[1] - p[0], p[2] - p[0])
        if np.dot(n, inward) < 0:
            pts = pts[::-1]
            uv = uv[::-1] if uv is not None else None
        B.quad(mat, list(pts), uv=uv, color=color, light=light)

    y_mid = y_lo + 0.75 * (y_hi - y_lo)
    glass = (255, 250, 232)
    # pale glass behind the tracery, then the tracery itself
    face('plaster', [P(-w, y_lo, 2), P(w, y_lo, 2), P(w, y_mid, 2), P(-w, y_mid, 2)], color=glass)
    face('plaster', [P(-w, y_mid, 2), P(w, y_mid, 2), P(0, y_hi, 2)], color=glass)
    face('lattice', [P(-w, y_lo, 4), P(w, y_lo, 4), P(w, y_mid, 4), P(-w, y_mid, 4)],
         uv=[(0, 64), (64, 64), (64, 16), (0, 16)])
    face('lattice', [P(-w, y_mid, 4), P(w, y_mid, 4), P(0, y_hi, 4)], uv=[(0, 16), (64, 16), (32, 0)])
    # the surround: jambs, the two sides of the arch and a sill, standing a little out of the wall
    t, d = 9, 7
    for s in (-1, 1):
        face('marble2', [P(s * w, y_lo - 6, d), P(s * (w + t), y_lo - 6, d), P(s * (w + t), y_mid, d), P(s * w, y_mid, d)],
             light=True)
        face('marble2', [P(s * w, y_mid, d), P(s * (w + t), y_mid, d), P(0, y_hi + t * 1.4, d), P(0, y_hi, d)], light=True)
    face('marble2', [P(-w - t - 6, y_lo - 14, d + 6), P(w + t + 6, y_lo - 14, d + 6), P(w + t + 6, y_lo - 6, d + 6),
                     P(-w - t - 6, y_lo - 6, d + 6)], light=True)
    face('marble2', [P(-w - t - 6, y_lo - 6, d + 6), P(w + t + 6, y_lo - 6, d + 6), P(w + t + 6, y_lo - 6, 0),
                     P(-w - t - 6, y_lo - 6, 0)], light=True)


def window(B, lights, beams, wall, pos, y0, scale, frame=True, floor=0.0):
    """A window in a wall with the sunbeam falling through it into the room.
    wall: 'W' (x = pos[0], facing +x), 'E', 'S' (z = pos[1], facing -z) or 'N'; pos: (x, z) on the wall's plane;
    y0: the beam's origin (the window's foot is a little above it)."""
    inward = {'W': (1, 0), 'E': (-1, 0), 'N': (0, 1), 'S': (0, -1)}[wall]
    yaw = {'W': 0x4000, 'E': -0x4000, 'N': 0, 'S': -0x8000}[wall]
    ox, oz = pos[0] + inward[0] * 68 * scale, pos[1] + inward[1] * 68 * scale
    beams.append(((ox, y0, oz), yaw, scale))
    w, y_lo, y_hi = 96 * scale, y0 + 100 * scale, y0 + 661 * scale
    if frame:
        tot_window(B, wall, pos, w, y_lo, y_hi)
    # Light: the window itself, and the pool where its rays reach the floor
    reach = 825 * scale * min(1.0, (y0 - floor) / (939 * scale))
    lights.append(((pos[0] + inward[0] * 30, (y_lo + y_hi) / 2, pos[1] + inward[1] * 30), 300, 0.55, WARM))
    lights.append(((ox + inward[0] * reach, floor + 30, oz + inward[1] * reach), 230, 0.75, (1.0, 0.95, 0.8)))



# Surface types (data[1], data[0]) from the Temple of Time: stone floor, wall; the front door floor carries exit 1
SURFACES = [(0x00004802, 0x00000000), (0x00002FCD, 0x00000000), (0x00004802, 0x00000100)]
SURF = {'floor': 0, 'wall': 1, 'exit': 2}


CASTLE_BEAM = 0.32  # the windows' default sunbeam scale (each beam carries its own: see emit())
WOOD = (150, 105, 75)
TERRACOTTA = (205, 125, 85)
LAMP = (1.0, 0.72, 0.42)


def cross(B, mat, x, z, y, w, h, tw, th, color=(255, 255, 255)):
    """A cut-out plant: two crossed upright quads, each seen from both sides."""
    uv = [(0.5, th - 0.5), (tw - 0.5, th - 0.5), (tw - 0.5, 0.5), (0.5, 0.5)]
    for a in (0.4, 0.4 + math.pi / 2):
        dx, dz = math.cos(a) * w / 2, math.sin(a) * w / 2
        p = [(x - dx, y, z - dz), (x + dx, y, z + dz), (x + dx, y + h, z + dz), (x - dx, y + h, z - dz)]
        B.quad(mat, p, uv=uv, color=color)
        B.quad(mat, [p[1], p[0], p[3], p[2]], uv=[uv[1], uv[0], uv[3], uv[2]], color=color)


def potted(B, x, z, y=None, big=False):
    y = FLOOR if y is None else y
    r = 26 if big else 20
    B.box(('leaves', 'brick_court'), x - r, x + r, y, y + 40, z - r, z + r, color=TERRACOTTA)
    B.box('stone_band', x - r - 3, x + r + 3, y + 34, y + 42, z - r - 3, z + r + 3, collide=False, color=TERRACOTTA)
    cross(B, 'plant', x, z, y + 30, 90 if big else 70, 110 if big else 85, 32, 32)


def flowerbed(B, x0, x1, z0, z1, y=None):
    y = FLOOR if y is None else y
    B.box(('leaves', 'stone_band'), x0, x1, y, y + 30, z0, z1)
    nx, nz = max(1, int((x1 - x0) // 45)), max(1, int((z1 - z0) // 45))
    for i in range(nx):
        for j in range(nz):
            cx = x0 + (i + 0.5) * (x1 - x0) / nx
            cz = z0 + (j + 0.5) * (z1 - z0) / nz
            cross(B, 'flowers' if (i + j) % 2 == 0 else 'plant', cx, cz, y + 28, 40, 55, 32, 64 if (i + j) % 2 == 0 else 32)


def hedge(B, x0, x1, z0, z1, h=80, y=None):
    y = FLOOR if y is None else y
    B.box(('hedge', 'hedge'), x0, x1, y, y + h, z0, z1)


def water(W, x0, x1, z0, z1, y):
    W.quad('water', [(x0, y, z1), (x1, y, z1), (x1, y, z0), (x0, y, z0)], light=False)


def falling(W, a, b, y0, y1):
    """A sheet of falling water between two points (seen from both sides)."""
    p = [(a[0], y1, a[1]), (b[0], y1, b[1]), (b[0], y0, b[1]), (a[0], y0, a[1])]
    W.quad('water', p, light=False, color=(230, 240, 255))
    W.quad('water', [p[1], p[0], p[3], p[2]], light=False, color=(230, 240, 255))


def fountain(B, W, cx, cz, r, y=None):
    """A marble basin, a column with a bowl on top and water falling from the bowl into the basin."""
    y = FLOOR if y is None else y
    t, h = 18, 42
    B.box(('marble', 'marble2'), cx - r, cx + r, y, y + h, cz - r, cz - r + t)
    B.box(('marble', 'marble2'), cx - r, cx + r, y, y + h, cz + r - t, cz + r)
    B.box(('marble', 'marble2'), cx - r, cx - r + t, y, y + h, cz - r + t, cz + r - t)
    B.box(('marble', 'marble2'), cx + r - t, cx + r, y, y + h, cz - r + t, cz + r - t)
    B.quad('tile_court', [(cx - r + t, y + 1, cz + r - t), (cx + r - t, y + 1, cz + r - t), (cx + r - t, y + 1, cz - r + t),
                          (cx - r + t, y + 1, cz - r + t)], color=(120, 150, 170))
    water(W, cx - r + t, cx + r - t, cz - r + t, cz + r - t, y + h - 10)
    B.box(('marble', 'pillar'), cx - 16, cx + 16, y, y + 120, cz - 16, cz + 16)
    k = 50
    B.box(('marble2', 'marble'), cx - k, cx + k, y + 120, y + 136, cz - k, cz + k, collide=False)
    B.box('stone_band', cx - k - 4, cx + k + 4, y + 132, y + 138, cz - k - 4, cz + k + 4, collide=False, color=GOLD)
    water(W, cx - k + 4, cx + k - 4, cz - k + 4, cz + k - 4, y + 137)
    B.box('marble', cx - 7, cx + 7, y + 136, y + 178, cz - 7, cz + 7, collide=False)
    B.box('stone_band', cx - 11, cx + 11, y + 178, y + 192, cz - 11, cz + 11, collide=False, color=GOLD)
    e = k + 6
    for a, b in (((cx - e, cz - e), (cx + e, cz - e)), ((cx + e, cz - e), (cx + e, cz + e)),
                 ((cx + e, cz + e), (cx - e, cz + e)), ((cx - e, cz + e), (cx - e, cz - e))):
        falling(W, a, b, y + h - 10, y + 134)
    falling(W, (cx - 12, cz), (cx + 12, cz), y + 137, y + 190)
    falling(W, (cx, cz - 12), (cx, cz + 12), y + 137, y + 190)


def bed(B, x0, x1, z0, z1, y, canopy=(170, 40, 50), blanket='carpet_blue'):
    """A four-poster bed, its head against the wall at z0."""
    B.box(('stone_band', 'stone_band'), x0, x1, y, y + 28, z0, z1, color=WOOD)
    B.box((blanket, 'plaster'), x0 + 6, x1 - 6, y + 28, y + 48, z0 + 6, z1 - 6, color=(235, 235, 240))
    xm = (x0 + x1) / 2
    for a, b in ((x0 + 12, xm - 4), (xm + 4, x1 - 12)):  # two pillows
        B.box('plaster', a, b, y + 48, y + 60, z0 + 10, z0 + 44, collide=False, color=(250, 250, 255))
    B.box('stone_band', x0 - 2, x1 + 2, y + 14, y + 30, z1 - 4, z1 + 2, collide=False, color=canopy)  # the bed skirt
    B.box(('stone_band', 'stone_band'), x0, x1, y, y + 140, z0 - 10, z0 + 2, color=WOOD)
    B.box('stone_band', x0 + 10, x1 - 10, y + 120, y + 132, z0 + 2, z0 + 5, collide=False, color=GOLD)
    for px in (x0 + 5, x1 - 5):
        for pz in (z0 - 5, z1 - 5):
            prism(B, 'stone_band', px, pz, 6, y, y + 230, 8, color=WOOD, collide=pz == z1 - 5)
            prism(B, 'stone_band', px, pz, 8, y + 246, y + 262, 8, color=GOLD, r1=2)
    B.box('carpet_red', x0 - 4, x1 + 4, y + 230, y + 246, z0 - 14, z1 + 4, collide=False, color=canopy)
    # curtains gathered at the foot posts, hanging from the canopy
    for px, s in ((x0 + 5, 1), (x1 - 5, -1)):
        for p in ([(px, y + 70, z1 - 4), (px + s * 30, y + 70, z1 - 4), (px + s * 22, y + 230, z1 - 4), (px, y + 230, z1 - 4)],
                  [(px, y + 70, z1 - 6), (px, y + 70, z1 - 40), (px, y + 230, z1 - 32), (px, y + 230, z1 - 6)]):
            B.quad('carpet_red', p, color=canopy, uv=[(0, 70), (14, 70), (10, 0), (0, 0)])
            B.quad('carpet_red', [p[1], p[0], p[3], p[2]], color=canopy, uv=[(14, 70), (0, 70), (0, 0), (10, 0)])


# ---- The architecture kit: what every room we build gets (castle, temples, dungeons, houses) ----
# Rooms are never bare boxes: coved ceilings with a gilded band, skirting, a dado and a chair rail, pilasters along
# long walls, round-arched doorways with a deep soffit, and decor (banners, candelabras, statues, rugs, plants).
# Use room_finish() for each room, arch_door() for each doorway; the shapes below are the building blocks.

DADO = (128, 96, 74)       # the dark lower wall band
TRIM = (225, 215, 195)     # stone mouldings
IRON2 = (60, 56, 54)


def oriented(B, mat, pts, hint, **kw):
    """A face whose front is turned toward hint (a direction)."""
    p = np.array(pts, float)
    n = np.cross(p[1] - p[0], p[2] - p[0])
    if np.dot(n, hint) < 0:
        pts = list(pts)[::-1]
        if kw.get('uv') is not None:
            kw['uv'] = list(kw['uv'])[::-1]
    B.quad(mat, list(pts), **kw)


def prism(B, mat, x, z, r, y0, y1, n=8, color=(255, 255, 255), top=None, collide=False, r1=None, rot=None):
    """An n-sided upright prism (or a tapered one, r at the foot, r1 at the top), its sides and its top."""
    r1 = r if r1 is None else r1
    rot = math.pi / n if rot is None else rot
    ring0 = [(x + r * math.cos(rot + 2 * math.pi * i / n), z + r * math.sin(rot + 2 * math.pi * i / n)) for i in range(n)]
    ring1 = [(x + r1 * math.cos(rot + 2 * math.pi * i / n), z + r1 * math.sin(rot + 2 * math.pi * i / n)) for i in range(n)]
    for i in range(n):
        a0, a1, b0, b1 = ring0[i], ring0[(i + 1) % n], ring1[i], ring1[(i + 1) % n]
        mx, mz = (a0[0] + a1[0]) / 2 - x, (a0[1] + a1[1]) / 2 - z
        oriented(B, mat, [(a0[0], y0, a0[1]), (a1[0], y0, a1[1]), (b1[0], y1, b1[1]), (b0[0], y1, b0[1])], (mx, 0, mz),
                 color=color, collide=SURF['wall'] if collide else None)
    if top:
        disc(B, top, x, z, r1, y1, n, color=color, rot=rot)


def disc(B, mat, x, z, r, y, n=16, color=(255, 255, 255), rot=None, up=True, light=True):
    rot = math.pi / n if rot is None else rot
    for i in range(n):
        a0, a1 = rot + 2 * math.pi * i / n, rot + 2 * math.pi * (i + 1) / n
        oriented(B, mat, [(x, y, z), (x + r * math.cos(a0), y, z + r * math.sin(a0)),
                          (x + r * math.cos(a1), y, z + r * math.sin(a1))], (0, 1 if up else -1, 0), color=color,
                 light=light)


def annulus(B, mat, x, z, r0, r1, y, n=16, color=(255, 255, 255), collide=False):
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        p = [(x + r0 * math.cos(a0), y, z + r0 * math.sin(a0)), (x + r1 * math.cos(a0), y, z + r1 * math.sin(a0)),
             (x + r1 * math.cos(a1), y, z + r1 * math.sin(a1)), (x + r0 * math.cos(a1), y, z + r0 * math.sin(a1))]
        oriented(B, mat, p, (0, 1, 0), color=color, collide=SURF['floor'] if collide else None)


def tube(B, mat, x, z, r, y0, y1, n=16, color=(255, 255, 255), inward=False, collide=False):
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        p = [(x + r * math.cos(a0), y0, z + r * math.sin(a0)), (x + r * math.cos(a1), y0, z + r * math.sin(a1)),
             (x + r * math.cos(a1), y1, z + r * math.sin(a1)), (x + r * math.cos(a0), y1, z + r * math.sin(a0))]
        am = (a0 + a1) / 2
        s = -1 if inward else 1
        oriented(B, mat, p, (s * math.cos(am), 0, s * math.sin(am)), color=color,
                 collide=SURF['wall'] if collide else None)


def cove_ceiling(B, x0, x1, z0, z1, top, c=60, mat='ceiling', color=(235, 232, 240), steps=5, band=GOLD,
                 centre=None, centre_color=None, centre_light=True):
    """A coved ceiling: a quarter-round sweep up from the top of the walls, a gilded band at its foot and a flat
    centre (or a glass one) above."""
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    prof = [(c - c * math.cos(0.5 * math.pi * i / steps), top + c * math.sin(0.5 * math.pi * i / steps))
            for i in range(steps + 1)]
    for (i0, y0), (i1, y1) in zip(prof, prof[1:]):
        rings = []
        for ins, yy in ((i0, y0), (i1, y1)):
            rings.append([(x0 + ins, yy, z0 + ins), (x1 - ins, yy, z0 + ins), (x1 - ins, yy, z1 - ins),
                          (x0 + ins, yy, z1 - ins)])
        for k in range(4):
            a, b = rings[0][k], rings[0][(k + 1) % 4]
            a2, b2 = rings[1][k], rings[1][(k + 1) % 4]
            mid = np.mean([a, b], 0)
            oriented(B, mat, [a, b, b2, a2], (cx - mid[0], -60, cz - mid[2]), color=color)
    yc = top + c
    if centre is None:
        B.ceiling(mat, x0 + c, x1 - c, z0 + c, z1 - c, yc, color=color)
    else:
        oriented(B, centre, [(x0 + c, yc, z0 + c), (x1 - c, yc, z0 + c), (x1 - c, yc, z1 - c), (x0 + c, yc, z1 - c)],
                 (0, -1, 0), color=centre_color or color, light=centre_light)
    if band:
        frieze(B, x0, x1, z0, z1, top - 14, top, 5, band)
    return yc


def _wall_runs(a, b, doors, y0, y1):
    """The stretches of a wall (a..b) that a moulding from y0 to y1 runs along, around the doorways."""
    runs, u = [], a
    for d0, d1, dy0, dy1 in sorted(doors):
        if dy0 < y1 and dy1 > y0:
            if d0 > u:
                runs.append((u, d0))
            u = max(u, d1)
    if u < b:
        runs.append((u, b))
    return runs


def _moulding(B, wall, x0, x1, z0, z1, u0, u1, y0, y1, depth, mat, color):
    if wall == 'N':
        B.box(mat, u0, u1, y0, y1, z0, z0 + depth, collide=False, color=color)
    elif wall == 'S':
        B.box(mat, u0, u1, y0, y1, z1 - depth, z1, collide=False, color=color)
    elif wall == 'W':
        B.box(mat, x0, x0 + depth, y0, y1, u0, u1, collide=False, color=color)
    else:
        B.box(mat, x1 - depth, x1, y0, y1, u0, u1, collide=False, color=color)


def frieze(B, x0, x1, z0, z1, y0, y1, depth, color, doors=None, mat='stone_band'):
    doors = doors or {}
    for wall in 'NSWE':
        a, b = (x0, x1) if wall in 'NS' else (z0, z1)
        for u0, u1 in _wall_runs(a, b, doors.get(wall, ()), y0, y1):
            _moulding(B, wall, x0, x1, z0, z1, u0, u1, y0, y1, depth, mat, color)


def room_finish(B, x0, x1, z0, z1, y, top, doors=None, walls='NSWE', dado=DADO, rail=95, pilasters=True,
                pil_step=260, pil_mat='marble', skip=()):
    """Skirting, a dark dado with a chair rail, a crown moulding and pilasters along the walls.
    doors: {'N': [(u0, u1, y0, y1), ...], ...} (u along the wall: x for N/S, z for W/E), including windows and
    anything else the mouldings must stop at; skip: [(wall, u0, u1)] stretches kept clear (shelves, stairs)."""
    doors = {k: list(v) for k, v in (doors or {}).items()}
    for wall, u0, u1 in skip:
        doors.setdefault(wall, []).append((u0, u1, -1e9, 1e9))
    for wall in walls:
        a, b = (x0, x1) if wall in 'NS' else (z0, z1)
        ds = doors.get(wall, ())
        for u0, u1 in _wall_runs(a, b, ds, y, y + rail + 8):
            _moulding(B, wall, x0, x1, z0, z1, u0, u1, y + 18, y + rail, 2, 'stone_band', dado)   # the dado
            _moulding(B, wall, x0, x1, z0, z1, u0, u1, y, y + 18, 7, 'stone_band', (90, 70, 58))  # skirting
        for u0, u1 in _wall_runs(a, b, ds, y + rail, y + rail + 9):
            _moulding(B, wall, x0, x1, z0, z1, u0, u1, y + rail, y + rail + 9, 6, 'stone_band', TRIM)  # chair rail
        for u0, u1 in _wall_runs(a, b, ds, top - 30, top - 14):
            _moulding(B, wall, x0, x1, z0, z1, u0, u1, top - 30, top - 14, 9, 'stone_band', TRIM)  # crown
        if pilasters and b - a > 2.2 * pil_step:
            n = int((b - a) // pil_step)
            for i in range(1, n):
                u = a + (b - a) * i / n
                if any(d0 - 40 < u < d1 + 40 for d0, d1, dy0, dy1 in ds):
                    continue
                _moulding(B, wall, x0, x1, z0, z1, u - 16, u + 16, y + 18, top - 30, 12, pil_mat, (240, 236, 228))
                _moulding(B, wall, x0, x1, z0, z1, u - 22, u + 22, top - 44, top - 30, 16, 'stone_band', TRIM)
                _moulding(B, wall, x0, x1, z0, z1, u - 22, u + 22, y, y + 26, 16, 'stone_band', TRIM)


def arch_door(B, axis, fixed, u0, u1, y0, y1, depth=12, color=TRIM, fill_mat='brick_white', fill_color=(255, 255, 255)):
    """Rounds the top of a doorway cut in a wall (axis 'x': the wall x = fixed, u along z; 'z': z = fixed, u along x):
    the wall filled in above the arch on both faces, the arch's soffit and jambs given depth, and a moulded
    architrave round it on both sides."""
    r = (u1 - u0) / 2
    uc = (u0 + u1) / 2
    ys = y1 - r if y1 - r > y0 + 40 else y0 + 40
    rr = min(r, y1 - ys)
    n = 10
    def P(u, y, d):
        return (fixed + d, y, u) if axis == 'x' else (u, y, fixed + d)
    nrm = (1, 0, 0) if axis == 'x' else (0, 0, 1)
    nneg = tuple(-c for c in nrm)
    pts = [(uc - r * math.cos(math.pi * i / n), ys + rr * math.sin(math.pi * i / n)) for i in range(n + 1)]
    for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
        quad = [P(ua, ya, 0), P(ub, yb, 0), P(ub, y1, 0), P(ua, y1, 0)]
        oriented(B, fill_mat, quad, nrm, color=fill_color)
        oriented(B, fill_mat, quad, nneg, color=fill_color)
        # the soffit: the arch's underside, through the wall's depth
        soff = [P(ua, ya, -depth), P(ub, yb, -depth), P(ub, yb, depth), P(ua, ya, depth)]
        mu, my = (ua + ub) / 2 - uc, (ya + yb) / 2 - ys
        hint = np.array(P(-mu, -my, 0)) - np.array(P(0, 0, 0))
        oriented(B, 'stone_band', soff, hint, color=color)
        # the architrave: a band round the arch on each face
        k = 14
        ca = ((ua - uc) / r, (ya - ys) / max(rr, 1))
        cb = ((ub - uc) / r, (yb - ys) / max(rr, 1))
        for d, h in ((depth + 0.5, nrm), (-depth - 0.5, nneg)):
            band = [P(ua, ya, d), P(ub, yb, d), P(ub + cb[0] * k, yb + cb[1] * k, d), P(ua + ca[0] * k, ya + ca[1] * k, d)]
            oriented(B, 'stone_band', band, h, color=color)
    for u, s in ((u0, 1), (u1, -1)):  # the jambs
        jamb = [P(u, y0, -depth), P(u, y0, depth), P(u, ys, depth), P(u, ys, -depth)]
        hint = np.array(P(uc, 0, 0)) - np.array(P(u, 0, 0))
        oriented(B, 'stone_band', jamb, hint, color=color)
        for d, h in ((depth + 0.5, nrm), (-depth - 0.5, nneg)):
            band = [P(u, y0, d), P(u - s * 14, y0, d), P(u - s * 14, ys, d), P(u, ys, d)]
            oriented(B, 'stone_band', band, h, color=color)


def banner(B, wall, x, z, top, w=70, h=230, mat='carpet_red', color=(255, 255, 255), rod=GOLD):
    """A hanging banner with a pointed foot, on a gilded rod, a little out from the wall."""
    inward = {'W': (1, 0, 0), 'E': (-1, 0, 0), 'N': (0, 0, 1), 'S': (0, 0, -1)}[wall]
    along = (-inward[2], 0, inward[0])
    def P(u, y, d):
        return (x + along[0] * u + inward[0] * d, y, z + along[2] * u + inward[2] * d)
    tw = 32
    oriented(B, mat, [P(-w / 2, top - h + 40, 3), P(w / 2, top - h + 40, 3), P(w / 2, top, 3), P(-w / 2, top, 3)], inward,
             uv=[(0, h / 2.2), (tw, h / 2.2), (tw, 0), (0, 0)], color=color)
    oriented(B, mat, [P(-w / 2, top - h + 40, 3), P(0, top - h, 3), P(w / 2, top - h + 40, 3)], inward,
             uv=[(0, h / 2.2), (tw / 2, h / 2.2 + 18), (tw, h / 2.2)], color=color)
    for s in (-1, 1):  # gold edging
        oriented(B, 'stone_band', [P(s * w / 2 - 3, top - h + 40, 3.5), P(s * w / 2 + 3, top - h + 40, 3.5),
                                   P(s * w / 2 + 3, top, 3.5), P(s * w / 2 - 3, top, 3.5)], inward, color=rod)
    oriented(B, 'stone_band', [P(-w / 2 - 10, top - 4, 6), P(w / 2 + 10, top - 4, 6), P(w / 2 + 10, top + 6, 6),
                               P(-w / 2 - 10, top + 6, 6)], inward, color=rod)


def candelabra(B, light, x, z, y, h=150):
    """A standing gilded candelabra with three candles."""
    prism(B, 'stone_band', x, z, 16, y, y + 8, 8, color=GOLD, top='stone_band')
    prism(B, 'stone_band', x, z, 4, y + 8, y + h, 6, color=GOLD)
    B.box('stone_band', x - 26, x + 26, y + h - 4, y + h + 2, z - 3, z + 3, collide=False, color=GOLD)
    for dx in (-24, 0, 24):
        cy = y + h + (14 if dx == 0 else 2)
        prism(B, 'plaster', x + dx, z, 3, cy, cy + 22, 6, color=(250, 245, 230))
        prism(B, 'lattice', x + dx, z, 2.5, cy + 22, cy + 30, 4, color=(255, 210, 120), r1=0.5)
    light((x, y + h + 40, z), 300, 0.55, LAMP)


def statue(B, x, z, y, face='S', color=(205, 205, 210)):
    """A knight in stone on a plinth, shield and spear."""
    prism(B, 'marble2', x, z, 34, y, y + 40, 8, color=(230, 228, 222), top='marble', collide=True)
    y += 40
    f = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[face]
    sx, sz = f[1], f[0]  # sideways
    for s in (-1, 1):
        prism(B, 'marble', x + s * 9 * sx, z + s * 9 * sz, 8, y, y + 70, 6, color=color, r1=9)
    B.box('marble', x - 20, x + 20, y + 70, y + 140, z - 20, z + 20, collide=False, color=color)
    B.box('marble', x - 26, x + 26, y + 125, y + 145, z - 24, z + 24, collide=False, color=color)
    prism(B, 'marble', x, z, 12, y + 145, y + 175, 8, color=color, top='marble')
    prism(B, 'marble', x, z, 4, y + 175, y + 190, 4, color=color, r1=1)
    # shield in front of the left arm, spear in the right hand
    px, pz = x - 30 * sx + f[0] * 18, z - 30 * sz + f[1] * 18
    B.box('stone_band', px - 14 * abs(sx) - 3 * abs(f[0]), px + 14 * abs(sx) + 3 * abs(f[0]), y + 70, y + 130,
          pz - 14 * abs(sz) - 3 * abs(f[1]), pz + 14 * abs(sz) + 3 * abs(f[1]), collide=False, color=(170, 165, 160))
    qx, qz = x + 30 * sx, z + 30 * sz
    prism(B, 'stone_band', qx, qz, 3, y - 40, y + 230, 4, color=(150, 145, 140))
    prism(B, 'stone_band', qx, qz, 6, y + 230, y + 260, 4, color=(170, 165, 160), r1=0.5)


def rug(B, x0, x1, z0, z1, y, mat='carpet_red', border=GOLD, color=(255, 255, 255)):
    B.quad('stone_band', [(x0, y + 1, z1), (x1, y + 1, z1), (x1, y + 1, z0), (x0, y + 1, z0)], color=border)
    k = 10
    B.quad(mat, [(x0 + k, y + 2, z1 - k), (x1 - k, y + 2, z1 - k), (x1 - k, y + 2, z0 + k), (x0 + k, y + 2, z0 + k)],
           uv=[(0, (z1 - z0) / 2.2), ((x1 - x0) / 2.2, (z1 - z0) / 2.2), ((x1 - x0) / 2.2, 0), (0, 0)], color=color)


def table(B, x0, x1, z0, z1, y, h=60, top='marble2', color=WOOD):
    B.box((top, 'stone_band'), x0, x1, y + h - 8, y + h, z0, z1, color=color)
    for lx in (x0 + 4, x1 - 10):
        for lz in (z0 + 4, z1 - 10):
            B.box('stone_band', lx, lx + 6, y, y + h - 8, lz, lz + 6, collide=False, color=color)
    B.box('stone_band', x0, x1, y, y + h - 8, (z0 + z1) / 2 - 1, (z0 + z1) / 2 + 1, color=color)  # (to stand on)


def chair(B, x, z, y, face='N', color=WOOD, seat=(160, 40, 50)):
    f = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[face]
    B.box(('carpet_red', 'stone_band'), x - 16, x + 16, y + 36, y + 44, z - 16, z + 16, collide=False, color=seat)
    for lx in (x - 15, x + 11):
        for lz in (z - 15, z + 11):
            B.box('stone_band', lx, lx + 4, y, y + 36, lz, lz + 4, collide=False, color=color)
    bx, bz = x - f[0] * 14, z - f[1] * 14
    B.box('stone_band', bx - (16 if f[0] == 0 else 2), bx + (16 if f[0] == 0 else 2), y + 44, y + 100,
          bz - (2 if f[0] == 0 else 16), bz + (2 if f[0] == 0 else 16), collide=False, color=color)


def tree(B, x, z, y, h=220, r=80):
    """A clipped garden tree: a trunk and a rounded crown of hedge."""
    prism(B, 'pillar', x, z, 11, y, y + h * 0.55, 6, color=(120, 85, 60), r1=8)
    for (y0, y1, ra, rb) in ((0.45, 0.7, 0.75, 1.0), (0.7, 0.9, 1.0, 0.8), (0.9, 1.0, 0.8, 0.35)):
        prism(B, 'hedge', x, z, r * ra, y + h * y0, y + h * y1, 10, r1=r * rb)
    disc(B, 'hedge', x, z, r * 0.35, y + h, 10)
    disc(B, 'hedge', x, z, r * 0.75, y + h * 0.45, 10, up=False)


def round_planter(B, x, z, y, r=70):
    prism(B, 'stone_band', x, z, r, y, y + 30, 12, color=TRIM, collide=True)
    disc(B, 'leaves', x, z, r - 4, y + 30, 12)
    for i in range(8):
        a = 2 * math.pi * i / 8
        cross(B, 'flowers' if i % 2 == 0 else 'plant', x + (r - 22) * math.cos(a), z + (r - 22) * math.sin(a), y + 28,
              36, 50, 32, 64 if i % 2 == 0 else 32)


def round_fountain(B, W, cx, cz, r, y, light=None):
    """A round, two-tiered fountain: a moulded basin, a lower and an upper bowl, water falling all round."""
    n = 20
    t, h = 16, 44
    tube(B, 'marble2', cx, cz, r, y, y + h, n, color=(240, 236, 228), collide=True)
    tube(B, 'marble2', cx, cz, r - t, y + 8, y + h, n, color=(220, 215, 210), inward=True)
    annulus(B, 'marble', cx, cz, r - t, r + 4, y + h, n, color=(250, 248, 242), collide=True)
    tube(B, 'stone_band', cx, cz, r + 4, y + h - 8, y + h, n, color=GOLD)
    disc(B, 'tile_court', cx, cz, r - t, y + 8, n, color=(110, 150, 175))
    disc(W, 'water', cx, cz, r - t, y + h - 10, n, light=False)
    # the column, the lower bowl, the upper bowl, the finial
    prism(B, 'pillar', cx, cz, 16, y, y + 120, 8, collide=True)
    for (ry, rr, rt, hh) in ((y + 108, 22, 70, 20), (y + 176, 12, 34, 14)):
        prism(B, 'marble', cx, cz, rr, ry, ry + hh, 12, r1=rt)          # the bowl's underside, flaring out
        tube(B, 'stone_band', cx, cz, rt, ry + hh, ry + hh + 6, 12, color=GOLD)
        disc(W, 'water', cx, cz, rt - 3, ry + hh + 4, 12, light=False)
    prism(B, 'marble', cx, cz, 8, y + 128, y + 176, 8)
    prism(B, 'stone_band', cx, cz, 10, y + 196, y + 214, 8, color=GOLD, r1=2)
    # falling water: a curtain round each bowl's rim, and a jet from the top
    for (yt, yb, rt) in ((y + 132, y + h - 10, 72), (y + 196, y + 132, 36)):
        for inward in (False, True):
            for i in range(12):
                a0, a1 = 2 * math.pi * i / 12, 2 * math.pi * (i + 1) / 12
                p = [(cx + rt * math.cos(a0), yb, cz + rt * math.sin(a0)), (cx + rt * math.cos(a1), yb, cz + rt * math.sin(a1)),
                     (cx + rt * math.cos(a1), yt, cz + rt * math.sin(a1)), (cx + rt * math.cos(a0), yt, cz + rt * math.sin(a0))]
                am = (a0 + a1) / 2
                s = -1 if inward else 1
                oriented(W, 'water', p, (s * math.cos(am), 0, s * math.sin(am)), light=False, color=(230, 240, 255))
    falling(W, (cx - 6, cz), (cx + 6, cz), y + 196, y + 232)
    falling(W, (cx, cz - 6), (cx, cz + 6), y + 196, y + 232)
    if light:
        light((cx, y + 150, cz), 420, 0.3, (0.85, 0.95, 1.0))


def ivy(B, wall, x, z, y0, y1, w):
    """Ivy climbing a wall (cut out), just off its face."""
    inward = {'W': (1, 0, 0), 'E': (-1, 0, 0), 'N': (0, 0, 1), 'S': (0, 0, -1)}[wall]
    along = (-inward[2], 0, inward[0])
    def P(u, y):
        return (x + along[0] * u + inward[0] * 2, y, z + along[2] * u + inward[2] * 2)
    oriented(B, 'ivy', [P(-w / 2, y0), P(w / 2, y0), P(w / 2, y1), P(-w / 2, y1)], inward,
             uv=[(0.5, (y1 - y0) / 2.5), (w / 2.5, (y1 - y0) / 2.5), (w / 2.5, 0.5), (0.5, 0.5)])


def build(fixed_lights=None):
    lights = []   # collected on the first pass, given to the builder on the second (the corners are lit as built)
    beams = []
    B = LitBuilder(fixed_lights if fixed_lights is not None else lights, height=620.0)
    B.AMBIENT = 0.66
    W = Builder()  # the water, drawn translucent
    F = FLOOR

    def light(pos, radius, inten, tint=WARM):
        lights.append((pos, radius, inten, tint))

    def win(wall, pos, y0, scale=CASTLE_BEAM):
        window(B, lights, beams, wall, pos, y0, scale, floor=FLOOR)

    def sconce(x, z, y, wall):
        # a gilded wall lamp, its glow on the wall and the floor below
        ix, iz = {'W': (1, 0), 'E': (-1, 0), 'N': (0, 1), 'S': (0, -1)}[wall]
        B.box('stone_band', x - 8 + ix * 8, x + 8 + ix * 8, y - 14, y + 14, z - 8 + iz * 8, z + 8 + iz * 8,
              collide=False, color=GOLD)
        B.box('lattice', x - 5 + ix * 10, x + 5 + ix * 10, y + 14, y + 30, z - 5 + iz * 10, z + 5 + iz * 10,
              collide=False, color=(255, 230, 160))
        light((x + ix * 40, y, z + iz * 40), 340, 0.6, LAMP)

    # ---- Throne room ----
    # Laid out as the courtyard shows it through Zelda's window: its floor at the courtyard's own height (y 84, the
    # little room set behind the window), the King's throne on the south side, so that from the window Ganondorf is
    # seen in profile kneeling before him on the carpet (where the courtyard's cutscene kneels him, x -891 z 140),
    # and the guards along the back (west) wall facing the window.
    T = THRONE_FLOOR
    x0, x1, z0, z1, top = -1150, -575, -460, 460, 540
    B.floor('marble_check', x0, x1, z0, z1, y=T)
    B.ceiling('ceiling', x0, x1, z0, z1, top)
    # The window onto the courtyard (east wall), exactly where the courtyard's window tunnel ends
    B.wall('brick_white', 'x', x1, z0, z1, T, top, -1, openings=[(-20, 20, 113, 198)])
    B.wall('brick_white', 'x', x0, z0, z1, T, top, +1)
    B.wall('brick_white', 'z', z0, x0, x1, T, top, +1)
    B.wall('brick_white', 'z', z1, x0, x1, T, top, -1, openings=[(-1100, -820, T, T + 300)])
    # A stone band around the walls at the top
    for zz, f in ((z0 + 1, 1), (z1 - 1, -1)):
        B.wall('stone_band', 'z', zz, x0, x1, top - 40, top - 10, f, collide=False)
    # Two rows of pillars along the carpet, clear of the line of sight from the window
    for xx, zs in ((-1060, (-300, -100, 100, 300)), (-690, (-300, 300))):
        for zz in zs:
            B.pillar(xx, zz, 28, T, top)
    # The dais and the throne on the south side, facing north up the carpet
    tx = -900
    for i, (dx, dz) in enumerate(((210, 120), (170, 90), (130, 60))):
        B.box(('marble', 'stone_band'), tx - dx, tx + dx, T, T + 15 * (i + 1), z0, z0 + dz)
    ty = T + 45
    B.box(('carpet_red', 'stone_band'), tx - 40, tx + 40, ty, ty + 40, -440, -380)       # seat
    B.box(('stone_band', 'marble2'), tx - 55, tx + 55, ty, ty + 200, -460, -440)          # back
    B.box(('stone_band', 'stone_band'), tx - 30, tx + 30, ty + 200, ty + 230, -462, -440, collide=False)  # crest
    B.box(('stone_band', 'marble2'), tx - 60, tx - 40, ty, ty + 70, -440, -380)           # arms
    B.box(('stone_band', 'marble2'), tx + 40, tx + 60, ty, ty + 70, -440, -380)
    # The red carpet from the dais to the doors
    B.quad('carpet_red', [(tx - 70, T + 1, z1), (tx + 70, T + 1, z1), (tx + 70, T + 1, z0 + 60), (tx - 70, T + 1, z0 + 60)],
           uv=[(0, 0), (64, 0), (64, 390), (0, 390)])
    # Stained glass along the back wall, lit from behind: what the window looks across at
    for zz in (-250, 0, 250):
        B.quad('lattice', [(x0 + 2, 200, zz + 50), (x0 + 2, 200, zz - 50), (x0 + 2, 420, zz - 50), (x0 + 2, 420, zz + 50)],
               uv=[(0, 64), (32, 64), (32, 0), (0, 0)], light=False, color=(255, 250, 220))
    light((-860, 330, 0), 760, 0.4)
    for zz in (-250, 0, 250):
        light((x0 + 40, 310, zz), 300, 0.5, (0.9, 0.85, 1.0))  # the stained glass's glow
    potted(B, -1125, -430, y=T, big=True)
    potted(B, -680, -430, y=T, big=True)
    room_finish(B, x0, x1, z0, z1, T, top, doors={'E': [(-20, 20, 113, 198)], 'S': [(-1100, -820, T, T + 300)]},
                skip=[('N', -980, -820)])
    arch_door(B, 'z', z1, -1100, -820, T, T + 300)
    for zz in (-125, 125):
        banner(B, 'W', x0, zz, T + 400, w=70, h=250)
    for xx in (tx - 250, tx + 250):
        candelabra(B, light, xx, z0 + 110, T)
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
    # Steps up to the throne room, which stands at the courtyard's height
    B.stairs('marble', gx0, gx1, gz0 + 200, 200, THRONE_FLOOR - FLOOR, 4, -1)
    for zz in range(760, 1660, 200):
        for xx in (-1070, -850):
            B.pillar(xx, zz, 18, F, gtop)
    light((-960, 300, 900), 600, 0.3)
    light((-960, 300, 1450), 600, 0.3)
    for zz in (700, 1560):
        potted(B, -1070, zz, big=True)
        potted(B, -850, zz, big=True)
    sconce(gx0 + 2, 800, F + 240, 'W')
    sconce(gx1 - 2, 1000, F + 240, 'E')
    sconce(gx0 + 2, 1400, F + 240, 'W')
    room_finish(B, gx0, gx1, gz0, gz1, F, gtop, walls='WE', pilasters=False,
                doors={'W': [(900, 1100, F, F + 220)], 'E': [(1150, 1350, F, F + 220)]})
    arch_door(B, 'x', gx0, 900, 1100, F, F + 220)
    arch_door(B, 'x', gx1, 1150, 1350, F, F + 220)
    for zz in (1260, 1460):
        banner(B, 'W', gx0, zz, F + 340, w=56, h=200, mat='carpet_blue')
    for zz in (860, 1060, 1460):
        banner(B, 'E', gx1, zz, F + 340, w=56, h=200, mat='carpet_blue')
    # the lintel over the throne room doors
    B.wall('stone_band', 'z', gz0 + 1, gx0, gx1, THRONE_FLOOR + 300, THRONE_FLOOR + 330, 1, collide=False)

    # ---- Library ----
    lx0, lx1, lz0, lz1, ltop = -1700, -1100, 700, 1300, 380
    B.floor('tile_court', lx0, lx1, lz0, lz1)
    cove_ceiling(B, lx0, lx1, lz0, lz1, ltop, c=50, color=(240, 225, 200))
    B.wall('plaster', 'x', lx0, lz0, lz1, F, ltop, +1, openings=[(900, 1100, F, F + 220)])  # to the long gallery
    B.wall('plaster', 'x', lx1, lz0, lz1, F, ltop, -1, openings=[(900, 1100, F, F + 220)])
    B.wall('plaster', 'z', lz0, lx0, lx1, F, ltop, +1)
    B.wall('plaster', 'z', lz1, lx0, lx1, F, ltop, -1)
    # Bookshelves round the walls, tables in the middle
    for zz in range(730, 1290, 140):
        if zz + 120 > 880 and zz < 1120:
            continue  # (the door to the long gallery)
        B.box(('stone_band', 'shelves'), lx0, lx0 + 40, F, F + 260, zz, zz + 120)
    for xx in range(-1680, -1120, 140):
        B.box(('stone_band', 'shelves'), xx, xx + 120, F, F + 260, lz0, lz0 + 40)
        B.box(('stone_band', 'shelves'), xx, xx + 120, F, F + 260, lz1 - 40, lz1)
    for xx in (-1520, -1320):
        B.box(('marble2', 'stone_band'), xx - 60, xx + 60, F, F + 50, 940, 1060)
        # a reading lamp on each table
        B.box('stone_band', xx - 8, xx + 8, F + 50, F + 80, 992, 1008, collide=False, color=GOLD)
        B.box('lattice', xx - 12, xx + 12, F + 80, F + 104, 988, 1012, collide=False, color=(255, 230, 160))
        light((xx, F + 110, 1000), 380, 0.65, LAMP)
    light((-1400, 260, 1000), 520, 0.25)
    potted(B, -1140, 740, big=True)
    potted(B, -1140, 1260, big=True)
    room_finish(B, lx0, lx1, lz0, lz1, F, ltop, walls='E', pilasters=False, doors={'E': [(900, 1100, F, F + 220)]})
    arch_door(B, 'x', lx0, 900, 1100, F, F + 220)
    rug(B, -1600, -1240, 900, 1100, F, mat='carpet_red')
    for xx in (-1520, -1320):
        for zz, f in ((925, 'S'), (1075, 'N')):
            chair(B, xx, zz, F, face=f)

    # ---- Barracks ----
    bx0, bx1, bz0, bz1, btop = -820, -260, 1000, 1500, 320
    B.floor('stone_court', bx0, bx1, bz0, bz1)
    cove_ceiling(B, bx0, bx1, bz0, bz1, btop, c=40, color=(225, 220, 215))
    B.wall('brick_court', 'x', bx0, bz0, bz1, F, btop, +1, openings=[(1150, 1350, F, F + 220)])
    B.wall('brick_court', 'x', bx1, bz0, bz1, F, btop, -1)
    B.wall('brick_court', 'z', bz0, bx0, bx1, F, btop, +1)
    B.wall('brick_court', 'z', bz1, bx0, bx1, F, btop, -1, openings=[(-450, -330, F, F + 220)])  # to the garden
    light((-540, 240, 1250), 520, 0.3)
    sconce(bx1 - 2, 1100, F + 200, 'E')
    room_finish(B, bx0, bx1, bz0, bz1, F, btop, doors={'W': [(1150, 1350, F, F + 220)], 'S': [(-450, -330, F, F + 220)]},
                skip=[('E', 1170, 1330)])
    arch_door(B, 'z', bz1, -450, -330, F, F + 220)
    banner(B, 'N', -540, bz0, F + 280, w=60, h=180)
    for xx in (-700, -560, -420):
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz0 + 20, bz0 + 150)   # bunks
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz1 - 150, bz1 - 20)
    B.box(('stone_band', 'brick_court'), bx1 - 50, bx1, F, F + 160, 1180, 1320)                  # weapon rack

    # ---- Entrance hall ----
    ex0, ex1, ez0, ez1, etop = -1400, -520, 1660, 2360, 560
    B.floor('marble_check', ex0, ex1, ez0, ez1)
    # the front doors' threshold carries the exit back out to the drawbridge
    B.floor('marble_check', -1060, -860, ez1 - 40, ez1 - 2, y=F + 0.5, surface='exit')
    cove_ceiling(B, ex0, ex1, ez0, ez1, etop, c=80)
    B.wall('brick_white2', 'x', ex0, ez0, ez1, F, etop, +1)
    B.wall('brick_white2', 'x', ex1, ez0, ez1, F, etop, -1, openings=[(1900, 2100, F, F + 260)])  # to the garden
    B.wall('brick_white2', 'z', ez0, ex0, ex1, F, etop, +1,
           openings=[(-1100, -820, F, F + 300), (-1400, -1200, F + 320, F + 520)])
    B.wall('brick_white2', 'z', ez1, ex0, ex1, F, etop, -1, openings=[(-1060, -860, F, F + 240)])
    # the great doors (shut, the way out is their threshold)
    B.quad('window', [(-860, F, ez1 + 20), (-1060, F, ez1 + 20), (-1060, F + 240, ez1 + 20), (-860, F + 240, ez1 + 20)],
           collide=SURF['wall'], color=(150, 110, 80))
    B.floor('marble_check', -1060, -860, ez1, ez1 + 20, surface='exit')
    B.quad('carpet_red', [(-1000, F + 1, ez1), (-920, F + 1, ez1), (-920, F + 1, ez0), (-1000, F + 1, ez0)],
           uv=[(0, 320), (36, 320), (36, 0), (0, 0)])
    for xx in (-1150, -770):
        for zz in (1850, 2150):
            B.pillar(xx, zz, 34, F, etop)
    # a chandelier's glow over the middle of the hall, plants by the doors
    light((-960, 380, 2010), 760, 0.4)
    B.box('stone_band', -1000, -920, etop - 140, etop - 125, 1970, 2050, collide=False, color=GOLD)
    B.box('stone_band', -964, -956, etop - 125, etop, 2006, 2014, collide=False, color=GOLD)
    for xx in (-990, -930):
        for zz in (1980, 2040):
            B.box('lattice', xx - 6, xx + 6, etop - 125, etop - 105, zz - 6, zz + 6, collide=False, color=(255, 230, 160))
    for x, z in ((-550, 1870), (-550, 2130), (-560, 1700)):
        potted(B, x, z, big=True)
    for xx in (-1185, -735):  # knights in stone either side of the front doors
        statue(B, xx, 2305, F, face='N')
    room_finish(B, ex0, ex1, ez0, ez1, F, etop, walls='N', pilasters=False,
                doors={'N': [(-1100, -820, F, F + 300), (-1400, -1200, F + 300, F + 520)]})
    room_finish(B, ex0, ex1, ez0, ez1, F, etop, walls='SE',
                doors={'S': [(-1060, -860, F, F + 240)], 'E': [(1900, 2100, F, F + 260), (1760, 1800, F + 200, F + 280),
                                                              (2220, 2260, F + 200, F + 280)]})
    arch_door(B, 'z', ez0, -1100, -820, F, F + 300)
    arch_door(B, 'x', ex1, 1900, 2100, F, F + 260)
    arch_door(B, 'z', ez1, -1060, -860, F, F + 240)
    for xx in (-1290, -630):
        banner(B, 'S', xx, ez1, F + 440, w=80, h=280)
    sconce(ex1 - 2, 1780, F + 240, 'E')
    sconce(ex1 - 2, 2240, F + 240, 'E')
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
    B.floor('marble2', rx0, rx1, rz0, rz1, y=gy)
    rug(B, rx0 + 60, rx1 - 60, rz0 + 40, rz1 - 40, gy, mat='carpet_red')
    cove_ceiling(B, rx0, rx1, rz0, rz1, rtop, c=50, color=(240, 228, 232))
    B.wall('plaster', 'x', rx0, rz0, rz1, gy, rtop, +1)
    B.wall('plaster', 'x', rx1, rz0, rz1, gy, rtop, -1)
    B.wall('plaster', 'z', rz0, rx0, rx1, gy, rtop, +1)
    B.wall('plaster', 'z', rz1, rx0, rx1, gy, rtop, -1, openings=[(-1400, -1200, gy, gy + 200)])
    # The royal portraits on the north wall
    for i, xx in enumerate(range(-1640, -1140, 100)):
        m = f'portrait{i + 1}'
        B.quad(m, [(xx, gy + 110, rz0 + 2), (xx + 70, gy + 110, rz0 + 2), (xx + 70, gy + 180, rz0 + 2),
                   (xx, gy + 180, rz0 + 2)], uv=[(0, 16), (16, 16), (16, 0), (0, 0)], light=False)
    bed(B, -1660, -1530, 1316, 1490, gy, canopy=(120, 40, 110), blanket='carpet_red')
    room_finish(B, rx0, rx1, rz0, rz1, gy, rtop, pilasters=False,
                doors={'S': [(-1400, -1200, gy, gy + 200)], 'W': [(1515, 1605, gy + 60, gy + 640)],
                       'N': [(-1650, -1120, gy + 100, gy + 190)]})
    arch_door(B, 'z', rz1, -1400, -1200, gy, gy + 200)
    candelabra(B, light, -1490, 1330, gy, h=130)
    table(B, -1240, -1150, 1420, 1540, gy)
    chair(B, -1270, 1480, gy, face='E')
    win('W', (rx0, 1560), gy + 50, 0.36)
    light((-1400, gy + 240, 1480), 520, 0.3)
    potted(B, -1130, 1330, y=gy, big=True)
    potted(B, -1130, 1630, y=gy)

    build_wings(B, W, F, light, win, sconce)
    return B, W, beams, lights


def build_wings(B, W, F, light, win, sconce):
    # ---- The garden court (east of the entrance hall, south of the barracks): a round two-tiered fountain on a
    # paved plaza, clipped trees in round planters, ivy on the walls, a coved glass roof and tall sunlit windows ----
    x0, x1, z0, z1, top = -520, 0, 1500, 2300, 600
    WALL = (252, 242, 222)
    fx, fz = -260, 1900
    B.floor('tile_court', x0, x1, z0, z1)
    annulus(B, 'stone_court', fx, fz, 128, 230, F + 0.6, 24)
    annulus(B, 'stone_band', fx, fz, 230, 242, F + 0.6, 24, color=TRIM)
    for a, b in (((x0, 1850), (fx - 236, 1950)), ((fx + 236, 1850), (x1, 1950))):   # paths west and east
        B.quad('stone_court', [(a[0], F + 0.6, b[1]), (b[0], F + 0.6, b[1]), (b[0], F + 0.6, a[1]), (a[0], F + 0.6, a[1])])
    for a, b in (((-330, z0), (-190, fz - 236)), ((-330, fz + 236), (-190, z1))):   # north and south
        B.quad('stone_court', [(a[0], F + 0.6, b[1]), (b[0], F + 0.6, b[1]), (b[0], F + 0.6, a[1]), (a[0], F + 0.6, a[1])])
    B.wall('brick_white2', 'x', x0, z0, z1, F, top, +1, openings=[(1900, 2100, F, F + 260)], color=WALL)
    B.wall('brick_white2', 'x', x1, z0, z1, F, top, -1, color=WALL)
    B.wall('brick_white2', 'z', z0, x0, x1, F, top, +1, openings=[(-450, -330, F, F + 220)], color=WALL)
    B.wall('brick_white2', 'z', z1, x0, x1, F, top, -1, color=WALL)
    yc = cove_ceiling(B, x0, x1, z0, z1, top, c=80, mat='brick_white2', color=WALL, centre='lattice',
                      centre_color=(255, 252, 236), centre_light=False)
    for xx in range(x0 + 80 + 90, x1 - 80, 90):   # the iron ribs of the glass roof
        B.box('stone_band', xx - 3, xx + 3, yc - 8, yc - 1, z0 + 80, z1 - 80, collide=False, color=IRON2)
    for zz in range(z0 + 80 + 100, z1 - 80, 100):
        B.box('stone_band', x0 + 80, x1 - 80, yc - 8, yc - 1, zz - 3, zz + 3, collide=False, color=IRON2)
    light((fx, yc - 40, fz), 900, 0.3, (1.0, 0.98, 0.92))   # daylight through the glass
    wins = {'E': [], 'S': []}
    for zz in (1650, 1900, 2150):
        win('E', (x1, zz), 270, 0.42)
        wins['E'].append((zz - 60, zz + 60, 300, 560))
    for xx in (-390, -130):
        win('S', (xx, z1), 270, 0.42)
        wins['S'].append((xx - 60, xx + 60, 300, 560))
    room_finish(B, x0, x1, z0, z1, F, top, dado=(150, 140, 120),
                doors={'W': [(1900, 2100, F, F + 260)], 'N': [(-450, -330, F, F + 220)], **wins},
                skip=[('N', -515, -460), ('N', -320, -5)])
    round_fountain(B, W, fx, fz, 120, F, light=light)
    for a in range(10):
        t = 2 * math.pi * (a + 0.5) / 10
        cross(B, 'grass', fx + 250 * math.cos(t), fz + 250 * math.sin(t), F, 60, 30, 64, 32)
    for tx in (-430, -90):
        for tz in (1640, 2160):
            round_planter(B, tx, tz, F, 64)
            tree(B, tx, tz, F + 30, h=240, r=78)
    hedge(B, -515, -460, 1505, 1550)
    hedge(B, -320, -5, 1505, 1550)
    for zz, f in ((1720, 'S'), (2080, 'N')):  # stone benches on the plaza's rim, facing the fountain
        B.box(('marble2', 'stone_band'), -320, -200, F, F + 34, zz - 15, zz + 15)
        for xx in (-316, -214):
            B.box('stone_band', xx, xx + 10, F, F + 34, zz - 17, zz + 17, collide=False, color=TRIM)
    for wall, x, z, w in (('E', x1, 1775, 90), ('E', x1, 2025, 90), ('S', -260, z1, 120), ('S', -480, z1, 70),
                          ('W', x0, 1700, 110), ('W', x0, 2220, 110), ('N', -160, z0, 120)):
        ivy(B, wall, x, z, F + 20, F + 300, w)
    for x, z in ((-490, 2270), (-30, 2270)):
        potted(B, x, z, big=True)
    candelabra(B, light, -490, 1870, F)
    candelabra(B, light, -490, 2130, F)

    # ---- The long gallery (west of the library): a coved hallway to the guest bedrooms, knights either side of the
    # north window, consoles, banners, and a wall fountain at its south end ----
    hx0, hx1, hz0, hz1, htop = -1950, -1700, 450, 1660, 420
    HW = (245, 236, 218)
    doors = (660, 1060, 1460)
    B.floor('marble', hx0, hx1, hz0, hz1)
    cove_ceiling(B, hx0, hx1, hz0, hz1, htop, c=50, color=(240, 232, 220))
    B.quad('carpet_red', [(-1860, F + 1, hz1 - 70), (-1790, F + 1, hz1 - 70), (-1790, F + 1, hz0 + 90), (-1860, F + 1, hz0 + 90)],
           uv=[(0, 520), (32, 520), (32, 0), (0, 0)])
    B.wall('brick_white', 'x', hx1, hz0, hz1, F, htop, -1, openings=[(900, 1100, F, F + 220)], color=HW)
    B.wall('brick_white', 'x', hx0, hz0, hz1, F, htop, +1, openings=[(d - 60, d + 60, F, F + 220) for d in doors], color=HW)
    B.wall('brick_white', 'z', hz0, hx0, hx1, F, htop, +1, color=HW)
    B.wall('brick_white', 'z', hz1, hx0, hx1, F, htop, -1, color=HW)
    win('N', (-1825, hz0), 170, 0.33)
    room_finish(B, hx0, hx1, hz0, hz1, F, htop,
                doors={'E': [(900, 1100, F, F + 220), (665, 735, F + 100, F + 280), (1265, 1335, F + 100, F + 280)],
                       'W': [(d - 60, d + 60, F, F + 220) for d in doors] + [(830, 890, F + 200, F + 280),
                                                                            (1230, 1290, F + 200, F + 280)],
                       'N': [(-1870, -1780, 190, 400)]},
                skip=[('S', -1890, -1760)])
    for d in doors:  # (each arch is drawn once, on both faces of the wall: here for the bedrooms' doors too; the
        arch_door(B, 'x', hx0, d - 60, d + 60, F, F + 220)  # library's door is drawn with the library)
    for zz in (700, 1300):
        sconce(hx1 - 2, zz, F + 240, 'E')
    for zz in (860, 1260):
        sconce(hx0 + 2, zz, F + 240, 'W')
    for i, zz in enumerate((700, 1300)):  # portraits under the east lamps, gilded frames
        m = ('portrait3', 'portrait4')[i]
        B.box('stone_band', hx1 - 6, hx1, F + 94, F + 176, zz - 41, zz + 41, collide=False, color=GOLD)
        B.quad(m, [(hx1 - 7, F + 100, zz - 35), (hx1 - 7, F + 100, zz + 35), (hx1 - 7, F + 170, zz + 35),
                   (hx1 - 7, F + 170, zz - 35)], uv=[(0, 16), (16, 16), (16, 0), (0, 0)], light=False)
    for xx in (-1915, -1735):
        statue(B, xx, hz0 + 50, F, face='S')
    for zz in (860, 1260):
        potted(B, hx0 + 30, zz)
    for zz in (820, 1180):  # consoles with a plant on each
        table(B, hx1 - 44, hx1 - 8, zz - 40, zz + 40, F, h=70)
        potted(B, hx1 - 26, zz, y=F + 70)
    potted(B, hx1 - 30, 1450)
    banner(B, 'E', hx1, 1500, F + 360, w=56, h=200, mat='carpet_blue')
    banner(B, 'W', hx0, 560, F + 360, w=56, h=200, mat='carpet_blue')
    # the wall fountain: a basin against the south wall, water falling into it from a gilded spout
    B.box(('marble', 'marble2'), -1885, -1765, F, F + 40, hz1 - 70, hz1 - 54)
    B.box(('marble', 'marble2'), -1885, -1869, F, F + 40, hz1 - 54, hz1)
    B.box(('marble', 'marble2'), -1781, -1765, F, F + 40, hz1 - 54, hz1)
    water(W, -1869, -1781, hz1 - 54, hz1, F + 30)
    B.box('marble2', -1890, -1760, F + 40, F + 220, hz1 - 6, hz1, collide=False, color=(235, 232, 225))  # its back
    arch_door(B, 'z', hz1 - 7, -1880, -1770, F + 60, F + 260, depth=3)
    B.box('stone_band', -1838, -1812, F + 150, F + 180, hz1 - 24, hz1 - 6, collide=False, color=GOLD)
    B.box('stone_band', -1850, -1800, F + 180, F + 196, hz1 - 14, hz1 - 6, collide=False, color=GOLD)
    falling(W, (-1834, hz1 - 22), (-1816, hz1 - 22), F + 30, F + 152)
    candelabra(B, light, -1925, 1560, F)
    candelabra(B, light, -1725, 1560, F)
    light((-1825, 200, hz1 - 80), 380, 0.35, (0.85, 0.95, 1.0))
    light((-1825, 260, 1050), 520, 0.2)

    # ---- The guest bedrooms (three, along the gallery's west side): rose, blue and green ----
    tints = ((246, 222, 222), (220, 228, 246), (222, 240, 222))
    for i, (rz0, rz1) in enumerate(((450, 850), (850, 1250), (1250, 1660))):
        rx0, rx1, rtop = -2350, -1950, 380
        d = doors[i]
        tint = tints[i]
        wz = (rz0 + rz1) / 2 + 40
        B.floor('marble2', rx0, rx1, rz0, rz1)
        cove_ceiling(B, rx0, rx1, rz0, rz1, rtop, c=40, color=tint)
        rug(B, -2290, -2050, rz0 + 60, rz1 - 70, F, mat='carpet_red' if i == 1 else 'carpet_blue')
        B.wall('plaster', 'x', rx0, rz0, rz1, F, rtop, +1, color=tint)
        B.wall('plaster', 'x', rx1, rz0, rz1, F, rtop, -1, openings=[(d - 60, d + 60, F, F + 220)], color=tint)
        B.wall('plaster', 'z', rz0, rx0, rx1, F, rtop, +1, color=tint)
        B.wall('plaster', 'z', rz1, rx0, rx1, F, rtop, -1, color=tint)
        win('W', (rx0, wz), 140, 0.33)
        room_finish(B, rx0, rx1, rz0, rz1, F, rtop, pilasters=False,
                    doors={'E': [(d - 60, d + 60, F, F + 220)], 'W': [(wz - 45, wz + 45, 160, 400)],
                           'S': [(-2190, -2100, F + 110, F + 200)]})
        canopy = ((170, 40, 50), (60, 80, 170), (90, 140, 80))[i]
        bed(B, -2270, -2150, rz0 + 12, rz0 + 190, F, canopy=canopy, blanket='carpet_red' if i == 1 else 'carpet_blue')
        # nightstand and its lamp
        B.box(('marble2', 'stone_band'), -2130, -2085, F, F + 50, rz0 + 14, rz0 + 58, color=WOOD)
        prism(B, 'stone_band', -2107, rz0 + 36, 5, F + 50, F + 74, 6, color=GOLD)
        prism(B, 'lattice', -2107, rz0 + 36, 14, F + 74, F + 98, 8, color=(255, 230, 160), r1=8, top='lattice')
        light((-2107, F + 100, rz0 + 36), 330, 0.6, LAMP)
        # wardrobe (with a crown and feet), chest, desk and chair
        B.box(('stone_band', 'window'), -2330, -2220, F + 8, F + 220, rz1 - 52, rz1 - 4, color=WOOD)
        B.box('stone_band', -2336, -2214, F + 220, F + 234, rz1 - 56, rz1 - 2, collide=False, color=WOOD)
        B.box('stone_band', -2300, -2250, F + 120, F + 128, rz1 - 54, rz1 - 52, collide=False, color=GOLD)
        B.box(('stone_band', 'stone_band'), -2245, -2175, F, F + 40, rz0 + 205, rz0 + 245, color=WOOD)
        B.box('stone_band', -2247, -2173, F + 30, F + 36, rz0 + 203, rz0 + 247, collide=False, color=GOLD)
        table(B, -2160, -2060, rz1 - 50, rz1 - 6, F, h=64)
        chair(B, -2110, rz1 - 80, F, face='S', seat=canopy)
        # a portrait in a gilded frame over the desk, and plants
        m = ('portrait1', 'portrait2', 'portrait5')[i]
        B.box('stone_band', -2186, -2104, F + 114, F + 196, rz1 - 6, rz1, collide=False, color=GOLD)
        B.quad(m, [(-2110, F + 120, rz1 - 7), (-2180, F + 120, rz1 - 7), (-2180, F + 190, rz1 - 7), (-2110, F + 190, rz1 - 7)],
               uv=[(0, 16), (16, 16), (16, 0), (0, 0)], light=False)
        potted(B, -1985, rz0 + 35, big=True)
        potted(B, -2320, rz0 + 35)
        light((-2150, 280, (rz0 + rz1) / 2), 460, 0.2)


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
            if OBJ_PRIM.get(mat, 0xFFFFFFFF) is not None:  # (None: the actor sets the colour)
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
    D.fade_to_black((0, 0, 0), (0, 0, -70), (-102, 102, -1, 297, -71, 1))
    dark = (45, 38, 36)
    D.quad('ceiling', [(-101, 0, -70), (101, 0, -70), (101, 296, -70), (-101, 296, -70)], color=dark, light=False)
    D.quad('ceiling', [(-101, 0, 0), (-101, 0, -70), (-101, 296, -70), (-101, 296, 0)], color=dark, light=False)
    D.quad('ceiling', [(101, 0, -70), (101, 0, 0), (101, 296, 0), (101, 296, -70)], color=dark, light=False)
    D.quad('ceiling', [(-101, 296, -70), (101, 296, -70), (101, 296, 0), (-101, 296, 0)], color=dark, light=False)
    D.quad('stone_court', [(-101, 0, 0), (101, 0, 0), (101, 0, -70), (-101, 0, -70)], color=(120, 110, 100))
    return B, D


def emit(B, W, beams):
    files = {}
    emit_dl(B, BASE + f'{SCENE}_room_0DL_castle', BASE + f'{SCENE}_room_0Vtx_castle', files)
    water_path = BASE + f'{SCENE}_room_0DL_water'
    emit_dl(W, water_path, BASE + f'{SCENE}_room_0Vtx_water', files)
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
    # (x, y, z, yaw): two by the throne, two along the throne room's back wall facing the window (as the courtyard
    # shows them), the rest about the castle
    T = THRONE_FLOOR
    guards = [(-1010, T, -330, 0), (-790, T, -330, 0), (-1037, T, 58, 0x4000), (-1037, T, -58, 0x4000),
              (-1060, FLOOR, 760, 0x4000), (-860, FLOOR, 1160, -0x4000), (-600, FLOOR, 1260, -0x8000),
              (-420, FLOOR, 1300, 0), (-1110, FLOOR, 2250, -0x8000), (-810, FLOOR, 2250, -0x8000),
              (-1450, FLOOR, 1000, 0x4000), (-1250, FLOOR + 300, 1500, 0x4000)]
    actors = [G.actor_entry(HEISHI3, (x, y, z), (0, yaw, 0), 0) for x, y, z, yaw in guards]
    # Zelda at the courtyard window, peering in (her own spot and facing in the courtyard: her peeking animation
    # turns her about, so it is the same 0x4000 the courtyard gives her)
    actors.append(G.actor_entry(ZL4, (-485, 84, 0), (0, 0x4000, 0), 0))
    # the windows' sunbeams (the Temple of Time's light plane, scaled to the castle by GanondorfCastle.cpp)
    # (each one's scale in its x rotation, x10000: GanondorfCastle.cpp reads it, sets the scale, and clears it)
    actors += [G.actor_entry(DEMO_KANKYO, pos, (int(sc * 10000), yaw, 0), LIGHT_PLANE) for pos, yaw, sc in beams]
    files[room_path] = G.room_file([(dl_path, water_path), (court_opa.path, court_xlu.path)],
                                   objects=(0x0097, 0x0191, OBJECT_TOKI_OBJECTS),
                                   actors=actors,
                                   echo=10, behavior=(0, 0), skymod=(0, 1))
    spawns = [
        G.actor_entry(0, (-960, FLOOR, 2280), (0, -0x8000, 0), 0x0FFF),   # inside the front doors, facing north
        G.actor_entry(0, (-700, THRONE_FLOOR, 0), (0, -0x4000, 0), 0x0FFF),  # the throne room, by the window
    ]
    # Lighting from the courtyard (its fog and colours)
    lighting = [(100, 70, 70, -74, 100, -21, -1, -6, -6, -74, -100, 21, 20, 20, 60, 0, 0, 0, 2020, 12800),
                (105, 90, 90, 78, 98, -17, -1, -1, -16, -82, -20, -94, 50, 50, 90, 100, 100, 120, 2020, 12800)]
    files[BASE + f'{SCENE}_scene'] = G.scene_file([room_path], col_path, spawns, [(0, 0), (1, 0)],
                                                   exits=(0x0138,), lighting=lighting, sound=(0, 19, 0x29),
                                                   special=(0, 3), sky=(0, 0, 0, 1), camera=(0, 0))
    return files


if __name__ == '__main__':
    _, _, _, lights = build()          # first pass: where the lights are
    B, W, beams, _ = build(lights)     # second pass: lit by all of them
    files = emit(B, W, beams)
    out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/claude-0/-home-claude/7ba8c2c1-f15e-5c38-bc23-64c21735746e/scratchpad/castle.o2r'
    G.pack(files, out)
    print('faces', sum(len(v) for v in B.faces.values()), 'col polys', len(B.col_polys), 'files', len(files))
    for k, v in files.items():
        print(len(v), k)
