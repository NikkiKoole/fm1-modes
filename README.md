# FM-1 modes (on M-VAVE's FM1 Open)

Extra modes for the **M-VAVE FM-1**, built on M-VAVE's official open kit (FM1 Open V14, the rest of
this page). M-VAVE's own FM engine and sequencer stay as they are; these modes sit beside them in
`app/` and take over the panel while they are open.

> **Status:** developed and tested in the browser emulator only. None of it has run on a real FM-1 yet.

## The modes

Open the launcher with **HOME + ARP**, turn **KNOB 1** to a mode, press **OCT+** to open it, **HOME** to go back.

| Mode | What it is |
|---|---|
| **TEKNO** | 90s free-party tekno (French hardtek, Czech tribe), played live: a rumble kick or a rolling kick + bass, tuned toms, hats, clap, an acid 303 with Devil Fish controls (Open303), a GRENADIER triple-filterbank drone, and effects locked to the bars (phaser, flanger, acid sweep, vowel filter). Seven layers on the black keys, patterns on the white keys. Its header comment in `app/tekno.c` has the controls and the to-do list. |
| **CR-78** | a CR-78 drum machine, 16 steps, ratchets and swing |
| **RHYTHM BOX** | 101 factory rhythms on TR-77 and Side Man kits |
| **BASS PLAYER** | follows the chords you play and writes bass lines to them |
| **MY SYNTH** | a six-voice subtractive synth on the keys |
| **FX** | a shared effects rack (drive, crush, EQ, chorus / phaser / flanger, spring reverb, a kick duck) for the modes |
| **LOOP DECK** | banks of loops you toggle live |
| **GROOVEBOX / GROOVE MINI** | an experiment: four tracks with one set of rules after Elektron and Roland's TR boxes (`docs/GROOVEBOX-MANUAL.md`) |
| **STARFALL** | a little game: catch the stars with the keys |

Many sounds are ports from the author's own fantasy console *dreamengine*. Third-party code is listed in
[THIRD_PARTY.md](THIRD_PARTY.md); TEKNO's acid voice adapts Open303 (Robin Schmidt, MIT) as in the kit's own 303.

## Building the browser emulator on a Mac

M-VAVE's build scripts are for Windows (`build.bat`). `tools/build_web_mac.py` builds the browser emulator
on macOS (or Linux):

1. Download [WASI SDK 27](https://github.com/WebAssembly/wasi-sdk/releases) and unpack it to
   `build/tools/wasi-sdk-27.0-arm64-macos` (or point `FM1_WASI_SDK` at it).
2. `python3 tools/build_web_mac.py`
3. Serve it: `python3 -m http.server 8765 -d build/preview` and open http://127.0.0.1:8765.
   Click a piano key once to start the sound.

The firmware for the device itself still needs M-VAVE's JieLi toolchain on Windows (`build.bat`).

## Notes

- `PANEL.md`: every key, button, encoder and LED as the SDK numbers it.
- `LAB-NOTES.md`: notes on the other FM-1 firmwares around (fm1-base, Felucca, Skiff, CHOPPAH, ...).
- `docs/GROOVEBOX-MANUAL.md`: the GROOVEBOX's panel rules, also the grammar the newer modes follow.

---

# FM1 Open V14

English · [简体中文](README.zh-CN.md)

8 tracks, 8 sample slots, 16 FX demos; binary SDK included.

[Workbench and development kit](https://open.m-vave.com/fm1-workbench) · [Official forum](https://forum.m-vave.com)

Use the workbench to update firmware, preview the instrument and manage projects, samples and DX7 voices. Select "Source + SDK" to download the development kit matching the online release. Back up projects, voices and samples before updating, and keep the device powered during writes.

## V14 features

- **DX7 library:** 128 voices in four groups of 32, with the M-VAVE FM1 original V15 factory voices and order. Saved user banks take priority. DX7 single-voice and 32-voice bank SysEx import/export is supported; extension parameters are not handled. Changing the library does not change patches already saved in projects.
- **Track roles:** on SEQ **8/8 TRACK ROLE**, K1 selects NORMAL, PLAY or OFF; K2 selects input MIDI channel 1–16. PLAY retains the track's timbre and held notes across project changes. MUTE/SOLO use the existing controls, with independent NORMAL and PLAY solo groups. PLAY timbre edits are not saved with projects: export a timbre file, or save DX7 voices to the independent library.
- **Step recording:** with the track set to NORMAL, press REC while stopped and play notes. Releasing all played keys advances to the next step, including DRUM 808 and SAMPLER. Overlapping notes share a step; during playback, REC records in time. PLAY tracks are for live performance and do not record notes.
- **Tempo and delay:** internal tempo is 30–300 BPM. Tap Tempo is available on the workbench, with SEQ on the FX DELAY page, or with HOME held while tapping SEQ elsewhere. Delay supports tempo synchronization and fixed times of about 30–2040 ms.

## MIDI

FM1 accepts DIN, USB and BLE MIDI input, with USB/BLE output. Default input channels 1–7 address tracks 1–7; channel 10 addresses track 8. Change each input channel on TRACK ROLE.

- **5/8 SYNC / LOOP:** select internal or external clock, tempo, loop length and clock input.
- **6/8 MIDI TRACK:** select SEQ/ARP LOCAL, BOTH or MIDI output, output channel, output ports and physical KEY OUT.
- **7/8 MIDI ROUTING:** configure LOCAL, CLK OUT and MIDI Thru. For DAW monitoring with Local Off, enable KEY OUT and select the output ports.

MIDI preferences are saved independently of projects. The browser simulator previews these settings without sending performance MIDI to external ports; configure the instrument on its panel. MIDI Clock, Start, Continue, Stop and Song Position Pointer input are supported. Clock loss stops synchronized playback.

## Updates and compatibility

V13 can update to V14. The workbench supports returning to original V15 and migrating from compatible third-party firmware. The standard FM1 update protocol remains available. Compatibility has been checked with selected firmware versions; it is not a guarantee for every third-party release.

Firmware switching does not make project and sample formats interchangeable. Keep exported backups for rollback. Older firmware may not support V14 track preferences, the independent voice library or synchronized/extended delay settings.

To back up the independent voice library, read and export groups A–D separately. Project files do not contain sample audio. Restore custom samples from their sample bank backup; writing the built-in sample bank replaces the current samples.

The eight sample slots share about 19.55 seconds of new sample storage. Existing larger banks remain readable; they may need to be backed up and reduced before writing the independent DX7 library.

## Build

Requires Windows x64, PowerShell, `curl.exe` and `tar.exe`. Firmware builds require **JieLi Windows toolchain 2.5.3**, which includes PI32v2 clang 4.0.1. [Download the 2.5.3 installer from JieLi](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) and follow the [installation guide](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/jl_toolchain.html). Set `FM1_PI32_TOOLCHAIN` to its `pi32` folder containing `bin/clang.exe` (normally `C:\JL\pi32`). FM1 continues to use the `r3` CPU target.

The build checks all 22 toolchain files against `sdk/manifest.json`. Older V14 kits were pinned to 2.5.2 and reject 2.5.3; update the complete source + SDK kit together, keeping the manifest checks enabled. For this revision, the expected SHA-256 of `pi32/bin/clang.exe` is `d4bc97cf421c1ccc373dd8924ce88720ff7457b3f1478d15df4aeda6ae923bce`.

For a local web preview:

~~~powershell
.\tools\Build-Web.ps1 -LocalPreview
~~~

For distribution builds, first set `FM1_SOURCE_URL` to the HTTPS URL of the matching source archive, including your changes, licenses and build/install scripts. Then run:

~~~powershell
.\build.bat --web
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
.\build.bat --package
~~~

For a local firmware build without a distribution source URL, run `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'`. A moving GitHub `tree/main` page is not a matching source archive for distribution.

The web version builds from the included C sources. Its first build downloads WASI SDK 27; set `FM1_WASI_SDK` to reuse an installation. Outputs are `build/preview/index.html`, `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc` and `build/official-web/FM1-Official-Open-Source.html`.

The [per-track tuning API](app/instrument.h) supports custom tuning; a dedicated tuning editor is not included.

## License

APP: [GPLv3 with the FM1 SDK linking exception](LICENSE.md). The device engine is supplied as a binary SDK with separate terms. Retain the applicable [SDK terms, third-party licenses and contributor notices](THIRD_PARTY.md) when distributing builds.
