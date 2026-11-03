"""Input/staging tests and the real frontend with optional packs absent."""
import csv,importlib.util,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('builder',ROOT/'tools/build_from_rom.py');b=importlib.util.module_from_spec(s);s.loader.exec_module(b)
with tempfile.TemporaryDirectory() as temporary:
 t=Path(temporary)
 bad=t/'bad rom.sfc';bad.write_bytes(bytes(524288))
 try:b.validate_rom(bad,'usa');raise AssertionError('Bad ROM accepted')
 except b.BuildError:pass
 staged=t/'project';digest=b.copy_source(ROOT,staged)
 assert len(digest)==64 and (staged/'data/save_icon/yoshi.icn').is_file()
 assert not list(staged.rglob('*.dat')) and not list(staged.rglob('*.sfc'))
 for p in [Path('/a/b path'),Path('/a/with,comma')]:
  m=next(csv.reader([b.mount(p,'/roms',True)]));assert m==['type=bind','source='+str(p),'target=/roms','readonly']
 cc=os.environ.get('CC','cc');obj=t/'frontend.o';binary=t/'launcher'
 subprocess.run([cc,'-O2','-std=gnu11','-I.','-Iinclude','-DSMW_LOOP_TEST','-DSMW_EMBEDDED_ASSETS','-Dmain=port_main','-c','src/ps2_main.c','-o',str(obj)],cwd=ROOT,check=True)
 subprocess.run([cc,'-O2','-std=gnu11','-Iinclude','-Itests','-DSMW_MOCK_NO_OPTIONAL_PACKS',str(obj),'tests/platform_mock.c','src/frame_pacer.c','src/audio_queue.c','-o',str(binary)],cwd=ROOT,check=True)
 r=subprocess.run([str(binary),'4','0','0','0','0'],cwd=t,check=True,capture_output=True,text=True)
 assert 'MODEL_RESULT' in r.stdout
print('PASS: malformed ROM rejected, code staging excludes all DAT/ROM files, mount paths quoted, actual frontend runs with both optional weak packs absent')
# Exercise the outer Docker orchestration with a contract-checking fake client.
# This verifies arguments/mounts/context isolation, not real container execution.
with tempfile.TemporaryDirectory() as temporary:
 import argparse,json
 t=Path(temporary);fake=t/'docker';rom=t/'contract-fixture.sfc';rom.write_bytes(b'contract fixture, not a game ROM')
 fake.write_text('''#!/usr/bin/env python3
import csv,json,sys,hashlib
from pathlib import Path
a=sys.argv[1:]
if a[0]=='info': print('contract-test-server')
elif a[0]=='build':
 p=Path(a[-1]);assert sorted(x.name for x in p.iterdir())==['Dockerfile'];print('context has Dockerfile only')
elif a[0]=='run':
 mounts=[]
 for i,x in enumerate(a):
  if x=='--mount':mounts.append(next(csv.reader([a[i+1]])))
 assert len(mounts)==3 and all('readonly' in m for m in mounts[:2])
 out=next(v.split('=',1)[1] for v in mounts[2] if v.startswith('source='));p=Path(out)
 files={}
 for ext in ['.iso','.elf']:
  name='SMWPS2_v022'+ext;data=b'contract-test-only';(p/name).write_bytes(data);files[name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 (p/'SMWPS2_v022_build.json').write_text(json.dumps({'version':'v022','inputs':{},'files':files,'contract_test_only':True}))
 print('read-only source and ROM mounts; separate writable output')
else: raise AssertionError(a)
''');fake.chmod(0o700)
 previous=os.environ.get('PATH','');os.environ['PATH']=str(t)+os.pathsep+previous
 try:
  a=argparse.Namespace(rom=rom,japanese_rom=None,super_system_rom=None,backend='docker',output=t/'output with, comma',jobs=2)
  from unittest.mock import patch
  with patch.object(b,'validate_rom',return_value=(b'contract fixture',{'sha1':b.HASHES['usa'],'copier_header_removed':False})):b.build(a)
  r=json.loads((a.output/'SMWPS2_v022_build.json').read_text());assert r['inputs']['usa']['sha1']==b.HASHES['usa']
  (a.output/'SMWPS2_v022.iso').write_bytes(b'corrupted')
  try:b.verify_outputs(a.output);raise AssertionError('Corrupt output accepted')
  except b.BuildError:pass
 finally:os.environ['PATH']=previous
 print('PASS: Docker command contract, isolated image context, read-only mounts and output paths with spaces/commas (mocked; not a container run)')

# Drive the actual dialog flow without a display server. Validation of real ROM
# bytes is separate; this checks .ic1 discovery and recovery after a wrong pick.
with tempfile.TemporaryDirectory() as temporary:
 import argparse,sys,types
 from unittest.mock import Mock,patch
 t=Path(temporary);usa=t/'USA.sfc';wrong=t/'nss-r__ic3__mw.ic3';right=t/b.NSS_PROGRAM_NAME
 wrong.write_bytes(bytes(32768))
 for name in ['nss-r__ic3__mw.ic3','security.prm']:
  path=t/name
  if not path.exists():path.write_bytes(bytes(16))
  try:b.validate_rom(path,'super_system');raise AssertionError('Companion accepted as program')
  except b.BuildError as error:
   assert b.NSS_PROGRAM_NAME in str(error) and 'not needed' in str(error)
 tk=types.ModuleType('tkinter');window=Mock();tk.Tk=Mock(return_value=window)
 tk.filedialog=types.SimpleNamespace(askopenfilename=Mock(side_effect=[str(usa),str(wrong),str(right)]),askdirectory=Mock(return_value=str(t/'output')))
 tk.messagebox=types.SimpleNamespace(askyesno=Mock(side_effect=[False,True]),showerror=Mock())
 real_validate=b.validate_rom
 def validate_dialog(path,kind):
  if path in [usa,right]:return b'fixture',{}
  return real_validate(path,kind)
 a=argparse.Namespace(rom=None,japanese_rom=None,super_system_rom=None,output=t)
 with patch.dict(sys.modules,{'tkinter':tk}),patch.object(b,'validate_rom',side_effect=validate_dialog):assert b.gui(a)
 assert a.super_system_rom==right and a.japanese_rom is None and tk.messagebox.showerror.call_count==1
 for call in tk.filedialog.askopenfilename.call_args_list[1:]:
  assert call.kwargs['initialfile']==b.NSS_PROGRAM_NAME
  assert '*.ic1' in call.kwargs['filetypes'][0][1]
 assert b.NSS_PROGRAM_NAME in tk.messagebox.askyesno.call_args_list[1].args[1]
 window.destroy.assert_called_once()
print('PASS: .ic1 picker, named program prompt, companion-file explanation and dialog retry (dialog API mocked)')
