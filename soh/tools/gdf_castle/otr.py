import struct, zipfile
POLY = 0x42F0E1EBA9EA3693
T = []
for i in range(256):
    c = i << 56
    for _ in range(8):
        c = ((c << 1) ^ POLY) if c & (1 << 63) else (c << 1)
    T.append(c & 0xFFFFFFFFFFFFFFFF)
def crc64(s):
    c = 0xFFFFFFFFFFFFFFFF
    for b in s.encode():
        c = T[((c >> 56) ^ b) & 0xFF] ^ ((c << 8) & 0xFFFFFFFFFFFFFFFF)
    return c & 0xFFFFFFFFFFFFFFFF
z = zipfile.ZipFile('/mnt/user-data/uploads/soh/oot.o2r')
HASH = {crc64(n): n for n in z.namelist()}
def vtx(path):
    d = z.read(path); n = struct.unpack_from('<I', d, 0x44)[0]
    assert 0x48 + n * 16 == len(d), (path, n, len(d))
    out = []
    for i in range(n):
        x,y,zz,fl,s,t,r,g,b,a = struct.unpack_from('<hhhHhhBBBB', d, 0x48 + i*16)
        out.append((x,y,zz,r,g,b,a))
    return out
def dl(path):
    d = z.read(path); p = 0x41
    while p % 8: p += 1
    cmds = []
    while p + 8 <= len(d):
        w0, w1 = struct.unpack_from('<II', d, p); p += 8
        op = w0 >> 24
        if op in (0x20,0x31,0x32,0x33,0x35,0x36,0x42):
            a0, a1 = struct.unpack_from('<II', d, p); p += 8
            cmds.append((op, w0, w1, a0, a1))
        else:
            cmds.append((op, w0, w1))
        if op == 0xDF: break
    return cmds
