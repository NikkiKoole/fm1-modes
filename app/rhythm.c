/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* RHYTHM BOX panel mode, in the shared grammar: SELECT pages, the four KNOBs
 * are labelled in the bottom bar, PRESETS picks the rhythm, HOME goes back to
 * the launcher. The rhythm keeps playing with the FM-1 transport after you leave.
 *
 *   MACHINE  MACH   MAP    TEMPO  LEVEL     FR-2L TR-77 M252 M253 HAMMOND M255 M254;
 *                                           MAP 0 = the machine's own kit (TR-77),
 *                                           then the Side Man maps (autorhythm's V key)
 *   CHORD    ROOT   QUAL   CLICK            what the CHORD and BASS lanes (Hammond,
 *                                           M255) play; CLICK = the TR-77 metronome lane
 *
 *   keys     hold a chord: lowest key = root, minor if its minor third is held
 *   FX  mute        PLAY  FM-1 play / stop
 *
 * Lane-to-voice mapping and the firing rules are autorhythm.c's; the voices
 * are tr77.h and sideman.h, number for number. */
#include "rhythm.h"
#include "pixel.h"
#include "instrument.h"
#include "rhythmbox.h"
#include <string.h>

/* ---- the machines, in date order (autorhythm.c) ---- */
struct machine { const char *name, *note; const RbRhythm *set; int count; };
static const struct machine machines[] = {
    {"FR-2L", "ACE TONE 1969", RB_FR2L, RB_FR2L_N},
    {"TR-77", "ROLAND 1972", RB_TR77, RB_TR77_N},
    {"M252", "SGS CHIP PINS", RB_SGS, RB_SGS_N},
    {"M253", "SGS CHIP PINS", RB_M253, RB_M253_N},
    {"HAMM", "HAMMOND PATENT 1969", RB_HAM, RB_HAM_N},
    {"M255", "SGS NAMED PINS", RB_M255, RB_M255_N},
    {"M254", "SGS ACCOMPANIMENT", RB_M254, RB_M254_N},
};
enum { MACHINES = sizeof machines / sizeof machines[0], MACH_TR77 = 1 };

/* ---- TR-77 kit (tr77.h) ---- */
enum { T_BD, T_SD, T_RS, T_HH, T_CY, T_HB, T_LB, T_LC, T_CB, T_CL, T_MA, T_GU, T_TB, T_TBS, T_ME };
/* a short gate into an exponential release (tr77__exp) */
#define X(w, n, t, r, g, v, lvl) {.wave = w, .note = n, .tune = t, .sustain = 7, .exp_release = 1, \
                                  .release = r, .gate = g, .vol = v, .level = lvl}
static struct dv_recipe tr77_recipe[RHYTHM_TR77_VOICES][2];
static const uint8_t tr77_parts[RHYTHM_TR77_VOICES] = {2, 2, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1};
static void tr77_recipes(void)
{
    struct dv_recipe (*r)[2] = tr77_recipe;
    r[T_BD][0] = (struct dv_recipe)X(DV_SINE, 36, 0, 310, 5, 7, 1.0f);
    r[T_BD][0].filter = DV_FILTER_HIGH; r[T_BD][0].cutoff = 40; r[T_BD][0].reso = 2;
    r[T_BD][0].pitch = 1.0f; r[T_BD][0].pitch_ms = 30;
    r[T_BD][1] = (struct dv_recipe)X(DV_SINE, 49, 0, 120, 5, 7, 0.12f);
    r[T_SD][0] = (struct dv_recipe)X(DV_SINE, 65, 0.29f, 140, 5, 5, 0.40f);
    r[T_SD][1] = (struct dv_recipe)X(DV_NOISE, 60, 0, 150, 5, 6, 1.0f);
    r[T_SD][1].filter = DV_FILTER_LOW; r[T_SD][1].cutoff = 9000; r[T_SD][1].reso = 4;
    r[T_RS][0] = (struct dv_recipe)X(DV_TRI, 86, 0, 6, 2, 7, 1.0f);
    r[T_RS][0].filter = DV_FILTER_BAND; r[T_RS][0].cutoff = 1100; r[T_RS][0].reso = 5;
    r[T_RS][1] = (struct dv_recipe)X(DV_SINE, 33, 0, 8, 1, 3, 1.0f);
    r[T_HH][0] = (struct dv_recipe)X(DV_NOISE, 60, 0, 14, 30, 6, 0.13f);
    r[T_HH][0].filter = DV_FILTER_BAND; r[T_HH][0].cutoff = 8550; r[T_HH][0].reso = 15;
    r[T_CY][0] = (struct dv_recipe)X(DV_NOISE, 60, 0, 480, 90, 6, 0.13f);
    r[T_CY][0].filter = DV_FILTER_BAND; r[T_CY][0].cutoff = 8650; r[T_CY][0].reso = 15;
    /* bongos / conga: start a little flat and settle up; a highpass takes out the DC thud */
    r[T_HB][0] = (struct dv_recipe)X(DV_SINE, 65, 0.58f, 130, 1, 7, 1.0f);
    r[T_HB][0].pitch = -0.30f; r[T_HB][0].pitch_ms = 100;
    r[T_HB][0].filter = DV_FILTER_HIGH; r[T_HB][0].cutoff = 180; r[T_HB][0].reso = 2;
    r[T_LB][0] = (struct dv_recipe)X(DV_SINE, 57, -0.16f, 165, 1, 7, 1.0f);
    r[T_LB][0].pitch = -0.15f; r[T_LB][0].pitch_ms = 120;
    r[T_LB][0].filter = DV_FILTER_HIGH; r[T_LB][0].cutoff = 110; r[T_LB][0].reso = 2;
    r[T_LC][0] = (struct dv_recipe)X(DV_SINE, 49, 0.78f, 280, 1, 7, 1.0f);
    r[T_LC][0].pitch = -0.50f; r[T_LC][0].pitch_ms = 40;
    r[T_LC][0].filter = DV_FILTER_HIGH; r[T_LC][0].cutoff = 75; r[T_LC][0].reso = 2;
    for (unsigned k = 0; k < 2; ++k) {   /* the CR-78 cowbell */
        r[T_CB][k] = (struct dv_recipe){.wave = DV_SQUARE, .note = k ? 73 : 79, .filter = DV_FILTER_LOW,
            .cutoff = 1100, .reso = 2, .decay = 55, .release = 25, .gate = 150, .vol = k ? 3 : 4, .level = 0.58f};
    }
    r[T_CL][0] = (struct dv_recipe)X(DV_SINE, 97, 0.25f, 45, 1, 6, 1.0f);
    r[T_MA][0] = (struct dv_recipe){.wave = DV_NOISE, .note = 90, .filter = DV_FILTER_BAND, .cutoff = 8700,
        .reso = 15, .decay = 15, .release = 8, .gate = 30, .vol = 4, .level = 0.18f};
    r[T_GU][0] = (struct dv_recipe){.wave = DV_SCRAPE, .note = 60, .sustain = 7, .release = 30, .gate = 520,
        .vol = 5, .level = 0.30f, .teeth = 74.13f};
    r[T_TB][0] = (struct dv_recipe){.wave = DV_NOISE, .note = 80, .filter = DV_FILTER_BAND, .cutoff = 4340,
        .reso = 15, .attack = 8, .decay = 200, .release = 60, .gate = 220, .vol = 5, .level = 0.10f};
    r[T_TBS][0] = r[T_TB][0]; r[T_TBS][0].vol = 3;
    r[T_ME][0] = (struct dv_recipe)X(DV_SINE, 96, 0, 20, 1, 5, 0.20f);
}

/* ---- the Wurlitzer Side Man (sideman.h) ---- */
enum { S_BASS, S_TOM1, S_TOM2, S_WOOD, S_TEMP1, S_TEMP2, S_CLAVES, S_BRUSH, S_MARACAS, S_CYMBAL };
static struct dv_recipe sideman_recipe[RHYTHM_SIDEMAN_VOICES][4];
static uint8_t sideman_parts[RHYTHM_SIDEMAN_VOICES];
static void sideman_recipes(void)
{
    static const struct { uint8_t note, vol; uint16_t dur; float level, tube; uint8_t click; } v[RHYTHM_SIDEMAN_VOICES] = {
        {42, 7, 200, 0.70f, 1.00f, 0}, {54, 7, 180, 0.65f, 1.00f, 2}, {49, 7, 200, 0.64f, 1.00f, 2},
        {90, 7, 70, 0.95f, 0.85f, 5},  {83, 7, 95, 0.93f, 0.90f, 4},  {78, 7, 120, 0.92f, 0.95f, 4},
        {97, 7, 45, 1.00f, 0.60f, 6},  {72, 6, 110, 0.80f, 0.50f, 0}, {84, 7, 40, 0.70f, 0.50f, 0},
        {90, 7, 380, 0.89f, 0.40f, 0}};
    const float tube = 0.45f;
    /* the voice circuits: wave, A, D, R, filter, cutoff, res, pitch tuck */
    static const struct { uint8_t wave, filter, reso; uint16_t a, d, r, cutoff; float pitch, pitch_ms; } c[RHYTHM_SIDEMAN_VOICES] = {
        {DV_SINE, DV_FILTER_LOW, 2, 4, 175, 45, 320, 5.0f, 26}, {DV_SINE, DV_FILTER_LOW, 3, 1, 150, 40, 850, 3.5f, 16},
        {DV_SINE, DV_FILTER_LOW, 3, 1, 170, 45, 700, 3.5f, 18}, {DV_TRI, DV_FILTER_BAND, 10, 0, 45, 14, 1480, 0, 0},
        {DV_TRI, DV_FILTER_BAND, 10, 0, 70, 20, 988, 0, 0},     {DV_TRI, DV_FILTER_BAND, 10, 0, 90, 24, 740, 0, 0},
        {DV_TRI, DV_FILTER_BAND, 10, 0, 28, 10, 2217, 0, 0},    {DV_NOISE, DV_FILTER_BAND, 7, 8, 85, 30, 1400, 0, 0},
        {DV_NOISE, DV_FILTER_BAND, 13, 0, 24, 10, 3600, 0, 0},  {DV_NOISE, DV_FILTER_BAND, 12, 0, 300, 95, 4200, 0, 0}};
    const struct dv_recipe click = {.wave = DV_NOISE, .note = 96, .filter = DV_FILTER_BAND, .cutoff = 3400, .reso = 2,
                                    .decay = 5, .release = 3, .gate = 8, .level = 1.0f, .drive = tube * 0.40f};
    for (unsigned i = 0; i < RHYTHM_SIDEMAN_VOICES; ++i) {
        struct dv_recipe *r = sideman_recipe[i];
        unsigned n = 0;
        if (i == S_BRUSH) {   /* the tonal shell under the head, a fourth apart */
            for (unsigned k = 0; k < 2; ++k)
                r[n++] = (struct dv_recipe){.wave = DV_SINE, .note = k ? 61 : 54, .filter = DV_FILTER_LOW, .cutoff = 1100,
                    .reso = 1, .attack = 2, .decay = 95, .release = 30, .gate = 110, .vol = k ? 2 : 3, .level = 0.55f,
                    .pitch = 3.0f, .pitch_ms = 18, .drive = tube * 0.50f};
        }
        r[n++] = (struct dv_recipe){.wave = c[i].wave, .note = v[i].note, .filter = c[i].filter, .cutoff = c[i].cutoff,
            .reso = c[i].reso, .attack = c[i].a, .decay = c[i].d, .release = c[i].r, .gate = v[i].dur, .vol = v[i].vol,
            .level = v[i].level, .pitch = c[i].pitch, .pitch_ms = c[i].pitch_ms, .drive = tube * v[i].tube};
        if (i == S_BRUSH)     /* the wires: 2 ms behind the stick, a frozen 260 Hz grain */
            r[n++] = (struct dv_recipe){.wave = DV_NOISE, .note = 72, .filter = DV_FILTER_BAND, .cutoff = 4600, .reso = 12,
                .attack = 4, .decay = 280, .release = 100, .gate = 320, .offset = 2, .vol = 6, .level = 0.34f,
                .drive = tube * 0.50f, .lfo_hz = 260.0f, .lfo_depth = 0.75f};
        if (i == S_CYMBAL)    /* the thin metallic layer, a tritone apart */
            for (unsigned k = 0; k < 2; ++k)
                r[n++] = (struct dv_recipe){.wave = DV_SQUARE, .note = k ? 97 : 91, .filter = DV_FILTER_BAND, .cutoff = 4200,
                    .reso = 11, .decay = 210, .release = 70, .gate = 220, .vol = 2, .level = 0.54f, .drive = tube * 0.70f};
        if (v[i].click) { r[n] = click; r[n].vol = v[i].click; ++n; }   /* the contact pulse on a block's front */
        sideman_parts[i] = (uint8_t)n;
    }
}
/* lane label -> Side Man voice, read as a HINT (autorhythm.c voice_hint) */
static const uint8_t VORDER[RHYTHM_SIDEMAN_VOICES] = {
    S_BASS, S_BRUSH, S_MARACAS, S_CLAVES, S_TOM1, S_WOOD, S_TEMP1, S_CYMBAL, S_TOM2, S_TEMP2};
static unsigned voice_hint(const char *label, unsigned lane)
{
    char t[8]; int n = 0, primed = 0;
    for (const char *p = label; *p && n < 7; p++) {
        if (*p == '+' || *p == ' ' || *p == '-') break;
        t[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
    }
    t[n] = 0;
    for (const char *p = label; *p; p++) if (*p == '\'') primed = 1;
    if (n >= 2) {
        if (t[0] == 'b' && t[1] == 'd') return S_BASS;
        if (t[0] == 's' && t[1] == 'd') return primed ? S_TEMP1 : S_BRUSH;
        if (t[0] == 'c' && t[1] == 'y') return primed ? S_BRUSH : S_CYMBAL;
        if (t[0] == 'h' && t[1] == 'h') return S_MARACAS;
        if (t[0] == 'c' && t[1] == 'b') return S_WOOD;
        if (t[0] == 'h' && t[1] == 'b') return S_TOM1;
        if (t[0] == 'l' && t[1] == 'b') return S_TOM2;
        if (t[0] == 'h' && t[1] == 'c') return S_TOM1;
        if (t[0] == 'l' && t[1] == 'c') return S_TOM2;
        if (t[0] == 'r' && t[1] == 's') return S_CLAVES;
        if (t[0] == 't' && t[1] == 'b') return S_MARACAS;
        if (t[0] == 'g' && t[1] == 'u') return S_MARACAS;
        if (t[0] == 'o' && t[1] == 'u') return VORDER[lane % RHYTHM_SIDEMAN_VOICES];
        if (t[0] == 'm' && t[1] == 'e') return S_WOOD;
    }
    if (n == 1) {
        if (t[0] == 'm') return S_MARACAS;
        if (t[0] == 'c') return S_CLAVES;
        if (t[0] == 'b') return S_BRUSH;
    }
    return VORDER[lane % RHYTHM_SIDEMAN_VOICES];
}
/* one printed TR-77 label -> the voices its trigger line fires ("CB+LB" is one diode line, two drums) */
static int tr77_token(const char *t, int n, int primed)
{
    char a = (char)(t[0] | 32), b = n > 1 ? (char)(t[1] | 32) : 0;
    if (n == 2) {
        if (a == 'b' && b == 'd') return T_BD;
        if (a == 's' && b == 'd') return T_SD;
        if (a == 'r' && b == 's') return T_RS;
        if (a == 'h' && b == 'h') return T_HH;
        if (a == 'c' && b == 'y') return T_CY;
        if (a == 'h' && b == 'b') return T_HB;
        if (a == 'l' && b == 'b') return T_LB;
        if (a == 'l' && b == 'c') return T_LC;
        if (a == 'c' && b == 'b') return T_CB;
        if (a == 'g' && b == 'u') return T_GU;
        if (a == 't' && b == 'b') return primed ? T_TBS : T_TB;
        if (a == 'm' && b == 'e') return T_ME;
    }
    if (n == 1) {
        if (a == 'c') return T_CL;
        if (a == 'm') return T_MA;
    }
    return -1;
}
static int tr77_lane(const char *label, uint8_t out[2])
{
    int k = 0, primed = 0;
    for (const char *p = label; *p; p++) if (*p == '\'') primed = 1;
    const char *s = label;
    while (*s && k < 2) {
        int n = 0;
        while (s[n] && s[n] != '+' && s[n] != '\'' && s[n] != ' ') n++;
        if (n) { int v = tr77_token(s, n, primed); if (v >= 0) out[k++] = (uint8_t)v; }
        s += n;
        while (*s == '+' || *s == '\'' || *s == ' ') s++;
    }
    return k;
}
static int is_metronome(const char *label)
{
    return (label[0] == 'M' || label[0] == 'm') && (label[1] == 'e' || label[1] == 'E');
}

/* ---- state ---- */
static unsigned active, initialized, full_redraw, machine = MACH_TR77, rhythm_index, rotation, metronome, level = 22;
static uint32_t prev_buttons, prev_keys, last_ms, flash_until[RHYTHM_LANES];
enum { MACHINE, CHORD, PAGES };
static const struct px_page pages[PAGES] = {
    {"MACHINE", {"MACH", "MAP", "TMPO", "LVL"}},
    {"CHORD", {"ROOT", "QUAL", "CLIK", ""}},
};
static unsigned page;
static const RbRhythm *current(void) { return &machines[machine].set[rhythm_index]; }
static unsigned tr77_kit(void) { return machine == MACH_TR77 && rotation == 0; }

static void build_voices(void)
{
    tr77_recipes(); sideman_recipes();
    for (unsigned v = 0; v < RHYTHM_TR77_VOICES; ++v) {
        struct dv_voice next = {0};
        next.parts = tr77_parts[v];
        for (unsigned p = 0; p < next.parts; ++p) dv_build_part(&next.part[p], &tr77_recipe[v][p], 0, 1.0f);
        rhythm.tr77[v] = next;
    }
    for (unsigned v = 0; v < RHYTHM_SIDEMAN_VOICES; ++v) {
        struct dv_voice next = {0};
        next.parts = sideman_parts[v];
        for (unsigned p = 0; p < next.parts; ++p) dv_build_part(&next.part[p], &sideman_recipe[v][p], 0, 1.0f);
        rhythm.sideman[v] = next;
    }
    /* the organ's thin drawbar chord and its divided bass (autorhythm.c) */
    const struct dv_recipe organ = {.wave = DV_SQUARE, .note = 60, .attack = 4, .decay = 40, .sustain = 5, .release = 90,
                                    .gate = 150, .vol = 3, .level = 0.45f, .filter = DV_FILTER_LOW, .cutoff = 2400, .reso = 1};
    const struct dv_recipe bass = {.wave = DV_TRI, .note = 45, .attack = 2, .decay = 60, .sustain = 4, .release = 120,
                                   .gate = 170, .vol = 5, .level = 0.80f};
    rhythm.organ.parts = 1; dv_build_part(&rhythm.organ.part[0], &organ, 0, 1.0f);
    rhythm.bass.parts = 1; dv_build_part(&rhythm.bass.part[0], &bass, 0, 1.0f);
}

/* Fill the idle pattern buffer from a rhythm, then flip it live. The firing
 * rules are autorhythm.c's: a GATE lane fires where its run of marks starts,
 * an EDGE_ONLY (SGS) rhythm never retriggers a mark whose predecessor is set. */
static void load(void)
{
    const RbRhythm *r = current();
    struct rhythm_pattern *p = &rhythm.pattern[(rhythm.live & 1U) ^ 1U];
    memset(p, 0, sizeof *p);
    p->kit = tr77_kit() ? RHYTHM_KIT_TR77 : RHYTHM_KIT_SIDEMAN;
    p->per_beat = r->per_beat ? r->per_beat : 4;
    for (int c = 0; c < r->counts && c < RHYTHM_COUNTS; ++c)
        if (rb_used(r, c)) p->order[p->used++] = (uint8_t)c;
    if (!p->used) p->order[p->used++] = 0;
    for (int l = 0; l < r->nlanes && l < RHYTHM_LANES; ++l) {
        const RbLane *src = &r->lane[l];
        struct rhythm_lane *lane = &p->lane[l];
        p->lanes = (uint8_t)(l + 1);
        if (src->flags & RB_RESETCOL) continue;                   /* a counter, not a drum */
        if (is_metronome(src->label) && !metronome) continue;     /* a click for the player */
        lane->role = src->role;
        if (src->role & RB_ROLE_DRUM) {
            if (p->kit == RHYTHM_KIT_TR77) lane->voices = (uint8_t)tr77_lane(src->label, lane->voice);
            else {
                unsigned v = voice_hint(src->label, (unsigned)l);
                lane->voice[0] = (uint8_t)(rotation ? VORDER[(v + rotation) % RHYTHM_SIDEMAN_VOICES] : v);
                lane->voices = 1;
            }
        }
        for (unsigned i = 0; i < p->used; ++i) {
            int c = p->order[i];
            int fires = src->kind == RB_GATE ? rb_hit(r, l, c) && !rb_hit(r, l, (c + r->counts - 1) % r->counts)
                                             : rb_trigger(r, l, c);
            if (!fires) continue;
            lane->fire |= 1ULL << c;
            if (p->kit == RHYTHM_KIT_TR77 && src->kind == RB_GATE) {
                /* a gate is held for its run of marks; the guiro runs to half a tick before the next one */
                unsigned run = 1, gap = 1;
                while (run < p->used && rb_hit(r, l, p->order[(i + run) % p->used])) ++run;
                while (gap < p->used && !rb_hit(r, l, p->order[(i + gap) % p->used])) ++gap;
                unsigned guiro = lane->voices && lane->voice[0] == T_GU;
                unsigned half = guiro ? 2 * gap - 1 : 2 * run;
                lane->hold[c] = (uint8_t)(half > 255 ? 255 : half);
            }
        }
    }
    __asm__ volatile("" ::: "memory");
    rhythm.live ^= 1U;
}
static unsigned has_accompaniment(void)
{
    const RbRhythm *r = current();
    for (int l = 0; l < r->nlanes; ++l)
        if (r->lane[l].role & (RB_ROLE_CHORD | RB_ROLE_BASSLO | RB_ROLE_BASSHI)) return 1;
    return 0;
}
/* the held chord: lowest key = root, minor if its minor third is held too.
 * Chord tones fold into the patent's fixed window (rb_easyplay_note): F3..E4. */
static unsigned chord_root = 9, chord_minor = 1;   /* A minor until you play */
static void follow_chord(uint32_t keys)
{
    if (keys) {
        unsigned low = 0;
        while (!((keys >> low) & 1U)) ++low;
        chord_root = (low + 5U) % 12U;                       /* key 0 is F */
        chord_minor = (keys >> (low + 3)) & 1U;
    }
    static const uint8_t degree[3] = {RB_CHORD_ROOT, RB_CHORD_MAJ3, RB_CHORD_FIFTH};
    for (unsigned k = 0; k < 3; ++k) {
        int d = k == 1 && chord_minor ? RB_CHORD_MIN3 : degree[k];
        rhythm.chord[k] = (uint8_t)(53 + (((int)chord_root + d - 5) % 12 + 12) % 12);
    }
    rhythm.bass_note[0] = (uint8_t)(36 + chord_root);
    rhythm.bass_note[1] = (uint8_t)(36 + chord_root + 7);   /* the fifth, above the root */
}
static void init(void)
{
    dv_build_tables();
    build_voices();
    rhythm.level = (int32_t)(level * 32768U / 100U);
    follow_chord(0);
    rhythm_index = 0;
    load();
    initialized = 1;
}

/* ---- screen ---- */
static const char *const note_names[12] = {"C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B"};
static void draw(uint32_t now)
{
    char text[24], v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    const RbRhythm *r = current();
    const struct rhythm_pattern *p = &rhythm.pattern[rhythm.live & 1U];
    px_cls(PX_NIGHT);
    if (page == MACHINE) {
        static const char *const short_name[MACHINES] = {"FR2L", "TR77", "M252", "M253", "HAMM", "M255", "M254"};
        strcpy(v[0], short_name[machine]);
        if (!rotation) strcpy(v[1], tr77_kit() ? "KIT" : "SM");
        else { strcpy(v[1], "SM"); px_number((int)rotation, v[1] + 2); }
        px_number((int)groove_tempo_current(), v[2]); px_number((int)level, v[3]);
    } else {
        strcpy(v[0], note_names[chord_root]); strcpy(v[1], chord_minor ? "MIN" : "MAJ");
        strcpy(v[2], metronome ? "ON" : "OFF"); v[3][0] = 0;
    }
    px_chrome("RHYTHM", pages, PAGES, page, value);
    /* the rhythm name, uppercased and cut to fit */
    unsigned n = 0, fit = rhythm.muted ? 14 : 19;
    for (const char *s = r->name; *s && n < fit; ++s)
        text[n++] = (*s >= 'a' && *s <= 'z') ? (char)(*s - 32) : *s;
    text[n] = 0;
    px_print(text, 2, 8, PX_WHITE);
    if (rhythm.muted) px_print("MUTE", PX_W - 17, 8, PX_RED);
    /* the lanes: one column per count, skipped counts left dark */
    int counts = r->counts < RHYTHM_COUNTS ? r->counts : RHYTHM_COUNTS;
    int cw = counts <= 16 ? 4 : counts <= 32 ? 2 : 1, x0 = 15, y0 = 16;
    unsigned head = rhythm_count, run = rhythm_running;
    uint16_t hits = rhythm_hits; rhythm_hits = 0;
    for (int c = 0; c < counts; ++c) {
        int x = x0 + c * cw;
        if (!rb_used(r, c)) continue;
        if (run && c == (int)head) px_rect(x, y0 - 2, cw, p->lanes * 3 + 2, PX_DUSK);
        if (c % r->per_beat == 0) px_pset(x, y0 - 2, PX_SLATE);
    }
    for (unsigned l = 0; l < p->lanes; ++l) {
        const struct rhythm_lane *lane = &p->lane[l];
        int y = y0 + (int)l * 3;
        if (hits & (1U << l)) flash_until[l] = now + 90U;
        unsigned lit = (int32_t)(flash_until[l] - now) > 0;
        px_rect(1, y, 11, 2, lit ? PX_WHITE : (lane->voices || lane->role & ~RHYTHM_ROLE_DRUM ? PX_DUSK : PX_NIGHT));
        unsigned color = lane->role & ~RHYTHM_ROLE_DRUM ? PX_GREEN : (l & 1U) ? PX_CYAN : PX_GOLD;
        for (int c = 0; c < counts; ++c) {
            if (!((lane->fire >> c) & 1U)) continue;
            int x = x0 + c * cw, w = cw > 1 ? cw - 1 : 1;
            if (lane->hold[c]) w = cw * lane->hold[c] / 2;   /* a held gate draws as a bar */
            px_rect(x, y, w, 2, run && c == (int)head ? PX_WHITE : color);
        }
    }
    /* footer: the source, and the chord the accompaniment follows (or the kit) */
    int fy = y0 + RHYTHM_LANES * 3 + 1;
    px_print(machines[machine].note, 2, fy, PX_SLATE);
    if (has_accompaniment()) {
        char chord[16] = "CHORD ";
        strcat(chord, note_names[chord_root]);
        strcat(chord, chord_minor ? " MIN" : " MAJ");
        px_print(chord, 2, fy + 6, PX_GREEN);
    } else {
        px_number(p->per_beat, text);
        strcat(text, " PER BEAT ");
        px_number(p->used, text + strlen(text));
        px_print(text, 2, fy + 6, PX_SLATE);
    }
    px_flash_draw(now);
}

unsigned rhythm_active(void) { return active; }

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void name_flash(uint32_t now)
{
    static char text[24];
    unsigned n = 0;
    for (const char *s = current()->name; *s && n < 9; ++s)
        text[n++] = (*s >= 'a' && *s <= 'z') ? (char)(*s - 32) : *s;
    text[n] = 0;
    px_flash(text, now, 700U);
}

int rhythm_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    unsigned toggle = px_mode_toggle(pressed, PX_MODE_RHYTHM, active);
    uint32_t keys_changed = c->keys ^ prev_keys;
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!toggle) return 0;
        if (!initialized) init();
        rhythm.armed = 1;
        px_leds_off();
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(pages[page].name, now, 700U);
        return 1;
    }
    if (toggle) { px_leds_off(); active = 0; return 2; }
    unsigned changed = 0, reload = 0;
    const int16_t *e = c->encoders;
    /* SELECT: pages */
    if (e[0]) { page = (unsigned)clamp((int)page + e[0], 0, PAGES - 1); px_flash(pages[page].name, now, 700U); changed = 1; }
    /* PRESETS: the rhythm */
    if (e[6]) {
        int count = machines[machine].count;
        rhythm_index = (unsigned)(((int)rhythm_index + e[6] + count * 8) % count); reload = 1;
        name_flash(now);
    }
    const int16_t k1 = e[2], k2 = e[3], k3 = e[4], k4 = e[5];
    if (page == MACHINE) {
        if (k1) { machine = (unsigned)(((int)machine + k1 + MACHINES * 8) % MACHINES); rhythm_index = 0; reload = 1;
                  px_flash(machines[machine].name, now, 700U); }
        if (k2) { rotation = (unsigned)(((int)rotation + k2 + RHYTHM_SIDEMAN_VOICES * 8) % RHYTHM_SIDEMAN_VOICES); reload = 1; }
        if (k3) { groove_tempo_edit(k3); changed = 1; }
        if (k4) { level = (unsigned)clamp((int)level + k4 * 4, 0, 100);
                  rhythm.level = (int32_t)(level * 32768U / 100U); changed = 1; }
    } else {
        if (k1) { chord_root = (unsigned)(((int)chord_root + k1 + 12 * 8) % 12); follow_chord(0); changed = 1; }
        if (k2) { chord_minor = k2 < 0; follow_chord(0); changed = 1; }
        if (k3) { metronome = k3 > 0; reload = 1; }
    }
    if (keys_changed) { follow_chord(c->keys); changed = 1; }
    if (reload) { load(); changed = 1; }
    if (pressed & PX_BIT(PX_FX)) { rhythm.muted ^= 1U; px_flash(rhythm.muted ? "MUTE" : "UNMUTE", now, 600U); changed = 1; }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0};
        groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (now - last_ms >= 33U || changed) {
        last_ms = now;
        draw(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
