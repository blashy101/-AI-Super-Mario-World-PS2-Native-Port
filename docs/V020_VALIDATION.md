# SMW PS2 v020 MX4SIO test build

The user reported that v019 freezes after about five seconds on the MX4SIO
launcher without a button press. It also failed to enter the game when
selected early. USB worked. This places the first observable failure before
the game's save and controller handoff.

The launcher has no five-second idle limit. The PS2SDK MX4SIO driver has a
background card-detection thread, and OPL's game reset path can reinstall the
driver. That is a possible cause of the observed freeze.
[PS2SDK MX4SIO driver](https://github.com/ps2dev/ps2sdk/blob/f1ca58a468ead937155ab818d9a38e52c8adf861/iop/sio/mx4sio_bd/src/mx4sio.c)
[OPL reset code](https://github.com/ps2homebrew/Open-PS2-Loader/blob/b3a2199795fe89ba7e2dd7b843354472dff2c3eb/ee_core/src/iopmgr.c)

The MX4SIO Apps archive contains the self-contained ELF and OPL title.cfg.
OPL Apps loads the ELF into EE memory through the SDK application loader.
The application loader requests a reset after reading the file. The game's
controller and audio clients bind after its own reset. The Apps argument
keeps saving off during this retest.
[OPL application launch](https://github.com/ps2homebrew/Open-PS2-Loader/blob/b3a2199795fe89ba7e2dd7b843354472dff2c3eb/src/appsupport.c)
[PS2SDK ELF loader](https://github.com/ps2dev/ps2sdk/tree/f1ca58a468ead937155ab818d9a38e52c8adf861/ee/elf-loader)

For the ISO, enable OPL Mode 3 and Mode 6. Mode 3 releases the SIF reset hooks
after the first reset on the OPL path inspected. When the frontend sees
MX4SIO still resident and observes that the SIF DMA handler changed, it
requests one further reset before opening controller and audio services.
If the loader still retains its driver, the launcher displays a hint about
Apps and the OPL settings.
[OPL Mode 3 hook release](https://github.com/ps2homebrew/Open-PS2-Loader/blob/b3a2199795fe89ba7e2dd7b843354472dff2c3eb/ee_core/src/asm.S)

The launcher now submits graphics when it needs a new frame and polls input
between changes. The SDK facade exercised 520 idle fields (8.7 seconds)
with one initial graphics submission in both the Mode 3 and Apps cases.
Original and Super System then reached gameplay input. The Mode 3 model
confirmed that pad initialization followed the second reset. The Apps
model tested the explicit MX4SIO argument with no resident driver.
Existing card, launcher, overlay, and pacing regression tests passed.

The R5900 executable cross-built with EE GCC 15.2.0. Physical PS2
confirmation is pending. Start with the Apps package, wait ten seconds
at the launcher, and test in-game input. The ISO still needs a separate
retest with the appropriate OPL settings.

The extracted kit passed ELF, ISO and Apps packaging checks for all three
known v019 asset packs and for US-only data. It cleared optional slots in
the US-only ELF and verified the ISO boot files and Apps archive contents.
The packaged builder regression tests passed. The asset packs used here
came from the verified v019 ELF.

`SMWPS2_v020.elf` SHA256 `f118c3e0596d858c341616561532e63d7ff2ed71a2dc6da7485fd9587652a343`

`SMWPS2_v020.iso` SHA256 `7d3598754c8d3b50293cec56d9fb2bc36fa31d819fa307dc08576d8532982434`

`SMWPS2_v020_MX4SIO_Apps.zip` SHA256 `541dae5d1c41f64235548b3b2b9c756ad747e4b6f31e05bd8a50029e74e80785`
