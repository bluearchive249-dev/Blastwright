# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""The Temple of Light, v2: built from the game's own rooms (the rule: if the base game has it, use it).

    python3 lighttemple2.py out/gdf_sacred.o2r [--header ../../soh/Enhancements/ExtraModes/GanondorfLightTempleData.h]

Stage 1 (this file): the Entrance Hall and the Sword Sanctum, the Temple of Time's two rooms used as they are:
  - each room's mesh is the game's own display lists, referenced by path (nothing copied)
  - the scene's collision is the game's own (the Temple of Time's, by path), so floors, walls and steps match the
    rooms exactly; the room change between them is the game's own En_Holl, where the game has it
  - the chests are the game's En_Box (small and big, the item-get animation); the locked doors are the game's
    dungeon door and boss door (drawn bigger by the runtime) with the game's chains and locks
  - the title card ("Temple of Light") is our own lettering, the game's size and format (titlecard.py)
Everything else the runtime needs (the props of the stages to come) is still made by lighttemple.py's prop kit.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lighttemple as L1    # (the prop kit, the lighting, the numbers the runtime shares)
import sacred as S
import o2rgen as G
import titlecard
import vcol

SCENE, BASE = L1.SCENE, L1.BASE
TOT = 'scenes/shared/tokinoma_scene/'
TOT_COL = TOT + 'tokinoma_sceneCollisionHeader_0032F8'
EN_BOX, OBJECT_BOX = 0x000A, 0x000E
ENTR_SASATEST_0 = 0x0018      # (the Mirror Room: where the hall's front door leads back to)
TITLE_TEX = 'textures/gdf_temple/gGdfTempleTitleCardTex'

# GetItem ids and En_Box types
GI = {'key': 0x42, 'boss': 0x3F, 'rupee20': 0x4E, 'rupee50': 0x55, 'rupee200': 0x56, 'heart': 0x48}
BOX_BIG, BOX_BIG_ORNATE, BOX_SMALL = 0, 2, 5

# Saved switch flags (0x00..0x1F): the key doors, then the boss door
FLAG_KEYDOOR0 = 0x10
FLAG_ASPECT = 0x18
FLAG_BOSSDOOR = 0x19

# Rooms, numbered as the Temple of Time numbers them (its collision and room change plane expect that)
#   id: (name, source room mesh (opa, xlu), floor y, respawn (x, y, z, yaw), kind)
ROOMS = {
    0: ('Sword Sanctum', (TOT + 'tokinoma_room_0DL_008128', ''), 0, (0, 0, -560, 0), 'quiet'),
    1: ('Entrance Hall', (TOT + 'tokinoma_room_1DL_0053E0', TOT + 'tokinoma_room_1DL_006470'), -40,
        (0, -40, 2480, -0x8000), 'quiet'),
}
ANTE = 0                 # (where the way to Rauru is, and where he comes back to)
RAURU_Z = -670           # (walking past this, through the open boss door, takes him to Rauru)
SPAWN = ((0, -40, 2480), -0x8000, 1)   # (from the Mirror Room: in the hall, facing up it)
# The game's own room-change plane between the two (the Temple of Time's, as it is)
TRANS = [(0, 0xFF, 1, 0xFF, 0x0023, 0, 0, 920, 0, 319)]

# Chests: room, position, facing, En_Box type, item, its treasure flag
CHESTS = [
    (1, (-300, -40, 1560), 0x4000, BOX_SMALL, 'key', 0),
    (1, (330, -40, 2160), -0x4000, BOX_SMALL, 'rupee50', 2),
    (0, (-400, 0, -300), 0x2600, BOX_BIG_ORNATE, 'boss', 1),
]
# Doors the runtime places (lighttemple's T_DOOR): room, position, facing, kind, flag
DOORS = [
    (1, (-4, 0, 1095), 0, L1.D_KEY, FLAG_KEYDOOR0),        # (the hall's far end, into the passage to the Sanctum)
    (0, (0, 0, -712), 0, L1.D_BOSS, FLAG_BOSSDOOR),        # (the Sanctum's back wall: the way to Rauru)
]


def check(V, P):
    """Every chest and door on the floor its room has there, and clear of the walls."""
    for room, pos, yaw, t, item, flag in CHESTS:
        y = vcol.floor_at(V, P, pos[0], pos[2], below=pos[1] + 30)
        assert y is not None and abs(y - pos[1]) < 2, ('chest off the floor', room, pos, y)
        for dx, dz in ((60, 0), (-60, 0), (0, 60), (0, -60)):
            y2 = vcol.floor_at(V, P, pos[0] + dx, pos[2] + dz, below=pos[1] + 30)
            assert y2 is not None and abs(y2 - pos[1]) < 12, ('chest by a ledge or a wall', room, pos)
    for room, pos, yaw, kind, flag in DOORS:
        y = vcol.floor_at(V, P, pos[0], pos[2], below=pos[1] + 30)
        assert y is not None and abs(y - pos[1]) < 2, ('door off the floor', room, pos, y)


def emit(files):
    V, P, _ = vcol.read(TOT_COL)
    check(V, P)
    for rid, (name, mesh, y, respawn, kind) in ROOMS.items():
        actors = []
        for room, pos, yaw, t, item, flag in CHESTS:
            if room == rid:
                actors.append(G.actor_entry(EN_BOX, pos, (0, yaw, 0), (t << 12) | (GI[item] << 5) | flag))
        files[BASE + f'{SCENE}_room_{rid}'] = G.room_file([mesh], objects=(OBJECT_BOX,), actors=actors, echo=12,
                                                          behavior=(0, 0), skymod=(0, 1))
    pos, yaw, room = SPAWN
    spawns = [G.actor_entry(0, pos, (0, yaw, 0), 0x0FFF)]
    lighting = [tuple(((v - 256) if v > 127 else v) if i < 18 else v for i, v in enumerate(l)) for l in L1.LIGHTING]
    files[BASE + f'{SCENE}_scene'] = G.scene_file([BASE + f'{SCENE}_room_{r}' for r in sorted(ROOMS)], TOT_COL,
                                                  spawns, [(0, room)], exits=(ENTR_SASATEST_0, ENTR_SASATEST_0),
                                                  lighting=lighting, sound=(0, 19, 0x56), special=(0, 3),
                                                  sky=(0, 0, 0, 1), camera=(0, 0), trans=TRANS)
    files[TITLE_TEX] = titlecard.resource(titlecard.render('Temple of Light'))


def emit_props(files):
    """The props the runtime draws (lighttemple.py's kit; the stages to come use them)."""
    PR, glow = L1.build_props()
    for name, B in PR.items():
        if not name.startswith('__col'):
            S.emit_obj(B, name, files)
    plain = L1.C.material_cmds
    combine = L1.glow_combine()
    L1.C.material_cmds = lambda t: [c for c in plain(t) if c[3] not in (0xFA, 0xFC)] + [combine]
    for name, B in glow.items():
        S.emit_obj(B, name, files)
    L1.C.material_cmds = plain
    for name in ('door', 'chest', 'bridge', 'lift', 'block'):
        B = PR['__col_' + name]
        files[f'objects/gdf_sacred/gGdfTCol_{name}'] = G.collision_file(B.col_verts, B.col_polys, L1.C.SURFACES,
                                                                         cam_settings=(1,))


def write_header(path):
    out = []
    out.append('// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI '
               'tools, without the author\'s written consent. See AI-POLICY.md.')
    out.append('// Generated by tools/gdf_castle/lighttemple2.py: the Temple of Light\'s rooms and what stands in them. '
               'Do not edit by hand.')
    out.append('#pragma once')
    out.append('')
    out.append('#define GDF_T_V2 1')
    out.append(f'#define GDF_T_ROOMS {len(ROOMS)}')
    out.append('#define GDF_T_ATRIUM -1')
    out.append(f'#define GDF_T_FLAG_ASPECT 0x{FLAG_ASPECT:02X}')
    out.append(f'#define GDF_T_FLAG_BOSSDOOR 0x{FLAG_BOSSDOOR:02X}')
    out.append(f'#define GDF_T_FLAG_KEYDOOR0 0x{FLAG_KEYDOOR0:02X}')
    out.append(f'#define GDF_T_KEYDOORS {sum(1 for d in DOORS if d[3] == L1.D_KEY)}')
    out.append(f'#define GDF_T_PLAT_R {L1.PLAT_R}.0f')
    out.append(f'#define GDF_T_ANTE {ANTE}')
    out.append(f'#define GDF_T_RAURU_Z {RAURU_Z}.0f')
    out.append(f'#define GDF_T_TITLE "__OTR__{TITLE_TEX}"')
    out.append('// (small keys and the boss key are the game\'s own, kept under this dungeon slot while in the temple)')
    out.append('#define GDF_T_KEY_SLOT 2')
    out.append('')
    out.append('typedef struct { s8 floor; s16 cx, cz, y; s16 rx, ry, rz, ryaw; s8 dark; s8 solveFlag; s8 kind; '
               'const char* name; } GdfTRoom;')
    out.append('static const GdfTRoom sGdfTRooms[GDF_T_ROOMS] = {')
    centres = {0: (0, 0), 1: (0, 1900)}
    for rid in sorted(ROOMS):
        name, mesh, y, (rx, ry, rz, ryaw), kind = ROOMS[rid]
        cx, cz = centres[rid]
        out.append(f'    {{ 0, {cx}, {cz}, {y}, {rx}, {ry}, {rz}, {ryaw}, 0, -1, {L1.KINDS.index(kind)}, "{name}" }},')
    out.append('};')
    out.append('')
    out.append('typedef struct { u8 room; u8 type; s16 x, y, z; s16 yaw; s16 p0, p1, p2; } GdfTSpawn;')
    out.append(f'#define GDF_T_SPAWNS {len(DOORS)}')
    out.append('static const GdfTSpawn sGdfTSpawns[GDF_T_SPAWNS] = {')
    for room, (x, y, z), yaw, kind, flag in DOORS:
        out.append(f'    {{ {room}, {L1.T_DOOR}, {x}, {y}, {z}, {yaw}, {kind}, {flag}, 0 }},')
    out.append('};')
    out.append('')
    out.append('typedef struct { u8 room; s16 x, y, z, yaw; s8 pair; s8 aspect; s8 kind; } GdfTMirror;')
    out.append('#define GDF_T_MIRRORS 0')
    out.append('static const GdfTMirror sGdfTMirrors[1] = { { 0, 0, 0, 0, 0, -1, -1, 0 } };')
    out.append('')
    out.append(f'#define GDF_T_CHESTS {len(CHESTS)}')
    out.append('')
    open(path, 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    out = sys.argv[1]
    files = S.emit()
    for k in list(files):
        if k.startswith(BASE):
            del files[k]
    emit_props(files)
    emit(files)
    G.pack(files, out)
    if '--header' in sys.argv:
        write_header(sys.argv[sys.argv.index('--header') + 1])
    print('rooms', len(ROOMS), 'chests', len(CHESTS), 'doors', len(DOORS))
