# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""gdf_pocket.o2r: the blank area the Map Editor's custom areas are made in (the unused SCENE_SASATEST slot).

It holds almost nothing: one room with no mesh, a small floor, one start point, four light settings. The game makes the
rest when the area loads, from the custom area's layout file (GanondorfMapEditor.cpp: its ground, walls, footstep
sound, light, fog and music). No game data goes in.

    python3 pocket.py out/gdf_pocket.o2r
"""
import os, sys, types

# (o2rgen only needs the path hash from otr.py, which otherwise opens the player's oot.o2r on import)
_otr = types.ModuleType('otr')
_POLY = 0x42F0E1EBA9EA3693
_T = []
for i in range(256):
    c = i << 56
    for _ in range(8):
        c = ((c << 1) ^ _POLY) if c & (1 << 63) else (c << 1)
    _T.append(c & 0xFFFFFFFFFFFFFFFF)


def crc64(s):
    c = 0xFFFFFFFFFFFFFFFF
    for b in s.encode():
        c = _T[((c >> 56) ^ b) & 0xFF] ^ ((c << 8) & 0xFFFFFFFFFFFFFFFF)
    return c & 0xFFFFFFFFFFFFFFFF


_otr.crc64 = crc64
sys.modules['otr'] = _otr
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gdf_castle'))
import o2rgen as G  # noqa: E402

SCENE = 'sasatest'
BASE = f'scenes/shared/{SCENE}_scene/'


def build():
    files = {}
    # A floor 400 across (replaced when the area loads)
    verts = [(-200, 0, -200), (200, 0, -200), (200, 0, 200), (-200, 0, 200)]
    polys = [(0, 0, 3, 2, 0, 0), (0, 0, 2, 1, 0, 0)]
    surfaces = [(0x00000002, 0x00000000)]
    col_path = BASE + f'{SCENE}_sceneCollisionHeader_pocket'
    files[col_path] = G.collision_file(verts, polys, surfaces, cam_settings=(1,))
    # The room: an empty display list for its one mesh
    dl = G.DL(BASE + f'{SCENE}_room_0DL_pocket')
    dl.end()
    files[dl.path] = dl.data()
    room_path = BASE + f'{SCENE}_room_0'
    files[room_path] = G.room_file([(dl.path, '')], objects=(), actors=(), echo=0, behavior=(0, 0), skymod=(1, 1))
    spawns = [G.actor_entry(0, (0, 0, 0), (0, 0, 0), 0x0FFF)]
    # One light setting (the game changes its colours and fog from the layout as the area loads)
    # (four alike: the game reads entries 0-3 by the time of day, and the Map Editor writes all four; POCKET_LIGHTS)
    lighting = [(80, 80, 80, 73, 73, 73, -1, -1, -16, -73, -73, -73, 60, 60, 80, 40, 40, 50, 2020, 12800)] * 4
    files[BASE + f'{SCENE}_scene'] = G.scene_file([room_path], col_path, spawns, [(0, 0)], exits=(),
                                                   lighting=lighting, sound=(0, 19, 0x7F), special=(0, 3),
                                                   sky=(0, 0, 0, 1), camera=(0, 0))
    return files


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'gdf_pocket.o2r'
    files = build()
    G.pack(files, out)
    for k, v in files.items():
        print(len(v), k)
