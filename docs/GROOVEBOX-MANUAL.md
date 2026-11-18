# GROOVEBOX — the manual (draft 2, 17 Nov 2026)

How the GROOVEBOX's panel works, written as the manual it will ship with. It is also the spec: the code
follows this page, and where the code differs, this page says so. Built so far: ✅. To build: 🔜.
Mark it up freely.

The panel copies what Elektron (Digitakt, Digitone) and Roland's TR boxes do wherever we had a
choice, so it works the way people who know those machines expect.

---

## 1. The idea in one page

Four tracks — **DRUMS · BASS · KEYS · LEAD** — play one pattern of 16 steps together. Sixteen
patterns, chained into longer phrases. Every track follows the same rules:

| | |
|---|---|
| **ALGORITHM** | where you are: OVERVIEW · DRUMS · BASS · KEYS · LEAD |
| **HOME** | one level up (a track → the overview → the launcher) |
| **SEQ** | the keybed's layer: **STEP** (programming) or **PLAY** (an instrument) |
| **Hold something** | the knobs and screen are about *that*: a step, a pattern |
| **Hold A, press B** | put them together: a step + a part, a pattern + patterns (a chain) |
| **FUNC (GLO)** | the verbs: copy, paste, clear; erase while recording |
| **FX held** | the black keys are effect pads |
| **REC** | what you play is written into the pattern |
| **PRESETS** | a ready-made starting point for where you are |
| **SAVE** | the song into Flash |

The keybed has 27 keys, F3 to G5: **16 white keys** (one for each step or pattern) and **11 black keys**.

---

## 2. Getting around ✅

- From the launcher, choose GROOVEBOX. You start in the **OVERVIEW**.
- **ALGORITHM** turns you along OVERVIEW → DRUMS → BASS → KEYS → LEAD. Each track has its colour
  (DRUMS ember, BASS green, KEYS cyan, LEAD pink) on its name, its notes and its screen.
- **HOME** goes one level up, and only that: from a track to the overview, from the overview to the launcher.

---

## 3. The two layers: STEP and PLAY ✅

Inside a track, the keybed is either a step sequencer or an instrument. **SEQ** switches between
them; its light is on in STEP. Every track starts in STEP. The overview has only STEP.

### STEP — programming ✅

| Keys | Do |
|---|---|
| **White keys** | the 16 steps: tap one on or off. A new step gets the part you last chose. |
| **Black keys** | the track's parts. Press one to hear it and choose it for new steps. |
| **Hold a step + black key** | that part on that step |
| **Hold a step + KNOB 1–4** | that step's details (§5) |

The black keys' parts, always in the song's key (§8):

| Track | Black keys | OCT − / + |
|---|---|---|
| DRUMS | the 11 main voices: KICK SNR RIM HHC HHO CYM COW CLV BNGH CONG GUI | — |
| BASS | notes of the scale | octave 0–3 |
| KEYS | a chord on each scale degree (triads, or sevenths: CHORD page) | octave 1–5 |
| LEAD | notes of the scale | octave 2–6 |

The label row above the knobs names what each black key does right now.

### PLAY — an instrument ✅

The **16 white keys are the instrument, low to high, always in key**. The black keys rest (dark).
As on an Elektron in its keyboard mode, only the keys that play are lit.

| Track | White keys, left to right |
|---|---|
| DRUMS | all 16 voices: KICK · RIM · SNARE · CONGA · BONGO L · BONGO H · CLAVES · COWBELL · GUIRO L · GUIRO · MARACAS · TAMB · HAT CL · HAT OP · METAL · CYMBAL |
| BASS, LEAD | the scale, from the track's octave up (OCT − / + move it) |
| KEYS | the chord on each scale degree, from the track's octave up |

The key you played last is the one the knobs edit (DRUMS: that voice's sound).

---

## 4. Recording live ✅

As on the Digitakt and the TR-8S:

- **Hold REC, press PLAY** — start with recording on. REC blinks while it records.
- **REC while playing** — punch in, and REC again to punch out.
- **REC while stopped** — arm. PLAY starts recording.

What you play is written into the pattern as it runs:

- In **STEP**, the black keys (the parts); in **PLAY**, the white keys (the instrument).
- **Drums add hits.** **Melodic tracks replace the note or chord on that step.**
- **Timing**: every hit goes to the nearest step. A hit just **before** a step sounds at once, goes
  onto that step, and is not played a second time when the playhead reaches it. A hit just **after**
  a step sounds at once and goes onto that step (you hear it from the next time round).
- **Erase while recording**: hold **FUNC** and a key: that part's hits disappear as the playhead
  passes them (DRUMS: that voice; a melodic track: any key, all its notes). The MPC's way.

---

## 5. A step's details: hold a step + KNOB 1–4 ✅

The same four knobs on every track, in the same places:

| | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| DRUMS | RATCHET (1–8 hits) | ACCENT on / off | — | — |
| BASS | LENGTH (to 1 step) | VELOCITY, then **ACC** at the top | SLIDE | OCTAVE − / + |
| KEYS | LENGTH (to 4 steps) | VELOCITY, then **ACC** at the top | — | OCTAVE − / + |
| LEAD | LENGTH (to 4 steps) | VELOCITY, then **ACC** at the top | SLIDE | OCTAVE − / + |

Holding several steps edits them all.

- **The drums' accent** is one row for the whole kit, as on the 808, 909 and TR-8S: an accented step
  hits every voice on it harder.
- **SLIDE** holds the step before over, and glides from it into this one with no new attack (the
  303's slide). On the LEAD its speed is GLIDE (OSC page).

ENV does nothing here any more.

---

## 6. The sound: KNOB 1–4, SELECT, PRESETS ✅

Without a step held, the four knobs play the track's sound. **SELECT** turns its pages. Turning a
knob shows its name and value large. While stopped, the sound plays as you turn so you hear it.

| Track | Pages (SELECT) | PRESETS |
|---|---|---|
| DRUMS | the chosen voice: SOUND · PITCH · BODY · MOD · LEVEL | a starter beat (drums), into the pattern playing |
| BASS | SOUND (CUT RES DEC SUB) · SHAPE (WAVE DRV ATK PTCH) | 🔜 bass sounds |
| KEYS | OSC · FILTER · AMP (MY SYNTH) · CHORD (SIZE STRUM) | patches |
| LEAD | OSC (WAVE DTUN GLIDE LVL) · FILTER · AMP | patches (its own; KEYS' don't change) |

KEYS shares its sound with the MY SYNTH mode. LEAD has its own sound.

---

## 7. Effects: FX ✅

- **Hold FX** — the black keys become eleven effect pads, the same everywhere:
  **R16 · R32 · HLF · REV · LPF · HPF · CRS · GAT · DRV · PMP · SWP**
  (rolls, half time, reverse, two filters, crush, gate, drive, pump, sweep).
  A pad works while held.
- **REC + FX + a pad** — latch it on (and off).
- **Where:** in a track, the pads hit that track only; in the overview, the whole mix.
- A tap on FX does nothing: muting lives in the overview (§9).

---

## 8. The song's key ✅

The overview's KEY page sets **ROOT** (C … B) and **SCALE** (MIN · MAJ · DOR · PENT), for every track:
the black keys (STEP) and white keys (PLAY) of BASS, KEYS and LEAD are always in it.
It starts in A minor.

---

## 9. The overview

The four tracks as four lanes, one bar, the playhead; muted lanes go dark.

| Keys / knobs | Do |
|---|---|
| **White keys** | the 16 patterns (§10) |
| **Black keys 1–4** | mute DRUMS · BASS · KEYS · LEAD ✅ |
| **SELECT** | MIX (TEMPO SWING FILTER PUMP) · FX (DRIVE CRUSH GATE SWEEP) · LEVELS (DRM BAS KEY LED) · KEY (ROOT SCALE) ✅ |
| **FX held + black keys** | effect pads on the whole mix ✅ |
| **PRESETS** | a starter beat into the pattern playing ✅ |

---

## 10. Patterns and chains

A pattern holds all four tracks' steps. The sounds, the mix and the key are shared by all sixteen.

| Do | How |
|---|---|
| **Switch** | press a white key (overview). It takes over when the playing pattern ends: it blinks until then. Stopped: at once. ✅ |
| **Chain** | hold a white key and press others, in order: hold 1, press 2 2 3 → plays **1 2 2 3**, a bar each, round and round. The screen says CHAIN 1 2 2 3 and the chain's keys are lit. Press one pattern alone to end it. From the start (and after loading) a chain plays from its first pattern. ✅ |
| **A new pattern** | an empty one starts as a copy of the one playing ✅ |

---

## 11. FUNC: copy, paste, clear ✅

**GLO** is the GROOVEBOX's FUNC button. Its verbs act on what you are looking at, as on an Elektron:

| | Overview | In a track | Holding a step |
|---|---|---|---|
| **COPY** — FUNC + REC | the pattern playing | this track's steps | that step |
| **PASTE** — FUNC + ARP | into the pattern playing | into this track | into that step |
| **CLEAR** — FUNC + PLAY | the whole pattern | this track's steps | that step |

PASTE and CLEAR: the same combination again, straight after, **undoes** it. So copying pattern 1 to 5
goes: on 1, FUNC + REC; switch to 5; FUNC + ARP. A copied step pastes onto any step of the same track.

---

## 12. Saving ✅

**SAVE** writes the song into Flash: the 16 patterns, the chain, the kit's sound, the bass sound, both
MY SYNTH sounds (KEYS, LEAD), the mix, the key, the octaves and the mutes. It comes back the first time
you open GROOVEBOX or GROOVE MINI after switching on.

- Two copies, written in turn: if the power goes during a save, the save before it is still there.
- They live in four sectors (16 KB) just before the DX7 banks, taken from the sample bank's room:
  new sample banks now stop 16 KB sooner. If an older, larger sample bank is in that room, SAVE says
  NO ROOM: SAMPLES and writes nothing.
- A save silences the sound for a moment (as any Flash write does).

## 13. GROOVE MINI ✅

The same instrument and the same song, cut down, to get the rules right before growing:
**DRUMS** (KICK · SNARE · HAT CL) and **BASS**.

- Its own entry in the launcher. KEYS and LEAD rest while it is open (GROOVEBOX wakes them again);
  the other drum voices are silent.
- DRUMS: the three voices on black keys 1–3; in PLAY, the white keys in three zones, low to high
  (kick, snare, hat), so two fingers can roll on one voice. The screen shows the three rows of steps.
- PRESETS (overview, DRUMS): starter beats — BASIC · HOUSE · BOOM BAP · BREAK.
- The first time it opens: the BASIC beat and a bass line, so it plays at once.

## 14. The lights

- **Lit** — on or chosen: a step that plays, the part chosen, the pattern playing, a muted track,
  a latched pad, PLAY's playable keys.
- **Blinking** — moving or waiting: the playhead, a pattern queued, REC recording.
- **SEQ** lit: STEP. **REC** lit: armed; blinking: recording.

---

## Open questions

1. **PASTE on ARP**: Elektron pastes with STOP, which we lack. ARP is a stand-in; another button?
2. **Accent while recording live**: the keys have no velocity (they are on/off). Hold a button while
   hitting (which?), or set accents afterwards (§5)?
3. **PLAY's black keys**: dark for now. Later something (an octave's sharps on BASS/LEAD, accented hits on DRUMS)?
4. **Mute inside a track**: only from the overview (one HOME away), or FUNC + FX as a shortcut?
5. **The other drum voices in GROOVE MINI** are silent but still in the patterns (from the CR-78 presets):
   leave them, or clear them when MINI opens?
