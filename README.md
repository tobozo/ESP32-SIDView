# ESP32-SIDView

This project is an attempt to provide a visual dimension to chiptune music while browsing the [High Voltage SID Collection](https://hvsc.c64.org) using an C64-themed UI.

The Waveform viewer is heavily inspired by [SIDWizPlus](https://github.com/maxim-zhao/SidWizPlus).


Features:
---------
- [HVSC](https://hvsc.c64.org/) compliant SID music player: can play SID Files / subsongs in SID files / playlists.
- File browser: with dircache and playlist cache generation on first access.
- Waveform viewer: using SID information (this is not analog capture!).
- HVSC Archive downloader: because dowloading and unzipping manually is so 2020.
- SD-Updatable: so the SD Card can host other loadable binaries (e.g. WiFi configurator, MIDI relay, etc).

Demos:
------
- https://www.youtube.com/watch?v=vK3MDsCDaJo
- https://www.youtube.com/watch?v=0w8IsVCzwHA

Bill of materials:
------------------
- ESP32 LoLin D32 Pro (Wroom works too but Wrover is recommended)
- SPI TFT 1.4 128x128 (e.g. Wemos 1.44“ inch TFT LCD shield)
- I2C [XPad Buttons Shield](https://www.tindie.com/products/deshipu/x-pad-buttons-shield-for-d1-mini-version-60/)
- SPI Micro SD Card reader
- SID6581/[FPGASID](http://www.fpgasid.de/)/[TeenSID](https://github.com/kokotisp/6581-SID-teensy)/[SwinSID](https://www.c64-wiki.com/wiki/SwinSID)
- [ESP32 <=> SID shield](https://github.com/hpwit/SID6581/blob/master/extras/Schematic_SIDESP32_Sheet_1_20200319160853.png)

Software Requirements:
----------------------
- https://github.com/hpwit/SID6581
- https://github.com/tobozo/ESP32-Chimera-Core
- https://github.com/Lovyan03/LovyanGFX
- https://github.com/tobozo/ESP32-targz
- https://github.com/tobozo/M5Stack-SD-Updater

Usage:
------
- Build yourself a SID6581 to ESP32 PCB (see the [/pcb](pcb/) folder or [hwpit](https://github.com/hpwit/SID6581/blob/master/extras/Schematic_SIDESP32_Sheet_1_20200319160853.png)'s stereo implementation
- Flash your ESP32 with the sketch
- Deploy the [HVSC archive](https://hvsc.c64.org/) on the SD Card by either:
  - Letting the ESP32-SIDViewer download and unzip it (this can take up to 4 hours and may require to setup the WiFi SSID/PASS in `config.h`)
  - Manually copying the `MUSICIANS`, `GAMES` and `DEMO` folders from the HVSC archive into the `/sid` folder and the `Songlengths.md5` file into the `/md5` folder.
- Let the ESP32 do the indexing of the MD5 file, this is only a one time operation.

Credits:
--------
- [@hpwit](https://github.com/hpwit)
- http://www.earlevel.com/main/2013/06/01/envelope-generators/
- https://github.com/fdeste/ADSR
- https://github.com/JonArintok/ResonantLowPassFilterExample
- http://archive.6502.org/datasheets/mos_6581_sid.pdf
- https://hvsc.c64.org
