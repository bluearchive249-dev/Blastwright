# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
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


class Builder(C.LitBuilder):
    pass


C.FLOOR = 0
C.MAT['mirror'] = C.tex('tokinoma_sceneTex_010FA0')   # the glass: plain marble, tinted by the actor each frame
C.MAT['gold'] = C.tex('nakaniwa_room_0Tex_013E18')     # gilded bands (the stone band, tinted gold)
C.OBJ_PRIM['mirror'] = None                            # (the actor sets its colour)

GOLD = (255, 205, 110)
IRON = (70, 64, 60)
DARK = (28, 24, 30)


def emit_obj(B, name, files):
    C.emit_dl(B, f'objects/gdf_sacred/{name}DL', f'objects/gdf_sacred/{name}Vtx', files, obj=True)


# ---- The sealed door (front +z, its foot at the origin; the Temple of Time's wall PORCH behind it) ----
PORCH = 140  # (sPorch in GanondorfSacred.cpp)

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
    # The porch: the door stands PORCH out from the Temple of Time's wall (its own wall can't be cut), and behind the
    # slab a passage runs back to the wall, its far end lost in darkness: what the bars look into, and once the door
    # is broken in, the way he walks into the dark
    D = PORCH
    for x0, x1 in ((-112, -75), (75, 112)):
        frame.box(('brick_white', 'brick_white'), x0, x1, 0, 282, -D, 0, collide=False)  # (inner sides: the passage's walls)
    frame.box(('brick_white', 'brick_white'), -112, 112, 282, 290, -D, 0, collide=False)  # its roof
    frame.fade_to_black((0, 0, 0), (0, 0, -D + 2), (-76, 76, -1, 283, -D, 1))
    frame.quad('marble_check', [(-74, 0.5, 0), (74, 0.5, 0), (74, 0.5, -D + 1), (-74, 0.5, -D + 1)], color=(150, 140, 135),
               light=False)
    frame.quad('ceiling', [(-74, 250, -D + 1), (74, 250, -D + 1), (74, 250, 0), (-74, 250, 0)], color=(120, 115, 115),
               light=False)
    frame.quad('ceiling', [(-76, 0, -D + 1), (76, 0, -D + 1), (76, 252, -D + 1), (-76, 252, -D + 1)], color=(0, 0, 0),
               light=False)
    # Broken open: only the threshold where the slab stood
    recess.quad('marble_check', [(-75, 1, 12), (75, 1, 12), (75, 1, 0), (-75, 1, 0)], color=(110, 100, 95))
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


window = C.window


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
    # The passage from the broken door: south, z 300..620 (the door is at its far end), fading to black
    B.fade_to_black((0, 0, 400), (0, 0, 620), (-90, 90, 0, 260, 300, 620))
    B.floor('marble_check', -90, 90, 300, 620, y=0)
    B.wall('brick_white', 'x', -90, 300, 620, 0, 260, 1)
    B.wall('brick_white', 'x', 90, 300, 620, 0, 260, -1)
    B.wall('brick_white', 'z', 620, -90, 90, 0, 260, -1)  # (the way out: walking up to it)
    B.ceiling('ceiling', -90, 90, 300, 620, 260)
    C.room_finish(B, -90, 90, 300, 620, 0, 260, walls='WE', pilasters=False, dado=(214, 208, 198))
    # The hall of the Sages
    B.floor('marble_check', -340, 340, -360, 300, y=0)
    B.floor('carpet_red', -60, 60, -250, 300, y=1, surface='floor')
    B.wall('brick_white', 'x', -340, -360, 300, 0, H, 1)
    B.wall('brick_white', 'x', 340, -360, 300, 0, H, -1)
    B.wall('brick_white', 'z', -360, -340, 340, 0, H, 1)
    B.wall('brick_white', 'z', 300, -340, 340, 0, H, -1, openings=[(-90, 90, 0, 260)])
    C.cove_ceiling(B, -340, 340, -360, 300, H, c=60)
    C.room_finish(B, -340, 340, -360, 300, 0, H, pilasters=False, dado=(214, 208, 198),
                  doors={'S': [(-90, 90, 0, 260)], 'W': [(-160, -60, 210, 400)], 'E': [(-160, -60, 210, 400)]})
    C.arch_door(B, 'z', 300, -90, 90, 0, 260)
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
    # Behind the great door: a short dark passage to the Sanctum (walking into it leads on), fading to black
    B.fade_to_black((0, 0, -1100), (0, 0, -1260), (-150, 150, 0, 340, -1260, -1100))
    B.floor('marble_check', -150, 150, -1260, -1100, y=0)
    B.wall('brick_white2', 'x', -150, -1260, -1100, 0, 340, 1)
    B.wall('brick_white2', 'x', 150, -1260, -1100, 0, 340, -1)
    B.wall('brick_white2', 'z', -1260, -150, 150, 0, 340, 1)
    B.ceiling('ceiling', -150, 150, -1260, -1100, 340)
    C.cove_ceiling(B, -320, 320, -1100, 300, H, c=70)
    C.room_finish(B, -320, 320, -1100, 300, 0, H, pilasters=False, dado=(214, 208, 198),
                  doors={'N': [(-150, 150, 0, 340)], 'W': [(z - 60, z + 60, 320, 560) for z in (-50, -350, -650)],
                         'E': [(z - 60, z + 60, 320, 560) for z in (-50, -350, -650)]},
                  skip=[('S', -140, 140)])
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
    plate.quad('mirror', [(-60, 4, 45), (60, 4, 45), (0, 4, -60)], color=(255, 255, 255), light=False)
    rim = Builder()
    for (ax, az), (bx, bz) in (((-60, 45), (60, 45)), ((60, 45), (0, -60)), ((0, -60), (-60, 45))):
        dx, dz = bx - ax, bz - az
        n = (dz / np.hypot(dx, dz) * 8, -dx / np.hypot(dx, dz) * 8)
        rim.quad('gold', [(ax, 3.5, az), (bx, 3.5, bz), (bx + n[0], 3.5, bz + n[1]), (ax + n[0], 3.5, az + n[1])],
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
    for mx, mz, ix, iz in diag:  # the Temple of Time's windows on the diagonal walls, where the beams fall from
        C.tot_window(B, (ix, iz), (mx, mz), 96 * HUB_BEAM, 320 + 100 * HUB_BEAM, 320 + 661 * HUB_BEAM)
    E = HUB_A + 40
    B.floor('marble_check', -E, E, -E, E, y=0)
    B.floor('carpet_red', -70, 70, 140, HUB_A, y=1)
    B.ceiling('ceiling', -E, E, -E, E, H)
    door = (-110, 110, 0, 300)
    B.wall('brick_white2', 'z', HUB_A, -HUB_S, HUB_S, 0, H, -1, openings=[door])     # south: the way back
    B.wall('brick_white2', 'z', -HUB_A, -HUB_S, HUB_S, 0, H, 1, openings=[(-160, 160, 0, 380)])  # north: Rauru
    B.wall('brick_white2', 'x', HUB_A, -HUB_S, HUB_S, 0, H, -1, openings=[(-110, 110, 0, 300)])  # east wing
    B.wall('brick_white2', 'x', -HUB_A, -HUB_S, HUB_S, 0, H, 1, openings=[(-110, 110, 0, 300)])  # west wing
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
    # The way back: a short passage south, fading to black
    B.fade_to_black((0, 0, HUB_A), (0, 0, HUB_A + 160), (-110, 110, 0, 300, HUB_A, HUB_A + 160))
    B.floor('marble_check', -110, 110, HUB_A, HUB_A + 160, y=0)
    B.wall('brick_white2', 'x', -110, HUB_A, HUB_A + 160, 0, 300, 1)
    B.wall('brick_white2', 'x', 110, HUB_A, HUB_A + 160, 0, 300, -1)
    B.wall('brick_white2', 'z', HUB_A + 160, -110, 110, 0, 300, -1)
    B.ceiling('ceiling', -110, 110, HUB_A, HUB_A + 160, 300)
    C.room_finish(B, -110, 110, HUB_A, HUB_A + 160, 0, 300, walls='WE', pilasters=False, dado=(214, 208, 198))
    C.arch_door(B, 'z', HUB_A, -110, 110, 0, 300)
    # The ways on: north to Rauru (its gilded gate is an actor: it rises once both wings' seals are his), east and
    # west to the wings. Each a short passage into the dark.
    B.box('gold', -176, 176, 380, 404, -HUB_A - 2, -HUB_A + 26, collide=False, color=GOLD)
    passage(B, 'N', -HUB_A, 160, 380, 170)
    passage(B, 'E', HUB_A, 110, 300, 170)
    passage(B, 'W', -HUB_A, 110, 300, 170)
    C.arch_door(B, 'x', HUB_A, -110, 110, 0, 300)
    C.arch_door(B, 'x', -HUB_A, -110, 110, 0, 300)
    # The ring of pillars and the dais
    for k in range(8):
        a = np.pi / 8 + k * np.pi / 4
        B.pillar(380 * np.cos(a), 380 * np.sin(a), 30, 0, H)
    B.box(('marble', 'stone_band'), -150, 150, 0, 14, -150, 150)
    B.box(('marble', 'stone_band'), -100, 100, 14, 28, -100, 100)
    for cx, cz in ((0, -40), (-35, 20), (35, 20)):
        B.quad('gold', [(cx - 34, 28.5, cz + 30), (cx + 34, 28.5, cz + 30), (cx, 28.5, cz - 30)], color=GOLD)
    return B


def passage(B, d, at, half, h, length, fade=True):
    """A short passage out of a room, fading into the dark: d is the way it leads (N -z, S +z, E +x, W -x), at the
    wall it leaves through, half its half-width, h its height."""
    s = {'N': -1, 'S': 1, 'E': 1, 'W': -1}[d]
    a, b = at, at + s * length
    lo, hi = min(a, b), max(a, b)
    if d in 'NS':
        if fade:
            B.fade_to_black((0, 0, a), (0, 0, b), (-half, half, 0, h, lo, hi))
        B.floor('marble_check', -half, half, lo, hi, y=0)
        B.wall('brick_white2', 'x', -half, lo, hi, 0, h, 1)
        B.wall('brick_white2', 'x', half, lo, hi, 0, h, -1)
        B.wall('brick_white2', 'z', b, -half, half, 0, h, -s)
        B.ceiling('ceiling', -half, half, lo, hi, h)
        C.room_finish(B, -half, half, lo, hi, 0, h, walls='WE', pilasters=False, dado=(214, 208, 198))
    else:
        if fade:
            B.fade_to_black((a, 0, 0), (b, 0, 0), (lo, hi, 0, h, -half, half))
        B.floor('marble_check', lo, hi, -half, half, y=0)
        B.wall('brick_white2', 'z', -half, lo, hi, 0, h, 1)
        B.wall('brick_white2', 'z', half, lo, hi, 0, h, -1)
        B.wall('brick_white2', 'x', b, -half, half, 0, h, -s)
        B.ceiling('ceiling', lo, hi, -half, half, h)
        C.room_finish(B, lo, hi, -half, half, 0, h, walls='NS', pilasters=False, dado=(214, 208, 198))


def long_hall(beams, x0, x1, z0, z1, H, beam, win_z, win_y0, extra_lights=(), door_s=(-110, 110, 0, 300)):
    """A Sacred Realm hall: marble and gold, Temple of Time windows down both sides, its way in at the south end."""
    lights = [((0, H - 80, z), 420, 0.3, WARM) for z in np.linspace(z1 - 150, z0 + 150, 4)]
    lights += [((0, 70, z), 340, 0.4, WARM) for z in np.linspace(z1 - 150, z0 + 150, 4)]
    lights += list(extra_lights)
    spots = [(wall, x, z) for z in win_z for wall, x in (('W', x0), ('E', x1))]
    for wall, x, z in spots:
        window(None, lights, beams, wall, (x, z), win_y0, beam, frame=False)
    B = Builder(lights, float(H))
    for wall, x, z in spots:
        window(B, [], [], wall, (x, z), win_y0, beam)
    B.wall('brick_white2', 'x', x0, z0, z1, 0, H, 1)
    B.wall('brick_white2', 'x', x1, z0, z1, 0, H, -1)
    B.wall('brick_white2', 'z', z1, x0, x1, 0, H, -1, openings=[door_s])
    B.wall('brick_white2', 'z', z0, x0, x1, 0, H, 1)
    C.cove_ceiling(B, x0, x1, z0, z1, H, c=70)
    wins = [(z - 60, z + 60, win_y0 + 20, H) for z in win_z]
    C.room_finish(B, x0, x1, z0, z1, 0, H, pilasters=False, dado=(214, 208, 198),
                  doors={'S': [door_s], 'W': wins, 'E': wins})
    C.arch_door(B, 'z', z1, door_s[0], door_s[1], door_s[2], door_s[3])
    passage(B, 'S', z1, door_s[1], door_s[3], 170)
    return B


# ---- The east wing: the Hall of Spears (SCENE_SUTARU) ----
# Spear and shield guards; four braziers his blows (or magic) set alight; the Seal of the East on an altar at the
# north end, given once the guards are down and all four burn.
WING_BEAM = 0.3   # (the same in GanondorfSacred.cpp)
EAST = dict(x0=-320, x1=320, z0=-1300, z1=600, H=560)


def build_east(beams):
    e = EAST
    B = long_hall(beams, e['x0'], e['x1'], e['z0'], e['z1'], e['H'], WING_BEAM, (300, -100, -500, -900), 300,
                  extra_lights=[((0, 220, -1180), 420, 0.7, (1.0, 0.85, 0.5))] +
                               [((x, 160, z), 260, 0.35, (1.0, 0.7, 0.4)) for x in (-170, 170) for z in (-350, -750)])
    B.floor('marble_check', e['x0'], e['x1'], e['z0'], e['z1'], y=0)
    C.rug(B, -70, 70, -1100, 520, 0, mat='carpet_red')
    for z in (400, 100, -200, -500, -800, -1100):
        for x in (-230, 230):
            B.pillar(x, z, 28, 0, e['H'])
    # the altar's dais
    B.box(('marble', 'stone_band'), -130, 130, 0, 14, -1290, -1080)
    B.box(('marble', 'stone_band'), -90, 90, 14, 28, -1260, -1110)
    for x in (-200, 200):
        C.banner(B, 'N', x, e['z0'], 470, w=80, h=280, mat='carpet_red')
    C.statue(B, -120, -1230, 28, face='S')
    C.statue(B, 120, -1230, 28, face='S')
    return B


# ---- The west wing: the Hall of Bows (SCENE_HAIRAL_NIWA2) ----
# A chasm across the middle (he floats over it, or falls and is put back), archers on balconies above it, sword
# guards on the floor, and the Guard Captain beyond; the Seal of the West on the altar at the north end.
WEST = dict(x0=-360, x1=360, z0=-1500, z1=600, H=600, gap0=-650, gap1=-350, pit=-420, balc=220)


def build_west(beams):
    w = WEST
    x0, x1, z0, z1, H = w['x0'], w['x1'], w['z0'], w['z1'], w['H']
    B = long_hall(beams, x0, x1, z0, z1, H, WING_BEAM, (300, 0, -1000, -1300), 320,
                  extra_lights=[((0, 220, -1380), 420, 0.7, (1.0, 0.85, 0.5)),
                                ((0, w['pit'] + 80, -500), 500, 0.5, (0.55, 0.6, 1.0))])
    g0, g1, pit = w['gap0'], w['gap1'], w['pit']
    B.floor('marble_check', x0, x1, g1, z1, y=0)
    B.floor('marble_check', x0, x1, z0, g0, y=0)
    # the chasm: its sheer sides, the far-down floor (blue-dark), and walls down to it
    B.floor('tile_court', x0, x1, g0, g1, y=pit, color=(70, 80, 120))
    B.wall('brick_white2', 'z', g1, x0, x1, pit, 0, -1)
    B.wall('brick_white2', 'z', g0, x0, x1, pit, 0, 1)
    B.wall('brick_white2', 'x', x0, g0, g1, pit, 0, 1)
    B.wall('brick_white2', 'x', x1, g0, g1, pit, 0, -1)
    for zz in (g1, g0):  # gilded lips
        B.box('gold', x0, x1, -6, 2, zz - 6, zz + 6, collide=False, color=GOLD)
    C.rug(B, -70, 70, -60, 520, 0, mat='carpet_blue')
    C.rug(B, -70, 70, -1420, -760, 0, mat='carpet_blue')
    # the balconies, either side above the chasm, with balustrades
    for s in (-1, 1):
        bx0, bx1 = (x1 - 130, x1) if s > 0 else (x0, x0 + 130)
        B.box(('marble', 'marble2'), bx0, bx1, w['balc'] - 24, w['balc'], -900, -100)
        inner = bx0 if s > 0 else bx1
        for zz in range(-880, -100, 60):
            C.prism(B, 'marble', inner + (-6 if s > 0 else 6), zz, 6, w['balc'], w['balc'] + 60, 6)
        B.box('marble2', inner - 8 * (s > 0), inner + 8 * (s < 0), w['balc'] + 60, w['balc'] + 70, -900, -100,
              collide=False)
        for zz in (-860, -140):  # corbels under them
            B.box('stone_band', min(inner, x1 if s > 0 else x0), max(inner, x1 if s > 0 else x0), w['balc'] - 70,
                  w['balc'] - 24, zz - 20, zz + 20, collide=False, color=(225, 215, 195))
    for z in (400, 150, -850, -1150):
        for x in (-260, 260):
            B.pillar(x, z, 28, 0, H)
    B.box(('marble', 'stone_band'), -130, 130, 0, 14, -1490, -1280)
    B.box(('marble', 'stone_band'), -90, 90, 14, 28, -1460, -1310)
    for x in (-220, 220):
        C.banner(B, 'N', x, z0, 500, w=80, h=280, mat='carpet_blue')
    return B


# ---- Rauru's arena (SCENE_BESITU): the Chamber of Sages under the Temple of Time's roof. An octagon 1300 across the
# flats (room for the light he'll throw about), the Temple of Time's marble and windows, the Chamber's six raised Sage
# platforms in a ring, each with its medallion and a column of its colour, round a raised Triforce dais; a starry
# blue vault over all. Rauru fights here ----
RAURU_A, RAURU_BEAM = 1300, 0.6   # (the beam scale is the same in GanondorfSacred.cpp; RAURU_A in GanondorfSacredTemple.cpp)
RAURU_S = int(RAURU_A * np.tan(np.pi / 8))


def octagon(B, A, S, H, mat='brick_white2', south_door=None):
    B.wall(mat, 'z', A, -S, S, 0, H, -1, openings=[south_door] if south_door else [])
    B.wall(mat, 'z', -A, -S, S, 0, H, 1)
    B.wall(mat, 'x', A, -S, S, 0, H, -1)
    B.wall(mat, 'x', -A, -S, S, 0, H, 1)
    for sx, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        a, b = (sx * S, sz * A), (sx * A, sz * S)
        for y0 in range(0, H, 500):
            y1 = min(H, y0 + 500)
            p = [(a[0], y0, a[1]), (b[0], y0, b[1]), (b[0], y1, b[1]), (a[0], y1, a[1])]
            n = np.cross(np.subtract(p[1], p[0]), np.subtract(p[3], p[0]))
            if np.dot(n, [-(a[0] + b[0]), 0, -(a[1] + b[1])]) < 0:
                p = [p[1], p[0], p[3], p[2]]
            B.quad(mat, p, collide=SURF['wall'])


def floor_medal(B, mat, x, z, r, y, color=(255, 255, 255)):
    """A Sage's medallion laid in a floor (the textures are half discs: the top half, then mirrored)."""
    B.quad(mat, [(x - r, y, z), (x + r, y, z), (x + r, y, z - r), (x - r, y, z - r)], uv=[(0, 32), (64, 32), (64, 0), (0, 0)],
           color=color, light=False)
    B.quad(mat, [(x - r, y, z + r), (x + r, y, z + r), (x + r, y, z), (x - r, y, z)], uv=[(0, 0), (64, 0), (64, 32), (0, 32)],
           color=color, light=False)


SAGE_MEDALS = ['medal_forest', 'medal_fire', 'medal_water', 'medal_shadow', 'medal_spirit', 'medal_light']


def build_rauru(beams):
    A, S, H = RAURU_A, RAURU_S, 1400
    WY = 520   # the windows' beams start here
    lights = [((0, 160, 0), 700, 0.7, (1.0, 0.95, 0.75)), ((0, 1100, 0), 1400, 0.3, WARM)]
    wins = []
    for sx, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        mx, mz = sx * (A + S) / 2, sz * (A + S) / 2
        wins.append((mx, mz, -sx / np.sqrt(2), -sz / np.sqrt(2)))
    for mx, mz in ((A, 0), (-A, 0), (0, -A)):
        n = np.hypot(mx, mz)
        wins.append((mx, mz, -mx / n, -mz / n))
    for mx, mz, ix, iz in wins:
        yaw = int(np.degrees(np.arctan2(ix, iz)) / 360 * 65536)
        o = (mx + ix * 68 * RAURU_BEAM, WY, mz + iz * 68 * RAURU_BEAM)
        beams.append((o, yaw))
        reach = 825 * RAURU_BEAM * min(1.0, WY / (939 * RAURU_BEAM))
        lights.append(((mx + ix * 30, WY + 200, mz + iz * 30), 420, 0.5, WARM))
        lights.append(((o[0] + ix * reach, 30, o[2] + iz * reach), 340, 0.7, (1.0, 0.95, 0.8)))
    sage = []
    for k in range(6):
        a = np.pi / 2 + np.pi / 6 + k * np.pi / 3   # (none in the way in, from the south)
        sage.append((880 * np.cos(a), 880 * np.sin(a), SAGE_TINTS[k], k))
        lights.append(((880 * np.cos(a), 260, 880 * np.sin(a)), 520, 0.9, SAGE_TINTS[k]))
    B = Builder(lights, float(H))
    for mx, mz, ix, iz in wins:
        C.tot_window(B, (ix, iz), (mx, mz), 96 * RAURU_BEAM, WY + 100 * RAURU_BEAM, WY + 661 * RAURU_BEAM)
    E = A + 40
    B.floor('marble_check', -E, E, -E, E, y=0)
    door = (-110, 110, 0, 300)
    octagon(B, A, S, H, south_door=door)
    C.arch_door(B, 'z', A, -110, 110, 0, 300)
    passage(B, 'S', A, 110, 300, 170)
    C.cove_ceiling(B, -A, A, -A, A, H, c=120, color=(55, 62, 130), band=GOLD)
    for i in range(70):   # gilded stars on the vault
        sx = -A + 200 + (i * 337) % (2 * A - 400)
        sz = -A + 200 + (i * 211) % (2 * A - 400)
        C.disc(B, 'gold', sx, sz, 9, H + 118, 5, color=GOLD, up=False, light=False)
    # the six Sages' platforms (the Chamber of Sages' own, raised in a ring), each with its medallion and a column of
    # its colour rising to the vault
    for x, z, t, k in sage:
        col = tuple(int(255 * c) for c in t)
        C.prism(B, 'marble', x, z, 170, 0, 30, 16, top='marble', collide=True, color=(240, 238, 246))
        C.annulus(B, 'gold', x, z, 152, 168, 30.5, 24, color=col)
        floor_medal(B, SAGE_MEDALS[k], x, z, 110, 31.5)
        C.prism(B, 'gold', x, z, 16, 30, H, 8, color=tuple(min(255, int(c * 1.2) + 40) for c in col))
    # the Triforce dais (Rauru hovers above it): two steps, a ring of gold, a great Triforce inlay
    C.prism(B, 'marble', 0, 0, 330, 0, 8, 24, top='marble', collide=True)
    C.prism(B, 'marble', 0, 0, 230, 8, 16, 24, top='marble', collide=True)
    C.annulus(B, 'gold', 0, 0, 210, 226, 16.5, 32, color=GOLD)
    for cx, cz in ((0, -70), (-61, 35), (61, 35)):
        B.quad('gold', [(cx - 60, 17, cz + 52), (cx + 60, 17, cz + 52), (cx, 17, cz - 52)], color=GOLD)
    # a carpet from the door to the dais, and candelabras round the walls
    B.floor('carpet_red', -70, 70, 330, A, y=1)
    for k in range(12):
        a = (k + 0.5) * np.pi / 6
        C.candelabra(B, lambda *args: None, (A - 120) * np.cos(a), (A - 120) * np.sin(a), 0)
    return B


# ---- The Goddesses' Spring (SCENE_DEPTH_TEST): above the Chamber of Sages ----
# From the portal in the chamber: a long hall where a path of water climbs to the Triforce's sanctum, an octagon where
# the three Golden Goddesses wait as figures of light round the Triforce's dais.
SPRING_BEAM = 0.3      # (the same in GanondorfSacredTemple.cpp)
SPRING_A, SPRING_S, SPRING_H = 600, 250, 900
SPRING_X = 240                       # the hall's half-width
SPRING_TOP, SPRING_FOOT = 760, 1960  # the climb, from the sanctum's door down to the hall's foot
SPRING_LOW = -480                    # the hall's foot (the sanctum's floor is at 0)
SPRING_END = 2200                    # the hall's south wall (the way back to the chamber)
GODDESS = [(0, -400, (1.0, 0.35, 0.25)), (-346, 200, (0.35, 0.55, 1.0)), (346, 200, (0.35, 1.0, 0.45))]
WATER = (175, 215, 255)


def spring_y(z):
    """The water path's height at z."""
    t = min(1.0, max(0.0, (z - SPRING_TOP) / (SPRING_FOOT - SPRING_TOP)))
    return SPRING_LOW * t


def build_spring(beams):
    A, S, H = SPRING_A, SPRING_S, SPRING_H
    X, LOW, END = SPRING_X, SPRING_LOW, SPRING_END
    lights = [((0, 160, 0), 560, 0.8, (1.0, 0.95, 0.75)), ((0, 780, 0), 700, 0.3, WARM)]
    # the sanctum's windows: on its four diagonal walls, their sunbeams falling on the dais
    diag = []
    for sx, sz in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        mx, mz = sx * (A + S) / 2, sz * (A + S) / 2
        diag.append((mx, mz, -sx / np.sqrt(2), -sz / np.sqrt(2)))
    for mx, mz, ix, iz in diag:
        yaw = int(np.degrees(np.arctan2(ix, iz)) / 360 * 65536)
        o = (mx + ix * 68 * SPRING_BEAM, 440, mz + iz * 68 * SPRING_BEAM)
        beams.append((o, yaw))
        reach = 825 * SPRING_BEAM * min(1.0, 440 / (939 * SPRING_BEAM))
        lights.append(((mx + ix * 30, 540, mz + iz * 30), 340, 0.5, WARM))
        lights.append(((o[0] + ix * reach, 30, o[2] + iz * reach), 260, 0.7, (1.0, 0.95, 0.8)))
    for x, z, t in GODDESS:
        lights.append(((x * 0.9, 160, z * 0.9), 320, 0.9, t))
    # the hall's windows, down both sides above the climb
    hall_win = (1000, 1400, 1800)
    for z in hall_win:
        for wall, x in (('W', -X), ('E', X)):
            window(None, lights, beams, wall, (x, z), spring_y(z) + 300, SPRING_BEAM, frame=False, floor=spring_y(z))
    for z in np.linspace(SPRING_TOP, END - 100, 5):
        lights.append(((0, spring_y(z) + 220, z), 360, 0.35, (0.75, 0.88, 1.0)))   # the water's cool light
    B = Builder(lights, float(H))
    for z in hall_win:
        for wall, x in (('W', -X), ('E', X)):
            window(B, [], [], wall, (x, z), spring_y(z) + 300, SPRING_BEAM)
    for mx, mz, ix, iz in diag:
        C.tot_window(B, (ix, iz), (mx, mz), 96 * SPRING_BEAM, 440 + 100 * SPRING_BEAM, 440 + 661 * SPRING_BEAM)

    # -- the sanctum --
    E = A + 40
    B.floor('marble_check', -E, E, -E, E, y=0)
    B.ceiling('ceiling', -E, E, -E, E, H)
    door = (-110, 110, 0, 300)
    octagon(B, A, S, H, south_door=door)
    C.arch_door(B, 'z', A, -110, 110, 0, 300)
    # each Goddess's pillar behind her, banded in her colour, and a disc of her colour where she floats
    for x, z, t in GODDESS:
        col = tuple(int(255 * c) for c in t)
        r = np.hypot(x, z)
        px, pz = x / r * 530, z / r * 530
        B.pillar(px, pz, 40, 0, H)
        for y in (120, 520):
            B.box('gold', px - 44, px + 44, y, y + 18, pz - 44, pz + 44, collide=False, color=col)
        C.disc(B, 'gold', x, z, 64, 1.5, 16, color=col)
        C.annulus(B, 'gold', x, z, 64, 74, 1.6, 16, color=GOLD)
    # the dais (the Triforce comes down above it), ringed by a moat of still water, and the great Triforce inlay
    C.annulus(B, 'water', 0, 0, 205, 290, 1.0, 32, color=WATER)
    C.annulus(B, 'gold', 0, 0, 290, 302, 2.0, 32, color=GOLD)
    C.prism(B, 'marble', 0, 0, 200, 0, 16, 16, top='marble', collide=True)
    C.annulus(B, 'gold', 0, 0, 180, 196, 16.5, 32, color=GOLD)
    for cx, cz in ((0, -70), (-61, 35), (61, 35)):
        B.quad('gold', [(cx - 60, 17, cz + 52), (cx + 60, 17, cz + 52), (cx, 17, cz - 52)], color=GOLD)
    for k in range(8):
        a = k * np.pi / 4 + np.pi / 8
        if abs(np.cos(a) * 520) < 140 and np.sin(a) > 0:
            continue   # (none in the doorway)
        C.candelabra(B, lambda *args: None, 520 * np.cos(a), 520 * np.sin(a), 0)

    # -- the hall: a path of water climbing from its foot to the sanctum's door --
    z0, z1 = A, END
    hb = (-110, 110, 0, 300)
    B.wall('brick_white2', 'z', A, -X, X, 0, 640, 1, openings=[hb])     # the sanctum's wall, from the hall
    B.wall('brick_white2', 'x', -X, z0, z1, LOW, 640, 1)
    B.wall('brick_white2', 'x', X, z0, z1, LOW, 640, -1)
    B.wall('brick_white2', 'z', z1, -X, X, LOW, 640, -1, openings=[(-110, 110, LOW, LOW + 300)])
    C.cove_ceiling(B, -X, X, z0, z1, 640, c=60)
    wins = [(z - 60, z + 60, spring_y(z) + 310, spring_y(z) + 520) for z in hall_win]
    C.room_finish(B, -X, X, z0, z1, LOW, 640, pilasters=True, dado=(214, 208, 198),
                  doors={'S': [(-110, 110, LOW, LOW + 300)], 'N': [(-110, 110, LOW, 300)], 'W': wins, 'E': wins})
    C.arch_door(B, 'z', z1, -110, 110, LOW, LOW + 300)
    # the floor: level by the sanctum's door and at the foot, the climb between; water down the middle, marble either
    # side of it, edged in gold
    def strip(mat, xa, xb, color=(255, 255, 255), lift=0.0, collide=True):
        cs = SURF['floor'] if collide else None
        B.quad(mat, [(xa, lift, SPRING_TOP), (xb, lift, SPRING_TOP), (xb, lift, z0), (xa, lift, z0)], collide=cs,
               color=color)
        B.quad(mat, [(xa, LOW + lift, SPRING_FOOT), (xb, LOW + lift, SPRING_FOOT), (xb, lift, SPRING_TOP),
                     (xa, lift, SPRING_TOP)], collide=cs, color=color)
        B.quad(mat, [(xa, LOW + lift, z1), (xb, LOW + lift, z1), (xb, LOW + lift, SPRING_FOOT),
                     (xa, LOW + lift, SPRING_FOOT)], collide=cs, color=color)
    strip('water', -110, 110, color=WATER)
    strip('marble_check', -X, -122)
    strip('marble_check', 122, X)
    for s_ in (-1, 1):
        strip('gold', min(s_ * 110, s_ * 122), max(s_ * 110, s_ * 122), color=GOLD, lift=0.5)
    # its way out: a short passage back to the chamber, fading into the dark
    pz0, pz1 = z1, z1 + 170
    B.fade_to_black((0, LOW, pz0), (0, LOW, pz1), (-110, 110, LOW, LOW + 300, pz0, pz1))
    B.floor('water', -110, 110, pz0, pz1, y=LOW, color=WATER)
    B.wall('brick_white2', 'x', -110, pz0, pz1, LOW, LOW + 300, 1)
    B.wall('brick_white2', 'x', 110, pz0, pz1, LOW, LOW + 300, -1)
    B.wall('brick_white2', 'z', pz1, -110, 110, LOW, LOW + 300, -1)
    B.ceiling('ceiling', -110, 110, pz0, pz1, LOW + 300)
    # statues guarding the foot of the climb, braziers of light up its sides, and the Goddesses' banners
    C.statue(B, -175, SPRING_FOOT + 120, LOW, face='E')
    C.statue(B, 175, SPRING_FOOT + 120, LOW, face='W')
    for z in (900, 1200, 1500, 1800):
        for s_ in (-1, 1):
            C.candelabra(B, lambda *args: None, s_ * 190, z, spring_y(z), h=120)
    for i, z in enumerate((1200, 1600)):
        for j, (wall, wx) in enumerate((('W', -X), ('E', X))):
            t = GODDESS[(i * 2 + j) % 3][2]
            C.banner(B, wall, wx, z, spring_y(z) + 520, color=tuple(int(110 + 145 * c) for c in t))
    return B


# ---- Props drawn by actors (front +z, foot at the origin) ----

def build_props():
    P = {}
    br = Builder()   # a brazier: a fluted stand and a gilded bowl
    C.prism(br, 'marble', 0, 0, 22, 0, 10, 8, top='marble')
    C.prism(br, 'pillar', 0, 0, 10, 10, 90, 8)
    C.prism(br, 'gold', 0, 0, 14, 90, 100, 8, color=GOLD)
    C.prism(br, 'gold', 0, 0, 16, 100, 118, 12, r1=34, color=GOLD)
    C.disc(br, 'ceiling', 0, 0, 32, 116, 12, color=(40, 32, 30))
    P['gGdfBrazier'] = br
    al = Builder()   # the altar the seal waits on
    C.prism(al, 'marble2', 0, 0, 40, 0, 20, 8, top='marble2', collide=False)
    C.prism(al, 'marble', 0, 0, 22, 20, 90, 8)
    C.prism(al, 'gold', 0, 0, 34, 90, 104, 8, top='gold', color=GOLD)
    P['gGdfAltar'] = al
    # weapons for the guards (in the hand's frame: grip at the origin, along +x)
    sw = Builder()
    sw.box('gold', -14, 0, -3, 3, -3, 3, collide=False, color=(120, 80, 40))           # grip
    sw.box('gold', 0, 4, -14, 14, -4, 4, collide=False, color=GOLD)                     # guard
    sw.box('mirror', 4, 84, -4, 4, -1.2, 1.2, collide=False, color=(225, 230, 240))     # blade
    sw.quad('mirror', [(84, -4, 1.3), (96, 0, 1.3), (84, 4, 1.3)], color=(225, 230, 240))
    sw.quad('mirror', [(84, 4, -1.3), (96, 0, -1.3), (84, -4, -1.3)], color=(225, 230, 240))
    P['gGdfGuardSword'] = sw
    sh = Builder()   # a kite shield (front +z)
    pts = [(-26, 34), (26, 34), (26, -6), (0, -44), (-26, -6)]
    for i in (1, 2, 3):
        a, b, c = pts[0], pts[i], pts[i + 1]
        sh.quad('carpet_blue', [(a[0], a[1], 4), (b[0], b[1], 4), (c[0], c[1], 4)], color=(200, 210, 255))
        sh.quad('stone_band', [(c[0], c[1], -2), (b[0], b[1], -2), (a[0], a[1], -2)], color=(150, 140, 130))
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        sh.quad('gold', [(ax, ay, 4.5), (bx, by, 4.5), (bx, by, -2.5), (ax, ay, -2.5)], color=GOLD)
    for cx, cy in ((0, 16), (-9, 0), (9, 0)):
        sh.quad('gold', [(cx - 9, cy - 8, 5), (cx + 9, cy - 8, 5), (cx, cy + 8, 5)], color=GOLD)
    P['gGdfGuardShield'] = sh
    bw = Builder()   # a bow (in the left hand: the grip at the origin, limbs up and down along y)
    prev = None
    for i in range(13):
        t = -1 + i / 6
        p = (14 * (1 - t * t), 60 * t)
        if prev:
            bw.box('stone_band', min(prev[0], p[0]) - 2, max(prev[0], p[0]) + 2, min(prev[1], p[1]), max(prev[1], p[1]),
                   -2, 2, collide=False, color=(130, 90, 50))
        prev = p
    bw.box('mirror', -0.6, 0.6, -60, 60, -0.6, 0.6, collide=False, color=(240, 240, 230))
    P['gGdfGuardBow'] = bw
    # light: an orb (Rauru's bolts) and a glyph on the floor (where a pillar of light will fall), tinted by the actor
    orb = Builder()
    v = [(0, 1, 0), (1, 0, 0), (0, 0, 1), (-1, 0, 0), (0, 0, -1), (0, -1, 0)]
    for top in (0, 5):
        for i in range(4):
            a, b = v[1 + i], v[1 + (i + 1) % 4]
            tri = [tuple(16 * c for c in v[top]), tuple(16 * c for c in a), tuple(16 * c for c in b)]
            n = np.cross(np.subtract(tri[1], tri[0]), np.subtract(tri[2], tri[0]))
            if np.dot(n, np.mean(tri, 0)) < 0:
                tri = [tri[0], tri[2], tri[1]]
            orb.quad('mirror', tri, color=(255, 255, 255), light=False)
    P['gGdfLightOrb'] = orb
    gl = Builder()
    C.annulus(gl, 'mirror', 0, 0, 52, 64, 1.5, 24)
    for cx, cz in ((0, -20), (-17, 10), (17, 10)):
        gl.quad('mirror', [(cx - 17, 1.6, cz + 15), (cx + 17, 1.6, cz + 15), (cx, 1.6, cz - 15)], color=(255, 255, 255),
                light=False)
    P['gGdfGlyph'] = gl
    # the gate to Rauru (320 x 380, front +z) and its two seal gems (tinted by the actor)
    gt = Builder()
    gt.box(('marble2', 'marble2'), -160, 160, 0, 380, -10, 10)
    gt.box('gold', -160, 160, 0, 16, -12, 12, collide=False, color=GOLD)
    for cx, cy in ((0, 330), (-46, 250), (46, 250)):
        gt.quad('gold', [(cx - 44, cy - 76, 11), (cx + 44, cy - 76, 11), (cx, cy, 11)], color=GOLD)
    P['gGdfRauruGate'] = gt
    gems = []
    for x in (-100, 100):
        g = Builder()
        g.quad('mirror', [(x - 22, 120, 11.5), (x, 98, 11.5), (x + 22, 120, 11.5), (x, 142, 11.5)],
               color=(255, 255, 255), light=False)
        gems.append(g)
    return P, gems


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
    actors = [G.actor_entry(DEMO_KANKYO, b[0], (0, b[1], 0), LIGHT_PLANE) for b in beams]
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
    # The wings and Rauru's sanctuary: in from the south passage, facing north
    for name, build, z in (('sutaru', build_east, EAST['z1'] - 80), ('hairal_niwa2', build_west, WEST['z1'] - 80),
                           ('besitu', build_rauru, RAURU_A - 80)):
        beams = []
        scene(files, name, build(beams), [G.actor_entry(0, (0, 0, z), (0, -0x8000, 0), 0x0FFF)], [(0, 0)],
              exits=(), beams=beams)
    # The Goddesses' Spring: in from the chamber's portal, at the foot of the climb, facing north
    beams = []
    scene(files, 'depth_test', build_spring(beams),
          [G.actor_entry(0, (0, SPRING_LOW, SPRING_END - 80), (0, -0x8000, 0), 0x0FFF)], [(0, 0)], exits=(),
          beams=beams)
    # The props the actors draw (glowing parts tinted by the actor, as the mirror's glass)
    props, gems = build_props()
    glow = ('gGdfLightOrb', 'gGdfGlyph')
    for name, B in props.items():
        if name not in glow:
            emit_obj(B, name, files)
    C.material_cmds = lambda t: [c for c in plain(t) if c[3] not in (0xFA, 0xFC)] + [combine]
    for name in glow:
        emit_obj(props[name], name, files)
    for i, g in enumerate(gems):
        emit_obj(g, f'gGdfRauruGateGem{i}', files)
    C.material_cmds = plain
    return files


if __name__ == '__main__':
    files = emit()
    out = sys.argv[1] if len(sys.argv) > 1 else 'gdf_sacred.o2r'
    G.pack(files, out)
    for k, v in files.items():
        print(len(v), k)
