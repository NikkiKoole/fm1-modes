/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* LOOP DECK: banks of loops that always fit together, toggled live.
 *
 *   white keys 1-4    BEAT: kick, snare, hats, percussion; they stack
 *   white keys 5-8    BASS      four lines; one at a time (pressing the one playing stops it)
 *   white keys 9-12   CHORDS    four parts; one at a time
 *   white keys 13-16  MELODY    four parts; one at a time
 *   black keys        the eleven effect pads, on the whole mix, while held
 *   PRESETS           the bank: HOUSE, HIP HOP, DISCO (its tempo, swing, progression, sounds)
 *   KNOB 1-4          FILTER (left low pass, right high pass), PUMP, SWING, TEMPO
 *   PLAY              play / stop (the first loop switched on starts it)
 *   HOME              back
 * Everything changes on the next bar (its key blinks until then).
 *
 * A bank is eight bars: a PROGRESSION (a chord a bar, usually twice almost the same four bars) that
 * every part follows, so any combination fits. The bass and melody are RIFFS in Playtime's format
 * (vector-sketch/experiments/playtime/src/music-riffs.lua): a one-bar PHRASE in degrees over each
 * bar's root, VARIATIONS laid over it (a space keeps the phrase's step), and a FORM of eight letters
 * ('AAABAAAC': the same bar three times, a turn into the fifth bar, a bigger one back to the top).
 * Written in the minor pentatonic, a riff follows the chord: over a major chord its notes move to the
 * same places on the major pentatonic. The chord parts are rhythms that strike the bar's chord.
 * They play on the GROOVEBOX's four tracks, which the deck borrows (gbx_deck_on) and gives back when
 * the GROOVEBOX opens. */
#include "deck.h"
#include "gbx.h"
#include "pixel.h"
#include "cr78.h"
#include "fx.h"
#include "instrument.h"
#include "mysynth.h"
#include <string.h>

enum { V_KICK = 0, V_SNARE = 1, V_RIM = 2, V_HHC = 3, V_HHO = 4, V_COW = 7, V_TAMB = 8, V_MAR = 9, V_CONGA = 14 };
enum { PARTS = 4, LOOPS = 4, ROWS = 3, BARS = GBX_DECK_BARS };
struct beat { const char *name; struct { uint8_t voice; const char *steps; } row[ROWS]; };   /* x a hit, X accented */
/* a riff: degrees R 2 m 3 4 b 5 6 7 8 (the octave), L (the fifth below), A (a half step under the next
 * bar's root), N (the next bar's root, early); '-' holds, '.' rests. A phrase shorter than 16 repeats. */
struct riff { const char *name, *phrase, *var[3], *form; };   /* var: B C D */
struct rhythm { const char *name, *steps; };                     /* X strikes the bar's chord, '-' holds */
struct bank {
    const char *name;
    uint8_t tempo, swing;                 /* BPM; swing 50..66 */
    uint8_t keys_patch, lead_patch;       /* MY SYNTH patches: 0 INIT 1 PAD 2 BRASS 3 PLUCK 4 BASS 5 LEAD */
    int16_t bass_sound[2][4];             /* {CUT RES DEC SUB}, {WAVE DRV ATK PTCH} */
    const char *progression;              /* eight chords, one a bar */
    struct beat beat[LOOPS];
    struct riff bass[LOOPS];
    struct rhythm chords[LOOPS];
    struct riff melody[LOOPS];
};

static const struct bank banks[] = {
    {"HOUSE", 124, 52, 3, 5, {{45, 40, 30, 70}, {0, 0, 1, 0}},
     "Am7 Am7 Fmaj7 G Am7 Am7 Fmaj7 Em7",
     {{"KICK", {{V_KICK, "X...x...X...x..."}}},
      {"CLAP", {{V_SNARE, "....x.......x..."}, {V_RIM, "....x.......x..x"}}},
      {"HATS", {{V_HHO, "..x...x...x...x."}, {V_HHC, "x.x.x.x.x.x.x.x."}}},
      {"PERC", {{V_CONGA, "..x..x....x..x.."}, {V_MAR, ".x.x.x.x.x.x.x.x"}}}},
     {{"POCKET", "R..R..7.R.....5.", {"            8.7A", "        8.7.5.mA"}, "AAABAAAC"},
      {"OFFBEAT", "..R...R...R...R.", {"          8...7A", "      8...7.5.mA"}, "AAABAAAC"},
      {"OCTAVES", "R.8.", {"            7.5A", "        8.7.5.mA"}, "AAABAAAC"},
      {"FUNK", "R.RR..7.R.m.4.5.", {"        8.7.5.4A", "    8.8.7.5.4.mA"}, "AAABAAAC"}},
     {{"STABS", "..X...X...X...X."}, {"PADS", "X---------------"}, {"CHOP", "..X..X....X..X.."}, {"PUSH", "X..X..X...X....."}},
     {{"HOOK", "5..5..4.m.....R.", {"5..5..7.8.......", "5..5..4.m.R....."}, "ABABABAC"},
      {"ARP", "Rm58", {"Rm587585", "8758m5mR"}, "AAABAAAC"},
      {"CALL", "8..7..5---......", {"4..5..m---......", "4..5..R---......"}, "ABABABAC"},
      {"STRINGS", "5---------------", {"m---------------", "4-------m-------"}, "ABABABAC"}}},
    {"HIP HOP", 90, 60, 1, 3, {{30, 20, 60, 70}, {0, 10, 2, 0}},
     "Cm7 Cm7 Abmaj7 Gm7 Cm7 Cm7 Abmaj7 G7",
     {{"KICK", {{V_KICK, "x......x..x....."}}},
      {"SNARE", {{V_SNARE, "....x.......x..."}}},
      {"HATS", {{V_HHC, "x.x.x.x.x.x.x.xx"}}},
      {"PERC", {{V_RIM, "...x......x....."}, {V_TAMB, "..x...x...x...x."}}}},
     {{"LOW", "R-------L-----..", {"        L---m-A.", "        5---4-A."}, "AAABAAAC"},
      {"CALL", "R..8..R.....5.7.", {"        7.5.4.mA", "    8.7.5.4.m.RA"}, "AAABAAAC"},
      {"MOTOWN", "R...5...8...5.4.", {"        7...5.mA", "        8.7.5.mA"}, "AAABAAAC"},
      {"LOW HIGH", "R...L...R..m4...", {"        R..m5.N.", "        8..75.N."}, "AAABAAAC"}},
     {{"SOUL", "X---------------"}, {"STABS", "X..X......X....."}, {"DRAG", "....X---....X---"}, {"HALF", "X-------........"}},
     {{"HOOK", "5..m..R.........", {"5..m..4..m.R....", "7..5..4..m.R...."}, "ABABABAC"},
      {"LINE", "8.7.5...m.4.5...", {"8.7.5...m.4.R...", "8.7.5.4.m.R....."}, "ABABABAC"},
      {"RISE", "R---m---4---5---", {"7---5---4---m---", "8---7---5---4---"}, "ABABABAC"},
      {"LONG", "5---------------", {"m---------------", "4-------m-------"}, "ABABABAC"}}},
    {"DISCO", 118, 50, 2, 3, {{70, 30, 25, 40}, {1, 0, 0, 0}},
     "Em7 A7 Em7 A7 Em7 A7 Cmaj7 B7",
     {{"KICK", {{V_KICK, "x...x...x...x..."}}},
      {"CLAP", {{V_SNARE, "....x.......x..."}}},
      {"HATS", {{V_HHO, "..x...x...x...x."}, {V_HHC, "x...x...x...x..."}}},
      {"PERC", {{V_TAMB, "x.xxx.xxx.xxx.xx"}, {V_COW, "......x.......x."}}}},
     {{"OCTAVES", "R.8.", {"            7.5A", "        7.5.4.mA"}, "AAABAAAC"},
      {"LADDER", "R.m.4.5.7.8.7.5.", {"        8.7.5.mA", "        8.8.7.5A"}, "AAABAAAC"},
      {"FUNK", "R.RR..7.R.m.4.5.", {"        8.7.5.4A", "    8.8.7.5.4.mA"}, "AAABAAAC"},
      {"PUSH", "R..8...R......N.", {"R..8...R..8.7.N.", "R..8.7.5.4.m.RN."}, "AAABAAAC"}},
     {{"STABS", "..X...X...X...X."}, {"CHIC", "..X..X....X..X.."}, {"STRINGS", "X---------------"}, {"CHOP", "X.X..X..X......."}},
     {{"HOOK", "5..5..7.8.......", {"8.7.5.4.m.......", "8.7.5.4.m.R....."}, "ABABABAC"},
      {"STRINGS", "8-------7-------", {"5-------4-------", "5-------m-------"}, "ABABABAC"},
      {"ARP", "R587", {"R58757m5", "8758m5mR"}, "AAABAAAC"},
      {"RIFF", "R.m.4.m.R.7.R.m.", {"R.m.4.5.7.5.4.m.", "8.7.5.4.m.R.m.R."}, "ABABABAC"}}},
};
enum { BANKS = sizeof banks / sizeof banks[0] };

/* ---- reading the loops ---- */
static int pitch_class(const char **p)
{
    static const int8_t base[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    char c = **p;
    if (c < 'A' || c > 'G') return -1;
    int pc = base[c - 'A']; ++*p;
    if (**p == '#') { ++pc; ++*p; } else if (**p == 'b') { --pc; ++*p; }
    return (pc + 12) % 12;
}
/* the progression: each bar's root (pitch class), kind (minor / major / dominant) and chord */
enum { K_MINOR, K_MAJOR, K_DOM };
static struct { uint8_t root, kind, note[GBX_CHORD]; } bar_chord[BARS];
static void read_progression(const char *s)
{
    for (unsigned b = 0; b < BARS; ++b) {
        while (*s == ' ') ++s;
        int pc = pitch_class(&s);
        if (pc < 0) pc = 9;
        char q[8]; unsigned n = 0;
        while (*s && *s != ' ' && n < 7) q[n++] = *s++;
        q[n] = 0;
        static const int8_t major[4] = {0, 4, 7, -1}, minor[4] = {0, 3, 7, -1}, dom7[4] = {0, 4, 7, 10},
                            min7[4] = {0, 3, 7, 10}, maj7[4] = {0, 4, 7, 11};
        const int8_t *iv = major; unsigned kind = K_MAJOR;
        if (!strcmp(q, "m")) { iv = minor; kind = K_MINOR; }
        else if (!strcmp(q, "m7")) { iv = min7; kind = K_MINOR; }
        else if (!strcmp(q, "7")) { iv = dom7; kind = K_DOM; }
        else if (!strcmp(q, "maj7")) iv = maj7;
        int root = 48 + pc; if (root < 52) root += 12;   /* the middle: E3 .. D#4 */
        bar_chord[b].root = (uint8_t)pc; bar_chord[b].kind = (uint8_t)kind;
        for (unsigned k = 0; k < GBX_CHORD; ++k) bar_chord[b].note[k] = iv[k] < 0 ? 0 : (uint8_t)(root + iv[k]);
    }
}
/* a degree letter over a chord of `kind`: semitones over its root (-99: not a note) */
static int degree(char c, unsigned kind)
{
    static const int8_t pent[3][5] = {{0, 3, 5, 7, 10}, {0, 2, 4, 7, 9}, {0, 2, 4, 7, 10}};
    switch (c) {   /* the minor pentatonic's five move to the chord's own pentatonic */
    case 'R': return 0;
    case 'm': return pent[kind][1];
    case '4': return pent[kind][2];
    case '5': return pent[kind][3];
    case '7': return pent[kind][4];
    case '8': return 12;
    case 'L': return -5;
    case '2': return 2;
    case '3': return 4;
    case 'b': return 6;
    case '6': return 9;
    default: return -99;
    }
}
/* bar b's line of a riff: the phrase (repeated to 16) with the form's variation laid over it */
static void riff_bar(const struct riff *r, unsigned b, char line[17])
{
    unsigned n = (unsigned)strlen(r->phrase);
    for (unsigned s = 0; s < 16; ++s) line[s] = n ? r->phrase[s % n] : '.';
    line[16] = 0;
    unsigned flen = r->form ? (unsigned)strlen(r->form) : 0;
    char letter = flen ? r->form[b % flen] : 'A';
    if (letter < 'B' || letter > 'D' || !r->var[letter - 'B']) return;
    const char *v = r->var[letter - 'B'];
    unsigned vn = (unsigned)strlen(v);
    for (unsigned s = 0; s < 16 && s < vn; ++s) if (v[s] != ' ') line[s] = v[s];
    if (vn < 16 && vn && v[0] != ' ') for (unsigned s = vn; s < 16; ++s) line[s] = v[s % vn];   /* a short variation repeats too */
}
/* a riff into the eight bars' records; `low` the register's lowest root (E1 for the bass, E4 the melody) */
static void compile_riff(const struct riff *r, unsigned part, int low)
{
    for (unsigned b = 0; b < BARS; ++b) {
        char line[17];
        riff_bar(r, b, line);
        unsigned next = (b + 1U) % BARS;
        int root = low + (int)((bar_chord[b].root + 12U - (unsigned)(low % 12)) % 12U);
        int next_root = low + (int)((bar_chord[next].root + 12U - (unsigned)(low % 12)) % 12U);
        struct gbx_line_steps *l = part == 1 ? &gbx_deck[b].bass : &gbx_deck[b].lead;
        for (unsigned s = 0; s < 16; ++s) {
            char c = line[s];
            int note;
            if (c == 'A') note = next_root - 1;
            else if (c == 'N') note = next_root;
            else { int d = degree(c, bar_chord[b].kind); if (d == -99) continue; note = root + d; }
            unsigned held = 1;   /* itself and the holds after it (a hold stops at the bar line) */
            for (unsigned j = s + 1U; j < 16U && line[j] == '-'; ++j) ++held;
            unsigned gate = held > 1 ? held * 16U - 2U : part == 1 ? 12U : 10U;
            uint16_t bit = (uint16_t)(1U << s);
            l->note[s] = (uint8_t)note; l->gate[s] = (uint8_t)(gate > 255U ? 255U : gate); l->vel[s] = 100; l->on |= bit;
        }
    }
}
static void compile_rhythm(const struct rhythm *r)
{
    unsigned n = (unsigned)strlen(r->steps);
    for (unsigned b = 0; b < BARS; ++b)
        for (unsigned s = 0; s < 16; ++s) {
            if (!n || r->steps[(b * 16U + s) % n] != 'X') continue;
            unsigned held = 1;
            for (unsigned j = s + 1U; j < 16U && r->steps[(b * 16U + j) % n] == '-'; ++j) ++held;
            unsigned gate = held > 1 ? held * 16U - 2U : 10U;
            struct gbx_pattern *p = &gbx_deck[b];
            for (unsigned k = 0; k < GBX_CHORD; ++k) p->keys_note[s][k] = bar_chord[b].note[k];
            p->keys_gate[s] = (uint8_t)(gate > 255U ? 255U : gate); p->keys_vel[s] = 92; p->keys_on |= (uint16_t)(1U << s);
        }
}

/* ---- the deck ---- */
static unsigned active, bank, full_redraw;
static uint16_t want, playing;          /* bit 4 * part + loop */
static uint8_t last_bar = 255;
static uint32_t prev_buttons, prev_keys, last_ms, led_state, pads;
static int16_t knob[4] = {0, 0, 50, 0};   /* FILTER -100..100, PUMP 0..100, SWING 50..66, TEMPO (shown) */
static uint8_t saved_synth[MYSYNTH_STATE_BYTES];
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[PX_PADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const char *const pad_name[PX_PADS] = {"R16", "R32", "HLF", "REV", "LPF", "HPF", "CRS", "GAT", "DRV", "PMP", "SWP"};
static const char *const part_name[PARTS] = {"BEAT", "BASS", "CHRD", "MEL"};
static const uint8_t part_color[PARTS] = {PX_EMBER, PX_GREEN, PX_CYAN, PX_PINK};
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* what is switched on, into the eight bars the audio side plays from the next bar */
static void compile(void)
{
    const struct bank *k = &banks[bank];
    memset(gbx_deck, 0, sizeof gbx_deck);
    read_progression(k->progression);
    for (unsigned j = 0; j < LOOPS; ++j) {
        if (!((want >> j) & 1U)) continue;
        for (unsigned r = 0; r < ROWS; ++r) {
            const char *row = k->beat[j].row[r].steps;
            if (!row) continue;
            unsigned len = (unsigned)strlen(row), v = k->beat[j].row[r].voice;
            for (unsigned b = 0; b < BARS; ++b)
                for (unsigned s = 0; s < 16; ++s) {
                    char c = row[(b * 16U + s) % len];
                    if (c == 'x' || c == 'X') gbx_deck[b].grid[v] |= (uint16_t)(1U << s);
                    if (c == 'X') gbx_deck[b].accent |= (uint16_t)(1U << s);
                }
        }
    }
    for (unsigned j = 0; j < LOOPS; ++j) {
        if ((want >> (4U + j)) & 1U) compile_riff(&k->bass[j], 1, 28);     /* roots E1 .. D#2 */
        if ((want >> (8U + j)) & 1U) compile_rhythm(&k->chords[j]);
        if ((want >> (12U + j)) & 1U) compile_riff(&k->melody[j], 3, 64);  /* roots E4 .. D#5 */
    }
    gbx_deck_dirty = 1;
}
/* the bank's sound: tempo, swing, the bass, the synths' patches */
static void bank_sound(void)
{
    const struct bank *k = &banks[bank];
    gbx_bass_sound(k->bass_sound);
    mysynth_patch(k->keys_patch); mysynth_lead_patch(k->lead_patch);
    groove_tempo_edit((int)k->tempo - (int)groove_tempo_current());
    knob[2] = k->swing;
}
/* the mix: levels, swing, the filter and pump knobs, the pads */
static void apply(void)
{
    struct fx_perf *m = &fx_perf;
    int32_t on = 0;
    if (knob[0] < -2) { m->lpf_f = gbx_svf(150.0f * gbx_exp2((float)(100 + knob[0]) * 0.069f)); m->lpf_damp = 34000; on |= PERF_LPF; }
    else if (knob[0] > 2) { m->hpf_f = gbx_svf(20.0f * gbx_exp2((float)knob[0] * 0.08f)); m->hpf_damp = 34000; on |= PERF_HPF; }
    if (knob[1]) { m->pump_amount = (int32_t)(knob[1] * 29491 / 100); m->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    m->on = on | gbx_throw_pads(m, pads);
    for (unsigned b = 0; b < FX_BUSES; ++b) fx_track_perf[b].on = 0;
    unsigned perf = 0;
    if (pads & 1U) perf |= CR78_PERF_ROLL16;
    if (pads & 2U) perf |= CR78_PERF_ROLL32;
    if (pads & 4U) perf |= CR78_PERF_HALF;
    if (pads & 8U) perf |= CR78_PERF_REV;
    cr78.roll_len = 1; cr78.roll_fade = 1; cr78.perf = (uint8_t)perf;
    cr78.swing_q16 = (int32_t)((knob[2] - 50) * 4 * 65536 / 100);
    cr78.level = 8800; cr78.muted = 0; cr78.voice_mute = 0;
    gbx_bass.level = 13000; gbx_bass.muted = 0; gbx_bass.armed = 1;
    gbx_keys.muted = 0; gbx_keys.armed = 1; gbx_keys.strum = 0; mysynth_params.duck = 18000;
    gbx_lead.muted = 0; gbx_lead.armed = 1; mysynth_lead_params.duck = 22500;
}
void deck_release(void)
{
    if (!gbx_deck_on) return;
    gbx_deck_on = 0;
    mysynth_state_set(saved_synth);
}
unsigned deck_active(void) { return active; }

/* ---- the screen ---- */
static const char *loop_name(const struct bank *k, unsigned part, unsigned j)
{
    return part == 0 ? k->beat[j].name : part == 1 ? k->bass[j].name : part == 2 ? k->chords[j].name : k->melody[j].name;
}
static void draw(uint32_t now)
{
    const struct bank *k = &banks[bank];
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    static const struct px_page page = {"MIX", {"FLTR", "PUMP", "SWNG", "TMPO"}};
    if (knob[0] < -2) { strcpy(v[0], "LP"); px_number(-knob[0], v[0] + 2); }
    else if (knob[0] > 2) { strcpy(v[0], "HP"); px_number(knob[0], v[0] + 2); }
    else strcpy(v[0], "OFF");
    if (knob[1]) px_number(knob[1], v[1]); else strcpy(v[1], "OFF");
    px_number(knob[2], v[2]); px_number((int)groove_tempo_current(), v[3]);
    px_cls(PX_NIGHT);
    px_chrome_pads("DECK", &page, 1, 0, value, pad_name, pads);
    px_print("DECK", 2, 1, PX_GOLD);
    px_print(k->name, 2, 8, PX_WHITE);
    /* four parts of four loops, as the keys lie: on filled, waiting to change blinking */
    unsigned blink = (now / 140U) & 1U;
    for (unsigned part = 0; part < PARTS; ++part) {
        int x0 = 1 + (int)part * 20;
        px_print(part_name[part], x0, 17, part_color[part]);
        for (unsigned j = 0; j < LOOPS; ++j) {
            unsigned bit = 4U * part + j, on = (playing >> bit) & 1U, will = (want >> bit) & 1U;
            int x = x0 + (int)j * 5, y = 24;
            if (on != will && blink) on = !on;
            if (on) px_rect(x, y, 4, 16, part_color[part]);
            else { px_rect(x, y, 4, 1, PX_DUSK); px_rect(x, y + 15, 4, 1, PX_DUSK); px_rect(x, y, 1, 16, PX_DUSK); px_rect(x + 3, y, 1, 16, PX_DUSK); }
        }
    }
    /* the loop just touched, by name; the eight bars and the beat */
    static unsigned shown = 255;
    for (unsigned b = 0; b < 16; ++b) if (((want ^ playing) >> b) & 1U) shown = b;
    if (shown < 16) {
        char line[24]; strcpy(line, part_name[shown / 4U]); strcat(line, " "); strcat(line, loop_name(k, shown / 4U, shown % 4U));
        px_print(line, 2, 45, part_color[shown / 4U]);
    }
    if (cr78_running) {
        for (unsigned b = 0; b < BARS; ++b) px_rect(2 + (int)b * 5, 54, 4, 3, b == gbx_deck_bar ? PX_WHITE : PX_DUSK);
        for (unsigned q = 0; q < 4; ++q) px_rect(48 + (int)q * 5, 54, 4, 3, q == cr78_playhead / 4U ? PX_GOLD : PX_DUSK);
    }
    px_flash_draw(now);
}
static void leds(uint32_t now)
{
    uint32_t show = playing;
    if ((now / 140U) & 1U) show ^= want ^ playing;   /* waiting: blinking */
    uint32_t all = show | (pads << 16);
    for (unsigned s = 0; s < 16; ++s)
        if (((all ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (all >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((all ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (all >> (16 + i)) & 1U);
    led_state = all;
}

/* ---- the panel ---- */
int deck_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons, down = c->keys & ~prev_keys;
    unsigned home = px_mode_toggle(pressed, PX_MODE_DECK, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!home) return 0;
        cr78_kit_ready(); mysynth_ready(); mysynth_lead_ready();
        if (!gbx_deck_on) {   /* borrow the tracks: the GROOVEBOX's pattern into its slot, MY SYNTH's sounds aside */
            gbx_pattern_take(&gbx_patterns[gbx_pattern_now]);
            mysynth_state_get(saved_synth);
            bank_sound();
            compile();
            gbx_deck_on = 1;
        }
        apply();
        px_leds_off(); led_state = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(banks[bank].name, now, 700U);
        return 1;
    }
    if (home) { pads = 0; apply(); px_leds_off(); active = 0; return 2; }   /* (it keeps playing) */
    unsigned changed = 0;
    /* what was waiting plays from the next bar (stopped: as soon as the audio side has it) */
    if ((cr78_running ? gbx_deck_bar != last_bar : !gbx_deck_dirty) && playing != want) { playing = want; changed = 1; }
    last_bar = gbx_deck_bar;
    /* white keys: the loops */
    for (unsigned s = 0; s < 16; ++s) {
        if (!((down >> white_keys[s]) & 1U)) continue;
        unsigned part = s / 4U;
        uint16_t bit = (uint16_t)(1U << s), group = (uint16_t)(15U << (4U * part));
        if (part == 0 || (want & bit)) want ^= bit;            /* BEAT stacks; the one playing stops */
        else want = (uint16_t)((want & ~group) | bit);         /* the rest: one at a time */
        compile();
        if (!cr78_running && (want & bit)) {   /* the first loop starts it */
            struct groove_view view = {0}; groove_get_view(0, 0, &view);
            if (!view.playing) { const uint8_t command = 0xfb; (void)groove_midi_local(&command, 1); }
        }
        changed = 1;
    }
    /* black keys: the pads, while held */
    uint32_t held = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) {
        if ((c->keys >> black_keys[i]) & 1U) held |= 1U << i;
        if ((down >> black_keys[i]) & 1U) px_flash(pad_name[i], now, 400U);
    }
    if (held != pads) { pads = held; apply(); changed = 1; }
    /* PRESETS: the bank (its loops change on the next bar) */
    if (c->encoders[6]) {
        bank = (unsigned)(((int)bank + c->encoders[6] + 8 * BANKS) % BANKS);
        bank_sound(); compile(); apply();
        px_flash(banks[bank].name, now, 800U);
        changed = 1;
    }
    /* KNOB 1-4: FILTER PUMP SWING TEMPO */
    const int16_t *k = c->encoders + 2;
    if (k[0] || k[1] || k[2] || k[3]) {
        static char text[16];
        if (k[0]) { int x = clamp(knob[0] + k[0] * 4, -100, 100); knob[0] = (int16_t)(x > -3 && x < 3 ? 0 : x); strcpy(text, "FILTER "); }
        if (k[1]) { knob[1] = (int16_t)clamp(knob[1] + k[1] * 5, 0, 100); strcpy(text, "PUMP "); px_number(knob[1], text + 5); }
        if (k[2]) { knob[2] = (int16_t)clamp(knob[2] + k[2] * 2, 50, 66); strcpy(text, "SWING "); px_number(knob[2], text + 6); }
        if (k[3]) { groove_tempo_edit(k[3]); strcpy(text, "TEMPO "); px_number((int)groove_tempo_current(), text + 6); }
        if (k[0]) { if (knob[0] < -2) { strcat(text, "LP"); px_number(-knob[0], text + strlen(text)); } else if (knob[0] > 2) { strcat(text, "HP"); px_number(knob[0], text + strlen(text)); } else strcat(text, "OFF"); }
        apply();
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
