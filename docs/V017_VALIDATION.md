# v017 — arcade input picker and launcher refresh

Date: 2026-11-02. This iteration changes release tooling and launcher artwork.
Native gameplay, widescreen rendering, audio, saves and controller behaviour
remain byte-for-byte unchanged at source level from v016. The only native
source differences are src/menu.c and version labels in src/ps2_main.c.

## Arcade inputs

The supplied `nss-mw-0_prg.ic1` is 524,288 bytes and matches accepted SHA1
`06a6efc246c6fdb83efab1d402d61d2179a84494`. It is the required NSS game program.
The supplied `nss-r__ic3__mw.ic3` is 32,768 bytes and `security.prm` is 16 bytes.
Neither companion is needed for this native free-play port. Cabinet BIOS,
security hardware and coin/timer overlays remain outside the port's scope.

The picker names the required `.ic1`, includes that extension in its primary
filter, pre-fills the program filename and validates each selection immediately.
Choosing either companion explains the mistake and returns to the picker.
Validation uses the actual program hash, so a valid renamed program is accepted.
CLI help and the README name the same file. Fresh staging/public packaging also
exclude raw `.ic1`, `.ic3`, `.prm`, `.rom` and `.bin` files.

The supplied program passes validation and extraction. Its 12,812-byte NSS DAT
is byte-identical to the prior working arcade pack, SHA256
`e054deb88e98590ab705b506850a12ddb385ec7ce58553be062dea0a4aa0262a`.
Both supplied companion files are rejected with the intended explanation.

## Menu

The yellow/blue packaging palette, multicolor title, repeating stars, question
blocks, green arcade header and brick footer are drawn by the production menu's
rectangle callback. No reference-image pixels, scan, external menu texture or
new game asset is embedded. The previous tagline is removed. Edition choice,
resume, language, display and controller actions retain their existing rules.

Production menu previews of both selected cards were compiled and inspected at
640x448. They are host renders of the same drawing code, not PS2 screenshots.

## Checks run for this iteration

- Real supplied NSS program accepted; both real companion inputs explained and
  rejected; extracted pack matched the working baseline byte-for-byte.
- Builder tests passed: invalid ROMs, staging, absent optional symbols, output
  hash verification and Docker command/mount contract. Docker execution is
  mocked here, not an actual container run.
- Actual dialog flow passed against mocked tkinter APIs: `.ic1` filter and
  default filename, named prompt, error explanation, retry and correct choice.
  This does not certify Windows file-dialog behaviour.
- Production frontend launcher test passed its English/arcade/Japanese/arcade/
  English switch sequence and return/resume rules.
- Python compilation checks passed for changed build/packaging tools and tests.
- The PS2DEV/EE GCC 15.2.0 native build passed the R5900 ELF check. The game and
  native sound entry points are present, with no CPU interpreter or test oracle.
- ISO packaging read back BOOT.ELF and SYSTEM.CNF byte-for-byte. The supplied
  test ELF/ISO embeds English, Japanese and NSS packs.
- A final source-package extraction repeats the focused builder/launcher checks.

The US/Japanese asset packs for the test build were recovered unchanged from the
previous working v015 ELF; the NSS pack was verified against the supplied `.ic1`.
The earlier real US-ROM-to-ISO integration tests and 1,410 ASan/UBSan room cases
are v016 results, retained separately in V016_VALIDATION.md and test evidence.
They have not been rerun or relabelled as v017 campaign/hardware results.

Actual Windows/Docker execution and physical PS2/PCSX2 gameplay of v017 remain
untested here. This is still a public beta; the previous campaign/exit and real
hardware acceptance gaps apply. This iteration introduces no gameplay fixes.
