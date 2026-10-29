"""Room display-list geometry with colours (vertex colour x mean texel colour), for checking camera shots."""
import struct, numpy as np, re
from otr import z, HASH, dl, vtx
_TEX={}
def texmean(path, tlut=None):
    key=(path,tlut)
    if key in _TEX: return _TEX[key]
    c=np.array([0.7,0.7,0.7,1.0])
    try:
        d=z.read(path); t,w,h,sz=struct.unpack_from('<IIII',d,0x40); px=d[0x50:0x50+sz]
        if t==2:
            v=np.frombuffer(px,'>u2').astype(int)
            c=np.array([((v>>11)&31).mean()/31,((v>>6)&31).mean()/31,((v>>1)&31).mean()/31,(v&1).mean()])
        elif t==1:
            a=np.frombuffer(px,np.uint8).reshape(-1,4)/255.; c=a.mean(0)
        elif t in (5,6):
            a=np.frombuffer(px,np.uint8); a=np.concatenate([a>>4,a&15])/15. if t==5 else a/255.
            c=np.array([a.mean()]*3+[1.0])
        elif t in (7,8,9):
            a=np.frombuffer(px,np.uint8)
            if t==7: i=(a>>5)/7.; al=((a>>4)&1); i=np.concatenate([i,((a>>1)&7)/7.]); al=np.concatenate([al,a&1])
            elif t==8: i=(a>>4)/15.; al=(a&15)/15.
            else: i=a[0::2]/255.; al=a[1::2]/255.
            c=np.array([i.mean()]*3+[al.mean()])
        elif t in (3,4) and tlut:
            pal=np.frombuffer(z.read(tlut)[0x50:],'>u2').astype(int)
            a=np.frombuffer(px,np.uint8)
            idx=np.concatenate([a>>4,a&15]) if t==3 else a
            idx=idx[idx<len(pal)]; v=pal[idx]
            c=np.array([((v>>11)&31).mean()/31,((v>>6)&31).mean()/31,((v>>1)&31).mean()/31,(v&1).mean()])
    except Exception as e: pass
    _TEX[key]=c; return c
def dl_tris(path, out, state):
    try: cmds=dl(path)
    except KeyError: return
    for c in cmds:
        op=c[0]
        if op==0x32:
            n=(c[1]>>12)&0xff; v0=((c[1]>>1)&0x7f)-n; off=c[2]//16
            p=HASH.get((c[3]<<32)|c[4])
            if not p: continue
            vs=vtx(p)
            for i in range(n): state['buf'][v0+i]=vs[off+i]
        elif op==0x20:
            state['img']=HASH.get((c[3]<<32)|c[4])
        elif op==0xF0:
            state['tlut']=state['img']
        elif op==0xFD:
            pass
        elif op in (0xF5,) and ((c[2]>>24)&7)==0 and state.get('img'):
            state['tex']=state['img']
        elif op==0xFA:
            w=c[2]; state['prim']=np.array([(w>>24)&255,(w>>16)&255,(w>>8)&255,w&255])/255.
        elif op in (0x05,0x06):
            ws=[c[1]] if op==0x05 else [c[1],c[2]]
            for w in ws:
                ids=[((w>>16)&0xff)//2,((w>>8)&0xff)//2,(w&0xff)//2]
                vv=[state['buf'][i] for i in ids]
                if None in vv: continue
                tc=texmean(state['tex'],state.get('tlut')) if state.get('tex') else np.array([1,1,1,1.])
                vc=np.array([np.mean([v[3] for v in vv]),np.mean([v[4] for v in vv]),np.mean([v[5] for v in vv])])/255.
                col=tc[:3]
                out.append((np.array([v[:3] for v in vv],float),col,tc[3]))
        elif op==0x31:
            sub=HASH.get((c[3]<<32)|c[4])
            if sub: dl_tris(sub,out,state)
def room_mesh(scene, rooms=None, opa_only=False):
    out=[]
    names=[n for n in z.namelist() if f'/{scene}_scene/{scene}_room_' in n and re.search(r'_room_\d+DL_',n)]
    for n in names:
        r=int(re.search(r'_room_(\d+)DL',n).group(1))
        if rooms is not None and r not in rooms: continue
        st={'buf':[None]*64}
        dl_tris(n,out,st)
    T=np.array([o[0] for o in out]); C=np.array([o[1] for o in out]); A=np.array([o[2] for o in out])
    return T,C,A
