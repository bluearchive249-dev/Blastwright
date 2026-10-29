"""Writes Ship of Harkinian resources (the binary formats in soh/resource/importer) for scenes we generate, and packs
them into an .o2r for the mods folder. Only our own geometry goes in: textures are referenced by the hash of their
path in the player's oot.o2r, and the game's own display lists can be called the same way."""
import struct, zipfile, numpy as np
from otr import crc64

TYPE = {'room': b'MORO', 'col': b'LOCO', 'array': b'RRAO', 'dl': b'TLDO'}


def header(kind):
    return b'\0\0\0\0' + TYPE[kind] + b'\0\0\0\0' + b'\xef\xbe\xad\xde' * 2 + b'\0' * 0x2C


def s(text):
    b = text.encode()
    return struct.pack('<I', len(b)) + b


def hash_words(path):
    h = crc64(path)
    return struct.pack('<II', h >> 32, h & 0xFFFFFFFF)


# ---- Vertex arrays and display lists ----

def vtx_file(verts):
    """verts: (x, y, z, s, t, r, g, b, a) with s, t in texels"""
    out = header('array') + struct.pack('<II', 0x19, len(verts))
    for x, y, z, u, v, r, g, b, a in verts:
        out += struct.pack('<hhhHhhBBBB', int(round(x)), int(round(y)), int(round(z)), 0,
                           int(round(u * 32)) & 0xFFFF if False else max(-32768, min(32767, int(round(u * 32)))),
                           max(-32768, min(32767, int(round(v * 32)))), r, g, b, a)
    return out


class DL:
    def __init__(self, path):
        self.path = path
        self.cmds = [struct.pack('<II', 0x33000000, 0xBEEFBEEF) + hash_words(path)]

    def raw(self, b):
        self.cmds.append(b)

    def w(self, w0, w1):
        self.cmds.append(struct.pack('<II', w0 & 0xFFFFFFFF, w1 & 0xFFFFFFFF))

    def vtx(self, vtx_path, offset, n, v0=0):
        self.cmds.append(struct.pack('<II', (0x32 << 24) | (n << 12) | ((v0 + n) << 1), offset * 16) +
                         hash_words(vtx_path))

    def tri2(self, a, b, c, d, e, f):
        self.w((0x06 << 24) | (a * 2 << 16) | (b * 2 << 8) | (c * 2), (d * 2 << 16) | (e * 2 << 8) | (f * 2))

    def tri1(self, a, b, c):
        self.w((0x05 << 24) | (a * 2 << 16) | (b * 2 << 8) | (c * 2), 0)

    def call(self, path):
        self.cmds.append(struct.pack('<II', 0x31000000, 0) + hash_words(path))

    def pipesync(self):
        self.w(0xE7000000, 0)

    def end(self):
        self.w(0xDF000000, 0)

    def data(self):
        return header('dl') + b'\x04' + b'\xff' * 7 + b''.join(self.cmds)


# ---- Collision ----

def collision_file(verts, polys, surfaces, cam_settings=(3,), waterboxes=()):
    """verts: list of (x,y,z); polys: (surface_index, a, b, c, flagsA, flagsB); surfaces: (data1, data0)"""
    V = np.array(verts, float)
    mn = V.min(0).astype(int) - 1
    mx = V.max(0).astype(int) + 1
    out = header('col') + struct.pack('<3h3h', *mn, *mx)
    out += struct.pack('<i', len(verts)) + b''.join(struct.pack('<3h', *map(int, v)) for v in verts)
    out += struct.pack('<I', len(polys))
    for st, a, b, c, fa, fb in polys:
        p0, p1, p2 = V[a], V[b], V[c]
        n = np.cross(p1 - p0, p2 - p0)
        n /= max(np.linalg.norm(n), 1e-9)
        d = -np.dot(n, p0)
        out += struct.pack('<HHHHhhhh', st, (fa << 13) | a, (fb << 13) | b, c, *(int(round(x * 32767)) for x in n),
                           int(round(d)))
    out += struct.pack('<I', len(surfaces)) + b''.join(struct.pack('<II', d1, d0) for d1, d0 in surfaces)
    out += struct.pack('<I', len(cam_settings)) + b''.join(struct.pack('<Hhi', c, 0, -1) for c in cam_settings)
    out += struct.pack('<i', 0)
    out += struct.pack('<i', len(waterboxes)) + b''.join(struct.pack('<5hi', *w) for w in waterboxes)
    return out


# ---- Rooms and scenes ----

def actor_entry(aid, pos, rot, params):
    return struct.pack('<HhhhhhhH', aid, *map(int, pos), *map(int, rot), params & 0xFFFF)


def room_file(meshes, objects=(), actors=(), echo=0, behavior=(0, 0), skymod=(0, 0), time=(-1, -1, 0)):
    """meshes: list of (opa_path, xlu_path) ('' for none); actors: actor_entry bytes"""
    cmds = []
    cmds.append(struct.pack('<ib', 0x16, echo))
    cmds.append(struct.pack('<ibi', 0x08, *behavior))
    cmds.append(struct.pack('<ibb', 0x12, *skymod))
    cmds.append(struct.pack('<ibbb', 0x10, *time))
    mesh = struct.pack('<ibbb', 0x0A, 0, 0, len(meshes))
    for opa, xlu in meshes:
        mesh += struct.pack('<b', 0) + s(opa) + s(xlu)
    cmds.append(mesh)
    cmds.append(struct.pack('<iI', 0x0B, len(objects)) + b''.join(struct.pack('<H', o) for o in objects))
    cmds.append(struct.pack('<iI', 0x01, len(actors)) + b''.join(actors))
    cmds.append(struct.pack('<i', 0x14))
    return header('room') + struct.pack('<I', len(cmds)) + b''.join(cmds)


def scene_file(rooms, collision, spawns, entrances, exits=(), lighting=(), sound=(0, 19, 0), special=(0, 3),
               sky=(0, 0, 0, 1), camera=(0, 0), trans=()):
    """rooms: room paths; spawns: actor_entry bytes for each start position; entrances: (spawn, room);
    lighting: (18 colour/dir bytes..., fogNear s16, fogFar u16); trans: transition actors"""
    cmds = []
    cmds.append(struct.pack('<ibbb', 0x15, *sound))
    cmds.append(struct.pack('<ii', 0x04, len(rooms)) + b''.join(s(r) + struct.pack('<ii', 0, 0) for r in rooms))
    if trans:
        cmds.append(struct.pack('<iI', 0x0E, len(trans)) + b''.join(struct.pack('<BBBBhhhhhH', *t) for t in trans))
    cmds.append(struct.pack('<ibi', 0x19, *camera))
    cmds.append(struct.pack('<i', 0x03) + s(collision))
    cmds.append(struct.pack('<iI', 0x06, len(entrances)) + b''.join(struct.pack('<bb', *e) for e in entrances))
    cmds.append(struct.pack('<ibh', 0x07, *special))
    cmds.append(struct.pack('<iI', 0x00, len(spawns)) + b''.join(spawns))
    cmds.append(struct.pack('<ibbbb', 0x11, *sky))
    cmds.append(struct.pack('<iI', 0x13, len(exits)) + b''.join(struct.pack('<H', e) for e in exits))
    cmds.append(struct.pack('<ii', 0x0F, len(lighting)) +
                b''.join(struct.pack('<18bhH', *l) for l in lighting))
    cmds.append(struct.pack('<i', 0x14))
    return header('room') + struct.pack('<I', len(cmds)) + b''.join(cmds)


def pack(files, out_path):
    with zipfile.ZipFile(out_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        for path, data in files.items():
            zf.writestr(path, data)
