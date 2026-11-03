# SMW PS2 v017 — installer and menu update

Date: 2026-11-02. Status: playable beta.

The arcade picker now asks for **nss-mw-0_prg.ic1 (512 KiB)** and shows `.ic1`
files directly. The companion `nss-r__ic3__mw.ic3` and `security.prm` files are not
needed by this native port. Selecting either shows a clear explanation and
returns to the picker. A matching renamed program remains valid; the tool checks
its hash, not just its filename. CLI help and the public README are updated.

The opening menu replaces the previous dark palette with yellow and deep blue,
multicolor title lettering, repeating stars, question blocks, a green arcade
header and a brick footer inspired by the supplied packaging references.
The familiar-adventure/new-home tagline is removed. Decoration uses the existing
rectangle renderer and needs no imported scan or texture. Controls and game
behaviour retain their existing rules.

## Verification

- The supplied 524,288-byte `.ic1` matches the supported program SHA1 and produces
  the same 12,812-byte NSS pack as the previous working arcade build.
- Both supplied companion files get the intended rejection/explanation.
- Focused builder checks pass, including optional-pack absence, output hashes
  and Docker mount/context behaviour. Docker is represented by a fake client.
- The actual dialog flow passes with mocked tkinter APIs, including a mistaken
  companion selection followed by the correct `.ic1`.
- The production launcher passes the English/arcade/Japanese/arcade/English
  switching and return/resume checks.
- Both menu-card selections were rendered by the production drawing code and
  inspected at 640x448. The preview is a host render, not a PS2 screenshot.
- A native PS2DEV/EE GCC 15.2.0 build passes the R5900 ELF checks. The ISO's
  BOOT.ELF and SYSTEM.CNF are read back byte-for-byte.
- The public kit is checked for forbidden game files and its extracted focused
  tests pass. It excludes all ROMs, DAT packs, generated game ELFs/ISOs and scans.

The downloadable test ELF/ISO includes English, Japanese and Super System. Its
US/Japanese DATs were recovered unchanged from the earlier working v015 ELF;
the NSS DAT was verified against the program supplied for this iteration.

The native game/renderer/audio/save/controller sources are unchanged from v016;
only menu drawing and diagnostic version labels differ. The earlier 1,410-case
sanitizer sweep is retained as **v016 evidence**, not claimed as a new v017 run.

Actual Windows/Docker execution and v017 gameplay on physical PS2/PCSX2 have not
been tested here. Campaign/exit/boss coverage and representative hardware checks
remain release acceptance work. This iteration does not claim new gameplay fixes.

## Use

With Python 3 and Docker Desktop ready, extract SMWPS2_v017_Build_Kit.zip and
run Build_ISO.cmd. Select a US ROM; when prompted for arcade support, choose
nss-mw-0_prg.ic1. Japanese requires its optional original SNES ROM. The builder
compiles an ELF and ISO locally. No ROM is uploaded or modified.

See docs/V017_VALIDATION.md and release/v017-test-evidence/ inside the kit for
scoped results, fingerprints and limitations. Historical v016 reports remain
labelled with their original version.
