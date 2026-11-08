import numpy as np, math
# (needs the head mesh: run soh/tools/gdf_castle/otr.py's helpers on your own oot.o2r to make head10.npy, as the session did)
H=np.load('head10.npy',allow_pickle=True)   # limbs 10,11,12,13,14,15,16,17 in limb 10's frame (x fwd, y down, z side)
head=np.vstack([H[0],H[4],H[7]])             # jewel, upper hair, head (not the lips, jaw or the hair hanging down)
C=np.array([300.0,-420.0,0.0])              # the hood's middle
rel=head-C; dist=np.linalg.norm(rel,axis=1); dirs=rel/dist[:,None]
ROWS,COLS=9,12
MARGIN=170.0
def radius(d):
    cosang=dirs@d
    m=cosang>math.cos(math.radians(18))
    if not m.any(): return None
    return (dist[m]*cosang[m]).max()
grid=np.zeros((ROWS,COLS,3)); R=np.zeros((ROWS,COLS))
for c in range(COLS):
    phi=math.radians(-95+190*c/(COLS-1))           # around the face: -z side, over the top, +z side
    u=np.array([0.0,-math.cos(phi),math.sin(phi)])
    side=abs(math.sin(phi))
    th0=math.radians(36+30*side)                    # the brim: further back at the cheeks (the face stays open)
    th1=math.radians(226-36*side)                   # down the back of the head to the nape
    for r in range(ROWS):
        th=th0+(th1-th0)*r/(ROWS-1)
        d=math.cos(th)*np.array([1.0,0,0])+math.sin(th)*u
        rr=radius(d)
        if rr is None: rr=R[r-1,c] if r>0 else 900
        R[r,c]=rr+MARGIN
# smooth the radii a little (no dents) and give the brim an overhang
from scipy.ndimage import uniform_filter
Rs=np.maximum(R,uniform_filter(R,size=3,mode='nearest'))
Rs[0]+=300; Rs[1]+=150; Rs[2]+=50
for c in range(COLS):
    phi=math.radians(-95+190*c/(COLS-1)); u=np.array([0.0,-math.cos(phi),math.sin(phi)]); side=abs(math.sin(phi))
    th0=math.radians(36+30*side); th1=math.radians(226-36*side)
    for r in range(ROWS):
        th=th0+(th1-th0)*r/(ROWS-1)
        d=math.cos(th)*np.array([1.0,0,0])+math.sin(th)*u
        grid[r,c]=C+d*Rs[r,c]
# check: no head point outside the shell (ray test from the middle)
print('shell radius range',int(Rs.min()),int(Rs.max()))
print('row0',grid[0].astype(int).tolist()[:3],'...'); print('row8',grid[8].astype(int).tolist()[:3])
np.save('shell.npy',grid)
lines=['static const s16 sGdfHoodShell[GDF_HOOD_SHELL_ROWS][GDF_HOOD_COLS][3] = {']
for r in range(ROWS): lines.append('    { '+', '.join('{ %d, %d, %d }'%tuple(int(round(v)) for v in grid[r,c]) for c in range(COLS))+' },')
lines.append('};'); open('shell.inc','w').write('\n'.join(lines)+'\n')
# a picture: the shell's points over the head's, side and front
from PIL import Image, ImageDraw
im=Image.new('RGB',(900,450),(25,25,30)); d=ImageDraw.Draw(im)
def P(v,ox,ax): return (ox+v[ax[0]]*0.15, 225+v[ax[1]]*0.15)
allh=np.vstack(list(H))
for v in allh:
    x,y=P(v,250,(0,1)); d.rectangle([x,y,x+1,y+1],fill=(170,150,120)); x,y=P(v,670,(2,1)); d.rectangle([x,y,x+1,y+1],fill=(170,150,120))
for r in range(ROWS):
    for c in range(COLS):
        col=(255,80,80) if r==0 else (90,170,255)
        x,y=P(grid[r,c],250,(0,1)); d.ellipse([x-2,y-2,x+2,y+2],fill=col)
        x,y=P(grid[r,c],670,(2,1)); d.ellipse([x-2,y-2,x+2,y+2],fill=col)
d.text((200,10),'side (face to the right)',fill=(220,220,220)); d.text((620,10),'front',fill=(220,220,220))
im.save('shell_check.png')
