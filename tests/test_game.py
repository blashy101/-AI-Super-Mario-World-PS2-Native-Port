"""Native title -> file selection -> intro -> overworld -> level smoke run.

Requires data/smw_assets.dat, generated from the supported USA ROM.
Host speed is deliberately never treated as PS2 performance evidence.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import os
import hashlib

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    title_hashes=Path(tmp)/"title.hashes"
    environment=os.environ.copy()
    environment["SMW_HASH_TRACE_PATH"]=str(title_hashes)
    idle = subprocess.run([
        str(ROOT / "build/host/smw-headless"), str(ROOT / "data/smw_assets.dat"),
        "1800", str(Path(tmp) / "title.ppm"), "title", str(Path(tmp) / "title.srm")],
        cwd=ROOT, env=environment, capture_output=True, text=True, check=True)
    print(idle.stdout.strip())
    assert "mode=7 " in idle.stdout, "Idle title screen not reached"
    assert hashlib.sha256(title_hashes.read_bytes()).hexdigest()=="9a2dad27e58de7f5337f16ac10ac90c9ab3ca557134ef3cfac9bb67ece8127ba", "Title replay differs from v002 image/RAM/audio baseline"
    outputs = []
    ram_hashes = []
    for index, policy in enumerate(("play", "play", "half", "play")):
        environment = os.environ.copy()
        hashes=Path(tmp)/f"replay-{index}.hashes"
        environment["SMW_HASH_TRACE_PATH"]=str(hashes)
        if index == 3:
            environment["SMW_TEST_MEMORY"] = "1"
        result = subprocess.run([
            str(ROOT / "build/host/smw-headless"), str(ROOT / "data/smw_assets.dat"),
            "7200", str(Path(tmp) / f"frame-{index}.ppm"), policy,
            str(Path(tmp) / f"save-{index}.srm")], cwd=ROOT, env=environment, capture_output=True, text=True, check=True)
        print(result.stdout.strip())
        modes = int(re.search(r"modes=0x([0-9a-f]+)", result.stdout)[1], 16)
        assert modes & (1 << 8), "File menu not reached"
        assert modes & (1 << 14), "Overworld not reached"
        assert modes & (1 << 20), "Level not reached"
        assert int(re.search(r"level_frames=(\d+)", result.stdout)[1]) > 500
        assert int(re.search(r"action_frames=(\d+)", result.stdout)[1]) > 500, "Did not play a level beyond Yoshi's House"
        assert int(re.search(r"audio_frames=(\d+)", result.stdout)[1]) > 1000
        assert (Path(tmp) / f"save-{index}.srm").stat().st_size == 2048
        outputs.append((Path(tmp) / f"frame-{index}.ppm").read_bytes())
        ram_hashes.append(re.search(r"ram_hash=([0-9a-f]+)", result.stdout)[1])
        if policy=="play":
            assert hashlib.sha256(hashes.read_bytes()).hexdigest()=="b76e22e03d5161b3af76826e668f7775dff91a2c4085d25074fda85695242af0", "Gameplay replay differs from v002 image/RAM/audio baseline"
    assert outputs[0] == outputs[1], "Non-deterministic rendered result"
    assert outputs[0] == outputs[2], "Alternate-frame presentation changed final image"
    assert outputs[0] == outputs[3], "Embedded asset initialization changed final image"
    assert len(set(ram_hashes)) == 1, "Alternate-frame presentation changed game RAM"
print("PASS: native gameplay, audio, saves, alternate-frame presentation, embedded assets and every-frame v002 regression hashes")
