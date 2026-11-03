# SMW PS2 v018 — MX4SIO launch candidate

This build targets the freeze after choosing a game on MX4SIO. Standalone
startup now rebinds IOP RPC clients, uses SDK card/pad drivers and reuses relevant
loader-resident modules. Card-info polling has a timeout; a stalled request
falls back to session-only play instead of waiting indefinitely. Existing normal
physical-card saves, icon and journal remain in use.

The top menu no longer shows v017. FPS/debug information starts hidden; press
L2+R2 to toggle it. L2+R2+Start still returns to the launcher.

The native source build, ten ASan/UBSan frontend fault/control scenarios,
launcher flow, scheduling model and portable builder checks pass. Real all-ROM
then US-only builds pass without Docker, compiler setup or pip packages. The
ISO boot executable and configuration are read back and checked. The ZIP also
passes real builds from an extracted path containing spaces. Fixture ROMs
were assembled from SMWDisX and matched accepted clean hashes.

Actual MX4SIO, PS2 and PCSX2 execution of v018 is not available here. This is a
candidate for your hardware retest, not a verified hardware fix. Please try the
same MX4SIO setup that froze, then Original and Super System launch, a level,
launcher return/resume and Original save/reload on a physical PS2 card. Also
check USB/PCSX2 startup after the driver change.

The public Build Kit remains Docker-free and contains no ROMs, extracted DATs
or prepared game ELF/ISO. Users provide their own US ROM, with original Japanese
and nss-mw-0_prg.ic1 inputs optional. Full source and license notices are included.
Detailed evidence and limitations are in docs/V018_VALIDATION.md and
release/v018-test-evidence/. Existing campaign-coverage gaps remain.
