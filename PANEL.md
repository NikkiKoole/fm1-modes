# The FM-1's panel, as the SDK numbers it

What every control is called in `struct fm1_controls` (`sdk/include/fm1.h`) and `fm1_led_set()`. The numbering
is confirmed from M-VAVE's own browser emulator, `sdk/Simulator.js`, which wires each on-screen control to its
index. App code names them in `app/pixel.h` (`PX_*`).

> **Careful:** the community platform (fm1-base / Felucca, e.g. CHOPPAH's `base/include/plat.h`) numbers the
> controls **differently**: its buttons start at FX, its encoders go SELECT, ALGO, PRESET, K1..K4. Never copy
> indices from there into this kit's code.

## Hardware

240×240 screen · 27 note keys (16 white, 11 black) · 14 buttons, each with an LED · 7 encoders · the MASTER
volume knob · speaker and headphone jack · MIDI in · USB-C · Bluetooth.

## Encoders: `encoders[7]` (signed detents since the last read)

| Index | Printed | `pixel.h` / use in our modes |
|---|---|---|
| 0 | SELECT | pages / instruments |
| 1 | ALGORITHM | free in our modes so far (the stock firmware flips its bottom parameter pages with it) |
| 2–5 | KNOB 1–4 | the four soft knobs (`c->encoders + 2`) |
| 6 | PRESETS | mode presets / slots |
| — | MASTER | not an encoder: the ADC reading `knob`, 0..1023 (volume; readable by the app) |

`battery` is the other ADC reading, 0..1023.

## Buttons: `buttons`, bits 0..13

| Bit | Printed | `pixel.h` |
|---|---|---|
| 0 | OCT − | `PX_OCT_DOWN` |
| 1 | OCT + | `PX_OCT_UP` |
| 2 | FX | `PX_FX` |
| 3 | **SEL** | `PX_SCL` (the SDK says SCL, after Felucca; the panel prints SEL) |
| 4 | ENV | `PX_ENV` |
| 5 | LFO | `PX_LFO` |
| 6 | EDIT | `PX_EDIT` |
| 7 | GLO | `PX_GLO` |
| 8 | HOME | `PX_HOME` |
| 9 | SAVE | `PX_SAVE` |
| 10 | ARP | `PX_ARP` |
| 11 | SEQ | `PX_SEQ` |
| 12 | PLAY | `PX_PLAY` |
| 13 | REC | `PX_REC` |

## Keys: `keys`, bits 0..26

Bit *k* is the note **F3 + k** (MIDI 53 + k, `PX_FIRST_NOTE`), up to G5.

- **White keys** (16), left to right: bits 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26
- **Black keys** (11), left to right: bits 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25. Our modes use them as the
  eleven pads (`PX_PADS`), labelled in the screen's bottom strip.

## LEDs: `fm1_led_set(n, on)`

- **0..13:** the buttons, each LED's number = its button's bit above
- **14..40:** the note keys: LED `14 + k` (`PX_KEY_LED + k`) for key bit *k*

## Combos and conventions

- **HOME + ARP** opens our launcher (KNOB 1 picks a mode, OCT + opens it, HOME goes back).
- The stock firmware: **ALGORITHM** flips the bottom parameter pages (SEQ, ARP); **SELECT** picks the track;
  **PRESETS** switches presets, or picks the step on SEQ (from the help in `sdk/Update.html.in`).
- Our modes follow `docs/GROOVEBOX-MANUAL.md`: hold = scope, GLO = the verbs, hold FX = performance pads.
