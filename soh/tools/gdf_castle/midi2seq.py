"""Turns a MIDI file into an Ocarina of Time music sequence played on the game's own instruments, so a tune sounds like
the game's own background music (its orchestra, choir, church organ, harp, drums and timpani).

    python3 midi2seq.py song.mid out.seq [--loop] [--volume 0x58]

- Each MIDI channel becomes a sequence channel; its notes are shared out over up to four layers (one note at a time
  each), so chords keep up to four notes.
- General MIDI instruments are matched to the nearest instrument in the game's soundfonts (MAP below). A sequence can
  use up to 16 fonts; this one loads the five it needs (FONTS) and each channel picks its own with the `font` command.
- GM drums (channel 10) are matched to the orchestra kit: bass drum, snare, cymbal and the pitched timpani.
- Tempo, channel volume (CC7 x CC11) and pan (CC10) carry over; pitch bend does not.
- Timing: 48 tatums to the beat (the game's own), MIDI ticks rounded on their absolute times so nothing drifts.

The sequence is wrapped as a Ship of Harkinian AudioSequence resource by seq_resource() (the fonts listed in it), and
goes into a mod archive under custom/music/..., where the game registers it with a sequence number of its own."""
import struct, sys
from midiparse import read_midi

TATUMS = 48

# The fonts this sequence loads (as the game numbers them). A channel picks one by its index counted from the end of
# this list (the game's `font` command reads fonts[numFonts - i - 1]).
FONTS = [0x03, 0x1E, 0x20, 0x21, 0x1B]   # Orchestra, Ganon's Castle Organ, Ganondorf's Battle, Ending 1, Gerudo Valley
F_ORCH, F_ORGAN, F_BATTLE, F_ENDING, F_GERUDO = FONTS


def font_cmd_index(font):
    return len(FONTS) - 1 - FONTS.index(font)


# General MIDI program -> (font, instrument, octave shift)
def gm_map(prog):
    if prog < 8:
        return (F_ORCH, 13, 0)                 # pianos: the orchestra's piano
    if prog in (8, 9, 10, 11, 98):
        return (F_ORCH, 8, 0)                  # celesta, glockenspiel, music box, vibraphone: glockenspiel
    if prog in (12, 13):
        return (F_ORCH, 15, 0)                 # marimba, xylophone
    if prog == 14:
        return (F_ENDING, 13, 0)               # tubular bells: the church bell
    if prog == 15:
        return (F_ORCH, 12, 0)                 # dulcimer: harp
    if 16 <= prog <= 23:
        return (F_ORGAN, 0, 0)                 # organs: Ganon's church organ
    if 24 <= prog <= 31:
        return (F_GERUDO, 2, 0)                # guitars: acoustic guitar
    if 32 <= prog <= 39:
        return (F_GERUDO, 5, 0)                # basses: bass guitar
    if prog in (40, 44, 48, 49, 50, 51):
        return (F_ORCH, 10, 0)                 # violin, tremolo, string ensembles: strings
    if prog in (41, 42):
        return (F_ENDING, 7, 0)                # viola, cello: viola
    if prog == 43:
        return (F_ORCH, 10, 0)                 # contrabass: strings
    if prog == 45:
        return (F_ORCH, 14, 0)                 # pizzicato: harp
    if prog == 46:
        return (F_ORCH, 12, 0)                 # harp
    if prog == 47:
        return ('timpani', 0, 0)               # timpani: the drum kit's tuned timpani
    if prog in (52, 54):
        return (F_BATTLE, 1, 0)                # choir aahs, synth voice: vocal aahs
    if prog == 53:
        return (F_BATTLE, 2, 0)                # voice oohs: vocal eees
    if prog == 55:
        return (F_BATTLE, 5, 0)                # orchestra hit: trumpet
    if prog in (56, 59, 61, 62, 63):
        return (F_ORCH, 5, 0)                  # trumpets, brass sections: trumpet
    if prog == 57:
        return (F_ORCH, 6, 0)                  # trombone
    if prog == 58:
        return (F_ORCH, 7, 0)                  # tuba
    if prog == 60:
        return (F_ORCH, 4, 0)                  # french horn
    if 64 <= prog <= 67 or prog == 71:
        return (F_ORCH, 2, 0)                  # saxes, clarinet: clarinet
    if prog in (68, 69):
        return (F_ORCH, 1, 0)                  # oboe, english horn
    if prog == 70:
        return (F_ORCH, 3, 0)                  # bassoon
    if 72 <= prog <= 79:
        return (F_ORCH, 0, 0)                  # pipes: flute
    if 80 <= prog <= 87:
        return (F_ORCH, 1, 0)                  # synth leads: oboe
    if 88 <= prog <= 95:
        return (F_BATTLE, 1, 0)                # pads: choir
    if 96 <= prog <= 103:
        return (F_ORCH, 8, 0)                  # synth effects: glockenspiel
    if prog in (104, 105, 106, 107):
        return (F_ORCH, 12, 0)                 # sitar, banjo, shamisen, koto: harp
    if prog == 108:
        return (F_ORCH, 15, 0)                 # kalimba: marimba
    if prog in (109, 111):
        return (F_ORCH, 1, 0)                  # bagpipe, shanai: oboe
    if prog == 110:
        return (F_ORCH, 10, 0)                 # fiddle: strings
    if 112 <= prog <= 119:
        return (F_ORCH, 15, 0)                 # percussive: marimba
    return None                                # sound effects: left out


# General MIDI drums (key) -> (drum index in the orchestra kit, velocity scale)
GM_DRUMS = {35: (6, 1.0), 36: (8, 1.0), 37: (17, 0.6), 38: (16, 1.0), 39: (19, 0.8), 40: (18, 1.0),
            41: (36, 1.0), 43: (39, 1.0), 45: (42, 1.0), 47: (45, 1.0), 48: (48, 1.0), 50: (51, 1.0),
            42: (32, 0.25), 44: (32, 0.2), 46: (31, 0.35), 49: (28, 1.0), 57: (27, 1.0), 51: (30, 0.45),
            59: (30, 0.45), 53: (31, 0.5), 52: (25, 0.9), 55: (31, 0.7), 54: (20, 0.3), 56: (20, 0.3),
            81: (32, 0.2), 80: (32, 0.2)}
TIMPANI_C4 = 51   # the kit's timpani: index 51 plays at the sample's own pitch, taken as middle C


def compressed(v):
    v = max(0, min(0x7FFF, int(v)))
    return bytes([v]) if v < 0x80 else bytes([0x80 | (v >> 8), v & 0xFF])


class Track:
    """One MIDI channel's notes, volume and pan, in tatums."""
    def __init__(self, ch):
        self.ch = ch
        self.notes = []      # (start, end, key, velocity, program)
        self.vol = []        # (time, volume 0..127)
        self.pan = []        # (time, pan 0..127)
        self.prog = []       # (time, program)


def gather(path):
    div, ev = read_midi(path)
    tempo = next((e[4] for e in ev if e[3] == 'tempo'), 500000)
    bpm = 60e6 / tempo
    T = lambda tick: int(round(tick * TATUMS / div))
    tracks = {}
    on = {}
    vol7 = {}
    exp11 = {}
    prog = {}
    end = 0
    for tick, _t, _s, kind, data in ev:
        t = T(tick)
        end = max(end, t)
        if kind in ('tempo', 'name', 'marker', 'end'):
            continue
        ch = data[0]
        tr = tracks.setdefault(ch, Track(ch))
        if kind == 'prog':
            prog[ch] = data[1]
            tr.prog.append((t, data[1]))
        elif kind == 'cc':
            if data[1] in (7, 11):
                (vol7 if data[1] == 7 else exp11)[ch] = data[2]
                v = vol7.get(ch, 100) * exp11.get(ch, 127) / 127
                tr.vol.append((t, int(round(v))))
            elif data[1] == 10:
                tr.pan.append((t, data[2]))
        elif kind == 'on':
            on.setdefault((ch, data[1]), []).append((t, data[2], prog.get(ch, 0)))
        elif kind == 'off':
            st = on.get((ch, data[1]))
            if st:
                s, vel, p = st.pop(0)
                tr.notes.append((s, max(t, s + 1), data[1], vel, p))
    for (ch, key), lst in on.items():   # notes never let go: end them at the song's end
        for s, vel, p in lst:
            tracks[ch].notes.append((s, max(end, s + 1), key, vel, p))
    tempos = [(T(e[0]), 60e6 / e[4]) for e in ev if e[3] == 'tempo']
    return trim(bpm, tempos, tracks, end)


def trim(bpm, tempos, tracks, end, bar=4 * TATUMS):
    """For a seamless loop: the silence before the first note is cut, and the song ends where it comes round to its
    start again (a MIDI written to be heard once repeats its opening and fades out: that repeat and its fade are cut,
    the game's loop plays the opening again instead). Failing a repeat, it ends where the closing fade begins."""
    notes = sorted((n[0], tr.ch, n[2]) for tr in tracks.values() for n in tr.notes)
    if not notes:
        return bpm, tempos, tracks, end
    start = notes[0][0]
    have = set(notes)
    window = [(t, c, k) for t, c, k in notes if start <= t < start + 8 * bar]
    repeats = []
    for t in range(start + 16 * bar, end, TATUMS):
        off = t - start
        match = sum((tt + off, c, k) in have for tt, c, k in window) / max(1, len(window))
        if match >= 0.4:
            repeats.append(t)
    cut = max(repeats) if repeats else end   # (the last repeat of the opening: the whole song before it plays)
    if not repeats:
        # (no repeat found: the closing fade, where every channel's volume starts falling for good)
        fades = [max((tt for tt, v in tr.vol if v >= max(vv for _, vv in tr.vol) - 1), default=end) for tr in tracks.values()
                 if tr.vol and tr.vol[-1][1] < 10]
        cut = min(fades) if fades else end
    for tr in tracks.values():
        tr.notes = [(a - start, min(b, cut) - start, k, v, p) for a, b, k, v, p in tr.notes if start <= a < cut]
        keep = lambda lst: [(max(0, t - start), v) for t, v in lst if t < cut]
        last_vol = [v for t, v in tr.vol if t <= start]
        tr.vol = [(t, v) for t, v in keep(tr.vol)]
        if last_vol:
            tr.vol = [(0, last_vol[-1])] + [(t, v) for t, v in tr.vol if t > 0]
        tr.pan = keep(tr.pan)
        tr.prog = keep(tr.prog)
    tempos = [(max(0, t - start), b) for t, b in tempos if t < cut]
    return bpm, tempos, tracks, cut - start


def split_layers(notes, max_layers=4):
    """Shares notes out over layers so no layer has two at once (lowest free layer first); notes beyond the last layer
    are dropped. A note cut short where the next one on its layer starts."""
    layers = [[] for _ in range(max_layers)]
    free_at = [0] * max_layers
    dropped = 0
    for n in sorted(notes, key=lambda n: (n[0], -n[2])):
        for i in range(max_layers):
            if free_at[i] <= n[0]:
                layers[i].append(n)
                free_at[i] = n[1]
                break
        else:
            dropped += 1
    return [l for l in layers if l], dropped


def layer_script(notes, drum_map=None, timpani=False):
    """A layer: notes (large-note form: pitch, delay, velocity, gate) and rests, transposed into range as it goes."""
    out = bytearray()
    t = 0
    transpose = 0
    for s, e, key, vel, _p in notes:
        if s > t:
            out += b'\xC0' + compressed(s - t)    # a rest
            t = s
        dur = e - s
        if drum_map is not None:
            if timpani:
                idx, vs = TIMPANI_C4 + (key - 60), 1.0
                while idx < 33:
                    idx += 12
                while idx > 63:
                    idx -= 12
            else:
                if key not in drum_map:
                    out += b'\xC0' + compressed(dur)
                    t = e
                    continue
                idx, vs = drum_map[key]
            pitch = idx
            vel = int(vel * vs)
            want = 0
        else:
            semi = key - 21
            want = transpose
            if not (0 <= semi - want <= 63):
                want = 12 * ((semi - 32) // 12)   # (keeps the note near the middle of the 0..63 range)
            pitch = semi - want
        if want != transpose:
            out += bytes([0xC2, want & 0xFF])     # layer_transpose
            transpose = want
        while dur > 0x7FFF:   # (a very long note: held on in pieces)
            out += bytes([0x00 | pitch]) + compressed(0x7FFF) + bytes([max(1, min(127, vel)), 0])
            dur -= 0x7FFF
        out += bytes([0x00 | pitch]) + compressed(dur) + bytes([max(1, min(127, vel)), 0xF4])
        t = e
    return out, t


def intro_parts(tracks, bars=8, bar=4 * TATUMS):
    """A slow intro that builds into the song, from the song's own harmony: each bar a chord (the notes the song
    strikes on its first beats, bar by bar), held on Ganon's church organ from the start, the choir joining at bar 3,
    low strings at bar 5, a timpani roll swelling through the last two bars into the song. Returns
    [(font, instrument, notes, volume ramp, drums?)], each notes (start, end, key, velocity, program)."""
    chords = []
    for b in range(bars):
        keys = sorted({n[2] for tr in tracks.values() if tr.ch != 9 for n in tr.notes
                       if b * bar <= n[0] < b * bar + bar // 2})
        if not keys:
            keys = chords[-1] if chords else [60]
        chords.append(keys)
    root = [min(k) for k in chords]
    def voiced(keys, lo, hi, n):
        pcs = []
        for k in sorted(keys):
            if k % 12 not in [p % 12 for p in pcs]:
                pcs.append(k)
        out = []
        for k in pcs[:n]:
            while k < lo:
                k += 12
            while k > hi:
                k -= 12
            out.append(k)
        return out
    organ, choir, strings, timp = [], [], [], []
    for b in range(bars):
        t0, t1 = b * bar, (b + 1) * bar
        swell = (b + 1) / bars
        for k in voiced(chords[b], 48, 67, 4):
            organ.append((t0, t1, k, int(40 + 60 * swell), 19))
        if b >= 2:
            for k in voiced(chords[b], 60, 76, 3):
                choir.append((t0, t1, k, int(30 + 80 * swell), 52))
        if b >= 4:
            strings.append((t0, t1, voiced([root[b]], 36, 47, 1)[0], int(50 + 60 * swell), 48))
    # the roll: every eighth of a beat over the last two bars, louder and louder
    r = voiced([root[-1]], 42, 60, 1)[0]
    start = (bars - 2) * bar
    step = TATUMS // 6
    for i, t in enumerate(range(start, bars * bar, step)):
        timp.append((t, t + step, r, int(20 + 60 * i / ((bars * bar - start) / step)), 47))
    ramp = lambda v0, v1, a, b_: [(a + i * TATUMS, int(v0 + (v1 - v0) * i / max(1, (b_ - a) // TATUMS)))
                                   for i in range((b_ - a) // TATUMS + 1)]
    return [(F_ORGAN, 0, organ, ramp(45, 105, 0, bars * bar), False),
            (F_BATTLE, 1, choir, ramp(40, 100, 2 * bar, bars * bar), False),
            (F_ORCH, 10, strings, ramp(50, 100, 4 * bar, bars * bar), False),
            (F_ORCH, 0x7F, timp, ramp(50, 95, start, bars * bar), 'timpani')], bars * bar


def build(path, loop=True, volume=0x58, reverb=0x28, intro=True, intro_tempo=0.72):
    """intro: a slow build-up played once before the song; the loop goes back to the song's start, not the intro's."""
    bpm, tempos, tracks, end = gather(path)
    chans = []
    for ch in sorted(tracks):
        tr = tracks[ch]
        if not tr.notes:
            continue
        chans.append(tr)
    chans = chans[:16]
    # The pieces: sequence script first (fixed length, addresses patched after), then channels, then layers
    blobs = []          # (label, bytes, fixups[(offset in blob, label)])
    def add(label, data, fixups=()):
        blobs.append((label, bytearray(data), list(fixups)))

    seq = bytearray()
    fix = []
    mask = 0
    parts, intro_len = intro_parts(tracks) if intro else ([], 0)
    n_intro = min(len(parts), 16 - len(chans))
    for i in range(len(chans) + n_intro):
        mask |= 1 << i
    seq += bytes([0xD3, 0x20])                         # mute behaviour (as the game's sequences do)
    seq += bytes([0xD5, 0x46])                         # mute scale
    seq += bytes([0xD7]) + struct.pack('>H', mask)     # init channels
    seq += bytes([0xDB, volume])                       # volume
    if n_intro:
        # the intro, once: slower, its own channels (after the song's), then the song
        seq += bytes([0xDD, max(1, min(255, int(round(bpm * intro_tempo))))])
        for k in range(n_intro):
            fix.append((len(seq) + 1, 'intro%d' % k))
            seq += bytes([0x90 | (len(chans) + k), 0, 0])
        t = 0
        while intro_len - t > 0:
            step = min(0x7FFF, intro_len - t)
            seq += b'\xFD' + compressed(step)
            t += step
    seq += bytes([0xDD, max(1, min(255, int(round(bpm))))])
    loop_at = len(seq)
    for i in range(len(chans)):
        fix.append((len(seq) + 1, 'chan%d' % i))
        seq += bytes([0x90 | i, 0, 0])                 # start channel i
    # tempo changes along the way, then the end (or back to the start)
    t = 0
    for tt, b in tempos[1:]:
        if tt > t:
            seq += b'\xFD' + compressed(tt - t)
            t = tt
        seq += bytes([0xDD, max(1, min(255, int(round(b))))])
    total = max(end, 1)
    while total - t > 0:
        step = min(0x7FFF, total - t)
        seq += b'\xFD' + compressed(step)
        t += step
    if loop:
        fix.append((len(seq) + 1, 'seq_loop'))
        seq += bytes([0xFB, 0, 0])
    else:
        seq += bytes([0xD6]) + struct.pack('>H', mask)  # (disable the channels) then end
        seq += b'\xFF'
    add('seq', seq, fix)

    dropped = 0
    for k in range(n_intro):
        font, inst, notes, ramp, kind = parts[k]
        c = bytearray([0xC4, 0xC6, font_cmd_index(font), 0xC1, inst, 0xD4, reverb + 0x10, 0xDD, 64, 0xDF, ramp[0][1]])
        cf = []
        layers, d = split_layers(notes)
        for li, ln in enumerate(layers):
            cf.append((len(c) + 1, 'ilayer%d_%d' % (k, li)))
            c += bytes([0x88 | li, 0, 0])
            ls, _ = layer_script(ln, {} if kind else None, timpani=bool(kind))
            add('ilayer%d_%d' % (k, li), ls + b'\xFF')
        t = 0
        for tt, v in ramp:
            if tt > t:
                c += b'\xFD' + compressed(tt - t)
                t = tt
            c += bytes([0xDF, max(0, min(127, v))])
        if intro_len > t:
            c += b'\xFD' + compressed(intro_len - t)
        c += b'\xFF'
        add('intro%d' % k, c, cf)
    for i, tr in enumerate(chans):
        drums = tr.ch == 9
        progs = {n[4] for n in tr.notes}
        prog0 = tr.notes[0][4]
        if drums:
            font, inst = F_ORCH, 0x7F
        else:
            m = gm_map(prog0)
            if m is None:
                continue
            if m[0] == 'timpani':
                font, inst = F_ORCH, 0x7F
            else:
                font, inst = m[0], m[1]
        timp = (not drums) and gm_map(prog0) and gm_map(prog0)[0] == 'timpani'
        c = bytearray()
        cf = []
        c += bytes([0xC4])                               # large notes (each note carries its velocity and gate)
        c += bytes([0xC6, font_cmd_index(font)])         # its font
        c += bytes([0xC1, inst])                         # its instrument
        c += bytes([0xD4, reverb])
        c += bytes([0xDD, 64])
        c += bytes([0xDF, 100])
        # program changes within the song: split notes by program, each its own layer set would be heavy: so the
        # channel keeps its first instrument, and a later program switch changes it at its time
        layers, d = split_layers(tr.notes)
        dropped += d
        for li, notes in enumerate(layers):
            cf.append((len(c) + 1, 'layer%d_%d' % (i, li)))
            c += bytes([0x88 | li, 0, 0])
            ls, _ = layer_script(notes, GM_DRUMS if drums else ({} if timp else None), timpani=bool(timp))
            add('layer%d_%d' % (i, li), ls + b'\xFF')
        # volume, pan and program changes over time
        evs = [(tt, 'v', v) for tt, v in tr.vol] + [(tt, 'p', p) for tt, p in tr.pan] + \
              [(tt, 'i', p) for tt, p in tr.prog if tt > 0]
        evs.sort()
        t = 0
        for tt, k, v in evs:
            if tt > t:
                c += b'\xFD' + compressed(tt - t)
                t = tt
            if k == 'v':
                c += bytes([0xDF, max(0, min(127, v))])
            elif k == 'p':
                c += bytes([0xDD, max(0, min(127, v))])
            elif k == 'i' and not drums:
                m = gm_map(v)
                if m is not None and m[0] != 'timpani':
                    c += bytes([0xC6, font_cmd_index(m[0]), 0xC1, m[1]])
        if total > t:
            rem = total - t
            while rem > 0:
                step = min(0x7FFF, rem)
                c += b'\xFD' + compressed(step)
                rem -= step
        c += b'\xFF'
        add('chan%d' % i, c, cf)

    # lay out and patch the addresses
    addr = {}
    pos = 0
    for label, data, _ in blobs:
        addr[label] = pos
        pos += len(data)
    addr['seq_loop'] = loop_at
    out = bytearray()
    for label, data, fixups in blobs:
        for off, target in fixups:
            a = addr[target]
            data[off:off + 2] = struct.pack('>H', a)
        out += data
    if len(out) > 0xFFFF:
        raise SystemExit('sequence too long (%d bytes): addresses are 16-bit' % len(out))
    return bytes(out), dict(bpm=bpm, channels=len(chans), dropped=dropped, tatums=total, size=len(out))


def seq_resource(seq, fonts=FONTS, seq_number=0):
    """A Ship of Harkinian AudioSequence resource (binary, version 2): the sequence and the fonts it loads."""
    head = bytes([0, 1, 0, 0]) + struct.pack('<I', 0x4F534551) + struct.pack('<I', 2) + b'\xef\xbe\xad\xde' * 2
    head += b'\0' * (0x40 - len(head))
    body = struct.pack('<I', len(seq)) + seq
    body += bytes([seq_number & 0xFF, 2, 2]) + struct.pack('<I', len(fonts)) + bytes(fonts)
    return head + body


if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    seq, info = build(src, loop='--noloop' not in sys.argv)
    open(dst, 'wb').write(seq)
    print(info)


def pack_music(mid_path, out_o2r, name='Temple of Light', pack='Ganondorf'):
    """The sequence in a mod archive of its own, where the game finds custom music (custom/music/<pack>/<name>_bgm):
    it is given a sequence number of its own as the game starts, and shows in the audio editor."""
    import zipfile
    seq, info = build(mid_path)
    with zipfile.ZipFile(out_o2r, 'w', zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(f'custom/music/{pack}/{name}_bgm', seq_resource(seq))
    return info
