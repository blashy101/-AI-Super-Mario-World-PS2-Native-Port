#!/usr/bin/env python3
"""Fill a compiled native engine's empty asset slots from locally extracted DATs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SYMBOLS = {
    'usa': ('smw_embedded_assets', 'size_smw_embedded_assets'),
    'japanese': ('smw_embedded_japanese', 'size_smw_embedded_japanese'),
    'super_system': ('smw_embedded_super_system', 'size_smw_embedded_super_system'),
}

def native_fingerprint(root):
    paths = [root / 'Makefile.ps2']
    for directory in ['src', 'include', 'vendor/smw', 'vendor/audsrv']:
        paths.extend(p for p in (root / directory).rglob('*') if p.is_file()
                     and p.suffix.lower() in {'.c', '.h', '.s'}
                     and not any(s in {'build', 'obj', '__pycache__'} for s in p.relative_to(root).parts))
    paths.append(root / 'vendor/audsrv/Makefile')
    digest = hashlib.sha256()
    # Sort relative strings: WindowsPath's native ordering ignores case.
    for path in sorted(set(paths), key=lambda p: p.relative_to(root).as_posix()):
        digest.update(path.relative_to(root).as_posix().encode() + b'\0' + path.read_bytes())
    return digest.hexdigest()

def locate_slots(data):
    if data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Expected a little-endian ELF32 engine')
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    _, kind, machine, _, _, phoff, shoff, flags, _, phsize, phnum, shsize, shnum, _ = header
    if kind != 2 or machine != 8 or flags & 0x00ff0000 != 0x00920000 or phsize != 32 or shsize != 40:
        raise ValueError('Expected an R5900 executable')
    loads = []
    for index in range(phnum):
        t, offset, address, _, length, _, _, _ = struct.unpack_from('<8I', data, phoff + index * phsize)
        if t == 1:
            if offset + length > len(data):raise ValueError('Truncated load segment')
            loads.append((address, offset, length))
    sections = [struct.unpack_from('<10I', data, shoff + index * shsize) for index in range(shnum)]
    symbols = {}
    for section in sections:
        if section[1] != 2:continue
        offset, length, link, stride = section[4], section[5], section[6], section[9]
        if stride != 16:raise ValueError('Unexpected ELF symbol stride')
        strings = sections[link]
        names = data[strings[4]:strings[4] + strings[5]]
        for pos in range(offset, offset + length, stride):
            name, value, size, _, _, _ = struct.unpack_from('<IIIBBH', data, pos)
            text = names[name:].split(b'\0', 1)[0].decode('ascii', errors='replace')
            if text in {s for pair in SYMBOLS.values() for s in pair}:
                for address, file_offset, file_size in loads:
                    if address <= value and value + size <= address + file_size:
                        symbols[text] = (file_offset + value - address, size)
                        break
    slots = {}
    ranges = []
    for name, (array, size_symbol) in SYMBOLS.items():
        offset, capacity = symbols[array]
        size_offset, size_length = symbols[size_symbol]
        if not capacity or size_length != 4:raise ValueError('Invalid asset-slot symbols')
        slots[name] = {'offset': offset, 'capacity': capacity, 'size_offset': size_offset}
        ranges.extend([(offset, offset + capacity), (size_offset, size_offset + 4)])
    ranges.sort()
    if any(end > next_start for (_, end), (next_start, _) in zip(ranges, ranges[1:])):
        raise ValueError('Overlapping engine slots')
    return slots

def write_manifest(template, output, compiler):
    data = template.read_bytes()
    slots = locate_slots(data)
    for slot in slots.values():
        offset, capacity = slot['offset'], slot['capacity']
        if any(data[offset:offset + capacity]):raise ValueError('Engine template contains nonempty asset data')
        if struct.unpack_from('<I', data, slot['size_offset'])[0] != capacity:
            raise ValueError('Template capacity word does not match its array')
    manifest = {'format': 'SMWPS2-ENGINE-1', 'version': 'v022',
                'engine_sha256': hashlib.sha256(data).hexdigest(),
                'native_source_sha256': native_fingerprint(ROOT),
                'compiler': compiler, 'slots': slots,
                'scope': 'Compiled native code, audsrv/SDK modules and zero-filled asset slots. No ROM or extracted DAT pack.'}
    output.write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest

def fill_engine(root, assets):
    folder = root / 'release/engine'
    template = (folder / 'smw_engine.template').read_bytes()
    manifest = json.loads((folder / 'manifest.json').read_text())
    if manifest['format'] != 'SMWPS2-ENGINE-1' or manifest['version'] != 'v022':
        raise ValueError('Unsupported engine template')
    if hashlib.sha256(template).hexdigest() != manifest['engine_sha256']:
        raise ValueError('Engine template hash mismatch; extract the build kit again')
    if native_fingerprint(root) != manifest['native_source_sha256']:
        raise ValueError('Native source differs from the compiled engine. Recompile with --backend local or --backend docker.')
    slots = locate_slots(template)
    if slots != manifest['slots']:raise ValueError('Engine slot manifest mismatch')
    if 'usa' not in assets or not assets['usa']:raise ValueError('The USA asset pack is required')
    result = bytearray(template)
    for name, slot in slots.items():
        offset, capacity = slot['offset'], slot['capacity']
        if any(template[offset:offset + capacity]):raise ValueError('Engine asset slots must be empty')
        payload = assets.get(name, b'')
        if len(payload) > capacity:raise ValueError(name + ' exceeds its engine asset slot')
        result[offset:offset + capacity] = payload + bytes(capacity - len(payload))
        struct.pack_into('<I', result, slot['size_offset'], len(payload))
    return bytes(result), manifest

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Record a freshly compiled zero-asset engine template')
    parser.add_argument('template', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', required=True)
    args = parser.parse_args()
    report = write_manifest(args.template, args.output, args.compiler)
    print('Verified empty engine slots:', ', '.join(f'{name}={slot["capacity"]}' for name, slot in report['slots'].items()))
