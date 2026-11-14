"""Plays a sequence made by midi2seq.py offline, on the game's own samples, to a .wav: a preview to listen to before
it goes in the game. It reads the sequence the way the game does (sequence script, channels, layers, the commands
midi2seq.py writes), so it also checks the sequence. The samples come out of oot.o2r (VADPCM decoded); envelopes are
simplified (a short attack, held, a short release) and the game's reverb is stood in for by a light echo.

    python3 seqpreview.py song.seq out.wav [seconds]"""
import struct, sys, numpy as np
sys.path.insert(0, '/home/claude/o2r')
from otr import z
from midi2seq import FONTS

RATE = 32000
TATUMS = 48


# ---- the game's data: fonts and samples ----
class Rd:
    def __init__(s, d, o=0x40):
        s.d, s.o = d, o
    def u8(s):
        v = s.d[s.o]; s.o += 1; return v
    def u16(s):
        v = struct.unpack_from('<H', s.d, s.o)[0]; s.o += 2; return v
    def u32(s):
        v = struct.unpack_from('<I', s.d, s.o)[0]; s.o += 4; return v
    def i16(s):
        v = struct.unpack_from('<h', s.d, s.o)[0]; s.o += 2; return v
    def f(s):
        v = struct.unpack_from('<f', s.d, s.o)[0]; s.o += 4; return v
    def st(s):
        n = s.u32(); v = s.d[s.o:s.o + n].decode(); s.o += n; return v


FONT_FILES = {int(n.split('/')[-1][:2]): n for n in z.namelist() if n.startswith('audio/fonts/')}


def load_font(idx):
    r = Rd(z.read(FONT_FILES[idx]))
    r.u32(); r.u8(); r.u8(); r.u16(); r.u16(); r.u16()
    nd, ni, ns = r.u32(), r.u32(), r.u32()
    drums = []
    for i in range(nd):
        r.u8(); r.u8(); r.u8(); ne = r.u32(); r.o += 4 * ne; r.u8(); n = r.st(); t = r.f()
        drums.append((n, t))
    insts = []
    def snd():
        if not r.u8():
            return None
        r.u8(); n = r.st(); t = r.f(); return (n, t)
    for i in range(ni):
        valid = r.u8(); r.u8(); lo = r.u8(); hi = r.u8(); rel = r.u8(); ne = r.u32(); r.o += 4 * ne
        a, b, c = snd(), snd(), snd()
        insts.append((valid, lo, hi, a, b, c) if valid else None)
    return drums, insts


_samples = {}


def load_sample(name):
    if name in _samples:
        return _samples[name]
    r = Rd(z.read(name))
    codec = r.u8(); r.u8(); r.u8(); r.u8()
    size = r.u32(); data = r.d[r.o:r.o + size]; r.o += size
    ls, le, lc = r.u32(), r.u32(), r.u32()
    n = r.u32(); r.o += 2 * n
    order = struct.unpack_from('<i', r.d, r.o)[0]; r.o += 4
    npred = struct.unpack_from('<i', r.d, r.o)[0]; r.o += 4
    nb = r.u32()
    book = np.array(struct.unpack_from('<%dh' % nb, r.d, r.o), np.int64).reshape(npred, order, 8) if nb else None
    if codec == 0:
        pcm = vadpcm(data, book, order)
    else:
        pcm = np.frombuffer(data[:size // 2 * 2], '>i2').astype(np.float32) / 32768.0
    loop = (ls, le) if lc != 0 and le > ls else None
    _samples[name] = (pcm, loop)
    return _samples[name]


def vadpcm(data, book, order):
    out = []
    state = [0] * 8
    for f in range(len(data) // 9):
        hdr = data[f * 9]
        scale = 1 << (hdr >> 4)
        pred = hdr & 0x0F
        if book is None or pred >= len(book):
            pred = 0
        nibs = []
        for b in data[f * 9 + 1:f * 9 + 9]:
            for v in (b >> 4, b & 0x0F):
                nibs.append((v - 16 if v >= 8 else v) * scale)
        for h in range(2):
            ins = nibs[h * 8:h * 8 + 8]
            res = []
            for k in range(8):
                tot = 0
                if book is not None:
                    tot = int(book[pred][0][k]) * state[6] + int(book[pred][1][k]) * state[7]
                    for l in range(k):
                        tot += int(book[pred][1][k - 1 - l]) * ins[l]
                tot += ins[k] << 11
                v = tot >> 11
                v = max(-32768, min(32767, v))
                res.append(v)
            state = res
            out.extend(res)
    return np.array(out, np.float32) / 32768.0


# ---- the sequence, read as the game reads it ----
def cu16(d, i):
    v = d[i]; i += 1
    if v & 0x80:
        v = ((v << 8) & 0x7F00) | d[i]; i += 1
    return v, i


def run(seq, seconds):
    end_t = int(seconds * 1e9)   # (in tatums: limited below by the song's own length)
    notes = []   # (start tatum, dur tatum, gate, font, inst, semi, vel, chan)
    chvol = {}   # chan -> [(t, vol)]
    chpan = {}
    tempo = 120
    # sequence script
    i = 0
    starts = {}
    seq_len = 0
    t = 0
    visited_loop = False
    while i < len(seq):
        c = seq[i]; i += 1
        if c in (0xD3, 0xD5, 0xDB):
            i += 1
        elif c == 0xDD:
            tempo = seq[i]; i += 1
        elif c in (0xD7, 0xD6):
            i += 2
        elif 0x90 <= c <= 0x9F:
            starts[c & 0xF] = (struct.unpack_from('>H', seq, i)[0], t); i += 2
        elif c == 0xFD:
            d, i = cu16(seq, i); t += d
        elif c == 0xFB:
            break   # (the loop back: the preview plays once)
        elif c == 0xFF:
            break
        else:
            raise SystemExit('sequence: unknown command %02X at %04X' % (c, i - 1))
    seq_len = t
    for ch, (addr, t0) in starts.items():
        j = addr; t = t0; font = FONTS[-1]; inst = 0; tr = 0
        chvol[ch] = [(0, 100)]; chpan[ch] = [(0, 64)]
        layers = []
        while True:
            c = seq[j]; j += 1
            if c == 0xC4:
                pass
            elif c == 0xC6:
                font = FONTS[len(FONTS) - 1 - seq[j]]; j += 1
            elif c == 0xC1:
                inst = seq[j]; j += 1
                layers_inst = (font, inst)
            elif c in (0xD4, 0xD9):
                j += 1
            elif c == 0xDD:
                chpan[ch].append((t, seq[j])); j += 1
            elif c == 0xDF:
                chvol[ch].append((t, seq[j])); j += 1
            elif c == 0xDB:
                tr = struct.unpack('b', seq[j:j + 1])[0]; j += 1
            elif 0x88 <= c <= 0x8B:
                layers.append((struct.unpack_from('>H', seq, j)[0], t, font, inst)); j += 2
            elif c == 0xFD:
                d, j = cu16(seq, j); t += d
            elif c == 0xFF:
                break
            else:
                raise SystemExit('channel %d: unknown command %02X at %04X' % (ch, c, j - 1))
        for addr, lt, lfont, linst in layers:
            k = addr; t = lt; transpose = 0
            while True:
                c = seq[k]; k += 1
                if c == 0xFF:
                    break
                if c == 0xC0:
                    d, k = cu16(seq, k); t += d
                elif c == 0xC2:
                    transpose = struct.unpack('b', seq[k:k + 1])[0]; k += 1
                elif c < 0xC0:
                    kind = c & 0xC0; semi = c & 0x3F
                    if kind == 0x00:
                        d, k = cu16(seq, k); vel = seq[k]; gate = seq[k + 1]; k += 2
                    elif kind == 0x40:
                        d, k = cu16(seq, k); vel = seq[k]; gate = 0; k += 1
                    else:
                        raise SystemExit('layer: unexpected short note')
                    notes.append((t, d, gate, lfont, linst, semi + transpose + tr, vel, ch))
                    t += d
                else:
                    raise SystemExit('layer: unknown command %02X at %04X' % (c, k - 1))
    return tempo, seq_len, notes, chvol, chpan


def at(lst, t):
    v = lst[0][1]
    for tt, x in lst:
        if tt <= t:
            v = x
    return v


def render(seq, out_wav, seconds=None):
    tempo, seq_len, notes, chvol, chpan = run(seq, seconds or 1e9)
    sec_per_tatum = 60.0 / tempo / TATUMS
    total = seq_len * sec_per_tatum
    if seconds:
        total = min(total, seconds)
    n_out = int((total + 3) * RATE)
    L = np.zeros(n_out, np.float32)
    R = np.zeros(n_out, np.float32)
    fonts = {f: load_font(f) for f in FONTS}
    gnote = lambda semi: 2.0 ** ((semi - 39) / 12.0)
    missing = set()
    for t, d, gate, font, inst, semi, vel, ch in notes:
        start = t * sec_per_tatum
        if start >= total:
            continue
        drums, insts = fonts[font]
        if inst == 0x7F:
            if not (0 <= semi < len(drums)):
                continue
            name, tun = drums[semi]
            step = tun
            hold = None   # drums ring out
        else:
            ins = insts[inst] if inst < len(insts) else None
            if ins is None:
                missing.add((font, inst)); continue
            _, lo, hi, a, b, c = ins
            snd = b
            if semi < lo and a:
                snd = a
            elif semi > hi and c:
                snd = c
            if snd is None:
                continue
            name, tun = snd
            step = tun * gnote(semi)
            hold = d * sec_per_tatum * (gate / 256.0 if gate else 1.0)
        pcm, loop = load_sample(name)
        rel = 0.12
        length = (hold + rel) if hold is not None else len(pcm) / (step * RATE)
        n = int(length * RATE)
        if n <= 0:
            continue
        pos = np.arange(n) * step
        if loop and hold is not None:
            ls, le = loop
            over = pos >= le
            pos[over] = ls + np.mod(pos[over] - ls, le - ls)
        else:
            keep = pos < len(pcm) - 1
            pos = pos[keep]; n = len(pos)
            if n == 0:
                continue
        i0 = pos.astype(np.int64)
        fr = pos - i0
        i1 = np.minimum(i0 + 1, len(pcm) - 1)
        s = pcm[i0] * (1 - fr) + pcm[i1] * fr
        env = np.ones(n, np.float32)
        a_n = min(n, int(0.006 * RATE))
        env[:a_n] = np.linspace(0, 1, a_n, endpoint=False) if a_n else env[:a_n]
        if hold is not None:
            h_n = int(hold * RATE)
            if h_n < n:
                env[h_n:] *= np.exp(-np.arange(n - h_n) / (rel * RATE / 4))
        vol = at(chvol[ch], t) / 127.0
        amp = (vel / 127.0) ** 2 * vol * 0.42
        pan = at(chpan[ch], t) / 127.0
        gl, gr = np.cos(pan * np.pi / 2), np.sin(pan * np.pi / 2)
        o = int(start * RATE)
        e = min(n_out, o + n)
        L[o:e] += (s * env)[:e - o] * amp * gl * 1.4
        R[o:e] += (s * env)[:e - o] * amp * gr * 1.4
    # a light echo for the game's reverb
    for dl, g in ((0.087, 0.22), (0.131, 0.16), (0.193, 0.11), (0.271, 0.07)):
        k = int(dl * RATE)
        L[k:] += L[:-k] * g
        R[k:] += R[:-k] * g
    mix = np.stack([L, R], 1)
    peak = np.max(np.abs(mix)) or 1.0
    mix = mix / max(peak, 1.0) * 0.95
    pcm16 = (mix * 32767).astype('<i2')
    with open(out_wav, 'wb') as f:
        data = pcm16.tobytes()
        f.write(b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' +
                struct.pack('<IHHIIHH', 16, 1, 2, RATE, RATE * 4, 4, 16) + b'data' + struct.pack('<I', len(data)) + data)
    return dict(seconds=round(total, 1), notes=len(notes), missing=sorted(missing))


if __name__ == '__main__':
    seq = open(sys.argv[1], 'rb').read()
    print(render(seq, sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else None))
