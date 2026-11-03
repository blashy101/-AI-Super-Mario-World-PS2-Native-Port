"""Verify recovery from interrupted/corrupt alternating memory-card records."""
import ctypes
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    library = folder / "journal.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-Iinclude", "src/save_journal.c", "-o", str(library)], cwd=ROOT, check=True)
    api = ctypes.CDLL(str(library))
    for name in ("smw_journal_load", "smw_journal_save"):
        function = getattr(api, name)
        function.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
        function.restype = ctypes.c_bool
    base = folder / "smw.srm"
    encoded = str(base).encode()
    old = bytes(range(256)) * 8
    new = bytes(reversed(range(256))) * 8
    a, b = ctypes.create_string_buffer(old), ctypes.create_string_buffer(new)
    out = ctypes.create_string_buffer(2048)
    assert not api.smw_journal_load(encoded, out, 2048)
    assert api.smw_journal_save(encoded, a, 2048)
    assert api.smw_journal_save(encoded, b, 2048)
    assert api.smw_journal_load(encoded, out, 2048) and out.raw == new
    record = Path(str(base) + ".1").read_bytes()
    assert record[:8] == b"SMWSAVE2"
    assert struct.unpack_from("<III", record, 8) == (2, 2048, zlib.crc32(new))
    assert api.smw_journal_save(encoded, b, 2048)
    assert Path(str(base) + ".1").read_bytes() == record
    Path(str(base) + ".1").write_bytes(record[:80])
    assert api.smw_journal_load(encoded, out, 2048) and out.raw == old
    assert api.smw_journal_save(encoded, b, 2048)
    damaged = bytearray(Path(str(base) + ".1").read_bytes())
    damaged[100] ^= 128
    Path(str(base) + ".1").write_bytes(damaged)
    assert api.smw_journal_load(encoded, out, 2048) and out.raw == old
    Path(str(base) + ".0").write_bytes(b"corrupt")
    assert not api.smw_journal_load(encoded, out, 2048)
print("PASS: save journal round trip, unchanged data, truncated write and CRC recovery")
