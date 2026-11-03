"""Exercise the real template, optional slots, corrupt input and source mismatch."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import portable_engine as engine
import build_from_rom as builder

template = (ROOT / 'release/engine/smw_engine.template').read_bytes()
slots = engine.locate_slots(template)
assert all(not any(template[s['offset']:s['offset'] + s['capacity']]) for s in slots.values())
assets = {'usa': b'USA fixture', 'japanese': b'JP fixture', 'super_system': b'NSS fixture'}
patched, manifest = engine.fill_engine(ROOT, assets)
allowed = set()
for name, slot in slots.items():
    offset, capacity, size_offset = slot['offset'], slot['capacity'], slot['size_offset']
    assert patched[offset:offset + len(assets[name])] == assets[name]
    assert struct.unpack_from('<I', patched, size_offset)[0] == len(assets[name])
    allowed.update(range(offset, offset + capacity));allowed.update(range(size_offset, size_offset + 4))
assert len(patched) == len(template)
assert all(a == b or index in allowed for index, (a, b) in enumerate(zip(template, patched)))
usa_only, _ = engine.fill_engine(ROOT, {'usa': assets['usa']})
for name in ['japanese', 'super_system']:
    slot = slots[name]
    assert struct.unpack_from('<I', usa_only, slot['size_offset'])[0] == 0
    assert not any(usa_only[slot['offset']:slot['offset'] + slot['capacity']])
for bad_assets in [{}, {'usa': b'x' * (slots['usa']['capacity'] + 1)}]:
    try:engine.fill_engine(ROOT, bad_assets);raise AssertionError('Invalid assets accepted')
    except ValueError:pass
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary) / 'kit'
    builder.copy_source(ROOT, root)
    shutil.copytree(ROOT / 'release/engine', root / 'release/engine')
    source = root / 'src/menu.c'
    source.write_bytes(source.read_bytes() + b'\n/* modified source */\n')
    try:engine.fill_engine(root, assets);raise AssertionError('Stale engine accepted')
    except ValueError as error:assert 'Native source differs' in str(error)
    source.write_bytes((ROOT / 'src/menu.c').read_bytes())
    path = root / 'release/engine/smw_engine.template'
    data = bytearray(path.read_bytes());data[-1] ^= 1;path.write_bytes(data)
    try:engine.fill_engine(root, assets);raise AssertionError('Corrupt template accepted')
    except ValueError as error:assert 'hash mismatch' in str(error)
print('PASS: real zero-asset engine, exact slot boundaries, unchanged code outside slots, optional clearing, size checks, source mismatch and corrupt-template rejection')
