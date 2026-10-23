import os
import sys, math
sys.argv=['x']; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'hoodrender.py')).read().split("COL={")[0])
import numpy as np
from zrender import render
from PIL import Image, ImageDraw

def head_verts(dl):
    T=tris(dl,{'mtx':None,'buf':{},'tex':None},[])
    V=np.array(sorted(set(p for _,tri in T for p in tri)),float)
    return T,V

def bake(sh, V, cone=22, margin_side=80, margin_top=130, smooth=4, overhang=140):
    C=np.array([sh['C'][0],sh['C'][1],0.0]); R=np.array(sh['R'],float)
    rel=V-C; reln=rel/np.linalg.norm(rel,axis=1)[:,None]
    P=np.zeros((9,12,3)); rad=np.zeros((9,12)); dirs=np.zeros((9,12,3)); req_store=np.zeros((9,12))
    for r in range(9):
        for c in range(12):
            d=shell(sh,c*math.pi/11,r/8)
            D=norm(d*R); dirs[r,c]=D
            ang=np.degrees(np.arccos(np.clip(reln@D,-1,1)))
            m=ang<cone
            # distance along D needed to clear every vertex in the cone
            need=(rel[m]@D) if m.any() else np.array([0.0])
            base=np.linalg.norm(d*R)*0.75
            up=max(0.0, D[1])  # more room on top of the head
            rad[r,c]=max(need.max()+margin_side+(margin_top-margin_side)*up, base*0.6)
            req_store[r,c]=need.max()+margin_side*0.6
    # smooth (max of neighbours first so we never cut into the head, then blur)
    for _ in range(smooth):
        mx=rad.copy()
        for r in range(9):
            for c in range(12):
                nb=[rad[rr,cc] for rr in (r-1,r,r+1) for cc in (c-1,c,c+1) if 0<=rr<9 and 0<=cc<12]
                mx[r,c]=0.5*rad[r,c]+0.5*np.mean(nb)
        rad=np.maximum(mx, req_store)
    for r in range(9):
        for c in range(12):
            P[r,c]=C+dirs[r,c]*rad[r,c]
    # brim overhang: pull the front edge forward and down along the face direction
    t=math.radians(sh['tilt']); fa=np.array([math.cos(t),math.sin(t),0.0])
    for r,w in ((0,1.0),(1,0.45),(2,0.15)):
        for c in range(12):
            P[r,c]+=fa*overhang*w*math.sin(c*math.pi/11)**0.5
    return P

def shoulders(age):
    # rough collar semicircle in head space for previews only (the game uses the real cape roots)
    k=1.0 if age=='adult' else 0.78
    out=[]
    for c in range(12):
        t=c*math.pi/11
        out.append([(-120-math.sin(t)*700)*k, -950*k, -math.cos(t)*820*k])
    return np.array(out)

def full(P, age, bulge=100):
    G=np.zeros((12,12,3)); G[:9]=P; Rt=shoulders(age)
    for i,row in enumerate((9,10,11)):
        t=(i+1)/3
        for c in range(12):
            p=P[8,c]+(Rt[c]-P[8,c])*t
            if row!=11:
                o=np.array([p[0]-(-120),0,p[2]]); o/=max(np.linalg.norm(o),1e-6); p=p+o*bulge*math.sin(t*math.pi)
            G[row,c]=p
    return G

RGB={'gLinkHairTex':(232,200,74),'gLinkAdultSkirtAndHatTex':(220,160,40),'gLinkTunic3Tex':(220,160,40)}
HOOD=(92,62,118); CAPE=(70,46,92)
SH={'adult':dict(dl='objects/object_link_boy/gLinkAdultHeadNearDL',C=(280,700),R=(850,850,860),tilt=-25,open=48),
    'child':dict(dl='objects/object_link_child/gLinkChildHeadNearDL',C=(160,540),R=(800,780,720),tilt=-20,open=50)}

def sheet(path, params=None):
    views=[('front',0,5),('3/4 front',35,10),('side',90,0),('3/4 back',150,15),('back',180,10)]
    W=260; img=Image.new('RGB',(W*5,(W+20)*2),'white'); d=ImageDraw.Draw(img); baked={}
    for i,(age,sh) in enumerate(SH.items()):
        T,V=head_verts(sh['dl'])
        P=bake(sh,V,**(params or {})); G=full(P,age); baked[age]=P
        sc=[(RGB.get(t,(242,201,160)),tri) for t,tri in T]
        for r in range(11):
            for c in range(11):
                col=HOOD
                sc.append((col,[G[r,c],G[r,c+1],G[r+1,c+1]])); sc.append((col,[G[r,c],G[r+1,c+1],G[r+1,c]]))
        # a stub of cape below the collar
        Rt=shoulders(age)
        for c in range(11):
            a,b=Rt[c],Rt[c+1]; a2=a+np.array([-60,-900,0])*(1 if age=='adult' else .78); b2=b+np.array([-60,-900,0])*(1 if age=='adult' else .78)
            sc.append((CAPE,[a,b,b2])); sc.append((CAPE,[a,b2,a2]))
        for j,(nm,az,el) in enumerate(views):
            im=render(sc,az,el,size=W,extent=3200,center=(sh['C'][0],sh['C'][1]-700,0))
            img.paste(im,(j*W,i*(W+20)+20)); d.text((j*W+6,i*(W+20)+4),f'{age} - {nm}',fill=(0,0,0))
    img.save(path); print(path)
    return baked
def closeups(path, params=None):
    W=320; views=[('front',0,0),('front low',0,-12),('3/4',30,0)]
    img=Image.new('RGB',(W*3,(W+20)*2),'white'); d=ImageDraw.Draw(img)
    for i,(age,sh) in enumerate(SH.items()):
        T,V=head_verts(sh['dl']); P=bake(sh,V,**(params or {}))
        sc=[(RGB.get(t,(242,201,160)),tri) for t,tri in T]
        for r in range(8):
            for c in range(11):
                sc.append((HOOD,[P[r,c],P[r,c+1],P[r+1,c+1]])); sc.append((HOOD,[P[r,c],P[r+1,c+1],P[r+1,c]]))
        for j,(nm,az,el) in enumerate(views):
            im=render(sc,az,el,size=W,extent=2000,center=(sh['C'][0],sh['C'][1],0))
            img.paste(im,(j*W,i*(W+20)+20)); d.text((j*W+6,i*(W+20)+4),f'{age} - {nm}',fill=(0,0,0))
    img.save(path); print(path)
if __name__=='__main__':
    closeups('hood_close.png')
    sheet('hood_v2.png')

def pierce_test(P, V, C):
    # triangles of the shell
    T=[]
    for r in range(8):
        for c in range(11):
            T.append((P[r,c],P[r,c+1],P[r+1,c+1])); T.append((P[r,c],P[r+1,c+1],P[r+1,c]))
    T=np.array(T); bad=[]
    for v in V:
        o=np.array(C,float); d=v-o; L=np.linalg.norm(d); d/=L
        e1=T[:,1]-T[:,0]; e2=T[:,2]-T[:,0]; h=np.cross(d,e2); a=np.einsum('ij,ij->i',e1,h)
        ok=np.abs(a)>1e-9; f=np.zeros_like(a); f[ok]=1/a[ok]
        s_=o-T[:,0]; u=f*np.einsum('ij,ij->i',s_,h); q=np.cross(s_,e1); w=f*(q@d); t=f*np.einsum('ij,ij->i',e2,q)
        hit=ok&(u>=0)&(w>=0)&(u+w<=1)&(t>0)
        if hit.any() and L>t[hit].min()+1: bad.append((tuple(v),round(L-t[hit].min())))
    return bad
