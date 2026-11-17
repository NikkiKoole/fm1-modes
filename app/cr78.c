/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* CR-78 panel mode, after X0X's way of driving its 808 and 909: the keys always mean the same,
 * the labelled buttons open screens, step details are gestures. HOME goes back to the launcher;
 * the pattern keeps playing with the FM-1 transport after you leave.
 *
 *   two zoom levels, EDIT between them:
 *   MIX   (start)   the whole pattern: every voice on the grid; the BLACK KEYS MUTE voices;
 *                   the knobs and FX pads work on the whole mix
 *   VOICE           one voice, big: the BLACK KEYS PICK the voice (and stay zoomed in);
 *                   the knobs shape it
 *
 *   black keys      (VOICE) pick the voice (and hear it, while stopped):
 *                   KCK SNR RIM HHC HHO CYM COW CLV BNG CNG GUI (ALGORITHM reaches all 16)
 *     + FX held     the performance pads, on while held (REC too: latch, again: unlatch):
 *                   R16 R32 HLF REV LPF HPF CRS GAT DRV PMP SWP  (FX tapped alone: mute)
 *   white keys      the voice's 16 steps: tap on / off
 *     + ENV held    accent the step instead
 *     held          the knob bar is that step's: KNOB 1 its ratchet (1..8), KNOB 2 its accent
 *
 *   two screens, each with pages on SELECT (the grid stays; turn a knob and its value shows big)
 *   MIX   (start)  LIVE    TEMPO  SWING  FILTER  PUMP    FILTER: left low-pass, right high-pass
 *                  FX      DRIVE  CRUSH  GATE    SWEEP   on everything the FM-1 plays
 *                  GROOVE  ROLL   HALF   REV     LEVEL   the CR-78's pattern bent live
 *   EDIT  VOICE    SOUND   TUNE   DECAY  CUT     DRIVE   the selected voice
 *                  PITCH   BEND   BTIME  FINE    ATTACK
 *                  BODY    SUB    BAL    CLICK   CLICK PITCH
 *                  MOD     WOBBLE DEPTH  RES     RELEASE
 *                  LEVEL   LEVEL  ACCENT CLICK LENGTH
 *   EDIT again: back to LIVE
 *
 *   PRESETS  pattern     ALGORITHM  voice     FX  tap: mute, hold: pads     PLAY  FM-1 play / stop
 *
 * Voices: the recipes are dreamengine's cr78 cart, number for number, played by
 * the shared drum engine (drumvoice.c). */
#include "cr78.h"
#include "drumvoice.h"
#include "pixel.h"
#include "fx.h"
#include "instrument.h"
#include <string.h>

enum { V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM, V_METAL, V_COW,
       V_TAMB, V_MAR, V_CLV, V_GUIRO, V_BONH, V_BONL, V_CONGA, V_GUIROL, NV };
static const char *const names[NV] = {
    "KICK", "SNARE", "RIM", "HAT CL", "HAT OP", "CYMBAL", "METAL", "COWBELL",
    "TAMB", "MARACAS", "CLAVES", "GUIRO", "BONGO H", "BONGO L", "CONGA", "GUIRO L"};
static const char *const short_names[NV] = {
    "KICK", "SNR", "RIM", "HHC", "HHO", "CYM", "MTL", "COW",
    "TAMB", "MAR", "CLV", "GUI", "BNGH", "BNGL", "CONG", "GUIL"};
static const uint8_t row_color[NV] = {
    PX_RED, PX_EMBER, PX_PINK, PX_GOLD, PX_GOLD, PX_WHITE, PX_CYAN, PX_GREEN,
    PX_PINK, PX_EMBER, PX_EMBER, PX_GREEN, PX_CYAN, PX_CYAN, PX_EMBER, PX_GREEN};

/* ---- the recipes (dreamengine instrument() calls + fire() hits) ---- */
#define R(w, n, flt, cut, res, a, d, s, r, ex, g, v, lvl, pa, pm) \
    {.wave = w, .note = n, .filter = flt, .cutoff = cut, .reso = res, .attack = a, .decay = d, \
     .sustain = s, .release = r, .exp_release = ex, .gate = g, .vol = v, .level = lvl, .pitch = pa, .pitch_ms = pm}
static const struct dv_recipe recipes[NV][3] = {
    [V_KICK]  = {R(DV_SINE, 36, DV_FILTER_LOW, 320, 2, 0, 140, 0, 40, 0, 220, 5, 1.0f, 2.0f, 30)},
    [V_SNARE] = {R(DV_SINE, 63, DV_FILTER_NONE, 0, 0, 0, 60, 0, 20, 0, 100, 3, 0.15f, 2.0f, 15),
                 R(DV_NOISE, 60, DV_FILTER_LOW, 8000, 6, 0, 70, 0, 25, 0, 120, 6, 1.0f, 0, 0)},
    [V_RIM]   = {R(DV_TRI, 90, DV_FILTER_BAND, 1500, 2, 0, 0, 7, 6, 1, 1, 7, 1.0f, 0, 0),
                 R(DV_SINE, 57, DV_FILTER_NONE, 0, 0, 0, 0, 7, 8, 1, 1, 1, 1.0f, 0, 0)},
    [V_HATC]  = {R(DV_NOISE, 90, DV_FILTER_BAND, 8400, 15, 0, 22, 0, 10, 0, 40, 3, 0.38f, 0, 0)},
    [V_HATO]  = {R(DV_NOISE, 90, DV_FILTER_BAND, 8400, 15, 0, 290, 0, 150, 0, 420, 3, 0.38f, 0, 0)},
    [V_CYM]   = {R(DV_NOISE, 90, DV_FILTER_BAND, 8600, 15, 0, 400, 0, 150, 0, 600, 3, 0.21f, 0, 0)},
    [V_METAL] = {R(DV_TRI, 114, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 4, 0.36f, 0, 0),
                 R(DV_TRI, 112, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 4, 0.36f, 0, 0),
                 R(DV_TRI, 107, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 1, 0.36f, 0, 0)},
    [V_COW]   = {R(DV_SQUARE, 79, DV_FILTER_LOW, 1100, 2, 0, 55, 0, 25, 0, 150, 4, 0.35f, 0, 0),
                 R(DV_SQUARE, 73, DV_FILTER_LOW, 1100, 2, 0, 55, 0, 25, 0, 150, 3, 0.35f, 0, 0)},
    [V_TAMB]  = {R(DV_NOISE, 80, DV_FILTER_BAND, 4650, 15, 8, 200, 0, 60, 0, 300, 3, 0.18f, 0, 0)},
    [V_MAR]   = {R(DV_NOISE, 90, DV_FILTER_BAND, 8700, 15, 0, 15, 0, 8, 0, 30, 3, 0.47f, 0, 0)},
    [V_CLV]   = {R(DV_SINE, 99, DV_FILTER_NONE, 0, 0, 0, 40, 0, 12, 0, 60, 4, 1.0f, 0, 0)},
    [V_GUIRO] = {R(DV_SCRAPE, 60, DV_FILTER_NONE, 0, 0, 0, 0, 7, 30, 0, 270, 4, 0.99f, 0, 0)},
    [V_BONH]  = {R(DV_SINE, 74, DV_FILTER_NONE, 0, 0, 0, 40, 0, 25, 0, 140, 4, 1.0f, 0, 0)},
    [V_BONL]  = {R(DV_SINE, 67, DV_FILTER_NONE, 0, 0, 0, 40, 0, 25, 0, 80, 3, 1.0f, 0, 0),
                 R(DV_SINE, 99, DV_FILTER_NONE, 0, 0, 0, 25, 0, 10, 0, 40, 3, 1.0f, 0, 0)},
    [V_CONGA] = {R(DV_SINE, 55, DV_FILTER_NONE, 0, 0, 0, 160, 0, 60, 0, 300, 4, 1.0f, 0, 0)},
    [V_GUIROL]= {R(DV_SCRAPE, 60, DV_FILTER_NONE, 0, 0, 0, 0, 7, 30, 0, 820, 4, 0.75f, 0, 0)},
};
static const uint8_t part_count[NV] = {1, 2, 2, 1, 1, 1, 3, 2, 1, 1, 1, 1, 1, 2, 1, 1};
static const float voice_tune[NV] = {[V_CLV] = -0.23f};
/* scrape: teeth per second (10 * 10^timbre) and the body transposition */
static const float teeth[NV] = {[V_GUIRO] = 74.13f, [V_GUIROL] = 83.18f};

/* ---- presets (the cart's eight) ---- */
struct preset { const char *name; uint8_t tempo, swing; const char *row[NV]; const char *accent; };
static const struct preset presets[] = {
    {"ROCK-1", 118, 50, {[V_KICK] = "x.......x.x.....", [V_SNARE] = "....x.......x...",
                         [V_HATC] = "x.x.x.x.x.x.x.x."}, "x.......x......."},
    {"DISCO-2", 100, 50, {[V_KICK] = "x...x...x...x...", [V_HATC] = "xx.xxx.xxx.xxx.x",
                          [V_HATO] = "..x...x...x...x.", [V_TAMB] = "....x.......x...",
                          [V_BONH] = ".........x.....x", [V_GUIRO] = "............x..."}, "x.......x......."},
    {"BOSSA", 132, 50, {[V_KICK] = "x.....x.x.....x.", [V_RIM] = "x..x..x...x..x..",
                        [V_HATC] = "x.x.x.x.x.x.x.x.", [V_MAR] = "xxxxxxxxxxxxxxxx"}, "x.......x......."},
    {"MAMBO", 176, 50, {[V_KICK] = "x.......x.......", [V_COW] = "x..x..x.x..x..x.",
                        [V_CONGA] = "..x...x...x...x.", [V_BONH] = ".x......x.x.....",
                        [V_CLV] = "x..x..x...x.x..."}, "x.......x......."},
    {"CHA-CHA", 118, 50, {[V_GUIROL] = "x...............", [V_GUIRO] = "........x...x...",
                          [V_COW] = "x...x...x...x...", [V_KICK] = "....x.......x...",
                          [V_CONGA] = "......x.......xx", [V_BONL] = "..x.......x....."}, "x.......x......."},
    {"BEGUINE", 104, 50, {[V_KICK] = "x.....x...x.....", [V_METAL] = "x...x...x...x...",
                          [V_RIM] = "...x...x.....x..", [V_HATC] = "x.x.x.x.x.x.x.x.",
                          [V_BONL] = "..............x."}, "x.........x....."},
    {"SHUFFLE", 96, 66, {[V_KICK] = "x.......x.......", [V_SNARE] = "....x.......x...",
                         [V_HATC] = "x.x.x.x.x.x.x.x."}, "x.......x......."},
    {"SWING", 138, 66, {[V_KICK] = "x.......x.......", [V_HATC] = "x...x.x.x...x.x.",
                        [V_RIM] = "....x.......x..."}, "x.......x......."},
};
enum { PRESETS = sizeof presets / sizeof presets[0] };

static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
enum { LIVE, VOICE, SCREENS };
enum { K_NUM, K_SIGNED, K_PCT, K_MS, K_TEMPO, K_FILTER, K_OPEN, K_OFF0, K_ROLL, K_ONOFF };
struct knob { const char *label; uint8_t kind; int16_t lo, hi, step; };
enum { LIVE_PAGES = 3, VOICE_PAGES = 5 };
static const struct px_page live_page[LIVE_PAGES] = {
    {"MIX", {"TMPO", "SWNG", "FLTR", "PUMP"}}, {"FX", {"DRV", "CRSH", "GATE", "SWP"}},
    {"GROOVE", {"ROLL", "HALF", "REV", "LVL"}}};
static const struct knob live_knob[LIVE_PAGES][4] = {
    {{"TMPO", K_TEMPO, 0, 0, 1}, {"SWNG", K_NUM, 50, 66, 2}, {"FLTR", K_FILTER, -100, 100, 4}, {"PUMP", K_OFF0, 0, 100, 5}},
    {{"DRV", K_OFF0, 0, 100, 5}, {"CRSH", K_OFF0, 0, 100, 5}, {"GATE", K_OFF0, 0, 100, 5}, {"SWP", K_OFF0, 0, 100, 5}},
    {{"ROLL", K_ROLL, 0, 2, 1}, {"HALF", K_ONOFF, 0, 1, 1}, {"REV", K_ONOFF, 0, 1, 1}, {"LVL", K_NUM, 0, 100, 4}}};
static int16_t live_val[LIVE_PAGES][4] = {{0, 50, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 40}};
static const struct px_page voice_page[VOICE_PAGES] = {
    {"SOUND", {"TUNE", "DEC", "CUT", "DRV"}}, {"PITCH", {"BEND", "BTIM", "FINE", "ATK"}},
    {"BODY", {"SUB", "BAL", "CLK", "CPIT"}}, {"MOD", {"WOB", "DPTH", "RES", "REL"}},
    {"LEVEL", {"LVL", "ACC", "CLEN", ""}}};
static const struct knob voice_knob[VOICE_PAGES][4] = {
    {{"TUNE", K_SIGNED, -24, 24, 1}, {"DEC", K_PCT, 10, 400, 5}, {"CUT", K_OPEN, 0, 100, 4}, {"DRV", K_OFF0, 0, 100, 5}},
    {{"BEND", K_NUM, 0, 24, 1}, {"BTIM", K_MS, 5, 300, 5}, {"FINE", K_SIGNED, -50, 50, 5}, {"ATK", K_MS, 0, 50, 1}},
    {{"SUB", K_OFF0, 0, 100, 5}, {"BAL", K_SIGNED, -100, 100, 10}, {"CLK", K_OFF0, 0, 100, 5}, {"CPIT", K_NUM, 0, 100, 4}},
    {{"WOB", K_OFF0, 0, 100, 4}, {"DPTH", K_NUM, 0, 100, 5}, {"RES", K_NUM, 0, 15, 1}, {"REL", K_PCT, 10, 400, 5}},
    {{"LVL", K_NUM, 0, 150, 5}, {"ACC", K_NUM, 0, 4, 1}, {"CLEN", K_MS, 1, 30, 1}, {"", K_NUM, 0, 0, 0}}};
static unsigned active, initialized, full_redraw, selected, preset, swing = 50, level = 40, page[SCREENS];
/* ---- each voice's SHAPE: eight categories of four knobs, on top of the cart's recipe ---- */
enum { C_PIT, C_FLT, C_ENV, C_DRV, C_CLK, C_BOD, C_LFO, C_VEL, NCATS };
static int16_t shape[NV][NCATS][4];
static int16_t cut[NV];   /* SOUND's CUT: 100 = as built, lower = darker */
static int filtered_part(unsigned v)
{
    for (unsigned p = 0; p < part_count[v]; ++p) if (recipes[v][p].filter != DV_FILTER_NONE) return (int)p;
    return -1;
}
static void shape_defaults(unsigned v)
{
    const struct dv_recipe *r = &recipes[v][0];
    int16_t (*s)[4] = shape[v];
    int fp = filtered_part(v);
    float drive = 0;
    for (unsigned p = 0; p < part_count[v]; ++p) if (recipes[v][p].drive > drive) drive = recipes[v][p].drive;
    s[C_PIT][0] = 0; s[C_PIT][1] = (int16_t)(r->pitch + 0.5f); s[C_PIT][2] = (int16_t)(r->pitch_ms > 0 ? r->pitch_ms : 30); s[C_PIT][3] = 0;
    s[C_FLT][0] = 0; s[C_FLT][1] = 50; s[C_FLT][2] = (int16_t)(fp >= 0 ? recipes[v][fp].reso : 2);
    s[C_ENV][0] = (int16_t)r->attack; s[C_ENV][1] = 100; s[C_ENV][2] = 100;
    s[C_DRV][0] = (int16_t)(drive * 100.0f + 0.5f);
    s[C_CLK][0] = 0; s[C_CLK][1] = 50; s[C_CLK][2] = 4;
    s[C_BOD][0] = 0; s[C_BOD][1] = 0;
    s[C_LFO][0] = 0; s[C_LFO][1] = 50;
    s[C_VEL][0] = 100; s[C_VEL][1] = 2;
    cut[v] = 100;
}
static uint32_t prev_buttons, prev_keys, last_ms, led_state, flash_until[NV];
static uint16_t held_steps;   /* step keys held right now */

/* the black keys pick the voice (the other five: ALGORITHM) */
static const uint8_t black_voice[PX_PADS] = {V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM, V_COW, V_CLV, V_BONH, V_CONGA, V_GUIRO};
static const char *const voice_pad[PX_PADS] = {"KCK", "SNR", "RIM", "HHC", "HHO", "CYM", "COW", "CLV", "BNG", "CNG", "GUI"};
static uint32_t voice_lit(void)
{
    for (unsigned i = 0; i < PX_PADS; ++i) if (black_voice[i] == selected) return 1U << i;
    return 0;
}
/* ... and with FX held, the performance pads: fixed throws of the LIVE effects, on while held */
enum { P_R16, P_R32, P_HLF, P_REV, P_LPF, P_HPF, P_CRS, P_GAT, P_DRV, P_PMP, P_SWP };
static const char *const pad_name[PX_PADS] = {"R16", "R32", "HLF", "REV", "LPF", "HPF", "CRS", "GAT", "DRV", "PMP", "SWP"};
static uint32_t pads_held, pads_latched;
static unsigned fx_held, fx_used;   /* FX down; a pad was played while it was (else its release mutes) */
static unsigned screen;   /* (forward: LIVE is the MIX zoom level, VOICE one voice) */
static uint32_t mute_lit(void)   /* the black keys of the muted voices */
{
    uint32_t m = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) if ((cr78.voice_mute >> black_voice[i]) & 1U) m |= 1U << i;
    return m;
}
static const char *const *key_row(void) { return fx_held ? pad_name : voice_pad; }
static uint32_t key_lit(void) { return fx_held ? pads_held | pads_latched : screen == LIVE ? mute_lit() : voice_lit(); }

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

/* the LIVE knobs -> the master stage on the whole output (perform_dsp.c) and the pattern's bends */
static void apply_live(void)
{
    struct fx_perf *p = &fx_perf;
    const int16_t filter = live_val[0][2], pump = live_val[0][3], drive = live_val[1][0], crush = live_val[1][1];
    const int16_t gate = live_val[1][2], sweep = live_val[1][3];
    int32_t on = 0;
    if (filter < -2) {   /* left: a low-pass closing from 18 kHz to 150 Hz */
        p->lpf_f = svf_f(150.0f * exp2_approx((float)(100 + filter) * 0.069f)); p->lpf_damp = 34000; on |= PERF_LPF;
    } else if (filter > 2) {   /* right: a high-pass opening from 20 Hz to 5 kHz */
        p->hpf_f = svf_f(20.0f * exp2_approx((float)filter * 0.08f)); p->hpf_damp = 34000; on |= PERF_HPF;
    }
    if (pump) { p->pump_amount = (int32_t)(pump * 29491 / 100); p->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (drive) {
        float dr = 0.15f + 0.85f * (float)drive / 100.0f, g = dr * dr * 24.0f;
        p->drive_norm = (int32_t)(32768.0f * 0.2f / tanh_approx(0.2f * g));
        p->drive_neg = (int32_t)(g * (1.0f - 0.4f * dr) * 4096.0f); p->drive_pos = (int32_t)(g * 4096.0f);
        p->drive_mix = 32768; p->drive_level = 32768; on |= PERF_DRIVE;
    }
    if (crush) {
        int bits = 16 - crush * 12 / 100;
        p->crush_step = 1 << (24 - bits); p->crush_rate = 1 + crush * 15 / 100; p->crush_mix = 32768; on |= PERF_CRUSH;
    }
    if (gate) {   /* chops in 1/16: the further up, the shorter */
        p->gate_shift = 0; p->gate_len = (int32_t)((95 - gate * 80 / 100) * 65536 / 100); p->gate_floor = 0; on |= PERF_GATE;
    }
    if (sweep) {   /* a slow phaser, deeper and wetter as it goes up */
        p->sweep_inc = (int32_t)(uint32_t)(0.25f * 97391.55f); p->sweep_depth = 26214; p->sweep_fb = 16384;
        p->sweep_mix = (int32_t)(sweep * 32768 / 100); on |= PERF_SWEEP;
    }
    /* the pads, over the knobs: each a fixed throw */
    uint32_t pads = pads_held | pads_latched;
    if (pads & (1U << P_LPF)) { p->lpf_f = svf_f(580.0f); p->lpf_damp = 28800; on |= PERF_LPF; }
    if (pads & (1U << P_HPF)) { p->hpf_f = svf_f(600.0f); p->hpf_damp = 33000; on |= PERF_HPF; }
    if (pads & (1U << P_CRS)) { p->crush_step = 1 << (24 - 6); p->crush_rate = 4; p->crush_mix = 32768; on |= PERF_CRUSH; }
    if (pads & (1U << P_GAT)) { p->gate_shift = 0; p->gate_len = 32768; p->gate_floor = 0; on |= PERF_GATE; }
    if (pads & (1U << P_DRV)) {
        float g = 0.745f * 0.745f * 24.0f;
        p->drive_norm = (int32_t)(32768.0f * 0.2f / tanh_approx(0.2f * g));
        p->drive_neg = (int32_t)(g * (1.0f - 0.4f * 0.745f) * 4096.0f); p->drive_pos = (int32_t)(g * 4096.0f);
        p->drive_mix = 32768; p->drive_level = 32768; on |= PERF_DRIVE;
    }
    if (pads & (1U << P_PMP)) { p->pump_amount = 26214; p->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (pads & (1U << P_SWP)) {
        p->sweep_inc = (int32_t)(uint32_t)(0.5f * 97391.55f); p->sweep_depth = 22938; p->sweep_fb = 16384; p->sweep_mix = 19661; on |= PERF_SWEEP;
    }
    p->on = on;
    unsigned perf = 0;
    if (pads & (1U << P_R16)) perf |= CR78_PERF_ROLL16;
    if (pads & (1U << P_R32)) perf |= CR78_PERF_ROLL32;
    if (pads & (1U << P_HLF)) perf |= CR78_PERF_HALF;
    if (pads & (1U << P_REV)) perf |= CR78_PERF_REV;
    if (live_val[2][0] == 1) perf |= CR78_PERF_ROLL16;
    if (live_val[2][0] == 2) perf |= CR78_PERF_ROLL32;
    if (live_val[2][1]) perf |= CR78_PERF_HALF;
    if (live_val[2][2]) perf |= CR78_PERF_REV;
    cr78.roll_len = 1; cr78.roll_fade = (pads & 3U) ? 1 : 0;   /* a thrown roll dies down a little */
    cr78.perf = (uint8_t)perf;
    level = (unsigned)live_val[2][3];
    swing = (unsigned)live_val[0][1];
}
static void build_voice(unsigned v)
{
    int16_t (*sh)[4] = shape[v];
    struct dv_voice next = {0};
    unsigned n = 0;
    const float bal = (float)sh[C_BOD][1] / 100.0f, level = (float)sh[C_VEL][0] / 100.0f;
    for (unsigned p = 0; p < part_count[v]; ++p) {
        struct dv_recipe r = recipes[v][p];
        unsigned noisy = r.wave == DV_NOISE || r.wave == DV_SCRAPE;
        r.level *= level * (bal > 0 && noisy ? 1.0f - bal : bal < 0 && !noisy ? 1.0f + bal : 1.0f);
        r.tune = voice_tune[v] + (float)sh[C_PIT][3] / 100.0f;
        r.teeth = teeth[v];
        r.body = v == V_GUIROL;
        /* PITCH: the drop every part starts with */
        r.pitch = (float)sh[C_PIT][1]; r.pitch_ms = (float)sh[C_PIT][2];
        /* FILTER: ORIG moves the cart's own filters (CUT 50 = as built, +-4 octaves); LP/BP/HP replace them */
        if (sh[C_FLT][0] == 0) {
            if (r.filter != DV_FILTER_NONE) {
                float hz = (float)r.cutoff * exp2_approx((float)(sh[C_FLT][1] - 50) / 12.5f);
                r.cutoff = (uint16_t)(hz < 20.0f ? 20.0f : hz > 18000.0f ? 18000.0f : hz);
                r.reso = (uint8_t)sh[C_FLT][2];
            }
        } else {
            static const uint8_t type[4] = {DV_FILTER_NONE, DV_FILTER_LOW, DV_FILTER_BAND, DV_FILTER_HIGH};
            r.filter = type[sh[C_FLT][0]];
            r.cutoff = (uint16_t)(30.0f * exp2_approx((float)sh[C_FLT][1] * 0.0906f));
            r.reso = (uint8_t)sh[C_FLT][2];
        }
        /* CUT: darker as it comes down. A lowpass closes (one is added where there is none),
         * a bandpass or highpass moves down, up to four octaves */
        if (cut[v] < 100) {
            float lp = 60.0f * exp2_approx((float)cut[v] * 0.084f);
            if (r.filter == DV_FILTER_NONE) { r.filter = DV_FILTER_LOW; r.cutoff = (uint16_t)lp; r.reso = (uint8_t)sh[C_FLT][2]; }
            else if (r.filter == DV_FILTER_LOW) { if (lp < (float)r.cutoff) r.cutoff = (uint16_t)lp; }
            else r.cutoff = (uint16_t)((float)r.cutoff * exp2_approx(-(float)(100 - cut[v]) * 0.04f));
        }
        /* ENVELOPE (DEC is the stretch below) */
        r.attack = (uint16_t)sh[C_ENV][0];
        r.release = (uint16_t)((uint32_t)r.release * (uint32_t)sh[C_ENV][2] / 100U);
        /* DRIVE, WOBBLE */
        r.drive = (float)sh[C_DRV][0] / 100.0f;
        if (sh[C_LFO][0]) { r.lfo_hz = 0.5f * exp2_approx((float)sh[C_LFO][0] * 0.063f); r.lfo_depth = (float)sh[C_LFO][1] / 100.0f; }
        else r.lfo_depth = 0, r.lfo_hz = 0;
        dv_build_part(&next.part[n++], &r, (float)sh[C_PIT][0], (float)sh[C_ENV][1] / 100.0f);
    }
    const struct dv_recipe *main = &recipes[v][0];
    if (sh[C_BOD][0] && n < DV_MAX_PARTS) {   /* BODY: a sine an octave under the first part */
        struct dv_recipe sub = {.wave = DV_SINE, .note = (uint8_t)(main->note > 12 ? main->note - 12 : main->note),
            .tune = voice_tune[v], .attack = (uint16_t)sh[C_ENV][0], .decay = (uint16_t)(main->decay ? main->decay * 6 / 5 : 120),
            .release = 30, .gate = main->gate ? main->gate : 150, .vol = 6, .level = (float)sh[C_BOD][0] / 100.0f * level,
            .pitch = (float)sh[C_PIT][1], .pitch_ms = (float)sh[C_PIT][2]};
        dv_build_part(&next.part[n++], &sub, (float)sh[C_PIT][0], (float)sh[C_ENV][1] / 100.0f);
    }
    if (sh[C_CLK][0] && n < DV_MAX_PARTS) {   /* CLICK: a bandpassed noise tick on the front */
        struct dv_recipe click = {.wave = DV_NOISE, .note = 60, .filter = DV_FILTER_BAND,
            .cutoff = (uint16_t)(1000.0f * exp2_approx((float)sh[C_CLK][1] * 0.033f)), .reso = 4,
            .decay = (uint16_t)sh[C_CLK][2], .release = 3, .gate = (uint16_t)sh[C_CLK][2], .vol = 7,
            .level = (float)sh[C_CLK][0] / 100.0f * level};
        dv_build_part(&next.part[n++], &click, 0, 1.0f);
    }
    next.parts = (uint8_t)n;
    cr78.accent_boost[v] = (uint8_t)sh[C_VEL][1];
    cr78.voice[v] = next;   /* one copy; a hit mid-update just picks either */
}
static void apply_globals(void)
{
    cr78.swing_q16 = (int32_t)((swing - 50U) * 4U * 65536U / 100U);
    cr78.level = (int32_t)(level * 32768U / 100U);
}
static void load_preset(unsigned index)
{
    const struct preset *p = &presets[index];
    for (unsigned v = 0; v < NV; ++v) {
        uint16_t bits = 0;
        for (unsigned s = 0; s < 16; ++s) {
            if (p->row[v] && p->row[v][s] == 'x') bits |= (uint16_t)(1U << s);
            cr78.ratchet[v][s] = 1;
        }
        cr78.grid[v] = bits;
    }
    uint16_t accent = 0;
    for (unsigned s = 0; s < 16; ++s) if (p->accent && p->accent[s] == 'x') accent |= (uint16_t)(1U << s);
    cr78.accent = accent;
    swing = p->swing; live_val[0][1] = (int16_t)swing;   /* the SWING knob shows the preset's */
    apply_globals();
    /* the preset's tempo becomes the FM-1 tempo, as on the real box */
    groove_tempo_edit((int)p->tempo - (int)groove_tempo_current());
}
static void init(void)
{
    dv_build_tables();
    for (unsigned v = 0; v < NV; ++v) { shape_defaults(v); build_voice(v); }
    preset = 1;   /* DISCO-2, obviously */
    load_preset(preset);
    initialized = 1;
}

/* ---- screen ---- */
enum { GRID_X = 13, GRID_Y = 15, ROW = 3, COL = 4 };
static void draw_grid(uint32_t now)
{
    unsigned head = cr78_playhead, run = cr78_running;
    if (run) px_rect(GRID_X + (int)head * COL - 1, GRID_Y - 2, COL + 1, NV * ROW + 2, PX_DUSK);
    for (unsigned s = 0; s < 16; ++s)   /* accent strip */
        px_rect(GRID_X + (int)s * COL, GRID_Y - 2, COL - 1, 1, (cr78.accent >> s) & 1U ? PX_EMBER : ((s & 3U) ? PX_NIGHT : PX_DUSK));
    for (unsigned v = 0; v < NV; ++v) {
        int y = GRID_Y + (int)v * ROW;
        unsigned lit = (int32_t)(flash_until[v] - now) > 0;
        unsigned muted = (cr78.voice_mute >> v) & 1U;
        px_rect(1, y, 2, 2, muted ? PX_DUSK : lit ? PX_WHITE : row_color[v]);
        if (v == selected) px_rect(5, y, 6, 2, PX_GOLD);
        for (unsigned s = 0; s < 16; ++s) {
            int x = GRID_X + (int)s * COL;
            unsigned on = (cr78.grid[v] >> s) & 1U;
            if (on) {
                unsigned r = cr78.ratchet[v][s];
                px_rect(x, y, COL - 1, 2, muted ? PX_DUSK : run && s == head ? PX_WHITE : row_color[v]);
                if (r > 1) px_pset(x + 1, y, PX_NIGHT);    /* a notch marks a ratchet */
            } else if (!(s & 3U)) px_pset(x + 1, y + 1, PX_DUSK);
        }
    }
}
/* VOICE: the one voice, big. Its name, its 16 steps as tall bars (height = ratchet hits),
 * accents under them, the playhead running along */
static void draw_zoom(uint32_t now)
{
    enum { BASE = 56, UNIT = 5, X0 = 8, W = 4 };
    unsigned head = cr78_playhead, run = cr78_running, v = selected;
    unsigned lit = (int32_t)(flash_until[v] - now) > 0;
    px_print_scaled(short_names[v], 2, 10, 2, lit ? PX_WHITE : row_color[v]);
    if ((cr78.voice_mute >> v) & 1U) px_print("MUTED", 2, 22, PX_RED);
    for (unsigned s = 0; s < 16; ++s) {
        int x = X0 + (int)s * W;
        unsigned on = (cr78.grid[v] >> s) & 1U, r = cr78.ratchet[v][s];
        if (run && s == head) px_rect(x - 1, 28, W + 1, BASE - 27, PX_DUSK);
        if (on) {
            unsigned color = (held_steps >> s) & 1U ? PX_GOLD : run && s == head ? PX_WHITE : row_color[v];
            for (unsigned i = 0; i < r; ++i) px_rect(x, BASE - (int)(i + 1) * UNIT + 1, W - 1, UNIT - 1, color);
        } else px_rect(x, BASE - 1, W - 1, 1, (s & 3U) ? PX_DUSK : PX_SLATE);
        px_rect(x, BASE + 2, W - 1, 2, (cr78.accent >> s) & 1U ? PX_EMBER : PX_NIGHT);
    }
}
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void signed_text(int x, char *out) { if (x > 0) *out++ = '+'; px_number(x, out); }
static int16_t *voice_ref(unsigned v, unsigned pg, unsigned k)
{
    int16_t (*sh)[4] = shape[v];
    switch (pg * 4 + k) {
    case 0: return &sh[C_PIT][0];  case 1: return &sh[C_ENV][1];  case 2: return &cut[v];        case 3: return &sh[C_DRV][0];
    case 4: return &sh[C_PIT][1];  case 5: return &sh[C_PIT][2];  case 6: return &sh[C_PIT][3];  case 7: return &sh[C_ENV][0];
    case 8: return &sh[C_BOD][0];  case 9: return &sh[C_BOD][1];  case 10: return &sh[C_CLK][0]; case 11: return &sh[C_CLK][1];
    case 12: return &sh[C_LFO][0]; case 13: return &sh[C_LFO][1]; case 14: return &sh[C_FLT][2]; case 15: return &sh[C_ENV][2];
    case 16: return &sh[C_VEL][0]; case 17: return &sh[C_VEL][1]; case 18: return &sh[C_CLK][2];
    default: return 0;
    }
}
static const struct knob *knob_at(unsigned scr, unsigned k)
{
    return scr == LIVE ? &live_knob[page[LIVE]][k] : &voice_knob[page[VOICE]][k];
}
static int knob_get(unsigned scr, unsigned k)
{
    if (scr == LIVE) return live_val[page[LIVE]][k];
    int16_t *r = voice_ref(selected, page[VOICE], k);
    return r ? *r : 0;
}
static void fmt_knob(const struct knob *kn, int x, char *out);
/* one knob's value as the screen writes it */
static void knob_text(unsigned scr, unsigned k, char *out) { fmt_knob(knob_at(scr, k), knob_get(scr, k), out); }
static void fmt_knob(const struct knob *kn, int x, char *out)
{
    *out = 0;
    if (!kn->label[0]) return;
    switch (kn->kind) {
    case K_TEMPO: px_number((int)groove_tempo_current(), out); break;
    case K_SIGNED: signed_text(x, out); break;
    case K_PCT: px_number(x, out); strcat(out, "%"); break;
    case K_MS: px_number(x, out); strcat(out, "MS"); break;
    case K_FILTER:
        if (x < -2) { strcpy(out, "LP"); px_number(-x, out + 2); }
        else if (x > 2) { strcpy(out, "HP"); px_number(x, out + 2); }
        else strcpy(out, "OFF");
        break;
    case K_OPEN: if (x >= 100) strcpy(out, "OPEN"); else px_number(x, out); break;
    case K_OFF0: if (!x) strcpy(out, "OFF"); else px_number(x, out); break;
    case K_ROLL: strcpy(out, x == 0 ? "OFF" : x == 1 ? "1/16" : "1/32"); break;
    case K_ONOFF: strcpy(out, x ? "ON" : "OFF"); break;
    default: px_number(x, out); break;
    }
}
static void readout(unsigned scr, unsigned k, uint32_t now)   /* turn a knob: its name and value, big */
{
    static char text[24];
    char value[12];
    if (scr == VOICE) { strcpy(text, short_names[selected]); strcat(text, " "); } else text[0] = 0;
    strcat(text, knob_at(scr, k)->label); strcat(text, " ");
    knob_text(scr, k, value); strcat(text, value);
    px_flash(text, now, 900U);
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    uint16_t hits = cr78_hits; cr78_hits = 0;   /* (the grid's row lamps) */
    for (unsigned i = 0; i < NV; ++i) if (hits & (1U << i)) flash_until[i] = now + 90U;
    if (held_steps) {   /* a held step takes the knob bar: its ratchet and accent */
        unsigned st = 0;
        while (!((held_steps >> st) & 1U)) ++st;
        static char title[12];
        strcpy(title, "STEP "); px_number((int)st + 1, title + 5);
        struct px_page step_page = {title, {"RTCH", "ACC", "", ""}};
        px_number(cr78.ratchet[selected][st], v[0]);
        strcpy(v[1], (cr78.accent >> st) & 1U ? "ON" : "OFF"); v[2][0] = v[3][0] = 0;
        px_chrome_pads("CR78", &step_page, 1, 0, value, key_row(), key_lit());
    } else {
        for (unsigned k = 0; k < 4; ++k) knob_text(screen, k, v[k]);
        if (screen == LIVE) px_chrome_pads("CR78", live_page, LIVE_PAGES, page[LIVE], value, key_row(), key_lit());
        else px_chrome_pads("CR78", voice_page, VOICE_PAGES, page[VOICE], value, key_row(), key_lit());
    }
    if (screen == VOICE) { draw_zoom(now); goto header_right; }
    px_print(names[selected], 2, 8, row_color[selected]);
header_right:
    if (cr78.muted) px_print("MUTE", PX_W - 17, 8, PX_RED);
    else if (pads_latched && !fx_held) {   /* latched pads: their names, so nothing plays unseen */
        char on[16] = ""; unsigned n = 0;
        for (unsigned i = 0; i < PX_PADS && n < 3; ++i) if ((pads_latched >> i) & 1U) { if (n++) strcat(on, " "); strcat(on, pad_name[i]); }
        px_print(on, PX_W - 1 - ((int)strlen(on) * 4 - 1), 8, PX_EMBER);
    } else px_print(presets[preset].name, PX_W - 1 - ((int)strlen(presets[preset].name) * 4 - 1), 8, PX_SLATE);
    if (screen != VOICE) draw_grid(now);
    px_flash_draw(now);
}
static void step_leds(uint32_t now)
{
    /* white-key LEDs: the voice's steps, the playhead blinking; black: the voice */
    uint32_t want = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned on = (cr78.grid[selected] >> s) & 1U;
        if (cr78_running && s == cr78_playhead) on = !on || ((now / 60U) & 1U);
        if (on) want |= 1U << s;
    }
    want |= key_lit() << 16;
    for (unsigned s = 0; s < 16; ++s)
        if (((want ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (want >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((want ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (want >> (16 + i)) & 1U);
    led_state = want;
}
static void set_ratchet(unsigned s, int r)
{
    cr78.ratchet[selected][s] = (uint8_t)clamp(r, 1, CR78_RATCHET_MAX);
    cr78.grid[selected] |= (uint16_t)(1U << s);
}

unsigned cr78_active(void) { return active; }

int cr78_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys;
    unsigned toggle = px_mode_toggle(pressed, PX_MODE_CR78, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (initialized) cr78.fm1_swing = (uint8_t)groove_sequence_setting(0);
    if (!active) {
        if (!toggle) return 0;
        if (!initialized) init();
        cr78.fm1_swing = (uint8_t)groove_sequence_setting(0);
        cr78.armed = 1;
        px_leds_off(); led_state = 0; held_steps = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(screen == LIVE ? live_page[page[LIVE]].name : voice_page[page[VOICE]].name, now, 700U);
        return 1;
    }
    if (toggle) {   /* held pads let go, latched ones keep playing */
        pads_held = 0; fx_held = 0; apply_live();
        px_leds_off(); led_state = 0;
        active = 0;
        return 2;
    }
    unsigned changed = 0;
    /* white keys: the steps; ENV held: accents */
    held_steps = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned key = white_keys[s];
        if ((c->keys >> key) & 1U) held_steps |= (uint16_t)(1U << s);
        if (!((down >> key) & 1U)) continue;
        uint16_t bit = (uint16_t)(1U << s);
        if (c->buttons & PX_BIT(PX_ENV)) cr78.accent ^= bit;
        else {
            cr78.grid[selected] ^= bit;
            if (cr78.grid[selected] & bit) {
                cr78.ratchet[selected][s] = 1;
                if (!cr78_running) (void)cr78_audition(selected, 0);
            }
        }
        changed = 1;
    }
    /* FX: tapped alone, mute; held, the black keys are the pads (REC too: latch) */
    if (pressed & PX_BIT(PX_FX)) { fx_held = 1; fx_used = 0; changed = 1; }
    if (fx_held && !(c->buttons & PX_BIT(PX_FX))) {
        fx_held = 0; changed = 1;
        if (!fx_used) { cr78.muted ^= 1U; px_flash(cr78.muted ? "MUTE" : "UNMUTE", now, 600U); }
    }
    uint32_t held = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) {
        unsigned key = black_keys[i], hit = (down >> key) & 1U;
        if (fx_held) {
            if (hit) {
                fx_used = 1; changed = 1;
                if (c->buttons & PX_BIT(PX_REC)) {
                    pads_latched ^= 1U << i;
                    px_flash((pads_latched >> i) & 1U ? "LATCH" : "OFF", now, 500U);
                } else px_flash(pad_name[i], now, 400U);
            }
            if (((c->keys >> key) & 1U) && !(c->buttons & PX_BIT(PX_REC))) held |= 1U << i;
        } else if (hit && screen == LIVE) {   /* MIX: mute / unmute the voice */
            unsigned v = black_voice[i];
            cr78.voice_mute ^= (uint16_t)(1U << v);
            static char text[12];
            strcpy(text, voice_pad[i]); strcat(text, (cr78.voice_mute >> v) & 1U ? " OFF" : " ON");
            px_flash(text, now, 500U);
            changed = 1;
        } else if (hit) {   /* VOICE: the voice, heard while stopped, silent while it plays */
            selected = black_voice[i];
            if (!cr78_running) (void)cr78_audition(selected, 1);
            changed = 1;
        }
    }
    if (held != pads_held || (fx_used && changed)) { pads_held = held; apply_live(); changed = 1; }
    /* EDIT: the voice's pages, again: LIVE's; SELECT: the pages of the screen you are on */
    const int16_t *e = c->encoders;
    if (pressed & PX_BIT(PX_EDIT)) {
        screen = screen == VOICE ? LIVE : VOICE;
        px_flash(screen == LIVE ? live_page[page[LIVE]].name : voice_page[page[VOICE]].name, now, 600U); changed = 1;
    }
    if (e[0]) {
        unsigned n = screen == LIVE ? LIVE_PAGES : VOICE_PAGES;
        page[screen] = (unsigned)clamp((int)page[screen] + e[0], 0, (int)n - 1);
        px_flash(screen == LIVE ? live_page[page[LIVE]].name : voice_page[page[VOICE]].name, now, 600U); changed = 1;
    }
    /* PRESETS: patterns; ALGORITHM: the voice */
    if (e[6]) {
        preset = (unsigned)(((int)preset + e[6] + PRESETS * 8) % PRESETS);
        load_preset(preset); px_flash(presets[preset].name, now, 700U); changed = 1;
    }
    if (e[1]) {
        selected = (unsigned)(((int)selected + e[1] + NV * 8) % NV);
        if (!cr78_running) (void)cr78_audition(selected, 0);
        changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t k1 = e[2], k2 = e[3], k3 = e[4], k4 = e[5];
    if (k1 || k2 || k3 || k4) {
        changed = 1;
        if (held_steps) {   /* a held step: its ratchet, its accent */
            for (unsigned st = 0; st < 16; ++st) {
                if (!((held_steps >> st) & 1U)) continue;
                if (k1) set_ratchet(st, (int)cr78.ratchet[selected][st] + k1);
                if (k2) cr78.accent = (uint16_t)(k2 > 0 ? cr78.accent | (1U << st) : cr78.accent & ~(1U << st));
            }
        } else {
            unsigned last = k4 ? 3U : k3 ? 2U : k2 ? 1U : 0U;
            for (unsigned k = 0; k < 4; ++k) {
                const struct knob *kn = knob_at(screen, k);
                int turn = e[2 + k];
                if (!turn || !kn->label[0]) continue;
                if (kn->kind == K_TEMPO) { groove_tempo_edit(turn); continue; }
                int16_t *ref = screen == LIVE ? &live_val[page[LIVE]][k] : voice_ref(selected, page[VOICE], k);
                if (!ref) continue;
                int x = clamp(*ref + turn * kn->step, kn->lo, kn->hi);
                if (kn->kind == K_FILTER && x > -3 && x < 3) x = 0;   /* the middle: off */
                *ref = (int16_t)x;
            }
            if (screen == LIVE) { apply_live(); apply_globals(); }
            else { build_voice(selected); if (!cr78_running) (void)cr78_audition(selected, 0); }
            if (knob_at(screen, last)->label[0]) readout(screen, last, now);
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
        step_leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}

/* ---- the kit, for other panels (GROOVEBOX's DRUMS track): the same voices, patterns and sequencer ---- */
void cr78_kit_ready(void)
{
    if (!initialized) init();
    cr78.armed = 1;
    cr78.fm1_swing = (uint8_t)groove_sequence_setting(0);
}
unsigned cr78_black_voice(unsigned i) { return i < PX_PADS ? black_voice[i] : 0; }
const char *const *cr78_voice_row(void) { return voice_pad; }
const char *cr78_voice_name(unsigned v) { return names[v % NV]; }
const char *cr78_voice_short(unsigned v) { return short_names[v % NV]; }
unsigned cr78_voice_color(unsigned v) { return row_color[v % NV]; }
const struct px_page *cr78_voice_pages(unsigned *count) { *count = VOICE_PAGES; return voice_page; }
void cr78_voice_text(unsigned v, unsigned pg, unsigned k, char *out)
{
    int16_t *r = voice_ref(v, pg, k);
    fmt_knob(&voice_knob[pg][k], r ? *r : 0, out);
}
void cr78_voice_turn(unsigned v, unsigned pg, unsigned k, int turn)
{
    const struct knob *kn = &voice_knob[pg][k];
    int16_t *r = voice_ref(v, pg, k);
    if (!r || !kn->label[0] || !turn) return;
    *r = (int16_t)clamp(*r + turn * kn->step, kn->lo, kn->hi);
    build_voice(v);
}
const char *cr78_voice_knob(unsigned pg, unsigned k) { return voice_knob[pg][k].label; }
unsigned cr78_preset_count(void) { return PRESETS; }
unsigned cr78_preset_index(void) { return preset; }
const char *cr78_preset_name(unsigned p) { return presets[p % PRESETS].name; }
void cr78_preset(unsigned p) { preset = p % PRESETS; load_preset(preset); }
void cr78_set_ratchet(unsigned v, unsigned s, int r)
{
    cr78.ratchet[v][s] = (uint8_t)clamp(r, 1, CR78_RATCHET_MAX);
    cr78.grid[v] |= (uint16_t)(1U << s);
}
/* the voice grid, the way the CR-78 screen draws it, with `v` marked */
void cr78_draw_kit(unsigned v, uint32_t now)
{
    uint16_t hits = cr78_hits; cr78_hits = 0;
    for (unsigned i = 0; i < NV; ++i) if (hits & (1U << i)) flash_until[i] = now + 90U;
    unsigned keep = selected; selected = v;
    draw_grid(now);
    selected = keep;
}

/* ---- the kit's sound settings, for saving (GROOVEBOX) ---- */
unsigned cr78_kit_bytes(void) { return sizeof shape + sizeof cut; }
void cr78_kit_get(void *out) { memcpy(out, shape, sizeof shape); memcpy((uint8_t *)out + sizeof shape, cut, sizeof cut); }
void cr78_kit_set(const void *in)
{
    if (!initialized) init();
    memcpy(shape, in, sizeof shape); memcpy(cut, (const uint8_t *)in + sizeof shape, sizeof cut);
    for (unsigned v = 0; v < NV; ++v) build_voice(v);
}

/* a voice back to as built, and one knob of its pages (SOUND PITCH BODY MOD LEVEL) set outright */
void cr78_voice_reset(unsigned v)
{
    if (!initialized) init();
    if (v >= NV) return;
    shape_defaults(v); build_voice(v);
}
void cr78_voice_set(unsigned v, unsigned pg, unsigned k, int value)
{
    if (!initialized) init();
    if (v >= NV || pg >= VOICE_PAGES || k > 3U) return;
    const struct knob *kn = &voice_knob[pg][k];
    int16_t *r = voice_ref(v, pg, k);
    if (!r || !kn->label[0]) return;
    *r = (int16_t)clamp(value, kn->lo, kn->hi);
    build_voice(v);
}
