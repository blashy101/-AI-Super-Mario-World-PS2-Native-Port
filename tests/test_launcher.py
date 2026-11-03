"""Exercise the production frontend's edition/language/resume controller flow."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
cc=os.environ.get('CC','cc')
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp);obj=tmp/'frontend.o';binary=tmp/'launcher'
    subprocess.run([cc,'-O2','-std=gnu11','-I.','-Iinclude','-DSMW_LOOP_TEST',
        '-DSMW_EMBEDDED_ASSETS','-Dmain=port_main','-c','src/ps2_main.c','-o',str(obj)],cwd=ROOT,check=True)
    subprocess.run([cc,'-O2','-std=gnu11','-Iinclude','-Itests',str(obj),'tests/platform_mock.c',
        'src/frame_pacer.c','src/audio_queue.c','-o',str(binary)],cwd=ROOT,check=True)
    r=subprocess.run([str(binary),'4','0','0','0','1'],cwd=tmp,capture_output=True,text=True)
    print('\n'.join(s for s in r.stdout.splitlines() if s.startswith('LAUNCHER_RESULT')))
    if r.returncode: print(r.stdout[-1500:]+r.stderr[-1000:])
    assert r.returncode==0
    assert 'sessions=EAJAE' in r.stdout
print('PASS: actual launcher buttons, quick return chord, Original -> NSS -> Japanese -> NSS -> English, Circle resume and arcade language lock')
