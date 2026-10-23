import os
import sys, math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw
def rot(az, el):
    # camera: azimuth around up axis (0 = looking at the face from the front), elevation
    a=math.radians(az); e=math.radians(el)
    # world (F,U,S) -> camera (x right, y up, z toward viewer)
    fwd=np.array([math.cos(a)*math.cos(e), math.sin(e), math.sin(a)*math.cos(e)])  # from target toward camera
    up=np.array([0,1,0.]); right=np.cross(up,fwd); right/=np.linalg.norm(right); up2=np.cross(fwd,right)
    return np.stack([right,up2,fwd])
def render(tris, az, el, size=360, extent=1900, center=(350,700,0), light=(0.4,0.8,0.5)):
    M=rot(az,el); L=np.array(light)/np.linalg.norm(light)
    img=np.full((size,size,3),255,np.float32); zb=np.full((size,size),-1e9)
    c=np.array(center,float); s=size/extent
    for col,tri in tris:
        P=(np.array(tri,float)-c)@M.T
        n=np.cross(P[1]-P[0],P[2]-P[0]); nl=np.linalg.norm(n)
        if nl==0: continue
        n/=nl; shade=0.45+0.55*abs(n@ (M@L))
        X=P[:,0]*s+size/2; Y=size/2-P[:,1]*s; Z=P[:,2]
        x0,x1=int(max(0,math.floor(X.min()))),int(min(size-1,math.ceil(X.max())))
        y0,y1=int(max(0,math.floor(Y.min()))),int(min(size-1,math.ceil(Y.max())))
        if x1<x0 or y1<y0: continue
        xs,ys=np.meshgrid(np.arange(x0,x1+1)+0.5,np.arange(y0,y1+1)+0.5)
        d=(Y[1]-Y[2])*(X[0]-X[2])+(X[2]-X[1])*(Y[0]-Y[2])
        if abs(d)<1e-9: continue
        w0=((Y[1]-Y[2])*(xs-X[2])+(X[2]-X[1])*(ys-Y[2]))/d
        w1=((Y[2]-Y[0])*(xs-X[2])+(X[0]-X[2])*(ys-Y[2]))/d
        w2=1-w0-w1
        m=(w0>=0)&(w1>=0)&(w2>=0)
        z=w0*Z[0]+w1*Z[1]+w2*Z[2]
        sub=zb[y0:y1+1,x0:x1+1]; upd=m&(z>sub)
        sub[upd]=z[upd]
        im=img[y0:y1+1,x0:x1+1]; im[upd]=np.array(col,np.float32)*shade
    return Image.fromarray(np.clip(img,0,255).astype(np.uint8))
