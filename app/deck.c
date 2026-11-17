/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* LOOP DECK: banks of loops that always fit together, toggled live.
 *
 *   white keys 1-4    BEAT: kick, snare, hats, percussion; they stack
 *   white keys 5-8    BASS      four lines; one at a time (pressing the one playing stops it)
 *   white keys 9-12   CHORDS    four parts; one at a time
 *   white keys 13-16  MELODY    four parts; one at a time
 *   black keys        the eleven effect pads, on the whole mix, while held
 *   PRESETS           the bank: HOUSE, HIP HOP, DISCO (its tempo, swing, sounds)
 *   KNOB 1-4          FILTER (left low pass, right high pass), PUMP, SWING, TEMPO
 *   PLAY              play / stop (the first loop switched on starts it)
 *   HOME              back
 * Everything changes on the next bar (its key blinks until then). The loops are text, up to four bars;
 * they play on the GROOVEBOX's four tracks, which the deck borrows (gbx_deck_on) and gives back when
 * the GROOVEBOX opens. */
#include "deck.h"
#include "gbx.h"
#include "pixel.h"
#include "cr78.h"
#include "fx.h"
#include "instrument.h"
#include "mysynth.h"
#include <string.h>

enum { V_KICK = 0, V_SNARE = 1, V_RIM = 2, V_HHC = 3, V_HHO = 4, V_CYM = 5, V_COW = 7, V_TAMB = 8, V_MAR = 9,
       V_CLV = 10, V_BONH = 12, V_BONL = 13, V_CONGA = 14 };
enum { PARTS = 4, LOOPS = 4, ROWS = 3 };
struct beat { const char *name; struct { uint8_t voice; const char *steps; } row[ROWS]; };   /* x a hit, X accented */
struct bank {
    const char *name;
    uint8_t tempo, swing;                 /* BPM; swing 50..66 */
    uint8_t keys_patch, lead_patch;       /* MY SYNTH patches: 0 INIT 1 PAD 2 BRASS 3 PLUCK 4 BASS 5 LEAD */
    int16_t bass_sound[2][4];             /* {CUT RES DEC SUB}, {WAVE DRV ATK PTCH} */
    struct beat beat[LOOPS];
    /* tokens, 16 a bar, up to four bars: a note (A2, C#3, Eb4) or chord (Am, Am7, C, Cmaj7, G7, Bdim);
     * "." rest, "-" hold the one before; "~" before a note slides into it */
    const char *bass[LOOPS], *chords[LOOPS], *melody[LOOPS];
    const char *bass_name[LOOPS], *chords_name[LOOPS], *melody_name[LOOPS];
};

static const struct bank banks[] = {
    {"HOUSE", 124, 52, 3, 5, {{45, 40, 30, 70}, {0, 0, 1, 0}},
     {{"KICK", {{V_KICK, "X...x...X...x..."}}},
      {"CLAP", {{V_SNARE, "....x.......x..."}, {V_RIM, "....x.......x..x"}}},
      {"HATS", {{V_HHO, "..x...x...x...x."}, {V_HHC, "x.x.x.x.x.x.x.x."}}},
      {"PERC", {{V_CONGA, "..x..x....x..x.."}, {V_MAR, "xxxxxxxxxxxxxxxx"}}}},
     {". . A2 . . . A2 . . . A2 . . . A2 .",
      "A1 . A2 . A1 . A2 . A1 . A2 . A1 . A2 .",
      "A2 . . A2 . . A2 . G2 . . G2 . . C3 . | A2 . . A2 . . A2 . E2 . . E2 . . G2 .",
      ". . A2 . . . A2 . . . A2 . . . A2 . | . . F2 . . . F2 . . . G2 . . . G2 ."},
     {". . Am7 . . . Am7 . . . Am7 . . . Am7 .",
      "Am7 - - - - - - - - - - - - - - - | Fmaj7 - - - - - - - - - - - - - - -",
      ". . Am7 . . Am7 . . . . Am7 . . Am7 . . | . . Dm7 . . Dm7 . . . . Dm7 . . Dm7 . .",
      "Am7 - - - - - - - - - - - - - - - | Fmaj7 - - - - - - - - - - - - - - - | C - - - - - - - - - - - - - - - | G - - - - - - - - - - - - - - -"},
     {"E5 . . E5 . . D5 . C5 . . A4 . . . .",
      "A4 C5 E5 A5 E5 C5 A4 C5 E5 A5 E5 C5 A4 C5 E5 C5",
      "A5 . . G5 . . E5 - - - . . . . . . | C5 . . D5 . . E5 - - - . . . . . .",
      "E5 - - - - - - - D5 - - - - - - - | C5 - - - - - - - B4 - - - - - - -"},
     {"OFFBEAT", "ROLLING", "BOUNCE", "LIFT"}, {"STABS", "PADS", "CHOP", "4 CHORDS"}, {"HOOK", "ARP", "CALL", "STRINGS"}},
    {"HIP HOP", 90, 60, 1, 3, {{30, 20, 60, 70}, {0, 10, 2, 0}},
     {{"KICK", {{V_KICK, "x......x..x....."}}},
      {"SNARE", {{V_SNARE, "....x.......x..."}}},
      {"HATS", {{V_HHC, "x.x.x.x.x.x.x.xx"}}},
      {"PERC", {{V_RIM, "...x......x....."}, {V_TAMB, "..x...x...x...x."}}}},
     {"C2 . . . . . . C2 . . Eb2 . . . . .",
      "C2 . . . . . . . G1 . . . . . Bb1 .",
      "C2 . . . . . . C2 . . Eb2 . . . . . | F1 . . . . . . F1 . . Ab1 . . . G1 .",
      "C2 . . C3 . . C2 . . . Bb1 . G1 . . ."},
     {"Cm7 - - - - - - - - - - - - - - - | Fm7 - - - - - - - - - - - - - - -",
      "Abmaj7 - - - - - - - - - - - - - - - | Gm7 - - - - - - - - - - - - - - -",
      "Cm7 . . Cm7 . . . . . . Cm7 . . . . .",
      "Cm7 - - - - - - - - - - - - - - - | Abmaj7 - - - - - - - - - - - - - - - | Fm7 - - - - - - - - - - - - - - - | G7 - - - - - - - - - - - - - - -"},
     {"G4 . . Eb4 . . C4 . . . . . . . . .",
      "C5 . Bb4 . G4 . . . Eb4 . F4 . G4 . . .",
      "G4 - - - Bb4 - - - C5 - - - . . . . | Eb5 - - - D5 - - - C5 - - - . . . .",
      "Eb5 - - - - - - - D5 - - - - - - - | C5 - - - - - - - Bb4 - - - - - - -"},
     {"BOOM", "LOW", "WALK", "JUMP"}, {"SOUL", "WARM", "STABS", "4 CHORDS"}, {"HOOK", "LINE", "RISE", "LONG"}},
    {"DISCO", 118, 50, 2, 3, {{70, 30, 25, 40}, {1, 0, 0, 0}},
     {{"KICK", {{V_KICK, "x...x...x...x..."}}},
      {"CLAP", {{V_SNARE, "....x.......x..."}}},
      {"HATS", {{V_HHO, "..x...x...x...x."}, {V_HHC, "x...x...x...x..."}}},
      {"PERC", {{V_TAMB, "x.xxx.xxx.xxx.xx"}, {V_COW, "......x.......x."}}}},
     {"E2 . E3 . E2 . E3 . E2 . E3 . E2 . E3 .",
      "E2 . E3 . E2 . E3 . E2 . E3 . E2 . E3 . | A1 . A2 . A1 . A2 . A1 . A2 . A1 . A2 .",
      "E2 . . E2 . . G2 . A2 . . A2 . . B2 .",
      "E2 . G2 . A2 . B2 . D3 . B2 . A2 . G2 ."},
     {". . Em7 . . . Em7 . . . Em7 . . . Em7 .",
      ". . Em7 . . Em7 . . . . Em7 . . Em7 . . | . . A . . A . . . . A . . A . .",
      "Em7 - - - - - - - - - - - - - - - | A7 - - - - - - - - - - - - - - -",
      "Em7 . Em7 . . Em7 . . Em7 . . . . . . ."},
     {"B4 . . B4 . . D5 . E5 . . . . . . .",
      "G5 - - - - - - - F#5 - - - - - - - | E5 - - - - - - - D5 - - - - - - -",
      "E5 G5 B5 G5 E5 G5 B5 G5 E5 G5 B5 G5 E5 G5 B5 D6",
      "E5 . G5 . A5 . G5 . E5 . D5 . B4 . D5 ."},
     {"OCTAVES", "VAMP", "SYNCO", "WALK"}, {"STABS", "CHOP", "STRINGS", "CHIC"}, {"HOOK", "STRINGS", "ARP", "RIFF"}},
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
/* the next token of a loop: returns its length, 0 at the end; skips spaces and bar lines */
static unsigned next_token(const char **p, char *out)
{
    while (**p == ' ' || **p == '|') ++*p;
    unsigned n = 0;
    while (**p && **p != ' ' && **p != '|' && n < 11) out[n++] = *(*p)++;
    out[n] = 0;
    return n;
}
static unsigned tokens(const char *s, char tok[64][12])   /* up to four bars; returns how many */
{
    unsigned n = 0;
    while (n < 64 && next_token(&s, tok[n])) ++n;
    return n;
}
static int note_of(const char *t, unsigned *slide)
{
    *slide = 0;
    if (*t == '~') { *slide = 1; ++t; }
    int pc = pitch_class(&t);
    if (pc < 0 || *t < '0' || *t > '8') return -1;
    return 12 * (*t - '0' + 1) + pc;
}
static int chord_of(const char *t, uint8_t out[GBX_CHORD])
{
    int pc = pitch_class(&t);
    if (pc < 0) return 0;
    static const int8_t major[4] = {0, 4, 7, -1}, minor[4] = {0, 3, 7, -1}, dom7[4] = {0, 4, 7, 10}, min7[4] = {0, 3, 7, 10},
                        maj7[4] = {0, 4, 7, 11}, dim[4] = {0, 3, 6, -1};
    const int8_t *iv = major;
    if (!strcmp(t, "m")) iv = minor; else if (!strcmp(t, "7")) iv = dom7; else if (!strcmp(t, "m7")) iv = min7;
    else if (!strcmp(t, "maj7")) iv = maj7; else if (!strcmp(t, "dim")) iv = dim;
    int root = 48 + pc; if (root < 52) root += 12;   /* the middle: E3 .. D#4 */
    for (unsigned k = 0; k < GBX_CHORD; ++k) out[k] = iv[k] < 0 ? 0 : (uint8_t)(root + iv[k]);
    return 1;
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

/* a melodic loop into the four bars' records: notes / chords, held lengths, slides */
static void compile_line(const char *src, unsigned part)
{
    static char tok[64][12];
    unsigned n = tokens(src, tok), bars = n >= 64 ? 4U : n >= 32 ? 2U : 1U;
    if (n < 16) return;
    for (unsigned b = 0; b < GBX_DECK_BARS; ++b) {
        struct gbx_pattern *p = &gbx_deck[b];
        unsigned base = (b % bars) * 16U;
        for (unsigned s = 0; s < 16; ++s) {
            const char *t = tok[base + s];
            if (t[0] == '.' || t[0] == '-') continue;
            unsigned held = 1;   /* how many steps it sounds: itself and the holds after it */
            for (unsigned j = base + s + 1U; j < bars * 16U && tok[j][0] == '-'; ++j) ++held;
            unsigned gate = held > 1 ? held * 16U - 2U : 10U;
            if (gate > 255U) gate = 255U;
            uint16_t bit = (uint16_t)(1U << s);
            if (part == 2) {
                uint8_t ch[GBX_CHORD];
                if (!chord_of(t, ch)) continue;
                for (unsigned k = 0; k < GBX_CHORD; ++k) p->keys_note[s][k] = ch[k];
                p->keys_gate[s] = (uint8_t)gate; p->keys_vel[s] = 92; p->keys_on |= bit;
            } else {
                unsigned slide; int note = note_of(t, &slide);
                if (note < 0) continue;
                struct gbx_line_steps *l = part == 1 ? &p->bass : &p->lead;
                l->note[s] = (uint8_t)note; l->vel[s] = 100; l->on |= bit;
                l->gate[s] = (uint8_t)(part == 1 ? (held > 1 ? 16U : 12U) : gate);
                if (slide) l->slide |= bit;
            }
        }
    }
}
/* what is switched on, into the four bars the audio side plays from the next bar */
static void compile(void)
{
    const struct bank *k = &banks[bank];
    memset(gbx_deck, 0, sizeof gbx_deck);
    for (unsigned j = 0; j < LOOPS; ++j) {
        if (!((want >> j) & 1U)) continue;
        for (unsigned r = 0; r < ROWS; ++r) {
            const char *row = k->beat[j].row[r].steps;
            if (!row) continue;
            unsigned len = (unsigned)strlen(row), bars = len >= 64 ? 4U : len >= 32 ? 2U : 1U, v = k->beat[j].row[r].voice;
            for (unsigned b = 0; b < GBX_DECK_BARS; ++b)
                for (unsigned s = 0; s < 16; ++s) {
                    char c = row[(b % bars) * 16U + s];
                    if (c == 'x' || c == 'X') gbx_deck[b].grid[v] |= (uint16_t)(1U << s);
                    if (c == 'X') gbx_deck[b].accent |= (uint16_t)(1U << s);
                }
        }
    }
    for (unsigned part = 1; part < PARTS; ++part)
        for (unsigned j = 0; j < LOOPS; ++j) {
            if (!((want >> (4U * part + j)) & 1U)) continue;
            compile_line(part == 1 ? k->bass[j] : part == 2 ? k->chords[j] : k->melody[j], part);
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
    cr78.level = 10300; cr78.muted = 0; cr78.voice_mute = 0;
    gbx_bass.level = 14500; gbx_bass.muted = 0; gbx_bass.armed = 1;
    gbx_keys.muted = 0; gbx_keys.armed = 1; gbx_keys.strum = 0; mysynth_params.duck = 18000;
    gbx_lead.muted = 0; gbx_lead.armed = 1; mysynth_lead_params.duck = 19000;
}
void deck_release(void)
{
    if (!gbx_deck_on) return;
    gbx_deck_on = 0;
    mysynth_state_set(saved_synth);
}
unsigned deck_active(void) { return active; }

/* ---- the screen ---- */
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
    /* the loop just touched, by name; the bar of four and the beat */
    static unsigned shown = 255;
    for (unsigned b = 0; b < 16; ++b) if (((want ^ playing) >> b) & 1U) shown = b;
    if (shown < 16) {
        unsigned part = shown / 4U, j = shown % 4U;
        const char *name = part == 0 ? k->beat[j].name : part == 1 ? k->bass_name[j] : part == 2 ? k->chords_name[j] : k->melody_name[j];
        char line[24]; strcpy(line, part_name[part]); strcat(line, " "); strcat(line, name);
        px_print(line, 2, 45, part_color[part]);
    }
    if (cr78_running) {
        for (unsigned b = 0; b < GBX_DECK_BARS; ++b) px_rect(2 + (int)b * 6, 54, 4, 3, b == gbx_deck_bar ? PX_WHITE : PX_DUSK);
        for (unsigned q = 0; q < 4; ++q) px_rect(30 + (int)q * 6, 54, 4, 3, q == cr78_playhead / 4U ? PX_GOLD : PX_DUSK);
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
