# SMW PS2 v022 validation

The user confirmed that v021 plays through OPL over MX4SIO on physical PS2.
The v022 ISO retains that IOP reset sequence. Original starts physical
memory-card services when the player selects the game. It checks for a
formatted PS2 card and opens the existing `SMWPS2` save directory on that
port. Card probing stays outside the gameplay loop. A timed-out probe or
an absent card leaves MX4SIO progress in the current session. Super System
uses its arcade session rules.

The Browser metadata installer writes the egg model before `icon.sys` and
checks the bytes after closing each repaired file. The descriptor uses the
PS2SDK `mcIcon` layout and the Shift-JIS title **Super Mario World**. The egg
uses 168 triangles and the existing icon filename. Repair preserves the
alternating SRAM journal files.

The Yellow Switch Palace approach reproduced the photo in the host engine.
On leaving Yoshi's Island by the ladder, the native HDMA iris opens to
columns 16 through 240 and stays active while Mario crosses to the palace.
The widescreen map places the terrain and marker in a wider frame. v022
projects the active main-map iris into that frame as it opens. Its left and
right edges finish flush with the map frame. The test verifies the actual
marker pixels and both terrain bands, including in the arcade start zone
where the switch has already been cleared.

## Checks

- Real ladder route and widescreen raster in Original English, Original
  Japanese and Super System with full and alternate draws. Also exercises
  4:3 and entry to the uncleared palace in Original. Tested in both host RGB
  and the native GS pixel layout.
- Existing map, water, message, mosaic and pipe regressions, plus launcher
  input, physical-card selection, no-card fallback and delayed RPC completion
  in the host SDK model. Icon repair and journal round-trip tests passed.
- Native R5900 build and verified ISO boot files. The test disc reuses the
  three verified game asset packs from v021. The public build kit ships the
  source and empty engine template for local ROM inputs.

## Physical PS2 acceptance

Boot the ISO from OPL using MX4SIO with a formatted PS2 memory card in the
free slot. Start Original in 16:9 and take the ladder to Yellow Switch Palace.
Check that the map marker is intact. Save through the game's save prompt,
reboot, and reload the slot. Open the PS2 Browser to check the **Super Mario
World** label and Yoshi egg. These hardware checks are still pending for v022.
