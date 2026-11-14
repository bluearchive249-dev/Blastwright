# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""The Temple of Light: 50 rooms in one scene (SCENE_SYOTES), built from templeplan.py's plan.

    python3 lighttemple.py out/gdf_sacred.o2r [--header ../../soh/Enhancements/ExtraModes/GanondorfLightTempleData.h]
                                             [--doc out/temple.md]

What it writes:
  - the scene: 50 rooms (each its own display lists, its sunbeams and the game's flames in its actor list), the
    passages between them with an En_Holl (the game's room-change plane) in each, one collision for all
  - the props the runtime draws (doors, seals, chests, mirrors, plates, crystals, pillars, braziers, the atrium's
    platform, bridges of light and shadow, the kick block) and their collision
  - GanondorfLightTempleData.h: the rooms and everything the runtime places in them (GanondorfLightTemple.cpp)
  - the design document (the plan, room by room, the solver's report)
Everything is merged into the gdf_sacred.o2r that sacred.py makes (it replaces sacred.py's first hall).

Coordinates: one grid for all floors, cells of 2400; the Entrance Hall sits where sacred.py's first hall was (x +-320,
z -1100..300), so the mirror back to the Mirror Room and the way in stay where they were.
"""
import math, os, struct, sys
sys.path.insert(0, '/home/claude/o2r')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import sacred as S          # (sets the castle kit up for the Sacred Realm: floor 0, the mirror and gold materials)
import castle as C
import o2rgen as G
import templeplan as P

CELL = 2400
DW, DH = 110, 280            # doorways: half width, height
WARM = C.WARM
GOLD = (255, 205, 110)
SAGE_TINTS = S.SAGE_TINTS     # Forest, Fire, Water, Shadow, Spirit, Light
SAGE_MEDALS = ['medal_forest', 'medal_fire', 'medal_water', 'medal_shadow', 'medal_spirit', 'medal_light']
# The order the Sages' pillars stand in the Mirror Room, walking in from the broken door (left, then right, nearest
# first): Water, Shadow, Fire, Spirit, Forest, Light. The Sanctum of Fire's pillars must be struck in that order.
SIMON_ORDER = [2, 3, 1, 4, 0, 5]
EN_HOLL = 0x0153
EN_LIGHT = 0x0008
DEMO_KANKYO, LIGHT_PLANE, OBJECT_TOKI_OBJECTS = C.DEMO_KANKYO, C.LIGHT_PLANE, C.OBJECT_TOKI_OBJECTS
SCENE = 'syotes'
BASE = f'scenes/shared/{SCENE}_scene/'

# ---- Things the runtime places (GanondorfLightTemple.cpp mirrors these numbers) ----
T_DOOR, T_CHEST, T_BRIDGE, T_MIRROR, T_PLATE, T_CRYSTAL, T_BRAZIER, T_PILLAR, T_BEAMMIRROR, T_EMITTER, T_TARGET, \
    T_BLOCK, T_SOCKET, T_HEAVY, T_GUARD, T_MURAL, T_LIFT, T_SWITCH, T_LIGHTBRIDGE, T_EXIT, T_INSCRIPTION = range(21)
D_KEY, D_BOSS, D_SUN, D_MOON, D_BARS = range(5)
ITEMS = {'key': 0, 'boss': 1, 'rupee20': 2, 'rupee50': 3, 'rupee200': 4, 'heart': 5, 'heartpiece': 6, 'magic': 7}
GUARD_SPEAR, GUARD_SWORD, GUARD_BOW, GUARD_SHIELD, GUARD_CAPTAIN, GUARD_IRON = 0, 1, 2, 3, 4, 5
# Switch flags of the temple scene (saved with the game): one per room (its puzzle solved or its guards beaten),
# then the key doors, the temple's aspect, the boss door
# (only switch flags 0x00..0x1F are kept in the save: the rest are cleared on leaving the scene)
SOLVE_KINDS = ('plates', 'braziers', 'crystals', 'beam', 'block', 'simon', 'combat', 'miniboss', 'dark')
SOLVE_FLAG = {rid: i for i, rid in enumerate(r for r in sorted(P.ROOMS) if P.ROOMS[r][3] in SOLVE_KINDS)}
FLAG_KEYDOOR0 = len(SOLVE_FLAG)
FLAG_ASPECT = FLAG_KEYDOOR0 + len(P.KEY_DOORS)
FLAG_BOSSDOOR = FLAG_ASPECT + 1
assert FLAG_BOSSDOOR < 0x20, 'the temple needs more than 32 saved switch flags'

KEY_DOOR_FLAG = {d: FLAG_KEYDOOR0 + i for i, d in enumerate(P.KEY_DOORS)}


def room_center(rid):
    name, floor, (col, row), kind = P.ROOMS[rid]
    return (col - 2) * CELL, (row - 3) * CELL - 400


def floor_y(rid, floor=None):
    f = P.ROOMS[rid][1] if rid != P.ATRIUM else floor
    return P.FLOORS[f]


KINDS = ['plates', 'hub', 'combat', 'braziers', 'treasure', 'crystals', 'mirror', 'quiet', 'beam', 'block', 'chasm',
         'timed', 'water', 'miniboss', 'dark', 'secret', 'simon', 'bridges', 'ante', 'atrium']
SIZES = {'plates': (600, 600), 'hub': (560, 560), 'combat': (650, 650), 'braziers': (600, 600),
         'treasure': (420, 420), 'crystals': (600, 600), 'mirror': (480, 480), 'quiet': (520, 520),
         'beam': (650, 650), 'block': (600, 600), 'chasm': (600, 720), 'timed': (600, 720), 'water': (650, 650),
         'miniboss': (760, 760), 'dark': (600, 600), 'secret': (420, 420), 'simon': (640, 640),
         'bridges': (620, 620), 'ante': (460, 620), 'atrium': (900, 900)}


def room_rect(rid):
    cx, cz = room_center(rid)
    hx, hz = SIZES[P.ROOMS[rid][3]]
    if rid == 0:
        return -320, 320, -1100, 300
    return cx - hx, cx + hx, cz - hz, cz + hz


def neighbours(rid, floor=None):
    """(side, other room, requirement crossing out of this room, door id) for each way out of the room"""
    out = []
    for a, b, ra, rb in P.DOORS:
        if rid not in (a, b):
            continue
        other = b if a == rid else a
        f = floor or P.ROOMS[rid][1]
        if rid == P.ATRIUM:
            f = P.ROOMS[other][1]
            if floor and f != floor:
                continue
        elif other == P.ATRIUM and floor and floor != P.ROOMS[rid][1]:
            continue
        ac, oc = room_center(rid), room_center(other)
        if rid == 0 and other == 7:
            side = 'W'
        elif rid == 0 and other == 8:
            side = 'E'
        elif rid == 7 and other == 0:
            side = 'E'
        elif rid == 8 and other == 0:
            side = 'W'
        elif abs(oc[0] - ac[0]) > abs(oc[1] - ac[1]):
            side = 'E' if oc[0] > ac[0] else 'W'
        else:
            side = 'S' if oc[1] > ac[1] else 'N'
        req = ra if a == rid else rb
        out.append((side, other, req, ('door', a, b), f))
    return out


def door_shift(rid, side):
    """Rooms with a broken floor across the middle: their east doorway stands in the north half, the west in the south
    half (and so do their neighbours' doorways onto them, so the passages run straight)."""
    if side not in 'EW':
        return 0
    def broken(r):
        return P.ROOMS[r][3] in ('chasm', 'timed') and P.ROOMS[r][1] == 'F1'
    if broken(rid):
        return -300 if side == 'E' else 300
    for a, b, ra, rb in P.DOORS:
        if rid in (a, b):
            other = b if a == rid else a
            if broken(other) and P.ROOMS[other][1] == P.ROOMS[rid][1] and other != P.ATRIUM and rid != P.ATRIUM:
                oc, rc = room_center(other), room_center(rid)
                if abs(oc[1] - rc[1]) < 1 and ((side == 'E' and oc[0] > rc[0]) or (side == 'W' and oc[0] < rc[0])):
                    return -300 if side == 'W' else 300   # (the neighbour's door on the facing side)
    return 0


def door_point(rid, side, floor=None):
    """Where a doorway on this side of the room is: (x, z) on the wall's line"""
    x0, x1, z0, z1 = room_rect(rid)
    cx, cz = room_center(rid)
    cz += door_shift(rid, side)
    if rid == 0:
        cz = (z0 + z1) / 2
        if side in 'EW':
            return (x1 if side == 'E' else x0), 140.0   # (the Entrance Hall's side doors stand near its south end)
    if rid == P.ATRIUM:
        A = SIZES['atrium'][0]
        return {'N': (cx, cz - A), 'S': (cx, cz + A), 'E': (cx + A, cz), 'W': (cx - A, cz)}[side]
    return {'N': (cx, z0), 'S': (cx, z1), 'E': (x1, cz), 'W': (x0, cz)}[side]


def axis_line(rid, other, side):
    """The line both doorways of a connection share (rooms of different sizes: the smaller's centre line)."""
    a = door_point(rid, side)
    return a


class Spawn:
    def __init__(self, room, kind, pos, yaw=0, p0=0, p1=0, p2=0):
        self.room, self.kind, self.pos, self.yaw, self.p = room, kind, pos, yaw, [p0, p1, p2]


class Room:
    def __init__(self, rid):
        self.id = rid
        self.name, self.floor, self.cell, self.kind = P.ROOMS[rid]
        self.spawns = []
        self.notes = []          # for the design document
        self.respawn = None


SPAWNS = []
ROOMDATA = {}
CHEST_FLAGS = {}


def add(room, kind, pos, yaw=0, p0=0, p1=0, p2=0):
    yaw = ((int(yaw) + 0x8000) % 0x10000) - 0x8000
    s = Spawn(room, kind, tuple(int(round(v)) for v in pos), yaw, int(p0), int(p1), int(p2))
    SPAWNS.append(s)
    return s


# ---------------------------------------------------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------------------------------------------------

def oriented(B, mat, pts, hint, **kw):
    C.oriented(B, mat, pts, hint, **kw)


def oct_pts(cx, cz, a, y):
    s = a * math.tan(math.pi / 8)
    return [(cx + s, y, cz - a), (cx + a, y, cz - s), (cx + a, y, cz + s), (cx + s, y, cz + a),
            (cx - s, y, cz + a), (cx - a, y, cz + s), (cx - a, y, cz - s), (cx - s, y, cz - a)]


def oct_ring_floor(B, mat, cx, cz, ri, ro, y, color=(255, 255, 255), up=True, collide=True, skip=()):
    inner, outer = oct_pts(cx, cz, ri, y), oct_pts(cx, cz, ro, y)
    for i in range(8):
        if i in skip:
            continue
        q = [inner[i], inner[(i + 1) % 8], outer[(i + 1) % 8], outer[i]]
        oriented(B, mat, q, (0, 1 if up else -1, 0), color=color,
                 collide=(C.SURF['floor'] if (collide and up) else None))


def oct_disc(B, mat, cx, cz, a, y, color=(255, 255, 255), up=True, collide=True):
    pts = oct_pts(cx, cz, a, y)
    for i in range(8):
        q = [(cx, y, cz), pts[i], pts[(i + 1) % 8]]
        oriented(B, mat, q, (0, 1 if up else -1, 0), color=color,
                 collide=(C.SURF['floor'] if (collide and up) else None))


def oct_walls(B, mat, cx, cz, a, y0, y1, openings, color=(255, 255, 255)):
    """An octagon's walls facing in: openings = {'N': [(u0, u1, v0, v1)], ...} on the four flat sides (u along x
    for N/S, z for E/W, absolute)."""
    s = a * math.tan(math.pi / 8)
    B.wall(mat, 'z', cz - a, cx - s, cx + s, y0, y1, +1, openings=openings.get('N', ()), color=color)
    B.wall(mat, 'z', cz + a, cx - s, cx + s, y0, y1, -1, openings=openings.get('S', ()), color=color)
    B.wall(mat, 'x', cx - a, cz - s, cz + s, y0, y1, +1, openings=openings.get('W', ()), color=color)
    B.wall(mat, 'x', cx + a, cz - s, cz + s, y0, y1, -1, openings=openings.get('E', ()), color=color)
    for (ax, az), (bx, bz) in (((cx + s, cz - a), (cx + a, cz - s)), ((cx + a, cz + s), (cx + s, cz + a)),
                               ((cx - s, cz + a), (cx - a, cz + s)), ((cx - a, cz - s), (cx - s, cz - a))):
        mx, mz = (ax + bx) / 2 - cx, (az + bz) / 2 - cz
        n = (-mx, 0, -mz)
        y = y0
        while y < y1 - 1:
            yb = min(y1, y + 600)
            oriented(B, mat, [(ax, y, az), (bx, y, bz), (bx, yb, bz), (ax, yb, az)], n, color=color,
                     collide=C.SURF['wall'])
            y = yb


def gate_frame(B, side, x, z, y, color=(240, 236, 228)):
    """A freestanding doorway (the Waterways' and the Sanctums' rooms have no walls): two pillars and a lintel."""
    if side in 'NS':
        for dx in (-DW - 24, DW + 24):
            C.prism(B, 'marble2', x + dx, z, 20, y, y + DH, 8, color=color, collide=True)
            C.prism(B, 'stone_band', x + dx, z, 26, y, y + 16, 8, color=GOLD)
        B.box(('marble', 'stone_band'), x - DW - 48, x + DW + 48, y + DH, y + DH + 34, z - 22, z + 22, collide=False,
              color=color)
        B.box('stone_band', x - DW - 48, x + DW + 48, y + DH + 34, y + DH + 40, z - 24, z + 24, collide=False,
              color=GOLD)
    else:
        for dz in (-DW - 24, DW + 24):
            C.prism(B, 'marble2', x, z + dz, 20, y, y + DH, 8, color=color, collide=True)
            C.prism(B, 'stone_band', x, z + dz, 26, y, y + 16, 8, color=GOLD)
        B.box(('marble', 'stone_band'), x - 22, x + 22, y + DH, y + DH + 34, z - DW - 48, z + DW + 48, collide=False,
              color=color)
        B.box('stone_band', x - 24, x + 24, y + DH + 34, y + DH + 40, z - DW - 48, z + DW + 48, collide=False,
              color=GOLD)


def side_vec(side):
    return {'N': (0, -1), 'S': (0, 1), 'E': (1, 0), 'W': (-1, 0)}[side]


# ---------------------------------------------------------------------------------------------------------------------
# Passages: each room builds its half of the way to the next, from its doorway out to the En_Holl in the middle
# ---------------------------------------------------------------------------------------------------------------------

def passage_half(B, W, rid, side, other, y, style, lights, req, half_ok=True):
    """Builds this room's half of the passage. style: 'hall' (walled, Temple of Time), 'walk' (a walkway over the
    water), 'sky' (a stone causeway in the starry dark). Returns the En_Holl point."""
    px, pz = door_point(rid, side)
    ox, oz = door_point(other, {'N': 'S', 'S': 'N', 'E': 'W', 'W': 'E'}[side])
    if rid == P.ATRIUM or other == P.ATRIUM:
        pass
    dx, dz = side_vec(side)
    # (the middle, where the room changes: halfway along the axis between the two walls)
    if side in 'NS':
        mid = (pz + oz) / 2
        a, b = pz, mid
        x = px
        lo, hi = min(a, b), max(a, b)
        box_x = (x - DW, x + DW)
    else:
        mid = (px + ox) / 2
        a, b = px, mid
        z = pz
        lo, hi = min(a, b), max(a, b)
    if style == 'hall':
        if side in 'NS':
            B.floor('marble_check', x - DW, x + DW, lo, hi, y=y)
            B.wall('brick_white2', 'x', x - DW, lo, hi, y, y + DH + 20, 1)
            B.wall('brick_white2', 'x', x + DW, lo, hi, y, y + DH + 20, -1)
            B.ceiling('ceiling', x - DW, x + DW, lo, hi, y + DH + 20)
            B.fade_to_black((x, y, a + dz * 60), (x, y, b), (x - DW - 1, x + DW + 1, y - 1, y + DH + 21, lo, hi))
            C.room_finish(B, x - DW, x + DW, lo + (16 if a < b else 0), hi - (16 if a > b else 0), y, y + DH + 20,
                          walls='WE', pilasters=False, dado=(214, 208, 198))
        else:
            B.floor('marble_check', lo, hi, z - DW, z + DW, y=y)
            B.wall('brick_white2', 'z', z - DW, lo, hi, y, y + DH + 20, 1)
            B.wall('brick_white2', 'z', z + DW, lo, hi, y, y + DH + 20, -1)
            B.ceiling('ceiling', lo, hi, z - DW, z + DW, y + DH + 20)
            B.fade_to_black((a + dx * 60, y, z), (b, y, z), (lo, hi, y - 1, y + DH + 21, z - DW - 1, z + DW + 1))
            C.room_finish(B, lo + (16 if a < b else 0), hi - (16 if a > b else 0), z - DW, z + DW, y, y + DH + 20,
                          walls='NS', pilasters=False, dado=(214, 208, 198))
    else:
        # a walkway (B1) or causeway (F2) with low parapets; over the water or the dark
        mat = 'stone_grey' if style == 'walk' else 'marble2'
        col = (205, 215, 225) if style == 'walk' else (235, 232, 245)
        state_bridge = req in ('L', 'S') and style == 'sky'
        if side in 'NS':
            if not state_bridge:
                B.box((mat, 'stone_band'), x - DW, x + DW, y - 30, y, lo, hi, color=col)
                for xx in (x - DW, x + DW):
                    B.box('marble2', xx - 6, xx + 6, y, y + 40, lo, hi, collide=False, color=col)
                col_wall(B, 'x', x - DW, lo, hi, y, y + 120, 1)
                col_wall(B, 'x', x + DW, lo, hi, y, y + 120, -1)
            else:
                add(rid, T_LIGHTBRIDGE, ((x), y, (lo + hi) / 2), 0, 0 if req == 'L' else 1, int(hi - lo), 1)
            B.fade_to_black((x, y, a + dz * 200), (x, y, b), (x - DW - 30, x + DW + 30, y - 80, y + DH, lo, hi))
            if style == 'walk':
                C.water(W, x - DW - 200, x + DW + 200, lo, hi, y - 140)
        else:
            if not state_bridge:
                B.box((mat, 'stone_band'), lo, hi, y - 30, y, z - DW, z + DW, color=col)
                for zz in (z - DW, z + DW):
                    B.box('marble2', lo, hi, y, y + 40, zz - 6, zz + 6, collide=False, color=col)
                col_wall(B, 'z', z - DW, lo, hi, y, y + 120, 1)
                col_wall(B, 'z', z + DW, lo, hi, y, y + 120, -1)
            else:
                add(rid, T_LIGHTBRIDGE, ((lo + hi) / 2, y, z), 0x4000, 0 if req == 'L' else 1, int(hi - lo), 1)
            B.fade_to_black((a + dx * 200, y, z), (b, y, z), (lo, hi, y - 80, y + DH, z - DW - 30, z + DW + 30))
            if style == 'walk':
                C.water(W, lo, hi, z - DW - 200, z + DW + 200, y - 140)
    if side in 'NS':
        return (x, y, mid), (0 if side == 'S' else -0x8000)
    return (mid, y, z), (0x4000 if side == 'E' else -0x4000)


def col_wall(B, axis, fixed, a, b, y0, y1, facing):
    """An invisible wall (collision only): parapets that keep him on a walkway."""
    if axis == 'x':
        p = [(fixed, y0, a), (fixed, y0, b), (fixed, y1, b), (fixed, y1, a)]
        if facing > 0:
            p = [p[1], p[0], p[3], p[2]]
    else:
        p = [(a, y0, fixed), (b, y0, fixed), (b, y1, fixed), (a, y1, fixed)]
        if facing < 0:
            p = [p[1], p[0], p[3], p[2]]
    idx = [B.cv(q) for q in p]
    B.col_polys.append((C.SURF['wall'], idx[0], idx[1], idx[2], 0, 0))
    B.col_polys.append((C.SURF['wall'], idx[0], idx[2], idx[3], 0, 0))


# ---------------------------------------------------------------------------------------------------------------------
# Doors and their seals (the runtime's door actors stand in the doorways)
# ---------------------------------------------------------------------------------------------------------------------

def door_kind(req):
    if req == 'key':
        return D_KEY
    if req == 'boss':
        return D_BOSS
    if req == 'L':
        return D_SUN
    if req == 'S':
        return D_MOON
    if isinstance(req, tuple):
        return D_BARS
    return None


def door_flag(req, door):
    if req == 'key':
        return KEY_DOOR_FLAG[door]
    if req == 'boss':
        return FLAG_BOSSDOOR
    if isinstance(req, tuple):
        return SOLVE_FLAG[req[1]]    # the room whose puzzle or guards open it
    return FLAG_ASPECT


def place_door(R, side, req, door, y, inset=14):
    k = door_kind(req)
    if k is None:
        return
    if R.floor == 'F2' and req in ('L', 'S'):
        return   # (bridges of light or shadow instead)
    x, z = door_point(R.id, side)
    dx, dz = side_vec(side)
    yaw = {'N': 0, 'S': -0x8000, 'E': -0x4000, 'W': 0x4000}[side]
    add(R.id, T_DOOR, (x - dx * inset, y, z - dz * inset), yaw, k, door_flag(req, door), 0)


# ---------------------------------------------------------------------------------------------------------------------
# Room styles
# ---------------------------------------------------------------------------------------------------------------------

def hall_room(R, B, W, lights, beams, H=560, gap=None):
    """The Halls of Light (F1): the Temple of Time's marble, white walls, a coved ceiling, its windows and beams."""
    x0, x1, z0, z1 = room_rect(R.id)
    y = 0
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    doors = {}
    for side, other, req, door, f in neighbours(R.id):
        x, z = door_point(R.id, side)
        u = x if side in 'NS' else z
        doors.setdefault(side, []).append((u - DW, u + DW, y, y + DH))
        R.passages.append((side, other, req, door))
    # windows where there are no doors: the middle of each side, high up, their rays falling into the room
    wins = {}
    for side in 'NSEW':
        if side in doors:
            continue
        if side in 'NS':
            pos = (cx, z0 if side == 'N' else z1)
            u = cx
        else:
            pos = (x0 if side == 'W' else x1, cz)
            u = cz
        C.window(B, lights, beams, side, pos, y + 150, 0.28, floor=y)
        wins.setdefault(side, []).append((u - 40, u + 40, y + 170, H))
    if gap is None:
        B.floor('marble_check', x0, x1, z0, z1, y=y)
    else:
        g0, g1 = (z0 + z1) / 2 - gap, (z0 + z1) / 2 + gap
        B.floor('marble_check', x0, x1, z0, g0, y=y)
        B.floor('marble_check', x0, x1, g1, z1, y=y)
        # the broken edges, the walls going on down into the dark, and two pillars standing up out of it
        oriented(B, 'brick_white2', [(x0, y - 700, g0), (x1, y - 700, g0), (x1, y, g0), (x0, y, g0)], (0, 0, 1),
                 color=(150, 145, 140), collide=C.SURF['wall'])
        oriented(B, 'brick_white2', [(x1, y - 700, g1), (x0, y - 700, g1), (x0, y, g1), (x1, y, g1)], (0, 0, -1),
                 color=(150, 145, 140), collide=C.SURF['wall'])
        for xs in ((x0, 'x', 1), (x1, 'x', -1)):
            oriented(B, 'brick_white2', [(xs[0], y - 700, g0), (xs[0], y - 700, g1), (xs[0], y, g1), (xs[0], y, g0)],
                     (xs[2], 0, 0), color=(150, 145, 140), collide=C.SURF['wall'])
        B.fade_to_black((cx, y, cz), (cx, y - 700, cz), (x0 - 1, x1 + 1, y - 701, y - 1, g0 - 1, g1 + 1))
        if R.kind == 'chasm':
            for xx in (cx - 250, cx + 250):
                C.prism(B, 'marble', xx, cz, 56, y - 700, y - 40, 8, color=(225, 222, 218), collide=True,
                        top='marble')
                C.prism(B, 'stone_band', xx, cz, 60, y - 60, y - 40, 8, color=GOLD)
        R.gap = (g0, g1)
    B.wall('brick_white2', 'x', x0, z0, z1, y, H, 1, openings=doors.get('W', ()))
    B.wall('brick_white2', 'x', x1, z0, z1, y, H, -1, openings=doors.get('E', ()))
    B.wall('brick_white2', 'z', z0, x0, x1, y, H, 1, openings=doors.get('N', ()))
    B.wall('brick_white2', 'z', z1, x0, x1, y, H, -1, openings=doors.get('S', ()))
    C.cove_ceiling(B, x0, x1, z0, z1, H, c=60)
    allops = {k: list(doors.get(k, [])) + list(wins.get(k, [])) for k in 'NSEW'}
    C.room_finish(B, x0, x1, z0, z1, y, H, pilasters=(x1 - x0 > 900), dado=(214, 208, 198), doors=allops)
    for side, ops in doors.items():
        for (u0, u1, v0, v1) in ops:
            if side in 'NS':
                C.arch_door(B, 'z', z0 if side == 'N' else z1, u0, u1, v0, v1)
            else:
                C.arch_door(B, 'x', x0 if side == 'W' else x1, u0, u1, v0, v1)
    # lights: a soft fill high up, warm pools low, candelabras in the corners
    lights.append(((cx, H - 100, cz), max(x1 - x0, z1 - z0) * 0.7, 0.28, WARM))
    for (xx, zz) in ((x0 + 60, z0 + 60), (x1 - 60, z0 + 60), (x0 + 60, z1 - 60), (x1 - 60, z1 - 60)):
        C.candelabra(B, R.light, xx, zz, y, h=140)
    return dict(x0=x0, x1=x1, z0=z0, z1=z1, y=y, H=H, cx=cx, cz=cz)


def water_room(R, B, W, lights, beams):
    """The Waterways (B1): walkways and islands over glowing magical water in the dark. An aqueduct crosses
    overhead, carrying the Atrium's overflow, and spills into the room as falls of light-water."""
    x0, x1, z0, z1 = room_rect(R.id)
    y = P.FLOORS['B1']
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    hx, hz = (x1 - x0) / 2, (z1 - z0) / 2
    # the floor: an island (a raised stone platform) in the water, walkways out to each doorway
    ix, iz = hx * 0.62, hz * 0.62
    B.box(('stone_grey', 'stone_band'), cx - ix, cx + ix, y - 60, y, cz - iz, cz + iz, color=(200, 210, 222))
    C.water(W, x0 - 600, x1 + 600, z0 - 600, z1 + 600, y - 140)
    # the glow of the water on everything above it
    for (xx, zz) in ((cx - hx * 0.8, cz - hz * 0.8), (cx + hx * 0.8, cz - hz * 0.8), (cx - hx * 0.8, cz + hz * 0.8),
                     (cx + hx * 0.8, cz + hz * 0.8)):
        lights.append(((xx, y - 100, zz), 700, 0.45, (0.55, 0.85, 1.0)))
    # pillars rising out of the water into the dark, gilded bands and lanterns
    for (xx, zz) in ((cx - hx * 0.85, cz - hz * 0.85), (cx + hx * 0.85, cz - hz * 0.85),
                     (cx - hx * 0.85, cz + hz * 0.85), (cx + hx * 0.85, cz + hz * 0.85)):
        C.prism(B, 'pillar', xx, zz, 34, y - 160, y + 900, 8, color=(210, 220, 235), collide=True)
        C.prism(B, 'stone_band', xx, zz, 40, y + 900, y + 920, 8, color=GOLD)
        C.prism(B, 'stone_band', xx, zz, 40, y - 10, y + 10, 8, color=GOLD)
    # the aqueduct overhead, and its falls
    ay = y + 620
    if hx > hz:
        B.box(('stone_grey', 'stone_band'), cx - 40, cx + 40, ay - 40, ay, z0 - 400, z1 + 400, collide=False,
              color=(170, 180, 195))
        C.falling(W, (cx - 30, z0 + 60), (cx + 30, z0 + 60), y - 140, ay - 40)
    else:
        B.box(('stone_grey', 'stone_band'), x0 - 400, x1 + 400, ay - 40, ay, cz - 40, cz + 40, collide=False,
              color=(170, 180, 195))
        C.falling(W, (x0 + 60, cz - 30), (x0 + 60, cz + 30), y - 140, ay - 40)
    lights.append(((cx, ay, cz), 900, 0.2, (0.6, 0.85, 1.0)))
    # lanterns on the island's corners
    for (xx, zz) in ((cx - ix + 40, cz - iz + 40), (cx + ix - 40, cz - iz + 40), (cx - ix + 40, cz + iz - 40),
                     (cx + ix - 40, cz + iz - 40)):
        C.prism(B, 'stone_band', xx, zz, 6, y, y + 110, 6, color=GOLD)
        C.prism(B, 'plaster', xx, zz, 5, y + 110, y + 126, 6, color=(250, 240, 220))
        C.flame(B, (xx, y + 126, zz), 0.0012, 2)
        lights.append(((xx, y + 140, zz), 300, 0.55, C.COZY))
    # walkways from the island to each doorway (the doorways stand at the room's edge)
    gaps = []
    for side, other, req, door, f in neighbours(R.id):
        x, z = door_point(R.id, side)
        dx, dz = side_vec(side)
        R.passages.append((side, other, req, door))
        if side in 'NS':
            a = cz + dz * iz
            lo, hi = min(a, z), max(a, z)
            B.box(('stone_grey', 'stone_band'), x - DW, x + DW, y - 30, y, lo, hi, color=(205, 215, 225))
            col_wall(B, 'x', x - DW, lo, hi, y, y + 120, 1)
            col_wall(B, 'x', x + DW, lo, hi, y, y + 120, -1)
        else:
            a = cx + dx * ix
            lo, hi = min(a, x), max(a, x)
            B.box(('stone_grey', 'stone_band'), lo, hi, y - 30, y, z - DW, z + DW, color=(205, 215, 225))
            col_wall(B, 'z', z - DW, lo, hi, y, y + 120, 1)
            col_wall(B, 'z', z + DW, lo, hi, y, y + 120, -1)
        gate_frame(B, side, x, z, y)
        gaps.append((side, x, z))
    parapets(B, cx, cz, ix, iz, y, gaps)
    return dict(x0=cx - ix, x1=cx + ix, z0=cz - iz, z1=cz + iz, y=y, H=900, cx=cx, cz=cz)


def parapets(B, cx, cz, ix, iz, y, gaps):
    """The island's invisible parapet (keeps him out of the water), in runs between the walkways' mouths."""
    for side, lo, hi, axis, facing in (('W', cz - iz, cz + iz, 'x', 1), ('E', cz - iz, cz + iz, 'x', -1),
                                       ('N', cx - ix, cx + ix, 'z', 1), ('S', cx - ix, cx + ix, 'z', -1)):
        plane = {'W': cx - ix, 'E': cx + ix, 'N': cz - iz, 'S': cz + iz}[side]
        cuts = sorted((u - DW, u + DW) for gs, gx, gz in gaps if gs == side for u in [gz if axis == 'x' else gx])
        u = lo
        for c0, c1 in cuts + [(hi, hi)]:
            if c0 > u:
                col_wall(B, axis, plane, u, c0, y, y + 120, facing)
            u = max(u, c1)


def sky_room(R, B, W, lights, beams):
    """The Sanctums (F2): round stone floors floating in a starry dark, after the Chamber of Sages: a ring of the
    six Sages' pillars, their colours, a Triforce set in the floor."""
    x0, x1, z0, z1 = room_rect(R.id)
    y = P.FLOORS['F2']
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    r = min(x1 - x0, z1 - z0) / 2
    a = r * 0.92
    oct_disc(B, 'marble2', cx, cz, a, y, color=(236, 232, 246))
    # the platform's underside and edge (seen from the causeways)
    pts = oct_pts(cx, cz, a, y)
    for i in range(8):
        p0, p1 = pts[i], pts[(i + 1) % 8]
        oriented(B, 'stone_band', [p0, p1, (p1[0], y - 60, p1[2]), (p0[0], y - 60, p0[2])],
                 ((p0[0] + p1[0]) / 2 - cx, 0, (p0[2] + p1[2]) / 2 - cz), color=(150, 150, 190))
    oct_disc(B, 'stone_band', cx, cz, a, y - 60, color=(70, 70, 110), up=False, collide=False)
    oct_ring_floor(B, 'stone_band', cx, cz, a - 40, a, y + 3, color=GOLD, collide=False)
    # the Triforce in the floor
    for (tx, tz) in ((0, -70), (-60, 34), (60, 34)):
        B.quad('stone_band', [(cx + tx - 60, y + 2, cz + tz + 34), (cx + tx + 60, y + 2, cz + tz + 34),
                              (cx + tx, y + 2, cz + tz - 70)], color=GOLD, light=False)
    # the Sages' pillars round the rim (the Chamber of Sages' six platforms, as pillars), each lit in its colour
    gaps = [side_angle(s) for s, *_ in neighbours(R.id)]
    if R.id == 48:
        gaps.append(side_angle('N'))
    for k in range(6):
        ang = 2 * math.pi * (k + 0.5) / 6
        if any(abs(((ang - g + math.pi) % (2 * math.pi)) - math.pi) < 0.35 for g in gaps):
            ang += 0.45
        px, pz = cx + (a - 80) * math.cos(ang), cz + (a - 80) * math.sin(ang)
        if R.kind != 'simon':
            C.prism(B, 'marble', px, pz, 24, y, y + 260, 8, color=(235, 235, 245), collide=True)
            C.prism(B, 'stone_band', px, pz, 30, y + 260, y + 280, 8, color=GOLD)
            C.disc(B, SAGE_MEDALS[k], px, pz, 26, y + 281, 12, light=False)
            lights.append(((px, y + 300, pz), 340, 0.5, SAGE_TINTS[k]))
    # invisible parapet round the rim, cut for the causeways
    for i in range(8):
        p0, p1 = pts[i], pts[(i + 1) % 8]
        am = math.atan2((p0[2] + p1[2]) / 2 - cz, (p0[0] + p1[0]) / 2 - cx)
        if any(abs(((am - g + math.pi) % (2 * math.pi)) - math.pi) < 0.3 for g in gaps):
            continue
        q = [(p1[0], y, p1[2]), (p0[0], y, p0[2]), (p0[0], y + 140, p0[2]), (p1[0], y + 140, p1[2])]
        idx = [B.cv(v) for v in q]
        B.col_polys.append((C.SURF['wall'], idx[0], idx[1], idx[2], 0, 0))
        B.col_polys.append((C.SURF['wall'], idx[0], idx[2], idx[3], 0, 0))
    lights.append(((cx, y + 400, cz), r * 1.6, 0.35, (0.85, 0.85, 1.0)))
    # causeways from the rim to each doorway
    for side, other, req, door, f in neighbours(R.id):
        x, z = door_point(R.id, side)
        dx, dz = side_vec(side)
        R.passages.append((side, other, req, door))
        edge_x, edge_z = cx + dx * (a - 10), cz + dz * (a - 10)
        if req in ('L', 'S'):
            # the bridge of light (or shadow) starts at the rim: the doorway stands at its end
            if side in 'NS':
                add(R.id, T_LIGHTBRIDGE, (x, y, (edge_z + z) / 2), 0, 0 if req == 'L' else 1, int(abs(z - edge_z)), 1)
            else:
                add(R.id, T_LIGHTBRIDGE, ((edge_x + x) / 2, y, z), 0x4000, 0 if req == 'L' else 1,
                    int(abs(x - edge_x)), 1)
        else:
            if side in 'NS':
                lo, hi = min(edge_z, z), max(edge_z, z)
                B.box(('marble2', 'stone_band'), x - DW, x + DW, y - 30, y, lo, hi, color=(235, 232, 245))
                col_wall(B, 'x', x - DW, lo, hi, y, y + 120, 1)
                col_wall(B, 'x', x + DW, lo, hi, y, y + 120, -1)
            else:
                lo, hi = min(edge_x, x), max(edge_x, x)
                B.box(('marble2', 'stone_band'), lo, hi, y - 30, y, z - DW, z + DW, color=(235, 232, 245))
                col_wall(B, 'z', z - DW, lo, hi, y, y + 120, 1)
                col_wall(B, 'z', z + DW, lo, hi, y, y + 120, -1)
        gate_frame(B, side, x, z, y, color=(236, 232, 246))
    if R.id == 48:
        # the way on to Rauru: a causeway north into a light (walking on into it leads to his arena)
        lo, hi = cz - 900, cz - a + 10
        B.box(('marble2', 'stone_band'), cx - DW, cx + DW, y - 30, y, lo, hi, color=(240, 236, 250))
        col_wall(B, 'x', cx - DW, lo, hi, y, y + 120, 1)
        col_wall(B, 'x', cx + DW, lo, hi, y, y + 120, -1)
        col_wall(B, 'z', lo, cx - DW, cx + DW, y, y + 300, 1)
        gate_frame(B, 'N', cx, cz - a - 60, y, color=(255, 240, 200))
        B.fade_to_black((cx, y, cz - a - 100), (cx, y, lo), (cx - DW - 40, cx + DW + 40, y - 40, y + DH + 60, lo, hi))
        lights.append(((cx, y + 200, cz - a - 120), 500, 0.8, (1.0, 0.95, 0.8)))
    return dict(x0=cx - a, x1=cx + a, z0=cz - a, z1=cz + a, y=y, H=600, cx=cx, cz=cz, r=a)


def side_angle(side):
    return {'E': 0.0, 'S': math.pi / 2, 'W': math.pi, 'N': -math.pi / 2}[side]


# ---------------------------------------------------------------------------------------------------------------------
# The Atrium of Light: the shaft through all three floors
# ---------------------------------------------------------------------------------------------------------------------

ATRIUM_TOP = 3300
ATRIUM_BOTTOM = -2700
PLAT_R = 260
BALC_IN = 640


def atrium(R, B, W, lights, beams):
    cx, cz = room_center(P.ATRIUM)
    A = SIZES['atrium'][0]
    ops = {}
    for floor in ('B1', 'F1', 'F2'):
        y = P.FLOORS[floor]
        for side, other, req, door, f in neighbours(P.ATRIUM, floor):
            u = cx if side in 'NS' else cz
            ops.setdefault(side, []).append((u - DW, u + DW, y, y + DH))
            R.passages.append((side, other, req, door, floor))
    oct_walls(B, 'brick_white2', cx, cz, A, ATRIUM_BOTTOM, ATRIUM_TOP, ops)
    for side, lst in ops.items():
        for (u0, u1, v0, v1) in lst:
            if side in 'NS':
                C.arch_door(B, 'z', cz - A if side == 'N' else cz + A, u0, u1, v0, v1)
            else:
                C.arch_door(B, 'x', cx - A if side == 'W' else cx + A, u0, u1, v0, v1)
    # the three levels: the Waterways' dock (a ring over the pool), the Halls' balcony, the Sanctums' gallery
    for floor, ri in (('B1', 600), ('F1', BALC_IN), ('F2', BALC_IN)):
        y = P.FLOORS[floor]
        oct_ring_floor(B, 'marble_check' if floor == 'F1' else ('stone_grey' if floor == 'B1' else 'marble2'),
                       cx, cz, ri, A, y)
        oct_ring_floor(B, 'stone_band', cx, cz, ri, A, y - 40, color=(150, 140, 130), up=False, collide=False)
        pin, pout = oct_pts(cx, cz, ri, y), oct_pts(cx, cz, ri, y - 40)
        for i in range(8):
            p0, p1 = pin[i], pin[(i + 1) % 8]
            oriented(B, 'stone_band', [p0, p1, pout[(i + 1) % 8], pout[i]], (cx - (p0[0] + p1[0]) / 2, 0,
                                                                          cz - (p0[2] + p1[2]) / 2), color=TRIM)
        # balustrade on the inner edge, broken where a bridge goes out to the platform (south, F1 and B1)
        for i in range(8):
            p0, p1 = pin[i], pin[(i + 1) % 8]
            mx, mz = (p0[0] + p1[0]) / 2 - cx, (p0[2] + p1[2]) / 2 - cz
            if floor in ('F1', 'B1') and mz > 0 and abs(mx) < 1:
                continue
            B.railing(p0[0], p1[0], p0[2], p1[2], y, h=70, posts=10)
        # wall sconces at each level
        for (sx, sz, w) in ((cx - A, cz - 250, 'W'), (cx - A, cz + 250, 'W'), (cx + A, cz - 250, 'E'),
                            (cx + A, cz + 250, 'E')):
            ix = 1 if w == 'W' else -1
            B.box('stone_band', sx + ix * 0 - 8 + ix * 8, sx + 8 + ix * 8, y + 186, y + 214, sz - 8, sz + 8,
                  collide=False, color=GOLD)
            C.prism(B, 'plaster', sx + ix * 10, sz, 4, y + 214, y + 234, 6, color=(250, 240, 220))
            C.flame(B, (sx + ix * 10, y + 234, sz), 0.0014, 2)
            lights.append(((sx + ix * 50, y + 220, sz), 380, 0.55, C.COZY))
    # the bridges from the balcony and the dock out to the platform (south side)
    for floor in ('F1', 'B1'):
        y = P.FLOORS[floor]
        ri = BALC_IN if floor == 'F1' else 600
        B.box(('marble2' if floor == 'F1' else 'stone_grey', 'stone_band'), cx - 90, cx + 90, y - 30, y,
              cz + PLAT_R - 4, cz + ri - 0.5, color=(236, 232, 226))
        col_wall(B, 'x', cx - 90, cz + PLAT_R, cz + ri, y, y + 140, 1)
        col_wall(B, 'x', cx + 90, cz + PLAT_R, cz + ri, y, y + 140, -1)
        for xx in (cx - 90, cx + 90):
            B.box('marble2', xx - 6, xx + 6, y, y + 50, cz + PLAT_R, cz + ri, collide=False)
    # the pool at the foot, and the Font's water falling the whole height from spouts under the gallery
    C.water(W, cx - 620, cx + 620, cz - 620, cz + 620, P.FLOORS['B1'] - 90)
    B.floor('stone_grey', cx - 700, cx + 700, cz - 700, cz + 700, y=ATRIUM_BOTTOM, color=(80, 110, 140))
    for k in range(4):
        ang = math.pi / 4 + k * math.pi / 2
        sx, sz = cx + 470 * math.cos(ang), cz + 470 * math.sin(ang)
        tx, tz = -math.sin(ang) * 40, math.cos(ang) * 40
        C.falling(W, (sx - tx, sz - tz), (sx + tx, sz + tz), P.FLOORS['B1'] - 90, P.FLOORS['F2'] - 40)
        C.prism(B, 'stone_band', sx, sz, 30, P.FLOORS['F2'] - 60, P.FLOORS['F2'] - 20, 8, color=GOLD, r1=44)
        lights.append(((sx, P.FLOORS['B1'] - 40, sz), 520, 0.5, (0.55, 0.85, 1.0)))
        lights.append(((sx, P.FLOORS['F1'] - 300, sz), 420, 0.3, (0.6, 0.85, 1.0)))
    # Temple of Time windows between the floors, their rays crossing the shaft
    for side in 'NSEW':
        for yy in (-1300, 1100):
            pos = {'N': (cx - 250, cz - A), 'S': (cx + 250, cz + A), 'E': (cx + A, cz - 250), 'W': (cx - A, cz + 250)}[side]
            C.window(B, lights, beams, side, pos, yy, 0.42, floor=yy - 900)
    # a vast chandelier hanging from the dome, the Font's basin overhead
    C.chandelier(B, R.light, cx, cz, ATRIUM_TOP, drop=260, r=220, arms=12, inten=0.55, radius=1500)
    C.cove_ceiling(B, cx - A, cx + A, cz - A, cz + A, ATRIUM_TOP, c=90, color=(60, 70, 140))
    lights.append(((cx, 0, cz), 1300, 0.35, WARM))
    lights.append(((cx, 2400, cz), 1300, 0.35, (0.85, 0.85, 1.0)))
    # the platform and its mirror (the runtime's): Light at the Halls, Shadow at the Waterways
    add(P.ATRIUM, T_LIFT, (cx, P.FLOORS['F1'], cz), 0, P.FLOORS['F1'], P.FLOORS['B1'], 0)
    return dict(x0=cx - A, x1=cx + A, z0=cz - A, z1=cz + A, y=0, H=ATRIUM_TOP, cx=cx, cz=cz)


TRIM = C.TRIM


# ---------------------------------------------------------------------------------------------------------------------
# The rooms' contents: puzzles, chests, guards (as spawns for the runtime) and their set pieces (geometry)
# ---------------------------------------------------------------------------------------------------------------------

def chest_spawn(R, x, y, z, yaw, item, appear_flag=-1):
    i = len(CHEST_FLAGS)
    CHEST_FLAGS[(R.id, len([1 for k in CHEST_FLAGS if k[0] == R.id]))] = i
    add(R.id, T_CHEST, (x, y, z), yaw, i, ITEMS[item], appear_flag)
    R.notes.append(f'chest ({item}){" appears once the room is solved" if appear_flag >= 0 else ""}')
    return i


def contents(R, B, W, lights, info):
    rid, kind = R.id, R.kind
    x0, x1, z0, z1, y, cx, cz = info['x0'], info['x1'], info['z0'], info['z1'], info['y'], info['cx'], info['cz']
    items = list(P.CHESTS.get(rid, []))
    solved_flag = SOLVE_FLAG.get(rid, -1)
    has_puzzle = kind in ('plates', 'braziers', 'crystals', 'beam', 'block', 'simon', 'dark')
    # a dais at the back of the room for its chest (north side unless a door's there)
    back = 'N'
    sides = {s for s, *_ in neighbours(rid)}
    for cand in ('N', 'E', 'W', 'S'):
        if cand not in sides:
            back = cand
            break
    bx, bz = {'N': (cx, z0 + 120), 'S': (cx, z1 - 120), 'E': (x1 - 120, cz), 'W': (x0 + 120, cz)}[back]
    byaw = {'N': 0, 'S': -0x8000, 'E': -0x4000, 'W': 0x4000}[back]   # (facing into the room)

    def dais_chest(item, appear):
        B.box(('marble', 'stone_band'), bx - 90, bx + 90, y, y + 12, bz - 90, bz + 90, color=(240, 236, 228))
        chest_spawn(R, bx, y + 12, bz, byaw, item, appear)

    if kind == 'plates':
        n = 3 if rid == 0 else 5
        if rid == 0:
            pts = [(-130, -840), (130, -840), (0, -960)]   # left, right, top: the Triforce's points
            order = [0, 1, 2]
        else:
            pts = [(cx - 300, cz + 150), (cx - 150, cz - 200), (cx, cz + 150), (cx + 150, cz - 200),
                   (cx + 300, cz + 150)]
            order = [2, 0, 4, 1, 3]
        for k, (px, pz) in enumerate(pts):
            add(rid, T_PLATE, (px, y, pz), 0, order.index(k), n, solved_flag)
        mz = z0 + 4 if rid != 0 else -1098
        # the mural's triangles stand left to right as the plates do (by x); it lights them in the plates' order
        rank = {k: sorted(range(n), key=lambda j: pts[j][0]).index(k) for k in range(n)}
        add(rid, T_MURAL, (cx if rid else 0, y + 330, mz), 0, n, solved_flag,
            sum(rank[order[i]] << (3 * i) for i in range(n)))
        R.notes.append(f'{n} Triforce plates: step on them in the order the mural over the north door lights them')
    elif kind == 'braziers' or kind == 'dark':
        n = {3: 2, 4: 3, 5: 4, 30: 4}.get(rid, 3)
        spots = [(cx - 300, cz - 250), (cx + 300, cz - 250), (cx - 300, cz + 250), (cx + 300, cz + 250)][:n]
        for k, (px, pz) in enumerate(spots):
            py = y
            if rid == 5 and k == 3:
                # the fourth brazier on a high ledge: reached by levitating up from the gallery
                B.box(('marble', 'stone_band'), px - 90, px + 90, y, y + 230, pz - 90, pz + 90, color=(236, 232, 226))
                py = y + 230
                R.notes.append('the fourth brazier stands on a ledge 230 up: float up to it')
            add(rid, T_BRAZIER, (px, py, pz), 0, k, n, solved_flag)
        R.notes.append(f'{n} braziers: any blow or spell lights them; all lit solves the room')
        if kind == 'dark':
            R.notes.append('dark: the braziers are only shown by faint moon glyphs on the floor in Shadow')
    elif kind == 'crystals':
        n, secs = (1, 10) if rid == 8 else (3, 14)
        for k in range(n):
            ang = 2 * math.pi * k / max(n, 1) + 0.6
            px, pz = cx + 330 * math.cos(ang), cz + 330 * math.sin(ang)
            add(rid, T_CRYSTAL, (px, y, pz), 0, k, n, secs)
        R.notes.append(f'{n} crystal switch(es): strike all within {secs} s (a ticking clock) to open the way')
    elif kind == 'beam':
        nm = {12: 1, 26: 2, 46: 3}[rid]
        # the sun's beam enters from a window in the west wall, low; mirrors on pedestals turn it 90 degrees
        ex, ez = x0 + 20, cz + 200
        add(rid, T_EMITTER, (ex, y + 80, ez), 0x4000, 0, 0, 0)
        mirrors = [(cx - 150, cz + 200), (cx - 150, cz - 250), (cx + 250, cz - 250)][:nm]
        for k, (mx, mz) in enumerate(mirrors):
            B.box(('marble', 'stone_band'), mx - 40, mx + 40, y, y + 30, mz - 40, mz + 40)
            add(rid, T_BEAMMIRROR, (mx, y + 30, mz), 0, (k + 1) % 4, k, 0)
        tx, tz = {1: (cx - 150, z0 + 20), 2: (x1 - 20, cz - 250), 3: (cx + 250, z0 + 20)}[nm]
        add(rid, T_TARGET, (tx, y + 80, tz), 0, solved_flag, 0, 0)
        R.notes.append(f'light: a sunbeam from the west window; {nm} mirror(s) turned by striking them; the sun emblem lit '
                       'solves the room')
    elif kind == 'block':
        B.box(('marble2', 'stone_band'), cx - 300, cx + 300, y, y + 4, cz - 300, cz + 300, collide=False,
              color=(220, 215, 205))   # a grid of floor tiles the block slides on
        add(rid, T_BLOCK, (cx - 200, y, cz + 200), 0, 0, 0, 0)
        add(rid, T_SOCKET, (cx + 200, y, cz + 200), 0, solved_flag, 0, 0)
        add(rid, T_HEAVY, (cx + 200, y, cz - 200), 0, solved_flag, 0, 0)
        add(rid, T_GUARD, (cx - 250, y, cz - 250), 0, GUARD_IRON, 0, 0)
        R.notes.append('kick the block (it slides till it hits something) onto its socket; the iron guard is heavy '
                       'enough to hold the second plate down: lure him onto it. Both held: the chest appears')
    elif kind == 'simon':
        for k in range(6):
            ang = 2 * math.pi * (k + 0.5) / 6
            px, pz = cx + 380 * math.cos(ang), cz + 380 * math.sin(ang)
            add(rid, T_PILLAR, (px, y, pz), 0, k, SIMON_ORDER.index(k), solved_flag)
        add(rid, T_INSCRIPTION, (cx, y, cz), 0, 0, 0, 0)
        R.notes.append('six Sage pillars: strike them in the order the Sages stood in the Mirror Room, from the '
                       'broken door (Water, Shadow, Fire, Spirit, Forest, Light)')
    elif kind in ('combat', 'miniboss'):
        if rid == 2:
            g = [(cx - 200, cz - 200, GUARD_SPEAR), (cx + 200, cz - 200, GUARD_SPEAR), (cx, cz + 150, GUARD_SHIELD)]
        elif rid == 25:
            g = [(cx, cz - 250, GUARD_CAPTAIN), (cx - 250, cz, GUARD_IRON), (cx + 250, cz, GUARD_IRON)]
        else:
            g = [(cx - 200, cz - 200, GUARD_SWORD), (cx + 200, cz - 200, GUARD_SHIELD), (cx, cz + 250, GUARD_BOW),
                 (cx - 250, cz + 200, GUARD_SPEAR)]
        for gx, gz, t in g:
            add(rid, T_GUARD, (gx, y, gz), -0x8000, t, solved_flag, 1)
        R.notes.append(f'guards: {len(g)}; the doors are barred till they fall' +
                       (' (the Guard Captain: mini-boss music, a charge)' if rid == 25 else ''))
    elif kind == 'chasm':
        R.notes.append('a chasm across the middle: float over (falling costs half a heart, back at the door)')
    elif kind == 'timed':
        add(rid, T_SWITCH, (cx, y, z1 - 200), 0, 8, 0, 0)
        add(rid, T_LIGHTBRIDGE, (cx, y, cz), 0, 2, 470, 1)
        R.notes.append('a floor switch: a bridge of light spans the chasm for 8 seconds')
    elif kind == 'mirror':
        B.box(('marble', 'stone_band'), cx - 160, cx + 160, y, y + 12, cz - 160, cz + 160)
        B.box(('marble', 'stone_band'), cx - 120, cx + 120, y + 12, y + 24, cz - 120, cz + 120)
        add(rid, T_MIRROR, (cx, y + 24, cz), 0, 1, 0, -1)
        R.notes.append('a master mirror: strike it and the whole temple turns from Light to Shadow, or back')
    # teleport mirrors
    for a, b, st in P.TELEPORTS:
        if rid in (a, b):
            other = b if rid == a else a
            mx, mz = {'N': (cx, z0 + 40), 'S': (cx, z1 - 40), 'E': (x1 - 40, cz), 'W': (x0 + 40, cz)}[
                _free_side(rid, prefer=('E', 'W', 'S', 'N'), avoid=back if items else None)]
            yaw = math.atan2(cx - mx, cz - mz)
            asp = {None: -1, 'L': 0, 'S': 1, 'oneway': -1}[st]
            kind_m = 0 if not (st == 'oneway' and rid == b) else 3   # 3: a dead reflection (the far end of one-way)
            y16 = ((int(round(yaw / math.pi * 0x8000)) + 0x8000) % 0x10000) - 0x8000
            R.tele = getattr(R, 'tele', []) + [(other, (mx, y, mz), y16, asp, kind_m)]
    # chests
    for k, item in enumerate(items):
        appear = solved_flag if (has_puzzle and kind != 'dark' and k == 0) else -1
        if rid == 13:
            appear = solved_flag
        dais_chest(item, appear)
    # set pieces
    decorate(R, B, W, lights, info)


def _free_side(rid, prefer, avoid=None):
    sides = {s for s, *_ in neighbours(rid)}
    for s in prefer:
        if s not in sides and s != avoid:
            return s
    for s in prefer:
        if s not in sides:
            return s
    return prefer[0]


def decorate(R, B, W, lights, info):
    """Set pieces: what each room is, beyond its puzzle."""
    rid, kind = R.id, R.kind
    x0, x1, z0, z1, y, cx, cz = info['x0'], info['x1'], info['z0'], info['z1'], info['y'], info['cx'], info['cz']
    if R.floor == 'F1' and kind in ('quiet', 'hub', 'treasure', 'secret'):
        # statues of robed figures (the Sages of old) along the walls, banners
        for (sx, sz, face) in ((cx - (x1 - x0) / 2 + 80, cz, 'E'), (cx + (x1 - x0) / 2 - 80, cz, 'W')):
            if (face == 'E' and any(s == 'W' for s, *_ in neighbours(rid))) or \
               (face == 'W' and any(s == 'E' for s, *_ in neighbours(rid))):
                continue
            C.statue(B, sx, sz, y, face=face, color=(225, 222, 230))
    if rid == 1:
        # the Sanctum: a Triforce dais under a ring of light, four great pillars
        B.box(('marble', 'stone_band'), cx - 180, cx + 180, y, y + 16, cz - 180, cz + 180, color=(240, 236, 228))
        for (tx, tz) in ((0, -60), (-52, 30), (52, 30)):
            B.quad('stone_band', [(cx + tx - 52, y + 17, cz + tz + 30), (cx + tx + 52, y + 17, cz + tz + 30),
                                  (cx + tx, y + 17, cz + tz - 60)], color=GOLD, light=False)
        for (px, pz) in ((cx - 330, cz - 330), (cx + 330, cz - 330), (cx - 330, cz + 330), (cx + 330, cz + 330)):
            B.pillar(px, pz, 34, y, 560)
        lights.append(((cx, 300, cz), 520, 0.5, (1.0, 0.95, 0.8)))
        R.notes.append('the old Sanctum, now the way to the Atrium: a sun seal north, closed in Shadow')
    if rid == 0:
        for (px, pz) in ((-230, -700), (230, -700), (-230, -300), (230, -300), (-230, 100), (230, 100)):
            B.pillar(px, pz, 30, y, 600)
        B.floor('carpet_red', -60, 60, -1000, 260, y=3)
        R.notes.append('the mirror back to the Mirror Room on the south wall; the great door north')
    if kind == 'treasure' or kind == 'secret':
        lights.append(((cx, y + 200, cz), 420, 0.6, (1.0, 0.9, 0.6)))
    if rid == 48:
        R.notes.append('the boss door north; beyond, Rauru\'s arena')


# ---------------------------------------------------------------------------------------------------------------------
# Building one room
# ---------------------------------------------------------------------------------------------------------------------

def build_room(rid, fixed_lights=None):
    R = Room(rid)
    R.passages = []
    lights = []
    beams = []

    def light(pos, radius, inten, tint=WARM):
        lights.append((pos, radius, inten, tint))
    R.light = light
    B = S.Builder(fixed_lights if fixed_lights is not None else lights, 700.0)
    B.AMBIENT = 0.34
    B.flames = []
    W = C.Builder()
    if rid == P.ATRIUM:
        info = atrium(R, B, W, lights, beams)
        holls = []
        for side, other, req, door, floor in R.passages:
            pt, yaw = passage_half(B, W, rid, side, other, P.FLOORS[floor],
                                   {'F1': 'hall', 'B1': 'walk', 'F2': 'sky'}[floor], lights, req)
            holls.append((other, pt, yaw))
            place_door(R, side, req, door, P.FLOORS[floor])
        R.respawn = (info['cx'], 0, info['cz'] + 760, -0x8000)
    else:
        if R.floor == 'F1':
            if R.kind in ('chasm', 'timed'):
                info = hall_room(R, B, W, lights, beams, H=640, gap=180 if R.kind == 'chasm' else 220)
            else:
                info = hall_room(R, B, W, lights, beams, H=600 if rid in (0, 1) else 560)
            style = 'hall'
        elif R.floor == 'B1':
            info = water_room(R, B, W, lights, beams)
            style = 'walk'
        else:
            info = sky_room(R, B, W, lights, beams)
            style = 'sky'
        holls = []
        for side, other, req, door in R.passages:
            pt, yaw = passage_half(B, W, rid, side, other, info['y'], style, lights, req)
            holls.append((other, pt, yaw))
            place_door(R, side, req, door, info['y'])
        contents(R, B, W, lights, info)
        # where he comes back to after a fall: the room's middle, or in front of its first doorway
        R.respawn = (info['cx'], info['y'], info['cz'], 0)
        if R.kind in ('chasm', 'timed') or R.floor in ('B1', 'F2'):
            s = R.passages[0][0] if R.passages else 'S'
            x, z = door_point(rid, s)
            dx, dz = side_vec(s)
            R.respawn = (x - dx * 120, info['y'], z - dz * 120, {'N': -0x8000, 'S': 0, 'E': -0x4000, 'W': 0x4000}[s])
            if R.floor != 'F1':
                R.respawn = (info['cx'], info['y'], info['cz'], 0)
    R.holls = holls
    R.info = info
    return R, B, W, lights, beams




# ---------------------------------------------------------------------------------------------------------------------
# The props the runtime draws (front +z, foot at the origin unless said)
# ---------------------------------------------------------------------------------------------------------------------

def build_props():
    PR = {}
    glow = {}     # (tinted by the actor each frame: TEXEL0 x PRIMITIVE)
    Bd = S.Builder
    # doors: a slab filling the doorway (2*DW+8 wide, DH+10 high, 24 thick), its face showing what holds it
    def slab():
        b = Bd()
        b.box(('marble', 'marble'), -DW - 4, DW + 4, 0, DH + 10, -12, 12)
        b.box('gold', -DW - 4, DW + 4, 0, 10, -13, 13, collide=False, color=GOLD)
        b.box('gold', -DW - 4, DW + 4, DH, DH + 10, -13, 13, collide=False, color=GOLD)
        return b
    k = slab()   # the key door: iron bands crossed over it, a lock in the middle
    for (a, c) in ((-1, 1), (1, -1)):
        k.quad('stone_band', [(a * DW, 20, 12.5), (a * DW + 18 * a * -1, 20, 12.5), (c * DW, DH - 20, 12.5),
                              (c * DW - 18 * c * -1, DH - 20, 12.5)][::1], color=(70, 64, 60))
    k.box('stone_band', -28, 28, 110, 170, 12, 22, collide=False, color=(90, 84, 78))
    k.box('gold', -10, 10, 125, 150, 22, 24, collide=False, color=GOLD)
    PR['gGdfTDoorKey'] = k
    bo = slab()  # the boss door: gilded, the Triforce and the Sages' six colours round it
    bo.box('gold', -DW, DW, 20, DH - 10, 12, 14, collide=False, color=(240, 200, 120))
    for (cx_, cy_) in ((0, 230), (-34, 170), (34, 170)):
        bo.quad('gold', [(cx_ - 34, cy_ - 58, 14.5), (cx_ + 34, cy_ - 58, 14.5), (cx_, cy_, 14.5)], color=(255, 235, 150))
    bo.box('stone_band', -40, 40, 70, 120, 14, 24, collide=False, color=(80, 70, 90))
    PR['gGdfTDoorBoss'] = bo
    sun = slab()  # sun seal: a gold sun disc with rays
    C.disc(sun, 'gold', 0, 0, 0, 0, 1) if False else None
    for i in range(12):
        a0 = 2 * math.pi * i / 12
        sun.quad('gold', [(0, 150, 12.6), (70 * math.cos(a0 - 0.12), 150 + 70 * math.sin(a0 - 0.12), 12.6),
                          (70 * math.cos(a0 + 0.12), 150 + 70 * math.sin(a0 + 0.12), 12.6)][::-1], color=GOLD)
    PR['gGdfTDoorSun'] = sun
    moon = slab()  # moon seal: a silver crescent
    for i in range(10):
        a0, a1 = math.pi * (0.25 + 1.5 * i / 10), math.pi * (0.25 + 1.5 * (i + 1) / 10)
        q = [(60 * math.cos(a0), 150 + 60 * math.sin(a0), 12.6), (60 * math.cos(a1), 150 + 60 * math.sin(a1), 12.6),
             (40 * math.cos(a1) + 14, 150 + 40 * math.sin(a1), 12.6), (40 * math.cos(a0) + 14, 150 + 40 * math.sin(a0), 12.6)]
        moon.quad('mirror', q[::-1], color=(170, 180, 230))
    moon.box(('marble', 'marble'), -DW - 4, DW + 4, 0, DH + 10, -12, 12, collide=False, color=(150, 150, 190))
    PR['gGdfTDoorMoon'] = moon
    bars = Bd()   # bars: gilded bars across the doorway
    for xx in range(-DW + 10, DW, 30):
        C.prism(bars, 'gold', xx, 0, 5, 0, DH + 10, 6, color=GOLD)
    for yy in (40, DH - 30):
        bars.box('gold', -DW - 4, DW + 4, yy, yy + 12, -6, 6, collide=False, color=GOLD)
    PR['gGdfTDoorBars'] = bars
    # the doors' collision: one slab for all of them
    col = Bd()
    col.box('marble', -DW - 4, DW + 4, 0, DH + 10, -12, 12)
    PR['__col_door'] = col
    # chest: body and lid (the lid's hinge at its back edge, y 50 z -30)
    cb = Bd()
    cb.box(('chest_wood', 'chest_wood'), -44, 44, 0, 50, -30, 30, color=(240, 228, 215))
    cb.box('gold', -46, 46, 0, 6, -32, 32, collide=False, color=GOLD)
    PR['gGdfTChest'] = cb
    cl = Bd()
    cl.box(('chest_wood', 'chest_wood'), -44, 44, 0, 22, 0, 60, collide=False, color=(240, 228, 215))
    cl.box('gold', -10, 10, -14, 4, 60, 63, collide=False, color=GOLD)
    PR['gGdfTChestLid'] = cl
    ccol = Bd()
    ccol.box('marble', -44, 44, 0, 50, -30, 30)
    PR['__col_chest'] = ccol
    # bridges of light / shadow / the timed bridge: a unit slab (1 long along z, 2*DW wide, 30 deep), scaled in z
    br = Bd()
    br.box(('mirror', 'mirror'), -DW, DW, -30, 0, -0.5, 0.5, color=(255, 255, 255))
    glow['gGdfTBridge'] = br
    bcol = Bd()
    bcol.box('marble', -DW, DW, -30, 0, -0.5, 0.5)
    PR['__col_bridge'] = bcol
    # the atrium's platform: a great round platform, gilded rim, the mirror's dais
    lf = Bd()
    C.prism(lf, 'marble2', 0, 0, PLAT_R, -40, 0, 24, top='marble2', color=(236, 232, 226))
    C.annulus(lf, 'stone_band', 0, 0, PLAT_R - 30, PLAT_R, 1, 24, color=GOLD)
    C.prism(lf, 'stone_band', 0, 0, PLAT_R * 0.5, -140, -40, 16, color=(180, 170, 160), r1=PLAT_R * 0.95)
    lf.box(('marble', 'stone_band'), -90, 90, 0, 14, -90, 90, color=(240, 236, 228))
    PR['gGdfTLift'] = lf
    lcol = Bd()
    C.prism(lcol, 'marble2', 0, 0, PLAT_R, -40, 0, 16, top='marble2', collide=True)
    PR['__col_lift'] = lcol
    # the kick block: a marble block with gilded edges (100 on a side)
    bk = Bd()
    bk.box(('marble', 'marble2'), -50, 50, 0, 100, -50, 50, color=(240, 236, 228))
    for yy in (0, 94):
        bk.box('gold', -52, 52, yy, yy + 6, -52, 52, collide=False, color=GOLD)
    PR['gGdfTBlock'] = bk
    kcol = Bd()
    kcol.box('marble', -50, 50, 0, 100, -50, 50)
    PR['__col_block'] = kcol
    # floor pieces: a plate (Triforce), a socket (the block's), a heavy iron plate, a floor switch
    pl = Bd()
    C.prism(pl, 'stone_band', 0, 0, 56, 0, 4, 12, top='stone_band', color=(200, 190, 170))
    PR['gGdfTPlate'] = pl
    tri = Bd()
    for (cx_, cz_) in ((0, -20), (-17, 10), (17, 10)):
        tri.quad('mirror', [(cx_ - 17, 4.5, cz_ + 15), (cx_ + 17, 4.5, cz_ + 15), (cx_, 4.5, cz_ - 15)],
                 color=(255, 255, 255), light=False)
    glow['gGdfTPlateGlow'] = tri
    so = Bd()
    C.annulus(so, 'stone_band', 0, 0, 52, 62, 1.5, 16, color=GOLD)
    so.box('stone_band', -50, 50, 0, 2, -50, 50, collide=False, color=(110, 100, 90))
    PR['gGdfTSocket'] = so
    hv = Bd()
    hv.box(('stone_band', 'stone_band'), -60, 60, 0, 8, -60, 60, collide=False, color=(80, 76, 72))
    for xx in (-30, 0, 30):
        hv.box('stone_band', xx - 4, xx + 4, 8, 10, -50, 50, collide=False, color=(120, 114, 108))
    PR['gGdfTHeavy'] = hv
    sw = Bd()
    sw.box(('gold', 'stone_band'), -36, 36, 0, 10, -36, 36, collide=False, color=GOLD)
    PR['gGdfTSwitch'] = sw
    # a crystal switch on a little stand, the crystal itself tinted (red unstruck, blue struck)
    cs = Bd()
    C.prism(cs, 'marble', 0, 0, 24, 0, 30, 8, top='marble')
    PR['gGdfTCrystalBase'] = cs
    cr = Bd()
    v = [(0, 1, 0), (1, 0, 0), (0, 0, 1), (-1, 0, 0), (0, 0, -1), (0, -1, 0)]
    for top in (0, 5):
        for i in range(4):
            a, b = v[1 + i], v[1 + (i + 1) % 4]
            t = [(20 * c[0], 70 + 34 * c[1], 20 * c[2]) for c in (v[top], a, b)]
            n = np.cross(np.subtract(t[1], t[0]), np.subtract(t[2], t[0]))
            if np.dot(n, np.subtract(np.mean(t, 0), (0, 70, 0))) < 0:
                t = [t[0], t[2], t[1]]
            cr.quad('mirror', t, color=(255, 255, 255), light=False)
    glow['gGdfTCrystal'] = cr
    # a Sage's pillar (Simon): a column with a medallion disc set in its top, the band tinted
    sp = Bd()
    C.prism(sp, 'marble', 0, 0, 34, 0, 220, 8, color=(235, 235, 245), collide=False)
    C.prism(sp, 'stone_band', 0, 0, 40, 0, 16, 8, color=GOLD)
    PR['gGdfTPillar'] = sp
    spg = Bd()
    C.prism(spg, 'mirror', 0, 0, 40, 220, 244, 8, color=(255, 255, 255))
    glow['gGdfTPillarGlow'] = spg
    # a turning mirror on a pedestal (the light puzzles): its face +z
    bm = Bd()
    C.prism(bm, 'gold', 0, 0, 8, 0, 60, 6, color=GOLD)
    bm.box('gold', -46, 46, 60, 150, -6, -2, collide=False, color=GOLD)
    bm.box('mirror', -40, 40, 66, 144, -2, 1, collide=False, color=(230, 240, 255))
    PR['gGdfTBeamMirror'] = bm
    # the sun emblem the light must reach (on a wall, its face +z), tinted by the actor
    se = Bd()
    for i in range(12):
        a0 = 2 * math.pi * i / 12
        se.quad('mirror', [(0, 0, 2), (60 * math.cos(a0 + 0.13), 60 * math.sin(a0 + 0.13), 2),
                           (60 * math.cos(a0 - 0.13), 60 * math.sin(a0 - 0.13), 2)], color=(255, 255, 255),
                light=False)
    C.disc(se, 'mirror', 0, 0, 0, 0, 3) if False else None
    glow['gGdfTSunEmblem'] = se
    # a beam of light: a unit long glowing bar (along +z, 1 long), scaled by the actor
    bmn = Bd()
    for (a, b) in (((-10, -10), (10, -10)), ((10, -10), (10, 10)), ((10, 10), (-10, 10)), ((-10, 10), (-10, -10))):
        bmn.quad('mirror', [(a[0], a[1], 0), (b[0], b[1], 0), (b[0], b[1], 1), (a[0], a[1], 1)],
                 color=(255, 255, 255), light=False)
        bmn.quad('mirror', [(b[0], b[1], 0), (a[0], a[1], 0), (a[0], a[1], 1), (b[0], b[1], 1)],
                 color=(255, 255, 255), light=False)
    glow['gGdfTBeam'] = bmn
    # the mural: three to five triangles in a gilded frame (on a wall, face +z), each lit by the actor in turn
    for i in range(5):
        t = Bd()
        cx_ = (i - 2) * 70
        t.quad('mirror', [(cx_ - 30, -26, 3), (cx_ + 30, -26, 3), (cx_, 26, 3)], color=(255, 255, 255), light=False)
        glow[f'gGdfTMuralTri{i}'] = t
    mf = Bd()
    mf.box('gold', -190, 190, -50, 50, 0, 2, collide=False, color=(120, 100, 70))
    PR['gGdfTMural'] = mf
    # a stone tablet (the Sanctum of Fire's inscription)
    tb = Bd()
    tb.box(('marble2', 'marble2'), -60, 60, 0, 120, -14, 14, color=(225, 220, 210))
    tb.box('gold', -64, 64, 120, 128, -16, 16, collide=False, color=GOLD)
    PR['gGdfTTablet'] = tb
    return PR, glow


# ---------------------------------------------------------------------------------------------------------------------
# The scene
# ---------------------------------------------------------------------------------------------------------------------

# lighting settings: 0/1 Halls (Light/Shadow), 2/3 Waterways, 4/5 Sanctums (the runtime picks one per room and aspect)
LIGHTING = [
    (60, 58, 54, 0, 112, -58, 120, 112, 100, 0, -112, 58, 70, 66, 60, 12, 10, 8, 2010, 7000),     # Halls, Light
    (34, 36, 54, 0, 112, -58, 60, 64, 110, 0, -112, 58, 40, 44, 70, 4, 4, 14, 1990, 6000),       # Halls, Shadow
    (30, 44, 56, 0, 112, -58, 70, 110, 140, 0, -112, 58, 40, 70, 90, 0, 6, 12, 1960, 5200),      # Waterways, Light
    (22, 22, 40, 0, 112, -58, 50, 50, 100, 0, -112, 58, 30, 30, 60, 2, 0, 8, 1940, 4600),        # Waterways, Shadow
    (60, 56, 70, 0, 112, -58, 120, 116, 150, 0, -112, 58, 70, 70, 100, 6, 6, 20, 2000, 7000),    # Sanctums, Light
    (30, 26, 50, 0, 112, -58, 70, 60, 130, 0, -112, 58, 40, 30, 80, 4, 2, 14, 1980, 6000),       # Sanctums, Shadow
]


def emit(files):
    rooms = {}
    holls = {}
    col_verts, col_polys = [], []
    stats = {}
    for rid in sorted(P.ROOMS):
        SP_before = len(SPAWNS)
        R, B, W, lights, beams = build_room(rid)
        del SPAWNS[SP_before:]                 # (first pass: lights only; the spawns are made again below)
        for k in [k for k in CHEST_FLAGS if k[0] == rid]:
            del CHEST_FLAGS[k]
        R, B, W, _, beams = build_room(rid, fixed_lights=lights)
        pruned = C.prune_buried(B)
        rooms[rid] = R
        opa = BASE + f'{SCENE}_room_{rid}DL_main'
        xlu = BASE + f'{SCENE}_room_{rid}DL_water'
        C.emit_dl(B, opa, BASE + f'{SCENE}_room_{rid}Vtx_main', files)
        has_w = any(W.faces.values())
        if has_w:
            C.emit_dl(W, xlu, BASE + f'{SCENE}_room_{rid}Vtx_water', files)
        # collision into the scene's one mesh
        off = len(col_verts)
        col_verts += B.col_verts
        col_polys += [(s, a + off, b + off, c + off, d, e) for s, a, b, c, d, e in B.col_polys]
        actors = [G.actor_entry(DEMO_KANKYO, pos, (int(sc * 10000), yaw, 0), LIGHT_PLANE) for pos, yaw, sc in beams]
        for pos, sc, kind in sorted(B.flames, key=lambda f: (0, 2, 1).index(f[2])):
            actors.append(G.actor_entry(EN_LIGHT, pos, (kind, 0, max(1, round(sc * 10000))), 0x0000))
        files[BASE + f'{SCENE}_room_{rid}'] = G.room_file([(opa, xlu if has_w else '')], objects=(OBJECT_TOKI_OBJECTS,),
                                                          actors=actors, echo=12, behavior=(0, 0), skymod=(0, 1))
        for other, pt, yaw in R.holls:
            key = tuple(sorted((rid, other)))
            holls.setdefault(key, []).append((rid, pt, yaw))
        stats[rid] = (sum(len(v) for v in B.faces.values()), len(B.col_polys), pruned)
    # the room-change planes: one per passage (from the two halves' common middle)
    trans = []
    for (a, b), lst in sorted(holls.items()):
        rid, pt, yaw = lst[0]
        other = b if rid == a else a
        # (local +z is the way the plane faces: the room on that side is sides[1])
        if yaw in (0, 0x4000):        # (facing S or E from rid: the other room lies on +z)
            front, back = rid, other
        else:
            front, back = other, rid
        yaw16 = yaw & 0xFFFF
        if yaw == -0x8000:
            yaw16 = 0
        elif yaw == -0x4000:
            yaw16 = 0x4000
        trans.append((front, 0xFF, back, 0xFF, EN_HOLL, int(pt[0]), int(pt[1]), int(pt[2]),
                      struct.unpack('<h', struct.pack('<H', yaw16))[0], 4 << 6))
    col_path = BASE + f'{SCENE}_sceneCollisionHeader_main'
    files[col_path] = G.collision_file(col_verts, col_polys, C.SURFACES, cam_settings=(1, 1))
    spawns = [G.actor_entry(0, (0, 0, 180), (0, -0x8000, 0), 0x0FFF)]
    files[BASE + f'{SCENE}_scene'] = G.scene_file([BASE + f'{SCENE}_room_{r}' for r in sorted(P.ROOMS)], col_path,
                                                  spawns, [(0, 0)], exits=(), lighting=[tuple(((v - 256) if v > 127 else v) if i < 18 else v for i, v in enumerate(l)) for l in LIGHTING],
                                                  sound=(0, 19, 0x56), special=(0, 3), sky=(0, 0, 0, 1),
                                                  camera=(0, 0), trans=trans)
    # the props
    PR, glow = build_props()
    for name, B in PR.items():
        if name.startswith('__col'):
            continue
        S.emit_obj(B, name, files)
    plain = C.material_cmds
    combine = glow_combine()
    C.material_cmds = lambda t: [c for c in plain(t) if c[3] not in (0xFA, 0xFC)] + [combine]
    for name, B in glow.items():
        S.emit_obj(B, name, files)
    C.material_cmds = plain
    for name in ('door', 'chest', 'bridge', 'lift', 'block'):
        B = PR['__col_' + name]
        files[f'objects/gdf_sacred/gGdfTCol_{name}'] = G.collision_file(B.col_verts, B.col_polys, C.SURFACES,
                                                                         cam_settings=(1,))
    return rooms, trans, stats, len(col_polys), len(col_verts)


def glow_combine():
    a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0 = 1, 15, 3, 7, 7, 7, 7, 6
    a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1 = 0, 15, 4, 7, 7, 7, 7, 6
    w0 = (0xFC << 24) | (a0 << 20) | (c0 << 15) | (Aa0 << 12) | (Ac0 << 9) | (a1 << 5) | c1
    w1 = (b0 << 28) | (b1 << 24) | (Aa1 << 21) | (Ac1 << 18) | (d0 << 15) | (Ab0 << 12) | (Ad0 << 9) | (d1 << 6) | \
         (Ab1 << 3) | Ad1
    return struct.pack('<II', w0, w1)


# ---------------------------------------------------------------------------------------------------------------------
# The runtime's data (C++ header)
# ---------------------------------------------------------------------------------------------------------------------

def write_header(path, rooms):
    out = []
    out.append('// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI '
               'tools, without the author\'s written consent. See AI-POLICY.md.')
    out.append('// Generated by tools/gdf_castle/lighttemple.py: the Temple of Light\'s rooms and what stands in them. '
               'Do not edit by hand.')
    out.append('#pragma once')
    out.append('')
    out.append(f'#define GDF_T_ROOMS {len(P.ROOMS)}')
    out.append(f'#define GDF_T_ATRIUM {P.ATRIUM}')
    out.append(f'#define GDF_T_FLAG_ASPECT 0x{FLAG_ASPECT:02X}')
    out.append(f'#define GDF_T_FLAG_BOSSDOOR 0x{FLAG_BOSSDOOR:02X}')
    out.append(f'#define GDF_T_FLAG_KEYDOOR0 0x{FLAG_KEYDOOR0:02X}')
    out.append(f'#define GDF_T_KEYDOORS {len(P.KEY_DOORS)}')
    out.append(f'#define GDF_T_PLAT_R {PLAT_R}.0f')
    out.append(f'#define GDF_T_ANTE {48}')
    out.append('')
    out.append('// Rooms: floor (0 Halls, 1 Waterways, 2 Sanctums, 3 the atrium), the middle, the floor height, where a '
               'fall puts him back, kind')
    out.append('typedef struct { s8 floor; s16 cx, cz, y; s16 rx, ry, rz, ryaw; s8 dark; s8 solveFlag; s8 kind; '
               'const char* name; } GdfTRoom;')
    out.append('// kinds: ' + ', '.join(f'{i} {k}' for i, k in enumerate(KINDS)))
    out.append('static const GdfTRoom sGdfTRooms[GDF_T_ROOMS] = {')
    for rid in sorted(P.ROOMS):
        R = rooms[rid]
        fl = {'F1': 0, 'B1': 1, 'F2': 2, 'ALL': 3}[R.floor]
        cx, cz = room_center(rid)
        y = 0 if rid == P.ATRIUM else P.FLOORS[R.floor]
        rx, ry, rz, ryaw = [int(round(v)) for v in R.respawn]
        out.append(f'    {{ {fl}, {int(cx)}, {int(cz)}, {y}, {rx}, {ry}, {rz}, {ryaw}, {1 if R.kind == "dark" else 0}, '
                   f'{SOLVE_FLAG.get(rid, -1)}, {KINDS.index(R.kind)}, "{R.name}" }},')
    out.append('};')
    out.append('')
    out.append('// Things to place: room, type, position, facing, three parameters')
    out.append('typedef struct { u8 room; u8 type; s16 x, y, z; s16 yaw; s16 p0, p1, p2; } GdfTSpawn;')
    out.append(f'#define GDF_T_SPAWNS {len(SPAWNS)}')
    out.append('static const GdfTSpawn sGdfTSpawns[GDF_T_SPAWNS] = {')
    for s in SPAWNS:
        out.append(f'    {{ {s.room}, {s.kind}, {s.pos[0]}, {s.pos[1]}, {s.pos[2]}, {s.yaw}, {s.p[0]}, {s.p[1]}, {s.p[2]} }},')
    out.append('};')
    out.append('')
    # mirrors: (room, position, facing, its pair's index, aspect, kind)
    mir = []
    for rid in sorted(P.ROOMS):
        for (other, pos, yaw, asp, kind) in getattr(rooms[rid], 'tele', []):
            mir.append((rid, pos, yaw, other, asp, kind))
    idx = {(m[0], m[3]): i for i, m in enumerate(mir)}
    out.append('// Mirrors linking the floors: room, position, facing, the index of its pair, the aspect it works in (-1 '
               'both), kind (0 linked, 3 only a reflection: the far end of a one-way mirror)')
    out.append('typedef struct { u8 room; s16 x, y, z, yaw; s8 pair; s8 aspect; s8 kind; } GdfTMirror;')
    out.append(f'#define GDF_T_MIRRORS {len(mir)}')
    out.append('static const GdfTMirror sGdfTMirrors[GDF_T_MIRRORS] = {')
    for (rid, pos, yaw, other, asp, kind) in mir:
        out.append(f'    {{ {rid}, {int(pos[0])}, {int(pos[1])}, {int(pos[2])}, {yaw}, {idx.get((other, rid), -1)}, {asp}, '
                   f'{kind} }},')
    out.append('};')
    out.append('')
    out.append('// Chests: how many there are (their flags are 0..n-1, the temple scene\'s treasure flags)')
    out.append(f'#define GDF_T_CHESTS {len(CHEST_FLAGS)}')
    keys = [CHEST_FLAGS[k] for k in CHEST_FLAGS if P.CHESTS[k[0]][k[1]] == 'key']
    out.append('static const s8 sGdfTKeyChests[] = { ' + ', '.join(str(k) for k in sorted(keys)) + ' };')
    out.append('')
    open(path, 'w').write('\n'.join(out) + '\n')


# ---------------------------------------------------------------------------------------------------------------------
# The design document
# ---------------------------------------------------------------------------------------------------------------------

def write_doc(path, rooms, report, route, stats, ncol):
    L = []
    L.append('# The Temple of Light: design and plan\n')
    L.append('Generated by `soh/tools/gdf_castle/lighttemple.py` from the plan in `templeplan.py` (rooms, doors, '
             'locks, chests). The solver\'s report is at the end (Document J).\n')
    L.append('## A. Identity\n')
    L.append('- **Name:** the Temple of Light, the Sacred Realm\'s temple, reached through the Sages\' mirror in the '
             'Temple of Time.')
    L.append('- **Theme:** a temple built by the Sages to keep the Triforce: light and its reflection. Every room is '
             'either the Halls (the Temple of Time\'s marble), the Waterways below (the Font\'s water, dark), or the '
             'Sanctums above (the Chamber of Sages\' floating rounds).')
    L.append('- **Ancient purpose:** the Font of Light at the top of the Atrium fed the whole temple; mirrors carried '
             'its light to every floor. The Sages sealed each way with Light and Shadow, so that only one who '
             'understood both could reach the Triforce.')
    L.append('- **Inhabitants:** Hyrule\'s guards, sent in by the King (spears, swords, bows, shields, iron '
             'guards), their Captain, and Rauru, the Sage of Light.')
    L.append('- **Main danger:** falls: into the Halls\' broken floors, the Waterways\' water, the Sanctums\' sky.')
    L.append('- **Central mechanic:** the temple\'s two aspects, Light and Shadow, flipped by striking a master '
             'mirror. Sun seals open in Light, moon seals in Shadow; the Waterways\' gates follow the water (high in '
             'Light); the Sanctums\' bridges are light (Light) or shadow (Shadow).')
    L.append('- **Gimmick:** the Atrium\'s mirror stands on a great platform: striking it flips the temple and '
             'carries the platform, and him, between the Halls and the Waterways.')
    L.append('- **Music:** an organ-led arrangement (from a MIDI, on the game\'s own instruments: Ganon\'s church '
             'organ, the orchestra\'s drums and timpani, bass, brass).\n')
    L.append('## B. Map\n')
    for floor in ('F2', 'F1', 'B1'):
        L.append(f'### {floor} ({ {"F1": "the Halls, y 0", "B1": "the Waterways, y -2400", "F2": "the Sanctums, y 2400"}[floor] })\n')
        grid = {}
        for rid, (name, fl, (c, r), kind) in P.ROOMS.items():
            if fl == floor or rid == P.ATRIUM:
                grid[(c, r)] = rid
        rows = sorted({r for c, r in grid})
        L.append('```')
        for r in rows:
            line = ''
            for c in range(5):
                rid = grid.get((c, r))
                line += (f'[{rid:2d}]' if rid is not None else '    ') + '  '
            L.append(line)
        L.append('```\n')
    L.append('| Room | Name | Floor | Kind | Ways out (requirement) |')
    L.append('| - | - | - | - | - |')
    for rid in sorted(P.ROOMS):
        name, fl, cell, kind = P.ROOMS[rid]
        ways = []
        for a, b, ra, rb in P.DOORS:
            if rid in (a, b):
                other = b if a == rid else a
                req = ra if a == rid else rb
                ways.append(f'{other} ({_req(req)})')
        for a, b, st in P.TELEPORTS:
            if rid == a or (rid == b and st != 'oneway'):
                other = b if rid == a else a
                ways.append(f'mirror to {other} ({_req(st)})')
        L.append(f'| {rid} | {name} | {fl} | {kind} | {", ".join(ways)} |')
    L.append('')
    L.append('## C. Rooms\n')
    for rid in sorted(P.ROOMS):
        R = rooms[rid]
        x0, x1, z0, z1 = room_rect(rid)
        L.append(f'**{rid}. {R.name}** ({R.floor}, {int(x1 - x0)} x {int(z1 - z0)}): ' +
                 ('; '.join(R.notes) if R.notes else 'a quiet room') + '.')
    L.append('')
    L.append('## D. Actors\n')
    L.append('| Actor | What it does |')
    L.append('| - | - |')
    for k, v in (('Door', 'key (kick it with a key), boss (the boss key), sun / moon seal (open in Light / Shadow), '
                          'bars (the room\'s puzzle or guards). A door shows why it is shut; a kick at a key door with no '
                          'key says so.'),
                 ('Chest', 'kicked open: small key, boss key, rupees, hearts, magic, a piece of heart. Some appear only '
                           'once their room is solved.'),
                 ('Master mirror', 'struck: the temple flips between Light and Shadow (a flash, the light changes).'),
                 ('Atrium platform', 'its mirror flips the temple and carries the platform between the Halls (Light) '
                                     'and the Waterways (Shadow).'),
                 ('Linked mirror', 'walk into it: out of its pair on another floor. Some work in one aspect only '
                                   '(their glass glows gold in Light, violet in Shadow, grey when dead).'),
                 ('Bridge of light / shadow', 'solid and drawn only in its aspect. The timed bridge stands for 8 s '
                                              'after its switch is pressed.'),
                 ('Triforce plates', 'step on them in the mural\'s order; a wrong step buzzes and resets.'),
                 ('Brazier', 'lit by any blow or spell; stays lit (saved).'),
                 ('Crystal switch', 'struck: turns blue; all within the time limit (a ticking clock) or they reset.'),
                 ('Sage pillar', 'struck in the right order (the Mirror Room\'s), each lights in its Sage\'s colour.'),
                 ('Turning mirror', 'struck: turns 90 degrees; the sunbeam bounces off it; the sun emblem lit '
                                    'solves the room.'),
                 ('Kick block', 'kicked: slides along the floor until it meets a wall or another thing.'),
                 ('Socket / heavy plate', 'held down by the block / by something heavy (an iron guard, or him).')):
        L.append(f'| {k} | {v} |')
    L.append('')
    L.append('## E. Enemies\n')
    L.append('The Sacred Realm guards (GanondorfSacredTemple.cpp): spear, sword, bow, shield, iron guard, the '
             'Captain. The iron guard is slow and heavy: heavy plates take his weight (the Block Vault\'s puzzle '
             'needs him lured onto one). Guards in a room bar its doors until they fall.\n')
    L.append('## F. Dependencies\n')
    L.append('Small keys and the doors they open, the aspect each region needs, the puzzles that open bars: '
             'see the map table (B). The solver explores every order of spending keys and every mirror strike.\n')
    L.append('## G. Events and saved state\n')
    L.append(f'- Switch flags of the temple scene (all within the 32 the save keeps): 0x00..0x{FLAG_KEYDOOR0 - 1:02X} '
             f'one per room with a puzzle or guards (solved / beaten), '
             f'0x{FLAG_KEYDOOR0:02X}.. key doors opened, 0x{FLAG_ASPECT:02X} the aspect (set: Shadow), '
             f'0x{FLAG_BOSSDOOR:02X} the boss door. Treasure flags 0..{len(CHEST_FLAGS) - 1}: the chests. All saved.')
    L.append('- Small keys held = key chests opened - key doors opened.\n')
    L.append('## H. Treasure\n')
    L.append('| Room | Item |')
    L.append('| - | - |')
    for rid in sorted(P.CHESTS):
        for item in P.CHESTS[rid]:
            L.append(f'| {rid} {P.ROOMS[rid][0]} | {item} |')
    L.append('')
    L.append('## I. Walkthrough (the fewest mirror strikes)\n')
    for step in route:
        kind, what, aspect = step
        if kind == 'mirror':
            L.append(f'- strike the master mirror in room {what} ({_place(what)}): the temple turns to '
                     f'{"Light" if aspect == "L" else "Shadow"}')
        else:
            L.append(f'- spend a key on the door between {what[1]} and {what[2]}')
    L.append('')
    L.append('## J. Quality assurance\n')
    for k, v in report.items():
        L.append(f'- {k}: {v}')
    L.append(f'- collision polygons: {ncol} (the scene\'s limit is 32767)')
    L.append('')
    open(path, 'w').write('\n'.join(L) + '\n')


def _req(r):
    if r is None:
        return 'open'
    if r == 'oneway':
        return 'one way'
    if isinstance(r, tuple):
        return f'{"puzzle" if r[0] == "puz" else "guards"} of {r[1]}'
    return {'L': 'Light', 'S': 'Shadow', 'key': 'small key', 'boss': 'boss key'}[r]


def _place(p):
    if isinstance(p, str):
        return {'A1': 'the Atrium, Halls level', 'A0': 'the Atrium, Waterways level', 'A2': 'the Atrium, gallery'}[p]
    return P.ROOMS[p][0]


if __name__ == '__main__':
    out = sys.argv[1]
    header = None
    doc = None
    if '--header' in sys.argv:
        header = sys.argv[sys.argv.index('--header') + 1]
    if '--doc' in sys.argv:
        doc = sys.argv[sys.argv.index('--doc') + 1]
    ok, report = P.solve()
    if not ok:
        raise SystemExit(f'the plan does not hold: {report}')
    cost, route = P.shortest_route()
    files = S.emit()
    # (sacred.py's first hall is replaced by the temple)
    for k in list(files):
        if k.startswith(BASE):
            del files[k]
    rooms, trans, stats, ncol, nverts = emit(files)
    if ncol > 32000:
        raise SystemExit(f'too much collision: {ncol} polygons')
    G.pack(files, out)
    if header:
        write_header(header, rooms)
    if doc:
        write_doc(doc, rooms, report, route, stats, ncol)
    print('rooms', len(rooms), 'passages', len(trans), 'spawns', len(SPAWNS), 'chests', len(CHEST_FLAGS),
          'collision', ncol, nverts, 'faces', sum(v[0] for v in stats.values()))
