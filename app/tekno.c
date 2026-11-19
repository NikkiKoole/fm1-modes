/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO: 90s free-party tekno, played live: two kicks, toms, hats, clap, an acid 303, a filterbank drone.
 *
 *   black key 1       RUMBLE on / off: the rumble kick on every beat, its long tuned tail the bass
 *   black key 2       ROLL on / off: a short kick on the beat, bass notes rolling between (with RUMBLE
 *                     on too: RUMBLE's long kick, ROLL's bass notes over it)
 *   black key 3       TOMS on / off: two tuned toms rolling on the sixteenths
 *   black key 4, 5    HATS, CLAP on / off
 *   black key 6       ACID on / off: a 303 line on the kick's note (an octave up)
 *   black key 7       GRENADIER on / off: a triple-filterbank drone on the same note, chugged by its gate
 *   black key 8       PERC on / off: two synth percussion voices after Korg's ER-1 (bloops, zaps, bubbles,
 *                     cowbells, trills, metal): a sound (SND) and a pattern (PATN) on its page, voice A and
 *                     voice B (PTCH DCAY MOD DPTH) on its others
 *   white keys 1-4    the toms' pattern: RUN, GALLOP, TRIBE (again: 3-3-2), SPARSE
 *   white keys 5-8    the hats' pattern: 16THS, OFFBEAT, GALLOP, BUSY
 *   white keys 9-12   the clap's pattern: BACKBEAT, OFFBEAT, BREAKER, FILL
 *   white keys 13-16  the acid's line: PULSE (again: DOTTED, again: RUN), GALLOP (BOUNCE, SPIRAL),
 *                     SQUELCH (SPARSE, WORM), DARK (GRIND, ARP)
 *                     (a lit pattern key pressed again swaps to its second pattern, where it has one)
 *   (the pages)       KICK (TUNE PUNCH DECAY DRIVE) · ROLL (FEEL: GALLOP ROLL OFFBEAT PUSH
 *                     REVERSE HICCUP 3-3-2 DOTTED; DECAY CUTOFF DRIVE)
 *                     · TOMS (TUNE GAP DECAY LEVEL) · HATS (TONE CLOSED OPEN LEVEL)
 *                     · CLAP (TUNE SPREAD DECAY LEVEL; BODY ROOM DRIVE VARY) · ACID (CUTOFF RESO ENV DECAY)
 *                     · DEVIL (ACCENT ACCENT-DECAY SLIDE DRIVE) · GNARL (FILTER-FM SUB TRACK LEVEL)
 *                     · SWEEP (the acid's cutoff ridden on the bars: DEPTH BARS SHAPE RESO)
 *                     · NOTES (the acid's SCALE: LINE MINOR PHRYG DORIAN HARM, its notes read as degrees;
 *                     OCT; TRNS against the kick's key)
 *                     · GREN (BASE SPACE Q MORPH) · MOVE (its sweep DEPTH and RATE, GATE, LEVEL) · SOURCE
 *                     (SRC: TRAP SAWS RING NOISE DIDGE, what goes into its filterbank; DTUN: SAWS' spread,
 *                     RING's ratio; DIDGE also sets the mouth: low formants sweeping over 2 bars, droning)
 *                     · PHASER (on the percussion: DEPTH BARS FDBK MIX) · FLANGE (on the hats: DEPTH BARS
 *                     FDBK MIX) · ECHO (a dub delay: TIME FDBK TONE MIX; its SENDS from CLAP TOMS ACID PERC) · VOWEL (VOWEL MOVE RATE MIX) · MIX (the levels: KICK ROLL TOMS HATS;
 *                     then CLAP ACID GREN, and TEMPO; then the DJ FILTER: FLTR (left low pass, right
 *                     high pass) and RES, on the whole output; PERC's level; PUMP: all but the kick ducks on it). KICK's second page: TONE CLICK LEVEL FINE (cents).
 *   SELECT            the instruments, the effects and the mix (each on the page it was left on);
 *   ALGORITHM         through the instrument's pages (KICK's two, ACID's four, GREN's two, MIX's two:
 *                     the header says "ACID 1/4"), as the stock firmware flips its parameter pages
 *                     Each level is also on its layer's own page: both move together. The pads a page
 *                     is about are marked at the bottom (a white bar under their boxes).
 *   PRESETS           the scenes (the whole setup: layers, patterns, every knob, the tempo), in on the
 *                     next bar: INIT DESERT GRINDER BREAKER MENTAL DRONE, then the ones saved
 *   SAVE + white key  save the scene in that slot (1-16; while SAVE is held the lit keys are the ones used)
 *   PLAY              play / stop (the first layer switched on starts it)
 *   HOME              back (it keeps playing)
 * Layers and patterns change on the next bar (their keys blink until then). A layer's black key also takes
 * SELECT to that instrument's pages (RUMBLE: KICK's).
 *
 * The sound is set from three references (French hardtek / Czech tribe, 155-174 BPM): the kick's
 * low band holds almost flat for the whole beat, the tail tuned (A1), its 2nd and 3rd harmonics
 * ~22 dB down; the toms a whole tone apart (G#3 / F#3) on every sixteenth; a phaser on the
 * percussion sweeping once every four bars; hats short on every sixteenth (Desert Planet) or short
 * on the beat and long on the & and a (Axe To Grind); claps broken across the bar (Axe Breaker).
 * No swing: the references measured dead straight (the bounce is in which sixteenths are hit).
 *
 * TODO (talked about, not built yet):
 *   - FROM TWO MORE REFERENCES (Sensory Overload "Instant Coffee", Esoteric / Curley "Floatdemonium", both
 *     185 BPM; measured 18 Nov 2026): their kick is our RUMBLE (flat on A1) but with strong EVEN harmonics:
 *     DONE, KICK's DRV now keeps the clipper lopsided above 26 (2nd, 3rd ~-15 dB at DRV 70-90, as theirs).
 *     Still open from them:
 *       - a METALLIC LEAD: Floatdemonium's mid lead is bright (1-8 kHz), tonal but INHARMONIC (peaks at 102,
 *         215, 447, 694, 867 Hz ...), smeared over G..A#, swept once a bar. We have nothing like it: give
 *         GRENADIER a ring / FM mode, or build the SCREECH layer
 *       - 185 BPM: a "mental" KIT (180-190) beside tribe's 160-175
 *       - their notes: A with A# and G# grinding (phrygian / chromatic): the acid's SCALE knob
 *       - arrangement: long kickless intros, a kickless breakdown, highs filtered out and back (DJ filter
 *         done; DROP / BUILD and scenes)
 *       - vocal snippets in both: samples (the chant, still undecided: flash space)
 *       - hats on every 16th, short (20-40 ms), accents moving; no prominent claps: as we have
 *   - PERC (DONE, a first version: after Korg's Electribe ER-1, black key 8): two synth percussion
 *     voices, their pitch MOD the sound (DROP RISE TRILL RANDOM NOISE), A ring-modulated by B; six
 *     sounds (SND), six patterns (PATN). Still to do: patterns on the white keys (with hold-to-pick),
 *     the wave as a knob, maybe the full four voices (two pairs), and cross-mod (FM) beside RING
 *   - THE BUTTON PLAN (TEKNO uses HOME, PLAY, SELECT, ALGO, KNOB 1-4 and the keys; the rest is free:
 *     the other buttons and the PRESETS encoder).
 *     After docs/GROOVEBOX-MANUAL.md's grammar (hold = scope, GLO = the verbs, hold FX = pads):
 *       PRESETS encoder   scenes: turn to pick, in on the next bar
 *       ALGO encoder      DONE: through the instrument's pages (as the stock firmware does)
 *       KIT               a knob in the MIX group: the sub-genre kit (tribe, mental / acidcore, hard
 *                         groove: the kick's model and the tempo range), set and forget
 *       SAVE (hold)       + a white key: store the scene in that slot
 *       FX (hold)         the black keys become performance pads: DROP, BUILD, DJ filter, echo
 *                         throw, stutter, ... (let go to release)
 *       ARP (hold)        stutter / beat repeat of the last 1/4, 1/8, 1/16 (KNOB 1 picks)
 *       GLO + layer key   the verbs on an instrument: GEN (a new pattern), its sound reset, ...
 *       OCT- / OCT+       transpose the key live (kick, ROLL, acid, grenadier together)
 *       SEL (SCL in the SDK)  free (tried for paging, ALGO does that now). The acid's scale (minor,
 *                         phrygian, chromatic) becomes a knob in the ACID group; the generator's style
 *                         for GEN (gallop-heavy, sparse, rolling, mental) still needs a home
 *       LFO               jump to the moving effects (SWEEP MOVE PHASER FLANGE VOWEL), each page
 *                         saying what it is on (answers "show what each effect is on")
 *       EDIT              jump to the last-touched layer's sound pages
 *       ENV               jump to levels and decays
 *       SEQ               a big view of the playing patterns; later step editing of a lane
 *       REC               record knob moves (a sweep, a filter ride), looped with the bars
 *     With hold-a-black-key = its 16 patterns on the white keys (below), everything has a home.
 *     First: FX-hold pads and PRESETS + SAVE scenes (together: a whole set playable)
 *   - SCENES on PRESETS: DONE (six factory scenes, sixteen slots, SAVE + white key, in on the bar;
 *     a short audio blip while Flash is written). Later maybe: a keep-tempo switch. What it was: a scene is the whole
 *     state (layers, patterns, every knob, tempo), recalled on the next bar. Factory scenes that are
 *     really different tracks (rolling-tom Desert Planet, rumble Axe Grinder, mental acid at 180,
 *     a slow Grenadier drone-tribe, ...) and the user's own saved to Flash (as GROOVEBOX's SAVE).
 *     Today nothing is kept: leaving the mode or powering off starts from the default again
 *   - GENERATORS: "a new one" for the acid line (as X0X's TB-3PO), the toms' pattern, the ROLL
 *     feel; always in key, by the genre's rules (gallop, offbeats, 3-3-2). With hold-to-pick: hold
 *     a layer key, press a GEN key
 *   - KEY AND SCALE: DONE (ACID's NOTES page: SCALE, OCT, TRNS). What it was: add a scale for the acid (minor,
 *     phrygian with its flat second, chromatic for mental)
 *   - ARRANGEMENT: live, DROP/BUILD + scenes is a set's shape; later maybe chained scenes over N
 *     bars (intro 16, groove 32, break 8, drop 32) for hands-off songs
 *   - KITS for the sub-genres: other kick models and tempo ranges (tribe 160-175, mental / acidcore
 *     180-200, a slower hard groove 140)
 *   - DROP / BUILD: in the references the builds TAKE AWAY (Floatdemonium's drums fade to near silence
 *     before its drop; Instant Coffee runs its steady 8ths straight through): no clap or snare rolls. So
 *     a roll (clap / snare, ratcheting 8 > 16 > 32, maybe rising in pitch) is an option inside BUILD,
 *     not the core; clap PITCH / RATCHET as a groove feature: considered, not for now (19 Nov 2026).
 *     The idea: hold a key to cut the kick and let the rest ride; a roll doubling up
 *     (8 > 16 > 32 a bar) with the filter rising; let go to slam back in on the bar
 *   - DJ FILTER over the whole mix: one knob, low pass to the left, high pass to the right
 *     (drain the kick out over 8 or 16 bars for a build)
 *   - DUB ECHO: DONE (ECHO 1/2 TIME FDBK TONE MIX, 2/2 the sends; half-rate, ~0.37 s). And PUMP on MIX 3/3.
 *     Left: throws on the FX-hold pads. What it was: a tempo-synced delay (dotted 8th, quarter) to throw onto a clap, tom or acid note,
 *     its repeats filtered darker (the FX rack has no delay, only the spring)
 *   - GRENADIER's CMOS drift: each pulse nudges the filters a little at random (the cart's RND)
 *   - fuller CLAPS: they sound thin next to the references; more noise body, a bit of room
 *   - MORE THAN 16 PATTERNS: the white keys are full (4 lanes x 4) and "press again" only gives
 *     each key a second. Leaning to: HOLD a layer's black key and the 16 white keys pick ITS
 *     pattern (16 per instrument, all seven layers; "hold = scope" as in docs/GROOVEBOX-MANUAL.md).
 *     Unheld, the white keys come free (for DROP/BUILD, mutes, fills). Keep "press again" too.
 *     (Also weighed: a PATTERNS page of knobs, one per lane; banks flipped by a button.)
 *   - SHOW WHAT EACH EFFECT IS ON: mostly DONE (the effects row says which are on, a white bar under
 *     the pads says what a page is on). Left: the FX mode's rack. What it was: SWEEP is the acid's, MOVE
 *     grenadier's, FLANGE the hats', PHASER the percussion's (toms, hats, clap), VOWEL the whole
 *     mix (the kick's sub kept out), and the FX mode's rack only what its CR78 SEND gives it.
 *     E.g. light the layer boxes an effect page reaches, or group the pages by what they are on
 *   - later maybe: a SCREECH (hoover / FM stab on the offbeats), STUTTER (Electribe-style
 *     beat repeat into a drop), an optional bar-locked CHOP on the ROLL bass (Mandidextrous) */
#include "tekno.h"
#include "pixel.h"
#include "drumvoice.h"
#include "instrument.h"
#include "cr78.h"
#include "fx.h"
#include "gbx.h"   /* gbx_svf, gbx_exp2: the PERFORM filter's coefficients (as LOOP DECK) */
#include <string.h>

/* PERC (ER-1 style, see tekno.h): its sounds (SND loads them into the knobs) and patterns (PATN):
 * a step is 'a' voice A, 'b' voice B, 'x' both, upper case accented, '.' a rest */
enum { PERC_SOUNDS = 6, PERC_PATTERNS = 6 };
static const struct { const char *name; uint8_t wave[2]; int16_t v[2][4]; int16_t ring; } perc_sounds[PERC_SOUNDS] = {
    /*           waves A, B                          A: PTCH DCAY MOD DPTH     B: PTCH DCAY MOD DPTH    RING */
    {"BLOOP",   {TEKNO_PERC_SINE, TEKNO_PERC_SINE},  {{60, 25, TEKNO_MOD_DROP, 12}, {55, 25, TEKNO_MOD_DROP, 12}}, 0},
    {"ZAP",     {TEKNO_PERC_SQUARE, TEKNO_PERC_SAW}, {{72, 12, TEKNO_MOD_DROP, 36}, {67, 10, TEKNO_MOD_DROP, 30}}, 0},
    {"BUBBLE",  {TEKNO_PERC_SQUARE, TEKNO_PERC_TRI}, {{70, 20, TEKNO_MOD_RANDOM_FAST, 24}, {63, 20, TEKNO_MOD_RANDOM, 12}}, 0},
    {"COWBELL", {TEKNO_PERC_SQUARE, TEKNO_PERC_SQUARE}, {{79, 20, TEKNO_MOD_OFF, 0}, {72, 20, TEKNO_MOD_OFF, 0}}, 70},
    {"TRILL",   {TEKNO_PERC_TRI, TEKNO_PERC_SINE},   {{65, 35, TEKNO_MOD_TRILL_FAST, 7}, {58, 35, TEKNO_MOD_TRILL, 5}}, 0},
    {"METAL",   {TEKNO_PERC_SAW, TEKNO_PERC_SQUARE}, {{66, 18, TEKNO_MOD_NOISE, 5}, {78, 18, TEKNO_MOD_OFF, 0}}, 100},
};
static const struct { const char *name, *steps; } perc_patterns[PERC_PATTERNS] = {
    {"OFFBEAT", "..a...b...a...b."}, {"GALLOP", "..ab..ab..ab..aB"}, {"TRIBE", ".a.b.ab..a.b.aBa"},
    {"BUBBLE", "abababababababab"}, {"SPARSE", "...a.......b...."}, {"3-3-2", "A..b..a.A..b..a."},
};
static const char *const mod_name[TEKNO_MODS] = {"OFF", "DROP", "RISE", "TRIL", "TRL+", "RAND", "RND+", "NOIS"};
static uint8_t perc_wave[2] = {TEKNO_PERC_SINE, TEKNO_PERC_SINE};
enum { PAGES = 26, PATTERNS = 4, FEELS = 8, LAYERS = 8, LANES = TEKNO_LANES };
static const struct px_page pages[PAGES] = {
    {"KICK", {"TUNE", "PNCH", "DCAY", "DRV"}},
    {"KICK", {"TONE", "CLK", "LVL", "FINE"}},
    {"ROLL", {"FEEL", "DCAY", "CUT", "DRV"}},
    {"TOMS", {"TUNE", "GAP", "DCAY", "LVL"}},
    {"HATS", {"TONE", "CLSD", "OPEN", "LVL"}},
    {"CLAP", {"TUNE", "SPRD", "DCAY", "LVL"}},
    {"CLAP", {"BODY", "ROOM", "DRV", "VARY"}},
    {"ACID", {"CUT", "RES", "ENV", "DCAY"}},
    {"DEVIL", {"ACC", "ADEC", "SLID", "DRV"}},
    {"GNARL", {"FFM", "SUB", "TRK", "LVL"}},
    {"SWEEP", {"DPTH", "BARS", "SHAP", "RES"}},
    {"NOTES", {"SCAL", "OCT", "TRNS", ""}},
    {"GREN", {"BASE", "SPCE", "Q", "MRPH"}},
    {"MOVE", {"DPTH", "RATE", "GATE", "LVL"}},
    {"SOURCE", {"SRC", "DTUN", "", ""}},
    {"PERC", {"SND", "PATN", "RING", "LVL"}},
    {"VOICE A", {"PTCH", "DCAY", "MOD", "DPTH"}},
    {"VOICE B", {"PTCH", "DCAY", "MOD", "DPTH"}},
    {"PHASER", {"DPTH", "BARS", "FDBK", "MIX"}},
    {"FLANGE", {"DPTH", "BARS", "FDBK", "MIX"}},
    {"ECHO", {"TIME", "FDBK", "TONE", "MIX"}},
    {"SENDS", {"CLAP", "TOMS", "ACID", "PERC"}},
    {"VOWEL", {"VOWL", "MOVE", "RATE", "MIX"}},
    {"MIX", {"KICK", "ROLL", "TOMS", "HATS"}},
    {"MIX", {"CLAP", "ACID", "GREN", "TMPO"}},
    {"MIX", {"FLTR", "RES", "PERC", "PUMP"}},
};
/* knob values, a page a row (see apply()); MIX 1 / MIX 2 are the levels, each also on its layer's own page */
enum { P_KICK, P_KICK2, P_ROLL, P_TOMS, P_HATS, P_CLAP, P_CLAP2, P_ACID, P_DEVIL, P_GNARL, P_SWEEP, P_ANOTES, P_GREN, P_MOVE, P_GSRC,
       P_PERC, P_PERCA, P_PERCB, P_PHASER, P_FLANGE, P_ECHO, P_ECHO2, P_VOWEL, P_MIX1, P_MIX2, P_MIX3 };
static int16_t knob[PAGES][4] = {{33, 60, 15, 26}, {62, 30, 62, 0}, {0, 40, 45, 40}, {56, 2, 45, 45}, {50, 30, 50, 50}, {40, 40, 40, 50}, {60, 50, 0, 30},
                                 {40, 75, 70, 30}, {80, 20, 30, 55}, {25, 0, 0, 60}, {0, 2, 0, 30}, {0, 0, 0, 0},
                                 {45, 45, 55, 55}, {50, 5, 3, 60}, {0, 40, 0, 0},
                                 {0, 0, 0, 50}, {60, 25, 1, 12}, {55, 25, 1, 12},   /* (PERC: its sound's values, loaded on open) */
                                 {80, 2, 50, 50}, {70, 3, 60, 0}, {2, 45, 40, 60}, {0, 0, 0, 0}, {50, 60, 3, 0},
                                 {62, 62, 45, 50}, {50, 60, 60, 0}, {0, 0, 0, 0}};
static const int16_t lo[PAGES][4] = {{28, 0, 0, 0}, {0, 0, 0, -50}, {0, 0, 0, 0}, {0, 0, 0, 0}, {48, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                     {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, -12, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                     {0, 0, 0, 0}, {36, 0, 0, 0}, {36, 0, 0, 0},
                                     {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                     {-100, 0, 0, 0}};
static const int16_t hi[PAGES][4] = {{39, 100, 100, 100}, {100, 100, 100, 50}, {FEELS - 1, 100, 100, 100}, {72, 12, 100, 100},
                                     {100, 100, 100, 100}, {100, 100, 100, 100}, {100, 100, 100, 100}, {100, 100, 100, 100},
                                     {100, 100, 100, 100}, {100, 100, 100, 100}, {100, 3, 2, 100}, {4, 1, 12, 0}, {100, 100, 100, 100}, {100, 6, 4, 100}, {TEKNO_GSRCS - 1, 100, 0, 0},
                                     {PERC_SOUNDS - 1, PERC_PATTERNS - 1, 100, 100}, {96, 100, TEKNO_MODS - 1, 48}, {96, 100, TEKNO_MODS - 1, 48},
                                     {100, 3, 95, 100}, {100, 3, 95, 100}, {3, 95, 100, 100}, {100, 100, 100, 100},
                                     {100, 100, 5, 100}, {100, 100, 100, 100}, {100, 100, 100, 0}, {100, 100, 100, 100}};
/* the levels live in two places (their layer's page and MIX 1 / 2): turning one moves the other */
static const uint8_t level_alias[][4] = {   /* page, knob  <->  page, knob */
    {P_KICK2, 2, P_MIX1, 0}, {P_TOMS, 3, P_MIX1, 2}, {P_HATS, 3, P_MIX1, 3},
    {P_CLAP, 3, P_MIX2, 0}, {P_GNARL, 3, P_MIX2, 1}, {P_MOVE, 3, P_MIX2, 2}, {P_PERC, 3, P_MIX3, 2}};
/* SELECT steps through the instruments (and the effects, the mix), ALGORITHM through an instrument's pages:
 * each group's first page, and which of its pages it was left on */
enum { GROUPS = 13 };
static const uint8_t group_first[GROUPS + 1] = {P_KICK, P_ROLL, P_TOMS, P_HATS, P_CLAP, P_ACID, P_GREN, P_PERC, P_PHASER, P_FLANGE, P_ECHO,
                                                P_VOWEL, P_MIX1, PAGES};
static uint8_t group_at[GROUPS];
/* which layers (pad bit l = black key l + 1) each page is about: their boxes are marked at the bottom */
static const uint8_t page_layers[PAGES] = {
    0x03, 0x03, 0x02, 0x04, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x40, 0x40, 0x40,   /* KICK (RUMBLE and ROLL's) .. SOURCE */
    0x80, 0x80, 0x80,                                                               /* PERC */
    0x9C, 0x08, 0xB4, 0xB4, 0xFF, 0xFF, 0xFF, 0xFF};   /* (ECHO: its sends, CLAP TOMS ACID PERC) */                                            /* PHASER (toms hats clap perc), FLANGE (hats), VOWEL, MIX */
/* ROLL's feels: which sixteenths of the bar get a bass note (bit s: step s; the kick has the beats).
 * The first ones repeat every beat; 3-3-2 and DOTTED cross it (groups of three against the kick) */
static const struct { const char *name; uint16_t mask; } feels[FEELS] = {
    {"GALLOP", 0xCCCC},    /* X . x x   dum . da-da */
    {"ROLL", 0xEEEE},      /* X x x x   a rumble between the kicks */
    {"OFFBEAT", 0x4444},   /* X . x .   the & pumping against the kick */
    {"PUSH", 0x8888},      /* X . . x   just before the beat */
    {"REVERSE", 0x6666},   /* X x x .   the gallop backwards: da-da DUM */
    {"HICCUP", 0x2222},    /* X x . .   right after the kick */
    {"3-3-2", 0x4848},     /* x..x..x.x..x..x.  (the tresillo, twice a bar) */
    {"DOTTED", 0x8248},    /* x..x..x..x..x..x  every third sixteenth, 3 against 4 */
};
/* the layers on the black keys, in order */
static const uint8_t layer_bit[LAYERS] = {TEKNO_KICK, TEKNO_ROLL, TEKNO_TOMS, TEKNO_HATS, TEKNO_CLAP, TEKNO_ACID, TEKNO_GREN, TEKNO_PERC};
static const char *const layer_name[LAYERS] = {"RUMBLE", "ROLL", "TOMS", "HATS", "CLAP", "ACID", "GRENADIER", "PERC"};
static const uint8_t layer_color[PX_PADS] = {PX_EMBER, PX_GOLD, PX_CYAN, PX_WHITE, PX_PINK, PX_GREEN, PX_RED, PX_VIOLET};   /* (their pads' colour) */
/* the patterns, a lane each: '.' rest, upper case accented.
 * TOMS h / l high / low; HATS x / o closed / open; CLAP x a clap.
 * Each key has a pattern, and some a second (VARIANTS later in the row): pressing a lit key again swaps */
static const char *const lane_name[LANES] = {"TOMS", "HATS", "CLAP"};
static const uint8_t lane_layer[LANES] = {2, 3, 4};   /* (index in the layers) */
enum { VARIANTS = 2 * PATTERNS };
static const struct { const char *name, *steps; } patterns[LANES][VARIANTS] = {
    {{"RUN", "HlhlHlhlHlhlHlhl"}, {"GALLOP", "H.hlH.hlH.hlHlhl"}, {"TRIBE", "L.hlh.HlL.hlhlHh"}, {"SPARSE", "..h...l...h..hl."},
     {0, 0}, {0, 0}, {"3-3-2", "H..l..h.H..l..h."}, {0, 0}},
    {{"16THS", "xxXxxxXxxxXxxxXx"}, {"OFFBEAT", "..O...O...O...O."}, {"GALLOP", "x.Oox.Oox.Oox.Oo"}, {"BUSY", "xxOxxxOxxxOxxxOx"}},
    {{"BACKBEAT", "....X.......X..."}, {"OFFBEAT", "..x...x...x...x."}, {"BREAKER", ".x.x..X..x.x..X."}, {"FILL", "....X.......XxXx"}},
};
/* the acid's lines, on the kick's note an octave up: a step is a note ('a' the root .. 'x' 23 semitones
 * up; upper case accented), '-' the last note held on, '.' a rest; '~' under a step slides it into the next */
enum { ACID_LINES = 3 * PATTERNS };   /* each key: its line, a second on a second press, a third on a third */
static const struct { const char *name, *notes, *slides; } acid_lines[ACID_LINES] = {
    /* sparse on purpose (3-7 notes a bar): the root, its octave, one colour note; the rhythm and the
     * accents and slides do the work. Each key's second line on a second press. */
    {"PULSE",   "..a...a...a...A.", "................"},   /* four offbeat roots, the last barking */
    {"GALLOP",  "..aM......aM....", "..~.......~....."},   /* root sliding to the octave on the gallop, twice */
    {"SQUELCH", "A-----m-a-------", ".....~.~........"},   /* three held notes: root, slid to the octave, slid back */
    {"DARK",    "ab.A....ab.A..m.", "~.......~......."},   /* the root grinding into the flat 2nd (phrygian) */
    {"DOTTED",  "a-.M-.a-.k-.a-.m", "................"},   /* a note every dotted 8th: 3 against 4 */
    {"BOUNCE",  "a..m..a..m..a.M.", "................"},   /* root and octave every third sixteenth */
    {"SPARSE",  "a.......m-a.....", ".........~......"},   /* one root, an octave pickup sliding down into the next */
    {"GRIND",   "A-b-a...A-b-m...", ".~.~.....~.~...."},   /* held notes sliding root, flat 2nd, root / octave */
    /* the third: sixteenth-note movement through the scale (written in minor; SCALE re-colours them) */
    {"RUN",     "acdfhfdcAcdfhkmK", "...~.......~...."},   /* runs up and down the scale */
    {"SPIRAL",  "aMdmfMhmfMdmcMam", "................"},   /* climbing against an octave pedal, the see-saw */
    {"WORM",    "acdcAcdfdcdfHfdc", "~.~.~.~.~.~.~.~."},   /* stepwise, slithering, half of it slid */
    {"ARP",     "adhmhdaDfimifdcA", "...~.......~...."},   /* root 3rd 5th octave, then the 4th and 6th */
};
static const char *const pad_name[PX_PADS] = {"RM", "RL", "TM", "HT", "CP", "AC", "GR", "PC", "", "", ""};   /* (the strip: two letters) */
static const char *const note_name[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[PX_PADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

static unsigned active, opened_once, page, full_redraw, shown_lane;
static uint8_t pattern[LANES], pattern_playing[LANES], acid_line, acid_line_playing;
static uint32_t prev_buttons, prev_keys, last_ms, led_state;
static int clamp(int v, int lo_, int hi_) { return v < lo_ ? lo_ : v > hi_ ? hi_ : v; }

/* ---- control-side maths (floats are fine here) ---- */
static float exp2f_(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float expf_(float x) { return exp2f_(x * 1.4426950f); }
static float tanhf_(float x) { float e = expf_(2.0f * x); return (e - 1.0f) / (e + 1.0f); }
#define KICK_BIAS 1.1f   /* (tuned on Instant Coffee's and Floatdemonium's kicks: 2nd, 3rd ~-15 dB at DRV 70-90) */
static int32_t q30(float x) { return (int32_t)(x * 1073741824.0f); }
static int32_t per_sample(float ms) { return q30(expf_(-1.0f / (ms * 44.1f))); }          /* a decay of time constant ms */
static int32_t per_16(float ms) { return q30(expf_(-16.0f / (ms * 44.1f))); }             /* ... applied every 16 samples */
static int32_t lowpass(float hz) { return (int32_t)((1.0f - expf_(-6.2831853f * hz / 44100.0f)) * 32768.0f); }
static float tanf_(float x)   /* (small angles: up to ~0.7) */
{
    float q = x * x;
    float sn = x * (1.0f - q / 6.0f * (1.0f - q / 20.0f * (1.0f - q / 42.0f * (1.0f - q / 72.0f))));
    float cs = 1.0f - q / 2.0f * (1.0f - q / 12.0f * (1.0f - q / 30.0f * (1.0f - q / 56.0f)));
    return sn / cs;
}
static void svf_set(struct tekno_svf *f, float hz, float q)
{
    f->g = tanf_(3.1415927f * hz / 44100.0f);
    f->r = 1.0f / q;
    f->h = 1.0f / (1.0f + f->r * f->g + f->g * f->g);
}
static float hp_a(float hz, float rate) { return expf_(-6.2831853f * hz / rate); }
static float decay_of(float seconds) { return expf_(-1.0f / (44100.0f * seconds)); }
static float log2f_(float x) { float e = 0.0f; while (x >= 2.0f) { x *= 0.5f; e += 1.0f; } while (x < 1.0f) { x *= 2.0f; e -= 1.0f; }
    float z = (x - 1.0f) / (x + 1.0f), q = z * z; return e + 2.8853901f * z * (1.0f + q * (1.0f / 3.0f + q * (1.0f / 5.0f + q / 7.0f))); }
static float ladder_b0(float hz) { float f = hz * (0.7071067811865f / 88200.0f); return (0.00045522346f + 6.1922189f * f) / (1.0f + 12.358354f * f + 4.4156345f * f * f); }

/* ACID: the knobs into Open303's terms (the FM-1's bass303.c), the Devil Fish's ranges past them */
static void apply_acid(void)
{
    struct tekno_acid *a = &tekno.acid;
    const int16_t *k = knob[P_ACID];
    float u;
    a->cutoff = 100.0f * exp2f_((float)k[0] * 0.056f);                                   /* 100 Hz .. 4.9 kHz (stock 314 .. 2394) */
    u = (float)k[1] / 100.0f; a->reso = (1.0f - expf_(-3.0f * u)) / (1.0f - expf_(-3.0f));
    {
        float c = log2f_(a->cutoff / 313.815278606f) / 2.93169f, e = 1.3f * (float)k[2] / 100.0f;   /* ENV up to 1.3x the stock top */
        c = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
        a->env_scale = (1.0f - c) * (3.773996325f * e + 0.736965594f) + c * (4.194548788f * e + 0.864344901f);
        a->env_offset = 0.048292931f * c + 0.294391201f;
    }
    a->decay = decay_of(0.03f * exp2f_((float)k[3] * 0.0664386f));                       /* 30 ms .. 3 s (stock 200 .. 2000) */
    k = knob[P_DEVIL];
    a->accent = (float)k[0] / 100.0f;
    a->accent_decay = decay_of(0.03f * exp2f_((float)k[1] * 0.0664386f));                /* its own decay: 30 ms .. 3 s */
    a->slide = decay_of(0.004f + 0.0008f * (float)k[2]);                                  /* 4 .. 84 ms to get there */
    if (k[3]) {   /* RAT, as bass303.c */
        float d = (float)k[3] / 100.0f;
        a->drive_high = 1.6f - 0.8f * d;
        svf_set(&a->drive_post, 3300.0f - 1400.0f * d, 0.7071f);
        a->drive_dc_a = hp_a(3.0f, 44100.0f);
        a->release = decay_of(0.006f); a->release_acc = decay_of(0.060f);
        a->drive_gain = 6.0f + d * d * (1500.0f - 800.0f * d);
    } else { a->drive_gain = 0.0f; a->release = decay_of(0.001f); a->release_acc = decay_of(0.050f); }
    k = knob[P_GNARL];
    a->ffm = 2.0f * (float)k[0] / 100.0f;
    a->sub = (float)k[1] / 100.0f;
    a->track = (float)k[2] / 100.0f;
    a->level = 2.0f * (float)k[3] / 100.0f;
    k = knob[P_SWEEP];
    a->sweep_oct = 4.0f * (float)k[0] / 100.0f;                                           /* up to four octaves open */
    a->sweep_res = (float)k[3] / 100.0f;
    a->base = tekno.acid_root = (uint8_t)(knob[P_KICK][0] + 12 + 12 * knob[P_ANOTES][1] + knob[P_ANOTES][2]);   /* + OCT, TRNS */
}
static void init_acid(void)   /* the fixed filters */
{
    struct tekno_acid *a = &tekno.acid;
    svf_set(&a->click, 200.0f, 0.70710678f);
    svf_set(&a->notch, 7.5164f, 0.2f);
    svf_set(&a->drive_pre, 720.0f, 0.7071f);
    a->pre_a = hp_a(44.486f, 88200.0f); a->fb_a = hp_a(150.0f, 88200.0f); a->post_a = hp_a(24.167f, 44100.0f);
    a->b0max = ladder_b0(20000.0f);
    static const float q[2] = {0.5411961f, 1.306563f};
    float *down[2] = {a->down0, a->down1};
    for (unsigned i = 0; i < 2; ++i) {
        float g = tanf_(3.1415927f * 9500.0f / 44100.0f), r = 1.0f / q[i], h = 1.0f / (1.0f + r * g + g * g);
        down[i][0] = g * g * h; down[i][1] = 2.0f * (g * g - 1.0f) * h; down[i][2] = (1.0f - r * g + g * g) * h;
    }
}
static void load_perc_sound(unsigned n)   /* SND: the sound's values into the knobs */
{
    for (unsigned v = 0; v < 2; ++v) {
        perc_wave[v] = perc_sounds[n].wave[v];
        for (unsigned i = 0; i < 4; ++i) knob[P_PERCA + v][i] = perc_sounds[n].v[v][i];
    }
    knob[P_PERC][2] = perc_sounds[n].ring;
}
static void load_perc_pattern(unsigned n)   /* PATN: in at the next bar */
{
    const char *st = perc_patterns[n].steps;
    for (unsigned i = 0; i < TEKNO_STEPS; ++i) {
        char c = st[i], l = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        tekno.perc_want[i] = (uint8_t)((l == 'a' ? 1 : l == 'b' ? 2 : l == 'x' ? 3 : 0) | (c >= 'A' && c <= 'Z' ? 4 : 0));
    }
    tekno.perc_dirty = 1;
}
/* the acid's SCALE (NOTES page): a line's notes are read as scale degrees (its root, 2nd, 3rd ... 7th, as
 * written) and played in the scale chosen: DARK's grinding 2nd is a flat 2nd in PHRYG, a whole tone in MINOR */
enum { SCALES = 5 };
static const char *const scale_name[SCALES] = {"LINE", "MINOR", "PHRYG", "DORIAN", "HARM"};
static const uint8_t scale_steps[SCALES][7] = {{0}, {0, 2, 3, 5, 7, 8, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 11}};
static unsigned in_scale(unsigned semis)
{
    static const uint8_t degree[12] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};   /* (a written note's degree) */
    unsigned sc = (unsigned)knob[P_ANOTES][0];
    if (!sc || sc >= SCALES) return semis;
    return semis / 12U * 12U + scale_steps[sc][degree[semis % 12U]];
}
static void load_acid(unsigned p)
{
    const char *n = acid_lines[p].notes, *sl = acid_lines[p].slides;
    for (unsigned i = 0; i < TEKNO_STEPS; ++i) {
        char c = n[i];
        uint16_t w = 0;
        if (c >= 'A' && c <= 'X') w = (uint16_t)(TEKNO_ACID_NOTE | TEKNO_ACID_ACCENT | in_scale((unsigned)(c - 'A')));
        else if (c >= 'a' && c <= 'x') w = (uint16_t)(TEKNO_ACID_NOTE | in_scale((unsigned)(c - 'a')));
        else if (c == '-') w = TEKNO_ACID_TIE;
        if (sl[i] == '~' && w) w |= TEKNO_ACID_SLIDE;
        tekno.acid_want[i] = w;
    }
    tekno.acid_dirty = 1;
    acid_line = (uint8_t)p;
}

/* the knobs into the audio side's words */
static void apply(void)
{
    struct tekno_params *t = &tekno;
    const int16_t *k = knob[0];
    t->kick_inc = dv.note_inc[k[0]];
    {   /* FINE (on KICK's second page): cents on the kick and on every note that follows it */
        int cents = knob[P_KICK2][3];
        if (cents) t->kick_inc = (uint32_t)((float)t->kick_inc * exp2f_((float)cents / 1200.0f));
        t->acid.fine = t->gren.fine = (float)cents / 100.0f;
    }
    t->kick_pitch = (int32_t)((24.0f + 0.24f * (float)k[1]) * 65536.0f);              /* 24..48 semitones above */
    t->kick_pitch_k = per_16(6.0f + 0.3f * (float)k[1]);                                /* 6..36 ms */
    t->kick_decay_k = k[2] ? per_sample(60.0f * exp2f_((float)(100 - k[2]) * 0.07f)) : (1 << 30);   /* flat .. 60 ms */
    if (k[3]) {
        float d = (float)k[3] / 100.0f, g = d * d * 24.0f;
        t->kick_drive_norm = (int32_t)(32768.0f / tanhf_(g));
        t->kick_drive_neg = (int32_t)(g * 0.4f * 4096.0f);   /* lopsided: the 2nd harmonic as loud as the 3rd (as measured) */
        t->kick_drive_pos = (int32_t)(g * 4096.0f);
        float b = d > 0.26f ? KICK_BIAS * (d - 0.26f) / 0.74f : 0.0f;   /* above the default (26) an offset grows with the
                                                                           * drive: pushed hard it stays lopsided */
        t->kick_drive_bias = (int32_t)(b * 32768.0f);
        t->kick_drive_rest = (int32_t)(tanhf_(b) * 32768.0f);
    } else t->kick_drive_pos = 0;
    k = knob[P_ROLL];
    t->roll_feel = feels[k[0]].mask;
    t->roll_decay_k = per_sample(15.0f + 1.6f * (float)k[1]);                           /* 15..175 ms */
    {
        float hz = 80.0f * exp2f_((float)k[2] * 0.055f), x = 3.1415927f * hz / 44100.0f;   /* 80 Hz .. 3.6 kHz */
        t->roll_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->roll_damp = (int32_t)(1.1f * 32768.0f);
    }
    if (k[3]) {
        float g = 1.0f + (float)k[3] * 0.08f;
        t->roll_drive_norm = (int32_t)(32768.0f / tanhf_(g));
        t->roll_drive = (int32_t)(g * 4096.0f);
    } else t->roll_drive = 0;
    k = knob[P_TOMS];
    t->tom_inc[0] = dv.note_inc[k[0]];
    t->tom_inc[1] = dv.note_inc[clamp(k[0] - k[1], 0, 127)];
    t->tom_pitch = 5 * 65536;                                                           /* a small drop onto the note */
    t->tom_pitch_k = per_16(15.0f);
    t->tom_decay_k = per_sample(20.0f + 2.8f * (float)k[2]);                            /* 20..300 ms */
    t->tom_level = k[3] * 110;
    t->tom_skin = 9000;
    k = knob[P_HATS];
    {
        float hz = 3000.0f * exp2f_((float)k[0] * 0.02f), x = 3.1415927f * hz / 44100.0f;   /* 3 .. 12 kHz */
        t->hat_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->hat_damp = (int32_t)(1.3f * 32768.0f);
    }
    t->hat_closed_k = per_sample(4.0f + 0.4f * (float)k[1]);                              /* 4..44 ms */
    t->hat_open_k = per_sample(20.0f + 1.6f * (float)k[2]);                               /* 20..180 ms */
    t->hat_level = k[3] * 55;
    k = knob[P_CLAP];
    {
        float hz = 600.0f * exp2f_((float)k[0] * 0.025f), x = 3.1415927f * hz / 44100.0f;    /* 600 Hz .. 3.4 kHz */
        t->clap_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->clap_damp = (int32_t)(1.3f * 32768.0f);                                         /* a wide band */
        hz *= 1.2f; x = 3.1415927f * (hz > 12000.0f ? 12000.0f : hz) / 44100.0f;               /* the room: a bit higher, wider */
        t->clap_room_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->clap_room_damp = (int32_t)(1.6f * 32768.0f);
    }
    t->clap_gap = (uint32_t)((4.0f + 0.12f * (float)k[1]) * 44.1f);                       /* 4..16 ms */
    t->clap_burst_k = per_sample(4.0f);
    t->clap_air = 14000;
    t->clap_tail_k = per_sample(30.0f + 3.0f * (float)k[2]);                              /* the room: 30..330 ms */
    t->clap_level = k[3] * 350;
    k = knob[P_CLAP2];
    {
        float x = 3.1415927f * 220.0f / 44100.0f;                                          /* BODY: a wide band at 220 Hz */
        t->clap_body_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->clap_body_damp = (int32_t)(1.4f * 32768.0f);
    }
    t->clap_body = k[0] * 700;
    t->clap_room = k[1] * 327;
    if (k[2]) {
        float g = 1.0f + (float)k[2] * 0.06f;
        t->clap_drive_norm = (int32_t)(32768.0f / tanhf_(g));
        t->clap_drive = (int32_t)(g * 4096.0f / 4.0f);   /* (the clap's sum runs ~4x full scale) */
    } else t->clap_drive = 0;
    t->clap_vary = k[3] * 327;
    k = knob[P_PHASER];
    t->ph_depth = k[0] * 327;
    t->ph_bars_shift = (uint8_t)k[1];
    t->ph_feedback = k[2] * 327;
    t->ph_mix = k[3] * 327;
    k = knob[P_FLANGE];
    t->fl_depth = k[0] * 327;
    t->fl_shift = (uint8_t)k[1];
    t->fl_feedback = k[2] * 327;
    t->fl_mix = k[3] * 327;
    {   /* GRENADIER (the cart's mapping: BASE 80 Hz .. 3 kHz log, SPACE 1 .. 3.5x, Q 1 .. 14) */
        struct tekno_gren *g = &t->gren;
        k = knob[P_GREN];
        g->base_hz = 80.0f * exp2f_((float)k[0] * 0.05229f);
        g->space = 1.0f + 2.5f * (float)k[1] / 100.0f;
        g->r = 1.0f / (1.0f + 13.0f * (float)k[2] / 100.0f);
        g->morph = 1.0f / (1.0f - (float)k[3] / 100.0f * 0.99f + 0.01f);
        k = knob[P_MOVE];
        g->move_oct = 2.0f * (float)k[0] / 100.0f;
        t->gren_rate = (uint8_t)k[1];
        t->gren_gate = (uint8_t)k[2];
        g->level = 1.0f * (float)k[3] / 100.0f;
        k = knob[P_GSRC];
        g->source = (unsigned)k[0];
        g->detune = 0.0005f + 0.017f * (float)k[1] / 100.0f;                            /* SAWS: 1 .. 30 cents */
        g->ratio = 1.0f + 3.0f * (float)k[1] / 100.0f + 0.137f;                          /* RING: 1.14 .. 4.14, never whole */
    }
    k = knob[P_SWEEP];
    t->asw_shift = (uint8_t)k[1];
    t->asw_shape = (uint8_t)k[2];
    k = knob[P_VOWEL];
    t->vow_center = k[0] * (TEKNO_VOWELS - 1) * 256 / 100;
    t->vow_depth = k[1] * 32 * 256 / 100;                                               /* up to half the path each way */
    t->vow_rate = (uint8_t)k[2];
    t->vow_mix = k[3] * 327;
    k = knob[P_KICK2];
    t->kick_tone = lowpass(120.0f * exp2f_((float)k[0] * 0.06f));                       /* 120 Hz .. 7.7 kHz */
    t->kick_click = k[1] * 200;
    t->kick_level = k[2] * 300;
    t->roll_level = knob[P_MIX1][1] * 20000 / 62;                                      /* (62: as it always was) */
    apply_acid();
    {   /* PERC */
        const int16_t *pk = knob[P_PERC];
        t->perc_ring = pk[2] * 327;
        t->perc_level = pk[3] * 260;
        t->perc_drop_k = per_16(15.0f);
        static const float lfo_hz[TEKNO_MODS] = {0, 0, 0, 6.0f, 18.0f, 8.0f, 24.0f, 0};
        for (unsigned v = 0; v < 2; ++v) {
            const int16_t *vk = knob[P_PERCA + v];
            struct tekno_perc_voice *pv = &t->perc[v];
            pv->wave = perc_wave[v];
            pv->mod = (uint8_t)vk[2];
            pv->inc = dv.note_inc[vk[0]];
            pv->inc_low = dv.note_inc[clamp(vk[0] - vk[3], 0, 127)];
            pv->depth = vk[3] * 65536;
            pv->decay_k = per_sample(5.0f * exp2f_((float)vk[1] * 0.0632f));                  /* 5 .. 400 ms */
            pv->lfo_inc = (uint32_t)(lfo_hz[vk[2]] * 1558261.0f);                            /* (16 samples a step) */
        }
    }
    {   /* ECHO and PUMP */
        const int16_t *e = knob[P_ECHO], *sd = knob[P_ECHO2];
        static const uint8_t steps[4] = {1, 2, 3, 4};                                    /* 1/16 1/8 3/16 1/4 */
        t->echo_steps = steps[e[0] & 3];
        t->echo_fb = e[1] * 327;
        t->echo_tone = lowpass(800.0f * exp2f_((float)e[2] * 0.04f));                    /* 800 Hz .. 12.8 kHz */
        t->echo_mix = e[3] * 327;
        for (unsigned i = 0; i < 4; ++i) t->echo_send[i] = sd[i] * 327;
        t->pump_amount = knob[P_MIX3][3] * 327;                                          /* up to a full duck */
        t->pump_k = per_sample(160.0f);                                                   /* swelling back over ~a beat */
    }
    {   /* the DJ FILTER (MIX 3/3): the PERFORM chain's low / high pass on the whole output, as LOOP DECK's
         * FLTR (left low pass, right high pass); only its LPF / HPF bits are TEKNO's */
        struct fx_perf *m = &fx_perf;
        int f = knob[P_MIX3][0];
        int32_t on = m->on & ~(PERF_LPF | PERF_HPF), damp = 34000 - knob[P_MIX3][1] * 280;   /* RES: 34000 .. 6000 */
        static unsigned owned;   /* (off again only if TEKNO turned it on: LOOP DECK's filter is the same one) */
        if (f < -2) { m->lpf_f = gbx_svf(150.0f * gbx_exp2((float)(100 + f) * 0.069f)); m->lpf_damp = damp; m->on = on | PERF_LPF; owned = 1; }
        else if (f > 2) { m->hpf_f = gbx_svf(20.0f * gbx_exp2((float)f * 0.08f)); m->hpf_damp = damp; m->on = on | PERF_HPF; owned = 1; }
        else if (owned) { m->on = on; owned = 0; }
    }
}
static uint8_t step_code(char c)
{
    char l = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    return (uint8_t)((l == 'h' || l == 'x' ? 1 : l == 'l' || l == 'o' ? 2 : 0) | (c >= 'A' && c <= 'Z' ? 4 : 0));
}
static void load_pattern(unsigned lane, unsigned p)
{
    const char *s = patterns[lane][p].steps;
    for (unsigned i = 0; i < TEKNO_STEPS; ++i) tekno.lane_want[lane][i] = step_code(s[i]);
    tekno.lane_dirty = (uint8_t)(tekno.lane_dirty | (1U << lane));
    pattern[lane] = (uint8_t)p;
}
static void start_if_stopped(void)
{
    struct groove_view view = {0};
    groove_get_view(0, 0, &view);
    if (!view.playing) { const uint8_t command = 0xfb; (void)groove_midi_local(&command, 1); }
}
unsigned tekno_active(void) { return active; }
static unsigned group_of(unsigned pg) { unsigned g = 0; while (g + 1U < GROUPS && pg >= group_first[g + 1]) ++g; return g; }
static unsigned group_size(unsigned g) { return (unsigned)(group_first[g + 1] - group_first[g]); }
static void page_title(unsigned pg, char *out)   /* the instrument and where in it: "ACID 2/4" (just "TOMS" for one page) */
{
    unsigned g = group_of(pg), n = group_size(g);
    strcpy(out, pages[group_first[g]].name);
    if (n > 1) { strcat(out, " "); px_number((int)(pg - group_first[g]) + 1, out + strlen(out)); strcat(out, "/"); px_number((int)n, out + strlen(out)); }
}

/* ---- VOWEL: tiny acid jam's formant filter (dreamengine runtime/sound.h, fx_set_formant): four band
 * passes on the measured vowel formants (Synth Secrets' adult male), U O A E I, bandwidths narrowed
 * as its q = 0.6. Filled once into the audio side's table of 65 places. ---- */
static const float vowel_f[5][4] = {{300, 870, 2240, 3400}, {570, 840, 2410, 3400}, {730, 1090, 2440, 3400},
                                    {530, 1840, 2480, 3400}, {270, 2290, 3010, 3400}};
static const float vowel_a[5][4] = {{1.0f, 0.30f, 0.15f, 0.08f}, {1.0f, 0.40f, 0.20f, 0.10f}, {1.0f, 0.50f, 0.30f, 0.10f},
                                    {1.0f, 0.70f, 0.30f, 0.10f}, {1.0f, 0.50f, 0.20f, 0.10f}};
static const float vowel_bw[5][4] = {{65, 90, 130, 170}, {80, 100, 140, 180}, {90, 110, 140, 180}, {70, 100, 130, 170}, {60, 90, 120, 170}};
static const char *vowel_name(int v)
{
    static const char *const names[5] = {"U", "O", "A", "E", "I"};
    return names[(v * 4 + 12) / 100];   /* nearest of the five */
}
static void build_vowels(void)
{
    const float bw_scale = 1.0f / (1.0f + 0.6f * 3.0f);
    for (unsigned p = 0; p < TEKNO_VOWELS; ++p) {
        float pos = (float)p * 4.0f / (float)(TEKNO_VOWELS - 1);
        int vi = (int)pos; if (vi > 3) vi = 3;
        float fr = pos - (float)vi;
        for (unsigned i = 0; i < 4; ++i) {
            float fc = vowel_f[vi][i] * (1.0f - fr) + vowel_f[vi + 1][i] * fr;
            float am = vowel_a[vi][i] * (1.0f - fr) + vowel_a[vi + 1][i] * fr;
            float bw = (vowel_bw[vi][i] * (1.0f - fr) + vowel_bw[vi + 1][i] * fr) * bw_scale;
            float kk = bw / fc; kk = kk < 0.02f ? 0.02f : kk > 0.99f ? 0.99f : kk;
            float x = 3.1415927f * fc / 44100.0f;
            tekno.vowel[p][0][i] = (int16_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
            tekno.vowel[p][1][i] = (int16_t)(kk * 32768.0f);
            tekno.vowel[p][2][i] = (int16_t)(am * 32767.0f);
        }
    }
}


/* ---- SCENES: the whole setup (the layers, every pattern, every knob, the tempo). PRESETS browses them and
 * the one chosen comes in on the next bar (instantly when stopped): six factory scenes, then the player's
 * own, saved by holding SAVE and pressing a white key (sixteen slots). Kept in Flash: two copies of one
 * sector, written in turn, the header last, so a cut while saving keeps the other. ---- */
enum { FACTORY = 6, USER_SLOTS = 16, SCENE_MAGIC = 0x31534B54u /* "TKS1" */, SCENE_VERSION = PAGES * 256 + LAYERS };
struct scene { uint8_t want, acid, tempo, perc_wave[2], pattern[LANES]; int16_t knob[PAGES][4]; };
struct scene_store { uint32_t magic, version, seq; uint8_t used[USER_SLOTS]; struct scene slot[USER_SLOTS]; };
typedef char scene_store_fits[sizeof(struct scene_store) <= FM1_FLASH_SECTOR_BYTES ? 1 : -1];
static struct scene_store store;
static int16_t knob_default[PAGES][4];
static struct scene pending_scene;
static unsigned scene_pending, scene_pending_bar, scene_at, scene_flash_until;
static char scene_name[16];
struct tweak { uint8_t page, i; int16_t v; };
static const struct {
    const char *name; uint8_t want, tempo, pattern[LANES], acid, perc_sound; struct tweak t[6];
} factory[FACTORY] = {
    {"INIT", TEKNO_KICK, 165, {0, 0, 0}, 0, 0, {{0, 0, 0}}},
    /* after Desert Planet: rolling bass on C2, toms on every sixteenth, short hats, the phaser over 4 bars */
    {"DESERT", TEKNO_ROLL | TEKNO_TOMS | TEKNO_HATS, 161, {0, 0, 0}, 0, 0, {{P_KICK, 0, 36}, {P_ROLL, 0, 1}}},
    /* after Axe To Grind: the rumble on A1, galloping toms and hats */
    {"GRINDER", TEKNO_KICK | TEKNO_TOMS | TEKNO_HATS, 172, {1, 2, 0}, 0, 0, {{0, 0, 0}}},
    /* after Axe Breaker: the rolling gallop, tribe toms, offbeat hats, the clap broken across the bar */
    {"BREAKER", TEKNO_ROLL | TEKNO_TOMS | TEKNO_HATS | TEKNO_CLAP, 168, {2, 1, 2}, 0, 0, {{0, 0, 0}}},
    /* after Instant Coffee and Floatdemonium: 185, the fat driven rumble, 16th hats, the mental acid */
    {"MENTAL", TEKNO_KICK | TEKNO_HATS | TEKNO_ACID, 185, {0, 0, 0}, 3, 0, {{P_KICK, 3, 80}, {P_PHASER, 1, 1}, {P_ACID, 0, 45}}},
    /* the grenadier droning over the rumble, tribe toms, PERC bloops */
    {"DRONE", TEKNO_KICK | TEKNO_TOMS | TEKNO_GREN | TEKNO_PERC, 160, {2, 0, 0}, 0, 0,
     {{P_MOVE, 2, 0}, {P_MOVE, 1, 5}, {P_MOVE, 0, 70}, {P_PERC, 1, 2}}},
};
static void scene_capture(struct scene *sc)
{
    sc->want = tekno.want; sc->acid = acid_line; sc->tempo = (uint8_t)groove_tempo_current();
    for (unsigned v = 0; v < 2; ++v) sc->perc_wave[v] = perc_wave[v];
    for (unsigned l = 0; l < LANES; ++l) sc->pattern[l] = pattern[l];
    for (unsigned pg = 0; pg < PAGES; ++pg) for (unsigned i = 0; i < 4; ++i) sc->knob[pg][i] = knob[pg][i];
}
static void scene_factory(unsigned n, struct scene *sc)
{
    sc->want = factory[n].want; sc->acid = factory[n].acid; sc->tempo = factory[n].tempo;
    for (unsigned l = 0; l < LANES; ++l) sc->pattern[l] = factory[n].pattern[l];
    for (unsigned pg = 0; pg < PAGES; ++pg) for (unsigned i = 0; i < 4; ++i) sc->knob[pg][i] = knob_default[pg][i];
    unsigned ps = factory[n].perc_sound;   /* (its PERC sound, as SND loads it) */
    for (unsigned v = 0; v < 2; ++v) {
        sc->perc_wave[v] = perc_sounds[ps].wave[v];
        for (unsigned i = 0; i < 4; ++i) sc->knob[P_PERCA + v][i] = perc_sounds[ps].v[v][i];
    }
    sc->knob[P_PERC][0] = (int16_t)ps; sc->knob[P_PERC][2] = perc_sounds[ps].ring;
    for (unsigned k = 0; k < 6; ++k) if (factory[n].t[k].page || factory[n].t[k].i || factory[n].t[k].v)
        sc->knob[factory[n].t[k].page][factory[n].t[k].i] = factory[n].t[k].v;
}
static void scene_apply_sound(const struct scene *sc)   /* the knobs and the tempo (at the bar) */
{
    for (unsigned pg = 0; pg < PAGES; ++pg) for (unsigned i = 0; i < 4; ++i) knob[pg][i] = sc->knob[pg][i];
    for (unsigned v = 0; v < 2; ++v) perc_wave[v] = sc->perc_wave[v];
    groove_tempo_edit((int)sc->tempo - (int)groove_tempo_current());
    apply();
}
static void scene_queue(const struct scene *sc)   /* layers and patterns now (the audio takes them at the bar), the rest at the bar */
{
    for (unsigned l = 0; l < LANES; ++l) load_pattern(l, sc->pattern[l]);
    load_acid(sc->acid);
    load_perc_pattern((unsigned)sc->knob[P_PERC][1]);
    tekno.want = sc->want; tekno.engaged = sc->want != 0;
    pending_scene = *sc;
    if (!cr78_running) { scene_apply_sound(&pending_scene); scene_pending = 0; }
    else { scene_pending = 1; scene_pending_bar = tekno_bar; }
}
static void scene_tick(void)   /* (each frame) the bar came: the knobs and tempo follow */
{
    if (scene_pending && (!cr78_running || tekno_bar != scene_pending_bar)) { scene_apply_sound(&pending_scene); scene_pending = 0; }
}
static void store_load(void)
{
    struct { uint32_t magic, version, seq; } h[2];
    int best = -1;
    for (unsigned c = 0; c < 2; ++c) {
        if (fm1_flash_read(GROOVE_TEKNO_OFFSET + c * FM1_FLASH_SECTOR_BYTES, &h[c], sizeof h[c]) ||
            h[c].magic != SCENE_MAGIC || h[c].version != SCENE_VERSION) continue;
        if (best < 0 || h[c].seq > h[best].seq) best = (int)c;
    }
    if (best < 0 || fm1_flash_read(GROOVE_TEKNO_OFFSET + (unsigned)best * FM1_FLASH_SECTOR_BYTES, &store, sizeof store)) {
        memset(&store, 0, sizeof store);
    }
}
static int store_save(unsigned slot)
{
    if (fm1_flash_size() < GROOVE_FLASH_BYTES) return FM1_FLASH_UNAVAILABLE;
    scene_capture(&store.slot[slot]);
    store.used[slot] = 1;
    store.magic = SCENE_MAGIC; store.version = SCENE_VERSION; ++store.seq;
    uint32_t at = GROOVE_TEKNO_OFFSET + (store.seq & 1U) * FM1_FLASH_SECTOR_BYTES;   /* the other copy than last time */
    int r = fm1_flash_erase(at);
    if (!r) r = fm1_flash_write(at + 12, (const uint8_t *)&store + 12, sizeof store - 12);
    if (!r) r = fm1_flash_write(at, &store, 12);   /* the header last */
    return r;
}
static unsigned scene_count(void)   /* the browsing list: the factory ones, then the slots saved */
{
    unsigned n = FACTORY;
    for (unsigned k = 0; k < USER_SLOTS; ++k) n += store.used[k];
    return n;
}
static void scene_pick(unsigned at)   /* entry `at` of the list: queue it, name it */
{
    struct scene sc;
    if (at < FACTORY) { scene_factory(at, &sc); strcpy(scene_name, factory[at].name); }
    else {
        unsigned k = 0, n = at - FACTORY;
        while (k < USER_SLOTS && (!store.used[k] || n--)) ++k;
        if (k >= USER_SLOTS) return;
        sc = store.slot[k];
        strcpy(scene_name, "USER "); px_number((int)k + 1, scene_name + 5);
    }
    scene_queue(&sc);
}

/* ---- the screen ---- */
static void value_text(unsigned pg, unsigned i, char *out)
{
    int v = knob[pg][i];
    if ((pg == P_KICK || pg == P_TOMS) && i == 0) { strcpy(out, note_name[v % 12]); px_number(v / 12 - 1, out + strlen(out)); }
    else if (pg == P_ROLL && i == 0) strcpy(out, feels[v].name);
    else if (pg == P_MOVE && i == 1) { static const char *const rates[7] = {"1/2 B", "BEAT", "2 B", "1", "2", "4", "8"}; strcpy(out, rates[v]); }
    else if ((pg == P_PHASER || pg == P_FLANGE || pg == P_SWEEP) && i == 1) px_number(1 << v, out);
    else if (pg == P_MOVE && i == 2) { static const char *const gates[TEKNO_GATES] = {"DRONE", "4THS", "8THS", "16THS", "GALLOP"}; strcpy(out, gates[v]); }
    else if (pg == P_GREN && i == 0) px_number((int)(80.0f * exp2f_((float)v * 0.05229f)), out);
    else if (pg == P_SWEEP && i == 2) { static const char *const shapes[3] = {"WAVE", "RISE", "FALL"}; strcpy(out, shapes[v]); }
    else if ((pg == P_SWEEP && i == 0) || (pg == P_FLANGE && i == 3)) { if (v) px_number(v, out); else strcpy(out, "OFF"); }
    else if (pg == P_VOWEL && i == 0) strcpy(out, vowel_name(v));
    else if (pg == P_VOWEL && i == 2) { if (v) px_number(1 << (v - 1), out); else strcpy(out, "BEAT"); }
    else if (pg == P_VOWEL && i == 3 && !v) strcpy(out, "OFF");
    else if (pg == P_MIX2 && i == 3) px_number((int)groove_tempo_current(), out);
    else if (pg == P_ANOTES && i == 0) strcpy(out, scale_name[v]);
    else if (pg == P_GSRC && i == 0) { static const char *const src[TEKNO_GSRCS] = {"TRAP", "SAWS", "RING", "NOISE", "DIDGE"}; strcpy(out, src[v]); }
    else if (pg == P_ANOTES && i > 0 && v > 0) { strcpy(out, "+"); px_number(v, out + 1); }
    else if (pg == P_ECHO && i == 0) { static const char *const times[4] = {"1/16", "1/8", "3/16", "1/4"}; strcpy(out, times[v & 3]); }
    else if ((pg == P_ECHO2 || (pg == P_MIX3 && i == 3)) && !v) strcpy(out, "OFF");
    else if (pg == P_PERC && i == 0) strcpy(out, perc_sounds[v].name);
    else if (pg == P_PERC && i == 1) strcpy(out, perc_patterns[v].name);
    else if ((pg == P_PERCA || pg == P_PERCB) && i == 0) { strcpy(out, note_name[v % 12]); px_number(v / 12 - 1, out + strlen(out)); }
    else if ((pg == P_PERCA || pg == P_PERCB) && i == 1) px_number((int)(5.0f * exp2f_((float)v * 0.0632f)), out);
    else if ((pg == P_PERCA || pg == P_PERCB) && i == 2) strcpy(out, mod_name[v]);
    else if (pg == P_MIX3 && i == 0) { if (v < -2) { strcpy(out, "LP "); px_number(-v, out + 3); } else if (v > 2) { strcpy(out, "HP "); px_number(v, out + 3); } else strcpy(out, "OFF"); }
    else if (pg == P_KICK && i == 2 && !v) strcpy(out, "FLAT");
    else if (pg == P_KICK2 && i == 3) { if (v > 0) { strcpy(out, "+"); px_number(v, out + 1); } else px_number(v, out); }
    else if (pg == P_ACID && i == 0) px_number((int)(100.0f * exp2f_((float)v * 0.056f)), out);
    else if ((pg == P_ACID && i == 3) || (pg == P_DEVIL && i == 1)) {   /* decays in ms */
        int ms = (int)(30.0f * exp2f_((float)v * 0.0664386f));
        if (ms >= 1000) { px_number(ms / 1000, out); strcat(out, "."); px_number(ms / 100 % 10, out + strlen(out)); strcat(out, "S"); }
        else px_number(ms, out);
    }
    else if (((pg == P_DEVIL && i == 3) || (pg == P_GNARL && i < 3)) && !v) strcpy(out, "OFF");
    else px_number(v, out);
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    for (unsigned i = 0; i < 4; ++i) value_text(page, i, v[i]);
    unsigned blink = (now / 140U) & 1U;
    uint32_t lit = 0;   /* the layers in the black keys' order, waiting ones blinking */
    for (unsigned l = 0; l < LAYERS; ++l) {
        unsigned on = (tekno_layers & layer_bit[l]) != 0, will = (tekno.want & layer_bit[l]) != 0;
        if (on != will && blink) on = !on;
        lit |= on << l;
    }
    px_cls(PX_NIGHT);
    /* the layers are the pads at the bottom, each lit in its own colour */
    {   /* the header: the page's name and where in its instrument ("ACID 1/4"); one dot an instrument */
        static struct px_page shown[GROUPS];
        static char title[16];
        unsigned g = group_of(page);
        page_title(page, title);
        shown[g].name = title;
        for (unsigned i = 0; i < 4; ++i) shown[g].knob[i] = pages[page].knob[i];
        px_chrome_pads_marked("TEKNO", shown, GROUPS, g, value, pad_name, lit, layer_color, page_layers[page]);
    }
    px_print("TEKNO", 2, 1, PX_GOLD);
    /* the three lanes, a row each (accents tall; toms' low and hats' open hits in their second colour),
     * dark when the layer is off; the playhead below */
    unsigned step = tekno_step;
    static const uint8_t lane_color[LANES][2] = {{PX_CYAN, PX_GREEN}, {PX_WHITE, PX_PINK}, {PX_GOLD, PX_GOLD}};
    for (unsigned lane = 0; lane < LANES; ++lane) {
        const char *steps = patterns[lane][pattern[lane]].steps;
        unsigned on = (tekno.want & layer_bit[lane_layer[lane]]) != 0;
        for (unsigned s = 0; s < 16; ++s) {
            uint8_t code = step_code(steps[s]);
            if (!(code & 3U)) continue;
            int x = 2 + (int)s * 5, y = 10 + (int)lane * 5;
            unsigned color = on ? lane_color[lane][(code & 3U) == 2U] : PX_DUSK;
            if (code & 4U) px_rect(x, y, 4, 4, color); else px_rect(x, y + 2, 4, 2, color);
        }
    }
    {   /* PERC: voice A low, B high, both full; accents brighter */
        unsigned on = (tekno.want & TEKNO_PERC) != 0;
        const char *st = perc_patterns[knob[P_PERC][1]].steps;
        for (unsigned s = 0; s < 16; ++s) {
            char c = st[s], l = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
            if (l == '.') continue;
            unsigned color = !on ? PX_DUSK : c < 'a' ? PX_WHITE : PX_VIOLET;
            int x = 2 + (int)s * 5;
            if (l == 'a' || l == 'x') px_rect(x, 27, 4, 2, color);
            if (l == 'b' || l == 'x') px_rect(x, 25, 4, 2, color);
        }
    }
    {   /* the acid's line: notes higher up the higher they are, slides joined to the next, ties held on */
        const char *n = acid_lines[acid_line].notes, *sl = acid_lines[acid_line].slides;
        unsigned on = (tekno.want & TEKNO_ACID) != 0, color = on ? PX_GREEN : PX_DUSK;
        int last_y = 0;
        for (unsigned s = 0; s < 16; ++s) {
            char c = n[s];
            int x = 2 + (int)s * 5;
            if (c == '-') { if (last_y) px_rect(x - 1, last_y, 5, 1, color); continue; }
            if (c == '.') { last_y = 0; continue; }
            int semis = c >= 'a' ? c - 'a' : c - 'A';
            int y = 33 - (semis > 15 ? 15 : semis) / 4;   /* rows 30..33 */
            px_rect(x, y, 4, c < 'a' ? 2 : 1, color);
            if (sl[s] == '~') px_rect(x + 4, y, 1, 1, color);
            last_y = y;
        }
    }
    for (unsigned s = 0; s < 16; ++s)
        px_rect(2 + (int)s * 5, 37, 4, 1, cr78_running && s == step ? PX_WHITE : (s & 3U) ? PX_SLATE : PX_GOLD);
    char line[24];
    if (scene_pending || (int32_t)(scene_flash_until - now) > 0) {   /* a scene: blinking until its bar */
        strcpy(line, "SCENE "); strcat(line, scene_name);
        if (scene_pending && blink) line[0] = 0;
        px_print(line, 2, 40, PX_WHITE);
    } else if (shown_lane == LANES) {
        strcpy(line, "ACID "); strcat(line, acid_lines[acid_line].name);
        if (acid_line != acid_line_playing && blink) line[0] = 0;
        px_print(line, 2, 40, PX_GREEN);
    } else {
        strcpy(line, lane_name[shown_lane]); strcat(line, " "); strcat(line, patterns[shown_lane][pattern[shown_lane]].name);
        if (pattern[shown_lane] != pattern_playing[shown_lane] && blink) line[0] = 0;
        px_print(line, 2, 40, lane_color[shown_lane][0]);
    }
    /* the effects row: a box each, lit in its colour when the effect is on, a mark under it riding its
     * sweep (PHASER, FLANGE, the acid's SWEEP, grenadier's MOVE, VOWEL); VOWEL's vowel large on the right */
    {
        static char vw[3] = "VW";
        static const char *const fx_name[7] = {"PH", "FL", "SW", "MV", vw, "EC", "DJ"};
        static const uint8_t fx_color[7] = {PX_PINK, PX_WHITE, PX_GREEN, PX_RED, PX_GOLD, PX_EMBER, PX_CYAN};
        int fltr = knob[P_MIX3][0];
        unsigned echo_on = knob[P_ECHO][3] && (knob[P_ECHO2][0] | knob[P_ECHO2][1] | knob[P_ECHO2][2] | knob[P_ECHO2][3]);
        const unsigned fx_on[7] = {tekno.ph_mix != 0, tekno.fl_mix != 0, knob[P_SWEEP][0] != 0,
                                   (tekno.want & TEKNO_GREN) && knob[P_MOVE][0], tekno.vow_mix != 0, echo_on, fltr < -2 || fltr > 2};
        const uint16_t fx_at[7] = {tekno_lfo, tekno_flfo, tekno_alfo, tekno_glfo, tekno_vlfo, (uint16_t)(tekno_step * 4096U),
                                   (uint16_t)((fltr + 100) * 327)};
        vw[1] = tekno.vow_mix ? vowel_name((int)tekno_vowel_at * 100 / ((TEKNO_VOWELS - 1) * 256))[0] : 'W';   /* VOWEL: its vowel */
        for (unsigned e = 0; e < 7; ++e) {
            int x = 2 + (int)e * 11;
            px_rect(x, 45, 10, 7, fx_on[e] ? fx_color[e] : PX_DUSK);
            px_print(fx_name[e], x + 2, 46, fx_on[e] ? PX_NIGHT : PX_SLATE);
            if (fx_on[e]) px_rect(x + (int)((uint32_t)fx_at[e] * 9U >> 16), 52, 2, 1, fx_color[e]);   /* (DJ: centre = off) */
        }
    }
    for (unsigned b = 0; b < 8; ++b) px_rect(2 + (int)b * 5, 57, 4, 3, cr78_running && b == tekno_bar ? PX_WHITE : PX_DUSK);
    strcpy(line, ""); px_number((int)groove_tempo_current(), line); strcat(line, " BPM");
    px_print(line, 46, 56, PX_SLATE);
    px_flash_draw(now);
}
static unsigned save_held;
static void leds(uint32_t now)
{
    unsigned blink = (now / 140U) & 1U;
    uint32_t layers = 0;
    for (unsigned l = 0; l < LAYERS; ++l) {
        unsigned on = (tekno_layers & layer_bit[l]) != 0, will = (tekno.want & layer_bit[l]) != 0;
        if (on != will && blink) on = !on;
        layers |= on << l;
    }
    uint32_t whites = 0;   /* each lane's pattern, the one waiting blinking */
    for (unsigned lane = 0; lane < LANES; ++lane)
        whites |= 1U << (4U * lane + (pattern[lane] != pattern_playing[lane] && blink ? pattern_playing[lane] : pattern[lane]) % PATTERNS);
    whites |= 1U << (12U + (acid_line != acid_line_playing && blink ? acid_line_playing : acid_line) % PATTERNS);
    if (save_held) {   /* SAVE held: the slots in use */
        whites = 0;
        for (unsigned k = 0; k < USER_SLOTS; ++k) whites |= (uint32_t)store.used[k] << k;
    }
    uint32_t all = whites | (layers << 16);
    for (unsigned s = 0; s < 16; ++s)
        if (((all ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (all >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((all ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (all >> (16 + i)) & 1U);
    led_state = all;
}

/* ---- the panel ---- */
int tekno_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons, down = c->keys & ~prev_keys;
    unsigned home = px_mode_toggle(pressed, PX_MODE_TEKNO, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!home) return 0;
        dv_build_tables();
        if (!opened_once) {   /* the first time: the ratios, the sound, a tempo of the scene */
            for (unsigned i = 0; i < TEKNO_RATIOS; ++i) tekno.ratio[i] = (uint32_t)(exp2f_((float)i / 96.0f) * 65536.0f);
            for (unsigned pg = 0; pg < PAGES; ++pg) for (unsigned i = 0; i < 4; ++i) knob_default[pg][i] = knob[pg][i];   /* (the factory scenes' base) */
            store_load();
            build_vowels();
            init_acid(); load_acid(0); acid_line_playing = 0;
            load_perc_sound(0); load_perc_pattern(0);
            for (unsigned lane = 0; lane < LANES; ++lane) { load_pattern(lane, 0); pattern_playing[lane] = 0; }
            groove_tempo_edit(165 - (int)groove_tempo_current());
            opened_once = 1;
        }
        apply();
        px_leds_off(); led_state = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash("TEKNO", now, 700U);
        return 1;
    }
    if (home) { px_leds_off(); active = 0; return 2; }
    unsigned changed = 0;
    for (unsigned lane = 0; lane < LANES; ++lane)   /* the audio side took the pattern at the bar */
        if (!((tekno.lane_dirty >> lane) & 1U) && pattern_playing[lane] != pattern[lane]) { pattern_playing[lane] = pattern[lane]; changed = 1; }
    if (!tekno.acid_dirty && acid_line_playing != acid_line) { acid_line_playing = acid_line; changed = 1; }
    /* black keys: the layers */
    for (unsigned i = 0; i < LAYERS; ++i) {
        if (!((down >> black_keys[i]) & 1U)) continue;
        uint8_t bit = layer_bit[i], want = tekno.want;
        if (want & bit) want = (uint8_t)(want & ~bit);
        else want = (uint8_t)(want | bit);
        tekno.want = want;
        tekno.engaged = want != 0;
        if (want & bit) start_if_stopped();
        {   /* and SELECT goes to that instrument (on the page it was left on): KICK for RUMBLE */
            static const uint8_t layer_group[LAYERS] = {0, 1, 2, 3, 4, 5, 6, 7};   /* KICK ROLL TOMS HATS CLAP ACID GREN PERC */
            unsigned g = layer_group[i];
            page = group_first[g] + group_at[g];
        }
        px_flash(layer_name[i], now, 400U);
        changed = 1;
    }
    /* SAVE held: a white key saves the scene in that slot (and the keys do nothing else) */
    unsigned saving = (c->buttons & PX_BIT(PX_SAVE)) != 0;
    if (saving != save_held) changed = 1;
    save_held = saving;
    if (saving) {
        for (unsigned k = 0; k < 16; ++k) {
            if (!((down >> white_keys[k]) & 1U)) continue;
            static char text[24];
            int r = store_save(k);
            strcpy(text, r ? "SAVE FAILED" : "SAVED "); if (!r) px_number((int)k + 1, text + 6);
            px_flash(text, now, 800U);
            changed = 1;
        }
        for (unsigned k = 0; k < 16; ++k) down &= ~(1U << white_keys[k]);   /* (the white keys are taken) */
    }
    /* PRESETS: the scenes, in on the next bar */
    if (c->encoders[6]) {
        unsigned n = scene_count();
        scene_at = (unsigned)(((int)scene_at + c->encoders[6] + 64 * (int)n) % (int)n);
        scene_pick(scene_at);
        px_flash(scene_name, now, 700U);
        scene_flash_until = now + 4000U;
        changed = 1;
    }
    scene_tick();
    /* white keys 1-12: the toms', hats' and clap's patterns, four each */
    for (unsigned s = 0; s < LANES * PATTERNS; ++s) {
        if (!((down >> white_keys[s]) & 1U)) continue;
        unsigned lane = s / PATTERNS, p = s % PATTERNS;
        if (pattern[lane] == p && patterns[lane][p + PATTERNS].name) p += PATTERNS;   /* pressed again: its second */
        load_pattern(lane, p);
        shown_lane = lane;
        static char text[24];
        strcpy(text, lane_name[lane]); strcat(text, " "); strcat(text, patterns[lane][p].name);
        px_flash(text, now, 500U);
        changed = 1;
    }
    /* white keys 13-16: the acid's line */
    for (unsigned key = 0; key < PATTERNS; ++key) {
        if (!((down >> white_keys[LANES * PATTERNS + key]) & 1U)) continue;
        unsigned p = key;
        if (acid_line % PATTERNS == key)   /* pressed again: the key's next line (first, second, third, first ...) */
            p = acid_line + PATTERNS < ACID_LINES ? acid_line + PATTERNS : key;
        load_acid(p);
        shown_lane = LANES;
        static char text[24];
        strcpy(text, "ACID "); strcat(text, acid_lines[p].name);
        px_flash(text, now, 500U);
        changed = 1;
    }
    /* SELECT: pages */
    if (c->encoders[0]) {   /* SELECT: the instruments, each on the page it was left on */
        unsigned g = (unsigned)(((int)group_of(page) + c->encoders[0] + 4 * GROUPS) % GROUPS);
        page = group_first[g] + group_at[g];
        static char text[16];
        page_title(page, text);
        px_flash(text, now, 500U);
        changed = 1;
    }
    if (c->encoders[1]) {   /* ALGORITHM: through the instrument's pages (as the stock firmware's parameter pages) */
        unsigned g = group_of(page), n = group_size(g);
        group_at[g] = (uint8_t)(((int)group_at[g] + c->encoders[1] + 4 * (int)n) % (int)n);
        page = group_first[g] + group_at[g];
        static char text[16];
        if (strcmp(pages[page].name, pages[group_first[g]].name)) strcpy(text, pages[page].name);   /* its own name: DEVIL */
        else page_title(page, text);                                                                  /* or KICK 2/2 */
        px_flash(text, now, 500U);
        changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t *k = c->encoders + 2;
    for (unsigned i = 0; i < 4; ++i) {
        if (!k[i] || !pages[page].knob[i][0]) continue;   /* (a knob with no name does nothing) */
        if (page == P_MIX2 && i == 3) groove_tempo_edit(k[i]);
        else {
            int step = (hi[page][i] - lo[page][i]) > 20 && page != P_ANOTES ? 2 : 1;   /* (TRNS: by semitones) */
            knob[page][i] = (int16_t)clamp(knob[page][i] + k[i] * step, lo[page][i], hi[page][i]);
            if (page == P_ANOTES && i == 0) load_acid(acid_line);
            if (page == P_GSRC && i == 0 && knob[page][0] == TEKNO_GSRC_DIDGE) {   /* DIDGE: the mouth's settings too */
                static const int16_t mouth[2][4] = {{30, 30, 75, 55}, {45, 4, 0, 70}};   /* GREN: BASE SPCE Q MRPH; MOVE: DPTH RATE(2 bars) GATE(drone) LVL */
                for (unsigned j = 0; j < 4; ++j) { knob[P_GREN][j] = mouth[0][j]; knob[P_MOVE][j] = mouth[1][j]; }
                knob[P_MIX2][2] = knob[P_MOVE][3];   /* (its level's twin) */
            }   /* the line in its new scale, at the next bar */
            if (page == P_PERC && i == 0) load_perc_sound((unsigned)knob[page][i]);
            if (page == P_PERC && i == 1) load_perc_pattern((unsigned)knob[page][i]);
            for (unsigned a = 0; a < sizeof level_alias / sizeof level_alias[0]; ++a) {   /* a level: its twin too */
                if (level_alias[a][0] == page && level_alias[a][1] == i) knob[level_alias[a][2]][level_alias[a][3]] = knob[page][i];
                if (level_alias[a][2] == page && level_alias[a][3] == i) knob[level_alias[a][0]][level_alias[a][1]] = knob[page][i];
            }
        }
        apply();
        static char text[24];
        strcpy(text, pages[page].knob[i]); strcat(text, " ");
        value_text(page, i, text + strlen(text));
        px_flash(text, now, 700U);
        changed = 1;
    }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0}; groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (now - last_ms >= 33U || changed) {
        last_ms = now;
        draw(now);
        leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
