# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""Hyrule Castle interior for "Play as Ganondorf": a castle wing in the style of the courtyard and the Temple of Time,
generated as a scene for the unused SCENE_TESTROOM slot (scenes/shared/testroom_scene). It's built in the courtyard's
own coordinates, so the throne room's east wall holds the courtyard window: the real courtyard (its display lists,
called from oot.o2r) is outside, with Zelda's garden.

Layout (redone 14 Nov 2026 after real castles; floor y 20, x west negative):
  Throne room    x -1150..-575  z -460..460   floor y 84 (the courtyard's), throne under a baldachin on the south side;
                 the window in the east wall looks across the carpet where Ganondorf kneels; a starred blue vault
  Processional   x -1100..-820  z  460..1300  a ribbed barrel vault, kings in stone, steps up to the throne room;
                 doors west to the chapel and the tower passage, east to the guard hall
  Chapel         x -1650..-1100 z  460..960   pews, an altar on a dais, the Sages' medallions as rose windows
  Tower          round, centre (-1480, 1170), r 170: a newel (spiral) stair from the tower passage (x -1330..-1100,
                 z 1100..1240) up to the royal apartments
  Great hall     x -1340..-580  z 1300..2200  a medieval hall: dais and high tables (north), long tables, a great
                 fireplace (west), a hammerbeam roof, a carved screen with the minstrels' gallery over it (south)
  Vestibule      x -1100..-820  z 2200..2360  the front doors (south)
  Guard hall     x  -820..-260  z  700..1240  bunks, a table, spears, a fireplace
  Cloister       x  -580..0     z 1240..2300  after the Alhambra's Court of the Lions: arcaded walks round a garth,
                 a fountain ringed with stone lions, four rills, sunken beds with trees, a glass roof
  Library        x -1900..-1340 z 1340..2100  two storeys of shelves, an upper gallery, reading tables, a globe
  Upstairs (y 440): presence chamber x -1600..-1100 z 760..1000; the King's bedchamber x -1400..-1100 z 460..760;
                 the Princess's bedchamber x -1900..-1400 z 460..760; a dressing room x -1900..-1600 z 760..1000

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
TAKARA = materials('takaraya')      # the Treasure Chest Shop: dark wood planks
MAHOU = materials('mahouya')        # the Potion Shop: wood, red and blue wallpapers, red cloth
SOUKO = materials('souko')          # Lon Lon's storehouse: light wood, pale stone
KENJYA = materials('kenjyanoma')    # the Chamber of Sages: the Sages' medallions
ALL = {**TOT, **NAKA, **SPOT15, **TAKARA, **MAHOU, **SOUKO, **KENJYA}


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
    # wood, cloth, wallpaper and stone from the game's interiors
    'wood_planks': tex('takaraya_room_0Tex_004BE0'),   # dark plank floor
    'wood_dark': tex('mahouya_sceneTex_009A20'),       # dark timber (beams, tables, the screen)
    'wood_light': tex('souko_sceneTex_005C10'),        # light timber (roofs, ceilings)
    'stone_grey': tex('souko_sceneTex_005410'),        # pale dressed stone
    'cloth_red': tex('mahouya_sceneTex_009220'),       # red cloth (hangings, the cloth of state)
    'cloth_blue': tex('nakaniwa_room_0Tex_011618'),    # blue cloth
    'wallpaper_red': tex('mahouya_sceneTex_007A20'),   # red-flowered wallpaper
    'wallpaper_blue': tex('mahouya_sceneTex_002A20'),  # blue-flowered wallpaper
    'door_wood': tex('takaraya_room_0Tex_0083E0'),     # a plank door with iron hinges (wardrobes, cupboards)
    'chest_wood': tex('takaraya_room_0Tex_005BE0'),    # a panel with metal corners (chests)
    'medal_light': tex('kenjyanoma_room_0Tex_008618'),
    'medal_forest': tex('kenjyanoma_room_0Tex_006618'),
    'medal_fire': tex('kenjyanoma_room_0Tex_004618'),
    'medal_water': tex('kenjyanoma_room_0Tex_00B618'),
    'medal_shadow': tex('kenjyanoma_room_0Tex_002618'),
    'medal_spirit': tex('kenjyanoma_room_0Tex_007618'),
}
# World units per texel, per material (how big the texture is on the surface)
DENSITY = {'door_wood': 1.4, 'chest_wood': 1.6, 'carpet_red': 2.2, 'carpet_blue': 2.2, 'portrait1': 3.0, 'portrait2': 3.0, 'portrait3': 3.0,
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
        self.wall_faces = set()   # (material, index) of every wall face, for prune_buried()
        self.wall_rects = []      # (axis, facing sign, plane, low corner, high corner) of every piece of wall

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
        before = len(self.faces.get(mat, ()))
        self._wall(mat, axis, fixed, a, b, y0, y1, facing, openings, collide, color)
        for i in range(before, len(self.faces.get(mat, ()))):
            self.wall_faces.add((mat, i))

    def _wall(self, mat, axis, fixed, a, b, y0, y1, facing, openings=(), collide=True, color=(255, 255, 255)):
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
            if axis == 'x':
                self.wall_rects.append((0, 1 if facing > 0 else -1, fixed, np.array([v0, u0]), np.array([v1, u1])))
            else:
                self.wall_rects.append((2, 1 if facing > 0 else -1, fixed, np.array([u0, v0]), np.array([u1, v1])))
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
    def box(self, mats, x0, x1, y0, y1, z0, z1, collide=True, color=(255, 255, 255), back=None):
        """back: the side standing against a wall ('x0', 'x1', 'z0' or 'z1'), not drawn (it would lie in the wall's
        plane and show through it into the room behind)."""
        top, side = (mats, mats) if isinstance(mats, str) else mats
        self.quad(top, [(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], collide=SURF['floor'] if collide else None,
                  color=color)
        c = SURF['wall'] if collide else None
        if back != 'z1':
            self.quad(side, [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], collide=c, color=color)
        if back != 'z0':
            self.quad(side, [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], collide=c, color=color)
        if back != 'x1':
            self.quad(side, [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], collide=c, color=color)
        if back != 'x0':
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
    AMB_TINT = (1.0, 1.0, 1.0)   # the ambient's colour (warm in the castle: cozy, firelit rooms)

    def __init__(self, lights=None, height=420.0):
        super().__init__()
        self.lights = lights
        self.height = height

    def lum(self, p, n):
        k = self.AMBIENT * np.asarray(self.AMB_TINT, float)
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
    face('plaster', [P(-w, y_lo, 3), P(w, y_lo, 3), P(w, y_mid, 3), P(-w, y_mid, 3)], color=glass)
    face('plaster', [P(-w, y_mid, 3), P(w, y_mid, 3), P(0, y_hi, 3)], color=glass)
    face('lattice', [P(-w, y_lo, 6), P(w, y_lo, 6), P(w, y_mid, 6), P(-w, y_mid, 6)],
         uv=[(0, 64), (64, 64), (64, 16), (0, 16)])
    face('lattice', [P(-w, y_mid, 6), P(w, y_mid, 6), P(0, y_hi, 6)], uv=[(0, 16), (64, 16), (32, 0)])
    # the surround: jambs, the two sides of the arch and a sill, standing a little out of the wall
    t, d = 9, 9
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
WOODT = (232, 214, 196)   # the tint over the game's own wood textures (they carry their colour)
COZY = (1.0, 0.62, 0.32)  # candle and firelight (about 2000-2700K)
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
    B.box(('leaves', 'brick_court'), x - r, x + r, y, y + 46, z - r, z + r, color=TERRACOTTA)
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
    B.box(('wood_dark', 'wood_dark'), x0, x1, y, y + 28, z0, z1, color=WOODT)
    B.box((blanket, 'plaster'), x0 + 6, x1 - 6, y + 28, y + 48, z0 + 6, z1 - 6, color=(235, 235, 240))
    xm = (x0 + x1) / 2
    for a, b in ((x0 + 12, xm - 4), (xm + 4, x1 - 12)):  # two pillows
        B.box('plaster', a, b, y + 48, y + 60, z0 + 10, z0 + 44, collide=False, color=(250, 250, 255))
    B.box('stone_band', x0 - 2, x1 + 2, y + 14, y + 30, z1 - 4, z1 + 2, collide=False, color=canopy)  # the bed skirt
    B.box(('wood_dark', 'door_wood'), x0, x1, y, y + 140, z0 - 10, z0 + 2, color=WOODT)
    B.box('stone_band', x0 + 10, x1 - 10, y + 120, y + 132, z0 + 2, z0 + 5, collide=False, color=GOLD)
    for px in (x0 + 5, x1 - 5):
        for pz in (z0 - 5, z1 - 5):
            prism(B, 'wood_dark', px, pz, 6, y, y + 230, 8, color=WOODT, collide=pz == z1 - 5)
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
        B.box(mat, u0, u1, y0, y1, z0, z0 + depth, collide=False, color=color, back='z0')
    elif wall == 'S':
        B.box(mat, u0, u1, y0, y1, z1 - depth, z1, collide=False, color=color, back='z1')
    elif wall == 'W':
        B.box(mat, x0, x0 + depth, y0, y1, u0, u1, collide=False, color=color, back='x0')
    else:
        B.box(mat, x1 - depth, x1, y0, y1, u0, u1, collide=False, color=color, back='x1')


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
        oriented(B, 'stone_band', [P(s * w / 2 - 3, top - h + 40, 5), P(s * w / 2 + 3, top - h + 40, 5),
                                   P(s * w / 2 + 3, top, 5), P(s * w / 2 - 3, top, 5)], inward, color=rod)
    oriented(B, 'stone_band', [P(-w / 2 - 10, top - 4, 8), P(w / 2 + 10, top - 4, 8), P(w / 2 + 10, top + 6, 8),
                               P(-w / 2 - 10, top + 6, 8)], inward, color=rod)


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
    B.quad('stone_band', [(x0, y + 2, z1), (x1, y + 2, z1), (x1, y + 2, z0), (x0, y + 2, z0)], color=border)
    k = 10
    B.quad(mat, [(x0 + k, y + 4, z1 - k), (x1 - k, y + 4, z1 - k), (x1 - k, y + 4, z0 + k), (x0 + k, y + 4, z0 + k)],
           uv=[(0, (z1 - z0) / 2.2), ((x1 - x0) / 2.2, (z1 - z0) / 2.2), ((x1 - x0) / 2.2, 0), (0, 0)], color=color)


def table(B, x0, x1, z0, z1, y, h=60, top='wood_dark', color=WOODT):
    """A table in the game's dark timber: a thick top on four turned legs with an apron under the top."""
    B.box((top, 'wood_dark'), x0, x1, y + h - 8, y + h, z0, z1, color=color)
    B.box('wood_dark', x0 + 6, x1 - 6, y + h - 20, y + h - 8, z0 + 6, z1 - 6, collide=False, color=WOODT)  # apron
    for lx in (x0 + 4, x1 - 12):
        for lz in (z0 + 4, z1 - 12):
            prism(B, 'wood_dark', lx + 4, lz + 4, 4, y, y + h - 20, 6, color=WOODT)
    B.box('wood_dark', x0 + 8, x1 - 8, y, y + h - 20, (z0 + z1) / 2 - 1, (z0 + z1) / 2 + 1, color=WOODT)  # a stretcher


def chair(B, x, z, y, face='N', color=WOODT, seat=(245, 232, 222), seat_mat='cloth_red'):
    """A chair in dark timber with a cloth cushion (face: the way the sitter looks)."""
    f = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[face]
    B.box('wood_dark', x - 16, x + 16, y + 32, y + 38, z - 16, z + 16, collide=False, color=color)
    B.box((seat_mat, seat_mat), x - 14, x + 14, y + 38, y + 46, z - 14, z + 14, collide=False, color=seat)
    for lx in (x - 15, x + 11):
        for lz in (z - 15, z + 11):
            B.box('wood_dark', lx, lx + 4, y, y + 32, lz, lz + 4, collide=False, color=color)
    bx, bz = x - f[0] * 14, z - f[1] * 14
    B.box('wood_dark', bx - (16 if f[0] == 0 else 2), bx + (16 if f[0] == 0 else 2), y + 44, y + 100,
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
        return (x + along[0] * u + inward[0] * 4, y, z + along[2] * u + inward[2] * 4)
    oriented(B, 'ivy', [P(-w / 2, y0), P(w / 2, y0), P(w / 2, y1), P(-w / 2, y1)], inward,
             uv=[(0.5, (y1 - y0) / 2.5), (w / 2.5, (y1 - y0) / 2.5), (w / 2.5, 0.5), (0.5, 0.5)])


def col_quad(B, pts, surface='floor'):
    """Collision only (nothing drawn): a quad, turned so it faces up (floors, ramps) or as given (walls)."""
    p = np.array(pts, float)
    n = np.cross(p[1] - p[0], p[2] - p[0])
    if surface != 'wall' and n[1] < 0:
        pts = list(pts)[::-1]
    idx = [B.cv(q) for q in pts]
    B.col_polys.append((SURF[surface], idx[0], idx[1], idx[2], 0, 0))
    B.col_polys.append((SURF[surface], idx[0], idx[2], idx[3], 0, 0))


def ring_wall(B, mat, cx, cz, r, y0, y1, n=24, doors=(), color=(255, 255, 255)):
    """A round tower's inner wall (facing in, solid), with doorways: doors = [(angle, half_width_angle, y0, y1)]."""
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        am = (a0 + a1) / 2
        cut = None
        for (da, hw, dy0, dy1) in doors:
            d = (am - da + math.pi) % (2 * math.pi) - math.pi
            if abs(d) < hw:
                cut = (dy0, dy1)
        spans = [(y0, y1)] if cut is None else [s for s in ((y0, cut[0]), (cut[1], y1)) if s[1] - s[0] > 1]
        for (ya, yb) in spans:
            p = [(cx + r * math.cos(a0), ya, cz + r * math.sin(a0)), (cx + r * math.cos(a1), ya, cz + r * math.sin(a1)),
                 (cx + r * math.cos(a1), yb, cz + r * math.sin(a1)), (cx + r * math.cos(a0), yb, cz + r * math.sin(a0))]
            oriented(B, mat, p, (-math.cos(am), 0, -math.sin(am)), color=color, collide=SURF['wall'],
                     uv=[(0, (yb - ya) / 2.0), (2 * math.pi * r / n / 2.0, (yb - ya) / 2.0), (2 * math.pi * r / n / 2.0, 0),
                         (0, 0)])


def spiral_stair(B, light, cx, cz, r_in, r_out, y0, y1, a0, turns, per_turn=22, tread='marble', riser=TRIM,
                 under=(205, 198, 188)):
    """A newel stair (the spiral stair of a castle tower): wedge treads winding up round a central newel post,
    each with its riser and the stair's underside above the turn below; walked as a smooth helical ramp."""
    n = max(4, int(turns * per_turn))
    da = 2 * math.pi * turns / n
    dh = (y1 - y0) / n
    for i in range(n):
        a, b = a0 + i * da, a0 + (i + 1) * da
        y = y0 + (i + 1) * dh
        ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
        tre = [(cx + r_in * ca, y, cz + r_in * sa), (cx + r_out * ca, y, cz + r_out * sa),
               (cx + r_out * cb, y, cz + r_out * sb), (cx + r_in * cb, y, cz + r_in * sb)]
        oriented(B, tread, tre, (0, 1, 0))
        ris = [(cx + r_in * ca, y - dh, cz + r_in * sa), (cx + r_out * ca, y - dh, cz + r_out * sa),
               (cx + r_out * ca, y, cz + r_out * sa), (cx + r_in * ca, y, cz + r_in * sa)]
        oriented(B, 'stone_band', ris, (sa, 0, -ca), color=riser)
        # the stair's underside, a step's thickness below the tread (seen from the turn below)
        if y - 40 > y0 + 10:
            und = [(cx + r_in * ca, y - dh - 34, cz + r_in * sa), (cx + r_out * ca, y - dh - 34, cz + r_out * sa),
                   (cx + r_out * cb, y - 34, cz + r_out * sb), (cx + r_in * cb, y - 34, cz + r_in * sb)]
            oriented(B, 'brick_white', und, (0, -1, 0), color=under)
        # the walking surface: a ramp from this step's foot to its top
        col_quad(B, [(cx + r_in * ca, y - dh, cz + r_in * sa), (cx + r_out * ca, y - dh, cz + r_out * sa),
                     (cx + r_out * cb, y, cz + r_out * sb), (cx + r_in * cb, y, cz + r_in * sb)])
    # the newel post, all the way up, with a gilded band each turn
    prism(B, 'pillar', cx, cz, r_in, y0, y1 + 260, 10, collide=True)
    k = 0
    while y0 + k * 256 < y1:
        prism(B, 'stone_band', cx, cz, r_in + 4, y0 + k * 256 + 120, y0 + k * 256 + 132, 10, color=GOLD)
        k += 1


def barrel_vault(B, axis, x0, x1, z0, z1, spring, mat='ceiling', color=(236, 230, 222), n=10, ribs=0, rib=GOLD):
    """A round barrel vault springing from the side walls at `spring` (axis: the vault runs along 'x' or 'z'),
    with gilded transverse ribs every `ribs` units."""
    if axis == 'z':
        w = (x1 - x0) / 2
        cxm = (x0 + x1) / 2
        pts = [(cxm - w * math.cos(math.pi * i / n), spring + w * math.sin(math.pi * i / n)) for i in range(n + 1)]
        for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
            q = [(ua, ya, z0), (ub, yb, z0), (ub, yb, z1), (ua, ya, z1)]
            oriented(B, mat, q, (cxm - (ua + ub) / 2, -(ya + yb) / 2 + spring - 1, 0), color=color)
        if ribs:
            zz = z0 + ribs
            while zz < z1 - ribs / 2:
                for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
                    q = [(ua, ya - 1, zz - 6), (ub, yb - 1, zz - 6), (ub, yb - 1, zz + 6), (ua, ya - 1, zz + 6)]
                    oriented(B, 'stone_band', q, (cxm - (ua + ub) / 2, -1, 0), color=rib)
                zz += ribs
        # the end walls' lunettes
        for zz, f in ((z0, 1), (z1, -1)):
            for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
                oriented(B, 'brick_white', [(ua, spring, zz), (ub, spring, zz), (ub, yb, zz), (ua, ya, zz)], (0, 0, f),
                         color=color)
                B.wall_rects.append((2, f, zz, np.array([ua, spring]), np.array([ub, min(ya, yb)])))
    else:
        w = (z1 - z0) / 2
        czm = (z0 + z1) / 2
        pts = [(czm - w * math.cos(math.pi * i / n), spring + w * math.sin(math.pi * i / n)) for i in range(n + 1)]
        for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
            q = [(x0, ya, ua), (x0, yb, ub), (x1, yb, ub), (x1, ya, ua)]
            oriented(B, mat, q, (0, -(ya + yb) / 2 + spring - 1, czm - (ua + ub) / 2), color=color)
        if ribs:
            xx = x0 + ribs
            while xx < x1 - ribs / 2:
                for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
                    q = [(xx - 6, ya - 1, ua), (xx - 6, yb - 1, ub), (xx + 6, yb - 1, ub), (xx + 6, ya - 1, ua)]
                    oriented(B, 'stone_band', q, (0, -1, czm - (ua + ub) / 2), color=rib)
                xx += ribs
        for xx, f in ((x0, 1), (x1, -1)):
            for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
                oriented(B, 'brick_white', [(xx, spring, ua), (xx, spring, ub), (xx, yb, ub), (xx, ya, ua)], (f, 0, 0),
                         color=color)
                B.wall_rects.append((0, f, xx, np.array([spring, ua]), np.array([min(ya, yb), ub])))


def arcade(B, axis, fixed, a, b, y0, h, bays, r=12, facing=1, roof=None, depth=110, mat='marble2', color=(245, 240, 230),
           spandrel='brick_white2', sp_color=(250, 242, 225), paired=True):
    """A row of slender columns carrying round arches (a cloister's or a colonnade's open side), the arches'
    spandrels filled up to the walk's roof, the walk roofed back to the wall behind (depth, on the side `facing`
    points away from... the walk lies on the -facing side of the arcade line). axis 'x': the arcade runs along z in
    the plane x = fixed; 'z': along x in the plane z = fixed."""
    bw = (b - a) / bays
    top = y0 + h
    spring = y0 + h - bw / 2 - 30
    for i in range(bays + 1):
        u = a + i * bw
        offs = (-9, 9) if paired and 0 < i < bays else (0,)
        for o in offs:
            px, pz = (fixed, u + o) if axis == 'x' else (u + o, fixed)
            prism(B, mat, px, pz, r, y0 + 18, spring, 8, color=color, collide=True)
            prism(B, 'stone_band', px, pz, r + 6, y0, y0 + 18, 8, color=TRIM)
            prism(B, 'stone_band', px, pz, r + 3, spring - 14, spring, 8, color=GOLD, r1=r + 8)
    for i in range(bays):
        u0, u1 = a + i * bw, a + (i + 1) * bw
        for side in (1, -1):
            arch_face(B, axis, fixed + side * 6 * (1 if axis == 'x' else 1), u0 + 12, u1 - 12, spring, top,
                      spandrel, sp_color, side)
    # the beam over the arches
    if axis == 'x':
        B.box('stone_band', fixed - 8, fixed + 8, top - 18, top, a, b, collide=False, color=TRIM)
    else:
        B.box('stone_band', a, b, top - 18, top, fixed - 8, fixed + 8, collide=False, color=TRIM)
    if roof is not None:
        # the walk's flat timber ceiling from the arcade back to the wall
        if axis == 'x':
            xa, xb = (fixed - facing * depth, fixed) if facing > 0 else (fixed, fixed - facing * depth)
            oriented(B, 'wood_light', [(xa, top, a), (xb, top, a), (xb, top, b), (xa, top, b)], (0, -1, 0), color=roof)
        else:
            za, zb = (fixed - facing * depth, fixed) if facing > 0 else (fixed, fixed - facing * depth)
            oriented(B, 'wood_light', [(a, top, za), (b, top, za), (b, top, zb), (a, top, zb)], (0, -1, 0), color=roof)


def arch_face(B, axis, plane, u0, u1, spring, top, mat, color, side):
    """The wall above a round arch (u0..u1, springing at `spring`) up to `top`, on one face (side +1/-1)."""
    r = (u1 - u0) / 2
    uc = (u0 + u1) / 2
    n = 8
    pts = [(uc - r * math.cos(math.pi * i / n), spring + r * math.sin(math.pi * i / n)) for i in range(n + 1)]
    nrm = (side, 0, 0) if axis == 'x' else (0, 0, side)
    for (ua, ya), (ub, yb) in zip(pts, pts[1:]):
        if axis == 'x':
            q = [(plane, ya, ua), (plane, yb, ub), (plane, top, ub), (plane, top, ua)]
        else:
            q = [(ua, ya, plane), (ub, yb, plane), (ub, top, plane), (ua, top, plane)]
        oriented(B, mat, q, nrm, color=color)
        # a gilded archivolt round the arch
        k = 7
        ca = ((ua - uc) / r, (ya - spring) / r)
        cb = ((ub - uc) / r, (yb - spring) / r)
        if axis == 'x':
            q = [(plane + 2 * side, ya, ua), (plane + 2 * side, yb, ub), (plane + 2 * side, yb + cb[1] * k, ub + cb[0] * k),
                 (plane + 2 * side, ya + ca[1] * k, ua + ca[0] * k)]
        else:
            q = [(ua, ya, plane + 2 * side), (ub, yb, plane + 2 * side), (ub + cb[0] * k, yb + cb[1] * k, plane + 2 * side),
                 (ua + ca[0] * k, ya + ca[1] * k, plane + 2 * side)]
        oriented(B, 'stone_band', q, nrm, color=GOLD)


def fireplace(B, light, wall, x, z, y, w=180, h=170, hood=True):
    """A great stone fireplace against a wall: hearth, jambs, a carved mantel, a hood up to the ceiling and a fire."""
    inward = {'W': (1, 0), 'E': (-1, 0), 'N': (0, 1), 'S': (0, -1)}[wall]
    along = (-inward[1], inward[0])
    against = {'W': 'x0', 'E': 'x1', 'N': 'z0', 'S': 'z1'}[wall]
    def bx(u0, u1, d0, d1, ya, yb, mats, color=(235, 228, 216), collide=True):
        xs = [x + along[0] * u0 + inward[0] * d0, x + along[0] * u1 + inward[0] * d1]
        zs = [z + along[1] * u0 + inward[1] * d0, z + along[1] * u1 + inward[1] * d1]
        B.box(mats, min(xs), max(xs), ya, yb, min(zs), max(zs), collide=collide, color=color,
              back=against if d0 == 0 else None)
    bx(-w / 2 - 30, w / 2 + 30, 0, 70, y, y + 6, ('marble2', 'stone_band'))            # hearth stone
    for s in (-1, 1):                                                                 # jambs
        bx(s * w / 2, s * (w / 2 + 28), 0, 46, y, y + h, ('marble2', 'marble2'))
    bx(-w / 2 - 40, w / 2 + 40, 0, 56, y + h, y + h + 26, ('marble', 'stone_band'), color=TRIM)   # mantel
    bx(-w / 2 - 34, w / 2 + 34, 0, 50, y + h + 26, y + h + 34, 'stone_band', color=GOLD, collide=False)
    bx(-w / 2, w / 2, 0, 4, y, y + h, 'brick_court', color=(60, 45, 40), collide=False)        # the sooty back
    if hood:
        bx(-w / 2 - 20, w / 2 + 20, 0, 40, y + h + 34, y + h + 220, ('marble2', 'brick_white2'), color=(238, 232, 222),
           collide=False)
    # the fire: glowing logs and flames
    for i, u in enumerate((-40, 0, 40)):
        fx, fz = x + along[0] * u + inward[0] * 26, z + along[1] * u + inward[1] * 26
        prism(B, 'wood_dark', fx, fz, 8, y + 6, y + 14, 6, color=(110, 60, 40))
        cross(B, 'lattice', fx, fz, y + 10, 50, 60 + 15 * (i % 2), 16, 16, color=(255, 150, 60))
    light((x + inward[0] * 80, y + 70, z + inward[1] * 80), 520, 0.85, (1.0, 0.6, 0.3))


def chandelier(B, light, x, z, top, drop=160, r=70, arms=8, inten=0.45, radius=620):
    """A gilded ring chandelier hung on a chain, candles round its rim."""
    y = top - drop
    prism(B, 'stone_band', x, z, 3, y + 20, top, 4, color=GOLD)
    tube(B, 'stone_band', x, z, r, y, y + 10, 16, color=GOLD)
    tube(B, 'stone_band', x, z, r - 6, y, y + 10, 16, color=GOLD, inward=True)
    prism(B, 'stone_band', x, z, 14, y - 16, y + 20, 8, color=GOLD, r1=6)
    for i in range(arms):
        a = 2 * math.pi * i / arms
        cxx, czz = x + r * math.cos(a), z + r * math.sin(a)
        prism(B, 'plaster', cxx, czz, 3, y + 10, y + 28, 6, color=(250, 245, 230))
        prism(B, 'lattice', cxx, czz, 2.5, y + 28, y + 36, 4, color=(255, 210, 120), r1=0.5)
    light((x, y - 20, z), radius, inten, LAMP)


def long_table(B, x0, x1, z0, z1, y, benches=True, cloth=None):
    """A long trestle table (along the longer side) with benches either side."""
    table(B, x0, x1, z0, z1, y, h=64, top='wood_dark' if cloth is None else cloth,
          color=WOODT if cloth is None else (245, 240, 230))
    if benches:
        if x1 - x0 > z1 - z0:
            for za, zb in ((z0 - 46, z0 - 16), (z1 + 16, z1 + 46)):
                B.box('wood_dark', x0 + 10, x1 - 10, y, y + 36, za, zb, color=WOODT)
        else:
            for xa, xb in ((x0 - 46, x0 - 16), (x1 + 16, x1 + 46)):
                B.box('wood_dark', xa, xb, y, y + 36, z0 + 10, z1 - 10, color=WOODT)


def bookcase(B, wall, u0, u1, fixed, y, h=260, depth=40):
    """Bookshelves standing against a wall from u0 to u1 (cut into cases, with a cornice)."""
    step = 120
    u = u0
    while u < u1 - 20:
        ub = min(u + step - 6, u1)
        if wall in 'WE':
            s = 1 if wall == 'W' else -1
            xa, xb = sorted((fixed, fixed + s * depth))
            B.box(('wood_dark', 'shelves'), xa, xb, y, y + h, u, ub, color=(225, 205, 180))
            B.box('wood_dark', xa - 2, xb + 2, y + h, y + h + 12, u - 2, ub + 2, collide=False, color=WOODT)
        else:
            s = 1 if wall == 'N' else -1
            za, zb = sorted((fixed, fixed + s * depth))
            B.box(('wood_dark', 'shelves'), u, ub, y, y + h, za, zb, color=(225, 205, 180))
            B.box('wood_dark', u - 2, ub + 2, y + h, y + h + 12, za - 2, zb + 2, collide=False, color=WOODT)
        u += step


def pew(B, x0, x1, z, y, face='W'):
    """A church pew (a bench with a back), running along z, facing -x ('W') or +x."""
    s = -1 if face == 'W' else 1
    B.box('wood_dark', x0, x1, y + 30, y + 38, z - 40, z + 40, color=WOODT)
    bx = x1 - 4 if face == 'W' else x0
    B.box('wood_dark', bx, bx + 6, y + 38, y + 96, z - 40, z + 40, collide=False, color=WOODT)
    for zz in (z - 46, z + 40):
        B.box('wood_dark', x0, x1, y, y + 46, zz, zz + 6, color=WOODT)


def chest(B, x0, x1, z0, z1, y, h=60):
    """A strongbox: the game's iron-cornered panel on its sides and lid, a dark timber plinth."""
    B.box('wood_dark', x0 - 2, x1 + 2, y, y + 6, z0 - 2, z1 + 2, color=WOODT)
    B.box('chest_wood', x0, x1, y + 6, y + h, z0, z1, color=(240, 228, 215))


def wardrobe(B, wall, fixed, u0, u1, y, h=220, depth=56):
    """A tall cupboard against a wall, its front the game's plank door with iron hinges, a cornice on top."""
    s = 1 if wall in 'WN' else -1
    a, b = sorted((fixed, fixed + s * depth))
    back = {'W': 'x0', 'E': 'x1', 'N': 'z0', 'S': 'z1'}[wall]
    if wall in 'WE':
        B.box(('wood_dark', 'door_wood'), a, b, y, y + h, u0, u1, color=WOODT, back=back)
        B.box('wood_dark', a - (2 if s < 0 else 0), b + (2 if s > 0 else 0), y + h, y + h + 12, u0 - 3, u1 + 3,
              collide=False, color=WOODT, back=back)
    else:
        B.box(('wood_dark', 'door_wood'), u0, u1, y, y + h, a, b, color=WOODT, back=back)
        B.box('wood_dark', u0 - 3, u1 + 3, y + h, y + h + 12, a - (2 if s < 0 else 0), b + (2 if s > 0 else 0),
              collide=False, color=WOODT, back=back)


def flight(B, x0, x1, za, zb, y0, y1, steps, base=FLOOR, mat='marble2', color=TRIM, sides=(True, True)):
    """A straight flight of solid stone steps (each step built up from `base`) from z = za at y0 to z = zb at y1,
    with one smooth ramp to walk on and its open sides walled for collision."""
    sh, dz = (y1 - y0) / steps, (zb - za) / steps
    for i in range(steps):
        a, b = za + dz * i, za + dz * (i + 1)
        B.box((mat, 'stone_band'), x0, x1, base, y0 + (i + 1) * sh, min(a, b), max(a, b), collide=False, color=color)
    col_quad(B, [(x0, y0, za), (x1, y0, za), (x1, y1, zb), (x0, y1, zb)])
    lo, hi = (za, zb) if za < zb else (zb, za)
    ylo, yhi = (y0, y1) if za < zb else (y1, y0)
    if sides[0]:   # the x0 side, facing -x
        col_quad(B, [(x0, base, lo), (x0, base, hi), (x0, yhi + 60, hi), (x0, ylo + 60, lo)], 'wall')
    if sides[1]:   # the x1 side, facing +x
        col_quad(B, [(x1, base, hi), (x1, base, lo), (x1, ylo + 60, lo), (x1, yhi + 60, hi)], 'wall')


def truss(B, x0, x1, z, top, rise=120, color=(120, 85, 60)):
    """A hammerbeam roof truss across a hall (x0..x1 at z): a tie, hammer beams on curved braces, a collar and
    the rafters up to the ridge."""
    t = 9
    cx = (x0 + x1) / 2
    ridge = top + rise
    w = x1 - x0
    # hammer beams out from each wall, braced from below
    for s, xw in ((1, x0), (-1, x1)):
        hb = xw + s * w * 0.18
        B.box('wood_dark', min(xw, hb), max(xw, hb), top - 26, top - 8, z - t, z + t, collide=False, color=color)
        for k in range(4):  # the brace, in short straight pieces
            a0, a1 = math.pi * k / 8, math.pi * (k + 1) / 8
            p0 = (xw + s * w * 0.18 * (1 - math.cos(a0)), top - 26 - 90 * (1 - math.sin(a0)))
            p1 = (xw + s * w * 0.18 * (1 - math.cos(a1)), top - 26 - 90 * (1 - math.sin(a1)))
            for dz in (-t, t):
                q = [(p0[0], p0[1] - 8, z + dz), (p1[0], p1[1] - 8, z + dz), (p1[0], p1[1] + 8, z + dz),
                     (p0[0], p0[1] + 8, z + dz)]
                oriented(B, 'wood_dark', q, (0, 0, dz), color=color)
        # a gilded angel's boss on the hammer beam's end
        prism(B, 'stone_band', hb, z, 12, top - 40, top - 8, 8, color=GOLD, r1=6)
        # the rafter from the wall top to the ridge
        q = [(xw, top - 8, z - t), (cx, ridge, z - t), (cx, ridge, z + t), (xw, top - 8, z + t)]
        oriented(B, 'wood_dark', q, (0, -1, 0), color=color)
    B.box('wood_dark', x0 + w * 0.3, x1 - w * 0.3, top + rise * 0.55 - 9, top + rise * 0.55 + 9, z - t, z + t,
          collide=False, color=color)   # the collar beam


def pitched_roof(B, x0, x1, z0, z1, top, rise, mat='wood_light', color=(170, 135, 105)):
    """A timber roof pitched along z (the ridge over the middle of x0..x1), seen from inside, with gable ends."""
    cx = (x0 + x1) / 2
    ridge = top + rise
    oriented(B, mat, [(x0, top, z0), (cx, ridge, z0), (cx, ridge, z1), (x0, top, z1)], (1, -1, 0), color=color)
    oriented(B, mat, [(x1, top, z0), (cx, ridge, z0), (cx, ridge, z1), (x1, top, z1)], (-1, -1, 0), color=color)
    for zz, f in ((z0, 1), (z1, -1)):
        oriented(B, 'brick_white2', [(x0, top, zz), (x1, top, zz), (cx, ridge, zz)], (0, 0, f), color=(232, 224, 210))
    # purlins along the roof
    for k in (0.33, 0.66):
        for s in (-1, 1):
            xx = cx + s * (x1 - x0) / 2 * (1 - k)
            yy = top + rise * k
            B.box('wood_dark', xx - 6, xx + 6, yy - 10, yy, z0, z1, collide=False, color=(110, 78, 55))


def channel(B, W, x0, x1, z0, z1, y):
    """A narrow marble water channel set in the floor (the Court of the Lions' rills)."""
    B.box(('marble', 'marble2'), x0 - 8, x1 + 8, y, y + 2, z0 - 8, z1 + 8, collide=False, color=(240, 236, 228))
    B.quad('tile_court', [(x0, y + 6, z1), (x1, y + 6, z1), (x1, y + 6, z0), (x0, y + 6, z0)],
           color=(110, 150, 175))
    water(W, x0, x1, z0, z1, y + 10)


def roundel(B, mat, wall, x, z, yc, r=60, color=(255, 255, 255)):
    """A round stained-glass window (a rose): one of the Sages' medallions, in a gilded ring."""
    inward = np.array({'W': (1, 0, 0), 'E': (-1, 0, 0), 'N': (0, 0, 1), 'S': (0, 0, -1)}[wall], float)
    along = np.array([-inward[2], 0, inward[0]])
    def P(u, y, d):
        return tuple(np.array([x, 0, z]) + along * u + inward * d + np.array([0, y, 0]))
    # the medallion textures are half-discs (64x32): the top half, then the same mirrored for the bottom
    oriented(B, mat, [P(-r, yc, 4), P(r, yc, 4), P(r, yc + r, 4), P(-r, yc + r, 4)], inward,
             uv=[(0, 32), (64, 32), (64, 0), (0, 0)], color=color, light=False)
    oriented(B, mat, [P(-r, yc - r, 4), P(r, yc - r, 4), P(r, yc, 4), P(-r, yc, 4)], inward,
             uv=[(0, 0), (64, 0), (64, 32), (0, 32)], color=color, light=False)
    n = 16
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        q = [P((r + 2) * math.cos(a0), yc + (r + 2) * math.sin(a0), 7), P((r + 2) * math.cos(a1), yc + (r + 2) * math.sin(a1), 7),
             P((r + 12) * math.cos(a1), yc + (r + 12) * math.sin(a1), 7), P((r + 12) * math.cos(a0), yc + (r + 12) * math.sin(a0), 7)]
        oriented(B, 'stone_band', q, inward, color=GOLD)


def build(fixed_lights=None):
    """The castle, redone (14 Nov 2026) after real castles: a medieval great hall with a screens passage, minstrels'
    gallery, dais, hammerbeam roof and fireplace; a vaulted processional gallery up to the throne room; a chapel
    with rose windows; a guard hall; a library with an upper gallery; a newel (spiral) stair in a round tower to the
    royal apartments (presence chamber, the King's and the Princess's bedchambers, a dressing room); and a cloister
    garden after the Alhambra's Court of the Lions (arcaded walks, a fountain, four rills, sunken beds), with an upper
    gallery all round over the walks; and an east wing of two storeys (a kitchen and a great stair below, a solar and a
    music room above). The lighting is cozy: a dim warm fill, and pools of fire- and candlelight low in each room."""
    lights = []
    beams = []
    B = LitBuilder(fixed_lights if fixed_lights is not None else lights, height=640.0)
    B.AMBIENT = 0.50
    B.AMB_TINT = (1.0, 0.86, 0.68)   # cozy: a dimmer, warm fill, the rooms lit in pools by fire and candles
    W = Builder()
    F = FLOOR
    T = THRONE_FLOOR
    UP = 440   # the royal apartments' floor

    def light(pos, radius, inten, tint=WARM):
        lights.append((pos, radius, inten, tint))

    def win(wall, pos, y0, scale=CASTLE_BEAM, floor=FLOOR):
        window(B, lights, beams, wall, pos, y0, scale, floor=floor)

    def sconce(x, z, y, wall):
        ix, iz = {'W': (1, 0), 'E': (-1, 0), 'N': (0, 1), 'S': (0, -1)}[wall]
        B.box('stone_band', x - 8 + ix * 8, x + 8 + ix * 8, y - 14, y + 14, z - 8 + iz * 8, z + 8 + iz * 8,
              collide=False, color=GOLD)
        B.box('lattice', x - 5 + ix * 10, x + 5 + ix * 10, y + 14, y + 30, z - 5 + iz * 10, z + 5 + iz * 10,
              collide=False, color=(255, 230, 160))
        light((x + ix * 40, y, z + iz * 40), 340, 0.6, LAMP)

    def candle(x, z, y, n=3, inten=0.42, radius=300):
        """A cluster of candles on a brass dish: a small warm pool of light low down, where people sit."""
        disc(B, 'stone_band', x, z, 11, y + 1, 8, color=GOLD)
        for i in range(n):
            a = 2 * math.pi * i / n
            cx, cz = x + 5 * math.cos(a), z + 5 * math.sin(a)
            hh = 14 + 5 * i
            prism(B, 'plaster', cx, cz, 2.5, y + 1, y + hh, 6, color=(250, 240, 220))
            prism(B, 'lattice', cx, cz, 2, y + hh, y + hh + 6, 4, color=(255, 200, 110), r1=0.4)
        light((x, y + 40, z), radius, inten, COZY)

    def portrait(wall, fixed, u, y, m, w=70, h=70):
        if wall in 'WE':
            s = 1 if wall == 'W' else -1
            B.box('stone_band', min(fixed, fixed + s * 6), max(fixed, fixed + s * 6), y - 6, y + h + 6, u - w / 2 - 6,
                  u + w / 2 + 6, collide=False, color=GOLD)
            q = [(fixed + s * 9, y, u - w / 2), (fixed + s * 9, y, u + w / 2), (fixed + s * 9, y + h, u + w / 2),
                 (fixed + s * 9, y + h, u - w / 2)]
            oriented(B, m, q, (s, 0, 0), uv=[(0, 16), (16, 16), (16, 0), (0, 0)] if s < 0 else [(16, 16), (0, 16), (0, 0), (16, 0)],
                     light=False)
        else:
            s = 1 if wall == 'N' else -1
            B.box('stone_band', u - w / 2 - 6, u + w / 2 + 6, y - 6, y + h + 6, min(fixed, fixed + s * 6),
                  max(fixed, fixed + s * 6), collide=False, color=GOLD)
            q = [(u - w / 2, y, fixed + s * 9), (u + w / 2, y, fixed + s * 9), (u + w / 2, y + h, fixed + s * 9),
                 (u - w / 2, y + h, fixed + s * 9)]
            oriented(B, m, q, (0, 0, s), uv=[(0, 16), (16, 16), (16, 0), (0, 0)] if s > 0 else [(16, 16), (0, 16), (0, 0), (16, 0)],
                     light=False)

    # ================= Throne room (as before: the courtyard window looks into it) =================
    x0, x1, z0, z1, top = -1150, -575, -460, 460, 540
    B.floor('marble_check', x0, x1, z0, z1, y=T)
    rug(B, -1120, -620, -420, 420, T, mat='carpet_blue', border=GOLD)
    cove_ceiling(B, x0, x1, z0, z1, top, c=70, color=(60, 70, 140), band=GOLD)   # a starry blue vault
    for i in range(40):  # gilded stars on the vault
        sx = -1080 + (i * 137) % 440
        sz = -390 + (i * 211) % 780
        disc(B, 'stone_band', sx, sz, 7, top + 66, 5, color=GOLD, up=False, light=False)
    B.wall('brick_white', 'x', x1, z0, z1, T, top, -1, openings=[(-20, 20, 113, 198)])
    B.wall('brick_white', 'x', x0, z0, z1, T, top, +1)
    B.wall('brick_white', 'z', z0, x0, x1, T, top, +1)
    B.wall('brick_white', 'z', z1, x0, x1, T, top, -1, openings=[(-1100, -820, T, T + 300)])
    for xx, zs in ((-1060, (-300, -100, 100, 300)), (-690, (-300, 300))):
        for zz in zs:
            B.pillar(xx, zz, 28, T, top)
    tx = -900
    for i, (dx, dz) in enumerate(((210, 120), (170, 90), (130, 60))):
        B.box(('marble', 'stone_band'), tx - dx, tx + dx, T, T + 15 * (i + 1), z0, z0 + dz)
    ty = T + 45
    B.box(('carpet_red', 'stone_band'), tx - 40, tx + 40, ty, ty + 40, -440, -380)
    B.box(('stone_band', 'marble2'), tx - 55, tx + 55, ty, ty + 200, -460, -440)
    B.box(('stone_band', 'stone_band'), tx - 30, tx + 30, ty + 200, ty + 230, -462, -440, collide=False)
    B.box(('stone_band', 'marble2'), tx - 60, tx - 40, ty, ty + 70, -440, -380)
    B.box(('stone_band', 'marble2'), tx + 40, tx + 60, ty, ty + 70, -440, -380)
    # the throne's baldachin: four slim gilded columns and a canopy of state over it
    for bx in (tx - 110, tx + 110):
        for bz in (-450, -330):
            prism(B, 'marble2', bx, bz, 8, T + 45, T + 330, 8, color=(245, 240, 230), collide=True)
            prism(B, 'stone_band', bx, bz, 11, T + 318, T + 332, 8, color=GOLD)
    B.box('carpet_red', tx - 122, tx + 122, T + 332, T + 356, -462, -318, collide=False, color=(150, 30, 45))
    B.box('stone_band', tx - 126, tx + 126, T + 356, T + 366, -464, -316, collide=False, color=GOLD)
    oriented(B, 'cloth_red', [(tx - 105, T + 60, -436), (tx + 105, T + 60, -436), (tx + 105, T + 330, -436),
                              (tx - 105, T + 330, -436)], (0, 0, 1), uv=[(0, 64), (64, 64), (64, 0), (0, 0)])
    roundel(B, 'medal_light', 'N', tx, -431, T + 290, r=34)
    B.quad('carpet_red', [(tx - 70, T + 7, z1), (tx + 70, T + 7, z1), (tx + 70, T + 7, z0 + 60), (tx - 70, T + 7, z0 + 60)],
           uv=[(0, 0), (64, 0), (64, 390), (0, 390)])
    for zz in (-250, 0, 250):
        B.quad('lattice', [(x0 + 4, 200, zz + 50), (x0 + 4, 200, zz - 50), (x0 + 4, 420, zz - 50), (x0 + 4, 420, zz + 50)],
               uv=[(0, 64), (32, 64), (32, 0), (0, 0)], light=False, color=(255, 250, 220))
        light((x0 + 40, 310, zz), 300, 0.5, (0.9, 0.85, 1.0))
    chandelier(B, light, -860, 0, top + 70, drop=220, r=90, arms=10)
    potted(B, -1125, -430, y=T, big=True)
    potted(B, -680, -430, y=T, big=True)
    room_finish(B, x0, x1, z0, z1, T, top, doors={'E': [(-20, 20, 113, 198)], 'S': [(-1100, -820, T, T + 300)]},
                skip=[('N', -1030, -770)])
    arch_door(B, 'z', z1, -1100, -820, T, T + 300)
    for zz in (-125, 125):
        banner(B, 'W', x0, zz, T + 400, w=70, h=250)
    for xx in (tx - 250, tx + 250):
        candelabra(B, light, xx, z0 + 110, T)
    B.quad('stone_court', [(-575, 113, 20), (-575, 113, -20), (-615, 113, -20), (-615, 113, 20)])

    # ================= Processional gallery (to the throne room): ribbed vault, kings in stone =================
    px0, px1, pz0, pz1, ptop = -1100, -820, 460, 1300, 400
    B.floor('marble', px0, px1, pz0, pz1)
    rug(B, -1010, -910, 660, 1300, F, mat='carpet_red')
    B.wall('brick_white', 'x', px0, pz0, pz1, F, ptop, +1, openings=[(760, 920, F, F + 240), (1100, 1240, F, F + 220)])
    B.wall('brick_white', 'z', pz1, px0, px1, F, ptop, -1, openings=[(-1060, -860, F, F + 260)])
    B.wall('brick_white', 'x', px1, pz0, pz1, F, ptop, -1, openings=[(900, 1060, F, F + 240)])
    barrel_vault(B, 'z', px0, px1, pz0, pz1, ptop, color=(236, 228, 214), ribs=120)
    B.stairs('marble', px0, px1, pz0 + 200, 200, T - F, 4, -1)
    for zz in (820, 1000, 1180):   # past kings in stone along the walls (statues), in shallow niches
        if not (880 < zz < 1080):
            statue(B, px1 - 40, zz, F, face='W')
    for zz in (700, 1030):
        sconce(px0 + 2, zz, F + 220, 'W')
    sconce(px1 - 2, 760, F + 220, 'E')
    sconce(px1 - 2, 1200, F + 220, 'E')
    light((-960, 260, 980), 520, 0.25)
    room_finish(B, px0, px1, pz0, pz1, F, ptop, walls='WE', pilasters=False,
                doors={'W': [(760, 920, F, F + 240), (1100, 1240, F, F + 220)], 'E': [(900, 1060, F, F + 240)]},
                skip=[('W', 460, 660), ('E', 460, 660)])
    arch_door(B, 'x', px0, 760, 920, F, F + 240)
    arch_door(B, 'x', px0, 1100, 1240, F, F + 220)
    arch_door(B, 'x', px1, 900, 1060, F, F + 240)
    B.wall('stone_band', 'z', pz0 + 1, px0, px1, T + 300, T + 330, 1, collide=False)

    # ================= Chapel (west of the processional): nave, pews, altar in an apse, rose windows =================
    cx0, cx1, cz0, cz1, ctop = -1650, -1100, 460, 960, 340
    B.floor('marble_check', cx0, cx1, cz0, cz1)
    B.wall('brick_white2', 'x', cx1, cz0, cz1, F, ctop, -1, openings=[(760, 920, F, F + 240)], color=(246, 240, 228))
    B.wall('brick_white2', 'x', cx0, cz0, cz1, F, ctop, +1, color=(246, 240, 228))
    B.wall('brick_white2', 'z', cz0, cx0, cx1, F, ctop, +1, color=(246, 240, 228))
    B.wall('brick_white2', 'z', cz1, cx0, cx1, F, ctop, -1, color=(246, 240, 228))
    cove_ceiling(B, cx0, cx1, cz0, cz1, ctop, c=50, color=(70, 80, 150), band=GOLD)   # blue, starred
    for i in range(30):
        sx = cx0 + 70 + (i * 97) % (cx1 - cx0 - 140)
        sz = cz0 + 70 + (i * 151) % (cz1 - cz0 - 140)
        disc(B, 'stone_band', sx, sz, 6, ctop + 46, 5, color=GOLD, up=False, light=False)
    # the altar on a dais at the west end, under the Sages' rose
    B.box(('marble', 'stone_band'), cx0, cx0 + 160, F, F + 24, 560, 860)
    B.box(('cloth_red', 'marble2'), cx0 + 30, cx0 + 90, F + 24, F + 94, 640, 780, color=(250, 245, 238))
    B.box('stone_band', cx0 + 28, cx0 + 92, F + 94, F + 100, 638, 782, collide=False, color=GOLD)
    for zz in (660, 760):
        candelabra(B, light, cx0 + 60, zz, F + 100, h=40)
    roundel(B, 'medal_light', 'W', cx0, 710, F + 236, r=64)
    for xx, m in ((-1560, 'medal_forest'), (-1440, 'medal_fire'), (-1320, 'medal_water'), (-1200, 'medal_spirit')):
        roundel(B, m, 'N', xx, cz0, F + 260, r=44)
    for xx, m in ((-1500, 'medal_shadow'), (-1260, 'medal_light')):
        roundel(B, m, 'S', xx, cz1, F + 260, r=44)
    light((cx0 + 20, F + 270, 710), 380, 0.55, (1.0, 0.95, 0.75))
    for xx in (-1440, -1200):
        light((xx, F + 250, cz0 + 40), 300, 0.4, (0.8, 0.85, 1.0))
    for xx in range(-1440, -1150, 90):   # pews either side of the aisle, facing the altar (west)
        pew(B, xx - 30, xx + 30, 580, F)
        pew(B, xx - 30, xx + 30, 840, F)
    rug(B, -1450, -1110, 670, 750, F, mat='carpet_red')
    room_finish(B, cx0, cx1, cz0, cz1, F, ctop, pilasters=False, rail=110,
                doors={'E': [(760, 920, F, F + 240)], 'N': [(xx - 60, xx + 60, F + 200, F + 320) for xx in (-1560, -1440, -1320, -1200)],
                       'S': [(xx - 60, xx + 60, F + 200, F + 320) for xx in (-1500, -1260)]}, skip=[('W', 560, 860)])
    for xx in (-1580, -1150):
        potted(B, xx, cz0 + 40, big=True)

    # ================= Tower passage and the spiral stair =================
    tx0, tx1, tz0, tz1, ttop = -1330, -1100, 1100, 1240, 300
    B.floor('marble', tx0, tx1, tz0, tz1)
    B.ceiling('ceiling', tx0, tx1, tz0, tz1, ttop, color=(220, 212, 200))
    B.wall('brick_white', 'z', tz0, tx0, tx1, F, ttop, +1)
    B.wall('brick_white', 'z', tz1, tx0, tx1, F, ttop, -1)
    sconce(-1210, tz0 + 2, F + 200, 'N')
    room_finish(B, tx0, tx1, tz0, tz1, F, ttop, walls='NS', pilasters=False)
    TCX, TCZ, TR = -1480, 1170, 170
    t_end = 3 * math.pi / 2 - 0.36 + 2 * math.pi       # the stair ends just short of the north door, one turn and
    t_start = 0.36                                    # more up from the east door
    turns = (t_end - t_start) / (2 * math.pi)
    annulus(B, 'marble', TCX, TCZ, 1, TR, F, 28, collide=True)
    ring_wall(B, 'stone_grey', TCX, TCZ, TR, F, UP + 300, 28,
              doors=[(0.0, 0.42, F, F + 190), (3 * math.pi / 2, 0.42, UP, UP + 220)], color=(232, 226, 214))
    spiral_stair(B, light, TCX, TCZ, 30, TR - 2, F, UP, t_start, turns)
    # the top landing (the last part of a turn, flat, to the north door)
    for k in range(6):
        a, b = t_end + 0.9 * k / 6, t_end + 0.9 * (k + 1) / 6
        q = [(TCX + 30 * math.cos(a), UP, TCZ + 30 * math.sin(a)), (TCX + TR * math.cos(a), UP, TCZ + TR * math.sin(a)),
             (TCX + TR * math.cos(b), UP, TCZ + TR * math.sin(b)), (TCX + 30 * math.cos(b), UP, TCZ + 30 * math.sin(b))]
        oriented(B, 'marble', q, (0, 1, 0))
        col_quad(B, q)
    disc(B, 'wood_light', TCX, TCZ, TR, UP + 300, 20, up=False, color=(170, 135, 105))
    for a in (math.pi * 0.75, math.pi * 1.1, math.pi * 0.25):   # arrow-slit windows, lit
        wx, wz = TCX + (TR - 3) * math.cos(a), TCZ + (TR - 3) * math.sin(a)
        oriented(B, 'plaster', [(wx - 8 * math.sin(a), F + 150 + 120 * (a / math.pi), wz + 8 * math.cos(a)),
                                (wx + 8 * math.sin(a), F + 150 + 120 * (a / math.pi), wz - 8 * math.cos(a)),
                                (wx + 8 * math.sin(a), F + 260 + 120 * (a / math.pi), wz - 8 * math.cos(a)),
                                (wx - 8 * math.sin(a), F + 260 + 120 * (a / math.pi), wz + 8 * math.cos(a))],
                 (-math.cos(a), 0, -math.sin(a)), color=(255, 250, 230), light=False)
        light((wx - 40 * math.cos(a), F + 200 + 120 * (a / math.pi), wz - 40 * math.sin(a)), 260, 0.5, WARM)
    light((TCX, F + 120, TCZ), 300, 0.35, LAMP)
    light((TCX, UP + 100, TCZ), 300, 0.35, LAMP)

    # ================= Great hall: screens passage, minstrels' gallery, dais, hammerbeam roof, fireplace =================
    hx0, hx1, hz0, hz1, htop = -1340, -580, 1300, 2200, 520
    B.floor('wood_planks', hx0, hx1, hz0, hz1)
    B.wall('stone_grey', 'x', hx0, hz0, hz1, F, htop, +1, openings=[(1640, 1800, F, F + 240)], color=(236, 228, 214))
    B.wall('stone_grey', 'x', hx1, hz0, hz1, F, htop, -1, openings=[(1680, 1840, F, F + 240)], color=(236, 228, 214))
    B.wall('stone_grey', 'z', hz0, hx0, hx1, F, htop, +1, openings=[(-1060, -860, F, F + 260)], color=(236, 228, 214))
    B.wall('stone_grey', 'z', hz1, hx0, hx1, F, htop, -1, openings=[(-1100, -820, F, F + 260)], color=(236, 228, 214))
    pitched_roof(B, hx0, hx1, hz0, hz1, htop, 220)
    for zz in range(hz0 + 120, hz1 - 60, 150):
        truss(B, hx0, hx1, zz, htop, rise=220)
    # the dais (high end, north): a step up, two high tables either side of the processional arch, the cloth of state
    for a, b in ((hx0, -1060), (-860, hx1)):
        B.box(('marble', 'stone_band'), a, b, F, F + 24, hz0, hz0 + 150)
        col_quad(B, [(a, F + 24, hz0 + 150), (b, F + 24, hz0 + 150), (b, F, hz0 + 180), (a, F, hz0 + 180)])
    for a, b in ((-1300, -1090), (-830, -620)):
        long_table(B, a, b, hz0 + 50, hz0 + 110, F + 24, benches=False, cloth='plaster')
        for xx in range(a + 25, b, 50):
            chair(B, xx, hz0 + 30, F + 24, face='S')
    for xx in (-1195, -725):
        banner(B, 'N', xx, hz0, F + 430, w=140, h=300, mat='cloth_red')
    # two long tables down the hall, benches either side, the aisle between
    long_table(B, -1250, -1180, 1500, 1980, F)
    long_table(B, -740, -670, 1500, 1980, F)
    for zz in (1600, 1740, 1880):
        candle(-1215, zz, F + 64)
        candle(-705, zz, F + 64)
    B.quad('carpet_red', [(-1000, F + 3, hz1), (-920, F + 3, hz1), (-920, F + 3, hz0 + 180), (-1000, F + 3, hz0 + 180)],
           uv=[(0, 330), (36, 330), (36, 0), (0, 0)])
    # the screens passage at the low end (south) under the minstrels' gallery: a carved screen with three openings
    sz = hz1 - 150
    gy = F + 300
    for a, b in ((hx0, -1200), (-1100, -1040), (-880, -820), (-720, hx1)):
        B.box(('wood_dark', 'wood_dark'), a, b, F, gy - 20, sz - 10, sz + 10, color=(150, 110, 80))
    for a, b in ((-1200, -1100), (-1040, -880), (-820, -720)):
        spring = gy - 30 - (b - a) / 2
        arch_face(B, 'z', sz - 10.5, a, b, spring, gy - 20, 'wood_dark', (150, 110, 80), -1)
        arch_face(B, 'z', sz + 10.5, a, b, spring, gy - 20, 'wood_dark', (150, 110, 80), 1)
    B.floor('wood_planks', hx0, hx1, sz - 10, hz1, y=gy)
    oriented(B, 'wood_planks', [(hx0, gy - 20, sz - 10), (hx1, gy - 20, sz - 10), (hx1, gy - 20, hz1), (hx0, gy - 20, hz1)],
             (0, -1, 0), color=(150, 115, 85))
    B.railing(hx0, hx1, sz - 10, sz - 10, gy, h=70, posts=24)
    oriented(B, 'cloth_red', [(hx0 + 40, gy - 20, sz - 12), (hx1 - 40, gy - 20, sz - 12), (hx1 - 40, gy + 10, sz - 12),
                              (hx0 + 40, gy + 10, sz - 12)], (0, 0, -1), uv=[(0, 8), (300, 8), (300, 0), (0, 0)])
    for xx in (-1200, -960, -720):   # musicians' music stands up on the gallery
        prism(B, 'stone_band', xx, hz1 - 70, 3, gy, gy + 90, 4, color=GOLD)
        B.box('plaster', xx - 18, xx + 18, gy + 90, gy + 116, hz1 - 74, hz1 - 70, collide=False, color=(250, 245, 230))
    # the great fireplace on the west wall
    fireplace(B, light, 'W', hx0, 1600, F, w=200, h=180)
    # the windows high in the east wall (onto the cloister), and two in the west
    for zz in (1450, 2000):
        win('E', (hx1, zz), 250, 0.36)
    win('W', (hx0, 1950), 250, 0.36)
    for zz, xx in ((1560, -1150), (1900, -1150), (1560, -770), (1900, -770)):
        chandelier(B, light, xx, zz, htop + 60, drop=200, r=60, arms=8, inten=0.35, radius=520)
    light((-960, 300, 1700), 760, 0.25)
    for xx in (-1300, -620):
        potted(B, xx, hz0 + 190, big=True)
    statue(B, -1280, hz1 - 200, F, face='E')
    statue(B, -640, hz1 - 200, F, face='W')
    room_finish(B, hx0, hx1, hz0, hz1, F, htop, pilasters=False,
                doors={'W': [(1640, 1800, F, F + 240), (1460, 1740, F, F + 420), (1890, 2010, F + 260, F + 500)],
                       'E': [(1680, 1840, F, F + 240), (1390, 1510, F + 260, F + 500), (1940, 2060, F + 260, F + 500)],
                       'N': [(-1060, -860, F, F + 260)], 'S': [(-1100, -820, F, F + 260)]},
                skip=[('N', hx0, hx1)])
    arch_door(B, 'x', hx0, 1640, 1800, F, F + 240)
    arch_door(B, 'x', hx1, 1680, 1840, F, F + 240)
    arch_door(B, 'z', hz0, -1060, -860, F, F + 260)

    # ================= Gatehouse vestibule (the front doors) =================
    vx0, vx1, vz0, vz1, vtop = -1100, -820, 2200, 2360, 320
    B.floor('marble_check', vx0, vx1, vz0, vz1)
    col_quad(B, [(-1060, F + 0.5, vz1 - 2), (-860, F + 0.5, vz1 - 2), (-860, F + 0.5, vz1 - 40), (-1060, F + 0.5, vz1 - 40)],
             surface='exit')
    B.wall('stone_grey', 'x', vx0, vz0, vz1, F, vtop, +1, color=(236, 228, 214))
    B.wall('stone_grey', 'x', vx1, vz0, vz1, F, vtop, -1, color=(236, 228, 214))
    B.wall('stone_grey', 'z', vz1, vx0, vx1, F, vtop, -1, openings=[(-1060, -860, F, F + 240)], color=(236, 228, 214))
    barrel_vault(B, 'z', vx0, vx1, vz0, vz1, vtop - 140 + 140, color=(230, 222, 208), ribs=80)
    B.quad('window', [(-860, F, vz1 + 20), (-1060, F, vz1 + 20), (-1060, F + 240, vz1 + 20), (-860, F + 240, vz1 + 20)],
           collide=SURF['wall'], color=(150, 110, 80))
    B.floor('marble_check', -1060, -860, vz1, vz1 + 20, surface='exit')
    arch_door(B, 'z', vz1, -1060, -860, F, F + 240)
    for zz in (2240,):
        sconce(vx0 + 2, zz + 40, F + 200, 'W')
        sconce(vx1 - 2, zz + 40, F + 200, 'E')
    room_finish(B, vx0, vx1, vz0, vz1, F, vtop, walls='WES', pilasters=False, doors={'S': [(-1060, -860, F, F + 240)]})

    # ================= Guard hall (east of the processional) =================
    gx0, gx1, gz0, gz1, gtop = -820, -260, 700, 1240, 360
    B.floor('stone_court', gx0, gx1, gz0, gz1)
    B.wall('stone_grey', 'x', gx0, gz0, gz1, F, gtop, +1, openings=[(900, 1060, F, F + 240)], color=(225, 218, 206))
    B.wall('stone_grey', 'x', gx1, gz0, gz1, F, gtop, -1, color=(225, 218, 206))
    B.wall('stone_grey', 'z', gz0, gx0, gx1, F, gtop, +1, color=(225, 218, 206))
    B.wall('stone_grey', 'z', gz1, gx0, gx1, F, gtop, -1, openings=[(-500, -380, F, F + 220)], color=(225, 218, 206))
    barrel_vault(B, 'x', gx0, gx1, gz0, gz1, gtop - 120 + 120, color=(222, 214, 200), ribs=140, rib=TRIM)
    fireplace(B, light, 'E', gx1, 970, F, w=150, h=150)
    long_table(B, -680, -440, 930, 1010, F)
    candle(-560, 970, F + 64)
    for xx in (-760, -640, -520, -400):   # bunks along the north wall
        B.box(('cloth_blue', 'wood_dark'), xx - 40, xx + 40, F, F + 34, gz0 + 10, gz0 + 160, color=(235, 235, 240))
    for k, xx in enumerate(range(-780, -540, 30)):
        prism(B, 'stone_band', xx, gz1 - 20, 3, F, F + 220, 4, color=(150, 145, 140))
        prism(B, 'stone_band', xx, gz1 - 20, 6, F + 220, F + 250, 4, color=(190, 190, 195), r1=0.5)
    B.box('wood_dark', -790, -530, F + 140, F + 150, gz1 - 30, gz1 - 10, collide=False, color=WOODT)
    for zz in (1090,):
        statue(B, gx0 + 60, zz + 60, F, face='E')
    banner(B, 'N', -540, gz0, F + 320, w=80, h=220)
    sconce(gx0 + 2, 800, F + 220, 'W')
    light((-540, 260, 970), 520, 0.25)
    room_finish(B, gx0, gx1, gz0, gz1, F, gtop, pilasters=False,
                doors={'W': [(900, 1060, F, F + 240)], 'S': [(-500, -380, F, F + 220)]}, skip=[('E', 860, 1080)])
    arch_door(B, 'z', gz1, -500, -380, F, F + 220)

    # ================= The cloister garden (after the Court of the Lions) =================
    ox0, ox1, oz0, oz1, otop = -580, 0, 1240, 2300, 620
    WALK = 120
    wt = F + 300    # the walks' roof
    WALLC = (252, 242, 222)
    B.floor('tile_court', ox0, ox1, oz0, oz1)
    B.wall('brick_white2', 'x', ox0, oz0, oz1, F, otop, +1, openings=[(1680, 1840, F, F + 240)], color=WALLC)
    UPG = F + 300   # the gallery floor (over the walks' roofs) and the east wing's upper floor
    EDOORS = [(1530, 1650, F, F + 240), (2120, 2240, F, F + 240),
              (1350, 1470, UPG, UPG + 220), (1700, 1820, UPG, UPG + 220), (2120, 2240, UPG, UPG + 220)]
    B.wall('brick_white2', 'x', ox1, oz0, oz1, F, otop, -1, openings=EDOORS, color=WALLC)
    B.wall('brick_white2', 'z', oz0, ox0, ox1, F, otop, +1, openings=[(-500, -380, F, F + 220)], color=WALLC)
    B.wall('brick_white2', 'z', oz1, ox0, ox1, F, otop, -1, color=WALLC)
    yc = cove_ceiling(B, ox0, ox1, oz0, oz1, otop, c=70, mat='brick_white2', color=WALLC, centre='lattice',
                      centre_color=(255, 252, 236), centre_light=False)
    for xx in range(ox0 + 70 + 90, ox1 - 70, 90):
        B.box('stone_band', xx - 3, xx + 3, yc - 8, yc - 1, oz0 + 70, oz1 - 70, collide=False, color=IRON2)
    light(((ox0 + ox1) / 2, yc - 40, (oz0 + oz1) / 2), 1000, 0.32, (1.0, 0.98, 0.92))
    # the four arcaded walks round the garth (paired columns, gilded archivolts), roofed in timber
    ga0, ga1, gb0, gb1 = ox0 + WALK, ox1 - WALK, oz0 + WALK, oz1 - WALK
    arcade(B, 'x', ga0, gb0, gb1, F, 280, 6, facing=1, roof=(185, 150, 115), depth=WALK)
    arcade(B, 'x', ga1, gb0, gb1, F, 280, 6, facing=-1, roof=(185, 150, 115), depth=WALK)
    arcade(B, 'z', gb0, ga0, ga1, F, 280, 3, facing=1, roof=(185, 150, 115), depth=WALK)
    arcade(B, 'z', gb1, ga0, ga1, F, 280, 3, facing=-1, roof=(185, 150, 115), depth=WALK)
    for (a, b, c, d) in ((ox0, ga0, oz0, gb0), (ga1, ox1, oz0, gb0), (ox0, ga0, gb1, oz1), (ga1, ox1, gb1, oz1)):
        oriented(B, 'wood_light', [(a, F + 280, c), (b, F + 280, c), (b, F + 280, d), (a, F + 280, d)], (0, -1, 0),
                 color=(185, 150, 115))
    # the garth: the lion fountain in the middle, four rills to the walks, four sunken flower beds
    fx, fz = (ga0 + ga1) / 2, (gb0 + gb1) / 2
    round_fountain(B, W, fx, fz, 90, F, light=light)
    for i in range(12):   # the twelve lions round the basin (little stone beasts)
        a = 2 * math.pi * (i + 0.5) / 12
        lx, lz = fx + 104 * math.cos(a), fz + 104 * math.sin(a)
        B.box('marble', lx - 9, lx + 9, F, F + 22, lz - 9, lz + 9, collide=False, color=(235, 230, 215))
        prism(B, 'marble', lx + 6 * math.cos(a), lz + 6 * math.sin(a), 7, F + 18, F + 34, 6, color=(235, 230, 215))
    channel(B, W, fx - 10, fx + 10, gb0, fz - 112, F)
    channel(B, W, fx - 10, fx + 10, fz + 112, gb1, F)
    channel(B, W, ga0, fx - 112, fz - 10, fz + 10, F)
    channel(B, W, fx + 112, ga1, fz - 10, fz + 10, F)
    for (a, b, c, d) in ((ga0 + 20, fx - 34, gb0 + 20, fz - 34), (fx + 34, ga1 - 20, gb0 + 20, fz - 34),
                         (ga0 + 20, fx - 34, fz + 34, gb1 - 20), (fx + 34, ga1 - 20, fz + 34, gb1 - 20)):
        if b - a > 60 and d - c > 60:
            hedge(B, a, b, c, c + 22, h=40)
            hedge(B, a, b, d - 22, d, h=40)
            flowerbed(B, a + 10, b - 10, c + 30, d - 30)
            tree(B, (a + b) / 2, (c + d) / 2, F + 30, h=200, r=60)
    for xx in (-430, -150):
        win('S', (xx, oz1), 300, 0.42)
    for wall, x, z, w in (('E', ox1, 1400, 90), ('E', ox1, 1800, 90), ('S', -290, oz1, 120), ('W', ox0, 1500, 110),
                          ('W', ox0, 2050, 110), ('N', -200, oz0, 140)):
        ivy(B, wall, x, z, F + 20, F + 280, w)
    for x, z in ((-540, 2260), (-40, 2260), (-40, 1280)):
        potted(B, x, z, big=True)
    room_finish(B, ox0, ox1, oz0, oz1, F, otop, dado=(150, 140, 120), pilasters=False,
                doors={'W': [(1680, 1840, F, F + 240)], 'N': [(-500, -380, F, F + 220)],
                       'E': EDOORS,
                       'S': [(xx - 60, xx + 60, 330, 620) for xx in (-430, -150)]})

    for (u0, u1, y0, y1) in EDOORS:
        arch_door(B, 'x', ox1, u0, u1, y0, y1)
    # ---- the upper gallery: a balcony all round on the walks' roofs, looking down into the garth ----
    for (a, b, c, d) in ((ox0, ox1, oz0, gb0), (ox0, ox1, gb1, oz1), (ox0, ga0, gb0, gb1), (ga1, ox1, gb0, gb1)):
        B.floor('wood_planks', a, b, c, d, y=UPG, color=(235, 220, 205))
    # its edge over the arcades: a moulded curb and a turned balustrade
    for axis, fixed, a, b in (('x', ga0, gb0 - 8, gb1 + 8), ('x', ga1, gb0 - 8, gb1 + 8), ('z', gb0, ga0 - 8, ga1 + 8),
                              ('z', gb1, ga0 - 8, ga1 + 8)):
        if axis == 'x':
            B.box(('marble2', 'stone_band'), fixed - 8, fixed + 8, F + 280, UPG + 6, a, b, collide=False, color=TRIM)
            B.railing(fixed, fixed, a + 8, b - 8, UPG + 6, h=70, posts=int((b - a) // 34))
        else:
            B.box(('marble2', 'stone_band'), a + 16, b - 16, F + 280, UPG + 6, fixed - 8, fixed + 8, collide=False,
                  color=TRIM)
            B.railing(a + 8, b - 8, fixed, fixed, UPG + 6, h=70, posts=int((b - a) // 34))
    # lanterns on the gallery walls, benches and potted orange trees between the doors
    for x, z, wall in ((-580, 1500, 'W'), (-580, 2000, 'W'), (-300, 1240, 'N'), (-300, 2300, 'S'), (0, 1585, 'E'),
                       (0, 1960, 'E')):
        sconce(x, z, UPG + 170, wall)
    for (a, b, c, d) in ((-560, -500, 1560, 1700), (-560, -500, 1860, 2000), (-380, -220, 1250, 1300),
                         (-380, -220, 2240, 2290)):
        B.box(('wood_dark', 'wood_dark'), a, b, UPG, UPG + 34, c, d, color=WOODT)
        B.box(('cloth_red', 'cloth_red'), a + 4, b - 4, UPG + 34, UPG + 40, c + 4, d - 4, collide=False,
              color=(245, 230, 220))
    for x, z in ((-540, 1280), (-40, 1280), (-540, 2260), (-40, 2260)):
        potted(B, x, z, y=UPG, big=True)
    light((-290, UPG + 140, 1300), 420, 0.3, COZY)
    light((-290, UPG + 140, 2240), 420, 0.3, COZY)

    # ================= The east wing: a kitchen and the great stair below, a solar and a music room above =================
    ex0, ex1, ez0, ez1, etop = 0, 420, 1240, 2300, F + 620
    KS = 1900                       # the kitchen / stair hall wall
    UT = UPG + 300                  # the upper rooms' ceilings
    EWALL = (246, 236, 220)
    B.floor('stone_grey', ex0, ex1, ez0, KS, color=(225, 215, 200))
    B.floor('marble2', ex0, ex1, KS, ez1)
    # the outer walls, full height, windows to the east on both floors
    eops = [(1360, 1480, F + 70, F + 230), (1700, 1820, F + 70, F + 230), (1360, 1480, UPG + 70, UPG + 230),
            (1690, 1810, UPG + 70, UPG + 230), (2020, 2140, UPG + 70, UPG + 230)]
    B.wall('plaster', 'x', ex1, ez0, ez1, F, etop, -1, color=EWALL)
    B.wall('plaster', 'x', ex0, ez0, ez1, F, etop, +1, openings=EDOORS, color=EWALL)
    B.wall('plaster', 'z', ez0, ex0, ex1, F, UT, +1, color=EWALL)
    B.wall('plaster', 'z', ez1, ex0, ex1, F, etop, -1, color=EWALL)
    for (u0, u1, y0, y1) in eops:
        win('E', (ex1, (u0 + u1) / 2), y0 - 40, 0.28, floor=F if y0 < UPG else UPG)
    # the floor between the storeys over the kitchen: planks above, a timbered ceiling below
    B.floor('wood_planks', ex0, ex1, ez0, KS, y=UPG)
    B.ceiling('wood_light', ex0, ex1, ez0, KS, UPG - 12, color=(200, 165, 130))
    for zz in range(ez0 + 80, KS, 110):
        B.box('wood_dark', ex0, ex1, UPG - 30, UPG - 12, zz - 9, zz + 9, collide=False, color=WOODT)
    # the kitchen / stair hall wall (below), the music room / stair hall wall (above), the solar / music room wall
    B.wall('plaster', 'z', KS, ex0, ex1, F, UPG - 12, -1, openings=[(40, 160, F, F + 230)], color=EWALL)
    B.wall('plaster', 'z', KS, ex0, ex1, UPG, UT, -1, openings=[(40, 160, UPG, UPG + 220)], color=EWALL)
    B.wall('plaster', 'z', KS, ex0, ex1, F, etop, +1, openings=[(40, 160, F, F + 230), (40, 160, UPG, UPG + 220)],
           color=EWALL)
    SM = 1580
    B.wall('wallpaper_red', 'z', SM, ex0, ex1, UPG, UT, -1, openings=[(160, 280, UPG, UPG + 220)])
    B.wall('wallpaper_blue', 'z', SM, ex0, ex1, UPG, UT, +1, openings=[(160, 280, UPG, UPG + 220)])
    for d in ((40, 160, F, F + 230), (40, 160, UPG, UPG + 220)):
        arch_door(B, 'z', KS, *d)
    arch_door(B, 'z', SM, 160, 280, UPG, UPG + 220)

    # -- the kitchen: a great hearth, the cook's table, a dresser, barrels and hanging pots --
    fireplace(B, light, 'N', 210, ez0, F, w=220, h=170, hood=False)
    B.box('stone_band', 140, 280, F + 200, F + 210, ez0, ez0 + 50, collide=False, color=IRON2)   # the pot crane
    prism(B, 'stone_band', 210, ez0 + 40, 26, F + 70, F + 120, 10, color=IRON2, r1=20)            # the cauldron
    long_table(B, 120, 300, 1560, 1640, F, benches=True)
    for xx in (160, 260):
        candle(xx, 1600, F + 64)
    for k, (bx_, bz_) in enumerate(((36, 1290), (36, 1346), (88, 1350))):
        prism(B, 'wood_dark', bx_, bz_, 24, F, F + 74, 10, color=WOODT, r1=22, collide=True)   # barrels
        tube(B, 'stone_band', bx_, bz_, 25, F + 14, F + 20, 10, color=IRON2)
        tube(B, 'stone_band', bx_, bz_, 25, F + 54, F + 60, 10, color=IRON2)
    wardrobe(B, 'S', KS, 220, 400, F, h=150, depth=40)    # the dresser
    for xx in (250, 310, 370):   # plates stood up along the back
        B.box('marble2', xx - 12, xx + 12, F + 150, F + 176, KS - 12, KS - 8, collide=False, color=(240, 235, 225))
    for xx in (240, 300, 360):   # copper pans on the dresser's top
        prism(B, 'stone_band', xx, KS - 34, 14, F + 150, F + 166, 8, color=(200, 120, 70), r1=11)
    light((210, F + 200, 1600), 420, 0.25, COZY)
    room_finish(B, ex0, ex1, ez0, KS, F, UPG - 12, pilasters=False, dado=(120, 100, 85),
                doors={'W': [(1530, 1650, F, F + 240)], 'S': [(40, 160, F, F + 230)],
                       'E': [(u0, u1, y0, y1) for u0, u1, y0, y1 in eops if y0 < UPG], 'N': [(70, 350, F, F + 240)]},
                skip=[('S', 216, 404), ('W', 1250, 1380)])

    # -- the great stair: two flights round a stone spine, up to a gallery that opens onto the cloister's --
    ux0 = 180                       # the upper landing's edge
    flight(B, 300, 410, 2240, 1990, F, F + 150, 10, sides=(False, False))
    B.box(('marble2', 'stone_band'), ux0, 420, F, F + 150, KS, 1990, color=TRIM, back='z0')        # the half landing
    flight(B, ux0, 290, 1990, 2240, F + 150, UPG, 10, sides=(True, False))
    B.box(('marble', 'stone_band'), 290, 300, F, F + 230, 1990, 2240, color=TRIM)                  # the spine
    # the upper landing and the gallery at the stair head, on timber, a balustrade round the well
    for (a, b, c, d) in ((ex0, ux0, KS, ez1), (ux0, ex1, 2240, ez1)):
        B.box(('wood_planks', 'stone_band'), a, b, UPG - 12, UPG, c, d, color=TRIM)
        B.ceiling('wood_light', a, b, c, d, UPG - 12, color=(200, 165, 130))
    B.railing(ux0, ux0, KS + 10, 2240, UPG, h=70, posts=10)
    B.railing(300, ex1 - 10, 2240, 2240, UPG, h=70, posts=4)
    prism(B, 'marble', ux0, 2240, 10, UPG, UPG + 90, 8, color=TRIM)                                 # newel post
    prism(B, 'stone_band', ux0, 2240, 12, UPG + 90, UPG + 102, 8, color=GOLD, r1=4)
    cove_ceiling(B, ex0, ex1, KS, ez1, etop, c=60, color=(240, 232, 218))
    chandelier(B, light, 300, 2100, etop + 60, drop=200, r=60, arms=8, inten=0.4, radius=520)
    banner(B, 'S', 300, ez1, etop - 40, w=110, h=250, mat='cloth_red')
    for z in (1975, 2060):
        sconce(0, z, F + 170, 'W')
    portrait('S', ez1, 90, UPG + 110, 'portrait5')
    room_finish(B, ex0, ex1, KS, ez1, F, etop, walls='NW', pilasters=False,
                doors={'W': [(2120, 2240, F, F + 240), (2120, 2240, UPG - 12, UPG + 220)],
                       'N': [(40, 160, F, F + 230), (40, 160, UPG - 12, UPG + 220), (ux0, ex1, -1e9, 1e9)]})

    # -- the solar (upstairs, north): the family's warm sitting room by the fire --
    rug(B, 60, 360, 1330, 1540, UPG, mat='carpet_red')
    cove_ceiling(B, ex0, ex1, ez0, SM, UT, c=40, color=(235, 215, 190))
    fireplace(B, light, 'N', 210, ez0, UPG, w=130, h=130, hood=False)   # (over the kitchen's: one chimney)
    for xx in (140, 280):
        chair(B, xx, 1420, UPG, face='N', seat_mat='cloth_red')
    table(B, 188, 232, 1400, 1444, UPG, h=50)
    candle(210, 1422, UPG + 50, n=2)
    bookcase(B, 'E', 1250, 1352, ex1, UPG, h=220)
    chest(B, 340, 400, 1510, 1566, UPG)
    potted(B, 40, 1280, y=UPG, big=True)
    candelabra(B, light, 40, 1540, UPG, h=120)
    candelabra(B, light, 370, 1470, UPG, h=120)
    room_finish(B, ex0, ex1, ez0, SM, UPG, UT, pilasters=False,
                doors={'W': [(1350, 1470, UPG, UPG + 220)], 'S': [(160, 280, UPG, UPG + 220)],
                       'E': [(u0, u1, y0, y1) for u0, u1, y0, y1 in eops if y0 >= UPG] + [(1246, 1356, UPG, UPG + 240)],
                       'N': [(70, 350, UPG, UPG + 400)]})

    # -- the music room (upstairs, between): a harp, a little chamber organ, chairs for the listeners --
    rug(B, 60, 360, 1630, 1860, UPG, mat='carpet_blue')
    cove_ceiling(B, ex0, ex1, SM, KS, UT, c=40, color=(225, 230, 245))
    B.box(('wood_dark', 'door_wood'), 300, 400, UPG, UPG + 90, SM, SM + 56, color=WOODT, back='z0')   # the organ
    B.box('wood_dark', 296, 404, UPG + 90, UPG + 96, SM, SM + 62, collide=False, color=WOODT, back='z0')
    for i in range(9):
        xx = 308 + i * 10.5
        hh = 60 + 40 * math.sin(math.pi * i / 8)
        prism(B, 'stone_band', xx, SM + 20, 4.5, UPG + 96, UPG + 96 + hh, 8, color=GOLD)
    chair(B, 350, 1670, UPG, face='N', seat_mat='cloth_blue')
    hx, hz = 150, 1760   # the harp: a gilded column and neck
    prism(B, 'stone_band', hx, hz, 5, UPG, UPG + 140, 6, color=GOLD)
    B.box('stone_band', hx - 3, hx + 3, UPG + 128, UPG + 140, hz - 50, hz, collide=False, color=GOLD)
    B.box('wood_dark', hx - 6, hx + 6, UPG, UPG + 14, hz - 56, hz + 6, collide=False, color=WOODT)
    for xx in (220, 300, 380):
        chair(B, xx, 1840, UPG, face='N', seat_mat='cloth_blue')
    candelabra(B, light, 40, 1620, UPG, h=120)
    candelabra(B, light, 40, 1870, UPG, h=120)
    candelabra(B, light, 380, 1650, UPG, h=120)
    room_finish(B, ex0, ex1, SM, KS, UPG, UT, pilasters=False,
                doors={'W': [(1700, 1820, UPG, UPG + 220)], 'N': [(160, 280, UPG, UPG + 220), (296, 404, UPG, UPG + 200)],
                       'S': [(40, 160, UPG, UPG + 220)],
                       'E': [(u0, u1, y0, y1) for u0, u1, y0, y1 in eops if y0 >= UPG and u0 < KS]})

    # ================= Library (west of the great hall): two storeys of shelves, an upper gallery =================
    lx0, lx1, lz0, lz1, ltop = -1900, -1340, 1340, 2100, 560
    B.floor('wood_planks', lx0, lx1, lz0, lz1)
    B.wall('plaster', 'x', lx1, lz0, lz1, F, ltop, -1, openings=[(1640, 1800, F, F + 240)], color=(240, 228, 206))
    B.wall('plaster', 'x', lx0, lz0, lz1, F, ltop, +1, color=(240, 228, 206))
    B.wall('plaster', 'z', lz0, lx0, lx1, F, ltop, +1, color=(240, 228, 206))
    B.wall('plaster', 'z', lz1, lx0, lx1, F, ltop, -1, color=(240, 228, 206))
    cove_ceiling(B, lx0, lx1, lz0, lz1, ltop, c=50, color=(235, 220, 195))
    bookcase(B, 'N', lx0 + 20, lx1 - 20, lz0, F)
    bookcase(B, 'S', lx0 + 20, lx1 - 20, lz1, F)
    bookcase(B, 'W', lz0 + 50, 1640, lx0, F)
    bookcase(B, 'W', 1900, lz1 - 50, lx0, F)
    bookcase(B, 'E', lz0 + 50, 1600, lx1, F)
    bookcase(B, 'E', 1840, lz1 - 50, lx1, F)
    # the upper gallery all round on brackets, its own shelves above
    ug = F + 290
    for (a, b, c, d) in ((lx0, lx1, lz0, lz0 + 70), (lx0, lx1, lz1 - 70, lz1), (lx0, lx0 + 70, lz0, lz1), (lx1 - 70, lx1, lz0, lz1)):
        oriented(B, 'wood_planks', [(a, ug, c), (b, ug, c), (b, ug, d), (a, ug, d)], (0, 1, 0))
        oriented(B, 'wood_dark', [(a, ug - 14, c), (b, ug - 14, c), (b, ug - 14, d), (a, ug - 14, d)], (0, -1, 0),
                 color=(130, 95, 70))
    B.railing(lx0 + 70, lx1 - 70, lz0 + 70, lz0 + 70, ug, posts=14)
    B.railing(lx0 + 70, lx1 - 70, lz1 - 70, lz1 - 70, ug, posts=14)
    B.railing(lx0 + 70, lx0 + 70, lz0 + 70, lz1 - 70, ug, posts=18)
    B.railing(lx1 - 70, lx1 - 70, lz0 + 70, lz1 - 70, ug, posts=18)
    bookcase(B, 'N', lx0 + 20, lx1 - 20, lz0, ug, h=200)
    bookcase(B, 'S', lx0 + 20, lx1 - 20, lz1, ug, h=200)
    fireplace(B, light, 'W', lx0 + 40, 1770, F, w=150, h=150, hood=False)
    for zz, xx in ((1560, -1620), (1880, -1620)):
        long_table(B, xx - 110, xx + 110, zz - 40, zz + 40, F, benches=False)
        for dx in (-60, 60):
            chair(B, xx + dx, zz - 64, F, face='S')
            chair(B, xx + dx, zz + 64, F, face='N')
        B.box('stone_band', xx - 8, xx + 8, F + 64, F + 94, zz - 8, zz + 8, collide=False, color=GOLD)
        B.box('lattice', xx - 12, xx + 12, F + 94, F + 118, zz - 12, zz + 12, collide=False, color=(255, 230, 160))
        light((xx, F + 124, zz), 380, 0.6, LAMP)
    # a great globe on a stand
    prism(B, 'wood_dark', -1450, 1720, 14, F, F + 70, 8, color=WOODT)
    for k in range(6):
        ya, yb = F + 70 + 16 * k, F + 70 + 16 * (k + 1)
        ra = 48 * math.sin(math.pi * k / 6) + 4
        rb = 48 * math.sin(math.pi * (k + 1) / 6) + 4
        prism(B, 'cloth_blue', -1450, 1720, ra, ya, yb, 14, color=(120, 160, 210), r1=rb)
    chandelier(B, light, (lx0 + lx1) / 2, 1720, ltop + 50, drop=200, r=70)
    light((-1620, 320, 1720), 600, 0.25)

    # ================= Upstairs: the royal apartments (floor y 440) =================
    # Presence chamber (antechamber) south of the bedchambers, entered from the tower's north door
    ax0, ax1, az0, az1, atop = -1600, -1100, 760, 1000, UP + 300
    B.floor('marble2', ax0, ax1, az0, az1, y=UP)
    rug(B, ax0 + 40, ax1 - 40, az0 + 40, az1 - 40, UP, mat='carpet_red')
    for wall_args in (('x', ax1, az0, az1, -1, ()), ('x', ax0, az0, az1, +1, [(820, 940, UP, UP + 220)])):
        ax, fx_, a, b, f, ops = wall_args
        B.wall('wallpaper_red', ax, fx_, a, b, UP, atop, f, openings=ops)
    B.wall('wallpaper_red', 'z', az1, ax0, ax1, UP, atop, -1, openings=[(-1530, -1430, UP, UP + 220)])
    B.wall('wallpaper_red', 'z', az0, ax0, ax1, UP, atop, +1, openings=[(-1320, -1200, UP, UP + 220)])
    cove_ceiling(B, ax0, ax1, az0, az1, atop, c=40, color=(240, 225, 225))
    for xx, m in ((-1300, 'portrait2'), (-1180, 'portrait3')):
        portrait('S', az1, xx, UP + 120, m)
    portrait('E', ax1, 880, UP + 120, 'portrait4')
    candelabra(B, light, -1580, 980, UP, h=130)
    candelabra(B, light, -1120, 980, UP, h=130)
    chandelier(B, light, -1350, 880, atop + 40, drop=140, r=50, arms=6, inten=0.4, radius=420)
    room_finish(B, ax0, ax1, az0, az1, UP, atop, pilasters=False,
                doors={'W': [(820, 940, UP, UP + 220)], 'S': [(-1530, -1430, UP, UP + 220)], 'N': [(-1320, -1200, UP, UP + 220)]})
    arch_door(B, 'x', ax0, 820, 940, UP, UP + 220)
    arch_door(B, 'z', az0, -1320, -1200, UP, UP + 220)
    arch_door(B, 'z', az1, -1530, -1430, UP, UP + 220)
    # the floor slab over the tower passage and chapel, seen from below in the passage: (the chapel draws its own)
    # The King's bedchamber (north-east): a great bed on a step, a fireplace, a window to the north
    kx0, kx1, kz0, kz1, ktop = -1400, -1100, 460, 760, UP + 320
    B.floor('wood_planks', kx0, kx1, kz0, kz1, y=UP)
    rug(B, kx0 + 30, kx1 - 30, kz0 + 200, kz1 - 30, UP, mat='carpet_blue')
    for args in (('x', kx1, kz0, kz1, -1), ('x', kx0, kz0, kz1, +1), ('z', kz0, kx0, kx1, +1)):
        B.wall('wallpaper_red', *args[:3], args[3], UP, ktop, args[4])
    B.wall('wallpaper_red', 'z', kz1, kx0, kx1, UP, ktop, -1, openings=[(-1320, -1200, UP, UP + 220)])
    cove_ceiling(B, kx0, kx1, kz0, kz1, ktop, c=50, color=(70, 60, 120))
    B.box(('carpet_red', 'stone_band'), kx0 + 20, kx0 + 220, UP, UP + 12, kz0 + 6, kz0 + 230, color=(160, 40, 50))
    bed(B, kx0 + 40, kx0 + 200, kz0 + 14, kz0 + 220, UP + 12, canopy=(120, 30, 60), blanket='cloth_red')
    fireplace(B, light, 'E', kx1, 620, UP, w=120, h=130, hood=False)
    win('N', (-1170, kz0), UP + 60, 0.30, floor=UP)
    chest(B, kx1 - 70, kx1 - 10, kz1 - 140, kz1 - 60, UP)
    potted(B, kx0 + 40, kz1 - 40, y=UP, big=True)
    room_finish(B, kx0, kx1, kz0, kz1, UP, ktop, pilasters=False,
                doors={'S': [(-1320, -1200, UP, UP + 220)], 'N': [(-1230, -1110, UP + 120, UP + 330)]},
                skip=[('E', 540, 700)])
    # The Princess's bedchamber (north-west): blue flowers, a canopy bed, a dressing table, a window seat
    qx0, qx1, qz0, qz1, qtop = -1900, -1400, 460, 760, UP + 320
    B.floor('wood_planks', qx0, qx1, qz0, qz1, y=UP)
    rug(B, qx0 + 60, qx1 - 60, qz0 + 60, qz1 - 40, UP, mat='carpet_red')
    for args in (('x', qx0, qz0, qz1, +1), ('z', qz0, qx0, qx1, +1), ('x', qx1, qz0, qz1, -1)):
        B.wall('wallpaper_blue', *args[:3], args[3], UP, qtop, args[4])
    B.wall('wallpaper_blue', 'z', qz1, qx0, qx1, UP, qtop, -1, openings=[(-1780, -1660, UP, UP + 220)])
    cove_ceiling(B, qx0, qx1, qz0, qz1, qtop, c=50, color=(240, 236, 250))
    bed(B, -1700, -1560, qz0 + 14, qz0 + 200, UP, canopy=(90, 110, 190), blanket='cloth_blue')
    table(B, -1880, -1790, qz0 + 20, qz0 + 70, UP, h=70, top='marble2')
    B.box('window', -1870, -1800, UP + 70, UP + 170, qz0 + 4, qz0 + 10, collide=False, color=(200, 220, 240))   # mirror
    chair(B, -1835, qz0 + 100, UP, face='N', seat_mat='cloth_blue')
    win('N', (-1480, qz0), UP + 60, 0.30, floor=UP)
    B.box(('cloth_blue', 'marble2'), -1540, -1420, UP, UP + 40, qz0 + 10, qz0 + 50, color=(220, 225, 245))   # window seat
    for x, z in ((-1860, qz1 - 40), (-1440, qz1 - 40)):
        potted(B, x, z, y=UP, big=True)
    harp_x, harp_z = -1820, 640
    prism(B, 'stone_band', harp_x, harp_z, 4, UP, UP + 130, 4, color=GOLD)
    B.box('stone_band', harp_x - 3, harp_x + 3, UP + 120, UP + 130, harp_z - 40, harp_z, collide=False, color=GOLD)
    light((-1650, UP + 200, 610), 460, 0.3)
    room_finish(B, qx0, qx1, qz0, qz1, UP, qtop, pilasters=False,
                doors={'S': [(-1780, -1660, UP, UP + 220)], 'N': [(-1540, -1420, UP + 100, UP + 340)]})
    arch_door(B, 'z', qz1, -1780, -1660, UP, UP + 220)
    # The dressing room (south-west), between the Princess's chamber and the presence chamber
    dx0, dx1, dz0, dz1, dtop = -1900, -1600, 760, 1000, UP + 280
    B.floor('wood_planks', dx0, dx1, dz0, dz1, y=UP)
    for args in (('x', dx0, dz0, dz1, +1), ('z', dz1, dx0, dx1, -1)):
        B.wall('wallpaper_blue', *args[:3], args[3], UP, dtop, args[4])
    B.wall('wallpaper_blue', 'z', dz0, dx0, dx1, UP, dtop, +1, openings=[(-1780, -1660, UP, UP + 220)])
    cove_ceiling(B, dx0, dx1, dz0, dz1, dtop, c=36, color=(240, 236, 250))
    for zz in (800, 900):
        wardrobe(B, 'W', dx0, zz, zz + 90, UP)
    B.box(('cloth_red', 'wood_dark'), -1700, -1620, UP, UP + 40, 930, 990, color=WOODT)
    B.wall('wallpaper_blue', 'x', dx1, dz0, dz1, UP, dtop, -1, openings=[(820, 940, UP, UP + 220)])
    light((-1750, UP + 180, 880), 360, 0.45, LAMP)
    room_finish(B, dx0, dx1, dz0, dz1, UP, dtop, pilasters=False, walls='WSN',
                doors={'N': [(-1780, -1660, UP, UP + 220)]})

    return B, W, beams, lights


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


def prune_buried(B, tol=0.6):
    """Walls here have no thickness, so whatever is set against one side of a wall (mouldings, shelves, a
    fireplace, a beam's end) has a back face lying exactly in the wall's plane, facing the room on the other side,
    where it fights the other room's wall and shows through it. Those faces are taken out: any face that isn't a wall
    itself, lying in a wall's plane, facing the same way, and at least half covered by that wall."""
    walls = {}
    for ax, sg, c, lo, hi in B.wall_rects:
        walls.setdefault((ax, sg), []).append((c, lo, hi))
    removed = 0
    for mat, fl in B.faces.items():
        keep = []
        for i, f in enumerate(fl):
            if (mat, i) not in B.wall_faces:
                P = np.array([v[:3] for v in f], float)
                n = np.cross(P[1] - P[0], P[2] - P[0])
                L = np.linalg.norm(n)
                if L > 1e-9:
                    ax = int(np.argmax(abs(n)))
                    if abs(n[ax]) > 0.999 * L and np.ptp(P[:, ax]) < 0.01:
                        o = [k for k in (0, 1, 2) if k != ax]
                        lo, hi = P[:, o].min(0), P[:, o].max(0)
                        a = max(1e-6, float(np.prod(hi - lo)))
                        cov = 0.0
                        for c, wlo, whi in walls.get((ax, int(np.sign(n[ax]))), ()):
                            if abs(c - P[0][ax]) < tol:
                                d = np.minimum(hi, whi) - np.maximum(lo, wlo)
                                if (d > 0).all():
                                    cov += float(np.prod(d))
                        if cov >= 0.5 * a:
                            removed += 1
                            continue
            keep.append(f)
        B.faces[mat] = keep
    return removed


def emit(B, W, beams):
    print('faces hidden in walls, taken out:', prune_buried(B))
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
    UP = 440
    guards = [(-1010, T, -330, 0), (-790, T, -330, 0), (-1037, T, 58, 0x4000), (-1037, T, -58, 0x4000),
              (-1060, FLOOR, 1000, 0x4000), (-860, FLOOR, 1180, -0x4000),           # the processional
              (-1090, FLOOR, 1500, 0), (-830, FLOOR, 1500, 0),                    # before the dais
              (-1070, FLOOR, 2250, -0x8000), (-850, FLOOR, 2250, -0x8000),         # at the front doors
              (-700, FLOOR, 1150, 0), (-330, FLOOR, 820, -0x8000),                 # the guard hall
              (-520, FLOOR, 2000, 0x4000),                                         # the cloister's west walk
              (-1400, FLOOR, 1500, -0x4000),                                       # the library
              (-1350, UP, 970, -0x8000)]                                           # the presence chamber
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
