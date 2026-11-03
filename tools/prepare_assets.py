#!/usr/bin/env python3
"""Extract native engine assets from a user supplied unmodified USA SMW ROM."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SHA1 = "6b47bb75d16514b6a476aa0c73a683a2a4c18765"

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--output", type=Path, default=ROOT / "data/smw_assets.dat")
    args = ap.parse_args()
    data = args.rom.read_bytes()
    if len(data) == 524800:
        data = data[512:]
    actual = hashlib.sha1(data).hexdigest()
    if len(data) != 524288 or actual != SHA1:
        ap.error(f"Expected clean USA ROM SHA1 {SHA1}; got {actual}. Use prepare_super_system.py for the NSS overlay.")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="smw-ps2-assets-") as tmp:
        rom = Path(tmp) / "smw.sfc"
        rom.write_bytes(data)
        subprocess.run([sys.executable, str(ROOT / "vendor/smw/assets/restool.py"),
                        "--rom", str(rom), "--no-include-rom"], cwd=tmp, check=True)
        output = Path(tmp) / "smw_assets.dat"
        args.output.write_bytes(output.read_bytes())
    print(f"Created {args.output} ({args.output.stat().st_size} bytes)")

if __name__ == "__main__":
    main()
