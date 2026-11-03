# Source provenance and local changes

## Game references

- SMWDisX: `30643c7595a7d097d731de69476f8059c7cf98c3`
  — https://github.com/IsoFrieze/SMWDisX
- Native C engine and extractor: snesrev/smw
  `eae20c65c58930c8b62c76188d259579ad4130f1`
  — https://github.com/snesrev/smw

The native C translation is the executable game foundation. This snapshot does
not implement an assembly-to-MIPS translator. SMWDisX provides the original
behavior and variant-difference reference. Retail-only resources used during
local validation were assembled from that source with Asar 1.91 and matched the
USA SHA1 checked by the asset preparation tool. Those ROM/resources are excluded
from the deliverable.

## Included engine changes

`vendor/smw/src` contains the C game banks, engine headers and PPU/DSP/DMA code.
The CPU, SPC700 interpreter, full SNES host runner, OpenGL, configuration and SDL
frontend implementations are excluded from the native build. v013's host tests
include a separately named, test-only 65816 oracle, described below.

- `types.h`: byte-aligned legacy 16/32-bit aliases let the MIPS compiler generate
  safe accesses for odd SNES RAM addresses. The native PPU's explicitly aligned
  arrays retain their ordinary fixed-width storage.
- `snes/ppu.c`: zero-initialize the PPU allocation so its first reset does not
  preserve uninitialized drawing pointers.
- `smw_04.c`: skip overworld sprite slots with id zero before indexing their
  per-sprite flag table. The prior C translation read table index -1. SMWDisX's
  original id-zero dispatch is a return; skipping the slot preserves the no-op.
  The pose, movement-distance and path-direction tables also include the exact
  adjacent ROM bytes reachable by original out-of-label reads. This preserves
  the original lookup behavior without reading outside C array objects.
- `smw_rtl.c`: separate pixel rendering from scanline advancement so skipped
  presentations still run HDMA and native IRQ handlers. The Mode 7 IRQ path
  changes game RAM, so skipping the complete PPU frame would be incorrect.
- `snes/dsp.c`: use a direct copy at 533/534 output samples, the source indices
  selected by the existing nearest-neighbor algorithm. This avoids software
  double arithmetic for the 32 kHz/60 Hz output path on PS2.

`src/runtime_helpers.c` and `src/asset_index.c` adapt helpers from snesrev under
the same MIT license. `Load24` and indirect pointer arithmetic explicitly combine
the three address bytes rather than reading four bytes from a three-byte struct.

New `src/runtime.c` owns native engine state, asset validation, sound-driver
calls and SRAM. `src/bus.c` provides registers/DMA memory access without a CPU
interpreter. `src/ps2_main.c` owns platform initialization, input, audio output,
GS presentation, scheduling and profiling. `src/menu.c` is shared by the PS2
launcher and host visual preview.

## Build environment

The supplied ELF was built with PS2DEV's `ps2dev-ubuntu-latest.tar.gz`, downloaded
on 2026-10-28, and `mips64r5900el-ps2-elf-gcc 15.2.0`.

Toolchain archive SHA256:

`b9cf90d0934f1b452711e6a5b26ecd549db92a6ec44772a4c2fad1224d874b79`

The moving `latest` release can change. Its exact archive hash is recorded for
provenance; use a fixed local SDK/toolchain version for future release builds.
The toolchain itself is not included.

Platform source repositories inspected for API behavior and license reference:

- PS2SDK `2a18d15da1332665451a9bcf9a0d4df9029c9490`
- gsKit `8ef73d05f022d14b3ded7c70cba670fe333ed61a`

These inspection revisions do not assert that every prebuilt SDK library in
the toolchain was produced from those exact commits.

## v002 renderer and standalone changes

The original `ppu.c` is retained under `tests/reference/ppu_reference.c` as
an independently compiled raster oracle. The optimized PPU uses generated
packed-nibble bitplane expansion tables, naturally aligned graphics accesses,
an aligned 128-bit MMI clear, a palette cache and GS-native RGBA output.
Game-RAM aliases remain byte-aligned. Palette invalidation occurs in both
`RtlUpdatePalette` and PPU register $2122 writes. MMI clear instructions and
aligned buffer offsets are checked in the PS2 build/disassembly; that native
assembly path requires target execution as part of the next test.

`smw_init_memory` accepts the asset pack embedded by the build. The asset
payload matches the user-supplied file exactly. `save_journal.c` supplies the
alternating CRC-checked memory-card records. The launcher binds memory-card
services once, probes storage at game launch and uses the same native engine.

`tools/make_iso.py` uses pycdlib to create and reopen the ISO, then verifies
the boot configuration and ELF bytes. No SDK, assembler or ROM extraction is
needed to run the supplied standalone executable.

## v003 renderer and scheduling changes

The PPU builds candidate sprite lists per scanline and caches decoded OAM
attributes. Register writes that affect selection invalidate those lists;
frame start also invalidates them to account for native OAM uploads. The
candidate limit is 33 so the original evaluator can still choose whether
sprite overflow or tile overflow occurs first.

A dedicated backdrop/subscreen palette caches colour-math results when only
the backdrop participates in math. Brightness, CGRAM changes and the math
control bits invalidate the appropriate cache. Fixed colour is calculated
per span. The general colour-math path remains available for other masks.

The PS2 loop renders every completed simulation catch-up cycle and waits for
the next refresh to flip. The secondary presentation gate was removed to
avoid losing an extra refresh. Native audio, game logic and HDMA/IRQ cadence
are retained. The overlay adds APU timing; the console includes CPU and IO.

The headless runner supports per-frame pixel/RAM/PCM hashes and phase timings.
`tests/test_game.py` checks stored v002 replay digests with fresh saves;
`tests/test_ppu.c` expands the independent raster comparisons to 240 fixtures
in each output format. `docs/V003_VALIDATION.md` distinguishes host results
from remaining PS2/PCSX2 target validation.

## v004 timing and audio changes

`frame_pacer.c` counts due game updates from NTSC VBlank fields and targets a
presentation every two fields. An EE interrupt counter and coalesced semaphore
wakeup use the gsKit/PS2SDK APIs; buffer display/toggle/activation follows the
sequence in gsKit_sync_flip after the absolute field wait. There is a timer
fallback for failed interrupt/semaphore initialization. Updates are selected
before work begins, retaining skipped-frame HDMA and native IRQ handling.

`audio_queue.c` bounds EE PCM storage, schedules fractional 48 kHz output,
absorbs small clock differences through resampling, queries available IOP
room and limits submission to one bounded packet per rendered frame. It never
waits for playback-buffer space. The DSP has an integer nearest-neighbour
path for output sizes 798–804; the old 533/534 path is retained for host golden
replays. SDK audsrv source and the bundled IRX's 1364-byte return constant
explain the old 32 kHz producer/consumer mismatch.

`tests/reference/ps2_main_v003.c` retains the prior frontend, with only SDK
includes switched under the host test macro. Both main loops compile against
`tests/platform_mock.c` for deterministic clock/ring regression checks.
This model is not a PS2 emulator. Results and target limits are documented in
`docs/V004_VALIDATION.md` and `docs/v004-platform-model.json`.

## v005 PCM output-volume correction

The user confirms the v004 PCSX2 performance baseline, with quiet output as
the remaining reported issue. Existing engine, PPU/DSP, runtime, frame-pacer
and audio-queue source is retained byte for byte; the frontend changes only
its displayed version text.

`vendor/audsrv` contains the corresponding IOP service sources from the same
PS2SDK inspection pin recorded above, preserving original LGPL-2 notices.
The only existing source changed is `audsrv.c`: its volume-update function
maps the API's 0..0x3fff values to the PCM input register's 0..0x7fff positive
range through the new MIT-licensed `pcm_volume.h`. Master levels, mute and
all streaming/RPC/threading behavior remain as in the vendored SDK sources.
The Makefile builds and embeds this module with installed SDK sample rules.

`tests/test_audsrv_volume.py` compiles the actual IOP volume-update function
body against recorded libsd writes and runs the gain-range checks. See the
vendor README for register references and `docs/V005_VALIDATION.md` for scope,
results and remaining listening/hardware checks. No PCSX2 source is included
in the deliverable; its public SPU2 implementation was inspected as a reference.

## v006 native widescreen

`src/widescreen.c` and its small bank/PPU hooks implement a 342-column level
projection. Existing PPU margin scaffolding supplies scanline windows, colour
math and priorities; the normal 256-column path remains independent. Map16
lookup follows the native layer-layout tables and Map16 quadrant order.
Pipe variants use their world screen along the horizontal or vertical axis,
matching the strip streamer's pointer updates. Signed OAM ownership is paired
with the exact completed image before upload, including FinishOAMWrite and
common extended-sprite draws. Placed-actor activation uses the existing loader,
not a replacement physics implementation. No ROM or new asset format is needed.

The local PolyForm Noncommercial SuperMarioWorldRecomp renderer was inspected for viewport, Map16,
HUD and OAM-ownership design references. The PS2 implementation uses its own
bank hooks and existing optimized scanline PPU; it does not import that project's
renderer, interpreter, SDL/desktop framework or per-line VRAM snapshots.
Reference: https://github.com/mstan/SuperMarioWorldRecomp at
`324dfc8fb8359c7eb514259a16867e05e06129bb` (local source snapshot).
The coordinate/viewport/HUD adaptation in `src/widescreen.c` retains that
license, copyright and attribution; its full notice is in
`licenses/SMWRecomp-PolyForm-Noncommercial-1.0.0.txt`. This exception is separate
from the MIT native engine/platform code and LGPL audsrv service.

Broader sanitizer fixtures exposed existing C array declarations that omitted
adjacent SNES ROM bytes. The fixes preserve those source bytes explicitly:

- `smw_00.c`: PipeSpeed's two zero X speeds for vertical pipe directions;
  reject unrelated block IDs before the regular-tileset table read in Mode 7.
- `smw_02.c`: the next bytes after ReservedSprite1/2 ($FF/$D0) for memory $12.
- `smw_05.c`: the contiguous low-word layout sequence DATA_00BAD8..BC70;
  the word after DATA_05CA46 needed by a word read at byte offset one.

These references are in the pinned SMWDisX `bank_00.asm`, `bank_02.asm` and
`bank_05.asm`. Frame pacing, queueing, DSP and the v005 IOP mixer are retained.

## v007 wide-rendering performance

`src/widescreen.c` prepares contiguous coordinate bands at scanline time. Its
Map16 side descriptors use a 1.5 KiB cache with full-coordinate tags and a new
generation per draw. The existing PolyForm Noncommercial attribution remains.
`ppu.c` renders VRAM bands with the packed/unrolled native tile-row walk and
uses Map16 only for the side bands. v006's missing-side-tile skip phase is
preserved, including a skip that crosses into the native band in layer-2 rooms.

`tests/reference/ppu_v006.c` retains the previous rasterizer independently.
The paired scanline tests compare native/wide pixels in both output formats;
fixed replay goldens come from a separately rebuilt v006 source. The matched
host benchmark alternates old/new order and measures process CPU time. It
provides no R5900 or PCSX2 frame-rate measurement. Rendering, audio and timing
scope is recorded in v007-scope-check.json and V007_VALIDATION.md.

## v008 widescreen sprite lifecycle

Reference inspected through git at current upstream HEAD on 2026-10-30:
`6d54aac4935edf475daadf2eb264396ac7fabaf5`,
https://github.com/mstan/SuperMarioWorldRecomp . Its `src/smw_renderer_hooks.c`
is byte-identical to the earlier inspected snapshot (SHA256
`e9de1f0be524e13a8d85096051a6b300289a29a97a873e0e6e31326d13bcec1f`).
The upstream design is described in `docs/adaptive-renderer.md`, especially
its activation/despawn policy and successful-placement visit guard.

`src/widescreen.c` adapts that visited-record policy and offscreen-boundary
calculation to the native C engine. Its existing PolyForm Noncommercial SPDX,
Matthew Stanley copyright/attribution and full license notice are retained.
The guard uses the actual sprite-list pointer plus 128 visited bytes; a
successful native load consumes the current visit, allocation failure does
not, and a changed list or scene exit resets it. The PS2 app only stores SRAM,
not runtime snapshots, so no RTLS snapshot-format code is imported.

`smw_02.c` routes its ordinary parser through that horizontal sweep in active
wide scenes and removes the prior second loader invocation. Native actors,
shooters, shells and factories still use `LoadOneSprite`; group factories
wait if their first native slot is unavailable. Commands/generators retain
the native frontier. Bank 01/02/03 offscreen hooks use the adapted boundaries
only for active wide horizontal scenes, retaining the original fall,
vertical, parity and per-sprite keep-alive rules. No desktop runtime, generated
bank code, interpreter, snapshot renderer or SDL/OpenGL dependency is imported.

The PPU, its tile cache and packed spans, game audio, frame pacer, SRAM format
and custom IOP module are byte-identical to v007. See v008-scope-check.json.

## v009 secondary entrances and stable transitions

The supplied asset pack and an unmodified extraction were byte-identical
(SHA256 `3b66781c5c66522fc2106d400f96b56939a88c4a2128094132a9ace87f068f02`).
Both omitted all four secondary-entrance tables. The original native loader
still indexes them for ordinary secondary exits, causing reads beyond each
zero-length logical asset into unrelated following data. This explains the
platform-dependent death/bonus-room symptoms without a widescreen physics hack.

`vendor/smw/assets/compile_resources.py` now extracts the original 512 bytes
at $05:FE00, F800, FA00 and FC00 when the Lunar Magic relocated-level feature
is absent. The moved-table path remains available. These are existing format
slots 150..153, with no new indices or signature change. Their original
SMWDisX labels are DATA_05FE00/DATA_05F800/DATA_05FA00/DATA_05FC00 in bank_05.asm.
They are type, destination, Y and X respectively. The locally assembled USA
ROM matches SHA1 `6b47bb75d16514b6a476aa0c73a683a2a4c18765` and is excluded
from the bundle. All 174 other asset records remain identical.

The corrected DAT is 355868 bytes, SHA256
`ac8fb8f6c0e712ed0314cd9cbce7f9bcf89d083ee82d06e1f38f56e501de512c`.
`src/runtime.c` requires complete tables before binding the pack. No destination,
position, timer, collision, exit flag or bonus flag is patched per level.

`src/widescreen.c` adopts the existing SMWRecomp level-scene fade policy:
normal level/fade-in modes establish the projection; shared outgoing fades
retain the originating scene; loaders reset it. The reference remains
`6d54aac4935edf475daadf2eb264396ac7fabaf5`, src/smw_renderer.c. The module's
PolyForm Noncommercial attribution/license remains. Loader OAM cannot retain
a preceding room's signed coordinates merely through an identical native image.
Rendering eligibility is separate from the unchanged v008 gameplay activation.

`vendor/smw/src/snes/ppu.c` adds wide mosaic sampling using the existing tile
projection and packed bitplane decoder. Negative cell origins use floor phase,
not negative indices into the native modulo array. Native mosaic paths remain
independent. The new routine is checked against unmosaicked raster pixels,
including source cells beyond the visible left edge, across all Mode-1 layers,
sizes 2..16 and viewport offsets 0/43/86, in RGB and GS output.

Tests share their native bootstrap/room helpers through tests/native_fixture.h.
The production game banks, frame pacer, audio queue, DSP/SPC player and custom
IOP module remain byte-identical to v008. See V009_VALIDATION.md.

## v010 original Japanese presentation, Browser icon and overworld

The user's original Super Famicom ROM matches Japan SHA1
`f977afabf24ed269d86366209a460450bbc37e76`. Address and behavior references
remain [IsoFrieze/SMWDisX](https://github.com/IsoFrieze/SMWDisX), commit
`30643c7595a7d097d731de69476f8059c7cf98c3`, `_VER=0`:

- bank_00.asm: Japanese LZ reference byte order, file text/erase tiles and
  CursorCoords; bank_04.asm: LevelNames, LevelNameStrings, the kana/dakuten
  two-row name writer and particles.
- bank_05.asm: Japanese MessageBoxes, paired message rows/offsets, original
  title/file/player/save stripes and switch-block positions.
- bank_0C.asm and bank_0D.asm: original credits, castle narrative stripes and
  enemy-name stripes. English-only Special World replacement labels are
  suppressed when Japanese is selected.

`tools/japanese_addresses.json` pins only needed symbol addresses and remaps
Japanese stripe assets into the shared native engine's index order.
`tools/prepare_japanese.py` validates the supplied ROM, extracts presentation
assets and verifies lossless LZ operand conversion. `src/localization.c` uses
native C presentation branches; no CPU interpreter or GBA material is added.
The shared secondary-entrance data and other gameplay/audio asset slots remain.
Japanese pack SHA256 is
`7ec286e4fa9cd89f812e0547c172b9ffaef5090174f73cabe9fe19cefbde58e4`.

Browser metadata uses the [PS2SDK mcIcon layout](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/memorycard/include/libmc.h)
and its mc_example.c sample. The icon file-format reference is the original
[HDLGameInstaller IconLoader.h](https://github.com/ps2homebrew/HDLGameInstaller/blob/master/IconLoader.h).
No loader code or artwork is copied. `tools/generate_save_icon.py` creates the
original 3D egg, fixed-point mesh, static sequence, texture and descriptor;
`src/save_icon.c` repairs these files without opening SRAM journals.

The map extension in the existing attributed `src/widescreen.c` is a new native
VRAM projection and decorative-frame adaptation. It does not import a new
renderer or change the map simulation. The module retains its existing
PolyForm Noncommercial 1.0.0 notice. Both languages exercise all seven native
map loaders in RGB and GS formats, alongside the retained v009 regressions.
See V010_VALIDATION.md and v010-release-checks.json.

## v011 native portrait coordinates and map color blending

The user reports a black rectangle covering the map portrait and reduced map
frame rate in v010; the screenshot shows DRAW 26.7, SIM 60.1, PPU 23.6 ms,
GPU 0.6 ms and APU 3.7 ms. Other v010 behavior is reported as good.

The v010 projection moved portrait and masking sprites to negative logical X.
v011 retains native coordinates for the complete portrait/mask/lives group,
using uploaded OAM rather than actor-coordinate metadata for these UI entries.
The level name remains centered. Main-map and submap terrain projection stays.

`vendor/smw/src/snes/ppu.c` extends the existing cached backdrop/subscreen
palette path to the map's OBJ-plus-backdrop color-math configuration. Backdrop
pixels use cached results; eligible OBJ pixels retain their exact general
blend; other layers keep plain palette lookup. The appended presentation flag
is enabled only for the supported wide map scene. Clipping, fixed color,
add/subtract, halving, palette/brightness invalidation, priority and scanline
state remain authoritative. Geometry invariant during scanout is cached once
in `SmwWide_PrepareDraw`; scroll registers still resolve per scanline.

The independent `tests/reference/ppu_v010.c` is the frozen v010 raster with
exported symbols renamed. Both renderers use the corrected projection, so
their paired timing measures the blending change on identical inputs.
`tests/test_overworld.c` checks pixels, native portrait/lives, mask ownership,
both languages, seven maps, windows/math, mosaic, palette changes and drawing
skips. This code uses the existing renderer/port licenses; no new upstream
game code, graphics, translation material or external assets are imported.

## v012 message boundary and map surround

The widescreen HUD projection now ends before source scanline 40, where the
native message tilemap begins. This removes the one-scanline black overhang
without changing native message text, animation, HDMA or window geometry.
The extra map surround uses the native tile-grid phase, preserving complete
8-pixel decorative glyphs at internal joins despite the 43-pixel screen inset.
The outer display boundary still clips naturally. This is a local correction
in the existing attributed widescreen module; no new external code or assets
are imported. See V012_VALIDATION.md and v012-release-checks.json.

## v013 Nintendo Super System

`src/super_system.c` translates the SS-specific branches of the pinned SMWDisX
source into native C. Hooks in banks 00, 04, 05 and 0C select these branches;
the default retail paths remain. The module implements the original zone
selector/initialization, session buffer updates, menu/save/game-over rules,
message offsets/position/timing and cutscene input/timers. The launcher starts
either edition and reinitializes the game on a switch. Standalone arcade uses
free play; cabinet BIOS/credits are not emulated.

`tools/super_system_addresses.json` records the pinned SS program hash and
symbol addresses. `tools/prepare_super_system.py` validates the supplied SS
program and emits only its overlay. The assembled program reproduces the
original MAME-listed NSS SMW program hash. No complete ROM, new translation,
GBA material, replacement renderer or cabinet BIOS is included.

`tests/reference/ss_cpu.c` is the MIT-licensed 65816 implementation from the
existing pinned snesrev/smw source. Include paths and exported names are the
only adaptations. The frozen small SS instruction/table capsule is identified
in ss_program.json and can be reproduced with the extractor's
`--reference-output` option. It is used only for independent host comparisons,
never linked into the native PS2 executable. See V013_VALIDATION.md.

The runtime additionally clears SPC sample/echo memory on a new session.
The launcher flushes queued audio on edition/language restart. Both retain the
original native sound synthesis and output gain; there is no extra in-game pass.

## v014 reported-scene fixes

The map camera return in `GameMode0E_ShowOverworld_Bank04` now extracts the
high/sign byte of each 16-bit saved-camera delta. SMWDisX's original bank-04
scratch stores and `_1,X` reads provide the reference. The previous native C
translation truncated the low byte into each direction slot, sending the
camera away from its saved position after a long pan. Both editions share it.

`GenericGFXRtMoveTileOffscreenVertically` preserves native X=$180 (-128) as
explicit signed OAM metadata when it hides either tile. Inferring a signed
horizontal position from that deliberate hide revived the wrapped Y tile of
an above-view P-switch in Chocolate Island 5. Native RAM/OAM writes and the
existing actor/spawn logic are unchanged.

The Mode-1 BG3/backdrop colour-math path adds a lazy 32-by-256 packed palette
cache, using the existing exact blend function. Palette, brightness and math
control invalidate rows; fixed-colour fallback refreshes per span. CGRAM
register writes retain the byte latch, pointer wrap and full raw 16-bit values,
but invalidate the palette only when that value changes. This avoids throwing
away the water cache for redundant native palette uploads. Cache fields are
appended outside the PPU's saved hardware data layout.

`tests/reference/ppu_v013.c` is the previous source raster, with only public
symbols renamed, following the existing frozen-raster regression convention.
It is never linked into the PS2 executable. `test_water_math.c` compares real
scenes and register writes with it. `test_v014_reports.c` uses native map and
level loaders, long controller-driven pans and the real Chocolate Island 5
P-switch scene. The same tests reject the original map and sprite defects
when each original bank is independently linked back into the host harness.
No new upstream code or game asset is imported by v014.

## v015 Yoshi camera-wrap fix

The native player graphics entry `PlayerGFXRt_01EA70` draws Yoshi before the
normal sprite loop. That delayed pass was outside widescreen actor tracking,
so its generic head/body graphics used truncated OAM X instead of signed
coordinates. Extending horizontal visibility exposed the aliases at X +/-256.
The pass is now bracketed by the existing actor begin/end helpers. In
`Spr035_Yoshi_HandleOffYoshi`, final dynamic-tile replacement also refreshes
the exact metadata for either native vertically hidden tile; native X=$180
must stay offscreen when the tile image changes. No new reference code or
assets are imported. Physics, world positions, collision and native OAM writes
are unchanged by these presentation annotations.

`tests/test_yoshi_wide.c` covers the actual delayed graphics entry, isolated
sprite raster positions and native Chocolate Island 5 carry-in/dismount/camera
runs. Its original-bank negative check reproduces the v014 failure. Existing
v014 data and runtime modules are retained; see V015_VALIDATION.md.

## v016 release tooling and native safety fixes

The new room catalog is derived from the original bank-05 Layer1Ptrs labels,
with only the common TestLevel placeholder excluded. The native batch harness
uses real loaders/input, ASan/UBSan and process isolation. It is explicitly a
controlled room smoke test rather than a campaign-completion oracle.

The broad sweep exposed original out-of-label reads that were unsafe as C
array accesses: fish X speeds, layer-2 scroll direction words and Mario pose
$46. Their exact adjacent ROM bytes are retained. The missing three canvas
variants are restored from DATA_0DE05E..DATA_0DE0A9. Original bank-0D MVN
instructions establish forward copying for the overlapping canvas screens.
The restored ranges can be checked with tools/verify_safety_tables.py and the
user's clean USA ROM. No gameplay-rule replacement is introduced.

The ROM builder extracts the existing packs from validated user inputs into
an isolated source tree and cross-compiles the existing native port. Japanese
and arcade arrays are optional weak references for USA-only builds. Public
packaging excludes ROMs, extracted DATs (including the test instruction capsule),
compiled game outputs and game screenshots; all existing notices are retained.

Official compiler/container references:
https://github.com/ps2dev/ps2dev
https://github.com/ps2dev/ps2dev/pkgs/container/ps2dev/870369059?tag=v2.0.0

The Dockerfile pins the published v2.0.0 multi-architecture manifest
sha256:8e6dc4564598ecce94d078d3111b8c4803f88faa94a6f183ac60240a0ae010a1.
The local validation SDK archive SHA256 is
 a49d9f066771402d7bf54d092efa77e8bc4bd0bdaf2292e4aa37642865ad172d,
with EE GCC 15.2.0. The actual Docker/Windows route has not been executed here.

## v017 arcade picker and menu

User-supplied NSS inputs establish the `.ic1` program filename and companion
hashes used for clear input errors. Only the validated 512 KiB program is needed
by this native free-play port. User-supplied packaging references inspired the
launcher's yellow/blue palette, multicolor title and star/block decoration.
All decoration is code drawn from rectangles; no scan/image pixels are imported.
The original compact menu font is retained. No native engine or game asset
change is introduced. See V017_VALIDATION.md for scoped test results and limits.

## v017.1 portable player build

The included native engine is compiled from the unchanged v017 C sources with
zero-filled asset inputs. It contains native code, existing runtime modules and
empty asset slots. User ROM data is inserted locally. Full source compilation
remains available; tools/rebuild_portable_engine.py regenerates the blank engine.
The existing license and attribution files are retained.

Pure Python pycdlib 1.14.0 and PyYAML 6.0.2 sources/notices are included under
vendor/build_py. Their source artifacts and hashes are recorded in
V017_1_VALIDATION.md. The optional Windows first-run Python installer is obtained
from python.org and its Authenticode publisher must be Python Software Foundation.
It is not bundled, and no Docker/WSL/compiler installer is invoked by the player
launcher. No new game assets or reference-image pixels are added by this update.


## v018 loader/card startup and clean launch UI

Standalone builds initialize/rebind the IOP RPC clients and use the existing
PS2SDK sio2man, padman, secrman, mcman and mcserv modules rather than layering
old ROM SIO2/MC services over the launcher's device stack. File-backed developer
builds retain host: and the legacy startup path. The actual SDK IRX identities
include USB_driver, bdmff, mcman_cex and secrman_special; loaded instances are
reused through libpatches smod_get_mod_by_name. iomanX is loaded explicitly
because ROM IOMAN shares its module identity. OPL can restore drivers during
an IOP reset, so the reset does not imply removing all loader hooks.

Primary references used to review this startup path (no OPL/wLaunchELF code
was copied into the project):

- https://github.com/israpps/wLaunchELF_ISR/blob/master/embed.make
- https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/memorycard/include/libmc.h
- https://github.com/ps2homebrew/Open-PS2-Loader/blob/master/ee_core/src/iopmgr.c
- https://github.com/ps2homebrew/Open-PS2-Loader/blob/master/ee_core/src/syshook.c

The card-info RPC is polled with MC_NOWAIT for up to two seconds. Its outputs
have static lifetime, because a delayed completion can arrive after the launch
wait expires. Timeout disables further card/filesystem probing for that session;
normal completed probes still select a formatted physical PS2 card and the
existing icon/journal path. Reset request/sync retry loops have three-second
deadlines; these do not bound a loader hook or SDK call that itself never returns.
No claim is made that every possible IOP hang is recoverable.

The version text is removed only from the launcher badge. Versioned filenames,
build manifests and opt-in diagnostics remain. FPS/debug drawing and periodic
PERF log output start disabled; the existing L2+R2 toggle and Start return chord
are retained. No game-bank, renderer, audio mixer or save-journal code changed.
See V018_VALIDATION.md for scoped evidence and pending hardware acceptance.
