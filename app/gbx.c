/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX and GROOVE MINI panel modes. The rules are GROOVEBOX-MANUAL.md; in short:
 *
 *   LAUNCHER  --ALGORITHM-->  OVERVIEW  --ALGORITHM-->  TRACK (DRUMS BASS KEYS LEAD; MINI: DRUMS BASS)
 *             <----HOME----             <----HOME----
 *
 *   SEQ               the keybed's layer: STEP (white = the steps, black = the parts) or PLAY (white = the instrument)
 *   hold a step       + a black key: that part on it; + KNOB 1-4: its details
 *   FUNC (GLO)        + REC copy, + ARP paste, + PLAY clear (again: undo); + keys while recording: erase
 *   FX held           the black keys are the effect pads (REC too: latch)
 *   REC               what you play is written (REC + PLAY: play and record); early hits go on the coming step
 *   SAVE              the song into Flash
 * Overview: white keys the 16 patterns (held + others: a chain), black keys 1-4 mute the tracks. */
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
static const char *const pad_short[PX_PADS] = {"16", "32", "HF", "RV", "LP", "HP", "CR", "GT", "DR", "PM", "SW"};   /* (the strip: two letters) */
enum { SCOPES = TRACKS + 1 };   /* [0] the mix, [1 + t] track t */
static uint32_t pads_held[SCOPES], pads_latched[SCOPES];

static unsigned active, initialized, full_redraw, voice, fx_held, rec, muted[TRACKS];
static int zoom = OVERVIEW;                  /* OVERVIEW or the track zoomed into */
static unsigned page[SCOPES];                /* each view's SELECT page */
static unsigned bass_oct = 2, bass_note = 45; /* the black keys' octave; the note new steps get */
static unsigned keys_oct = 3, keys_chord = 0;  /* the black keys' octave; the chord (a black key) new steps get */
static unsigned lead_oct = 4, lead_note = 69;  /* the black keys' octave; the note new steps get */
static int16_t keys_size = 3, keys_strum = 0;  /* notes in a chord (3 triad, 4 seventh); ms between them */
static const struct px_page keys_chord_page = {"CHORD", {"SIZE", "STRM", "", ""}};
static uint32_t prev_buttons, prev_keys, last_ms, led_state;
static uint16_t held_steps;

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
static void build_bass_from(const int16_t bass_val[BASS_PAGES][4])
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
static void build_bass(void) { build_bass_from(bass_val); }

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

/* ---- GROOVE MINI: the same instrument, cut down to DRUMS (kick, snare, hat) and BASS ---- */
static unsigned mini;   /* the mode on the panel is GROOVE MINI */
static const uint8_t mini_voices[3] = {0, 1, 3};   /* KICK SNARE HAT CL (the CR-78's voice numbers) */
/* PLAY's drum keys, low to high: kicks, snares, skins, then the metals */
static const uint8_t play_map[16] = {0, 2, 1, 14, 13, 12, 10, 7, 15, 11, 9, 8, 3, 4, 6, 5};
static unsigned tracks_n(void) { return mini ? 2U : TRACKS; }
static unsigned drum_parts(void) { return mini ? 3U : PX_PADS; }
static unsigned drum_part(unsigned i) { return mini ? mini_voices[i % 3U] : cr78_black_voice(i); }
static unsigned play_voice(unsigned s) { return mini ? mini_voices[s * 3U / 16U] : play_map[s & 15U]; }
static uint16_t mini_mask(void) { return (uint16_t)((1U << 0) | (1U << 1) | (1U << 3)); }
/* starter beats (PRESETS in GROOVE MINI): kick, snare, hat */
static const struct { const char *name, *kick, *snare, *hat; } starters[] = {
    {"BASIC", "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x."},
    {"HOUSE", "x...x...x...x...", "....x.......x...", "..x...x...x...x."},
    {"BOOM BAP", "x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x."},
    {"BREAK", "x.........x.....", "....x.......x..x", "x.xxx.x.x.xxx.x."},
};
enum { STARTERS = sizeof starters / sizeof starters[0] };
static unsigned starter;
static uint16_t bits_of(const char *row) { uint16_t b = 0; for (unsigned s = 0; s < 16U && row[s]; ++s) if (row[s] == 'x') b |= (uint16_t)(1U << s); return b; }
static void load_starter(unsigned i)
{
    starter = i % STARTERS;
    for (unsigned v = 0; v < CR78_VOICES; ++v) { cr78.grid[v] = 0; for (unsigned s = 0; s < 16; ++s) cr78.ratchet[v][s] = 1; }
    cr78.accent = 0;
    cr78.grid[0] = bits_of(starters[starter].kick); cr78.grid[1] = bits_of(starters[starter].snare); cr78.grid[3] = bits_of(starters[starter].hat);
}
static unsigned preset_count(void) { return mini ? STARTERS : cr78_preset_count(); }
static unsigned preset_index(void) { return mini ? starter : cr78_preset_index(); }
static const char *preset_name(unsigned p) { return mini ? starters[p % STARTERS].name : cr78_preset_name(p); }
static void preset_load(unsigned p) { if (mini) load_starter(p); else cr78_preset(p); mix_val[0][1] = 50; }

/* ---- state of the panel ---- */
static unsigned play_layer;                  /* the keybed in a track: 0 STEP, 1 PLAY */
static uint8_t chain_build[GBX_CHAIN_MAX];
static unsigned chain_n;
static int chain_anchor = -1;                /* the overview's pattern key held (a chain is being made) */
static struct gbx_pattern clip, undo, scratch;
static unsigned clip_what = GBX_NONE, undo_what = GBX_NONE;   /* what the clipboard holds; what undo puts back */
enum { V_NONE, V_COPY, V_PASTE, V_CLEAR };
static unsigned last_verb;                   /* the verb just used (the same again undoes it) */
static uint32_t led_buttons;

/* ---- screens ---- */
static unsigned scope(void) { return zoom == OVERVIEW ? 0U : 1U + (unsigned)zoom; }
static const char *const *key_row(void)
{
    static const char *const lanes[PX_PADS] = {"DR", "BS", "KY", "LD"};   /* (the strip: two letters) */
    static const char *const mini_lanes[PX_PADS] = {"DR", "BS"};
    static const char *const none[PX_PADS] = {""};
    static char notes[PX_PADS][6];
    static const char *note_row[PX_PADS];
    if (fx_held) return pad_short;
    if (zoom == OVERVIEW) return mini ? mini_lanes : lanes;
    if (play_layer) return none;   /* PLAY: the white keys play; the black keys rest */
    if (zoom == T_DRUMS) {
        if (!mini) return cr78_voice_row();
        for (unsigned i = 0; i < PX_PADS; ++i) note_row[i] = i < 3U ? cr78_voice_short(mini_voices[i]) : "";
        return note_row;
    }
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
    if (zoom == OVERVIEW) { uint32_t m = 0; for (unsigned t = 0; t < tracks_n(); ++t) if (muted[t]) m |= 1U << t; return m; }
    if (play_layer) return 0;
    if (zoom == T_DRUMS) for (unsigned i = 0; i < drum_parts(); ++i) if (drum_part(i) == voice) return 1U << i;
    if (zoom == T_BASS) for (unsigned i = 0; i < PX_PADS; ++i) if (key_note(i, bass_oct) == bass_note) return 1U << i;
    if (zoom == T_KEYS) return keys_chord < PX_PADS ? 1U << keys_chord : 0;
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
/* the drums in GROOVE MINI: three rows of steps, the voice chosen bright */
static void draw_mini_drums(void)
{
    unsigned head = cr78_playhead, run = cr78_running;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned v = mini_voices[i], y = 18U + i * 13U, on = v == voice;
        px_print(cr78_voice_short(v), 2, (int)y + 2, on ? PX_WHITE : cr78_voice_color(v));
        for (unsigned s = 0; s < 16; ++s) {
            int x = 18 + (int)s * 4;
            if (run && s == head) px_rect(x - 1, (int)y - 1, 5, 11, PX_DUSK);
            if ((cr78.grid[v] >> s) & 1U) {
                unsigned c = run && s == head ? PX_WHITE : (cr78.accent >> s) & 1U ? PX_GOLD : on ? PX_EMBER : PX_SLATE;
                int h = cr78.ratchet[v][s] > 1 ? 9 : 7;
                px_rect(x, (int)y + 9 - h, 3, h, c);
            } else px_pset(x + 1, (int)y + 8, PX_DUSK);
        }
    }
}
static void draw_overview(void)
{
    /* a lane a track, one bar each */
    unsigned head = cr78_playhead, run = cr78_running;
    for (unsigned t = 0; t < tracks_n(); ++t) {
        int y = 16 + (int)t * 12;
        unsigned c = muted[t] ? PX_DUSK : track_color[t];
        px_print(track_short[t], 2, y + 2, c);
        if (t == T_BASS) { draw_roll(&gbx_bass, bass_note, PX_GREEN, y, y + 9, 0); continue; }
        if (t == T_KEYS) { draw_chords(y, y + 9, 0); continue; }
        if (t == T_LEAD) { draw_roll(&gbx_lead, lead_note, PX_PINK, y, y + 9, 0); continue; }
        for (unsigned s = 0; s < 16; ++s) {   /* DRUMS: taller where more voices hit */
            unsigned n = 0;
            for (unsigned v = 0; v < CR78_VOICES; ++v) if (!mini || ((mini_mask() >> v) & 1U)) n += (cr78.grid[v] >> s) & 1U;
            int x = 16 + (int)s * 4, h = n > 4 ? 9 : (int)n * 2 + 1;
            if (run && s == head) px_rect(x - 1, y - 1, 5, 11, PX_DUSK);
            if (n) px_rect(x, y + 9 - h, 3, h, run && s == head ? PX_WHITE : c);
            else px_pset(x + 1, y + 8, PX_DUSK);
        }
    }
}
static void velo_text(unsigned vel, unsigned acc, char *out) { if (acc) strcpy(out, "ACC"); else px_number((int)vel, out); }
static const char *mix_label(unsigned pg, unsigned k) { return mini && pg == 2U && k >= 2U ? "" : mix_knob[pg][k].label; }
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    const char *mode = zoom == OVERVIEW ? (mini ? "MINI" : "GBX") : track_name[zoom];
    unsigned tint = zoom == OVERVIEW ? PX_GOLD : track_color[zoom];
    unsigned st = 0;
    if (held_steps) while (!((held_steps >> st) & 1U)) ++st;
    static char title[12];
    if (held_steps) { strcpy(title, "STEP "); px_number((int)st + 1, title + 5); }
    v[0][0] = v[1][0] = v[2][0] = v[3][0] = 0;
    if (zoom == T_DRUMS && held_steps) {   /* a held drum step: its ratchet and accent */
        struct px_page step = {title, {"RTCH", "ACC", "", ""}};
        px_number(cr78.ratchet[voice][st], v[0]); strcpy(v[1], (cr78.accent >> st) & 1U ? "ON" : "OFF");
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if ((zoom == T_BASS || zoom == T_LEAD) && held_steps) {   /* length, velocity (then accent), slide, octave */
        const struct gbx_line *l = zoom == T_BASS ? &gbx_bass : &gbx_lead;
        struct px_page step = {title, {"LEN", "VELO", "SLDE", "NOTE"}};
        px_number(l->gate[st], v[0]); strcat(v[0], "/16"); velo_text(l->vel[st], (l->accent >> st) & 1U, v[1]);
        strcpy(v[2], (l->slide >> st) & 1U ? "ON" : "OFF"); note_text(l->note[st], v[3]);
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if (zoom == T_KEYS && held_steps) {   /* length, velocity (then accent), octave */
        struct px_page step = {title, {"LEN", "VELO", "", "OCT"}};
        px_number(gbx_keys.gate[st], v[0]); strcat(v[0], "/16"); velo_text(gbx_keys.vel[st], (gbx_keys.accent >> st) & 1U, v[1]);
        chord_text(gbx_keys.note[st], v[3]);
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if (zoom == T_LEAD) {
        unsigned n; const struct px_page *lp = mysynth_lead_pages(&n);
        for (unsigned k = 0; k < 4; ++k) mysynth_lead_knob_text(page[scope()], k, v[k]);
        px_chrome_pads(mode, lp, n, page[scope()], value, key_row(), key_lit());
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
        }
        px_chrome_pads(mode, kp, 4, pg, value, key_row(), key_lit());
    } else if (zoom == OVERVIEW) {
        static struct px_page mp[MIX_PAGES];
        for (unsigned pg = 0; pg < MIX_PAGES; ++pg) { mp[pg].name = mix_page[pg].name; for (unsigned k = 0; k < 4; ++k) mp[pg].knob[k] = mix_label(pg, k); }
        for (unsigned k = 0; k < 4; ++k) if (mix_label(page[0], k)[0]) knob_text(&mix_knob[page[0]][k], mix_val[page[0]][k], v[k]);
        px_chrome_pads(mode, mp, MIX_PAGES, page[0], value, key_row(), key_lit());
    } else if (zoom == T_DRUMS) {
        unsigned n; const struct px_page *pages = cr78_voice_pages(&n);
        for (unsigned k = 0; k < 4; ++k) cr78_voice_text(voice, page[scope()], k, v[k]);
        px_chrome_pads(mode, pages, n, page[scope()], value, key_row(), key_lit());
    } else if (zoom == T_BASS) {
        for (unsigned k = 0; k < 4; ++k) knob_text(&bass_knob[page[scope()]][k], bass_val[page[scope()]][k], v[k]);
        px_chrome_pads(mode, bass_page, BASS_PAGES, page[scope()], value, key_row(), key_lit());
    }
    px_print(mode, 2, 1, tint);   /* the track's colour on its name */
    /* the line under the header: what is chosen, and what is going on */
    char line[28];
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
    } else {   /* the pattern playing, the one queued, or the chain */
        unsigned queued = gbx_pattern_next != GBX_NONE;
        if (gbx_chain_len > 1) {
            strcpy(line, "CHAIN");
            for (unsigned i = 0; i < gbx_chain_len && strlen(line) < 17; ++i) { strcat(line, " "); px_number(gbx_chain[i] + 1, line + strlen(line)); }
        } else {
            strcpy(line, "PATTERN "); px_number(gbx_pattern_now + 1, line + 8);
            if (queued) { strcat(line, " NEXT "); px_number(gbx_pattern_next + 1, line + strlen(line)); }
        }
        px_print(line, 2, 8, queued ? PX_GOLD : PX_SLATE);
    }
    /* right: the layer, recording, a mute or the latched pads */
    if (zoom != OVERVIEW && (rec || play_layer)) {
        strcpy(line, play_layer ? "PLAY" : ""); if (rec) strcat(line, play_layer ? " REC" : "REC");
        px_print(line, PX_W - 1 - ((int)strlen(line) * 4 - 1), 8, rec ? PX_RED : tint);
    } else if (rec) px_print("REC", PX_W - 13, 8, PX_RED);
    else if (zoom >= 0 && muted[zoom]) px_print("MUTE", PX_W - 17, 8, PX_RED);
    else if (pads_latched[scope()]) {
        char on[16] = ""; unsigned n = 0;
        for (unsigned i = 0; i < PX_PADS && n < 3; ++i) if ((pads_latched[scope()] >> i) & 1U) { if (n++) strcat(on, " "); strcat(on, pad_name[i]); }
        px_print(on, PX_W - 1 - ((int)strlen(on) * 4 - 1), 8, PX_EMBER);
    }
    if (zoom == OVERVIEW) draw_overview();
    else if (zoom == T_DRUMS) { if (mini) draw_mini_drums(); else cr78_draw_kit(voice, now); }
    else if (zoom == T_BASS) draw_roll(&gbx_bass, bass_note, PX_GREEN, 16, 58, 1);
    else if (zoom == T_KEYS) draw_chords(16, 58, 1);
    else draw_roll(&gbx_lead, lead_note, PX_PINK, 16, 58, 1);
    px_flash_draw(now);
}
static void leds(uint32_t now)
{
    uint32_t want = 0;
    if (zoom == OVERVIEW) {   /* the pattern playing and the chain's; the one queued blinking */
        want |= 1U << gbx_pattern_now;
        if (gbx_chain_len > 1) for (unsigned i = 0; i < gbx_chain_len; ++i) want |= 1U << gbx_chain[i];
        if (gbx_pattern_next != GBX_NONE) { want &= ~(1U << gbx_pattern_next); if ((now / 120U) & 1U) want |= 1U << gbx_pattern_next; }
    } else if (play_layer) want = 0xFFFFU;   /* PLAY: the keys that play */
    else {   /* STEP: the track's steps, the playhead blinking */
        uint16_t steps = zoom == T_DRUMS ? cr78.grid[voice] : zoom == T_BASS ? gbx_bass.on : zoom == T_KEYS ? gbx_keys.on : gbx_lead.on;
        for (unsigned s = 0; s < 16; ++s) {
            unsigned on = (steps >> s) & 1U;
            if (cr78_running && s == cr78_playhead) on = !on || ((now / 60U) & 1U);
            if (on) want |= 1U << s;
        }
    }
    want |= key_lit() << 16;
    for (unsigned s = 0; s < 16; ++s)
        if (((want ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (want >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((want ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (want >> (16 + i)) & 1U);
    led_state = want;
    /* the buttons: SEQ lit in STEP; REC lit when armed, blinking while it records */
    uint32_t b = 0;
    if (zoom != OVERVIEW && !play_layer) b |= PX_BIT(PX_SEQ);
    if (rec && (!cr78_running || ((now / 150U) & 1U))) b |= PX_BIT(PX_REC);
    for (unsigned i = 0; i < 14; ++i) if (((b ^ led_buttons) >> i) & 1U) fm1_led_set(i, (b >> i) & 1U);
    led_buttons = b;
}
static void flash_view(uint32_t now) { px_flash(zoom == OVERVIEW ? "OVERVIEW" : track_name[zoom], now, 600U); }

/* ---- playing and recording ---- */
/* the step a live hit goes to: the nearest; one just before a step goes on it, and is skipped there once */
static unsigned live_step(unsigned *early)
{
    unsigned s = cr78_playhead;
    *early = (fx_step_clock & 0xFFFFU) >= 0x8000U;
    return *early ? (s + 1U) & 15U : s;
}
static void play_drum(unsigned v)
{
    voice = v;
    if (rec && cr78_running) {
        unsigned early, s = live_step(&early);
        uint16_t bit = (uint16_t)(1U << s);
        cr78.ratchet[v][s] = 1; cr78.grid[v] |= bit;
        if (early) cr78.skip[v] |= bit;
    }
    (void)cr78_audition(v, 1);
}
static void play_note(unsigned t, unsigned note)   /* BASS, LEAD */
{
    struct gbx_line *l = t == T_BASS ? &gbx_bass : &gbx_lead;
    if (t == T_BASS) bass_note = note; else lead_note = note;
    if (rec && cr78_running) {
        unsigned early, s = live_step(&early);
        uint16_t bit = (uint16_t)(1U << s);
        l->note[s] = (uint8_t)note; l->gate[s] = t == T_BASS ? 10 : 12; l->vel[s] = 100; l->on |= bit;
        if (early) l->skip |= bit;
    }
    if (t == T_BASS) (void)gbx_audition(note); else (void)gbx_lead_audition(note, 9000U);
}
static void play_chord_key(unsigned i)   /* KEYS: the chord on scale degree i */
{
    uint8_t ch[GBX_CHORD]; chord_notes(i, keys_oct, ch);
    keys_chord = i;
    if (rec && cr78_running) {
        unsigned early, s = live_step(&early);
        uint16_t bit = (uint16_t)(1U << s);
        for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
        gbx_keys.gate[s] = 14; gbx_keys.vel[s] = 90; gbx_keys.on |= bit;
        if (early) gbx_keys.skip |= bit;
    }
    (void)gbx_chord_audition(ch, 11025U);
}
/* FUNC + keys held while recording: those hits go as the playhead passes them */
static void erase_held(const struct fm1_controls *c)
{
    uint16_t bit = (uint16_t)(1U << cr78_playhead);
    if (zoom == T_DRUMS) {
        for (unsigned i = 0; i < PX_PADS; ++i) if (!play_layer && i < drum_parts() && ((c->keys >> black_keys[i]) & 1U)) cr78.grid[drum_part(i)] &= (uint16_t)~bit;
        for (unsigned s = 0; s < 16; ++s) if (play_layer && ((c->keys >> white_keys[s]) & 1U)) cr78.grid[play_voice(s)] &= (uint16_t)~bit;
    } else if (c->keys) {
        if (zoom == T_BASS) gbx_bass.on &= (uint16_t)~bit;
        else if (zoom == T_LEAD) gbx_lead.on &= (uint16_t)~bit;
        else if (zoom == T_KEYS) gbx_keys.on &= (uint16_t)~bit;
    }
}

/* ---- FUNC: copy, paste, clear (the same again: undo) ---- */
static void verb(unsigned which, uint32_t now)
{
    static const char *const what_name[TRACKS] = {"DRUMS", "BASS", "KEYS", "LEAD"};
    static char text[24];
    unsigned st = 0;
    if (held_steps) while (!((held_steps >> st) & 1U)) ++st;
    unsigned focus = zoom == OVERVIEW ? (unsigned)GBX_ALL : (unsigned)zoom | (held_steps ? GBX_STEP_BIT * (st + 1U) : 0U);
    strcpy(text, zoom == OVERVIEW ? "PATTERN" : what_name[zoom]);
    if (held_steps) { strcat(text, " STEP "); px_number((int)st + 1, text + strlen(text)); }
    if ((which == V_PASTE || which == V_CLEAR) && last_verb == which && undo_what != GBX_NONE) {   /* again: undo */
        gbx_pattern_put(&undo, undo_what); undo_what = GBX_NONE; last_verb = V_NONE;
        px_flash("UNDONE", now, 700U);
        return;
    }
    last_verb = which;
    if (which == V_COPY) {
        gbx_pattern_take(&clip); clip_what = focus;
        if (held_steps) {   /* a step: spread it over all sixteen, so it pastes onto any step */
            struct gbx_pattern one = clip;
            for (unsigned s = 0; s < 16; ++s) {
                uint16_t from = (uint16_t)(1U << st), to = (uint16_t)(1U << s);
                for (unsigned v = 0; v < GBX_DRUM_VOICES; ++v) {
                    clip.grid[v] = (uint16_t)((clip.grid[v] & ~to) | ((one.grid[v] & from) ? to : 0));
                    unsigned r = (one.ratchet[v][st >> 1] >> ((st & 1U) * 4U)) & 15U, sh = (s & 1U) * 4U;
                    clip.ratchet[v][s >> 1] = (uint8_t)((clip.ratchet[v][s >> 1] & ~(15U << sh)) | (r << sh));
                }
                clip.accent = (uint16_t)((clip.accent & ~to) | ((one.accent & from) ? to : 0));
                struct gbx_line_steps *ls[2] = {&clip.bass, &clip.lead}; const struct gbx_line_steps *lo[2] = {&one.bass, &one.lead};
                for (unsigned k = 0; k < 2; ++k) {
                    ls[k]->on = (uint16_t)((ls[k]->on & ~to) | ((lo[k]->on & from) ? to : 0));
                    ls[k]->slide = (uint16_t)((ls[k]->slide & ~to) | ((lo[k]->slide & from) ? to : 0));
                    ls[k]->accent = (uint16_t)((ls[k]->accent & ~to) | ((lo[k]->accent & from) ? to : 0));
                    ls[k]->note[s] = lo[k]->note[st]; ls[k]->gate[s] = lo[k]->gate[st]; ls[k]->vel[s] = lo[k]->vel[st];
                }
                clip.keys_on = (uint16_t)((clip.keys_on & ~to) | ((one.keys_on & from) ? to : 0));
                clip.keys_accent = (uint16_t)((clip.keys_accent & ~to) | ((one.keys_accent & from) ? to : 0));
                for (unsigned k = 0; k < GBX_CHORD; ++k) clip.keys_note[s][k] = one.keys_note[st][k];
                clip.keys_gate[s] = one.keys_gate[st]; clip.keys_vel[s] = one.keys_vel[st];
            }
        }
        strcat(text, " COPIED"); px_flash(text, now, 800U);
        return;
    }
    if (which == V_PASTE) {
        /* the clipboard pastes onto the same kind of thing: a pattern, a track (the same track), a step */
        unsigned ok = clip_what != GBX_NONE &&
            (focus == GBX_ALL ? clip_what == GBX_ALL :
             held_steps ? (clip_what & 15U) == (unsigned)zoom && clip_what >= GBX_STEP_BIT :
             clip_what == GBX_ALL || clip_what == (unsigned)zoom);
        if (!ok) { px_flash("NOTHING TO PASTE", now, 800U); last_verb = V_NONE; return; }
        gbx_pattern_take(&undo); undo_what = GBX_ALL;
        gbx_pattern_put(&clip, focus);
        strcat(text, " PASTED"); px_flash(text, now, 800U);
        return;
    }
    gbx_pattern_take(&undo); undo_what = GBX_ALL;   /* CLEAR */
    scratch = undo;
    gbx_pattern_wipe(&scratch, focus);
    gbx_pattern_put(&scratch, focus);
    strcat(text, " CLEARED"); px_flash(text, now, 800U);
}

/* ---- saving: the song in two copies of two sectors, written in turn; the newer good one loads ---- */
enum { SAVE_MAGIC = 0x31584247U /* GBX1 */, SAVE_VERSION = 1, KIT_MAX = 1088, SAVE_COPY = 2 * FM1_FLASH_SECTOR_BYTES };
struct save_head { uint32_t magic, seq, bytes, crc; };
struct save_meta {
    uint8_t version, now, chain_len, mini_seeded;
    uint16_t used, kit_bytes;
    int16_t mix[MIX_PAGES][4], bass[BASS_PAGES][4], keys_size, keys_strum;
    uint8_t bass_oct, keys_oct, lead_oct, mutes, chain[GBX_CHAIN_MAX];
    uint8_t synth[MYSYNTH_STATE_BYTES];
    uint8_t kit[KIT_MAX];
};
_Static_assert(sizeof(struct save_head) + sizeof(struct save_meta) + sizeof gbx_patterns <= SAVE_COPY, "a save fits two sectors");
static struct save_meta meta;
static uint32_t save_seq;
static unsigned save_slot, mini_seeded;
static int save_room(void)   /* 0, or why the save sectors are not ours to write */
{
    uint32_t header[3];
    if (fm1_flash_size() < GROOVE_FLASH_BYTES) return FM1_FLASH_UNAVAILABLE;
    int r = fm1_flash_read(GROOVE_SAMPLE_OFFSET, header, sizeof header);
    if (r) return r;
    return header[0] == 0x53314d46U && header[2] > GROOVE_SAMPLE_LIMIT ? FM1_FLASH_INVALID : 0;   /* an old, larger sample bank */
}
static int save_song(void)
{
    int r = save_room();
    if (r) return r;
    unsigned kit = cr78_kit_bytes();
    if (kit > KIT_MAX) return FM1_FLASH_INVALID;
    gbx_pattern_take(&gbx_patterns[gbx_pattern_now]);
    memset(&meta, 0, sizeof meta);
    meta.version = SAVE_VERSION; meta.now = gbx_pattern_now; meta.used = gbx_patterns_used; meta.mini_seeded = (uint8_t)mini_seeded;
    meta.chain_len = gbx_chain_len; for (unsigned i = 0; i < GBX_CHAIN_MAX; ++i) meta.chain[i] = gbx_chain[i];
    memcpy(meta.mix, mix_val, sizeof meta.mix); memcpy(meta.bass, bass_val, sizeof meta.bass);
    meta.keys_size = keys_size; meta.keys_strum = keys_strum;
    meta.bass_oct = (uint8_t)bass_oct; meta.keys_oct = (uint8_t)keys_oct; meta.lead_oct = (uint8_t)lead_oct;
    for (unsigned t = 0; t < TRACKS; ++t) if (muted[t]) meta.mutes |= (uint8_t)(1U << t);
    mysynth_state_get(meta.synth);
    meta.kit_bytes = (uint16_t)kit; cr78_kit_get(meta.kit);
    struct save_head head = {SAVE_MAGIC, save_seq + 1U, sizeof meta + sizeof gbx_patterns, 0};
    head.crc = groove_crc32(groove_crc32(~0U, &meta, sizeof meta), gbx_patterns, sizeof gbx_patterns);
    unsigned slot = save_slot ^ 1U;   /* the other copy: the last good save survives a cut-off write */
    uint32_t at = GROOVE_GBX_OFFSET + slot * SAVE_COPY;
    r = fm1_flash_erase(at);
    if (!r) r = fm1_flash_erase(at + FM1_FLASH_SECTOR_BYTES);
    if (!r) r = fm1_flash_write(at + sizeof head, &meta, sizeof meta);
    if (!r) r = fm1_flash_write(at + sizeof head + sizeof meta, gbx_patterns, sizeof gbx_patterns);
    if (!r) r = fm1_flash_write(at, &head, sizeof head);   /* the header last: a copy without one is no save */
    if (!r) { save_seq = head.seq; save_slot = slot; }
    return r;
}
static int copy_good(unsigned slot, struct save_head *head)
{
    uint32_t at = GROOVE_GBX_OFFSET + slot * SAVE_COPY;
    if (fm1_flash_read(at, head, sizeof *head) || head->magic != SAVE_MAGIC || head->bytes != sizeof meta + sizeof gbx_patterns) return 0;
    uint8_t buf[256]; uint32_t crc = ~0U;
    for (uint32_t done = 0; done < head->bytes;) {
        uint32_t n = head->bytes - done > sizeof buf ? sizeof buf : head->bytes - done;
        if (fm1_flash_read(at + sizeof *head + done, buf, n)) return 0;
        crc = groove_crc32(crc, buf, n); done += n;
    }
    return crc == head->crc;
}
static int load_song(void)   /* 1: the song came back */
{
    if (fm1_flash_size() < GROOVE_FLASH_BYTES) return 0;
    struct save_head h[2];
    unsigned good0 = copy_good(0, &h[0]), good1 = copy_good(1, &h[1]);
    if (!good0 && !good1) return 0;
    unsigned slot = good0 && good1 ? (h[1].seq - h[0].seq < 0x80000000U ? 1U : 0U) : good1;
    uint32_t at = GROOVE_GBX_OFFSET + slot * SAVE_COPY + sizeof(struct save_head);
    if (fm1_flash_read(at, &meta, sizeof meta) || meta.version != SAVE_VERSION) return 0;
    if (fm1_flash_read(at + sizeof meta, gbx_patterns, sizeof gbx_patterns)) return 0;
    save_seq = h[slot].seq; save_slot = slot;
    memcpy(mix_val, meta.mix, sizeof meta.mix); memcpy(bass_val, meta.bass, sizeof meta.bass);
    keys_size = meta.keys_size == 4 ? 4 : 3; keys_strum = (int16_t)clamp(meta.keys_strum, 0, 80);
    bass_oct = meta.bass_oct > 3 ? 2 : meta.bass_oct; keys_oct = clamp(meta.keys_oct, 1, 5); lead_oct = clamp(meta.lead_oct, 2, 6);
    for (unsigned t = 0; t < TRACKS; ++t) muted[t] = (meta.mutes >> t) & 1U;
    mysynth_state_set(meta.synth);
    if (meta.kit_bytes == cr78_kit_bytes()) cr78_kit_set(meta.kit);
    build_bass();
    unsigned p = meta.now % GBX_PATTERNS;
    gbx_pattern_put(&gbx_patterns[p], GBX_ALL);
    gbx_pattern_now = (uint8_t)p; gbx_pattern_next = GBX_NONE; gbx_patterns_used = (uint16_t)(meta.used | (1U << p));
    unsigned n = meta.chain_len >= 1 && meta.chain_len <= GBX_CHAIN_MAX ? meta.chain_len : 1;
    for (unsigned i = 0; i < n; ++i) chain_build[i] = meta.chain[i] % GBX_PATTERNS;
    if (n > 1) gbx_chain_set(chain_build, n);
    mini_seeded = meta.mini_seeded;
    return 1;
}

/* ---- the panel ---- */
unsigned gbx_active(void) { return active && !mini; }
unsigned gbx_mini_active(void) { return active && mini; }
static void enter(unsigned as_mini, uint32_t now)
{
    mini = as_mini;
    if (gbx_deck_on) {   /* LOOP DECK had the tracks: our song and sounds back */
        deck_release();
        if (initialized) { gbx_pattern_put(&gbx_patterns[gbx_pattern_now], GBX_ALL); build_bass(); }
    }
    cr78_kit_ready();
    mysynth_ready();
    mysynth_lead_ready();
    if (!initialized) { init(); (void)load_song(); }
    if (mini && !mini_seeded) {   /* GROOVE MINI's first time: a beat and a bass line to start from */
        load_starter(0);
        static const uint8_t line[16] = {45, 0, 0, 45, 0, 0, 45, 0, 43, 0, 45, 0, 48, 0, 47, 0};
        gbx_bass.on = 0;
        for (unsigned s = 0; s < 16; ++s) if (line[s]) { gbx_bass.note[s] = line[s]; gbx_bass.gate[s] = 10; gbx_bass.vel[s] = 100; gbx_bass.on |= (uint16_t)(1U << s); }
        mini_seeded = 1;
    }
    gbx_keys.armed = gbx_lead.armed = (uint8_t)!mini;   /* GROOVE MINI: KEYS and LEAD rest */
    cr78.voice_mute = mini ? (uint16_t)~mini_mask() : 0;
    if (mini && voice != 0 && voice != 1 && voice != 3) voice = 0;
    px_leds_off(); led_state = 0; led_buttons = 0;
    active = 1; full_redraw = 1; last_ms = now - 100U; zoom = OVERVIEW; play_layer = 0; chain_anchor = -1;
    apply();
    flash_view(now);
}
static int control(const struct fm1_controls *c, unsigned as_mini)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys, up = prev_keys & ~c->keys;
    unsigned home = px_mode_toggle(pressed, as_mini ? PX_MODE_MINI : PX_MODE_GBX, active && mini == as_mini);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active || mini != as_mini) {
        if (!home) return 0;
        enter(as_mini, now);
        return 1;
    }
    if (home) {   /* HOME: one level up */
        for (unsigned i = 0; i < SCOPES; ++i) pads_held[i] = 0;
        fx_held = 0; held_steps = 0; chain_anchor = -1;
        if (zoom != OVERVIEW) { zoom = OVERVIEW; apply(); flash_view(now); full_redraw = 1; return 1; }
        apply(); px_leds_off(); led_buttons = 0; active = 0;
        return 2;
    }
    unsigned changed = 0, func = (c->buttons & PX_BIT(PX_GLO)) != 0;
    const int16_t *e = c->encoders;
    /* ALGORITHM: along OVERVIEW and the tracks */
    if (e[1]) {
        int z = clamp(zoom + e[1], OVERVIEW, (int)tracks_n() - 1);
        if (z != zoom) { pads_held[scope()] = 0; zoom = z; chain_anchor = -1; last_verb = V_NONE; apply(); flash_view(now); }
        changed = 1;
    }
    /* FUNC's verbs: + REC copy, + ARP paste, + PLAY clear */
    if (func && (pressed & (PX_BIT(PX_REC) | PX_BIT(PX_ARP) | PX_BIT(PX_PLAY)))) {
        verb(pressed & PX_BIT(PX_REC) ? V_COPY : pressed & PX_BIT(PX_ARP) ? V_PASTE : V_CLEAR, now);
        pressed &= ~(PX_BIT(PX_REC) | PX_BIT(PX_ARP) | PX_BIT(PX_PLAY));
        changed = 1;
    }
    /* SAVE: the song into Flash */
    if (pressed & PX_BIT(PX_SAVE)) {
        int r = save_song();
        px_flash(!r ? "SAVED" : r == FM1_FLASH_INVALID ? "NO ROOM: SAMPLES" : "SAVE FAILED", now, 900U);
        changed = 1;
    }
    /* SEQ: the keybed's layer, STEP or PLAY */
    if ((pressed & PX_BIT(PX_SEQ)) && zoom != OVERVIEW) { play_layer ^= 1U; held_steps = 0; px_flash(play_layer ? "PLAY" : "STEP", now, 500U); changed = 1; }
    /* REC: arm / punch in and out; held REC + PLAY: play and record */
    if ((pressed & PX_BIT(PX_REC)) && !fx_held) { rec ^= 1U; px_flash(rec ? "REC ON" : "REC OFF", now, 500U); changed = 1; }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0};
        groove_get_view(0, 0, &view);
        unsigned rec_play = (c->buttons & PX_BIT(PX_REC)) != 0;
        if (rec_play) rec = 1;
        if (!(rec_play && view.playing)) { const uint8_t command = view.playing ? 0xfc : 0xfb; (void)groove_midi_local(&command, 1); }
        changed = 1;
    }
    /* FX: held, the black keys throw effects (a tap does nothing) */
    if (pressed & PX_BIT(PX_FX)) { fx_held = 1; changed = 1; }
    if (fx_held && !(c->buttons & PX_BIT(PX_FX))) { fx_held = 0; pads_held[scope()] = 0; changed = 1; apply(); }
    /* OCT -/+: the octave the keys play (BASS, KEYS, LEAD) */
    if (zoom > T_DRUMS && !held_steps && (pressed & (PX_BIT(PX_OCT_DOWN) | PX_BIT(PX_OCT_UP)))) {
        int d = (pressed & PX_BIT(PX_OCT_UP)) ? 1 : -1;
        unsigned *o = zoom == T_BASS ? &bass_oct : zoom == T_KEYS ? &keys_oct : &lead_oct;
        int lo = zoom == T_BASS ? 0 : zoom == T_KEYS ? 1 : 2, hi = zoom == T_BASS ? 3 : zoom == T_KEYS ? 5 : 6;
        int n = clamp((int)*o + d, lo, hi);
        if (n != (int)*o) { *o = (unsigned)n; px_flash(d > 0 ? "OCT UP" : "OCT DOWN", now, 400U); }
        changed = 1;
    }
    if (c->keys & ~prev_keys) last_verb = V_NONE;
    /* FUNC + keys while recording: erase */
    if (func && rec && cr78_running && zoom != OVERVIEW) { erase_held(c); down = 0; }
    else if (func) down = 0;   /* (FUNC + a key: nothing else) */
    /* white keys */
    held_steps = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned key = white_keys[s];
        uint16_t bit = (uint16_t)(1U << s);
        if (zoom != OVERVIEW && !play_layer && ((c->keys >> key) & 1U)) held_steps |= bit;
        if (zoom == OVERVIEW && ((up >> key) & 1U) && (int)s == chain_anchor) chain_anchor = -1;
        if (!((down >> key) & 1U)) continue;
        changed = 1;
        if (zoom == OVERVIEW) {   /* a pattern; held + others: a chain */
            static char t[24];
            if (chain_anchor < 0) {
                chain_anchor = (int)s; chain_n = 1; chain_build[0] = (uint8_t)s;
                unsigned fresh = !((gbx_patterns_used >> s) & 1U);
                gbx_chain_set(chain_build, 1);
                strcpy(t, "PATTERN "); px_number((int)s + 1, t + 8); if (fresh) strcat(t, " NEW");
            } else if (chain_n < GBX_CHAIN_MAX) {
                chain_build[chain_n++] = (uint8_t)s;
                gbx_chain_set(chain_build, chain_n);
                strcpy(t, "CHAIN"); for (unsigned i = 0; i < chain_n && strlen(t) < 18; ++i) { strcat(t, " "); px_number(chain_build[i] + 1, t + strlen(t)); }
            }
            px_flash(t, now, 700U);
        } else if (play_layer) {   /* PLAY: the white keys are the instrument */
            if (zoom == T_DRUMS) play_drum(play_voice(s));
            else if (zoom == T_BASS) play_note(T_BASS, key_note(s, bass_oct));
            else if (zoom == T_LEAD) play_note(T_LEAD, key_note(s, lead_oct));
            else play_chord_key(s);
        } else if (zoom == T_DRUMS) {   /* STEP: a step on / off */
            cr78.grid[voice] ^= bit;
            if (cr78.grid[voice] & bit) { cr78.ratchet[voice][s] = 1; if (!cr78_running) (void)cr78_audition(voice, 0); }
        } else if (zoom == T_KEYS) {
            if (gbx_keys.on & bit) gbx_keys.on &= (uint16_t)~bit;
            else {   /* a new step gets the chord you last played */
                uint8_t ch[GBX_CHORD]; chord_notes(keys_chord, keys_oct, ch);
                for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
                gbx_keys.gate[s] = 14; gbx_keys.vel[s] = 90; gbx_keys.accent &= (uint16_t)~bit; gbx_keys.on |= bit;
                if (!cr78_running) (void)gbx_chord_audition(ch, 11025U);
            }
        } else {   /* BASS, LEAD */
            struct gbx_line *l = zoom == T_BASS ? &gbx_bass : &gbx_lead;
            unsigned note = zoom == T_BASS ? bass_note : lead_note;
            if (l->on & bit) l->on &= (uint16_t)~bit;
            else {   /* a new step gets the note you last played */
                l->note[s] = (uint8_t)note; l->gate[s] = zoom == T_BASS ? 10 : 12; l->vel[s] = 100;
                l->accent &= (uint16_t)~bit; l->slide &= (uint16_t)~bit; l->on |= bit;
                if (!cr78_running) { if (zoom == T_BASS) (void)gbx_audition(note); else (void)gbx_lead_audition(note, 9000U); }
            }
        }
    }
    /* black keys */
    uint32_t held = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) {
        unsigned key = black_keys[i], hit = (down >> key) & 1U;
        if (fx_held) {   /* the pads */
            if (hit) {
                changed = 1;
                if (c->buttons & PX_BIT(PX_REC)) { pads_latched[scope()] ^= 1U << i; px_flash((pads_latched[scope()] >> i) & 1U ? "LATCH" : "OFF", now, 500U); }
                else px_flash(pad_name[i], now, 400U);
            }
            if (((c->keys >> key) & 1U) && !(c->buttons & PX_BIT(PX_REC))) held |= 1U << i;
            continue;
        }
        if (!hit) continue;
        changed = 1;
        if (zoom == OVERVIEW) {   /* mute a track */
            if (i < tracks_n()) { muted[i] ^= 1U; static char t[12]; strcpy(t, track_short[i]); strcat(t, muted[i] ? " OFF" : " ON"); px_flash(t, now, 500U); apply(); }
        } else if (play_layer) {
            /* PLAY: the black keys rest */
        } else if (zoom == T_DRUMS) {
            if (i >= drum_parts()) continue;
            unsigned v = drum_part(i);
            if (held_steps) {   /* a held step: that voice's hit on it */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) { cr78.grid[v] ^= (uint16_t)(1U << s); cr78.ratchet[v][s] = 1; }
                voice = v;
            } else play_drum(v);
        } else if (zoom == T_KEYS) {
            uint8_t ch[GBX_CHORD]; chord_notes(i, keys_oct, ch);
            if (held_steps) {   /* a held step: its chord */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) {
                    for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = ch[k];
                    gbx_keys.on |= (uint16_t)(1U << s);
                }
                (void)gbx_chord_audition(ch, 11025U);
            } else play_chord_key(i);
        } else {   /* BASS, LEAD */
            struct gbx_line *l = zoom == T_BASS ? &gbx_bass : &gbx_lead;
            unsigned note = key_note(i, zoom == T_BASS ? bass_oct : lead_oct);
            if (held_steps) {   /* a held step: its note */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) { l->note[s] = (uint8_t)note; l->on |= (uint16_t)(1U << s); }
                if (zoom == T_BASS) (void)gbx_audition(note); else (void)gbx_lead_audition(note, 9000U);
            } else play_note((unsigned)zoom, note);
        }
    }
    if (held != pads_held[scope()] || (fx_held && changed)) { pads_held[scope()] = held; apply(); }
    /* SELECT: the view's pages */
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
    /* PRESETS: a starting point for where you are */
    if (e[6]) {
        if (zoom == T_KEYS) {
            unsigned p = (unsigned)(((int)mysynth_patch_index() + e[6] + 8 * (int)mysynth_patch_count()) % (int)mysynth_patch_count());
            mysynth_patch(p); px_flash(mysynth_patch_name(p), now, 700U);
        } else if (zoom == T_LEAD) {
            unsigned p = (unsigned)(((int)mysynth_lead_patch_index() + e[6] + 8 * (int)mysynth_patch_count()) % (int)mysynth_patch_count());
            mysynth_lead_patch(p); px_flash(mysynth_patch_name(p), now, 700U);
        } else if (zoom <= T_DRUMS) {
            unsigned p = (unsigned)(((int)preset_index() + e[6] + 8 * (int)preset_count()) % (int)preset_count());
            preset_load(p); apply(); px_flash(preset_name(p), now, 700U);
        }
        changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t *k = e + 2;
    if (k[0] || k[1] || k[2] || k[3]) {
        changed = 1; last_verb = V_NONE;
        unsigned last = k[3] ? 3U : k[2] ? 2U : k[1] ? 1U : 0U;
        char value[12];
        if (zoom == T_DRUMS && held_steps) {   /* a held step: ratchet, accent */
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                if (k[0]) cr78_set_ratchet(voice, s, (int)cr78.ratchet[voice][s] + k[0]);
                if (k[1]) cr78.accent = (uint16_t)(k[1] > 0 ? cr78.accent | (1U << s) : cr78.accent & ~(1U << s));
            }
        } else if (zoom != OVERVIEW && zoom != T_DRUMS && held_steps) {   /* a held step: length, velocity (then accent), slide, octave */
            struct gbx_line *l = zoom == T_BASS ? &gbx_bass : zoom == T_LEAD ? &gbx_lead : 0;
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                uint16_t bit = (uint16_t)(1U << s);
                volatile uint8_t *gate = l ? &l->gate[s] : &gbx_keys.gate[s], *vel = l ? &l->vel[s] : &gbx_keys.vel[s];
                volatile uint16_t *acc = l ? &l->accent : &gbx_keys.accent;
                if (k[0]) *gate = (uint8_t)clamp(*gate + k[0], 1, zoom == T_BASS ? 16 : 64);
                if (k[1] > 0) { if (*vel >= 127) *acc |= bit; else *vel = (uint8_t)clamp(*vel + k[1] * 8, 1, 127); }
                if (k[1] < 0) { if (*acc & bit) *acc &= (uint16_t)~bit; else *vel = (uint8_t)clamp(*vel + k[1] * 8, 1, 127); }
                if (k[2] && l) l->slide = (uint16_t)(k[2] > 0 ? l->slide | bit : l->slide & ~bit);
                if (k[3]) {
                    int d = k[3] > 0 ? 12 : -12;
                    if (l) l->note[s] = (uint8_t)clamp(l->note[s] + d, 24, zoom == T_BASS ? 72 : 108);
                    else for (unsigned j = 0; j < GBX_CHORD; ++j) if (gbx_keys.note[s][j]) gbx_keys.note[s][j] = (uint8_t)clamp(gbx_keys.note[s][j] + d, 24, 96);
                }
                if (l) l->on |= bit; else gbx_keys.on |= bit;
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
                if (!k[i] || !mix_label(pg, i)[0]) continue;
                if (kn->kind == K_TEMPO) { groove_tempo_edit(k[i]); continue; }
                int x = clamp(mix_val[pg][i] + k[i] * kn->step, kn->lo, kn->hi);
                if (kn->kind == K_FILTER && x > -3 && x < 3) x = 0;
                mix_val[pg][i] = (int16_t)x;
            }
            apply();
            if (mix_label(pg, last)[0]) { knob_text(&mix_knob[pg][last], mix_val[pg][last], value); readout(0, mix_knob[pg][last].label, value, now); }
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
    if (now - last_ms >= 33U || changed) {
        last_ms = now;
        draw(now);
        leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
int gbx_control(const struct fm1_controls *c) { return control(c, 0); }
int gbx_mini_control(const struct fm1_controls *c) { return control(c, 1); }

/* ---- helpers for LOOP DECK ---- */
int32_t gbx_throw_pads(struct fx_perf *p, uint32_t pads) { return throw_pads(p, pads); }
void gbx_bass_sound(const int16_t values[2][4]) { build_bass_from(values); }
int32_t gbx_svf(float hz) { return svf_f(hz); }
float gbx_exp2(float x) { return exp2_approx(x); }
