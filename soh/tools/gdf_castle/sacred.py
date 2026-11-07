"""The way into the Sacred Realm Temple for "Play as Ganondorf" (stage 1), generated like the castle interior.

  gdf_sacred.o2r
    objects/gdf_sacred/   the sealed door set into the Temple of Time's west wall (its frame, its slab with the
                          barred window, the dark doorway once it's broken in), and the Sages' mirror (frame, glass)
    sasatest_scene        the Mirror Room (SCENE_SASATEST): a passage from the broken door, then a hall of six
                          pillars (one for each Sage) and, on a dais at the far end, the mirror
    syotes_scene          the Sacred Realm Temple's first hall (SCENE_SYOTES): where the mirror lets him out, a long
                          hall of light ending at a great sealed door (the rest of the temple comes in later stages)

    python3 sacred.py out/gdf_sacred.o2r

Floor y 0 in both rooms. The model's "front" is +z (the actors turn it to face the way they need).
"""
import os, sys
sys.path.insert(0, '/home/claude/o2r')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # (this folder's castle.py first)
import numpy as np
import castle as C
import o2rgen as G

SURF = C.SURF


class Builder(C.Builder):
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
        if self.lights is None or not light:
            return super().quad(mat, pts, collide=collide, color=color, uv=uv, light=light)
        P = np.array(pts, float)
        n = np.cross(P[1] - P[0], P[2] - P[0])
        n /= max(np.linalg.norm(n), 1e-9)
        # The texture coordinates as the castle's builder gives them, for the whole face
        probe = C.Builder()
        probe.quad(mat, pts, color=(255, 255, 255), uv=uv, light=False)
        UV = np.array([(v[3], v[4]) for v in probe.faces[mat][0]], float)
        if collide is not None:
            idx = [self.cv(q) for q in P]
            for tri in ((0, 1, 2), (0, 2, 3)) if len(idx) == 4 else ((0, 1, 2),):
                self.col_polys.append((collide, idx[tri[0]], idx[tri[1]], idx[tri[2]], 0, 0))
        def vert(q, t):
            c = np.asarray(color, float) * self.lum(q, n)
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
C.FLOOR = 0
C.MAT['mirror'] = C.tex('tokinoma_sceneTex_010FA0')   # the glass: plain marble, tinted by the actor each frame
C.MAT['gold'] = C.tex('nakaniwa_room_0Tex_013E18')     # gilded bands (the stone band, tinted gold)
C.OBJ_PRIM['mirror'] = None                            # (the actor sets its colour)

GOLD = (255, 205, 110)
IRON = (70, 64, 60)
DARK = (28, 24, 30)


def emit_obj(B, name, files):
    C.emit_dl(B, f'objects/gdf_sacred/{name}DL', f'objects/gdf_sacred/{name}Vtx', files, obj=True)


# ---- The sealed door (front +z, its foot at the origin, the wall at z 0) ----

def build_door():
    frame, slab, recess = Builder(), Builder(), Builder()
    # Frame: two jambs and a lintel standing proud of the wall, gilded edges
    for x0, x1 in ((-95, -75), (75, 95)):
        frame.box(('marble2', 'marble2'), x0, x1, 0, 250, 0, 22, collide=False)
        frame.box('gold', x0 - 3, x1 + 3, 0, 14, 0, 26, collide=False, color=GOLD)
    frame.box(('marble2', 'marble2'), -110, 110, 250, 282, 0, 26, collide=False)
    frame.box('gold', -110, 110, 278, 288, 0, 28, collide=False, color=GOLD)
    # Above the lintel: the Triforce, three gold triangles
    for cx, cy in ((0, 370), (-24, 332), (24, 332)):
        frame.quad('gold', [(cx - 24, cy - 38, 4), (cx + 24, cy - 38, 4), (cx, cy, 4)], color=GOLD)
    # The slab: a stone door with no keyhole, and a barred window through which the next room's glow shows
    w0, w1, v0, v1 = -42, 42, 120, 205
    slab.wall('marble', 'z', 12, -75, 75, 0, 250, 1, openings=[(w0, w1, v0, v1)], collide=False)
    slab.quad('marble', [(-75, 0, 2), (-75, 0, 12), (-75, 250, 12), (-75, 250, 2)])
    slab.quad('marble', [(75, 0, 12), (75, 0, 2), (75, 250, 2), (75, 250, 12)])
    for i in range(6):  # the grates: iron bars across the window
        x = w0 + (w1 - w0) * (i + 0.5) / 6
        slab.box('stone_band', x - 3, x + 3, v0, v1, 8, 16, collide=False, color=IRON)
    for y in (v0 + 28, v0 + 58):
        slab.box('stone_band', w0, w1, y - 3, y + 3, 7, 17, collide=False, color=IRON)
    slab.box('gold', -60, 60, 40, 48, 12, 15, collide=False, color=GOLD)  # a gilded band, no lock
    # Behind the window: a dark hollow (the glow is drawn in it by the actor)
    slab.quad('ceiling', [(w0, v0, -40), (w1, v0, -40), (w1, v1, -40), (w0, v1, -40)], color=DARK, light=False)
    for a, b in (((w0, -40), (w0, 12)), ((w1, 12), (w1, -40))):
        slab.quad('ceiling', [(a[0], v0, a[1]), (b[0], v0, b[1]), (b[0], v1, b[1]), (a[0], v1, a[1])], color=DARK,
                  light=False)
    # Broken open: the dark doorway
    recess.quad('ceiling', [(-75, 0, -60), (75, 0, -60), (75, 250, -60), (-75, 250, -60)], color=DARK, light=False)
    recess.quad('ceiling', [(-75, 0, 0), (-75, 0, -60), (-75, 250, -60), (-75, 250, 0)], color=DARK, light=False)
    recess.quad('ceiling', [(75, 0, -60), (75, 0, 0), (75, 250, 0), (75, 250, -60)], color=DARK, light=False)
    recess.quad('ceiling', [(-75, 250, -60), (75, 250, -60), (75, 250, 0), (-75, 250, 0)], color=DARK, light=False)
    recess.quad('marble_check', [(-75, 1, 0), (75, 1, 0), (75, 1, -60), (-75, 1, -60)], color=(110, 100, 95))
    return frame, slab, recess


# ---- The Sages' mirror (front +z, standing on the floor at the origin) ----

def build_mirror():
    frame, glass = Builder(), Builder()
    W, H, B0 = 105, 330, 30
    frame.box(('gold', 'marble2'), -W - 30, W + 30, 0, B0, -20, 20, color=GOLD)  # its plinth
    for x0, x1 in ((-W - 22, -W), (W, W + 22)):
        frame.box('gold', x0, x1, B0, H, -14, 14, collide=False, color=GOLD)
    frame.box('gold', -W - 22, W + 22, H, H + 22, -14, 14, collide=False, color=GOLD)
    # A crest: the Triforce over the mirror
    for cx, cy in ((0, H + 122), (-30, H + 74), (30, H + 74)):
        frame.quad('gold', [(cx - 30, cy - 48, 15), (cx + 30, cy - 48, 15), (cx, cy, 15)], color=GOLD)
    glass.quad('mirror', [(-W, B0, 2), (W, B0, 2), (W, H, 2), (-W, H, 2)], color=(200, 220, 255), light=False)
    return frame, glass


SAGE_TINTS = [(0.35, 1.0, 0.4), (1.0, 0.45, 0.25), (0.35, 0.6, 1.0), (0.8, 0.4, 1.0), (1.0, 0.7, 0.3), (1.0, 0.95, 0.5)]
WARM = (1.0, 0.9, 0.72)

# ---- Windows and their sunrays ----
# The Temple of Time's own sunbeam (Demo_Kankyo's light plane, object_toki_objects_DL_008390, drawn by the actor):
# a pointed window of light at its back (local z -68, y 100..661) and rays falling from its origin, down and forward
# (to y -939 some 600..1040 ahead). The game code scales it to the room (GanondorfSacred.cpp, sBeamScale).
DEMO_KANKYO, LIGHT_PLANE, OBJECT_TOKI_OBJECTS = 0x008C, 0x000E, 0x005E
MIRROR_ROOM_BEAM, FIRST_HALL_BEAM, HUB_BEAM = 0.26, 0.3, 0.38  # (the same in GanondorfSacred.cpp)


def window(B, lights, beams, wall, pos, y0, scale, frame=True):
    """A window in a wall with the sunbeam falling through it into the room.
    wall: 'W' (x = pos[0], facing +x), 'E', 'S' (z = pos[1], facing -z) or 'N'; pos: (x, z) on the wall's plane;
    y0: the beam's origin (the window's foot is a little above it)."""
    inward = {'W': (1, 0), 'E': (-1, 0), 'N': (0, 1), 'S': (0, -1)}[wall]
    yaw = {'W': 0x4000, 'E': -0x4000, 'N': 0, 'S': -0x8000}[wall]
    ox, oz = pos[0] + inward[0] * 68 * scale, pos[1] + inward[1] * 68 * scale
    beams.append(((ox, y0, oz), yaw))
    w, y_lo, y_hi = 100 * scale, y0 + 100 * scale, y0 + 661 * scale
    if frame:  # a gilded stone frame around the light, standing out of the wall
        x, z = pos
        d = 10
        def slab(u0, u1, v0, v1, depth=d):
            if wall in 'WE':
                B.box(('stone_band', 'stone_band'), x - depth if wall == 'E' else x, x + depth if wall == 'W' else x,
                      v0, v1, z + u0, z + u1, collide=False, color=GOLD)
            else:
                B.box(('stone_band', 'stone_band'), x + u0, x + u1, v0, v1, z - depth if wall == 'S' else z,
                      z + depth if wall == 'N' else z, collide=False, color=GOLD)
        slab(-w - 14, -w, y_lo - 10, y_hi - 40)
        slab(w, w + 14, y_lo - 10, y_hi - 40)
        slab(-w - 14, w + 14, y_lo - 18, y_lo - 4, d + 4)   # the sill
    # Light: the window itself, and the pool where its rays reach the floor
    reach = 825 * scale * min(1.0, y0 / (939 * scale))
    lights.append(((pos[0] + inward[0] * 30, (y_lo + y_hi) / 2, pos[1] + inward[1] * 30), 300, 0.55, WARM))
    lights.append(((ox + inward[0] * reach, 30, oz + inward[1] * reach), 230, 0.75, (1.0, 0.95, 0.8)))


# ---- The Mirror Room (SCENE_SASATEST) ----

def build_mirror_room(beams):
    pillars = [(-240, -200), (-240, -20), (-240, 160), (240, 160), (240, -20), (240, -200)]
    lights = [((0, 170, -320), 560, 0.95, (1.0, 0.95, 0.88)),   # the mirror's glow
              ((0, 400, -40), 420, 0.4, WARM),                  # light from above, the middle of the hall,
              ((0, 70, -40), 320, 0.6, WARM),                   # and its pool on the floor
              ((0, 230, 470), 300, 0.45, WARM)]                 # the passage
    lights += [((x, 340, z), 230, 0.4, t) for (x, z), t in zip(pillars, SAGE_TINTS)]  # each Sage's colour
    for wall, x in (('W', -340), ('E', 340)):
        window(None, lights, beams, wall, (x, -110), 200, MIRROR_ROOM_BEAM, frame=False)
    B = Builder(lights, 420.0)
    for wall, x in (('W', -340), ('E', 340)):
        window(B, [], [], wall, (x, -110), 200, MIRROR_ROOM_BEAM)
    H = 420
    # The passage from the broken door: south, z 300..620 (the door is at its far end)
    B.floor('marble_check', -90, 90, 300, 620, y=0)
    B.wall('brick_white', 'x', -90, 300, 620, 0, 260, 1)
    B.wall('brick_white', 'x', 90, 300, 620, 0, 260, -1)
    B.wall('brick_white', 'z', 620, -90, 90, 0, 260, -1)  # (the way out: walking up to it)
    B.ceiling('ceiling', -90, 90, 300, 620, 260)
    # The hall of the Sages
    B.floor('marble_check', -340, 340, -360, 300, y=0)
    B.floor('carpet_red', -60, 60, -250, 300, y=1, surface='floor')
    B.wall('brick_white', 'x', -340, -360, 300, 0, H, 1)
    B.wall('brick_white', 'x', 340, -360, 300, 0, H, -1)
    B.wall('brick_white', 'z', -360, -340, 340, 0, H, 1)
    B.wall('brick_white', 'z', 300, -340, 340, 0, H, -1, openings=[(-90, 90, 0, 260)])
    B.ceiling('ceiling', -340, 340, -360, 300, H)
    for x in (-240, 240):
        for zc in (-200, -20, 160):
            B.pillar(x, zc, 26, 0, H)
    # The dais at the north end, two steps up
    B.box(('marble', 'stone_band'), -170, 170, 0, 12, -360, -230)
    B.box(('marble', 'stone_band'), -130, 130, 12, 24, -360, -270)
    return B


# ---- The Sacred Realm Temple's first hall (SCENE_SYOTES) ----

def build_first_hall(beams):
    lights = [((0, 460, z), 400, 0.3, WARM) for z in (150, -250, -650)]    # light down the hall,
    lights += [((0, 70, z), 330, 0.45, WARM) for z in (150, -250, -650)]   # pooling on the floor
    lights += [((0, 250, -1050), 520, 0.85, (1.0, 0.85, 0.5)),            # the great door, gilded
               ((0, 170, 280), 360, 0.6, (1.0, 0.95, 0.88))]               # the mirror behind him
    spots = [(wall, x, z) for z in (-50, -350, -650) for wall, x in (('W', -320), ('E', 320))]
    for wall, x, z in spots:
        window(None, lights, beams, wall, (x, z), 300, FIRST_HALL_BEAM, frame=False)
    B = Builder(lights, 520.0)
    for wall, x, z in spots:
        window(B, [], [], wall, (x, z), 300, FIRST_HALL_BEAM)
    H = 520
    B.floor('marble_check', -320, 320, -1100, 300, y=0)
    B.floor('carpet_red', -70, 70, -1000, 220, y=1)
    B.wall('brick_white2', 'x', -320, -1100, 300, 0, H, 1)
    B.wall('brick_white2', 'x', 320, -1100, 300, 0, H, -1)
    B.wall('brick_white2', 'z', 300, -320, 320, 0, H, -1)
    B.wall('brick_white2', 'z', -1100, -320, 320, 0, H, 1, openings=[(-150, 150, 0, 340)])
    # Behind the great door: a short dark passage to the Sanctum (walking into it leads on)
    B.floor('marble_check', -150, 150, -1260, -1100, y=0)
    B.wall('brick_white2', 'x', -150, -1260, -1100, 0, 340, 1)
    B.wall('brick_white2', 'x', 150, -1260, -1100, 0, 340, -1)
    B.wall('brick_white2', 'z', -1260, -150, 150, 0, 340, 1)
    B.ceiling('ceiling', -150, 150, -1260, -1100, 340)
    B.ceiling('ceiling', -320, 320, -1100, 300, H)
    for zc in (100, -200, -500, -800):
        for x in (-220, 220):
            B.pillar(x, zc, 30, 0, H)
    # The great door's lintel (the door itself is an actor: it rises when the plates are stepped in order)
    B.box('gold', -170, 170, 340, 362, -1100, -1074, color=GOLD)
    for x0 in (-170, 150):
        B.box(('marble2', 'marble2'), x0, x0 + 20, 0, 340, -1100, -1080)
    return B


# ---- The great door and its Triforce plates (actors; front +z, the door's foot centre at the origin) ----
GREAT_DOOR_TRIS = [(0, 290), (-46, 210), (46, 210)]  # top, left, right: the order the plates are named in


def build_great_door():
    door = Builder()
    door.box(('marble2', 'marble2'), -150, 150, 0, 340, -10, 10, collide=True)
    door.box('gold', -150, 150, 0, 14, -12, 12, collide=False, color=GOLD)
    tris = []
    for cx, cy in GREAT_DOOR_TRIS:
        t = Builder()
        t.quad('mirror', [(cx - 44, cy - 76, 12), (cx + 44, cy - 76, 12), (cx, cy, 12)], color=(255, 255, 255),
               light=False)
        tris.append(t)
    plate = Builder()
    plate.quad('mirror', [(-60, 1, 45), (60, 1, 45), (0, 1, -60)], color=(255, 255, 255), light=False)
    rim = Builder()
    for (ax, az), (bx, bz) in (((-60, 45), (60, 45)), ((60, 45), (0, -60)), ((0, -60), (-60, 45))):
        dx, dz = bx - ax, bz - az
        n = (dz / np.hypot(dx, dz) * 8, -dx / np.hypot(dx, dz) * 8)
        rim.quad('gold', [(ax, 0.5, az), (bx, 0.5, bz), (bx + n[0], 0.5, bz + n[1]), (ax + n[0], 0.5, az + n[1])],
                 color=GOLD)
    return door, tris, plate, rim


# ---- The Sanctum of Light: the hub (SCENE_SYOTES2) ----
# An octagon (apothem 560, sides 464), 600 high. South: the way back to the first hall. North: the great door to
# Rauru (a later stage). East and west: the sealed doors to the wings. A ring of eight pillars about a Triforce dais;
# windows with sunrays in the four diagonal walls.
HUB_A, HUB_S = 560, 232


def build_hub(beams):
    H = 600
    lights = [((0, 80, 0), 380, 0.6, WARM), ((0, 560, 0), 500, 0.3, WARM)]
    diag = []
    for sx, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        # the middle of each diagonal wall, and its inward direction
        mx, mz = sx * (HUB_A + HUB_S) / 2, sz * (HUB_A + HUB_S) / 2
        diag.append((mx, mz, -sx / np.sqrt(2), -sz / np.sqrt(2)))
    for mx, mz, ix, iz in diag:
        yaw = int(np.degrees(np.arctan2(ix, iz)) / 360 * 65536)
        o = (mx + ix * 68 * HUB_BEAM, 320, mz + iz * 68 * HUB_BEAM)
        beams.append((o, yaw))
        reach = 825 * HUB_BEAM * min(1.0, 320 / (939 * HUB_BEAM))
        lights.append(((mx + ix * 30, 420, mz + iz * 30), 320, 0.55, WARM))
        lights.append(((o[0] + ix * reach, 30, o[2] + iz * reach), 240, 0.75, (1.0, 0.95, 0.8)))
    lights += [((0, 200, -HUB_A + 20), 380, 0.7, (1.0, 0.85, 0.5))]  # the gilded door north
    B = Builder(lights, float(H))
    E = HUB_A + 40
    B.floor('marble_check', -E, E, -E, E, y=0)
    B.floor('carpet_red', -70, 70, 140, HUB_A, y=1)
    B.ceiling('ceiling', -E, E, -E, E, H)
    door = (-110, 110, 0, 300)
    B.wall('brick_white2', 'z', HUB_A, -HUB_S, HUB_S, 0, H, -1, openings=[door])     # south: the way back
    B.wall('brick_white2', 'z', -HUB_A, -HUB_S, HUB_S, 0, H, 1)                      # north
    B.wall('brick_white2', 'x', HUB_A, -HUB_S, HUB_S, 0, H, -1)                      # east
    B.wall('brick_white2', 'x', -HUB_A, -HUB_S, HUB_S, 0, H, 1)                      # west
    for sx, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        a = (sx * HUB_S, sz * HUB_A)
        b = (sx * HUB_A, sz * HUB_S)
        if sx * sz > 0:
            a, b = b, a
        if sz > 0:
            a, b = b, a
        if sx < 0:
            a, b = b, a
        # facing the centre
        p = [(a[0], 0, a[1]), (b[0], 0, b[1]), (b[0], H, b[1]), (a[0], H, a[1])]
        n = np.cross(np.subtract(p[1], p[0]), np.subtract(p[3], p[0]))
        if np.dot(n, [-(a[0] + b[0]), 0, -(a[1] + b[1])]) < 0:
            p = [p[1], p[0], p[3], p[2]]
        B.quad('brick_white2', p, collide=SURF['wall'])
    # The way back: a short passage south, ending dark
    B.floor('marble_check', -110, 110, HUB_A, HUB_A + 160, y=0)
    B.wall('brick_white2', 'x', -110, HUB_A, HUB_A + 160, 0, 300, 1)
    B.wall('brick_white2', 'x', 110, HUB_A, HUB_A + 160, 0, 300, -1)
    B.wall('brick_white2', 'z', HUB_A + 160, -110, 110, 0, 300, -1)
    B.ceiling('ceiling', -110, 110, HUB_A, HUB_A + 160, 300)
    # The sealed doors: north to Rauru (gilded, with the Triforce), east and west to the wings
    B.box(('marble2', 'marble2'), -160, 160, 0, 380, -HUB_A, -HUB_A + 22)
    B.box('gold', -170, 170, 380, 400, -HUB_A, -HUB_A + 26, color=GOLD)
    for cx, cy in ((0, 330), (-46, 250), (46, 250)):
        B.quad('gold', [(cx - 44, cy - 76, -HUB_A + 23), (cx + 44, cy - 76, -HUB_A + 23), (cx, cy, -HUB_A + 23)],
               color=GOLD)
    for sx in (-1, 1):
        x0, x1 = (HUB_A - 20, HUB_A) if sx > 0 else (-HUB_A, -HUB_A + 20)
        B.box(('marble2', 'marble2'), x0, x1, 0, 300, -120, 120)
        B.box('gold', x0 - 2, x1 + 2, 300, 316, -130, 130, color=GOLD)
    # The ring of pillars and the dais
    for k in range(8):
        a = np.pi / 8 + k * np.pi / 4
        B.pillar(380 * np.cos(a), 380 * np.sin(a), 30, 0, H)
    B.box(('marble', 'stone_band'), -150, 150, 0, 14, -150, 150)
    B.box(('marble', 'stone_band'), -100, 100, 14, 28, -100, 100)
    for cx, cz in ((0, -40), (-35, 20), (35, 20)):
        B.quad('gold', [(cx - 34, 28.5, cz + 30), (cx + 34, 28.5, cz + 30), (cx, 28.5, cz - 30)], color=GOLD)
    return B


# The Temple of Time's own lighting and fog (its scene's first four settings): dim, the fog dark and close
LIGHTING = [(50, 50, 50, 0, 112, -58, -56, -56, -56, 0, -112, 58, 80, 80, 80, 10, 10, 10, 2012, 6000),
            (50, 50, 50, 0, 112, -58, -101, -101, -101, 0, -112, 58, 50, 50, 50, 10, 10, 10, 2004, 6000),
            (60, 60, 60, 8, 26, -123, -1, -1, -1, 8, -26, 123, 50, 50, 50, 0, 0, 0, -28, 6000),
            (50, 50, 50, 0, 112, -58, 120, 120, 120, 0, -112, 58, 50, 50, 50, 0, 0, 0, -28, 6000)]


def scene(files, name, B, spawns, entrances, exits, beams=()):
    base = f'scenes/shared/{name}_scene/'
    C.emit_dl(B, base + f'{name}_room_0DL_main', base + f'{name}_room_0Vtx_main', files)
    col = base + f'{name}_sceneCollisionHeader_main'
    files[col] = G.collision_file(B.col_verts, B.col_polys, C.SURFACES, cam_settings=(1, 1))
    room = base + f'{name}_room_0'
    actors = [G.actor_entry(DEMO_KANKYO, pos, (0, yaw, 0), LIGHT_PLANE) for pos, yaw in beams]
    files[room] = G.room_file([(base + f'{name}_room_0DL_main', '')], objects=(OBJECT_TOKI_OBJECTS,), actors=actors,
                              echo=12,
                              behavior=(0, 0), skymod=(0, 1))
    files[base + f'{name}_scene'] = G.scene_file([room], col, spawns, entrances, exits=exits, lighting=LIGHTING,
                                                 sound=(0, 19, 0x56), special=(0, 3), sky=(0, 0, 0, 1),
                                                 camera=(0, 0))


def emit():
    files = {}
    frame, slab, recess = build_door()
    emit_obj(frame, 'gGdfSealedDoorFrame', files)
    emit_obj(slab, 'gGdfSealedDoorSlab', files)
    emit_obj(recess, 'gGdfSealedDoorway', files)
    mframe, glass = build_mirror()
    emit_obj(mframe, 'gGdfMirrorFrame', files)
    # (the glass's colour is the actor's: no colour of its own from the material)
    plain = C.material_cmds
    # TEXEL0 x PRIMITIVE, then x SHADE in the second cycle; alpha 1 (the castle's materials leave PRIMITIVE out)
    def cc(a, b, c, d):
        return a, b, c, d
    a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0 = 1, 15, 3, 7, 7, 7, 7, 6
    a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1 = 0, 15, 4, 7, 7, 7, 7, 6
    w0 = (0xFC << 24) | (a0 << 20) | (c0 << 15) | (Aa0 << 12) | (Ac0 << 9) | (a1 << 5) | c1
    w1 = (b0 << 28) | (b1 << 24) | (Aa1 << 21) | (Ac1 << 18) | (d0 << 15) | (Ab0 << 12) | (Ad0 << 9) | (d1 << 6) | \
         (Ab1 << 3) | Ad1
    combine = __import__('struct').pack('<II', w0, w1)
    C.material_cmds = lambda t: [c for c in plain(t) if c[3] not in (0xFA, 0xFC)] + [combine]
    emit_obj(glass, 'gGdfMirrorGlass', files)
    C.material_cmds = plain
    # The Mirror Room: in from the broken door (facing north), or back out of the mirror (facing south)
    beams = []
    room = build_mirror_room(beams)
    scene(files, 'sasatest', room,
          [G.actor_entry(0, (0, 0, 560), (0, -0x8000, 0), 0x0FFF),
           G.actor_entry(0, (0, 24, -230), (0, 0, 0), 0x0FFF)],
          [(0, 0), (1, 0)], exits=(), beams=beams)
    # The first hall: out of its mirror on the south wall, facing north
    beams = []
    hall = build_first_hall(beams)
    scene(files, 'syotes', hall,
          [G.actor_entry(0, (0, 0, 180), (0, -0x8000, 0), 0x0FFF)],
          [(0, 0)], exits=(), beams=beams)
    # The great door and its plates (glowing parts tinted by the actor, like the mirror's glass)
    door, tris, plate, rim = build_great_door()
    emit_obj(door, 'gGdfGreatDoor', files)
    files['objects/gdf_sacred/gGdfGreatDoorCol'] = G.collision_file(door.col_verts, door.col_polys, C.SURFACES,
                                                                    cam_settings=(1,))
    emit_obj(rim, 'gGdfTriPlateRim', files)
    C.material_cmds = lambda t: [c for c in plain(t) if c[3] not in (0xFA, 0xFC)] + [combine]
    for i, t in enumerate(tris):
        emit_obj(t, f'gGdfGreatDoorTri{i}', files)
    emit_obj(plate, 'gGdfTriPlate', files)
    C.material_cmds = plain
    # The Sanctum of Light: in from the first hall (facing north)
    beams = []
    scene(files, 'syotes2', build_hub(beams),
          [G.actor_entry(0, (0, 0, HUB_A + 60), (0, -0x8000, 0), 0x0FFF)],
          [(0, 0)], exits=(), beams=beams)
    return files


if __name__ == '__main__':
    files = emit()
    out = sys.argv[1] if len(sys.argv) > 1 else 'gdf_sacred.o2r'
    G.pack(files, out)
    for k, v in files.items():
        print(len(v), k)
