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

Builder = C.Builder
SURF = C.SURF
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


# ---- The Mirror Room (SCENE_SASATEST) ----

def build_mirror_room():
    B = Builder()
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

def build_first_hall():
    B = Builder()
    H = 520
    B.floor('marble_check', -320, 320, -1100, 300, y=0)
    B.floor('carpet_red', -70, 70, -1000, 220, y=1)
    B.wall('brick_white2', 'x', -320, -1100, 300, 0, H, 1)
    B.wall('brick_white2', 'x', 320, -1100, 300, 0, H, -1)
    B.wall('brick_white2', 'z', 300, -320, 320, 0, H, -1)
    B.wall('brick_white2', 'z', -1100, -320, 320, 0, H, 1)
    B.ceiling('ceiling', -320, 320, -1100, 300, H)
    for zc in (100, -200, -500, -800):
        for x in (-220, 220):
            B.pillar(x, zc, 30, 0, H)
    # The great sealed door at the far end (opened in a later stage)
    B.box(('marble2', 'marble2'), -150, 150, 0, 340, -1100, -1080)
    B.box('gold', -160, 160, 340, 360, -1100, -1074, color=GOLD)
    for cx, cy in ((0, 300), (-40, 230), (40, 230)):
        B.quad('gold', [(cx - 40, cy - 70, -1079), (cx + 40, cy - 70, -1079), (cx, cy, -1079)], color=GOLD)
    return B


LIGHTING = [(90, 85, 100, 60, 100, 40, -1, -6, -6, -74, -100, 21, 70, 70, 90, 30, 30, 50, 2020, 12800),
            (110, 105, 115, 78, 98, -17, -1, -1, -16, -82, -20, -94, 80, 80, 100, 60, 60, 80, 2020, 12800)]


def scene(files, name, B, spawns, entrances, exits):
    base = f'scenes/shared/{name}_scene/'
    C.emit_dl(B, base + f'{name}_room_0DL_main', base + f'{name}_room_0Vtx_main', files)
    col = base + f'{name}_sceneCollisionHeader_main'
    files[col] = G.collision_file(B.col_verts, B.col_polys, C.SURFACES, cam_settings=(1, 1))
    room = base + f'{name}_room_0'
    files[room] = G.room_file([(base + f'{name}_room_0DL_main', '')], objects=(), actors=(), echo=12,
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
    C.material_cmds = lambda t: [c for c in plain(t) if c[3] != 0xFA]
    emit_obj(glass, 'gGdfMirrorGlass', files)
    C.material_cmds = plain
    # The Mirror Room: in from the broken door (facing north), or back out of the mirror (facing south)
    scene(files, 'sasatest', build_mirror_room(),
          [G.actor_entry(0, (0, 0, 560), (0, -0x8000, 0), 0x0FFF),
           G.actor_entry(0, (0, 24, -230), (0, 0, 0), 0x0FFF)],
          [(0, 0), (1, 0)], exits=())
    # The first hall: out of its mirror on the south wall, facing north
    scene(files, 'syotes', build_first_hall(),
          [G.actor_entry(0, (0, 0, 180), (0, -0x8000, 0), 0x0FFF)],
          [(0, 0)], exits=())
    return files


if __name__ == '__main__':
    files = emit()
    out = sys.argv[1] if len(sys.argv) > 1 else 'gdf_sacred.o2r'
    G.pack(files, out)
    for k, v in files.items():
        print(len(v), k)
