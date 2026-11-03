# v017.1 — portable ROM-to-ISO builder

This is a builder update for the existing v017 game. No native gameplay, menu,
renderer, sound, save or controller source is changed.

## Player workflow

Build_ISO.cmd uses a Windows PowerShell launcher to locate Python with tkinter.
If missing, it offers a per-user installation of official Python 3.13.17 and
checks the installer's Authenticode publisher before running it. This setup does
not install Docker, WSL or a compiler. Python libraries are included as editable
source under vendor/build_py, with their original notices. After Python is
available, asset extraction, ELF preparation and ISO creation work offline.

The default portable backend uses precompiled native code with empty asset
slots. It validates the user's ROMs, extracts the required DAT packs locally,
inserts them into verified file-backed ELF slots, updates their length words
and verifies the resulting disc. Optional packs are explicitly zeroed and have
zero lengths when omitted. ROM files are never changed or uploaded.

This default does not compile native source on the player's PC. Explicit local
and Docker source-build backends remain available. A native-source fingerprint
rejects a stale compiled engine after edits. Fingerprints sort relative POSIX
strings so Windows and Linux path ordering do not change their meaning.

## Engine generation and verification

The engine was rebuilt with EE GCC 15.2.0 and the same SDK used for v017, using
three zero-filled inputs of 355,868, 126,408 and 12,812 bytes. Only asset objects
use -fno-zero-initialized-in-bss so their blank arrays remain file-backed.
The new Makefile variables preserve existing defaults for normal source builds.
tools/rebuild_portable_engine.py repeats this process in an isolated source tree
without needing any ROM.

Both the manual build and that regeneration tool produced the same template:
SHA256 c82169a8a02f0d58b3c8f65b3726370fcdfe9d7fb58d12ddf8f60b364195417e.
All three arrays were verified completely zero-filled before packaging. The
manifest records actual ELF symbols, capacities, compiler and native-source
fingerprint. Inserting the real existing packs yields an ELF byte-for-byte
identical to the previous source-built v017 game:
SHA256 619b94f3be1d5ac317ad95a39200af0738719e5623dca589b9fb196dc1a2bc3f.

## Tests

- Full US/Japan/NSS and then US-only builds passed in one output directory using
  an isolated Python environment with no pip packages, SDK or Docker available.
- Headered US input was accepted without changing it. US/Japan fixture ROMs were
  assembled from SMWDisX and matched the supported hashes; NSS used the supplied
  original .ic1 file. No independent US/Japan dump was part of this run.
- The three-input ELF matched the prior source-built v017 ELF exactly. In the
  subsequent US-only ELF, Japanese/NSS slots were entirely zero and disabled.
- Existing builder validation, optional-symbol absence, .ic1 picker/retry and
  explicit Docker command-contract checks passed. Docker remains mocked there.
- Portable-engine tests passed for exact slot boundaries, unchanged bytes outside
  slots, zeroed optional data, missing/oversized packs, corrupt templates and
  native-source mismatch rejection.
- PowerShell syntax and launcher contracts passed with dependency lookup/install
  mocked: existing/missing Python, GUI selection, spaced paths, cancellation and
  propagation of a nonzero child exit code.
- The final ZIP is extracted into a path with spaces. Focused tests and actual
  isolated Python full/US-only ROM-to-ISO builds are repeated from that copy.
- BOOT.ELF and SYSTEM.CNF are read back by the ISO tool; final independent ELF
  hash/slot checks also verify the delivery outputs.

Real Windows file dialogs, certificate validation, Python installation and a
clean Windows first run have not been executed here. The installer is therefore
still beta software. Existing v016/v017 campaign/hardware limitations remain;
this work does not claim a new gameplay or performance validation.

## Third-party Python sources

- pycdlib 1.14.0: original pure-Python wheel contents, including LGPL 2.1 COPYING.
  Wheel SHA256 a905827335f0066af3fd416c5cf9b1f29dffaf4d0914b714555213d1809f38d4.
- PyYAML 6.0.2: pure-Python lib/yaml source and original MIT notice/metadata.
  Source archive SHA256 d584d9ec91ad65861cc08d42e834324ef890a082e591037abe114850ff7bbc3e.

No downloaded compiler, MSYS2 environment, PowerShell test runtime, fixture ROM,
converted DAT pack, completed game ELF/ISO or reference image is in the kit.
The precompiled zero-asset engine and corresponding native source are included.
