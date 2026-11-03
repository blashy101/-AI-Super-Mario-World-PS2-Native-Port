# v016 validation — 2026-11-02

v016 adds a ROM-to-ELF/ISO build kit and broader native-engine room testing.
It also fixes five memory/copy issues found by that testing. No new physical
PS2, PCSX2, Windows GUI or actual Docker-container run is claimed.

## Controlled room sweep

The catalog comes from the pinned SMWDisX bank-05 Layer1Ptrs table. It excludes
the common TestLevel placeholder only: 235 numeric entries, including aliases
and a development slope room. Each batch starts from a real native title/map/
level context, then uses the shared loader and deterministic controller input.
Each room advances sound and rendering as well as simulation. Timeouts and
process failures are recorded separately, and later rooms continue in a fresh
process after a crash. This is not a complete campaign or exit-graph test.

The initial ordinary 120-update sweep passed 1,410 cases. Sanitizers exposed
unsafe accesses that an ordinary run could miss. The longer, final 240-update
ASan/UBSan sweep passes all 1,410 cases, covering Original English/Japanese and
Super System in 4:3/16:9, with 335,962 simulation updates before scene changes.
A separate targeted sweep of the six affected room IDs passes all 36 cases.

## Native safety fixes

- Vertical Cheep Cheep indexes 0..3 into its X-speed data. The last two original
  bytes are zeroes adjacent to the original two-byte label; the C table now
  contains all four bytes ($01B01D).
- Layer-2 vertical-scroll initialization performs byte-indexed 16-bit reads
  across a five-byte label. Index 4 includes the following table's first byte.
  The C representation retains all six bytes ($05CA5C) and assembles the word
  explicitly, including the original $0100 result.
- Large-canvas screen copies use original MVN forward-copy semantics. A
  433-byte copy at a 432-byte stride overlaps by one byte. The explicit forward
  loops preserve propagation; memcpy was undefined and memmove would have a
  different overlap result.
- The canvas object has four 19-tile variants. The C table retained only the
  first; all 76 original bytes ($0DE05E) are restored.
- Mario pose $46 reads the next original byte after two nominal 70-byte tables.
  Both C tables now retain that byte ($00DCEC and $00E18E).

verify_safety_tables.py compares these five restored ranges directly with a
validated user-supplied USA ROM. No arbitrary clamping or replacement tile data
is used. Original bytes, not C-object/linker placement, now determine results.

## ROM build workflow

The builder validates a clean 512 KiB USA ROM, strips an optional 512-byte
copier header and stages source without any DAT/ROM files. Optional Japan/NSS
ROMs are validated independently and extracted only when provided. Optional
embedded arrays are weak references and the target makefile embeds only packs
that exist. A USA-only frontend runs against the actual SDK mock with both
optional symbols absent. The normal full launcher flow also passes.

A real PS2DEV/EE GCC 15.2.0 source build with only the USA input produces a
native R5900 ELF and verified boot ISO. A second build with all three validated
inputs also produces an ELF/ISO with the correct overlays. The public archive
is additionally extracted and built from a headered USA ROM as a clean-package
check; its final outcome and hashes are in the build/test reports.

ISO BOOT.ELF and SYSTEM.CNF are read back byte-for-byte; output hashes are checked
against the build report. Game assets are embedded in the user's generated ELF.
Build reports record input, source and output fingerprints. Original ROM files
are read only. The Docker image-build context contains only its Dockerfile;
source/ROM mounts are read-only and the output mount is separate.

Windows Build_ISO.cmd offers file dialogs through Python and uses a pinned
official PS2DEV v2.0.0 container. Its mount/argument/context behaviour is checked
with a contract-test client, including paths with spaces/commas. This is not
an actual container run: Docker is unavailable in the test environment.
A real Windows/Docker acceptance run remains a beta gate.

## Retained validation and limits

The existing reference raster, projection/lifecycle, pipes/fades/mosaic,
Japanese, overworld/messages, arcade, Yoshi, Browser icon, journal, launcher,
volume and timing-model checks pass. Every-frame v002 retail pixel/RAM/PCM
and v006 title goldens remain exact; the 7,200-update wide replay remains
identical across full/alternate drawing. An initial host-test execute-bit issue
was repaired and the affected/unstarted suites were rerun; both logs are kept.

The native game, PPU/DSP, cadence and save architecture are retained. Full
campaign progression, dynamic route selection, authentic boss-entry context,
visual review and hardware frame rates require further acceptance work.
RELEASE_STATUS.md gives a short public-beta gate and a 1.0 checklist.
