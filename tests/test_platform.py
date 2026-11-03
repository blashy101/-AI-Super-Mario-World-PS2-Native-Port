"""Compile the actual old/new frontend against deterministic PS2 SDK clocks.

This verifies pacing and buffering, not real PS2 execution or performance.
"""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
cc=os.environ.get("CC","cc")
report={"scope":"SDK clock model of actual frontend; not PS2 runtime evidence","cases":[]}
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    for version,source in (("v003","tests/reference/ps2_main_v003.c"),("v022","src/ps2_main.c")):
        obj=tmp/f"{version}.o"
        binary=tmp/version
        subprocess.run([cc,"-O2","-std=gnu11","-I.","-Iinclude","-DSMW_LOOP_TEST",
            "-DSMW_EMBEDDED_ASSETS","-Dmain=port_main","-c",source,"-o",str(obj)],cwd=ROOT,check=True)
        subprocess.run([cc,"-O2","-std=gnu11","-Iinclude","-Itests",str(obj),
            "tests/platform_mock.c","src/frame_pacer.c","src/audio_queue.c","-o",str(binary)],cwd=ROOT,check=True)
        cases=[(180,0,0,0)] if version=="v003" else [(1800,0,0,0),(1800,-2000,0,0),(1800,2000,0,0),(1800,0,1,0),
            (1800,0,0,1),(1800,-2000,0,1),(1800,2000,0,1),(1800,0,1,1)]
        for seconds,ppm,fallback,wide in cases:
            run=subprocess.run([str(binary),str(seconds),str(ppm),str(fallback),str(wide)],cwd=tmp,
                capture_output=True,text=True,check=True,timeout=30)
            lines=run.stdout.splitlines()
            result=json.loads(next(s.removeprefix("MODEL_RESULT ") for s in lines if s.startswith("MODEL_RESULT ")))
            result.update(version=version,audio_clock_ppm=ppm,timer_fallback=bool(fallback),widescreen=bool(wide))
            if version=="v003":
                assert result["first_fps"]>30 and result["first_fps"]>result["last_fps"]*2 and result["last_fps"]<20, result
                assert result["wait_calls"]>100, result
            else:
                assert 29.8<result["first_fps"]<30.1 and 29.8<result["last_fps"]<30.1, result
                assert 59.8<result["sim_hz"]<60.1, result
                assert result["wait_calls"]==result["underflows"]==result["short_writes"]==0, result
                assert result["max_gap_ms"]<34, result
                for line in lines:
                    if f"SMWPS2 {version.upper()} PERF:" in line:
                        assert "LOST 0 DROP 0 ERR 0" in line, line
            report["cases"].append(result)
            print(json.dumps(result))
(ROOT/"docs/v022-platform-model.json").write_text(json.dumps(report,indent=2)+"\n")
print("PASS: old-loop collapse reproduced; new-loop 30-minute cadence, audio drift and timer fallback")
