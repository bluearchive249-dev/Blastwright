import os
import sys, math, struct
sys.argv=['x']; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from dlparse import z,H,vtxres
import numpy as np, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def tris(name, st, out):
    d=z.read(name); off=0x48
    while off+8<=len(d):
        w0,w1=struct.unpack_from('<II',d,off); op=w0>>24
        if op==0xda and (w1>>24)==0x0d: st['mtx']=(w1&0xffffff)//0x40
        elif op==0x32:
            n=(w0>>12)&0xff; dst=((w0>>1)&0x7f)-n
            h0,h1=struct.unpack_from('<II',d,off+8); vn=H.get((h0<<32)|h1)
            if vn:
                for i,v in enumerate(vtxres(vn)[w1//16:w1//16+n]): st['buf'][dst+i]=(st['mtx'],v[0])
            off+=8
        elif op==0x20:
            h0,h1=struct.unpack_from('<II',d,off+8); st['tex']=H.get((h0<<32)|h1,'?').split('/')[-1]; off+=8
        elif op in (5,6):
            for ws in ([w0] if op==5 else [w0,w1]):
                idx=[((ws>>16)&0xff)//2,((ws>>8)&0xff)//2,(ws&0xff)//2]
                vs=[st['buf'].get(i) for i in idx]
                if all(v and v[0]==7 for v in vs): out.append((st['tex'],[(v[1][0],-v[1][1],v[1][2]) for v in vs]))
        elif op==0x31:
            h0,h1=struct.unpack_from('<II',d,off+8)
            if H.get((h0<<32)|h1): tris(H[(h0<<32)|h1],st,out)
            off+=8
        elif op==0xdf: break
        off+=8
    return out

D=math.radians
def norm(v): v=np.array(v,float); return v/np.linalg.norm(v)
def slerp(a,b,t):
    ang=math.acos(max(-1,min(1,a@b)))
    if ang<1e-4: return a
    return (math.sin((1-t)*ang)*a+math.sin(t*ang)*b)/math.sin(ang)
def brim(sh,phi):
    tilt,op=D(sh['tilt']),D(sh['open'])
    ax=np.array([math.cos(tilt),math.sin(tilt),0]); up=np.array([-math.sin(tilt),math.cos(tilt),0])
    v=math.cos(op)*ax+math.sin(op)*math.sin(phi)*up; v[2]=-math.sin(op)*math.cos(phi); return norm(v)
def around(phi,el):
    psi=D(-110+220*phi/math.pi); el=D(el)
    return norm([-math.cos(psi)*math.cos(el), math.sin(el), math.sin(psi)*math.cos(el)])
def shell(sh,phi,t):
    b=brim(sh,phi); e=around(phi,-35); o=around(phi,65)
    mid=norm(b+e); via=norm(mid+(o-mid)*math.sin(phi))
    a1=math.acos(b@via); a2=math.acos(via@e); s=t*(a1+a2)
    return slerp(b,via,s/a1) if s<=a1 else slerp(via,e,(s-a1)/a2)
def grid(sh,size=1.03):
    P=np.zeros((9,12,3))
    for c in range(12):
        phi=c*math.pi/11
        for r in range(9):
            d=shell(sh,phi,r/8)
            P[r,c]=[sh['C'][0]+d[0]*sh['R'][0]*size, sh['C'][1]+d[1]*sh['R'][1]*size, d[2]*sh['R'][2]*size]
    return P
COL={'gLinkHairTex':'#e8c84a','gLinkAdultSkirtAndHatTex':'#2f9e3a','gLinkTunic3Tex':'#2f9e3a','gLinkAdultHeadTLUT':'#f2c9a0','gLinkChildSkinTLUT':'#f2c9a0','gLinkAdultEarTex':'#f2c9a0','gLinkChildEarTex':'#f2c9a0'}
shapes={'adult':dict(dl='objects/object_link_boy/gLinkAdultHeadNearDL',C=(280,700),R=(850,850,860),tilt=-25,open=48),
        'child':dict(dl='objects/object_link_child/gLinkChildHeadNearDL',C=(160,540),R=(800,780,720),tilt=-20,open=50)}
views=[('Front',0,0),('Side',0,90),('Back 3/4',15,215)]
fig=plt.figure(figsize=(15,10),facecolor='white')
for row,(age,sh) in enumerate(shapes.items()):
    T=tris(sh['dl'],{'mtx':None,'buf':{}, 'tex':None},[])
    P=grid(sh)
    hood=[]
    for r in range(8):
        for c in range(11):
            hood.append([P[r,c],P[r,c+1],P[r+1,c+1],P[r+1,c]])
    for col,(title,el,az) in enumerate(views):
        ax=fig.add_subplot(2,3,row*3+col+1,projection='3d',proj_type='ortho')
        # map (F,U,S) -> plot (x=S, y=F, z=U) so "front" view looks along -F
        head=[[(p[2],p[0],p[1]) for p in tri] for tex,tri in T]
        ax.add_collection3d(Poly3DCollection(head,facecolors=[COL.get(t,'#c99a6a') for t,_ in T],edgecolors='none',alpha=1.0))
        hq=[[(p[2],p[0],p[1]) for p in q] for q in hood]
        ax.add_collection3d(Poly3DCollection(hq,facecolors='#3a2448',edgecolors='#7a5a90',linewidths=0.3,alpha=0.82))
        ax.set_xlim(-1000,1000); ax.set_ylim(-800,1400); ax.set_zlim(-500,1700)
        ax.set_box_aspect((2000,2200,2200)); ax.view_init(elev=el, azim=-90+az if title!='Front' else 90)
        ax.set_axis_off(); ax.set_title(f'{age} — {title}')
plt.tight_layout()
out='hood_preview.png'
plt.savefig(out,dpi=90); print(out)
