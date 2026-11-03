#!/usr/bin/env python3
"""Isolated room loading/input tests; these do not prove campaign completion."""
import argparse
from concurrent.futures import ThreadPoolExecutor,as_completed
import json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frames',type=int,default=240);p.add_argument('--jobs',type=int,default=4);p.add_argument('--batch',type=int,default=16)
    p.add_argument('--sanitizers',action='store_true');p.add_argument('--only',help='Comma-separated numeric room IDs')
    p.add_argument('--editions',default='en,jp,ss');p.add_argument('--output',type=Path,default=ROOT/'build/room-sweep')
    a=p.parse_args()
    if not 1<=a.frames<=36000 or not 1<=a.jobs<=16 or not 1<=a.batch<=64:p.error('Invalid frames/jobs/batch')
    catalog=json.loads((ROOT/'tests/room_catalog.json').read_text());rooms=catalog['rooms']
    if a.only:
        ids={int(x,0) for x in a.only.split(',')};rooms=[r for r in rooms if r['id'] in ids]
        if ids!={r['id'] for r in rooms}:p.error('Room outside the non-TestLevel catalog')
    editions=a.editions.split(',');edition_ids={'en':0,'jp':1,'ss':2}
    if any(e not in edition_ids for e in editions):p.error('Editions: en,jp,ss')
    for e,n in [('en','smw_assets'),('jp','smw_japanese'),('ss','smw_super_system')]:
        if e in editions and not (ROOT/('data/'+n+'.dat')).is_file():p.error(e+' needs its extracted DAT; supply the corresponding ROM first')
    a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
    build='build/asan-rooms' if a.sanitizers else 'build/host';cmd=['make','-j'+str(a.jobs),'BUILD='+build]
    if a.sanitizers:cmd+=['CFLAGS=-O1 -g -std=gnu11 -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections -Wno-unknown-pragmas -fsanitize=address,undefined -fno-omit-frame-pointer']
    cmd+=[build+'/test-rooms']
    with (a.output/'compile.log').open('w') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    exe=ROOT/build/'test-rooms';exe.chmod(exe.stat().st_mode|0o100)
    jp=str(ROOT/'data/smw_japanese.dat') if (ROOT/'data/smw_japanese.dat').exists() else '-'
    ss=str(ROOT/'data/smw_super_system.dat') if (ROOT/'data/smw_super_system.dat').exists() else '-'
    env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(task):
        edition,wide,group=task;pending=list(group);results=[];attempt=0
        while pending:
            stem=f'{edition}-{wide}-{group[0]["id"]:03x}-{attempt}'
            cmd=[str(exe),str(ROOT/'data/smw_assets.dat'),jp,ss,str(edition_ids[edition]),str(wide),str(a.frames)]+[str(r['id']) for r in pending]
            try:
                r=subprocess.run(cmd,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120);text,status=r.stdout,r.returncode
            except subprocess.TimeoutExpired as e:text,status=e.stdout or '', 'timeout'
            if isinstance(text,bytes):text=text.decode(errors='replace')
            (a.output/(stem+'.log')).write_text(text);passed={};last=None
            for line in text.splitlines():
                if line.startswith('ROOM_BEGIN '):last=int(line.split()[1])
                if line.startswith('ROOM_PASS '):
                    item=json.loads(line[10:]);passed[item['room']]=item
            results.extend(dict(r,edition=edition,wide=bool(wide),status='pass',log=stem+'.log',**{k:v for k,v in passed[r['id']].items() if k!='room'}) for r in pending if r['id'] in passed)
            remain=[r for r in pending if r['id'] not in passed]
            if status==0 and not remain:break
            if last is None or last in passed:
                results.extend(dict(r,edition=edition,wide=bool(wide),status='harness_failure',exit_status=status,log=stem+'.log') for r in remain);break
            bad=next(r for r in remain if r['id']==last);results.append(dict(bad,edition=edition,wide=bool(wide),status='failure',exit_status=status,log=stem+'.log'))
            pending=[r for r in remain if r['id']!=last];attempt+=1
        return results
    tasks=[(e,w,rooms[i:i+a.batch]) for e in editions for w in [0,1] for i in range(0,len(rooms),a.batch)]
    start=time.monotonic();results=[]
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for done,f in enumerate(as_completed([pool.submit(run,t) for t in tasks]),1):
            results+=f.result();print(f'Room batches {done}/{len(tasks)}; failures {sum(r["status"]!="pass" for r in results)}',flush=True)
            report={'scope':'Controlled room-loader/input smoke tests, not campaign completion or PS2 runtime evidence','catalog_source':catalog['source'],'sanitizers':a.sanitizers,'requested_frames':a.frames,'wall_seconds':round(time.monotonic()-start,2),'cases':results}
            (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    failed=sum(r['status']!='pass' for r in results);print(f'{len(results)-failed}/{len(results)} passed; {failed} failures; {a.output/"report.json"}')
    return bool(failed)
if __name__=='__main__':raise SystemExit(main())
