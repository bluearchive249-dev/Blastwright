import math, random, numpy as np
from PIL import Image, ImageDraw, ImageFilter
N=32; K=8; W=N*K
def finish(img):
    small=img.resize((N,N),Image.LANCZOS)
    a=np.array(small).astype(float)
    # a crisp dark rim so it reads on the button
    al=a[...,3]/255
    solid=(al>0.55).astype(float)
    pad=np.pad(solid,1)
    near=np.zeros((N,N))
    for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
        near=np.maximum(near,pad[1+dy:1+dy+N,1+dx:1+dx+N])
    rim=near*(al<0.35)*0.8   # a dark rim only around the solid shape, never over a glow
    al=al*(al>0.06)          # no faint haze out to the icon's edges
    out=np.zeros_like(a)
    out[...,:3]=a[...,:3]*al[...,None]+np.array([20,10,30])*rim[...,None]
    out[...,3]=np.clip(al+rim,0,1)*255
    out[...,:3]=out[...,:3]/np.maximum(out[...,3:4]/255,1e-3)
    return Image.fromarray(np.clip(out,0,255).astype(np.uint8),'RGBA')
def glow(draw_fn, blur):
    base=Image.new('RGBA',(W,W),(0,0,0,0)); draw_fn(ImageDraw.Draw(base))
    return base.filter(ImageFilter.GaussianBlur(blur))
def comp(*layers):
    out=Image.new('RGBA',(W,W),(0,0,0,0))
    for l in layers: out=Image.alpha_composite(out,l)
    return out
c=W/2
# 1. A ball of thunderous light
random.seed(4)
def bolts(d,width,col):
    for k in range(6):
        a=k*math.pi/3+0.35
        x,y=c+math.cos(a)*W*0.18,c+math.sin(a)*W*0.18
        pts=[(x,y)]
        for s in range(5):
            r=W*(0.18+0.055*(s+1)); a2=a+random.uniform(-0.35,0.35)
            pts.append((c+math.cos(a2)*r,c+math.sin(a2)*r))
        d.line(pts,fill=col,width=width,joint='curve')
halo=glow(lambda d:d.ellipse([c-W*0.36]*2+[c+W*0.36]*2,fill=(255,200,80,150)),W*0.06)
boltglow=glow(lambda d:bolts(d,int(W*0.06),(255,220,90,255)),W*0.015)
random.seed(4)
boltcore=glow(lambda d:bolts(d,int(W*0.028),(255,255,235,255)),1)
ballg=Image.new('RGBA',(W,W),(0,0,0,0)); px=ballg.load()
R=W*0.24
for yy in range(W):
    for xx in range(W):
        r=math.hypot(xx-c+W*0.03,yy-c+W*0.03)/R
        rr=math.hypot(xx-c,yy-c)/R
        if rr<=1:
            t=min(r,1)
            col=(255,int(255-90*t**1.5),int(235-200*t),255)
            px[xx,yy]=col
ball=ballg.filter(ImageFilter.GaussianBlur(2))
light=finish(comp(halo,boltglow,ball,boltcore))
# 2. The trident's spearhead (diagonal, point to the top right)
tri=Image.new('RGBA',(W,W),(0,0,0,0))
d=ImageDraw.Draw(tri)
def P(u,v):  # u along the shaft (0 bottom-left .. 1 top-right), v across
    ux,uy=1/math.sqrt(2),-1/math.sqrt(2); vx,vy=1/math.sqrt(2),1/math.sqrt(2)
    o=(W*0.12,W*0.88); L=W*1.08
    return (o[0]+ux*u*L+vx*v*L, o[1]+uy*u*L+vy*v*L)
steel=(150,160,190,255); dark=(40,40,70,255); edge=(225,235,255,255); gold=(220,170,60,255); goldd=(140,95,30,255)
d.polygon([P(0.0,-0.035),P(0.0,0.035),P(0.42,0.03),P(0.42,-0.03)],fill=(70,45,60,255))       # the shaft
d.polygon([P(0.38,-0.09),P(0.38,0.09),P(0.47,0.09),P(0.47,-0.09)],fill=gold)               # the collar
d.polygon([P(0.40,-0.09),P(0.40,0.09),P(0.42,0.09),P(0.42,-0.09)],fill=goldd)
d.polygon([P(0.47,-0.05),P(0.47,0.05),P(0.80,0.03),P(0.98,0.0),P(0.80,-0.03)],fill=steel)   # the middle blade
d.polygon([P(0.50,0.0),P(0.98,0.0),P(0.80,-0.03),P(0.47,-0.05)],fill=edge)                  # its bright edge
for s in (1,-1):                                                                              # the side prongs, curving out
    pts=[P(0.46,s*0.08),P(0.56,s*0.20),P(0.74,s*0.25),P(0.84,s*0.22),P(0.70,s*0.17),P(0.58,s*0.11),P(0.50,s*0.05)]
    d.polygon(pts,fill=steel)
    d.line([P(0.56,s*0.20),P(0.74,s*0.25),P(0.84,s*0.22)],fill=edge,width=int(W*0.02))
d.polygon([P(0.44,-0.03),P(0.44,0.03),P(0.53,0.0)],fill=(180,30,60,255))                    # a red gem at the root
trig=glow(lambda dd:dd.ellipse([P(0.70,0)[0]-W*0.3,P(0.70,0)[1]-W*0.3,P(0.70,0)[0]+W*0.3,P(0.70,0)[1]+W*0.3],fill=(150,90,255,70)),W*0.08)
trident=finish(tri)
# 3. A lethal purple dark aura: a vortex of violet flame about a black heart
aur=Image.new('RGBA',(W,W),(0,0,0,0)); px=aur.load()
for yy in range(W):
    for xx in range(W):
        dx,dy=xx-c,yy-c; r=math.hypot(dx,dy)/(W*0.46); a=math.atan2(dy,dx)
        if r>1: continue
        sw=math.sin(3*a+r*9.0)              # three arms, wound in
        arm=max(0,sw)**1.6
        lum=arm*(1-r)**0.6+0.25*(1-r)
        core=max(0,1-r/0.22)
        R_=int(min(255,90+170*lum)); G_=int(min(255,20+60*lum**2)); B_=int(min(255,140+115*lum))
        al=min(1,lum*1.6+0.15*(1-r))*(1-core*0.0)
        if core>0:
            R_,G_,B_=int(R_*(1-core)+10*core),int(G_*(1-core)),int(B_*(1-core)+25*core); al=max(al,core)
        px[xx,yy]=(R_,G_,B_,int(255*al))
aur=aur.filter(ImageFilter.GaussianBlur(1.5))
spark=Image.new('RGBA',(W,W),(0,0,0,0)); d=ImageDraw.Draw(spark); random.seed(9)
for i in range(7):
    a=random.uniform(0,6.28); r=W*random.uniform(0.25,0.42); x,y=c+math.cos(a)*r,c+math.sin(a)*r; s=W*0.022
    d.ellipse([x-s,y-s,x+s,y+s],fill=(240,190,255,255))
aura=finish(comp(aur,spark))
icons={'Light':light,'Trident':trident,'Aura':aura}
# preview, each on a B button look-alike, big
prev=Image.new('RGB',(3*200+40,240),(30,30,36))
for i,(n,im) in enumerate(icons.items()):
    btn=Image.new('RGBA',(180,180),(0,0,0,0)); bd=ImageDraw.Draw(btn); bd.ellipse([0,0,179,179],fill=(70,190,90,255))
    big=im.resize((128,128),Image.NEAREST); btn.alpha_composite(big,(26,26))
    prev.paste(btn,(20+i*200,20),btn)
    ImageDraw.Draw(prev).text((20+i*200+70,210),n,fill=(230,230,230))
prev.save('b_icons_preview.png')
# C
out=['// SoH: "Play as Ganondorf", his B button\'s icons (32x32 RGBA32), drawn for this mod: a ball of thunderous light,',
     '// the trident\'s spearhead, and his dark aura. Generated by soh/tools/gdf_hud/b_icons.py; no game data.',
     '#include <libultraship/libultra.h>','#include "macros.h"','']
for n,im in icons.items():
    b=im.tobytes()
    out.append('u8 gGdfBIcon%sTex[32 * 32 * 4] ALIGNED8 = {'%n)
    for i in range(0,len(b),24): out.append('    '+', '.join('0x%02X'%v for v in b[i:i+24])+',')
    out.append('};'); out.append('')
open('GanondorfHudIcons.c','w').write('\n'.join(out))
print('ok')
