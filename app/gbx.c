/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX panel mode (GROOVEBOX-SPEC.md). Three levels:
 *
 *   LAUNCHER  --ALGORITHM-->  OVERVIEW  --ALGORITHM-->  TRACK (DRUMS BASS KEYS LEAD)
 *             <----HOME----             <----HOME----
 *
 * ALGORITHM moves along OVERVIEW, DRUMS, BASS, KEYS, LEAD; HOME is always one level up.
 * The same rules on every track:
 *   white keys        its steps (ENV held: accent)
 *   black keys        its parts: DRUMS the voice; BASS and LEAD notes, KEYS chords, in the song's key (OCT -/+)
 *   hold a step       + a knob: that step (DRUMS ratchet, accent; BASS and LEAD length, velocity, slide,
 *                     octave; KEYS length, velocity, octave)
 *                     + a black key: that step's voice hit / note
 *   KNOB 1-4          its sound (SELECT: pages)
 *   REC               live: the black keys play and write into the step playing
 *   FX tapped / held  mute / throw an effect on this track while held (REC too: latch)
 * Overview: four lanes; black keys 1-4 mute the tracks; white keys the 16 patterns (all four tracks'
 * steps; from the next bar; an empty one starts as a copy of the one playing); PRESETS a drum pattern
 * into the pattern playing; knobs MIX (TEMPO SWING FILTER PUMP), FX (DRIVE CRUSH GATE SWEEP), LEVELS,
 * KEY (ROOT SCALE). */
#include "gbx.h"
#include "pixel.h"
#include "cr78.h"
#include "fx.h"
#include "instrument.h"
#include "mysynth.h"
#include <string.h>

enum { T_DRUMS, T_BASS, T_KEYS, T_LEAD, TRACKS, OVERVIEW = -1 };
static const char *const track_name[TRACKS] = {"DRUMS", "BASS", "KEYS", "LEAD"};
static const char *const track_short[TRACKS] = {"DRM", "BAS", "KEY", "LED"};
static const uint8_t track_color[TRACKS] = {PX_EMBER, PX_GREEN, PX_CYAN, PX_PINK};
static const uint8_t track_bus[TRACKS] = {FX_CR78, FX_BASS, FX_SYNTH, FX_RHYTHM};
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[PX_PADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* ---- knobs ---- */
enum { K_NUM, K_TEMPO, K_FILTER, K_OFF0, K_ROOT, K_SCALE, K_WAVE, K_MS };
struct knob { const char *label; uint8_t kind; int16_t lo, hi, step; };
enum { MIX_PAGES = 4 };
static const struct px_page mix_page[MIX_PAGES] = {
    {"MIX", {"TMPO", "SWNG", "FLTR", "PUMP"}}, {"FX", {"DRV", "CRSH", "GATE", "SWP"}},
    {"LEVELS", {"DRM", "BAS", "KEY", "LED"}}, {"KEY", {"ROOT", "SCAL", "", ""}}};
static const struct knob mix_knob[MIX_PAGES][4] = {
    {{"TMPO", K_TEMPO, 0, 0, 1}, {"SWNG", K_NUM, 50, 66, 2}, {"FLTR", K_FILTER, -100, 100, 4}, {"PUMP", K_OFF0, 0, 100, 5}},
    {{"DRV", K_OFF0, 0, 100, 5}, {"CRSH", K_OFF0, 0, 100, 5}, {"GATE", K_OFF0, 0, 100, 5}, {"SWP", K_OFF0, 0, 100, 5}},
    {{"DRM", K_NUM, 0, 100, 4}, {"BAS", K_NUM, 0, 100, 4}, {"KEY", K_NUM, 0, 100, 4}, {"LED", K_NUM, 0, 100, 4}},
    {{"ROOT", K_ROOT, 0, 11, 1}, {"SCAL", K_SCALE, 0, 3, 1}, {"", K_NUM, 0, 0, 0}, {"", K_NUM, 0, 0, 0}}};
static int16_t mix_val[MIX_PAGES][4] = {{0, 50, 0, 0}, {0, 0, 0, 0}, {40, 60, 70, 70}, {9, 0, 0, 0}};   /* A minor */

/* the song's key: the black keys play its scale, so melodic tracks are always in key */
static const char *const pitch_name[12] = {"C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B"};
static const char *const scale_name[4] = {"MIN", "MAJ", "DOR", "PENT"};
static const uint8_t scale_steps[4][7] = {{0, 2, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 3, 5, 7, 10}};
static const uint8_t scale_len[4] = {7, 7, 7, 5};

/* the BASS track's sound */
enum { BASS_PAGES = 2 };
static const struct px_page bass_page[BASS_PAGES] = {{"SOUND", {"CUT", "RES", "DEC", "SUB"}}, {"SHAPE", {"WAVE", "DRV", "ATK", "PTCH"}}};
static const struct knob bass_knob[BASS_PAGES][4] = {
    {{"CUT", K_NUM, 0, 100, 3}, {"RES", K_NUM, 0, 100, 5}, {"DEC", K_NUM, 0, 100, 5}, {"SUB", K_OFF0, 0, 100, 5}},
    {{"WAVE", K_WAVE, 0, 2, 1}, {"DRV", K_OFF0, 0, 100, 5}, {"ATK", K_MS, 0, 40, 2}, {"PTCH", K_OFF0, 0, 12, 1}}};
static int16_t bass_val[BASS_PAGES][4] = {{40, 35, 35, 60}, {2, 0, 2, 0}};
static const char *const wave_name[3] = {"SINE", "TRI", "SQR"};

/* ---- the pads (FX held), the same eleven everywhere ---- */
enum { P_R16, P_R32, P_HLF, P_REV, P_LPF, P_HPF, P_CRS, P_GAT, P_DRV, P_PMP, P_SWP };
static const char *const pad_name[PX_PADS] = {"R16", "R32", "HLF", "REV", "LPF", "HPF", "CRS", "GAT", "DRV", "PMP", "SWP"};
enum { SCOPES = TRACKS + 1 };   /* [0] the mix, [1 + t] track t */
static uint32_t pads_held[SCOPES], pads_latched[SCOPES];

static unsigned active, initialized, full_redraw, voice, fx_held, fx_used, rec, muted[TRACKS];
static int zoom = OVERVIEW;                  /* OVERVIEW or the track zoomed into */
static unsigned page[SCOPES];                /* each view's SELECT page */
static unsigned bass_oct = 2, bass_note = 45; /* the black keys' octave; the note new steps get */
static unsigned keys_oct = 3, keys_chord = 0;  /* the black keys' octave; the chord (a black key) new steps get */
static unsigned lead_oct = 4, lead_note = 69;  /* the black keys' octave; the note new steps get */
static int16_t keys_size = 3, keys_strum = 0;  /* notes in a chord (3 triad, 4 seventh); ms between them */
static const struct px_page keys_chord_page = {"CHORD", {"SIZE", "STRM", "", ""}};
static uint32_t prev_buttons, prev_keys, last_ms, led_state;
static uint16_t held_steps, patterns_used = 1;   /* the slots that hold a pattern (1 plays at power-on) */

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float exp2_approx(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float sin_small(float x) { float x2 = x * x; return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f))); }
static float tanh_approx(float x) { float e = exp2_approx(2.0f * x * 1.442695f); return (e - 1.0f) / (e + 1.0f); }
static int32_t svf_f(float hz) { if (hz > 14000.0f) hz = 14000.0f; return (int32_t)(2.0f * sin_small(3.1415927f * hz / 88200.0f) * 32768.0f); }
static void set_drive(struct fx_perf *p, float amount)
{
    float dr = 0.15f + 0.85f * amount, g = dr * dr * 24.0f;
    p->drive_norm = (int32_t)(32768.0f * 0.2f / tanh_approx(0.2f * g));
    p->drive_neg = (int32_t)(g * (1.0f - 0.4f * dr) * 4096.0f); p->drive_pos = (int32_t)(g * 4096.0f);
    p->drive_mix = 32768; p->drive_level = 32768;
}

/* ---- the song's key ---- */
static unsigned key_note(unsigned i, unsigned oct)   /* black key i -> MIDI note, in the key */
{
    unsigned sc = (unsigned)mix_val[3][1], n = scale_len[sc];
    return 12U * (oct + 1U) + (unsigned)mix_val[3][0] + scale_steps[sc][i % n] + 12U * (i / n);
}
static void note_text(unsigned note, char *out)
{
    strcpy(out, pitch_name[note % 12U]);
    px_number((int)(note / 12U) - 1, out + strlen(out));
}

/* black key i's chord: the scale's degrees i, i+2, i+4 (and i+6) */
static void chord_notes(unsigned i, unsigned oct, uint8_t out[GBX_CHORD])
{
    for (unsigned k = 0; k < GBX_CHORD; ++k) out[k] = (int)k < keys_size ? (uint8_t)key_note(i + 2U * k, oct) : 0;
}
static void chord_text(const volatile uint8_t *n, char *out)   /* A- minor, B0 diminished, C major */
{
    strcpy(out, pitch_name[n[0] % 12U]);
    int third = n[1] - n[0], fifth = n[2] - n[0];
    if (fifth == 6) strcat(out, "0"); else if (third == 3) strcat(out, "-");
}

/* ---- the bass voice: a plucked wave through a lowpass, over a sine an octave down ---- */
static void build_bass(void)
{
    static const uint8_t waves[3] = {DV_SINE, DV_TRI, DV_SQUARE};
    float hz = 120.0f + (float)(bass_val[0][0] * bass_val[0][0]) * 0.45f;   /* 120 Hz .. 4.6 kHz */
    uint16_t dec = (uint16_t)(40 + bass_val[0][2] * 6);
    struct dv_recipe top = {.wave = waves[bass_val[1][0]], .note = 45, .filter = DV_FILTER_LOW, .cutoff = (uint16_t)hz,
                            .reso = (uint8_t)(bass_val[0][1] * 13 / 100), .attack = (uint16_t)bass_val[1][2], .decay = dec,
                            .sustain = 3, .release = 40, .gate = 400, .vol = 6, .level = 0.6f,
                            .pitch = (float)bass_val[1][3], .pitch_ms = 25, .drive = (float)bass_val[1][1] / 100.0f};
    struct dv_recipe low = {.wave = DV_SINE, .note = 33, .transpose = -12, .attack = (uint16_t)(bass_val[1][2] + 2),
                            .decay = (uint16_t)(dec + 60), .sustain = 5, .release = 50, .gate = 400, .vol = 6,
                            .level = (float)bass_val[0][3] / 100.0f};
    struct dv_voice next = {0};
    next.parts = bass_val[0][3] ? 2 : 1;
    dv_build_part(&next.part[0], &top, 0, 1.0f);
    if (bass_val[0][3]) dv_build_part(&next.part[1], &low, 0, 1.0f);
    gbx_bass.voice = next;
}

/* ---- the pads: a fixed throw each, on top of whatever is set; returns the effect bits they add ---- */
static int32_t throw_pads(struct fx_perf *p, uint32_t pads)
{
    int32_t on = 0;
    if (pads & (1U << P_LPF)) { p->lpf_f = svf_f(580.0f); p->lpf_damp = 28800; on |= PERF_LPF; }
    if (pads & (1U << P_HPF)) { p->hpf_f = svf_f(600.0f); p->hpf_damp = 33000; on |= PERF_HPF; }
    if (pads & (1U << P_CRS)) { p->crush_step = 1 << (24 - 6); p->crush_rate = 4; p->crush_mix = 32768; on |= PERF_CRUSH; }
    if (pads & (1U << P_GAT)) { p->gate_shift = 0; p->gate_len = 32768; p->gate_floor = 0; on |= PERF_GATE; }
    if (pads & (1U << P_DRV)) { set_drive(p, 0.7f); on |= PERF_DRIVE; }
    if (pads & (1U << P_PMP)) { p->pump_amount = 26214; p->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (pads & (1U << P_SWP)) { p->sweep_inc = (int32_t)(uint32_t)(0.5f * 97391.55f); p->sweep_depth = 22938; p->sweep_fb = 16384; p->sweep_mix = 19661; on |= PERF_SWEEP; }
    return on;
}
/* everything that sounds: the mix knobs + the mix's pads on the master, each track's pads on its bus */
static void apply(void)
{
    struct fx_perf *m = &fx_perf;
    const int16_t filter = mix_val[0][2], pump = mix_val[0][3];
    int32_t on = 0;
    if (filter < -2) { m->lpf_f = svf_f(150.0f * exp2_approx((float)(100 + filter) * 0.069f)); m->lpf_damp = 34000; on |= PERF_LPF; }
    else if (filter > 2) { m->hpf_f = svf_f(20.0f * exp2_approx((float)filter * 0.08f)); m->hpf_damp = 34000; on |= PERF_HPF; }
    if (pump) { m->pump_amount = (int32_t)(pump * 29491 / 100); m->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (mix_val[1][0]) { set_drive(m, (float)mix_val[1][0] / 100.0f); on |= PERF_DRIVE; }
    if (mix_val[1][1]) { int bits = 16 - mix_val[1][1] * 12 / 100; m->crush_step = 1 << (24 - bits); m->crush_rate = 1 + mix_val[1][1] * 15 / 100; m->crush_mix = 32768; on |= PERF_CRUSH; }
    if (mix_val[1][2]) { m->gate_shift = 0; m->gate_len = (int32_t)((95 - mix_val[1][2] * 80 / 100) * 65536 / 100); m->gate_floor = 0; on |= PERF_GATE; }
    if (mix_val[1][3]) { m->sweep_inc = (int32_t)(uint32_t)(0.25f * 97391.55f); m->sweep_depth = 26214; m->sweep_fb = 16384; m->sweep_mix = (int32_t)(mix_val[1][3] * 32768 / 100); on |= PERF_SWEEP; }
    uint32_t mix_pads = pads_held[0] | pads_latched[0];
    m->on = on | throw_pads(m, mix_pads);
    for (unsigned t = 0; t < TRACKS; ++t)
        fx_track_perf[track_bus[t]].on = throw_pads(&fx_track_perf[track_bus[t]], pads_held[1 + t] | pads_latched[1 + t]);
    /* the pattern bends: the drums' sequencer (the mix's rolls bend it too) */
    uint32_t bends = mix_pads | pads_held[1 + T_DRUMS] | pads_latched[1 + T_DRUMS];
    unsigned perf = 0;
    if (bends & (1U << P_R16)) perf |= CR78_PERF_ROLL16;
    if (bends & (1U << P_R32)) perf |= CR78_PERF_ROLL32;
    if (bends & (1U << P_HLF)) perf |= CR78_PERF_HALF;
    if (bends & (1U << P_REV)) perf |= CR78_PERF_REV;
    cr78.roll_len = 1; cr78.roll_fade = 1; cr78.perf = (uint8_t)perf;
    cr78.swing_q16 = (int32_t)((mix_val[0][1] - 50) * 4 * 65536 / 100);
    cr78.level = (int32_t)(mix_val[2][0] * 32768 / 100);
    cr78.muted = (uint8_t)muted[T_DRUMS];
    gbx_bass.level = (int32_t)(mix_val[2][1] * 32768 / 100);
    gbx_bass.muted = (uint8_t)muted[T_BASS];
    gbx_keys.muted = (uint8_t)muted[T_KEYS];
    mysynth_params.duck = (int32_t)((100 - mix_val[2][2]) * 32768 / 100);
    gbx_lead.muted = (uint8_t)muted[T_LEAD];
    mysynth_lead_params.duck = (int32_t)((100 - mix_val[2][3]) * 32768 / 100);
    gbx_keys.strum = (uint16_t)(keys_strum * 44);
}
static void init(void)
{
    for (unsigned s = 0; s < GBX_STEPS; ++s) { gbx_bass.note[s] = 45; gbx_bass.gate[s] = 10; gbx_bass.vel[s] = 100; }
    build_bass();
    gbx_bass.armed = 1;
    for (unsigned s = 0; s < GBX_STEPS; ++s) { gbx_keys.gate[s] = 14; gbx_keys.vel[s] = 90; }
    gbx_keys.armed = 1;
    for (unsigned s = 0; s < GBX_STEPS; ++s) { gbx_lead.note[s] = 69; gbx_lead.gate[s] = 12; gbx_lead.vel[s] = 100; }
    gbx_lead.armed = 1;
    initialized = 1;
}

/* ---- text ---- */
static void knob_text(const struct knob *kn, int x, char *out)
{
    *out = 0;
    if (!kn->label[0]) return;
    switch (kn->kind) {
    case K_TEMPO: px_number((int)groove_tempo_current(), out); break;
    case K_FILTER:
        if (x < -2) { strcpy(out, "LP"); px_number(-x, out + 2); }
        else if (x > 2) { strcpy(out, "HP"); px_number(x, out + 2); }
        else strcpy(out, "OFF");
        break;
    case K_OFF0: if (!x) strcpy(out, "OFF"); else px_number(x, out); break;
    case K_ROOT: strcpy(out, pitch_name[x % 12]); break;
    case K_SCALE: strcpy(out, scale_name[x & 3]); break;
    case K_WAVE: strcpy(out, wave_name[x % 3]); break;
    case K_MS: px_number(x, out); strcat(out, "MS"); break;
    default: px_number(x, out); break;
    }
}
static void readout(const char *who, const char *label, const char *value, uint32_t now)
{
    static char text[24];
    text[0] = 0;
    if (who) { strcpy(text, who); strcat(text, " "); }
    strcat(text, label); strcat(text, " "); strcat(text, value);
    px_flash(text, now, 900U);
}

/* ---- screens ---- */
static unsigned scope(void) { return zoom == OVERVIEW ? 0U : 1U + (unsigned)zoom; }
static const char *const *key_row(void)
{
    static const char *const lanes[PX_PADS] = {"DRM", "BAS", "KEY", "LED"};
    static const char *const none[PX_PADS] = {""};
    static char notes[PX_PADS][6];
    static const char *note_row[PX_PADS];
    if (fx_held) return pad_name;
    if (zoom == OVERVIEW) return lanes;
    if (zoom == T_DRUMS) return cr78_voice_row();
    if (zoom == T_BASS || zoom == T_LEAD) {
        unsigned oct = zoom == T_BASS ? bass_oct : lead_oct;
        for (unsigned i = 0; i < PX_PADS; ++i) { note_text(key_note(i, oct), notes[i]); note_row[i] = notes[i]; }
        return note_row;
    }
    if (zoom == T_KEYS) {
        uint8_t ch[GBX_CHORD];
        for (unsigned i = 0; i < PX_PADS; ++i) { chord_notes(i, keys_oct, ch); chord_text(ch, notes[i]); note_row[i] = notes[i]; }
        return note_row;
    }
    return none;
}
static uint32_t key_lit(void)
{
    if (fx_held) return pads_held[scope()] | pads_latched[scope()];
    if (zoom == OVERVIEW) { uint32_t m = 0; for (unsigned t = 0; t < TRACKS; ++t) if (muted[t]) m |= 1U << t; return m; }
    if (zoom == T_DRUMS) for (unsigned i = 0; i < PX_PADS; ++i) if (cr78_black_voice(i) == voice) return 1U << i;
    if (zoom == T_BASS) for (unsigned i = 0; i < PX_PADS; ++i) if (key_note(i, bass_oct) == bass_note) return 1U << i;
    if (zoom == T_KEYS) return 1U << keys_chord;
    if (zoom == T_LEAD) for (unsigned i = 0; i < PX_PADS; ++i) if (key_note(i, lead_oct) == lead_note) return 1U << i;
    return 0;
}
/* a line (BASS, LEAD) as a piano roll: pitch up, the notes as bars as long as they sound, slides joined */
static void draw_roll(const struct gbx_line *l, unsigned centre, unsigned colour, int top, int bottom, unsigned big)
{
    unsigned head = cr78_playhead, run = cr78_running;
    int lo = 127, hi = 0;
    for (unsigned s = 0; s < GBX_STEPS; ++s) if ((l->on >> s) & 1U) {
        if (l->note[s] < lo) lo = l->note[s];
        if (l->note[s] > hi) hi = l->note[s];
    }
    if (lo > hi) { lo = (int)centre - 6; hi = (int)centre + 6; }
    if (hi - lo < 12) { int m = (lo + hi) / 2; lo = m - 6; hi = m + 6; }
    int rows = bottom - top, x0 = big ? 8 : 16, w = 4;
    for (unsigned s = 0; s < GBX_STEPS; ++s) {
        int x = x0 + (int)s * w;
        if (run && s == head) px_rect(x - 1, top, w + 1, rows + 1, PX_DUSK);
        if (big && !(s & 3U)) px_pset(x, bottom + 1, PX_SLATE);
        if (!((l->on >> s) & 1U)) continue;
        int y = bottom - (int)((l->note[s] - lo) * rows / (hi - lo));
        int len = big ? 1 + (int)l->gate[s] * (w - 1) / 16 : w - 1;
        if (len > (16 - (int)s) * w) len = (16 - (int)s) * w;
        unsigned c = run && s == head ? PX_WHITE : (l->accent >> s) & 1U ? PX_GOLD : colour;
        px_rect(x, y - (big ? 1 : 0), len, big ? 2 : 1, c);
        if (big && ((l->slide >> s) & 1U) && s && ((l->on >> (s - 1)) & 1U)) {   /* the slide from the step before */
            int py = bottom - (int)((l->note[s - 1] - lo) * rows / (hi - lo));
            for (int k = 0; k <= w; ++k) px_pset(x - w + k, py + (y - py) * k / w, PX_SLATE);
        }
    }
    if (big) {   /* the range, low and high */
        char t[6];
        note_text((unsigned)hi, t); px_print(t, PX_W - 12, top, PX_DUSK);
        note_text((unsigned)lo, t); px_print(t, PX_W - 12, bottom - 4, PX_DUSK);
    }
}
/* the chords: each step's notes as dots on a pitch axis, a bar as long as it holds */
static void draw_chords(int top, int bottom, unsigned big)
{
    unsigned head = cr78_playhead, run = cr78_running;
    int lo = 127, hi = 0;
    for (unsigned s = 0; s < GBX_STEPS; ++s) if ((gbx_keys.on >> s) & 1U)
        for (unsigned k = 0; k < GBX_CHORD; ++k) if (gbx_keys.note[s][k]) {
            if (gbx_keys.note[s][k] < lo) lo = gbx_keys.note[s][k];
            if (gbx_keys.note[s][k] > hi) hi = gbx_keys.note[s][k];
        }
    if (lo > hi) { lo = (int)key_note(0, keys_oct); hi = lo + 14; }
    if (hi - lo < 14) { int m = (lo + hi) / 2; lo = m - 7; hi = m + 7; }
    int rows = bottom - top, x0 = big ? 8 : 16, w = 4;
    for (unsigned s = 0; s < GBX_STEPS; ++s) {
        int x = x0 + (int)s * w;
        if (run && s == head) px_rect(x - 1, top, w + 1, rows + 1, PX_DUSK);
        if (big && !(s & 3U)) px_pset(x, bottom + 1, PX_SLATE);
        if (!((gbx_keys.on >> s) & 1U)) continue;
        unsigned c = run && s == head ? PX_WHITE : (gbx_keys.accent >> s) & 1U ? PX_GOLD : PX_CYAN;
        int len = big ? (int)gbx_keys.gate[s] * w / 16 : w - 1;
        if (len < 1) len = 1;
        if (len > (16 - (int)s) * w) len = (16 - (int)s) * w;
        for (unsigned k = 0; k < GBX_CHORD; ++k) {
            if (!gbx_keys.note[s][k]) continue;
            int y = bottom - (int)((gbx_keys.note[s][k] - lo) * rows / (hi - lo));
            px_rect(x, y, big ? len : w - 1, 1, c);
        }
    }
    if (big) {
        char t[6];
        note_text((unsigned)hi, t); px_print(t, PX_W - 12, top, PX_DUSK);
        note_text((unsigned)lo, t); px_print(t, PX_W - 12, bottom - 4, PX_DUSK);
    }
}
static void draw_overview(void)
{
    /* four lanes, one bar each */
    unsigned head = cr78_playhead, run = cr78_running;
    for (unsigned t = 0; t < TRACKS; ++t) {
        int y = 16 + (int)t * 12;
        unsigned c = muted[t] ? PX_DUSK : track_color[t];
        px_print(track_short[t], 2, y + 2, c);
        if (t == T_BASS) { draw_roll(&gbx_bass, bass_note, PX_GREEN, y, y + 9, 0); continue; }
        if (t == T_KEYS) { draw_chords(y, y + 9, 0); continue; }
        if (t == T_LEAD) { draw_roll(&gbx_lead, lead_note, PX_PINK, y, y + 9, 0); continue; }
        for (unsigned s = 0; s < 16; ++s) {   /* DRUMS: taller where more voices hit */
            unsigned n = 0;
            for (unsigned v = 0; v < CR78_VOICES; ++v) n += (cr78.grid[v] >> s) & 1U;
            int x = 16 + (int)s * 4, h = n > 4 ? 9 : (int)n * 2 + 1;
            if (run && s == head) px_rect(x - 1, y - 1, 5, 11, PX_DUSK);
            if (n) px_rect(x, y + 9 - h, 3, h, run && s == head ? PX_WHITE : c);
            else px_pset(x + 1, y + 8, PX_DUSK);
        }
    }
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    const char *mode = zoom == OVERVIEW ? "GBX" : track_name[zoom];
    unsigned tint = zoom == OVERVIEW ? PX_GOLD : track_color[zoom];
    unsigned st = 0;
    if (held_steps) while (!((held_steps >> st) & 1U)) ++st;
    static char title[12];
    if (held_steps) { strcpy(title, "STEP "); px_number((int)st + 1, title + 5); }
    if (zoom == T_DRUMS && held_steps) {   /* a held drum step: its ratchet and accent */
        struct px_page step = {title, {"RTCH", "ACC", "", ""}};
        px_number(cr78.ratchet[voice][st], v[0]); strcpy(v[1], (cr78.accent >> st) & 1U ? "ON" : "OFF"); v[2][0] = v[3][0] = 0;
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if ((zoom == T_BASS || zoom == T_LEAD) && held_steps) {   /* a held bass / lead step: length, velocity, slide, octave */
        const struct gbx_line *l = zoom == T_BASS ? &gbx_bass : &gbx_lead;
        struct px_page step = {title, {"LEN", "VELO", "SLDE", "NOTE"}};
        px_number(l->gate[st], v[0]); strcat(v[0], "/16"); px_number(l->vel[st], v[1]);
        strcpy(v[2], (l->slide >> st) & 1U ? "ON" : "OFF"); note_text(l->note[st], v[3]);
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if (zoom == T_LEAD) {
        unsigned n; const struct px_page *lp = mysynth_lead_pages(&n);
        for (unsigned k = 0; k < 4; ++k) mysynth_lead_knob_text(page[scope()], k, v[k]);
        px_chrome_pads(mode, lp, n, page[scope()], value, key_row(), key_lit());
    } else if (zoom == T_KEYS && held_steps) {   /* a held keys step: length, velocity, octave */
        struct px_page step = {title, {"LEN", "VELO", "", "OCT"}};
        px_number(gbx_keys.gate[st], v[0]); strcat(v[0], "/16"); px_number(gbx_keys.vel[st], v[1]); v[2][0] = 0;
        chord_text(gbx_keys.note[st], v[3]);
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if (zoom == T_KEYS) {
        unsigned n; const struct px_page *mp = mysynth_pages(&n);
        static struct px_page kp[4];
        for (unsigned i = 0; i < n && i < 3; ++i) kp[i] = mp[i];
        kp[3] = keys_chord_page;
        unsigned pg = page[scope()];
        if (pg < 3) for (unsigned k = 0; k < 4; ++k) mysynth_knob_text(pg, k, v[k]);
        else {
            px_number(keys_size, v[0]); strcat(v[0], keys_size == 3 ? " TRI" : " 7TH");
            if (keys_strum) { px_number(keys_strum, v[1]); strcat(v[1], "MS"); } else strcpy(v[1], "OFF");
            v[2][0] = v[3][0] = 0;
        }
        px_chrome_pads(mode, kp, 4, pg, value, key_row(), key_lit());
    } else if (zoom == OVERVIEW) {
        for (unsigned k = 0; k < 4; ++k) knob_text(&mix_knob[page[0]][k], mix_val[page[0]][k], v[k]);
        px_chrome_pads(mode, mix_page, MIX_PAGES, page[0], value, key_row(), key_lit());
    } else if (zoom == T_DRUMS) {
        unsigned n; const struct px_page *pages = cr78_voice_pages(&n);
        for (unsigned k = 0; k < 4; ++k) cr78_voice_text(voice, page[scope()], k, v[k]);
        px_chrome_pads(mode, pages, n, page[scope()], value, key_row(), key_lit());
    } else if (zoom == T_BASS) {
        for (unsigned k = 0; k < 4; ++k) knob_text(&bass_knob[page[scope()]][k], bass_val[page[scope()]][k], v[k]);
        px_chrome_pads(mode, bass_page, BASS_PAGES, page[scope()], value, key_row(), key_lit());
    }
    px_print(mode, 2, 1, tint);   /* the track's colour on its name */
    /* the line under the header: what is selected, and what is going on */
    char line[24];
    if (zoom == T_DRUMS) px_print(cr78_voice_name(voice), 2, 8, cr78_voice_color(voice));
    else if (zoom == T_BASS) {
        note_text(bass_note, line); strcat(line, " "); strcat(line, pitch_name[mix_val[3][0]]); strcat(line, " "); strcat(line, scale_name[mix_val[3][1]]);
        px_print(line, 2, 8, PX_GREEN);
    } else if (zoom == T_KEYS) {
        uint8_t ch[GBX_CHORD]; chord_notes(keys_chord, keys_oct, ch); chord_text(ch, line);
        strcat(line, " "); strcat(line, mysynth_patch_name(mysynth_patch_index()));
        px_print(line, 2, 8, PX_CYAN);
    } else if (zoom == T_LEAD) {
        note_text(lead_note, line); strcat(line, " "); strcat(line, mysynth_patch_name(mysynth_lead_patch_index()));
        px_print(line, 2, 8, PX_PINK);
    } else if (zoom == OVERVIEW) {   /* the pattern playing, and the one queued */
        strcpy(line, "PATTERN "); px_number(gbx_pattern_now + 1, line + 8);
        if (gbx_pattern_next != GBX_NONE) { strcat(line, " NEXT "); px_number(gbx_pattern_next + 1, line + strlen(line)); }
        px_print(line, 2, 8, gbx_pattern_next != GBX_NONE ? PX_GOLD : PX_SLATE);
    }
    if (rec) px_print("REC", PX_W - 13, 8, PX_RED);
    else if (zoom >= 0 && muted[zoom]) px_print("MUTE", PX_W - 17, 8, PX_RED);
    else if (pads_latched[scope()]) {
        char on[16] = ""; unsigned n = 0;
        for (unsigned i = 0; i < PX_PADS && n < 3; ++i) if ((pads_latched[scope()] >> i) & 1U) { if (n++) strcat(on, " "); strcat(on, pad_name[i]); }
        px_print(on, PX_W - 1 - ((int)strlen(on) * 4 - 1), 8, PX_EMBER);
    }
    if (zoom == OVERVIEW) draw_overview();
    else if (zoom == T_DRUMS) cr78_draw_kit(voice, now);
    else if (zoom == T_BASS) draw_roll(&gbx_bass, bass_note, PX_GREEN, 16, 58, 1);
    else if (zoom == T_KEYS) draw_chords(16, 58, 1);
    else draw_roll(&gbx_lead, lead_note, PX_PINK, 16, 58, 1);
    px_flash_draw(now);
}
static void leds(uint32_t now)
{
    uint32_t want = 0;
    uint16_t steps = zoom == T_DRUMS ? cr78.grid[voice] : zoom == T_BASS ? gbx_bass.on : zoom == T_KEYS ? gbx_keys.on : gbx_lead.on;
    if (zoom != OVERVIEW)   /* the track's steps, the playhead blinking */
        for (unsigned s = 0; s < 16; ++s) {
            unsigned on = (steps >> s) & 1U;
            if (cr78_running && s == cr78_playhead) on = !on || ((now / 60U) & 1U);
            if (on) want |= 1U << s;
        }
    else if (zoom == OVERVIEW) {   /* the pattern playing; the one queued blinking */
        want |= 1U << gbx_pattern_now;
        if (gbx_pattern_next != GBX_NONE && ((now / 120U) & 1U)) want |= 1U << gbx_pattern_next;
    }
    want |= key_lit() << 16;
    for (unsigned s = 0; s < 16; ++s)
        if (((want ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (want >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((want ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (want >> (16 + i)) & 1U);
    led_state = want;
}
static void flash_view(uint32_t now) { px_flash(zoom == OVERVIEW ? "OVERVIEW" : track_name[zoom], now, 600U); }

unsigned gbx_active(void) { return active; }

int gbx_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys;
    unsigned home = px_mode_toggle(pressed, PX_MODE_GBX, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!home) return 0;
        cr78_kit_ready();
        mysynth_ready();
        mysynth_lead_ready();
        if (!initialized) init();
        px_leds_off(); led_state = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U; zoom = OVERVIEW;
        apply();
        flash_view(now);
        return 1;
    }
    if (home) {   /* HOME: one level up */
        for (unsigned i = 0; i < SCOPES; ++i) pads_held[i] = 0;
        fx_held = 0; held_steps = 0;
        if (zoom != OVERVIEW) { zoom = OVERVIEW; apply(); flash_view(now); full_redraw = 1; return 1; }
        apply(); px_leds_off(); active = 0;
        return 2;
    }
    unsigned changed = 0;
    const int16_t *e = c->encoders;
    /* ALGORITHM: along OVERVIEW, DRUMS, BASS, KEYS, LEAD */
    if (e[1]) {
        int z = clamp(zoom + e[1], OVERVIEW, TRACKS - 1);
        if (z != zoom) { pads_held[scope()] = 0; zoom = z; apply(); flash_view(now); }
        changed = 1;
    }
    /* REC: live playing goes into the steps */
    if ((pressed & PX_BIT(PX_REC)) && !fx_held) { rec ^= 1U; px_flash(rec ? "REC ON" : "REC OFF", now, 500U); changed = 1; }
    /* FX: tapped, mute (the overview: all tracks); held, the black keys throw effects */
    if (pressed & PX_BIT(PX_FX)) { fx_held = 1; fx_used = 0; changed = 1; }
    if (fx_held && !(c->buttons & PX_BIT(PX_FX))) {
        fx_held = 0; pads_held[scope()] = 0; changed = 1;
        if (!fx_used) {
            if (zoom == OVERVIEW) { unsigned all = muted[0] && muted[1] && muted[2] && muted[3]; for (unsigned t = 0; t < TRACKS; ++t) muted[t] = !all; px_flash(all ? "ALL ON" : "ALL MUTED", now, 600U); }
            else { muted[zoom] ^= 1U; px_flash(muted[zoom] ? "MUTE" : "UNMUTE", now, 600U); }
        }
        apply();
    }
    /* OCT -/+: the black keys' octave (BASS, KEYS) */
    if (zoom == T_KEYS && !held_steps) {
        if ((pressed & PX_BIT(PX_OCT_DOWN)) && keys_oct > 1) { --keys_oct; px_flash("OCT DOWN", now, 400U); changed = 1; }
        if ((pressed & PX_BIT(PX_OCT_UP)) && keys_oct < 5) { ++keys_oct; px_flash("OCT UP", now, 400U); changed = 1; }
    }
    if (zoom == T_BASS && !held_steps) {
        if ((pressed & PX_BIT(PX_OCT_DOWN)) && bass_oct > 0) { --bass_oct; px_flash(bass_oct ? "OCT DOWN" : "LOWEST", now, 400U); changed = 1; }
        if ((pressed & PX_BIT(PX_OCT_UP)) && bass_oct < 3) { ++bass_oct; px_flash("OCT UP", now, 400U); changed = 1; }
    }
    if (zoom == T_LEAD && !held_steps) {
        if ((pressed & PX_BIT(PX_OCT_DOWN)) && lead_oct > 2) { --lead_oct; px_flash("OCT DOWN", now, 400U); changed = 1; }
        if ((pressed & PX_BIT(PX_OCT_UP)) && lead_oct < 6) { ++lead_oct; px_flash("OCT UP", now, 400U); changed = 1; }
    }
    /* white keys */
    held_steps = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned key = white_keys[s];
        if (zoom != OVERVIEW && ((c->keys >> key) & 1U)) held_steps |= (uint16_t)(1U << s);
        if (!((down >> key) & 1U)) continue;
        uint16_t bit = (uint16_t)(1U << s);
        if (zoom == OVERVIEW) {   /* a pattern, from the next bar; an empty slot starts as a copy */
            if (s != gbx_pattern_now || gbx_pattern_next != GBX_NONE) {
                unsigned fresh = !((patterns_used >> s) & 1U);
                gbx_pattern_queue(s, fresh);
                patterns_used |= bit;
                static char t[16]; strcpy(t, "PATTERN "); px_number((int)s + 1, t + 8); if (fresh) strcat(t, " NEW");
                px_flash(t, now, 700U);
            }
        } else if (zoom == T_DRUMS) {
            if (c->buttons & PX_BIT(PX_ENV)) cr78.accent ^= bit;
            else {
                cr78.grid[voice] ^= bit;
                if (cr78.grid[voice] & bit) { cr78.ratchet[voice][s] = 1; if (!cr78_running) (void)cr78_audition(voice, 0); }
            }
        } else if (zoom == T_KEYS) {
            if (c->buttons & PX_BIT(PX_ENV)) gbx_keys.accent ^= bit;
            else if (gbx_keys.on & bit) gbx_keys.on &= (uint16_t)~bit;
            else {   /* a new step gets the chord you last played */
                uint8_t ch[GBX_CHORD]; chord_notes(keys_chord, keys_oct, ch);
                for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
                gbx_keys.gate[s] = 14; gbx_keys.vel[s] = 90; gbx_keys.on |= bit;
                if (!cr78_running) (void)gbx_chord_audition(ch, 11025U);
            }
        } else if (zoom == T_BASS) {
            if (c->buttons & PX_BIT(PX_ENV)) gbx_bass.accent ^= bit;
            else if (gbx_bass.on & bit) gbx_bass.on &= (uint16_t)~bit;
            else {   /* a new step gets the note you last played */
                gbx_bass.note[s] = (uint8_t)bass_note; gbx_bass.gate[s] = 10; gbx_bass.vel[s] = 100;
                gbx_bass.on |= bit;
                if (!cr78_running) (void)gbx_audition(bass_note);
            }
        } else if (zoom == T_LEAD) {
            if (c->buttons & PX_BIT(PX_ENV)) gbx_lead.accent ^= bit;
            else if (gbx_lead.on & bit) gbx_lead.on &= (uint16_t)~bit;
            else {
                gbx_lead.note[s] = (uint8_t)lead_note; gbx_lead.gate[s] = 12; gbx_lead.vel[s] = 100;
                gbx_lead.on |= bit;
                if (!cr78_running) (void)gbx_lead_audition(lead_note, 9000U);
            }
        }
        changed = 1;
    }
    /* black keys */
    uint32_t held = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) {
        unsigned key = black_keys[i], hit = (down >> key) & 1U;
        if (fx_held) {   /* the pads */
            if (hit) {
                fx_used = 1; changed = 1;
                if (c->buttons & PX_BIT(PX_REC)) { pads_latched[scope()] ^= 1U << i; px_flash((pads_latched[scope()] >> i) & 1U ? "LATCH" : "OFF", now, 500U); }
                else px_flash(pad_name[i], now, 400U);
            }
            if (((c->keys >> key) & 1U) && !(c->buttons & PX_BIT(PX_REC))) held |= 1U << i;
            continue;
        }
        if (!hit) continue;
        changed = 1;
        if (zoom == OVERVIEW) {   /* mute a track */
            if (i < TRACKS) { muted[i] ^= 1U; static char t[12]; strcpy(t, track_short[i]); strcat(t, muted[i] ? " OFF" : " ON"); px_flash(t, now, 500U); apply(); }
        } else if (zoom == T_DRUMS) {
            unsigned v = cr78_black_voice(i);
            if (held_steps) {   /* a held step: that voice's hit on it */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) cr78.grid[v] ^= (uint16_t)(1U << s);
            } else {
                voice = v;
                if (rec && cr78_running) { cr78.grid[v] |= (uint16_t)(1U << cr78_playhead); cr78.ratchet[v][cr78_playhead] = 1; }
                if (!cr78_running || rec) (void)cr78_audition(v, 1);
            }
        } else if (zoom == T_KEYS) {
            uint8_t ch[GBX_CHORD]; chord_notes(i, keys_oct, ch);
            if (held_steps) {   /* a held step: its chord */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) {
                    for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
                    gbx_keys.on |= (uint16_t)(1U << s);
                }
                (void)gbx_chord_audition(ch, 11025U);
            } else {
                keys_chord = i;
                if (rec && cr78_running) {   /* live: into the step playing */
                    unsigned s = cr78_playhead;
                    for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
                    gbx_keys.gate[s] = 14; gbx_keys.vel[s] = 90; gbx_keys.on |= (uint16_t)(1U << s);
                }
                if (!cr78_running || rec) (void)gbx_chord_audition(ch, 11025U);
            }
        } else if (zoom == T_BASS) {
            unsigned note = key_note(i, bass_oct);
            if (held_steps) {   /* a held step: its note */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) { gbx_bass.note[s] = (uint8_t)note; gbx_bass.on |= (uint16_t)(1U << s); }
                (void)gbx_audition(note);
            } else {
                bass_note = note;
                if (rec && cr78_running) {   /* live: into the step playing */
                    unsigned s = cr78_playhead;
                    gbx_bass.note[s] = (uint8_t)note; gbx_bass.gate[s] = 10; gbx_bass.vel[s] = 100; gbx_bass.on |= (uint16_t)(1U << s);
                }
                if (!cr78_running || rec) (void)gbx_audition(note);
            }
        } else if (zoom == T_LEAD) {
            unsigned note = key_note(i, lead_oct);
            if (held_steps) {
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) { gbx_lead.note[s] = (uint8_t)note; gbx_lead.on |= (uint16_t)(1U << s); }
                (void)gbx_lead_audition(note, 9000U);
            } else {
                lead_note = note;
                if (rec && cr78_running) {   /* live: into the step playing */
                    unsigned s = cr78_playhead;
                    gbx_lead.note[s] = (uint8_t)note; gbx_lead.gate[s] = 12; gbx_lead.vel[s] = 100; gbx_lead.on |= (uint16_t)(1U << s);
                }
                if (!cr78_running || rec) (void)gbx_lead_audition(note, 9000U);
            }
        }
    }
    if (held != pads_held[scope()] || (fx_held && changed)) { pads_held[scope()] = held; apply(); }
    /* SELECT: the view's pages; PRESETS: the drum pattern */
    if (e[0]) {
        unsigned n = 1;
        if (zoom == OVERVIEW) n = MIX_PAGES; else if (zoom == T_DRUMS) (void)cr78_voice_pages(&n); else if (zoom == T_BASS) n = BASS_PAGES; else if (zoom == T_KEYS) n = 4; else (void)mysynth_lead_pages(&n);
        unsigned *pg = &page[scope()];
        *pg = (unsigned)clamp((int)*pg + e[0], 0, (int)n - 1);
        if (zoom == OVERVIEW) px_flash(mix_page[*pg].name, now, 600U);
        else if (zoom == T_DRUMS) { unsigned m; px_flash(cr78_voice_pages(&m)[*pg].name, now, 600U); }
        else if (zoom == T_BASS) px_flash(bass_page[*pg].name, now, 600U);
        else if (zoom == T_KEYS) { unsigned m; px_flash(*pg < 3 ? mysynth_pages(&m)[*pg].name : keys_chord_page.name, now, 600U); }
        else { unsigned m; px_flash(mysynth_lead_pages(&m)[*pg].name, now, 600U); }
        changed = 1;
    }
    if (e[6] && zoom == T_KEYS) {
        unsigned p = (unsigned)(((int)mysynth_patch_index() + e[6] + 8 * (int)mysynth_patch_count()) % (int)mysynth_patch_count());
        mysynth_patch(p); px_flash(mysynth_patch_name(p), now, 700U); changed = 1;
    }
    if (e[6] && zoom == T_LEAD) {
        unsigned p = (unsigned)(((int)mysynth_lead_patch_index() + e[6] + 8 * (int)mysynth_patch_count()) % (int)mysynth_patch_count());
        mysynth_lead_patch(p); px_flash(mysynth_patch_name(p), now, 700U); changed = 1;
    }
    if (e[6] && zoom <= T_DRUMS) {
        unsigned p = (unsigned)(((int)cr78_preset_index() + e[6] + 8 * (int)cr78_preset_count()) % (int)cr78_preset_count());
        cr78_preset(p); mix_val[0][1] = 50; px_flash(cr78_preset_name(p), now, 700U); changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t *k = e + 2;
    if (k[0] || k[1] || k[2] || k[3]) {
        changed = 1;
        unsigned last = k[3] ? 3U : k[2] ? 2U : k[1] ? 1U : 0U;
        char value[12];
        if (zoom == T_DRUMS && held_steps) {   /* a held step: ratchet, accent */
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                if (k[0]) cr78_set_ratchet(voice, s, (int)cr78.ratchet[voice][s] + k[0]);
                if (k[1]) cr78.accent = (uint16_t)(k[1] > 0 ? cr78.accent | (1U << s) : cr78.accent & ~(1U << s));
            }
        } else if ((zoom == T_BASS || zoom == T_LEAD) && held_steps) {   /* a held step: length, velocity, slide, an octave up or down */
            struct gbx_line *l = zoom == T_BASS ? &gbx_bass : &gbx_lead;
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                if (k[0]) l->gate[s] = (uint8_t)clamp(l->gate[s] + k[0], 1, zoom == T_BASS ? 16 : 64);
                if (k[1]) l->vel[s] = (uint8_t)clamp(l->vel[s] + k[1] * 8, 1, 127);
                if (k[2]) l->slide = (uint16_t)(k[2] > 0 ? l->slide | (1U << s) : l->slide & ~(1U << s));
                if (k[3]) l->note[s] = (uint8_t)clamp(l->note[s] + (k[3] > 0 ? 12 : -12), 24, zoom == T_BASS ? 72 : 108);
                l->on |= (uint16_t)(1U << s);
            }
        } else if (zoom == T_KEYS && held_steps) {   /* a held step: length, velocity, an octave up or down */
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                if (k[0]) gbx_keys.gate[s] = (uint8_t)clamp(gbx_keys.gate[s] + k[0], 1, 64);
                if (k[1]) gbx_keys.vel[s] = (uint8_t)clamp(gbx_keys.vel[s] + k[1] * 8, 1, 127);
                if (k[3]) for (unsigned j = 0; j < GBX_CHORD; ++j)
                    if (gbx_keys.note[s][j]) gbx_keys.note[s][j] = (uint8_t)clamp(gbx_keys.note[s][j] + (k[3] > 0 ? 12 : -12), 24, 96);
                gbx_keys.on |= (uint16_t)(1U << s);
            }
        } else if (zoom == T_KEYS) {
            unsigned pg = page[scope()];
            if (pg < 3) {
                for (unsigned i = 0; i < 4; ++i) if (k[i]) mysynth_turn(pg, i, k[i]);
                unsigned m; const struct px_page *mp = mysynth_pages(&m);
                if (mp[pg].knob[last][0]) { mysynth_knob_text(pg, last, value); readout("KEY", mp[pg].knob[last], value, now); }
            } else {
                if (k[0]) keys_size = (int16_t)clamp(keys_size + k[0], 3, 4);
                if (k[1]) keys_strum = (int16_t)clamp(keys_strum + k[1] * 5, 0, 80);
                apply();
                if (last == 0) readout("KEY", "SIZE", keys_size == 3 ? "TRIAD" : "SEVENTH", now);
                else if (last == 1) { px_number(keys_strum, value); strcat(value, "MS"); readout("KEY", "STRM", value, now); }
            }
            if (!cr78_running && pg < 3) { uint8_t ch[GBX_CHORD]; chord_notes(keys_chord, keys_oct, ch); (void)gbx_chord_audition(ch, 11025U); }
        } else if (zoom == OVERVIEW) {
            unsigned pg = page[0];
            for (unsigned i = 0; i < 4; ++i) {
                const struct knob *kn = &mix_knob[pg][i];
                if (!k[i] || !kn->label[0]) continue;
                if (kn->kind == K_TEMPO) { groove_tempo_edit(k[i]); continue; }
                int x = clamp(mix_val[pg][i] + k[i] * kn->step, kn->lo, kn->hi);
                if (kn->kind == K_FILTER && x > -3 && x < 3) x = 0;
                mix_val[pg][i] = (int16_t)x;
            }
            apply();
            if (mix_knob[pg][last].label[0]) { knob_text(&mix_knob[pg][last], mix_val[pg][last], value); readout(0, mix_knob[pg][last].label, value, now); }
        } else if (zoom == T_DRUMS) {
            unsigned pg = page[scope()];
            for (unsigned i = 0; i < 4; ++i) if (k[i]) cr78_voice_turn(voice, pg, i, k[i]);
            if (!cr78_running) (void)cr78_audition(voice, 0);
            if (cr78_voice_knob(pg, last)[0]) { cr78_voice_text(voice, pg, last, value); readout(cr78_voice_short(voice), cr78_voice_knob(pg, last), value, now); }
        } else if (zoom == T_BASS) {
            unsigned pg = page[scope()];
            for (unsigned i = 0; i < 4; ++i) {
                const struct knob *kn = &bass_knob[pg][i];
                if (k[i]) bass_val[pg][i] = (int16_t)clamp(bass_val[pg][i] + k[i] * kn->step, kn->lo, kn->hi);
            }
            build_bass();
            if (!cr78_running) (void)gbx_audition(bass_note);
            knob_text(&bass_knob[pg][last], bass_val[pg][last], value); readout("BAS", bass_knob[pg][last].label, value, now);
        } else if (zoom == T_LEAD) {
            unsigned pg = page[scope()];
            for (unsigned i = 0; i < 4; ++i) if (k[i]) mysynth_lead_turn(pg, i, k[i]);
            unsigned m; const struct px_page *lp = mysynth_lead_pages(&m);
            if (lp[pg].knob[last][0]) { mysynth_lead_knob_text(pg, last, value); readout("LED", lp[pg].knob[last], value, now); }
            if (!cr78_running) (void)gbx_lead_audition(lead_note, 9000U);
        }
    }
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
        leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
