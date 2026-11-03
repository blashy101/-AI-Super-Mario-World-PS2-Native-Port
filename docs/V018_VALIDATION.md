# v018 validation — loader/card startup and clean launch UI

v018 is a hardware-test candidate for the reported MX4SIO freeze after launcher
selection. The user confirmed the preceding build with PCSX2 and USB on PS2.
The exact MX4SIO hardware failure cannot be reproduced in this environment;
no v018 PS2, MX4SIO or PCSX2 runtime pass is claimed.

## Changes

Standalone builds reset/rebind IOP RPC clients and use bundled PS2SDK SIO2,
pad, security and memory-card modules instead of the previous ROM services.
Services reinjected by an OPL-style reset are reused by actual IRX identity.
iomanX is loaded explicitly to avoid mistaking ROM IOMAN for its bridge.
File-backed developer builds preserve host: and their prior startup path.

Card-info requests now use MC_NOWAIT with a two-second polling limit. On
failure/timeout the game can start in session-only mode, without more probes
through the potentially pending request. The RPC output storage has static
lifetime so a late completion cannot write into an expired stack frame. Normal
completed probes still use a formatted physical PS2 card, existing icon and
save journal. Normal mounted mass: fallback is retained. This does not certify
MX4SIO save storage or OPL VMCs. The reset request/sync retry loops have
three-second deadlines, which cannot bound a loader hook that never returns.

The menu badge says PLAYSTATION 2 with no version number. The FPS/debug overlay
and periodic PERF log output start off. L2+R2 toggles the overlay; adding Start
still returns to the launcher without also changing its state. Game banks,
renderer, audio mixer, save format and journal implementation are unchanged.

## New results

- Source cross-build with EE GCC 15.2.0 passed. The official Linux SDK archive
  SHA256 is a49d9f066771402d7bf54d092efa77e8bc4bd0bdaf2292e4aa37642865ad172d.
- Empty engine: 1,840,472 bytes,
  SHA256 `99b6ff81d38280e7f1750e356fe86b228b4db2a6a66abea67a47bd67cdaf2526`. All three asset arrays are zero before packaging.
- Ten actual-frontend SDK-facade scenarios pass with ASan and UBSan, including
  stack-use-after-return checking: formatted card; stuck request; failed RPC;
  unformatted/absent card; stuck reset sync; missing MC module; overlay controls;
  rejected reset request; reinjected resident drivers; file-backed host: startup.
  LeakSanitizer is disabled because this restricted runner cannot inspect its
  thread list; address, stack-lifetime and undefined-behavior checks remain on.
- The stuck request takes 121 nonblocking polls and then gameplay runs. A late
  mock RPC completion writes to the retained output addresses without a sanitizer
  error. The overlay counts in stable input phases are 0,10,5,0: off initially,
  on after L2+R2, still on after launcher/resume, then off after the next toggle.
- Actual launcher button flow Original -> NSS -> Japanese -> NSS -> English
  passes, including Circle resume and the arcade language lock.
- Eight 30-minute scheduling-model cases pass at both widths, audio clocks at
  +/-2000 ppm and timer fallback. First/last rates are 29.92/29.97 displayed fps,
  simulation 59.939 Hz, with no modeled underflows, waits or short writes.
  This is a regression model, not real PS2 performance evidence.
- ROM validation, .ic1 selection/retry, source staging, optional-symbol handling
  and explicit Docker command contracts pass. Docker calls remain mocked.
- Empty-engine boundary, size, optional-clearing, hash and native-source
  mismatch checks pass. Only asset ranges/lengths change when filling the engine.
- Real portable builds pass for all three inputs then USA-only in the same output
  folder, in a no-pip Python environment with no PS2 compiler on PATH or Docker.
  The US input has a copier header. All three fixture ROMs were assembled from
  SMWDisX commit 306fdc7595a7d097d731de69476f8059c7cf98c3 using Asar source
  5fd539cd510e4a26afef66fc14e35ca6d8ff0497 and match the accepted SHA1s.
  They are not independent original dumps. Resulting packs match those recovered
  from the previous game ELF exactly; optional arrays are zero/disabled in the
  subsequent US-only output.
- ISO creation reads BOOT.ELF and SYSTEM.CNF back. Independent delivery checks
  confirm embedded packs and BOOT.ELF against the final game executable.
- The release ZIP is extracted into a path with spaces. Actual all-input and
  US-only builds pass again with automatic backend selection, the same isolated
  Python and a reused output folder. The all-input ELF equals the delivery ELF;
  the US-only optional slots are cleared. Extracted portable-engine and builder
  regression scripts pass. Native source/tool files match the tested originals.
- The code-drawn production menu preview is inspected at 640x448.

## Delivery hashes

| File | Bytes | SHA256 |
| --- | ---: | --- |
| SMWPS2_v018.elf | 1,840,472 | `ecc63660c604ba02230b34e90f7f1a57999f6b4e31829899dfcfdca4f34921e9` |
| SMWPS2_v018.iso | 18,671,616 | `0257776e05fb57c6ea5582ac977bec9d8a4c0ca99bfb341be16e71a32c2fe1dc` |

## Acceptance still needed

Boot this v018 ISO through MX4SIO with the same loader/settings used for the
failing disc. Try Original English, Japanese if supplied, and Super System;
check a level, Start+L2+R2 return/resume, the overlay toggle, and Original
save/reload using a separate formatted physical PS2 card. Also repeat a short
USB/PCSX2 smoke test because startup services changed. The existing broad
v016 room/sanitizer evidence is retained as history; this update does not
establish campaign completion or universal hardware compatibility.

The public kit contains the matching empty engine, source, tools and notices,
but no ROMs, DAT packs or prepared game ELF/ISO. Normal player builds remain
Docker-free. The retained Windows launcher is not newly executed on Windows.
See release/v018-test-evidence/ for the scoped logs.
