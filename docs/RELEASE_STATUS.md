# Release status — 2026-11-03

The feature set is at playable-beta stage. Native gameplay, sound, controls,
saving, optional widescreen, Original English/Japanese and the Super System
launcher/session rules are implemented. This is substantially beyond a
prototype. The main remaining work is release acceptance and broader gameplay
coverage; a precise completion percentage would overstate what is known.

The user confirmed that v021 boots and plays via MX4SIO on their PS2.
v022 restores Original memory-card saves after that same clean IOP reset
and fixes the main-map window that clipped Mario on the approach to Yellow
Switch Palace. See V022_VALIDATION.md for test scope and hardware checks.

## What automated checks establish

The retained v016 engine test results include targeted renderer, input, audio, save, language,
arcade, transition, pipe, map, message and Yoshi regressions. The v016 controlled
room sweep covers 235 non-placeholder room-table entries in three editions and
two display modes. The final ASan/UBSan pass ran 240 updates per case where the
scene remained active: 1,410 passing cases and 335,962 native simulation updates.
Some entries are aliases or development rooms. These are short loader/input
checks rather than routes from the title to every exit.

The expanded checks found five safety issues: vertical-fish table bounds,
layer-2 direction-table bounds, overlapping canvas copies, three missing canvas
tile variants, and Mario pose $46 reading beyond two C tables. The fixes retain
original ROM bytes and forward MVN semantics. The final sweep reports none of
these memory errors. Exact reference/golden tests also remain in place.

## What is still unproven

- v022 map appearance, save persistence and Browser icon on physical PS2.
- Full normal/secret-exit progression, dynamic Chocolate Island routes, every
  boss phase, castle destruction, ending and credits in their campaign context.
- Every special sprite/effect and long gameplay session on physical hardware.
- A uniform 30 displayed fps across the entire game on real PS2 models.
- Clean-install Windows launcher/Python-setup execution of the public builder.
  The v017.1 default uses a precompiled zero-asset engine and requires no Docker.
  Advanced Docker source compilation remains optional. The real Linux
  extraction/compiler/ISO pipeline is tested; Docker command contracts are
  tested with a mock client. No actual Docker or Windows GUI run is claimed.

## Practical release gates

For a public beta, first have a tester run Build_ISO.cmd with only a USA ROM,
boot the generated disc and check controller/audio, one level, saving/restart,
launcher return and widescreen. Repeat a build with optional ROMs to confirm
those cards/languages. This is a short smoke test, not a request to play the
whole campaign. Label the release beta and include the supplied bug-report
form so community testers can split worlds, bosses and secret exits.

For 1.0, record campaign/exit coverage and representative hardware results,
resolve reported blockers, and rerun the automated suite. Do not equate a
clean sanitizer result with a guarantee that the game has no bugs.

## Public packaging

Distribute SMWPS2_v022_Build_Kit.zip. It contains source, scripts and the
compiled engine with empty asset slots, but no ROMs, extracted DAT packs,
prepared game executables/discs or game screenshots.
Users supply the USA ROM; Japanese and NSS content require their optional
matching ROMs. A source-tree copy without pre-existing DATs is made for each
build. Keep the included upstream license and attribution files.
