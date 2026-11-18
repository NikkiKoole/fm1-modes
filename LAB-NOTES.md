# Lab notes

Findings from looking around the FM-1 firmware scene, next to our own work on this branch (`my-modes`).
(A lab page outside this repository runs the emulators side by side.)

## The firmware family tree (17 Nov 2026)

```
felucca-bsp (Hügelton: the bare hardware layer)
├── Felucca ── Skiff, Melodee, … (the multi-engine synth line)
└── fm1-base (Charles Vestal: platform + host simulator + browser emulator + tools, MPL-2.0)
    ├── CTL-1, Radio (its own proof points)
    └── CHOPPAH (Laurens Profittlich, an Amen chopper) ── LOAF BREAKER (FLEXOR, hardcore / breakcore)

M-VAVE fm1_open SDK (the official kit: the sealed stock engine + room for app code)
└── this repository (GROOVEBOX, GROOVE MINI, LOOP DECK, …)
```

- **Our route** builds on top of M-VAVE's own firmware: the stock FM engine and sequencer stay, our modes sit beside
  them (`app/`). **fm1-base and Felucca firmwares** replace everything and have the whole chip.
- **fm1-base**: https://github.com/charlesvestal/fm1-base: screen, drawing and fonts, keys and LEDs, audio, MIDI over
  USB and TRS, storage, a screen reader; optional Bluetooth MIDI, Bluetooth audio (A2DP), WiFi, USB audio.

## The keys can do velocity and pressure

M-VAVE's SDK gives the keys as on/off bits only (`keys` in `sdk/include/fm1.h`), so we took the FM-1 to have no
velocity. **fm1-base reads velocity and pressure from the keys** (`#define APP_TOUCH` in its app.h: `touch_on`,
`touch_poll`, `touch_keys`, `touch_vel`, `touch_pressure`, lib/touch.h). The hardware can do it; our SDK doesn't
expose it. Worth reading how fm1-base does it (`choppah/base/lib/touch.*`) before deciding how live accents work
(docs/GROOVEBOX-MANUAL.md, open question 2).

## Firmwares worth knowing

- **Skiff 2.7** (illuminus5, on Felucca): a six-track groovebox, 14 engines, 64 steps with parameter locks, patterns,
  scenes, songs, chord follow, a performance layer, a browser editor. Much of what our GROOVEBOX aimed at, grown up.
  https://github.com/illuminus5/skiff · try it: https://illuminus5.github.io/skiff/try/
- **LOAF BREAKER** (FLEXOR, on CHOPPAH): hardcore kick synth + break chopper, pads on the black keys, builds and
  drops, a CDJ-style screen; BEGINNER / EXPERT / TUTORIAL modes (31 guided steps) as an answer to "too many options".
  https://github.com/FLEXOR-R/loaf-breaker · emulator: https://flexor-r.github.io/loaf-breaker/emu/
- **CHOPPAH**: cloned in the lab (`~/Projects/mvave/choppah`); its CLAUDE.md is a good model for a firmware's notes.
- Also in the lab: Felucca, Melodee, FoMni, LOF-1, SLOOP, fm1-emu (a real chip emulator that runs any .fwsc).

## Where our own experiments stand

- GROOVEBOX / GROOVE MINI: the panel's rules in `docs/GROOVEBOX-MANUAL.md`; called a good experiment,
  parked (Skiff does the groovebox well).
- LOOP DECK: banks of loops that always fit (an idea from Reddit: a DJ toy's banks of pads). Possibly still worth it,
  since no firmware does it; check Skiff's scenes / PERFORM layer first.
