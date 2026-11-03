"""Exercise malformed external asset files through the public load path."""
from pathlib import Path
import struct
import subprocess
import tempfile
import hashlib

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build/host/smw-headless"
SIG = bytes([83,109,119,95,118,48,32,32,32,32,32,32,32,32,10,0,85,177,205,26,249,0,56,113,186,11,242,196,157,30,113,190,86,157,53,15,96,246,11,95,59,106,128,210,127,126,117,134])
valid=(ROOT/"data/smw_assets.dat").read_bytes()
count,names=struct.unpack_from("<II",valid,80)
sizes=struct.unpack_from("<"+str(count)+"I",valid,88)
prefix=88+count*4+names;pos=prefix;records=[]
for size in sizes:
    pos=(pos+3)&~3;records.append(valid[pos:pos+size]);pos+=size
# Complete vanilla tables from the verified USA ROM, not per-pipe overrides.
table_hashes={
    150:"a6ae44009a209fe59b496cc4a206138d3902b6cb044db09aa2bda9b989720153",
    151:"53df07874c3d8d10a186b80457f70c413fc0bf5fb864ce22e8e30c0e41502697",
    152:"1491a5ded3bd80d402146a5257f9b43bb2ac408e350bff46ac341dc32c5728f0",
    153:"146918c325f73ce98716610049db19cd8453d313c2ed66427e92f4220232e2cf",
}
for index,digest in table_hashes.items():
    assert len(records[index])==512 and hashlib.sha256(records[index]).hexdigest()==digest
def pack(parts):
    out=bytearray(valid[:prefix])
    for i,data in enumerate(parts):
        struct.pack_into("<I",out,88+i*4,len(data))
        out.extend(bytes(-len(out)%4));out.extend(data)
    return out
with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    cases = [("short", b"bad"), ("wrong-magic", bytes(900))]
    base = bytearray(900)
    base[:48] = SIG
    struct.pack_into("<II", base, 80, 178, 0)
    broken = bytearray(base)
    struct.pack_into("<I", broken, 84, 0xffffffff)
    cases.append(("names-overflow", broken))
    broken = bytearray(base)
    struct.pack_into("<I", broken, 88, 0xffffffff)
    cases.append(("asset-overflow", broken))
    missing=records.copy()
    for i in range(150,154):missing[i]=b""
    cases.append(("missing-secondary-tables",pack(missing)))
    short=records.copy();short[151]=short[151][:-1]
    cases.append(("short-secondary-table",pack(short)))
    for name, blob in cases:
        path = folder / name
        path.write_bytes(blob)
        result = subprocess.run([str(BIN), str(path), "1"], capture_output=True, text=True)
        assert result.returncode == 1, (name, result.returncode, result.stderr)
        if "secondary" in name:assert "secondary entrance" in result.stderr
    result = subprocess.run([str(BIN), str(folder / "missing"), "1"], capture_output=True)
    assert result.returncode == 1
print("PASS: seven malformed/legacy asset cases rejected; all four complete secondary tables match USA data")
