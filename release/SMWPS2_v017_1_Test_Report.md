# SMW PS2 v017.1 — Docker-free builder

The normal player workflow no longer needs Docker Desktop, WSL, a compiler,
or pip setup. Extract the kit and double-click Build_ISO.cmd. The launcher uses
an existing Python installation with tkinter, or offers an official per-user
Python installation if needed. Once Python is available, builds work offline.

The default uses a precompiled native engine with empty game-asset slots. Your
ROMs are validated and read locally, the extracted assets are inserted, and the
ISO is created and read back. The source is included, and full source compilation
remains available through the optional local/Docker backends. This default
workflow does not compile the native code on the player's PC.

A source fingerprint rejects a stale engine after native edits. The regeneration
tool builds a new empty engine from source without needing a ROM. Required
pure-Python libraries and their original license notices are included.

## Results

- The engine template's three asset arrays are completely zero-filled.
- With all three packs inserted, the resulting ELF is **byte-for-byte identical**
  to the previous source-built v017 ELF: SHA256
  619b94f3be1d5ac317ad95a39200af0738719e5623dca589b9fb196dc1a2bc3f.
- Isolated Python ROM-to-ELF/ISO builds pass for all three inputs and then US-only
  in one output folder, without installed pip packages, an SDK or Docker.
- Optional Japanese/NSS arrays are zeroed and disabled in the US-only result.
- Builder/picker regression checks and engine corruption, size, source-mismatch
  and slot-boundary checks pass.
- PowerShell parser/launcher contract tests pass with dependencies mocked,
  including first-time setup, cancellation, spaced paths and exit-code handling.
- The final ZIP is extracted into a path with spaces and its actual portable
  pipeline and focused checks are repeated before delivery.

US/Japan fixture ROMs were assembled from the disassembly and matched accepted
hashes. NSS used the supplied .ic1. No independent US/Japan dump was tested.
Actual Windows dialogs, Python download/signature/install behaviour and clean
Windows first-run execution remain untested here. Existing campaign/hardware
acceptance gaps remain. This installer update changes no native gameplay/menu.

The generated files retain the v017 names because the game itself is unchanged.
Japanese still needs its original SNES ROM; arcade needs nss-mw-0_prg.ic1.
The .ic3 companion and security.prm are not needed. No ROM is uploaded/modified.
The kit contains no ROMs, DAT packs or prepared game ELF/ISO; it includes the
compiled empty engine, corresponding source, Python tools and license notices.
See docs/V017_1_VALIDATION.md for details and release/v017_1-test-evidence/ for logs.
