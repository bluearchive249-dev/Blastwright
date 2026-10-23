import os
import zipfile, struct, re, sys
SRC=open(os.environ.get('STRHASH64_CPP','../libultraship/src/ship/utils/StrHash64.cpp')).read()
tbl=[int(x,16) for x in re.findall(r'CONST64\((0x[0-9a-fA-F]+)\)',SRC)]
assert len(tbl)==256
M=(1<<64)-1
def crc64(s):
    c=M
    for b in s.encode():
        c=(tbl[((c>>56)^b)&0xff]^(c<<8))&M
    return c
z=zipfile.ZipFile(os.environ.get('OOT_O2R','oot.o2r'))
names=z.namelist()
H={crc64(n):n for n in names}
def vtxres(name):
    d=z.read(name); n=struct.unpack_from('<I',d,0x44)[0]
    out=[]
    for i in range(n):
        ob=struct.unpack_from('<3h',d,0x48+i*16); fl,=struct.unpack_from('<H',d,0x48+i*16+6)
        tc=struct.unpack_from('<2h',d,0x48+i*16+8); cn=struct.unpack_from('<4B',d,0x48+i*16+12)
        out.append((ob,tc,cn))
    return out
def parse(name, mtx=None, acc=None, depth=0):
    if acc is None: acc={}
    d=z.read(name); off=0x40
    # find start: skip ucode byte + padding until first 8-byte aligned command; heuristically start at 0x48
    off=0x48
    while off+8<=len(d):
        w0,w1=struct.unpack_from('<II',d,off); op=w0>>24
        if op==0xda and (w1>>24)==0x0d:
            mtx=(w1&0xffffff)//0x40
        elif op==0x32:
            n=(w0>>12)&0xff; dst=((w0>>1)&0x7f)-n
            h0,h1=struct.unpack_from('<II',d,off+8); h=(h0<<32)|h1
            vn=H.get(h); 
            if vn:
                vs=vtxres(vn)[w1//16:w1//16+n]
                acc.setdefault(mtx,[]).extend(vs)
            off+=8
        elif op==0x31:
            h0,h1=struct.unpack_from('<II',d,off+8); h=(h0<<32)|h1
            if H.get(h): parse(H[h],mtx,acc,depth+1)
            off+=8
        elif op==0xdf: break
        off+=8
    return acc
if __name__=='__main__':
    for nm in sys.argv[1:]:
        acc=parse(nm)
        for m,vs in sorted(acc.items(), key=lambda x: -1 if x[0] is None else x[0]):
            xs=[v[0] for v in vs]
            mn=[min(p[i] for p in xs) for i in range(3)]; mx=[max(p[i] for p in xs) for i in range(3)]
            print(nm.split('/')[-1], 'mtx',m,'n',len(vs),'min',mn,'max',mx)
