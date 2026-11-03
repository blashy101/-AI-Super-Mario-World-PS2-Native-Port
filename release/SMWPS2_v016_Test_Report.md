# Super Mario World PS2 v016 — release and test report

Date: 2026-11-02. Status: **playable beta; release acceptance pending**.

The native port, optional widescreen, Original English/Japanese options and
Super System launcher are implemented. The remaining work is broader gameplay
coverage and release acceptance. A precise completion percentage would suggest
more certainty than the available evidence supports.

## What changed in v016

- Added a local ROM-to-source-build-to-ELF/ISO tool, strict ROM validation,
  optional copier-header removal, build fingerprints and ISO readback checks.
- Added a Windows launcher with file dialogs and a pinned PS2DEV Docker build
  environment. This Windows/container path still needs an actual acceptance run.
- Added a source-only public package and a bug-report template.
- Expanded room testing and fixed five native memory/copy issues, matching
  original ROM tables and 65816 forward-copy behaviour.

The public kit excludes ROMs, DAT asset packs, generated game ELFs/ISOs and game
screenshots. Each build extracts assets from the user's own inputs into a fresh
working tree. Existing DAT files cannot leak optional content into a US-only
build. The input ROM is read only and is not uploaded.

## Results

| Check | Observed result | What it establishes |
| --- | --- | --- |
| ASan/UBSan room sweep | 1,410/1,410 cases pass; 335,962 native updates | No detected memory/undefined-behaviour errors in these short scenarios |
| Room coverage | 235 numeric room-table entries, 193 unique labels, three editions, two widths | Broad loader/input coverage; includes aliases and a development room |
| Targeted safety recheck | 36/36 cases pass | Six affected room IDs across all editions and widths |
| Existing regressions | Pass | Renderer, messages/maps, pipes/transitions/mosaic, language, arcade, Yoshi, save metadata/journal, launcher and audio checks |
| Deterministic replays | Pass | 7,200-update wide RAM/PCM replay across drawing schedules; retained retail/title reference hashes |
| Timing model | Pass | Modelled 30-minute cadence, audio-clock drift and fallback; no hardware performance claim |
| Real PS2 cross-build, US only | Pass | Native R5900 ELF and verified ISO produced from the extracted public kit using a headered US input |
| Real PS2 cross-build, all inputs | Pass | ELF/ISO produced with US, Japan and NSS inputs |
| Builder isolation/error checks | Pass | Optional-symbol absence, bad ROM rejection, output hash validation and no optional DAT leakage |
| Docker command contract | Pass with fake client | Mount quoting, read-only source/ROM mounts, separate output and Dockerfile-only image context |
| Actual Windows/Docker execution | **Not run** | Still requires a clean-install acceptance test |
| v016 physical PS2/PCSX2 gameplay | **Not run here** | Earlier user observations are useful, but do not validate the new binary |

The initial ordinary room sweep passed, but sanitizers then exposed real errors.
Five issues were corrected: vertical-fish table bounds; Layer-2 direction-table
bounds; overlapping canvas copies; three missing canvas tile variants; and
Mario pose $46 reading beyond two truncated C tables. Original byte ranges are
verified by `tools/verify_safety_tables.py`; details are in
`docs/V016_VALIDATION.md`.

An initial host suite stopped because one test executable lacked its execute
bit. The affected and unstarted suites were rerun successfully after correcting
the mode. The original and follow-up logs are included as evidence.

## Limits of the automated testing

Each room receives deterministic controller input for up to 240 updates. Some
scenes change mode earlier. The loader uses a controlled native context rather
than reaching every room through normal campaign progression. Tests include
simulation, rendering and sound; they do not solve levels or prove that every
exit, puzzle, dynamic Chocolate Island route, boss phase, castle sequence or
ending is completable. A clean sanitizer run is not a guarantee of zero bugs.

Host execution and a cadence model do not prove real-PS2 frame rates. The actual
Linux PS2DEV extraction/compiler/ISO pipeline was tested. Docker and Windows GUI
execution were not available. Integration inputs were assembled from the pinned
SMWDisX source and matched the accepted clean-ROM hashes; an independent external
ROM dump was not part of this validation. ISO timestamps may produce different
hashes on repeated builds.

## Building

On Windows, install Python 3 with its `py` launcher, start Docker Desktop in
Linux-container mode, extract `SMWPS2_v016_Build_Kit.zip`, then double-click
`Build_ISO.cmd`. Select a clean US SMW ROM and an output folder. The tool builds
`SMWPS2_v016.elf` and `SMWPS2_v016.iso` and writes a build JSON plus logs.

A US ROM supplies Original English. Original Japanese needs its optional SNES
Japanese ROM; Super System needs its optional NSS game-program ROM. GBA content
is not used. Patched and other-region inputs are rejected. Arcade is a free-play
game/session port; cabinet BIOS and coin-countdown overlays are outside scope.
The kit README lists accepted hashes and local-SDK/command-line instructions.

## Short public-beta gate

Before a public beta, ask one tester to run the Windows builder from a clean
setup, boot the generated US-only ISO, check one level in both widths, audio,
launcher return, save and restart. Check a second build with the optional inputs
if those modes will be advertised. This does not require you to play the whole
campaign.

Then label the release beta and split worlds, bosses and secret exits among
community testers. Use the included bug-report form and request each generated
build JSON. A 1.0 release should follow recorded campaign/exit coverage and
representative physical-hardware results, with blockers resolved and automated
checks rerun.

The kit contains the machine-readable summary, build fingerprints, full final
room results and selected regression logs under `release/test-evidence/`.
