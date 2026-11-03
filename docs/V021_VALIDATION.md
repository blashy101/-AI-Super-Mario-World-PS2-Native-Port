# SMW PS2 v021 MX4SIO ISO test

The user reported that v020 still froze at the launcher when its ISO ran
through OPL over MX4SIO. The v021 ISO isolates the OPL reset handoff.

OPL intercepts the usual single-entry SDK reset DMA and can reinstall the
disc and MX4SIO drivers. After the complete boot ELF and its assets are in
EE RAM, v021 sends the normal ROM reset command as the second entry in a
SIF DMA chain. The first entry repeats the same command header without an
interrupt. The OPL hook inspected for this work checks the first entry.
The reset clears the SDK's cached bindings and the game waits for the new
IOP. It then checks that MX4SIO is absent before opening controller and
audio services.
[OPL hook](https://github.com/ps2homebrew/Open-PS2-Loader/blob/b3a2199795fe89ba7e2dd7b843354472dff2c3eb/ee_core/src/asm.S)
[PS2SDK SIF DMA chains](https://github.com/ps2dev/ps2sdk/blob/d41ea62946e53d9a0c8788bb9eb8c8faf488b559/ee/kernel/src/sifcmd.c)
[PS2SDK reset handshake](https://github.com/ps2dev/ps2sdk/blob/d41ea62946e53d9a0c8788bb9eb8c8faf488b559/ee/kernel/src/iopcontrol.c)

An MX4SIO reset request or reboot timeout produces an on-screen error.
A driver that remains resident also produces an error before the
controller services start. The USB path uses the normal SDK reset.
Progress on the MX4SIO test path is session-only.

The host SDK facade checked the two DMA descriptors, the reset command
header, the register order, and the SDK reboot counter. It exercised
retry, both timeout screens, and the resident-driver error. Two cases
waited 520 idle fields (8.7 seconds) and then reached gameplay input.
Normal card, edition selection, widescreen, and pacing regression tests
passed. These are host results. The next check is on the reported PS2
setup.

Launch the v021 ISO through OPL and leave the launcher open for at least
ten seconds. Look for MX4SIO ISO / CLEAN IOP / SESSION ONLY, then start an
edition and check gameplay input. Record any startup error as displayed.

The R5900 executable cross-built with mips64r5900el-ps2-elf-gcc (GCC) 15.2.0. The PS2DEV archive
matched the official release digest
`9551991091b3fc403b67cda43d8b077177b33251bbfa8fa32789fa2d0ec00ac4`.
The extracted portable kit packaged all editions and US-only from the
verified v020 asset packs. It checked optional slots, read both ISO boot
files back, and passed the packaged builder regression tests.

`SMWPS2_v021.elf` SHA256 `f4c0d200d740cda442d961d5313c1dfbe206225ccad6be26de4873a675a4d927`

`SMWPS2_v021.iso` SHA256 `f38435d8185a98d030c316af5af57a7a0c2c99fe4df146245cc6934075ff097b`
