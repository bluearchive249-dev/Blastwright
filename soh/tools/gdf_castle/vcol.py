"""Reading a vanilla collision header from the game's archive (o2r): vertices, polygons, surfaces."""
import struct
from otr import z


def read(path):
    d = z.read(path)
    p = 0x40
    mn = struct.unpack_from('<3h', d, p); mx = struct.unpack_from('<3h', d, p + 6); p += 12
    nv = struct.unpack_from('<i', d, p)[0]; p += 4
    verts = [struct.unpack_from('<3h', d, p + i * 6) for i in range(nv)]; p += nv * 6
    npl = struct.unpack_from('<I', d, p)[0]; p += 4
    polys = []
    for i in range(npl):
        t, a, b, c, nx, ny, nz, dist = struct.unpack_from('<HHHHhhhh', d, p + i * 16)
        polys.append((t, a & 0x1FFF, b & 0x1FFF, c, (nx, ny, nz), dist, a >> 13, b >> 13))
    p += npl * 16
    ns = struct.unpack_from('<I', d, p)[0]; p += 4
    surf = [struct.unpack_from('<II', d, p + i * 8) for i in range(ns)]
    return verts, polys, surf


def floor_at(verts, polys, x, z, below=10000):
    """The highest floor under (x, z) (below `below`), or None."""
    best = None
    for t, a, b, c, n, dist, fa, fb in polys:
        if n[1] < 8000:
            continue
        p = [verts[a], verts[b], verts[c]]
        def side(p1, p2):
            return (p2[0] - p1[0]) * (z - p1[2]) - (p2[2] - p1[2]) * (x - p1[0])
        s = [side(p[0], p[1]), side(p[1], p[2]), side(p[2], p[0])]
        if not (all(v >= 0 for v in s) or all(v <= 0 for v in s)):
            continue
        y = -(n[0] * x + n[2] * z + dist * 32767) / n[1]
        if y < below and (best is None or y > best):
            best = y
    return best
