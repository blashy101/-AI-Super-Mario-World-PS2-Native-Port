#!/usr/bin/env python3
"""Extract original SNES Japanese presentation assets; no GBA or ROM payload.

The native engine retains its shared USA gameplay/entrance tables and audio.
Symbol addresses are pinned to the byte-identical Japanese SMWDisX assembly.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
SHA1 = "f977afabf24ed269d86366209a460450bbc37e76"
sys.path.insert(0, str(ROOT / "vendor/smw/assets"))
import util

def extract(rom):
    data = rom.read_bytes()
    if len(data) == 524800:
        data = data[512:]
    if len(data) != 524288 or hashlib.sha1(data).hexdigest() != SHA1:
        raise ValueError(f"Expected original SNES Japan ROM SHA1 {SHA1}")
    # util's normal loader accepts USA only. This extractor validates Japan above.
    util.load_rom(str(rom.resolve()), disable_hash_check=True)
    from compile_resources import pack_arrays, get_stripe_len
    manifest = json.loads((ROOT / "tools/japanese_addresses.json").read_text())
    s = manifest["symbols"]
    def raw(name, size): return bytes(util.get_bytes(s[name], size))
    def native_compressed(addr):
        # Japan stores LZ back-reference words little-endian; the common native
        # decoder uses USA byte order. Convert operands, retaining every pixel.
        decoded, length = util.decomp(addr, util.get_byte, offset_is_be=False, return_length=True)
        encoded = bytearray(util.get_bytes(addr, length))
        p = 0
        while encoded[p] != 255:
            b = encoded[p]; p += 1
            if b & 0xe0 == 0xe0:
                count = ((b & 3)<<8 | encoded[p])+1; p += 1
                command = b << 3 & 0xe0
            else: count, command = (b & 31)+1, b & 0xe0
            if command & 0x80:
                encoded[p], encoded[p+1] = encoded[p+1], encoded[p]; p += 2
            elif command == 0: p += count
            elif command == 0x40: p += 2
            else: p += 1
        assert p+1 == length
        assert util.decomp(0, lambda i: encoded[i]) == decoded
        return encoded
    gfx = []
    for i in range(50):
        addr = (util.get_byte(s['GFXFilesBank']+i)<<16 |
                util.get_byte(s['GFXFilesHigh']+i)<<8 |
                util.get_byte(s['GFXFilesLow']+i))
        gfx.append(native_compressed(addr))
    stripes = [bytes(util.get_bytes(a, get_stripe_len(a))) if a else b'\xff'
               for a in manifest['stripes_in_native_US_order']]
    stripes[0] = b''
    layer3 = []
    for i in range(45):
        a = util.get_24(s['Layer3Ptr'] + i*3)
        layer3.append(bytes(util.get_bytes(a, get_stripe_len(a))))
    assets = {
        0: pack_arrays(gfx), 3: pack_arrays(stripes), 4: pack_arrays(layer3),
        32: raw('DATA_0C95C7', s['DATA_0C9D18']-s['DATA_0C95C7']),
        33: raw('DATA_0C9D18', 404),
        39: raw('MessageBoxes', s['ClearMessageStripe']-s['MessageBoxes']),
        41: raw('LevelNames', 512), 60: raw('FileSelectStripe', 176),
        85: raw('LevelNameStrings', s['DATA_049C91']-s['LevelNameStrings']),
        # Auxiliary presentation tables; never bound as engine asset slots.
        178: raw('DATA_05A580', 8), 179: raw('DATA_05A5A7', 50),
        180: raw('DATA_049C91', 38), 181: raw('DATA_049CCF', 28),
        182: raw('JDATA_049BE1', 22), 183: raw('DATA_049CED', 4),
        184: raw('CursorCoords', 10), 185: raw('EraseFileTiles', 16),
        186: raw('DATA_05B2DB', 16),
    }
    out = bytearray(b'SMWJP001' + struct.pack('<I', len(assets)))
    for index, payload in sorted(assets.items()):
        out += struct.pack('<II', index, len(payload)) + payload
        out += bytes(-len(out) % 4)
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('rom', type=Path)
    ap.add_argument('--output', type=Path, default=ROOT/'data/smw_japanese.dat')
    args = ap.parse_args()
    try: payload = extract(args.rom)
    except ValueError as e: ap.error(str(e))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f'Created {args.output} ({len(payload)} bytes, SHA256 {hashlib.sha256(payload).hexdigest()})')

if __name__ == '__main__': main()
