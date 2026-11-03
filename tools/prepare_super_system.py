#!/usr/bin/env python3
"""Extract the original NSS game presentation and seven-zone starting data.

Accepts the original 512 KiB game program, optionally with a copier header.
Addresses come from the matching SMWDisX SS symbols, never retail offsets.
The cabinet BIOS/instruction ROM and a complete game ROM are not embedded.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
SHA1 = '06a6efc246c6fdb83efab1d402d61d2179a84494'
sys.path.insert(0, str(ROOT/'vendor/smw/assets'))
import util

def extract(rom):
    data = rom.read_bytes()
    if len(data) == 524800:
        data = data[512:]
    if len(data) != 524288 or hashlib.sha1(data).hexdigest() != SHA1:
        raise ValueError(f'Expected NSS SMW game program SHA1 {SHA1}')
    util.load_rom(str(rom.resolve()), disable_hash_check=True)
    from compile_resources import pack_arrays, get_stripe_len
    s = json.loads((ROOT/'tools/super_system_addresses.json').read_text())['symbols']
    def raw(name, size): return bytes(util.get_bytes(s[name], size))
    stripes = [b'']
    for i in range(1, 86):
        addr = util.get_24(s['StripeImages'] + i*3)
        stripes.append(bytes(util.get_bytes(addr, get_stripe_len(addr))))
    assets = {
        3: pack_arrays(stripes),
        39: raw('MessageBoxes', s['ClearMessageStripe']-s['MessageBoxes']),
        178: raw('DATA_05A5A7', 50),
        179: raw('DATA_05B2DB', 16),
        180: raw('ZoneCursorStripe', 19),
        181: raw('ZoneCursorPositions', 7),
        182: raw('InitLevelTileMoveIndices', 8),
        183: raw('InitLevelTileMovementData', 92),
        # The assembly copies overlapping 16-bit words at offsets 40..0.
        # Preserve the extra high byte at offset 41, including for zone 7.
        184: raw('InitOWEventPlayerData', 288),
        185: raw('InitOWEventPlayerDataIndices', 14),
        186: raw('CursorCoords', 10),
        187: raw('FileSelectStripe', get_stripe_len(s['FileSelectStripe'])),
    }
    out = bytearray(b'SMWSS001' + struct.pack('<I', len(assets)))
    for index, payload in sorted(assets.items()):
        out += struct.pack('<II', index, len(payload)) + payload
        out += bytes(-len(out) % 4)
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('rom', type=Path)
    ap.add_argument('--output', type=Path, default=ROOT/'data/smw_super_system.dat')
    ap.add_argument('--reference-output', type=Path, help='Optional tiny test-only assembled-code capsule')
    args = ap.parse_args()
    try: payload = extract(args.rom)
    except ValueError as e: ap.error(str(e))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f'Created {args.output} ({len(payload)} bytes, SHA256 {hashlib.sha256(payload).hexdigest()})')
    if args.reference_output:
        data = args.rom.read_bytes()
        if len(data) == 524800: data = data[512:]
        m = json.loads((ROOT/'tools/super_system_addresses.json').read_text())
        capsule = bytearray(b'SSREF001' + struct.pack('<I', len(m['reference_ranges'])))
        for a,b in m['reference_ranges']:
            pc = (a >> 16)*0x8000 + (a & 0x7fff)
            capsule += struct.pack('<II', a, b-a) + data[pc:pc+b-a]
        args.reference_output.parent.mkdir(parents=True, exist_ok=True)
        args.reference_output.write_bytes(capsule)


if __name__ == '__main__': main()
