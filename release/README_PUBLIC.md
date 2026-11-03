# Super Mario World PS2 — v022 portable build kit

The native game, optional widescreen and Original/Super System launcher are
implemented. This is a playable beta, with automated tests and known validation
gaps; it is not a completed full-campaign certification.

This kit contains source, an engine with empty asset slots, pure-Python build
tools and notices. It excludes ROM files, extracted DAT packs, prepared game
executables/discs and game screenshots.
Users supply their own ROMs and build locally. No Game Boy Advance material is
used.

## Windows quick start — Docker is not required

1. Extract **SMWPS2_v022_Build_Kit.zip** and double-click `Build_ISO.cmd`.
2. The launcher uses an existing Python 3 with tkinter. If missing, it offers
   to download and install official Python for your account.
3. Select your clean USA Super Mario World `.sfc` or `.smc` ROM.
4. Optional prompts accept the Japanese ROM and/or **nss-mw-0_prg.ic1** (512 KiB).
   The `.ic3` companion and `security.prm` are not required.
5. Choose an output folder. The tool extracts assets locally, fills the included
   engine's empty asset slots and creates a verified ELF and ISO.

This player workflow needs **no Docker Desktop, WSL, compiler or pip setup**.
Once Python is available, it works offline. Required pure-Python libraries and
notices are included. The ROM is never uploaded or modified.

The builder writes `SMWPS2_v022.iso`, `SMWPS2_v022.elf`, a build JSON
and logs. The bundled engine template has zero-filled game-asset slots.

The normal workflow uses **precompiled native code**; it does not compile the
source on the player's PC. Full source and an advanced rebuild path are included.
A native-source fingerprint prevents silently using an old engine after edits.

Dragging a US ROM onto Build_ISO.cmd builds Original English directly. For all
inputs from the command line:

```bat
py -3 tools\build_from_rom.py "SMW-USA.sfc" --japanese-rom "SMW-Japan.sfc" --super-system-rom "nss-mw-0_prg.ic1" --backend portable --output output
```

The no-Docker extraction/engine-fill/ISO pipeline was tested in an isolated
Python environment without installed pip packages or an SDK. The Windows
launcher was syntax/contract tested with dependencies mocked; a real Windows
first-run installer/GUI test is still needed.

## ROM inputs and included modes

| Input | Result |
| --- | --- |
| USA ROM, required | Original English, 4:3/16:9, saves and launcher |
| Original SNES Japanese ROM, optional | Japanese option for Original |
| NSS `nss-mw-0_prg.ic1` game program, optional | Super System arcade option |

A USA ROM alone does not provide the Japanese or NSS-specific asset overlay.
Those options remain unavailable unless the corresponding optional input is
supplied. Arcade mode uses free play and session progress; cabinet BIOS,
coin/credit countdown and instruction overlay are outside this port.

Supported SHA1 hashes, after removing an optional 512-byte copier header:

- USA: `6b47bb75d16514b6a476aa0c73a683a2a4c18765`
- Japan: `f977afabf24ed269d86366209a460450bbc37e76`
- NSS game program: `06a6efc246c6fdb83efab1d402d61d2179a84494`

Patched, PAL, expanded and ROM-hack inputs are currently rejected. The tool
reports the expected hash rather than silently building from another revision.

## Advanced source compilation (optional)

For a full native-source build, set PS2DEV/PS2SDK/GSKIT as described by PS2DEV, then:

```sh
python3 tools/build_from_rom.py SMW-USA.sfc --backend local --output output
```

Every build stages a fresh source tree without pre-existing DAT files. Optional
packs cannot accidentally leak from an earlier full build into a USA-only one.
The explicit `--backend docker` option is also retained for developers who
already use Docker. It is not used by Build_ISO.cmd. The developer container
is pinned as recorded in release/Dockerfile and docs/SOURCES.md.

After editing native code, regenerate the portable template with a local SDK:

```sh
python3 tools/rebuild_portable_engine.py
```

This builds from zero-filled asset inputs. No ROM is needed to rebuild the
empty engine. The manifest is generated from the actual source and ELF symbols.

## MX4SIO and memory cards

Load the ISO in OPL using your usual MX4SIO setup. The ISO starts with an
IOP reset that detaches the resident MX4SIO driver after the game is in RAM.
The launcher displays **MX4SIO ISO / CLEAN IOP** after the reset. Start
Original with a formatted PS2 memory card in the free card slot. The game
uses the existing `SMWPS2` save folder and repairs the Browser metadata
there. The save appears as **Super Mario World** with a Yoshi egg icon.

## Playing and reporting

Cross starts/resumes; Square changes 4:3/16:9 in the launcher; Triangle changes
Original language when Japanese is included. L2+R2+Start returns to the launcher.
L2+R2 toggles the FPS/debug overlay, which is off at startup.
Match the TV/PCSX2 aspect to the selected mode. Output remains NTSC 480i.

Original uses a formatted physical PS2 memory card first. USB builds can
fall back to writable mass storage. With MX4SIO, an absent card or a card
probe timeout leaves progress in the current session. Arcade uses session
progress. Title/intro and unsupported Mode-7 scenes retain native framing.
The target is about 30 displayed frames and 60 game updates per second; room
host-test speed is not PS2 performance evidence.

For a public beta, distribute this portable build kit and ask testers to generate their
own files. Include the license/notices folder. The bug-report template requests
the generated build JSON, platform, edition, display mode, level and reproducible
steps. Generated game ISOs/ELFs contain extracted game assets and are not part of
this portable distribution kit.

## Automated testing

Tests can detect crashes, invalid memory accesses, input/transition regressions
and many rendering mistakes. They do not prove every puzzle, exit and boss can
be completed, nor do they measure real-PS2 performance.

To prepare local assets for host tests, run the extractors against your ROMs:

```sh
python3 tools/prepare_assets.py SMW-USA.sfc
python3 tools/prepare_japanese.py SMW-Japan.sfc
python3 tools/prepare_super_system.py nss-mw-0_prg.ic1 --reference-output tests/reference/ss_program.dat
python3 tools/run_room_tests.py --sanitizers --frames 240
```

With only a USA ROM, omit the optional extractors and use `--editions en`.
The room runner needs a host C compiler/GNU make with ASan/UBSan support. It
covers 235 non-placeholder room-table entries in controlled loader contexts,
with deterministic inputs, both widths and the selected editions. Some entries
are aliases or development rooms. Dynamic route choices, full boss setup and
campaign progression require targeted routes and human testing.

See `docs/V022_VALIDATION.md`, `docs/V020_VALIDATION.md`, `docs/V019_VALIDATION.md`, `docs/V018_VALIDATION.md`, `docs/V017_1_VALIDATION.md`, `docs/V017_VALIDATION.md` and `docs/RELEASE_STATUS.md` for the exact results,
remaining work and suggested public-beta checklist.
