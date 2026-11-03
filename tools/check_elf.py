#!/usr/bin/env python3
"""Check PS2 ELF structure and confirm the native game entry points are linked."""
import argparse
import hashlib
from pathlib import Path
import struct

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("elf", type=Path)
args = ap.parse_args()
data = args.elf.read_bytes()
assert data[:6] == b"\x7fELF\x01\x01", "Expected little-endian ELF32"
header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
_, etype, machine, version, entry, phoff, shoff, flags, _, phsize, phnum, shsize, shnum, _ = header
assert etype == 2 and machine == 8 and version == 1, "Expected MIPS executable"
assert flags & 0x00ff0000 == 0x00920000, "Expected R5900 machine flag"
assert phsize == 32 and shsize == 40
load_ranges = []
for i in range(phnum):
    ptype, off, vaddr, _, filesz, memsz, _, _ = struct.unpack_from("<8I", data, phoff + i * phsize)
    if ptype == 1:
        assert off + filesz <= len(data) and filesz <= memsz
        assert 0x100000 <= vaddr < vaddr + memsz <= 0x2000000, "Segment outside EE user RAM"
        load_ranges.append((vaddr, vaddr + memsz))
assert any(a <= entry < b for a, b in load_ranges), "Entry point outside loadable memory"
sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
names = set()
for section in sections:
    if section[1] != 2:
        continue
    offset, size, link, stride = section[4], section[5], section[6], section[9]
    strings = sections[link]
    blob = data[strings[4]:strings[4] + strings[5]]
    for pos in range(offset, offset + size, stride):
        name = struct.unpack_from("<I", data, pos)[0]
        names.add(blob[name:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
assert "SmwRunOneFrameOfGame" in names
assert "SmwSpcPlayer_GenerateSamples" in names
for forbidden in ("snes_runCpu", "cpu_runOpcode", "cpu_cycle", "spc_cycle"):
    assert forbidden not in names, f"CPU interpreter linked: {forbidden}"
if "SmwArcade_Active" in names:
    assert {"SmwArcade_ZoneSelect", "SmwArcade_InitSaveData", "SmwArcade_CastleText", "SmwArcade_MessagePrompt"} <= names
    assert not any(n.startswith(("ss_ref_", "SmwTestSS")) for n in names), "Test oracle linked into native build"
print(f"PASS: R5900 ELF, entry 0x{entry:08x}, native game/audio symbols, no CPU interpreter symbols")
print(f"Bytes: {len(data)}; SHA256: {hashlib.sha256(data).hexdigest()}")
