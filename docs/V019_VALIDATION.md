# SMW PS2 v019 test build

v018 loaded via MX4SIO on the user's PS2. The controller stopped responding
at the gameplay handoff, and two launcher aspect changes caused a crash.

The launcher now draws its artwork into a clipped 640×448 texture and uploads
it when the menu changes. The menu displays the aspect choice immediately and
the game applies it on launch or resume. Both screens share one VRAM region.

Controller polling retains the connected port during a busy or incomplete
report and sends neutral buttons until the next valid sample. A reported
disconnect clears the port. This keeps a transient read failure from assigning
player one to the other controller during load.

A resident MX4SIO driver selects session-only play. This test path skips
memory-card initialization and save-device probing to isolate potential SIO2
contention. The regular path supports formatted PS2 cards and mounted mass:
storage.
[PS2SDK MX4SIO driver](https://github.com/ps2dev/ps2sdk/blob/f1ca58a468ead937155ab818d9a38e52c8adf861/iop/sio/mx4sio_bd/src/sio2man_hook.c)

The R5900 build passed with EE GCC 15.2.0. The ASan/UBSan frontend test used the
real launcher renderer with a simulated SDK. It switched 4:3 → 16:9 → 4:3 before
launching Original, then returned and resumed in 16:9. Both game widths produced
frames. The controller tests observed neutral buttons and stable port
assignment during busy and short reports, then valid movement and jump inputs.
Original and Super System each ran with zero save probes in the MX4SIO cases.
The normal card, timeout, overlay, and loader tests passed.

The native host replay reached file selection, the map, and gameplay and
matched the existing RAM, image, and audio reference hashes. The pacing model
and edition/language switching tests passed.

Physical PS2 testing is pending. On the setup that reproduced the issues,
press Square twice before launch, start each edition, and check D-pad and
action buttons once the game is running. Progress is temporary when the
launcher reports MX4SIO / SESSION ONLY.

The recovered source matches the tested v019 native fingerprint, and the
rebuilt engine matches the earlier v019 ELF template byte for byte. The kit
was extracted into a path with spaces. The portable engine and ISO packager
passed with all three previously validated asset packs and with US-only data.
Optional slots were cleared in the US-only result. Both ISO boot files were
read back. The packaged builder and engine regression tests passed.

`SMWPS2_v019.elf` SHA256 `382f9e8a24457d4a3646821ba46ddfd7f1f14873d0ecafd7ab3263d6b562266f`

`SMWPS2_v019.iso` SHA256 `edf874650b57e56c36ee19dd3443eff04e74afd3823d724d793f79d70454b5e8`
