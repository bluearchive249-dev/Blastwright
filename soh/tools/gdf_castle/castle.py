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
LAMP = (1.0, 0.78, 0.5)


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
    B.box('plaster', x0 + 16, x1 - 16, y + 48, y + 60, z0 + 10, z0 + 46, collide=False, color=(250, 250, 255))
    B.box(('stone_band', 'stone_band'), x0, x1, y, y + 140, z0 - 10, z0 + 2, color=WOOD)
    B.box('stone_band', x0 + 10, x1 - 10, y + 120, y + 132, z0 + 2, z0 + 5, collide=False, color=GOLD)
    for px in (x0, x1 - 10):
        for pz in (z0 - 10, z1 - 10):
            B.box('stone_band', px, px + 10, y, y + 230, pz, pz + 10, collide=pz == z1 - 10, color=WOOD)
    B.box('carpet_red', x0 - 4, x1 + 4, y + 230, y + 246, z0 - 14, z1 + 4, collide=False, color=canopy)


def build(fixed_lights=None):
    lights = []   # collected on the first pass, given to the builder on the second (the corners are lit as built)
    beams = []
    B = LitBuilder(fixed_lights if fixed_lights is not None else lights, height=620.0)
    B.AMBIENT = 0.72
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
    # the lintel over the throne room doors
    B.wall('stone_band', 'z', gz0 + 1, gx0, gx1, THRONE_FLOOR + 300, THRONE_FLOOR + 330, 1, collide=False)

    # ---- Library ----
    lx0, lx1, lz0, lz1, ltop = -1700, -1100, 700, 1300, 380
    B.floor('tile_court', lx0, lx1, lz0, lz1)
    B.ceiling('ceiling', lx0, lx1, lz0, lz1, ltop)
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

    # ---- Barracks ----
    bx0, bx1, bz0, bz1, btop = -820, -260, 1000, 1500, 320
    B.floor('stone_court', bx0, bx1, bz0, bz1)
    B.ceiling('ceiling', bx0, bx1, bz0, bz1, btop)
    B.wall('brick_court', 'x', bx0, bz0, bz1, F, btop, +1, openings=[(1150, 1350, F, F + 220)])
    B.wall('brick_court', 'x', bx1, bz0, bz1, F, btop, -1)
    B.wall('brick_court', 'z', bz0, bx0, bx1, F, btop, +1)
    B.wall('brick_court', 'z', bz1, bx0, bx1, F, btop, -1, openings=[(-450, -330, F, F + 220)])  # to the garden
    light((-540, 240, 1250), 520, 0.3)
    sconce(bx1 - 2, 1100, F + 200, 'E')
    for xx in (-700, -560, -420):
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz0 + 20, bz0 + 150)   # bunks
        B.box(('carpet_blue', 'stone_band'), xx - 35, xx + 35, F, F + 30, bz1 - 150, bz1 - 20)
    B.box(('stone_band', 'brick_court'), bx1 - 50, bx1, F, F + 160, 1180, 1320)                  # weapon rack

    # ---- Entrance hall ----
    ex0, ex1, ez0, ez1, etop = -1400, -520, 1660, 2360, 560
    B.floor('marble_check', ex0, ex1, ez0, ez1)
    # the front doors' threshold carries the exit back out to the drawbridge
    B.floor('marble_check', -1060, -860, ez1 - 40, ez1 - 2, y=F + 0.5, surface='exit')
    B.ceiling('ceiling', ex0, ex1, ez0, ez1, etop)
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
    for x, z in ((-1100, 2320), (-820, 2320), (-550, 1870), (-550, 2130), (-560, 1700)):
        potted(B, x, z, big=True)
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
    win('W', (rx0, 1560), gy + 50, 0.36)
    light((-1400, gy + 240, 1480), 520, 0.3)
    potted(B, -1130, 1330, y=gy, big=True)
    potted(B, -1130, 1630, y=gy)

    build_wings(B, W, F, light, win, sconce)
    return B, W, beams, lights


def build_wings(B, W, F, light, win, sconce):
    # ---- The garden court (east of the entrance hall, south of the barracks): under a glass roof, a fountain
    # among flowerbeds, hedges and tall windows full of sun ----
    x0, x1, z0, z1, top = -520, 0, 1500, 2300, 600
    B.floor('tile_court', x0, x1, z0, z1)
    # stone paths crossing at the fountain
    B.quad('stone_court', [(x0, F + 0.6, 1960), (x1, F + 0.6, 1960), (x1, F + 0.6, 1840), (x0, F + 0.6, 1840)])
    B.quad('stone_court', [(-320, F + 0.6, z1), (-200, F + 0.6, z1), (-200, F + 0.6, z0), (-320, F + 0.6, z0)])
    B.quad('lattice', [(x0, top, z0), (x1, top, z0), (x1, top, z1), (x0, top, z1)], light=False, color=(255, 250, 225),
           uv=[(0, 0), (130, 0), (130, 200), (0, 200)])
    B.wall('brick_white2', 'x', x0, z0, z1, F, top, +1, openings=[(1900, 2100, F, F + 260)])
    B.wall('brick_white2', 'x', x1, z0, z1, F, top, -1)
    B.wall('brick_white2', 'z', z0, x0, x1, F, top, +1, openings=[(-450, -330, F, F + 220)])
    B.wall('brick_white2', 'z', z1, x0, x1, F, top, -1)
    for zz, f in ((z0 + 1, 1), (z1 - 1, -1)):
        B.wall('stone_band', 'z', zz, x0, x1, top - 30, top - 4, f, collide=False, color=GOLD)
    for zz in (1650, 1900, 2150):
        win('E', (x1, zz), 270, 0.42)
    for xx in (-390, -130):
        win('S', (xx, z1), 270, 0.42)
    light((-260, 260, 1900), 640, 0.3, (1.0, 0.97, 0.9))
    fountain(B, W, -260, 1900, 120)
    for a in range(8):
        t = 2 * math.pi * (a + 0.5) / 8
        cross(B, 'grass', -260 + 160 * math.cos(t), 1900 + 160 * math.sin(t), F, 60, 30, 64, 32)
    for bx0, bx1 in ((-500, -400), (-120, -20)):
        for bz0, bz1 in ((1580, 1720), (2080, 2220)):
            flowerbed(B, bx0, bx1, bz0, bz1)
    hedge(B, -515, -460, 1505, 1550)
    hedge(B, -320, -5, 1505, 1550)
    for zz in (1740, 2060):  # stone benches facing the fountain
        B.box(('marble2', 'stone_band'), -320, -200, F, F + 34, zz - 15, zz + 15)
    for x, z in ((-490, 2270), (-30, 2270), (-30, 1760), (-30, 2040)):
        potted(B, x, z, big=True)

    # ---- The long gallery (west of the library): a hallway to the guest bedrooms, with a wall fountain at its
    # south end and a tall window at its north end ----
    hx0, hx1, hz0, hz1, htop = -1950, -1700, 450, 1660, 420
    doors = (660, 1060, 1460)
    B.floor('marble', hx0, hx1, hz0, hz1)
    B.ceiling('ceiling', hx0, hx1, hz0, hz1, htop)
    B.quad('carpet_red', [(-1860, F + 1, hz1 - 70), (-1790, F + 1, hz1 - 70), (-1790, F + 1, hz0), (-1860, F + 1, hz0)],
           uv=[(0, 540), (32, 540), (32, 0), (0, 0)])
    B.wall('brick_white', 'x', hx1, hz0, hz1, F, htop, -1, openings=[(900, 1100, F, F + 220)])
    B.wall('brick_white', 'x', hx0, hz0, hz1, F, htop, +1, openings=[(d - 60, d + 60, F, F + 220) for d in doors])
    B.wall('brick_white', 'z', hz0, hx0, hx1, F, htop, +1)
    B.wall('brick_white', 'z', hz1, hx0, hx1, F, htop, -1)
    for xx, f in ((hx0 + 1, 1), (hx1 - 1, -1)):
        B.wall('stone_band', 'x', xx, hz0, hz1, htop - 30, htop - 6, f, collide=False)
    win('N', (-1825, hz0), 170, 0.33)
    for zz in (700, 1300):
        sconce(hx1 - 2, zz, F + 240, 'E')
    for zz in (860, 1260):
        sconce(hx0 + 2, zz, F + 240, 'W')
    for i, zz in enumerate((700, 1300)):  # portraits under the east lamps
        m = ('portrait3', 'portrait4')[i]
        B.quad(m, [(hx1 - 2, F + 100, zz - 35), (hx1 - 2, F + 100, zz + 35), (hx1 - 2, F + 170, zz + 35),
                   (hx1 - 2, F + 170, zz - 35)], uv=[(0, 16), (16, 16), (16, 0), (0, 0)], light=False)
    for zz in (520, 860, 1260):
        potted(B, hx0 + 30, zz)
    for zz in (560, 1180, 1450):
        potted(B, hx1 - 30, zz)
    # the wall fountain: a basin against the south wall, water falling into it from a gilded spout
    B.box(('marble', 'marble2'), -1885, -1765, F, F + 40, hz1 - 70, hz1 - 54)
    B.box(('marble', 'marble2'), -1885, -1869, F, F + 40, hz1 - 54, hz1)
    B.box(('marble', 'marble2'), -1781, -1765, F, F + 40, hz1 - 54, hz1)
    water(W, -1869, -1781, hz1 - 54, hz1, F + 30)
    B.box('stone_band', -1838, -1812, F + 150, F + 180, hz1 - 24, hz1, collide=False, color=GOLD)
    B.box('stone_band', -1850, -1800, F + 180, F + 196, hz1 - 10, hz1, collide=False, color=GOLD)
    falling(W, (-1834, hz1 - 22), (-1816, hz1 - 22), F + 30, F + 152)
    light((-1825, 200, hz1 - 80), 380, 0.35, (0.85, 0.95, 1.0))
    light((-1825, 260, 1050), 520, 0.2)

    # ---- The guest bedrooms (three, along the gallery's west side) ----
    for i, (rz0, rz1) in enumerate(((450, 850), (850, 1250), (1250, 1660))):
        rx0, rx1, rtop = -2350, -1950, 380
        d = doors[i]
        B.floor('marble2', rx0, rx1, rz0, rz1)
        B.ceiling('ceiling', rx0, rx1, rz0, rz1, rtop)
        B.quad('carpet_red' if i == 1 else 'carpet_blue',
               [(-2280, F + 1, rz1 - 70), (-2060, F + 1, rz1 - 70), (-2060, F + 1, rz0 + 60), (-2280, F + 1, rz0 + 60)],
               uv=[(0, 120), (100, 120), (100, 0), (0, 0)])
        B.wall('plaster', 'x', rx0, rz0, rz1, F, rtop, +1)
        B.wall('plaster', 'x', rx1, rz0, rz1, F, rtop, -1, openings=[(d - 60, d + 60, F, F + 220)])
        B.wall('plaster', 'z', rz0, rx0, rx1, F, rtop, +1)
        B.wall('plaster', 'z', rz1, rx0, rx1, F, rtop, -1)
        for zz, f in ((rz0 + 1, 1), (rz1 - 1, -1)):
            B.wall('stone_band', 'z', zz, rx0, rx1, rtop - 24, rtop - 6, f, collide=False, color=GOLD)
        win('W', (rx0, (rz0 + rz1) / 2 + 40), 140, 0.33)
        canopy = ((170, 40, 50), (60, 80, 170), (90, 140, 80))[i]
        bed(B, -2270, -2150, rz0 + 12, rz0 + 190, F, canopy=canopy, blanket='carpet_red' if i == 1 else 'carpet_blue')
        # nightstand and its lamp
        B.box(('marble2', 'stone_band'), -2130, -2085, F, F + 50, rz0 + 14, rz0 + 58, color=WOOD)
        B.box('stone_band', -2112, -2103, F + 50, F + 74, rz0 + 31, rz0 + 41, collide=False, color=GOLD)
        B.box('lattice', -2118, -2097, F + 74, F + 96, rz0 + 25, rz0 + 47, collide=False, color=(255, 230, 160))
        light((-2107, F + 100, rz0 + 36), 330, 0.6, LAMP)
        # wardrobe and chest
        B.box(('stone_band', 'window'), -2330, -2220, F, F + 220, rz1 - 52, rz1 - 4, color=WOOD)
        B.box(('stone_band', 'stone_band'), -2245, -2175, F, F + 40, rz0 + 205, rz0 + 245, color=WOOD)
        B.box('stone_band', -2247, -2173, F + 30, F + 36, rz0 + 203, rz0 + 247, collide=False, color=GOLD)
        # a portrait and plants
        m = ('portrait1', 'portrait2', 'portrait5')[i]
        B.quad(m, [(-2110, F + 120, rz1 - 2), (-2180, F + 120, rz1 - 2), (-2180, F + 190, rz1 - 2), (-2110, F + 190, rz1 - 2)],
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
