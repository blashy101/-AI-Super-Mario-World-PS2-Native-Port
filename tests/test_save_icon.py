"""Check actual installer repair behavior and independently decode OSD files."""
from pathlib import Path
import math
import os
import struct
import subprocess
import tempfile
import unicodedata

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    driver=tmp/'driver.c'
    driver.write_text('#include "save_icon.h"\nint main(int n,char **v){return n!=2 || !SmwSaveIcon_Ensure(v[1]);}\n')
    binary=tmp/'install'
    subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-Iinclude',str(driver),'src/save_icon.c','-o',str(binary)],cwd=ROOT,check=True)
    card=tmp/'SMWPS2';card.mkdir()
    journals={name:bytes((i*17+31)%256 for i in range(2068)) for name in ('smw.srm.0','smw.srm.1','smw.srm')}
    for name,data in journals.items():(card/name).write_bytes(data)
    def install():subprocess.run([str(binary),str(card)],check=True)
    install()
    metadata={name:(ROOT/'data/save_icon'/name).read_bytes() for name in ('icon.sys','yoshi.icn')}
    for name,data in metadata.items():
        assert (card/name).read_bytes()==data
        os.utime(card/name,ns=(123456789000000,123456789000000))
    install()
    assert all((card/name).stat().st_mtime_ns==123456789000000 for name in metadata),'rewrote unchanged metadata'
    (card/'icon.sys').write_bytes(b'corrupt')
    (card/'yoshi.icn').write_bytes(metadata['yoshi.icn']+b'old extra data')
    install()
    assert all((card/name).read_bytes()==data for name,data in metadata.items())
    assert all((card/name).read_bytes()==data for name,data in journals.items()),'save progress changed'
    assert subprocess.run([str(binary),str(card/'smw.srm')]).returncode!=0
    b=metadata['icon.sys']
    assert len(b)==964 and b[:4]==b'PS2D'
    assert struct.unpack_from('<H',b,6)[0]==22
    title=b[192:260].split(b'\0',1)[0].decode('shift_jis')
    assert unicodedata.normalize('NFKC',title)=='Super Mario World'
    assert all(b[p:p+64].split(b'\0',1)[0]==b'yoshi.icn' for p in (260,324,388))
    for p in range(80,192,4):assert math.isfinite(struct.unpack_from('<f',b,p)[0])
    b=metadata['yoshi.icn']
    version,shapes,attributes,backface,count=struct.unpack_from('<IIIfI',b)
    assert version==0x10000 and shapes==1 and attributes==15 and backface==1 and count==168*3
    points=[]
    for p in range(20,20+24*count,24):
        vertex=struct.unpack_from('<8h2h4B',b,p)
        points.append(tuple(v/1024 for v in vertex[:3]))
        assert abs(sum((v/4096)**2 for v in vertex[4:7])-1)<0.001
        assert all(0<=c<=128 for c in vertex[10:])
    for i in range(0,count,3):
        a,c,d=points[i:i+3];u=[c[j]-a[j] for j in range(3)];v=[d[j]-a[j] for j in range(3)]
        cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        assert sum(cross[j]*(a[j]+c[j]+d[j]) for j in range(3))>0,'inverted/degenerate face'
    p=20+24*count
    seq,frames,speed,offset,used,shape,keys,frame,weight=struct.unpack_from('<IIfIIIIff',b,p)
    assert (seq,frames,speed,offset,used,shape,keys,frame,weight)==(1,1,1,0,1,0,1,0,1)
    p+=36
    size=struct.unpack_from('<I',b,p)[0];p+=4;end=p+size;texture=[]
    while p<end:
        run=struct.unpack_from('<h',b,p)[0];p+=2
        if run>0:
            value=struct.unpack_from('<H',b,p)[0];p+=2;texture.extend([value]*run)
        else:
            texture.extend(struct.unpack_from('<'+'H'*(-run),b,p));p-=run*2
    assert p==end==len(b) and texture==[0xffff]*16384
print('PASS: Browser title, 168 outward egg triangles, normals, animation and texture; existing journals untouched; repair/idempotence/error paths')
