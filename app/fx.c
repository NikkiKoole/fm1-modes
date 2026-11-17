/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* FX panel mode, in the shared grammar: SELECT pages, the four KNOBs are
 * labelled in the bottom bar, PRESETS loads a rack, HOME goes back to the
 * launcher. The rack keeps working after you leave.
 *
 *   SEND    BASS   CR78   RHY    SYN       how much of each mode goes through the rack
 *   SPACE   MIX    SIZE   DAMP   COIL      the spring (COIL = its dispersion, the boing)
 *   COLOR   DRIVE  BITS   RATE   EQ        drive, bitcrush, console EQ curve
 *   MOTION  TYPE   RATE   DEPTH  MIX       chorus, phaser, vibe, flanger
 *   DUCK    AMT    REL    KEY    FM1       our kicks pump the bass and the synth
 *                                          (and the FM-1's own tracks with FM1 on)
 *
 *   FX   bypass the whole rack (the settings stay)
 *
 * The coefficients follow dreamengine's setters (fx_set_drive, fx_set_crush,
 * outboard.h's EQ curves, chorus/phaser/flanger, reverb + reverb_spring). */
#include "fx.h"
#include "pixel.h"
#include "drumvoice.h"
#include "instrument.h"
#include <string.h>

enum { SEND, SPACE, COLOR, MOTION, DUCK, PAGES };
static const struct px_page pages[PAGES] = {
    {"SEND", {"BASS", "CR78", "RHY", "SYN"}},
    {"SPACE", {"MIX", "SIZE", "DAMP", "COIL"}},
    {"COLOR", {"DRIV", "BITS", "RATE", "EQ"}},
    {"MOTION", {"TYPE", "RATE", "DPTH", "MIX"}},
    {"DUCK", {"AMT", "REL", "KEY", "FM1"}},
};
static const char *const eq_name[] = {"OFF", "WARM", "AIR", "SCOOP"};
static const float eq_curve[4][3] = {{0, 0, 0}, {5.0f, -2.0f, 2.0f}, {1.0f, -1.0f, 7.0f}, {3.0f, -7.0f, 3.0f}};
static const char *const motion_name[FX_MOTIONS] = {"OFF", "CHOR", "PHAS", "VIBE", "FLNG"};
static const char *const key_name[] = {"", "CR78", "RHY", "BOTH"};

/* everything a preset holds; values are what the knobs show */
struct rack {
    uint8_t send[FX_BUSES];
    uint8_t space_mix, size, damp, coil;
    uint8_t drive, bits, rate, eq;
    uint8_t motion, motion_rate, depth, motion_mix;
    uint8_t duck, release, key, fm1;   /* release in 10 ms */
};
struct preset { const char *name; struct rack r; };
#define R(sb, sc, sr, ss, mx, sz, dp, cl, dr, bt, rt, eq, mt, mr, md, mm, da, dl, dk, df) \
    {{sb, sc, sr, ss}, mx, sz, dp, cl, dr, bt, rt, eq, mt, mr, md, mm, da, dl, dk, df}
static const struct preset presets[] = {
    {"DRY",    R(0, 0, 0, 0,        0, 50, 40, 60,   0, 16, 1, 0,   0, 30, 50, 50,   0, 25, 3, 0)},
    {"SPRING", R(40, 50, 50, 50,   50, 55, 40, 60,   0, 16, 1, 0,   0, 30, 50, 50,   0, 25, 3, 0)},
    {"DUB",    R(50, 60, 50, 60,   70, 85, 60, 70,   0, 16, 1, 0,   1, 20, 40, 30,   0, 25, 3, 0)},
    {"LOFI",   R(100, 100, 100, 100, 15, 40, 60, 50, 0, 8, 3, 1,    0, 30, 50, 50,   0, 25, 3, 0)},
    {"PUMP",   R(0, 0, 0, 0,        0, 50, 40, 60,   0, 16, 1, 0,   0, 30, 50, 50,  70, 25, 3, 1)},
    {"PHASE",  R(40, 0, 30, 100,   15, 50, 40, 60,   0, 16, 1, 0,   2, 25, 70, 50,   0, 25, 3, 0)},
    {"WIDE",   R(60, 0, 0, 100,    20, 60, 40, 50,   0, 16, 1, 0,   1, 30, 50, 50,   0, 25, 3, 0)},
    {"VIBE",   R(0, 0, 60, 100,    20, 50, 50, 50,   0, 16, 1, 0,   3, 40, 80, 50,   0, 25, 3, 0)},
    {"DIRTY",  R(80, 80, 60, 60,   10, 40, 50, 50,  55, 16, 1, 3,   0, 30, 50, 50,  30, 20, 1, 0)},
};
#undef R
enum { PRESETS = sizeof presets / sizeof presets[0] };

static struct rack rk;
static unsigned active, initialized, full_redraw, page, preset, bypass;
static uint32_t prev_buttons, last_ms;

/* ---- tiny float helpers (control task only, no libm) ---- */
static float exp2_approx(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float tanh_approx(float x) { float e = exp2_approx(2.0f * x * 1.442695f); return (e - 1.0f) / (e + 1.0f); }
static int32_t q15(unsigned percent) { return (int32_t)(percent * 32768U / 100U); }
static int32_t db_q12(float db) { return (int32_t)(4096.0f * exp2_approx(db * 0.1660964f)); }

static void apply(void)
{
    struct fx_params *p = &fx_params;
    for (unsigned b = 0; b < FX_BUSES; ++b) p->send[b] = bypass ? 0 : q15(rk.send[b]);
    /* SPACE (reverb(size, damp) + reverb_spring) */
    p->spring_fb = (int32_t)((0.7f + 0.25f * (float)rk.size / 100.0f) * 32768.0f);
    p->spring_damp = (int32_t)((1.0f - 0.4f * (float)rk.damp / 100.0f) * 32768.0f);
    p->spring_disp = (int32_t)((0.2f + 0.7f * (float)rk.coil / 100.0f) * 32768.0f);
    p->spring_mix = q15(rk.space_mix) * 2;   /* up to +6 dB: the spring is band-limited to 75 Hz..4 kHz */
    /* COLOR: DRIVE_ASYM; the knob starts at amount 0.15 so the shaper never runs on a tiny gain */
    if (rk.drive) {
        float dr = 0.15f + 0.85f * (float)rk.drive / 100.0f, g = dr * dr * 24.0f;
        /* dreamengine normalizes full scale to full scale; on a bus that sits around -15 dB that
         * is up to +14 dB louder, so normalize at amplitude 0.2 instead: about level-matched */
        p->drive_norm = (int32_t)(32768.0f * 0.2f / tanh_approx(0.2f * g));
        p->drive_neg = (int32_t)(g * (1.0f - 0.4f * dr) * 4096.0f);
        p->drive_pos = (int32_t)(g * 4096.0f);
    } else p->drive_pos = 0;
    p->crush_step = 1 << (16 - rk.bits);
    p->crush_rate = rk.rate;
    p->eq_low = db_q12(eq_curve[rk.eq][0]); p->eq_mid = db_q12(eq_curve[rk.eq][1]); p->eq_high = db_q12(eq_curve[rk.eq][2]);
    p->eq_on = rk.eq != 0;
    /* MOTION: rate 0..100 -> 0.05..6.4 Hz */
    float hz = 0.05f * exp2_approx((float)rk.motion_rate * 0.07f);
    p->motion_inc = (int32_t)(uint32_t)(hz * 97391.55f);   /* 2^32 / 44100 */
    p->motion_depth = q15(rk.depth); p->motion_mix = q15(rk.motion_mix);
    p->motion = rk.motion;
    /* DUCK: release as a per-sample one-pole, 2^24 / (ms * 44.1) */
    p->duck_release = (int32_t)(16777216.0f / ((float)rk.release * 441.0f));
    p->duck_keys = rk.key; p->duck_fm1 = rk.fm1;
    p->duck_amount = bypass ? 0 : q15(rk.duck);
}
static void load(unsigned index) { rk = presets[index].r; apply(); }

/* ---- screen ---- */
static void bar(int x, int y, int w, unsigned value, unsigned max, unsigned color)
{
    px_rect(x, y, w, 3, PX_DUSK);
    px_rect(x, y, (int)(value * (unsigned)w / max), 3, color);
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    switch (page) {
    case SEND: for (unsigned k = 0; k < 4; ++k) px_number(rk.send[k], v[k]); break;
    case SPACE: px_number(rk.space_mix, v[0]); px_number(rk.size, v[1]); px_number(rk.damp, v[2]); px_number(rk.coil, v[3]); break;
    case COLOR:
        px_number(rk.drive, v[0]); px_number(rk.bits, v[1]); px_number(rk.rate, v[2]); strcpy(v[3], eq_name[rk.eq]);
        break;
    case MOTION:
        strcpy(v[0], motion_name[rk.motion]); px_number(rk.motion_rate, v[1]); px_number(rk.depth, v[2]);
        px_number(rk.motion_mix, v[3]);
        break;
    default:
        px_number(rk.duck, v[0]); px_number(rk.release * 10, v[1]); strcpy(v[2], key_name[rk.key]);
        strcpy(v[3], rk.fm1 ? "ON" : "OFF");
        break;
    }
    px_cls(PX_NIGHT);
    px_chrome("FX", pages, PAGES, page, value);
    if (bypass) px_print("BYPASS", PX_W - 24, 8, PX_RED);
    else px_print(presets[preset].name, PX_W - 1 - ((int)strlen(presets[preset].name) * 4 - 1), 8, PX_SLATE);
    /* the flow: four sends on the left, into the rack's stages on the right */
    static const char *const bus_name[FX_BUSES] = {"BASS", "CR78", "RHY", "SYN"};
    for (unsigned b = 0; b < FX_BUSES; ++b) {
        int y = 16 + (int)b * 7;
        unsigned hot = page == SEND;
        px_print(bus_name[b], 2, y, hot ? PX_WHITE : PX_SLATE);
        bar(20, y + 1, 24, rk.send[b], 100, rk.send[b] && !bypass ? (hot ? PX_GOLD : PX_CYAN) : PX_DUSK);
        if (rk.send[b] && !bypass) px_rect(45, y + 2, 3, 1, PX_SLATE);
    }
    px_rect(48, 18, 1, 22, PX_DUSK);   /* the summing rail */
    struct { const char *name; unsigned on, page; } stage[5] = {
        {"DRV", rk.drive != 0, COLOR}, {"CRS", rk.bits < 16 || rk.rate > 1, COLOR}, {"EQ", rk.eq != 0, COLOR},
        {motion_name[rk.motion], rk.motion != FX_MOTION_OFF && rk.motion_mix, MOTION}, {"SPR", rk.space_mix != 0, SPACE}};
    for (unsigned s = 0; s < 5; ++s) {
        int y = 13 + (int)s * 7;
        unsigned on = stage[s].on && !bypass, here = stage[s].page == page;
        px_rect(51, y, 27, 6, on ? (here ? PX_EMBER : PX_DUSK) : PX_NIGHT);
        px_print(s == 3 && rk.motion == FX_MOTION_OFF ? "MOT" : stage[s].name, 53, y + 1,
                 on ? PX_WHITE : here ? PX_SLATE : PX_DUSK);
    }
    /* meters: what the rack puts out, and how far the duck pulls down */
    int32_t peak = fx_wet_peak; fx_wet_peak = 0;
    static int32_t shown_peak;
    shown_peak = peak > shown_peak ? peak : shown_peak - (shown_peak >> 3);
    px_print("WET", 2, 50, PX_SLATE);
    bar(18, 51, 60, (unsigned)(shown_peak > 32767 ? 32767 : shown_peak), 32768, PX_GREEN);
    int32_t pulled = 32768 - fx_duck_gain;
    px_print("DUCK", 2, 57, page == DUCK ? PX_WHITE : PX_SLATE);
    bar(18, 58, 60, (unsigned)(pulled < 0 ? 0 : pulled), 32768, rk.duck ? PX_PINK : PX_DUSK);
    px_flash_draw(now);
}

unsigned fx_active(void) { return active; }
unsigned fx_running(void)
{
    unsigned on = fx_params.duck_amount != 0;
    for (unsigned b = 0; b < FX_BUSES; ++b) on |= fx_params.send[b] != 0;
    return on;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint8_t turn(uint8_t value, int delta, int step, int lo, int hi) { return (uint8_t)clampi((int)value + delta * step, lo, hi); }

int fx_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    unsigned toggle = px_mode_toggle(pressed, PX_MODE_FX, active);
    prev_buttons = c->buttons;
    if (!active) {
        if (!toggle) return 0;
        if (!initialized) { if (!dv.ready) dv_build_tables(); load(preset); initialized = 1; }
        px_leds_off();
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(pages[page].name, now, 700U);
        return 1;
    }
    if (toggle) { px_leds_off(); active = 0; return 2; }
    const int16_t *e = c->encoders;
    unsigned changed = 0;
    if (e[0]) { page = (unsigned)clampi((int)page + e[0], 0, PAGES - 1); px_flash(pages[page].name, now, 700U); changed = 1; }
    if (e[6]) {
        preset = (unsigned)(((int)preset + e[6] + PRESETS * 8) % PRESETS);
        load(preset); px_flash(presets[preset].name, now, 700U); changed = 1;
    }
    const int16_t k1 = e[2], k2 = e[3], k3 = e[4], k4 = e[5];
    if (k1 || k2 || k3 || k4) {
        switch (page) {
        case SEND:
            for (unsigned k = 0; k < 4; ++k) rk.send[k] = turn(rk.send[k], e[2 + k], 5, 0, 100);
            break;
        case SPACE:
            rk.space_mix = turn(rk.space_mix, k1, 5, 0, 100); rk.size = turn(rk.size, k2, 5, 0, 100);
            rk.damp = turn(rk.damp, k3, 5, 0, 100); rk.coil = turn(rk.coil, k4, 5, 0, 100);
            break;
        case COLOR:
            rk.drive = turn(rk.drive, k1, 5, 0, 100); rk.bits = turn(rk.bits, k2, 1, 4, 16);
            rk.rate = turn(rk.rate, k3, 1, 1, 16); rk.eq = turn(rk.eq, k4, 1, 0, 3);
            break;
        case MOTION:
            rk.motion = turn(rk.motion, k1, 1, 0, FX_MOTIONS - 1); rk.motion_rate = turn(rk.motion_rate, k2, 5, 0, 100);
            rk.depth = turn(rk.depth, k3, 5, 0, 100); rk.motion_mix = turn(rk.motion_mix, k4, 5, 0, 100);
            break;
        default:
            rk.duck = turn(rk.duck, k1, 5, 0, 100); rk.release = turn(rk.release, k2, 1, 5, 80);
            rk.key = turn(rk.key, k3, 1, 1, 3); if (k4) rk.fm1 = k4 > 0;
            break;
        }
        apply(); changed = 1;
    }
    if (pressed & PX_BIT(PX_FX)) { bypass ^= 1U; apply(); px_flash(bypass ? "BYPASS" : "ON", now, 600U); changed = 1; }
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
