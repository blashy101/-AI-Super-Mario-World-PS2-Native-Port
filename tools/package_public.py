#!/usr/bin/env python3
"""Package source, empty native engine and Python tools; exclude ROM/asset packs."""
import argparse,hashlib,json,zipfile
from pathlib import Path
from portable_engine import locate_slots,native_fingerprint
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'dist/SMWPS2_v022_Build_Kit.zip');a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
 template=(ROOT/'release/engine/smw_engine.template').read_bytes();manifest=json.loads((ROOT/'release/engine/manifest.json').read_text())
 assert hashlib.sha256(template).hexdigest()==manifest['engine_sha256']
 assert native_fingerprint(ROOT)==manifest['native_source_sha256']
 slots=locate_slots(template);assert slots==manifest['slots']
 assert all(not any(template[s['offset']:s['offset']+s['capacity']]) for s in slots.values()),'Nonempty engine assets cannot be distributed'
 paths=[]
 for directory in ['src','include','vendor','tools','tests','licenses','release','.github']:
  for f in (ROOT/directory).rglob('*'):
   if not f.is_file() or any(s in {'build','dist','__pycache__','.git'} for s in f.relative_to(ROOT).parts):continue
   if f.suffix.lower() in {'.dat','.sfc','.smc','.srm','.iso','.elf','.o','.pyc','.zip','.png','.ppm','.jpg','.ic1','.ic3','.prm','.rom','.bin'}:continue
   paths.append((f,f.relative_to(ROOT).as_posix()))
 for name in ['Makefile','Makefile.ps2','Build_ISO.cmd','LICENSE','data/save_icon/icon.sys','data/save_icon/yoshi.icn','docs/SOURCES.md','docs/V016_VALIDATION.md','docs/V017_VALIDATION.md','docs/V017_1_VALIDATION.md','docs/V018_VALIDATION.md','docs/V019_VALIDATION.md','docs/V020_VALIDATION.md','docs/V021_VALIDATION.md','docs/V022_VALIDATION.md','docs/RELEASE_STATUS.md']:
  paths.append((ROOT/name,name))
 paths.append((ROOT/'release/README_PUBLIC.md','README.md'))
 with zipfile.ZipFile(a.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for f,n in paths:z.write(f,'SMWPS2_v022_Build_Kit/'+n)
 with zipfile.ZipFile(a.output) as z:
  assert z.testzip() is None
  assert not any(n.lower().endswith(('.dat','.sfc','.smc','.iso','.elf','.srm','.png','.ppm','.jpg','.ic1','.ic3','.prm','.rom','.bin')) for n in z.namelist())
 print('Verified public kit with empty engine slots:',a.output,len(paths),'files')
if __name__=='__main__':main()
