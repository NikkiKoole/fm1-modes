/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO: 90s free-party tekno, played live: two kicks, toms, hats, clap, an acid 303, a filterbank drone.
 *
 *   black key 1       RUMBLE on / off: the rumble kick on every beat, its long tuned tail the bass
 *   black key 2       ROLL on / off: a short kick on the beat, bass notes rolling between (RUMBLE
 *                     and ROLL are one at a time: pressing one swaps them at the next bar)
 *   black key 3       TOMS on / off: two tuned toms rolling on the sixteenths
 *   black key 4, 5    HATS, CLAP on / off
 *   black key 6       ACID on / off: a 303 line on the kick's note (an octave up)
 *   black key 7       GRENADIER on / off: a triple-filterbank drone on the same note, chugged by its gate
 *   white keys 1-4    the toms' pattern: RUN, GALLOP, TRIBE (again: 3-3-2), SPARSE
 *   white keys 5-8    the hats' pattern: 16THS, OFFBEAT, GALLOP, BUSY
 *   white keys 9-12   the clap's pattern: BACKBEAT, OFFBEAT, BREAKER, FILL
 *   white keys 13-16  the acid's line: PULSE (again: DOTTED), GALLOP, SQUELCH, MENTAL
 *                     (a lit pattern key pressed again swaps to its second pattern, where it has one)
 *   (the pages)       KICK (TUNE PUNCH DECAY DRIVE) · ROLL (FEEL: GALLOP ROLL OFFBEAT PUSH
 *                     REVERSE HICCUP 3-3-2 DOTTED; DECAY CUTOFF DRIVE)
 *                     · TOMS (TUNE GAP DECAY LEVEL) · HATS (TONE CLOSED OPEN LEVEL)
 *                     · CLAP (TUNE SPREAD DECAY LEVEL) · ACID (CUTOFF RESO ENV DECAY)
 *                     · DEVIL (ACCENT ACCENT-DECAY SLIDE DRIVE) · GNARL (FILTER-FM SUB TRACK LEVEL)
 *                     · SWEEP (the acid's cutoff ridden on the bars: DEPTH BARS SHAPE RESO)
 *                     · GREN (BASE SPACE Q MORPH) · MOVE (its sweep DEPTH and RATE, GATE, LEVEL)
 *                     · PHASER (on the percussion: DEPTH BARS FDBK MIX) · FLANGE (on the hats: DEPTH BARS
 *                     FDBK MIX) · VOWEL (VOWEL MOVE RATE MIX) · MIX (the levels: KICK ROLL TOMS HATS;
 *                     then CLAP ACID GREN, and TEMPO). KICK's second page: TONE CLICK LEVEL FINE (cents).
 *   SELECT            the instruments, the effects and the mix (each on the page it was left on);
 *   ALGORITHM         through the instrument's pages (KICK's two, ACID's four, GREN's two, MIX's two:
 *                     the header says "ACID 1/4"), as the stock firmware flips its parameter pages
 *                     Each level is also on its layer's own page: both move together. The pads a page
 *                     is about are marked at the bottom (a white bar under their boxes).
 *   PLAY              play / stop (the first layer switched on starts it)
 *   HOME              back (it keeps playing)
 * Layers and patterns change on the next bar (their keys blink until then).
 *
 * The sound is set from three references (French hardtek / Czech tribe, 155-174 BPM): the kick's
 * low band holds almost flat for the whole beat, the tail tuned (A1), its 2nd and 3rd harmonics
 * ~22 dB down; the toms a whole tone apart (G#3 / F#3) on every sixteenth; a phaser on the
 * percussion sweeping once every four bars; hats short on every sixteenth (Desert Planet) or short
 * on the beat and long on the & and a (Axe To Grind); claps broken across the bar (Axe Breaker).
 * No swing: the references measured dead straight (the bounce is in which sixteenths are hit).
 *
 * TODO (talked about, not built yet):
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
 *   - SCENES on PRESETS (the biggest win: from one song to a set of songs): a scene is the whole
 *     state (layers, patterns, every knob, tempo), recalled on the next bar. Factory scenes that are
 *     really different tracks (rolling-tom Desert Planet, rumble Axe Grinder, mental acid at 180,
 *     a slow Grenadier drone-tribe, ...) and the user's own saved to Flash (as GROOVEBOX's SAVE).
 *     Today nothing is kept: leaving the mode or powering off starts from the default again
 *   - GENERATORS: "a new one" for the acid line (as X0X's TB-3PO), the toms' pattern, the ROLL
 *     feel; always in key, by the genre's rules (gallop, offbeats, 3-3-2). With hold-to-pick: hold
 *     a layer key, press a GEN key
 *   - KEY AND SCALE: the kick's note is the key already; add a scale for the acid (minor,
 *     phrygian with its flat second, chromatic for mental)
 *   - ARRANGEMENT: live, DROP/BUILD + scenes is a set's shape; later maybe chained scenes over N
 *     bars (intro 16, groove 32, break 8, drop 32) for hands-off songs
 *   - KITS for the sub-genres: other kick models and tempo ranges (tribe 160-175, mental / acidcore
 *     180-200, a slower hard groove 140)
 *   - DROP / BUILD: hold a key to cut the kick and let the rest ride; a roll doubling up
 *     (8 > 16 > 32 a bar) with the filter rising; let go to slam back in on the bar
 *   - DJ FILTER over the whole mix: one knob, low pass to the left, high pass to the right
 *     (drain the kick out over 8 or 16 bars for a build)
 *   - DUB ECHO: a tempo-synced delay (dotted 8th, quarter) to throw onto a clap, tom or acid note,
 *     its repeats filtered darker (the FX rack has no delay, only the spring)
 *   - GRENADIER's CMOS drift: each pulse nudges the filters a little at random (the cart's RND)
 *   - fuller CLAPS: they sound thin next to the references; more noise body, a bit of room
 *   - MORE THAN 16 PATTERNS: the white keys are full (4 lanes x 4) and "press again" only gives
 *     each key a second. Leaning to: HOLD a layer's black key and the 16 white keys pick ITS
 *     pattern (16 per instrument, all seven layers; "hold = scope" as in docs/GROOVEBOX-MANUAL.md).
 *     Unheld, the white keys come free (for DROP/BUILD, mutes, fills). Keep "press again" too.
 *     (Also weighed: a PATTERNS page of knobs, one per lane; banks flipped by a button.)
 *   - SHOW WHAT EACH EFFECT IS ON: the screen doesn't say it. SWEEP is the acid's, MOVE
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
#include <string.h>

enum { PAGES = 17, PATTERNS = 4, FEELS = 8, LAYERS = 7, LANES = TEKNO_LANES };
static const struct px_page pages[PAGES] = {
    {"KICK", {"TUNE", "PNCH", "DCAY", "DRV"}},
    {"KICK", {"TONE", "CLK", "LVL", "FINE"}},
    {"ROLL", {"FEEL", "DCAY", "CUT", "DRV"}},
    {"TOMS", {"TUNE", "GAP", "DCAY", "LVL"}},
    {"HATS", {"TONE", "CLSD", "OPEN", "LVL"}},
    {"CLAP", {"TUNE", "SPRD", "DCAY", "LVL"}},
    {"ACID", {"CUT", "RES", "ENV", "DCAY"}},
    {"DEVIL", {"ACC", "ADEC", "SLID", "DRV"}},
    {"GNARL", {"FFM", "SUB", "TRK", "LVL"}},
    {"SWEEP", {"DPTH", "BARS", "SHAP", "RES"}},
    {"GREN", {"BASE", "SPCE", "Q", "MRPH"}},
    {"MOVE", {"DPTH", "RATE", "GATE", "LVL"}},
    {"PHASER", {"DPTH", "BARS", "FDBK", "MIX"}},
    {"FLANGE", {"DPTH", "BARS", "FDBK", "MIX"}},
    {"VOWEL", {"VOWL", "MOVE", "RATE", "MIX"}},
    {"MIX", {"KICK", "ROLL", "TOMS", "HATS"}},
    {"MIX", {"CLAP", "ACID", "GREN", "TMPO"}},
};
/* knob values, a page a row (see apply()); MIX 1 / MIX 2 are the levels, each also on its layer's own page */
enum { P_KICK, P_KICK2, P_ROLL, P_TOMS, P_HATS, P_CLAP, P_ACID, P_DEVIL, P_GNARL, P_SWEEP, P_GREN, P_MOVE, P_PHASER, P_FLANGE,
       P_VOWEL, P_MIX1, P_MIX2 };
static int16_t knob[PAGES][4] = {{33, 60, 15, 26}, {62, 30, 62, 0}, {0, 40, 45, 40}, {56, 2, 45, 45}, {50, 30, 50, 50}, {40, 40, 40, 50},
                                 {40, 75, 70, 30}, {80, 20, 30, 55}, {25, 0, 0, 60}, {0, 2, 0, 30},
                                 {45, 45, 55, 55}, {50, 5, 3, 60},
                                 {80, 2, 50, 50}, {70, 3, 60, 0}, {50, 60, 3, 0},
                                 {62, 62, 45, 50}, {50, 60, 60, 0}};
static const int16_t lo[PAGES][4] = {{28, 0, 0, 0}, {0, 0, 0, -50}, {0, 0, 0, 0}, {48, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                     {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                     {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
static const int16_t hi[PAGES][4] = {{39, 100, 100, 100}, {100, 100, 100, 50}, {FEELS - 1, 100, 100, 100}, {72, 12, 100, 100},
                                     {100, 100, 100, 100}, {100, 100, 100, 100}, {100, 100, 100, 100}, {100, 100, 100, 100},
                                     {100, 100, 100, 100}, {100, 3, 2, 100}, {100, 100, 100, 100}, {100, 6, 4, 100},
                                     {100, 3, 95, 100}, {100, 3, 95, 100}, {100, 100, 5, 100}, {100, 100, 100, 100}, {100, 100, 100, 0}};
/* the levels live in two places (their layer's page and MIX 1 / 2): turning one moves the other */
static const uint8_t level_alias[][4] = {   /* page, knob  <->  page, knob */
    {P_KICK2, 2, P_MIX1, 0}, {P_TOMS, 3, P_MIX1, 2}, {P_HATS, 3, P_MIX1, 3},
    {P_CLAP, 3, P_MIX2, 0}, {P_GNARL, 3, P_MIX2, 1}, {P_MOVE, 3, P_MIX2, 2}};
/* SELECT steps through the instruments (and the effects, the mix), ALGORITHM through an instrument's pages:
 * each group's first page, and which of its pages it was left on */
enum { GROUPS = 11 };
static const uint8_t group_first[GROUPS + 1] = {P_KICK, P_ROLL, P_TOMS, P_HATS, P_CLAP, P_ACID, P_GREN, P_PHASER, P_FLANGE,
                                                P_VOWEL, P_MIX1, PAGES};
static uint8_t group_at[GROUPS];
/* which layers (pad bit l = black key l + 1) each page is about: their boxes are marked at the bottom */
static const uint8_t page_layers[PAGES] = {
    0x03, 0x03, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20, 0x20, 0x40, 0x40,   /* KICK (RUMBLE and ROLL's) .. MOVE */
    0x1C, 0x08, 0x7F, 0x7F, 0x7F};                                              /* PHASER (toms hats clap), FLANGE (hats), VOWEL, MIX */
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
static const uint8_t layer_bit[LAYERS] = {TEKNO_KICK, TEKNO_ROLL, TEKNO_TOMS, TEKNO_HATS, TEKNO_CLAP, TEKNO_ACID, TEKNO_GREN};
static const char *const layer_name[LAYERS] = {"RUMBLE", "ROLL", "TOMS", "HATS", "CLAP", "ACID", "GRENADIER"};
static const uint8_t layer_color[PX_PADS] = {PX_EMBER, PX_GOLD, PX_CYAN, PX_WHITE, PX_PINK, PX_GREEN, PX_RED};   /* (their pads' colour) */
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
static const struct { const char *name, *notes, *slides; } acid_lines[VARIANTS] = {
    {"PULSE",   "a.Ma.aMaa.Ma.dMa", "..~.....~...~..."},   /* one note and its octave, hypnotic */
    {"GALLOP",  "..aM..am..aM..kD", "..~...~...~...~."},   /* on the gallop's & and a, between the kicks */
    {"SQUELCH", "A-m-a~mDa-M-k~hd", "....~~.~...~.~.~"},   /* held notes sliding up and down */
    {"MENTAL",  "aBaMabNaaBaMakbM", "...~...~...~..~."},   /* every sixteenth, the flat second grinding */
    {"DOTTED",  "a-.M-.a-.k-.a-.m", "................"},   /* (PULSE's second) a note every dotted 8th: 3 against 4 */
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
};
static const char *const pad_name[PX_PADS] = {"RM", "RL", "TM", "HT", "CP", "AC", "GR", "", "", "", ""};   /* (the strip: two letters) */
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
    a->base = tekno.acid_root = (uint8_t)(knob[P_KICK][0] + 12);
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
static void load_acid(unsigned p)
{
    const char *n = acid_lines[p].notes, *sl = acid_lines[p].slides;
    for (unsigned i = 0; i < TEKNO_STEPS; ++i) {
        char c = n[i];
        uint16_t w = 0;
        if (c >= 'A' && c <= 'X') w = (uint16_t)(TEKNO_ACID_NOTE | TEKNO_ACID_ACCENT | (c - 'A'));
        else if (c >= 'a' && c <= 'x') w = (uint16_t)(TEKNO_ACID_NOTE | (c - 'a'));
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
        t->clap_damp = (int32_t)(0.7f * 32768.0f);
    }
    t->clap_gap = (uint32_t)((4.0f + 0.12f * (float)k[1]) * 44.1f);                       /* 4..16 ms */
    t->clap_tail_k = per_sample(15.0f + 1.5f * (float)k[2]);                              /* 15..165 ms */
    t->clap_level = k[3] * 320;
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
            int x = 2 + (int)s * 5, y = 10 + (int)lane * 6;
            unsigned color = on ? lane_color[lane][(code & 3U) == 2U] : PX_DUSK;
            if (code & 4U) px_rect(x, y, 4, 4, color); else px_rect(x, y + 2, 4, 2, color);
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
    if (shown_lane == LANES) {
        strcpy(line, "ACID "); strcat(line, acid_lines[acid_line].name);
        if (acid_line != acid_line_playing && blink) line[0] = 0;
        px_print(line, 2, 40, PX_GREEN);
    } else {
        strcpy(line, lane_name[shown_lane]); strcat(line, " "); strcat(line, patterns[shown_lane][pattern[shown_lane]].name);
        if (pattern[shown_lane] != pattern_playing[shown_lane] && blink) line[0] = 0;
        px_print(line, 2, 40, lane_color[shown_lane][0]);
    }
    if (tekno.vow_mix) {   /* VOWEL on: the vowel it is on now, large-ish, right */
        int at = (int)tekno_vowel_at;
        px_print_scaled(vowel_name(at * 100 / ((TEKNO_VOWELS - 1) * 256)), 70, 41, 2, PX_PINK);
    }
    /* the phaser's sweep, and the eight bars */
    int w = (int)((uint32_t)tekno_lfo * 62U >> 16);   /* (short of the vowel's corner) */
    px_rect(2, 47, 64, 2, PX_DUSK);
    px_rect(2 + w, 46, 2, 4, PX_PINK);
    if (tekno.fl_mix) px_rect(2 + (int)((uint32_t)tekno_flfo * 62U >> 16), 46, 2, 4, PX_WHITE);
    if ((tekno.want & TEKNO_GREN) && knob[P_MOVE][0]) px_rect(2 + (int)((uint32_t)tekno_glfo * 62U >> 16), 46, 2, 4, PX_RED);
    if (knob[P_SWEEP][0]) px_rect(2 + (int)((uint32_t)tekno_alfo * 62U >> 16), 46, 2, 4, PX_GREEN);
    for (unsigned b = 0; b < 8; ++b) px_rect(2 + (int)b * 5, 52, 4, 3, cr78_running && b == tekno_bar ? PX_WHITE : PX_DUSK);
    strcpy(line, ""); px_number((int)groove_tempo_current(), line); strcat(line, " BPM");
    px_print(line, 46, 51, PX_SLATE);
    px_flash_draw(now);
}
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
            build_vowels();
            init_acid(); load_acid(0); acid_line_playing = 0;
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
        else if (bit & (TEKNO_KICK | TEKNO_ROLL)) want = (uint8_t)((want & ~(TEKNO_KICK | TEKNO_ROLL)) | bit);   /* one kick at a time */
        else want = (uint8_t)(want | bit);
        tekno.want = want;
        tekno.engaged = want != 0;
        if (want & bit) start_if_stopped();
        px_flash(layer_name[i], now, 400U);
        changed = 1;
    }
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
        if (acid_line == p && acid_lines[p + PATTERNS].name) p += PATTERNS;   /* pressed again: its second */
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
            int step = (hi[page][i] - lo[page][i]) > 20 ? 2 : 1;
            knob[page][i] = (int16_t)clamp(knob[page][i] + k[i] * step, lo[page][i], hi[page][i]);
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
