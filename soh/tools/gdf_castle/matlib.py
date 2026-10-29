"""Materials from existing room display lists: for each texture, the exact command run that sets it up (from G_SETTIMG
up to the first vertex load), plus its size, format and mean colour. Our generated rooms reuse these runs verbatim, so
the texture formats, wraps and combiners are the game's own."""
import struct, re, numpy as np
from otr import z, HASH, dl
from roommesh import texmean
def raw_cmds(path):
    d=z.read(path); p=0x48; out=[]
    while p+8<=len(d):
        w0,w1=struct.unpack_from('<II',d,p); op=w0>>24
        n=16 if op in (0x20,0x31,0x32,0x33,0x35,0x36,0x42) else 8
        out.append(d[p:p+n]); p+=n
        if op==0xDF: break
    return out
def materials(scene):
    mats={}
    names=[n for n in z.namelist() if f'/{scene}_scene/{scene}_room_' in n and re.search(r'_room_\d+DL_',n)]
    for n in names:
        cmds=raw_cmds(n); i=0
        while i<len(cmds):
            op=cmds[i][3]
            if op==0x20:
                h=struct.unpack_from('<II',cmds[i],8); tex=HASH.get((h[0]<<32)|h[1])
                run=[]; j=i
                while j<len(cmds) and cmds[j][3] not in (0x32,0x01,0x06,0x05,0xDF,0x31):
                    run.append(cmds[j]); j+=1
                if tex and tex not in mats:
                    # size from the render tile's settilesize (0xF2)
                    w=h_=0; fmt=None
                    for c in run:
                        if c[3]==0xF2:
                            w1=struct.unpack_from('<I',c,4)[0]; w=((w1>>12)&0xFFF)//4+1; h_=(w1&0xFFF)//4+1
                        if c[3]==0xF5 and fmt is None: fmt=(struct.unpack_from('<I',c,0)[0]>>19)&0x1F
                    tl=None
                    mats[tex]=dict(run=run,w=w,h=h_,fmt=fmt,src=n)
                i=j
            else: i+=1
    return mats
