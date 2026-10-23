import os
import sys, struct
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from dlparse import z,H,vtxres
def run(name, st, groups, depth=0):
    d=z.read(name); off=0x48
    while off+8<=len(d):
        w0,w1=struct.unpack_from('<II',d,off); op=w0>>24
        if op==0xda and (w1>>24)==0x0d: st['mtx']=(w1&0xffffff)//0x40
        elif op==0x32:
            n=(w0>>12)&0xff; dst=((w0>>1)&0x7f)-n
            h0,h1=struct.unpack_from('<II',d,off+8); vn=H.get((h0<<32)|h1)
            if vn:
                vs=vtxres(vn)[w1//16:w1//16+n]
                for i,v in enumerate(vs): st['buf'][dst+i]=(st['mtx'],v)
            off+=8
        elif op==0x20:
            h0,h1=struct.unpack_from('<II',d,off+8); st['tex']=H.get((h0<<32)|h1,'?').split('/')[-1]; off+=8
        elif op==0xfc: st['cc']='%08x%08x'%(w0,w1)
        elif op==0xfa: st['prim']='%08x'%w1
        elif op==0xfb: st['env']='%08x'%w1
        elif op in (0x05,0x06):
            idx=[((w0>>16)&0xff)//2,((w0>>8)&0xff)//2,(w0&0xff)//2]
            if op==0x06: idx+= [((w1>>16)&0xff)//2,((w1>>8)&0xff)//2,(w1&0xff)//2]
            key=(st['tex'],st['cc'],st.get('prim'),st.get('env'))
            for i in idx:
                m,v=st['buf'].get(i,(None,None))
                if v: groups.setdefault(key,{}).setdefault(m,[]).append(v)
        elif op==0x31:
            h0,h1=struct.unpack_from('<II',d,off+8)
            if H.get((h0<<32)|h1): run(H[(h0<<32)|h1],st,groups,depth+1)
            off+=8
        elif op==0xdf: break
        off+=8
for nm in sys.argv[1:]:
    g={}; run(nm,{'mtx':None,'buf':{},'tex':None,'cc':None},g)
    print('##',nm.split('/')[-1])
    for k,ms in g.items():
        for m,vs in ms.items():
            xs=[v[0] for v in vs]
            mn=[min(p[i] for p in xs) for i in range(3)]; mx=[max(p[i] for p in xs) for i in range(3)]
            print(k[0],k[1][:8],k[2],k[3],'mtx',m,'n',len(vs),mn,mx)
