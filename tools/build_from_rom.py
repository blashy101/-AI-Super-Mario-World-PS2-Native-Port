#!/usr/bin/env python3
"""Compile a standalone PS2 ELF/ISO from local clean ROMs; no ROM is uploaded."""
import argparse,csv,hashlib,io,json,os,shutil,subprocess,sys,tempfile
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VERSION='v022';IMAGE='smwps2-builder:v022'
KIT_VERSION='v022'
BUILD_LIBS=ROOT/'vendor/build_py'
sys.path.insert(0,str(BUILD_LIBS))
HASHES={'usa':'6b47bb75d16514b6a476aa0c73a683a2a4c18765','japanese':'f977afabf24ed269d86366209a460450bbc37e76','super_system':'06a6efc246c6fdb83efab1d402d61d2179a84494'}
NSS_PROGRAM_NAME='nss-mw-0_prg.ic1'
NSS_FILETYPES=[('NSS SMW game program (*.ic1)', '*.ic1 *.IC1'),('SNES program ROM', '*.sfc *.smc *.bin'),('All files', '*.*')]
NSS_COMPANIONS={'e116f01342fcf359498ed8750741c139093b1fb2':'the 32 KiB companion ROM','6805cefb1856c3498b7a7a049c0d09858afac47c':'the 16-byte security PROM'}
class BuildError(Exception):pass
def validate_rom(path,kind):
    data=Path(path).read_bytes();header=len(data)==524800
    if header:data=data[512:]
    digest=hashlib.sha1(data).hexdigest()
    if len(data)!=524288 or digest!=HASHES[kind]:
        if kind=='super_system':
            companion=NSS_COMPANIONS.get(digest)
            if not companion and Path(path).name.lower() in {'nss-r__ic3__mw.ic3','security.prm'}:companion='a companion/security file'
            detail=f'This is {companion}, not the SMW game program. ' if companion else ''
            raise BuildError(detail+f'Select {NSS_PROGRAM_NAME} (512 KiB) for Super System. The .ic3 companion ROM and security.prm are not needed by this port. Expected SHA1 {HASHES[kind]}; got {digest}.')
        raise BuildError(f'{kind}: expected clean 512 KiB ROM SHA1 {HASHES[kind]}; got {digest}. Patched or other-region ROMs are unsupported.')
    return data,{'sha1':digest,'copier_header_removed':header}
def copy_source(source,dest):
    dest.mkdir(parents=True)
    def ignore(folder,names):return [n for n in names if n in {'build','dist','__pycache__','.git'} or Path(n).suffix.lower() in {'.dat','.sfc','.smc','.srm','.iso','.elf','.o','.pyc','.zip','.ic1','.ic3','.prm','.rom','.bin'}]
    for name in ['src','include','vendor','tools']:shutil.copytree(source/name,dest/name,ignore=ignore)
    shutil.copytree(source/'data/save_icon',dest/'data/save_icon')
    for name in ['Makefile','Makefile.ps2','LICENSE']:
        if (source/name).exists():shutil.copyfile(source/name,dest/name)
    h=hashlib.sha256()
    for p in sorted(dest.rglob('*'),key=lambda p:p.relative_to(dest).as_posix()):
        if p.is_file():h.update(p.relative_to(dest).as_posix().encode()+b'\0'+p.read_bytes())
    return h.hexdigest()
def mount(source,target,readonly=False):
    fields=['type=bind','source='+str(source),'target='+target]+(['readonly'] if readonly else [])
    out=io.StringIO();csv.writer(out,lineterminator='').writerow(fields);return out.getvalue()
def run(command,log,**kwargs):
    print('Running:',Path(command[0]).name,' '.join(str(s) for s in command[1:3]),flush=True)
    p=subprocess.Popen([str(s) for s in command],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace',**kwargs)
    for line in p.stdout:print(line,end='',flush=True);log.write(line);log.flush()
    if p.wait():raise BuildError(f'{Path(command[0]).name} failed with status {p.returncode}; see {log.name}')
def sdk_environment():
    e=os.environ.copy();sdk=e.get('PS2DEV','/usr/local/ps2dev')
    e['PS2DEV']=sdk;e.setdefault('PS2SDK',sdk+'/ps2sdk');e.setdefault('GSKIT',sdk+'/gsKit')
    e['PATH']=os.pathsep.join([sdk+'/bin',sdk+'/ee/bin',sdk+'/iop/bin',sdk+'/dvp/bin',e['PS2SDK']+'/bin',e.get('PATH','')])
    if not shutil.which('mips64r5900el-ps2-elf-gcc',path=e['PATH']):raise BuildError('PS2DEV compiler not found. Set PS2DEV/PS2SDK/GSKIT, or use --backend docker.')
    if not shutil.which('make',path=e['PATH']):raise BuildError('GNU make is required.')
    return e
def verify_outputs(out):
    try:
        report=json.loads((out/('SMWPS2_'+VERSION+'_build.json')).read_text())
        if report['version']!=VERSION:raise ValueError('wrong build version')
        for extension in ['.elf','.iso']:
            name='SMWPS2_'+VERSION+extension;data=(out/name).read_bytes();expected=report['files'][name]
            if len(data)!=expected['bytes'] or hashlib.sha256(data).hexdigest()!=expected['sha256']:raise ValueError(name+' hash mismatch')
    except (OSError,KeyError,TypeError,ValueError) as e:raise BuildError('Output verification failed: '+str(e))
def build(a):
    payloads={};inputs={}
    for kind,path in {'usa':a.rom,'japanese':a.japanese_rom,'super_system':a.super_system_rom}.items():
        if path:payloads[kind],inputs[kind]=validate_rom(path,kind);print(kind+': clean ROM validated',flush=True)
    if 'usa' not in payloads:raise BuildError('A clean USA ROM is required.')
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    backend=a.backend
    if backend=='auto':backend='portable' if (ROOT/'release/engine/manifest.json').is_file() else 'local' if os.name!='nt' and (os.environ.get('PS2DEV') or Path('/usr/local/ps2dev').is_dir()) else 'docker'
    with (out/('build-'+backend+'.log')).open('w',encoding='utf-8') as log,tempfile.TemporaryDirectory(prefix='smwps2-build-') as tmp:
        temp=Path(tmp);stage=temp/'project';code_hash=copy_source(ROOT,stage);roms=temp/'roms';roms.mkdir()
        for kind,data in payloads.items():(roms/(kind+'.sfc')).write_bytes(data)
        if backend=='docker':
            if not shutil.which('docker'):raise BuildError('Install/start Docker Desktop, then retry. This backend needs no manual PS2SDK setup.')
            run(['docker','info','--format','{{.ServerVersion}}'],log)
            context=temp/'docker-context';context.mkdir();shutil.copyfile(ROOT/'release/Dockerfile',context/'Dockerfile')
            run(['docker','build','--tag',IMAGE,'--file',context/'Dockerfile',context],log)
            cmd=['docker','run','--rm','--mount',mount(stage,'/source',True),'--mount',mount(roms,'/roms',True),'--mount',mount(out,'/out'),IMAGE,'python3','/source/tools/build_from_rom.py','/roms/usa.sfc','--backend','local','--output','/out','--jobs',str(a.jobs)]
            for kind,option in [('japanese','--japanese-rom'),('super_system','--super-system-rom')]:
                if kind in payloads:cmd.extend([option,'/roms/'+kind+'.sfc'])
            run(cmd,log)
            report_path=out/('SMWPS2_'+VERSION+'_build.json');report=json.loads(report_path.read_text());report['inputs']=inputs
            report_path.write_text(json.dumps(report,indent=2)+'\n')
        else:
            try:__import__('pycdlib')
            except ImportError:raise BuildError('Install the local ISO dependency: python3 -m pip install pycdlib==1.14.0')
            env=os.environ.copy() if backend=='portable' else sdk_environment()
            env['PYTHONPATH']=str(BUILD_LIBS)+os.pathsep+env.get('PYTHONPATH','')
            extractors={'usa':'prepare_assets.py','japanese':'prepare_japanese.py','super_system':'prepare_super_system.py'}
            for kind in payloads:run([sys.executable,stage/'tools'/extractors[kind],roms/(kind+'.sfc')],log,cwd=stage,env=env)
            engine_manifest=None
            elf=stage/'dist/SMWPS2.elf'
            if backend=='portable':
                from portable_engine import fill_engine
                names={'usa':'smw_assets.dat','japanese':'smw_japanese.dat','super_system':'smw_super_system.dat'}
                try:binary,engine_manifest=fill_engine(ROOT,{kind:(stage/'data'/names[kind]).read_bytes() for kind in payloads})
                except (ValueError,KeyError) as error:raise BuildError(str(error)) from error
                elf.parent.mkdir(parents=True,exist_ok=True);elf.write_bytes(binary)
                print('Prepared native engine with locally extracted assets; no compiler or Docker is used.',flush=True)
            else:run(['make','-j'+str(a.jobs),'ps2'],log,cwd=stage,env=env)
            run([sys.executable,stage/'tools/check_elf.py',elf],log,cwd=stage,env=env)
            iso=stage/('dist/SMWPS2_'+VERSION+'.iso')
            run([sys.executable,stage/'tools/make_iso.py','--elf',elf,'--output',iso,'--features',','.join(payloads)],log,cwd=stage,env=env)
            files={}
            for p in [elf,iso]:
                dest=out/('SMWPS2_'+VERSION+p.suffix);shutil.copyfile(p,dest);files[dest.name]={'bytes':dest.stat().st_size,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
            compiler=engine_manifest['compiler'] if engine_manifest else subprocess.check_output(['mips64r5900el-ps2-elf-gcc','--version'],env=env,text=True).splitlines()[0]
            report={'version':VERSION,'kit_version':KIT_VERSION,'backend':backend,'built_utc':datetime.now(timezone.utc).isoformat(),'inputs':inputs,'source_sha256':code_hash,'features':list(payloads),'files':files,'compiler':compiler,'scope':'Local asset extraction, precompiled zero-asset engine filling and verified ISO packaging; no client source compilation.' if engine_manifest else 'Source cross-build and verified ISO packaging; not PS2 runtime validation.'}
            if engine_manifest:report['engine_sha256']=engine_manifest['engine_sha256'];report['native_source_sha256']=engine_manifest['native_source_sha256']
            (out/('SMWPS2_'+VERSION+'_build.json')).write_text(json.dumps(report,indent=2)+'\n')
    verify_outputs(out)
    print('Build complete:',out/('SMWPS2_'+VERSION+'.iso'),flush=True)
def gui(a):
    try:
        import tkinter as tk
        from tkinter import filedialog,messagebox
    except ImportError:raise BuildError('File dialogs need tkinter. Supply the ROM filename on the command line instead.')
    root=tk.Tk();root.withdraw()
    def choose(title,kind):
        options={'title':title,'filetypes':NSS_FILETYPES if kind=='super_system' else [('SNES ROM','*.sfc *.smc'),('All files','*.*')]}
        if kind=='super_system':options['initialfile']=NSS_PROGRAM_NAME
        while True:
            v=filedialog.askopenfilename(**options)
            if not v:return None
            try:validate_rom(Path(v),kind)
            except (BuildError,OSError) as error:
                messagebox.showerror('Choose the SMW game program' if kind=='super_system' else 'Unsupported ROM',str(error));continue
            return Path(v)
    a.rom=choose('Select your clean Super Mario World USA ROM','usa')
    if not a.rom:root.destroy();return False
    if messagebox.askyesno('Optional Japanese','Add original SNES Japanese? This needs the Japanese ROM. Choose No for English only.'):a.japanese_rom=choose('Select Super Mario World Japan ROM','japanese')
    if messagebox.askyesno('Optional Super System','Add Super System arcade mode? Select nss-mw-0_prg.ic1 (512 KiB). The .ic3 file and security.prm are not required. Choose No for Original only.'):a.super_system_rom=choose('Select nss-mw-0_prg.ic1 - NSS SMW game program','super_system')
    v=filedialog.askdirectory(title='Choose a folder for the finished ISO and ELF')
    if not v:root.destroy();return False
    a.output=Path(v);root.destroy();return True
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',nargs='?',type=Path);p.add_argument('--japanese-rom',type=Path);p.add_argument('--super-system-rom',type=Path,help='NSS SMW game program: nss-mw-0_prg.ic1 (512 KiB); not the .ic3 or security.prm companion')
    p.add_argument('--backend',choices=['auto','portable','local','docker'],default='auto',help='portable fills the included asset-free native engine; local/docker compile from source');p.add_argument('--output',type=Path,default=ROOT/'output');p.add_argument('--jobs',type=int,default=4);p.add_argument('--gui',action='store_true');a=p.parse_args()
    try:
        if a.gui and not gui(a):return 0
        if not a.rom:p.error('Supply the USA ROM or use --gui.')
        if not 1<=a.jobs<=16:p.error('--jobs must be 1..16')
        build(a);return 0
    except (BuildError,OSError,subprocess.SubprocessError) as e:print('Build stopped:',e,file=sys.stderr);return 1
    except KeyboardInterrupt:print('Build cancelled.',file=sys.stderr);return 130
if __name__=='__main__':raise SystemExit(main())
