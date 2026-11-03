#!/usr/bin/env python3
"""Package the standalone, asset-embedded PS2 ELF into an ISO9660 boot disc."""
import argparse
import hashlib
import io
from pathlib import Path
import tempfile

import pycdlib

ROOT = Path(__file__).resolve().parents[1]

def make_iso(elf: Path, output: Path, padding_mb: int, features: str = "usa,japanese,super_system") -> None:
    payload = elf.read_bytes()
    if payload[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected the PS2 ELF32 executable")
    config = b"BOOT2 = cdrom0:\\BOOT.ELF;1\r\nVER = 1.22\r\nVMODE = NTSC\r\n"
    note = ("SMW PS2 v022 - ROM-built native port\r\n"
            f"Included ROM inputs: {features}\r\n"
            "MX4SIO ISO clean IOP handoff at boot.\r\n"
            "Game assets are embedded in BOOT.ELF. No host: or USB asset pack is needed.\r\n"
            "Original saves to a PS2 card on MX4SIO. USB can also use mounted mass: storage.\r\n"
            "L2+R2 toggles profiling; L2+R2+Start returns to the launcher.\r\n"
            "30 fps display target, about 60 game updates/sec. 48 kHz stereo output.\r\n"
            "Square selects 4:3 or 16:9; Triangle selects English or original SNES Japanese.\r\n"
            "Optional Super System uses its seven-zone menu and session-only free play.\r\n"
            "PPU/GPU/IO: ms per draw. CPU/APU: ms per update. Q: queued audio ms.\r\n").encode("ascii")
    output.parent.mkdir(parents=True, exist_ok=True)
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=1, sys_ident="PLAYSTATION", vol_ident="SMWPS2_V022",
            app_ident_str="SMW PS2 NATIVE PORT")
    with tempfile.TemporaryFile() as padding:
        padding.truncate(padding_mb * 1024 * 1024)
        iso.add_file(str(elf), iso_path="/BOOT.ELF;1")
        iso.add_fp(io.BytesIO(config), len(config), iso_path="/SYSTEM.CNF;1")
        iso.add_fp(io.BytesIO(note), len(note), iso_path="/README.TXT;1")
        if padding_mb:
            iso.add_fp(padding, padding_mb * 1024 * 1024, iso_path="/PADDING.BIN;1")
        iso.write(str(output))
    iso.close()
    check = pycdlib.PyCdlib()
    check.open(str(output))
    for path, expected in (("/BOOT.ELF;1", payload), ("/SYSTEM.CNF;1", config)):
        actual = io.BytesIO()
        check.get_file_from_iso_fp(actual, iso_path=path)
        if actual.getvalue() != expected:
            raise ValueError(f"ISO verification failed: {path}")
    check.close()
    print(f"Created and verified: {output} ({output.stat().st_size} bytes)")
    print(f"ELF SHA256: {hashlib.sha256(payload).hexdigest()}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=ROOT / "dist/SMWPS2.elf")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/SMWPS2_v022.iso")
    parser.add_argument("--padding-mb", type=int, default=16)
    parser.add_argument("--features", default="usa,japanese,super_system")
    args = parser.parse_args()
    if not 0 <= args.padding_mb <= 256:
        parser.error("padding must be between 0 and 256 MiB")
    make_iso(args.elf, args.output, args.padding_mb, args.features)
