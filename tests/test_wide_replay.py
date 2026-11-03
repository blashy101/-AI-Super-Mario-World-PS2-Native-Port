"""Compare repeated/alternate-frame wide replays and native-view pillarboxing."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/"build/host/smw-headless"
def image(path):
    header,w_h,depth,pixels=path.read_bytes().split(b"\n",3)
    assert header==b"P6" and depth==b"255"
    w,h=map(int,w_h.split())
    assert len(pixels)==w*h*3
    return w,h,pixels
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    for wide in (False,True):
        env=os.environ.copy()
        if wide: env["SMW_TEST_WIDE"]="1"
        subprocess.run([str(BIN),str(ROOT/"data/smw_assets.dat"),"1800",str(tmp/f"title-{wide}.ppm"),
            "title",str(tmp/f"title-{wide}.srm")],env=env,cwd=ROOT,check=True,capture_output=True)
    w,h,a=image(tmp/"title-False.ppm")
    ww,wh,b=image(tmp/"title-True.ppm")
    assert (w,h,ww,wh)==(256,224,342,224)
    for y in range(224):
        assert b[(y*342+43)*3:(y*342+299)*3]==a[y*256*3:(y+1)*256*3]
        assert not any(b[y*342*3:(y*342+43)*3]+b[(y*342+299)*3:(y+1)*342*3])
    print("PASS: title art is pixel-identical with clean 43-column pillarboxes")
    outputs=[]; states=[]
    for index,policy in enumerate(("play","play","half")):
        env=os.environ.copy(); env["SMW_TEST_WIDE"]="1"
        trace=tmp/f"trace-{index}.bin"; env["SMW_HASH_TRACE_PATH"]=str(trace)
        result=subprocess.run([str(BIN),str(ROOT/"data/smw_assets.dat"),"7200",str(tmp/f"wide-{index}.ppm"),
            policy,str(tmp/f"wide-{index}.srm")],env=env,cwd=ROOT,check=True,capture_output=True,text=True)
        print(result.stdout.strip())
        rows=trace.read_bytes(); assert len(rows)==7200*12
        states.append(b"".join(rows[i+4:i+12] for i in range(0,len(rows),12)))
        outputs.append((tmp/f"wide-{index}.ppm").read_bytes())
    assert outputs[0]==outputs[1]==outputs[2]
    assert states[0]==states[1]==states[2]
print("PASS: deterministic wide gameplay; all 7200 RAM/PCM states match across render skipping")
