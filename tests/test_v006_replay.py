"""Every-frame title pixel/RAM/PCM comparison with the shipped v006 wide build.

Golden traces were generated from its preserved source and supplied asset pack.
The trace includes the padded framebuffer and generated stereo samples.
Gameplay intentionally differs with v008 activation fixes and v009 wide fades;
test_widescreen.c exercises its lifecycle and test_wide_replay.py checks that
render skipping never changes its simulation or audio state.
"""
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
GOLDENS={
    "title":(1800,"153190376a4b67789e365fef50154e70a5b4db9a73c97ba40e0ad22a96e77696"),
}
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    for policy,(frames,digest) in GOLDENS.items():
        env=os.environ.copy();env["SMW_TEST_WIDE"]="1"
        trace=tmp/"trace.bin";env["SMW_HASH_TRACE_PATH"]=str(trace)
        result=subprocess.run([str(ROOT/"build/host/smw-headless"),str(ROOT/"data/smw_assets.dat"),
            str(frames),str(tmp/"frame.ppm"),policy,str(tmp/(policy+".srm"))],cwd=ROOT,env=env,
            check=True,capture_output=True,text=True)
        print(result.stdout.strip())
        assert hashlib.sha256(trace.read_bytes()).hexdigest()==digest, f"v006 {policy} raster/RAM/audio changed"
print("PASS: every-frame v006 wide title pixel, RAM and PCM goldens")
