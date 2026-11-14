import struct
def varlen(d, i):
    v = 0
    while True:
        b = d[i]; i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80: return v, i
def read_midi(path):
    """Returns (ticks per beat, [(abs tick, kind, data...)]) merged from all tracks, in time order."""
    d = open(path, 'rb').read()
    assert d[:4] == b'MThd'
    fmt, ntr, div = struct.unpack('>HHH', d[8:14])
    i = 8 + struct.unpack('>I', d[4:8])[0]
    ev = []
    for t in range(ntr):
        while d[i:i+4] != b'MTrk':
            i += 8 + struct.unpack('>I', d[i+4:i+8])[0]
        ln = struct.unpack('>I', d[i+4:i+8])[0]; j = i + 8; end = j + ln; i = end
        tick = 0; run = None; seq = 0
        while j < end:
            dt, j = varlen(d, j); tick += dt
            st = d[j]
            if st == 0xFF:
                typ = d[j+1]; ln2, k = varlen(d, j + 2); data = d[k:k+ln2]; j = k + ln2
                if typ == 0x51: ev.append((tick, t, seq, 'tempo', (data[0] << 16) | (data[1] << 8) | data[2]))
                elif typ == 0x2F: ev.append((tick, t, seq, 'end', 0))
                elif typ in (0x03,): ev.append((tick, t, seq, 'name', data.decode('latin1')))
                elif typ == 0x06: ev.append((tick, t, seq, 'marker', data.decode('latin1')))
                seq += 1; continue
            if st in (0xF0, 0xF7):
                ln2, k = varlen(d, j + 1); j = k + ln2; continue
            if st & 0x80: run = st; j += 1
            st = run; ch = st & 0x0F; kind = st & 0xF0
            n1 = 1 if kind in (0xC0, 0xD0) else 2
            a = d[j]; b = d[j+1] if n1 == 2 else 0; j += n1
            if kind == 0x90 and b == 0: kind = 0x80
            ev.append((tick, t, seq, {0x80: 'off', 0x90: 'on', 0xA0: 'pat', 0xB0: 'cc', 0xC0: 'prog', 0xD0: 'cat', 0xE0: 'bend'}[kind], (ch, a, b)))
            seq += 1
    ev.sort(key=lambda e: (e[0], 0 if e[3] in ('off',) else 1, e[1], e[2]))
    return div, ev
