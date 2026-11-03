# audsrv PCM input-volume correction

Sources copied from ps2dev/ps2sdk commit
`2a18d15da1332665451a9bcf9a0d4df9029c9490`, directory
`iop/sound/audsrv`. Original copyright/license headers are preserved.
The original GNU Library GPL version 2 text is included at
`../../licenses/LGPL-2.txt`. The new small mapping helper is MIT-licensed.

The only existing runtime source modified is `src/audsrv.c`: include the
helper and map `core1_volume` before writing the two PCM input-volume
registers in `update_volume`. No playback, buffer, resampling, RPC or
threading logic is changed. Export/import tables and public volume API stay
compatible. The Makefile adapts the SDK source build to installed-toolchain
sample rules; the resulting IRX is embedded into the standalone ELF.

The public EE volume table ends at 0x3fff, also used by audsrv for the SPU2
master control. The PCM input-volume register is a direct signed 16-bit
value with positive full scale 0x7fff. Reusing 0x3fff in that register
attenuates the PCM stream to approximately half amplitude. The helper maps
the existing API's range to 0..0x7fff, preserving zero/mute and never
requesting gain above unity. At 100%, the ratio is 32767/16383, about +6.02 dB.

References checked on 2026-10-30:

- https://github.com/ps2dev/ps2sdk/blob/2a18d15da1332665451a9bcf9a0d4df9029c9490/iop/sound/audsrv/src/audsrv.c
- https://github.com/ps2dev/ps2sdk/blob/2a18d15da1332665451a9bcf9a0d4df9029c9490/ee/rpc/audsrv/src/audsrv_rpc.c
- https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/spu2sys.cpp
  (`V_VolumeLR::Max`, `V_VolumeSlideLR::Max`, and direct `InpVol` writes).
- https://github.com/PCSX2/pcsx2/blob/master/pcsx2/SPU2/Mixer.cpp
  (application of input and master volume to the stream).

Use a fresh emulator/console boot when comparing builds, so the new embedded
module is loaded. Auditory and physical-hardware confirmation is still needed.
