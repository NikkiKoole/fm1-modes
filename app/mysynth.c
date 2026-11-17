/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* MY SYNTH panel mode, in the shared grammar: SELECT pages, the four KNOBs
 * are labelled in the bottom bar, PRESETS loads a patch, HOME goes back.
 *
 *   OSC     WAVE    DETUNE  OCTAVE  LEVEL
 *   FILTER  CUTOFF  RESO    ENV
 *   AMP     ATTACK  DECAY   SUSTAIN RELEASE
 *
 *   27 keys   play the synth (6 voices), their LEDs show what is held
 *   OCT -/+   octave shift, -2..+2 (the same as OSC KNOB 3)
 * While the mode is on, the keys play MY SYNTH instead of the tracks; the
 * sequencer, external MIDI and the engine keep running underneath.
 * No libm: the few curves needed here are computed with small helpers. */
#include "mysynth.h"
#include "pixel.h"
#include <string.h>

enum { WAVE, DETUNE, LEVEL, CUTOFF, RESO, ENVMOD, ATTACK, DECAY, SUSTAIN, RELEASE, PARAMS, OCTAVE = PARAMS, NONE };
enum { OSC, FILTER, AMP, PAGES };
static const struct px_page pages[PAGES] = {
    {"OSC", {"WAVE", "DTUN", "OCT", "LVL"}},
    {"FILTER", {"CUT", "RES", "ENV", ""}},
    {"AMP", {"ATK", "DEC", "SUS", "REL"}},
};
/* what each page's four knobs edit */
static const uint8_t page_param[PAGES][4] = {
    {WAVE, DETUNE, OCTAVE, LEVEL}, {CUTOFF, RESO, ENVMOD, NONE}, {ATTACK, DECAY, SUSTAIN, RELEASE}};
/* patches (PRESETS): WAVE DTUN LVL  CUT RES ENV  ATK DEC SUS REL */
struct patch { const char *name; uint8_t value[PARAMS]; };
static const struct patch patches[] = {
    {"INIT", {24, 36, 100, 64, 50, 70, 4, 73, 76, 50}},
    {"PAD", {30, 90, 100, 50, 30, 40, 90, 90, 100, 100}},
    {"BRASS", {10, 40, 100, 40, 40, 90, 30, 60, 90, 50}},
    {"PLUCK", {60, 20, 100, 30, 70, 110, 0, 45, 0, 55}},
    {"BASS", {100, 10, 110, 30, 80, 80, 0, 50, 60, 30}},
    {"LEAD", {127, 60, 95, 80, 60, 50, 6, 70, 100, 45}},
};
enum { PATCHES = sizeof patches / sizeof patches[0] };
static uint8_t value[PARAMS];
static uint8_t held_note[PX_KEYS];
static int octave;
static unsigned active, initialized, frame, touched = NONE, full_redraw, page, patch;
static uint32_t prev_buttons, prev_keys, last_ms, touched_at;

/* ---- tiny float helpers (control task only) ---- */
static float exp2_approx(float x)               /* good to ~0.01% over our range */
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float sin_small(float x)                 /* |x| <= ~1 */
{
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f)));
}
/* per-block multiplier for an exponential with time constant `seconds` */
static int32_t block_decay(float seconds)
{
    float blocks = seconds * 44100.0f / (float)FM1_BLOCK_FRAMES;
    return (int32_t)(exp2_approx(-1.442695f / blocks) * 32768.0f);
}

static void build_ftab(void)
{
    /* 25 Hz .. ~18 kHz over 256 steps; f = 2 sin(pi fc / (2 fs)) for the 2x SVF */
    for (unsigned i = 0; i < MYSYNTH_FTAB; ++i) {
        float fc = 25.0f * exp2_approx((float)i * 9.5f / 256.0f);
        float f = 2.0f * sin_small(3.1415927f * fc / 88200.0f);
        if (f > 1.2f) f = 1.2f;
        mysynth_params.ftab[i] = (int32_t)(f * 32768.0f);
    }
}
/* a sound's knob values -> its coefficients (MY SYNTH's own, or the LEAD's) */
static void apply_to(const uint8_t *val, struct mysynth_params *p)
{
#define unit(x) ((float)val[x] / 127.0f)
    float d = unit(DETUNE);
    float cents = 40.0f * d * d;
    p->wave = (int32_t)(unit(WAVE) * 32767.0f);
    p->detune = (int32_t)((exp2_approx(cents / 1200.0f) - 1.0f) * 65536.0f);
    p->cutoff = val[CUTOFF] * 2;
    p->damping = (int32_t)((1.4f - 1.34f * unit(RESO)) * 32768.0f);
    p->envmod = val[ENVMOD] * 3 / 2;
    float attack = 0.001f * exp2_approx(unit(ATTACK) * 11.0f);          /* 1 ms .. 2 s */
    float blocks = attack * 44100.0f / (float)FM1_BLOCK_FRAMES;
    p->attack = blocks < 1.0f ? (1 << 30) : (int32_t)((float)(1 << 30) / blocks);
    p->decay = block_decay(0.02f * exp2_approx(unit(DECAY) * 7.2f));     /* 20 ms .. 3 s */
    p->sustain = (int32_t)(unit(SUSTAIN) * (float)(1 << 30));
    p->release = block_decay(0.005f * exp2_approx(unit(RELEASE) * 9.2f)); /* 5 ms .. 3 s */
    int level = val[LEVEL] * 260;
    p->level = level > 32767 ? 32767 : level;
#undef unit
}
static void apply(void) { apply_to(value, &mysynth_params); }
static void note_on(unsigned key)
{
    int note = PX_FIRST_NOTE + (int)key + 12 * octave;
    if (note < 0 || note > 127) return;
    float hz = 440.0f * exp2_approx((float)(note - 69) / 12.0f);
    float inc = hz * 4294967296.0f / 44100.0f;
    uint32_t rec = (uint32_t)(140737488355328.0f / inc);   /* 2^47 / inc */
    if (mysynth_event(MYSYNTH_NOTE_ON, (unsigned)note, 110, (uint32_t)inc, rec) == 0)
        held_note[key] = (uint8_t)note;
    fm1_led_set(PX_KEY_LED + key, 1);
}
static void note_off(unsigned key)
{
    if (held_note[key]) (void)mysynth_event(MYSYNTH_NOTE_OFF, held_note[key], 0, 0, 0);
    held_note[key] = 0;
    fm1_led_set(PX_KEY_LED + key, 0);
}

/* ---- screen ---- */
static void param_text(unsigned param, char *out)
{
    if (param == OCTAVE) {
        if (octave > 0) { *out++ = '+'; }
        px_number(octave, out);
    } else if (param < PARAMS) px_number(value[param], out);
    else *out = 0;
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *values[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    for (unsigned k = 0; k < 4; ++k) param_text(page_param[page][k], v[k]);
    px_chrome("SYNTH", pages, PAGES, page, values);
    px_print(patches[patch].name, PX_W - 1 - ((int)strlen(patches[patch].name) * 4 - 1), 8, PX_SLATE);
    /* this page's parameters as bars, the one you are turning in gold */
    unsigned recent = touched != NONE && now - touched_at < 1200U;
    for (unsigned k = 0; k < 4; ++k) {
        unsigned param = page_param[page][k];
        if (param == NONE) continue;
        int y = 15 + (int)k * 7;
        unsigned hot = recent && touched == param;
        int fill = param == OCTAVE ? (octave + 2) * 11 : (value[param] * 44 + 63) / 127;
        px_print(pages[page].knob[k], 2, y, hot ? PX_GOLD : PX_WHITE);
        px_rect(20, y + 1, 44, 3, PX_DUSK);
        px_rect(20, y + 1, fill, 3, hot ? PX_GOLD : PX_CYAN);
    }
    /* scope: find a rising zero crossing, then draw 80 points, 4 samples apart */
    const int top = 45, mid = 54;
    px_rect(0, top - 2, PX_W, 1, PX_DUSK);
    if (!mysynth_voices_active()) {
        px_rect(0, mid, PX_W, 1, PX_DUSK);
        px_flash_draw(now);
        return;
    }
    uint32_t end = mysynth_scope_pos + MYSYNTH_SCOPE, start = end - 330U;
    for (uint32_t s = end - 330U; s > end - MYSYNTH_SCOPE + 1U; --s) {
        int16_t a = mysynth_scope[(s - 1U) % MYSYNTH_SCOPE], b = mysynth_scope[s % MYSYNTH_SCOPE];
        if (a < 0 && b >= 0) { start = s; break; }
    }
    int prev = mid;
    for (int x = 0; x < PX_W; ++x) {
        int s = mysynth_scope[(start + (uint32_t)x * 4U) % MYSYNTH_SCOPE];
        int y = mid - s * 24 / 32768;   /* zoomed: clamps at the box edge */
        if (y < top) y = top;
        if (y > top + 18) y = top + 18;
        for (int j = prev < y ? prev : y; j <= (prev < y ? y : prev); ++j) px_pset(x, j, PX_GREEN);
        prev = y;
    }
    px_flash_draw(now);
}

unsigned mysynth_active(void) { return active; }

/* 0: not running, 1: owns the panel, 2: closing on this poll. */
int mysynth_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys, up = prev_keys & ~c->keys;
    unsigned combo = px_mode_toggle(pressed, PX_MODE_SYNTH, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!combo) return 0;
        if (!initialized) { memcpy(value, patches[0].value, sizeof value); build_ftab(); initialized = 1; }
        apply();
        px_leds_off();
        memset(held_note, 0, sizeof held_note);
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(pages[page].name, now, 700U);
        return 1;
    }
    if (combo) {
        (void)mysynth_event(MYSYNTH_ALL_OFF, 0, 0, 0, 0);
        memset(held_note, 0, sizeof held_note);
        px_leds_off();
        active = 0;
        return 2;
    }
    for (unsigned k = 0; k < PX_KEYS; ++k) {
        if (up & PX_BIT(k)) note_off(k);
        if (down & PX_BIT(k)) note_on(k);
    }
    if ((pressed & PX_BIT(PX_OCT_DOWN)) && octave > -2) --octave;
    if ((pressed & PX_BIT(PX_OCT_UP)) && octave < 2) ++octave;
    unsigned changed = 0;
    const int16_t *e = c->encoders;
    /* SELECT: pages */
    if (e[0]) {
        int p = (int)page + e[0];
        page = (unsigned)(p < 0 ? 0 : p >= PAGES ? PAGES - 1 : p);
        px_flash(pages[page].name, now, 700U); changed = 1;
    }
    /* PRESETS: patches */
    if (e[6]) {
        patch = (unsigned)(((int)patch + e[6] + PATCHES * 8) % PATCHES);
        memcpy(value, patches[patch].value, sizeof value);
        px_flash(patches[patch].name, now, 700U); changed = 1;
    }
    /* KNOBS 1-4: this page's parameters */
    for (unsigned k = 0; k < 4; ++k) {
        int turn = e[2 + k];
        unsigned param = page_param[page][k];
        if (!turn || param == NONE) continue;
        if (param == OCTAVE) {
            int o = octave + turn;
            octave = o < -2 ? -2 : o > 2 ? 2 : o;
        } else {
            int v = (int)value[param] + turn * 2;
            value[param] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        }
        touched = param; touched_at = now; changed = 1;
    }
    if (changed) apply();
    if (now - last_ms >= 33U || changed || pressed) {
        last_ms = now; ++frame;
        draw(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}

/* ---- the sound, for other panels (the GROOVEBOX's KEYS track) ---- */
void mysynth_ready(void)
{
    if (!initialized) { memcpy(value, patches[0].value, sizeof value); build_ftab(); initialized = 1; }
    apply();
}
const struct px_page *mysynth_pages(unsigned *count) { *count = PAGES; return pages; }
void mysynth_knob_text(unsigned pg, unsigned k, char *out) { param_text(page_param[pg % PAGES][k & 3U], out); }
void mysynth_turn(unsigned pg, unsigned k, int turn)
{
    unsigned param = page_param[pg % PAGES][k & 3U];
    if (!turn || param >= PARAMS) return;   /* (OCTAVE is the MY SYNTH keyboard's) */
    int v = (int)value[param] + turn * 2;
    value[param] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    apply();
}
unsigned mysynth_patch_count(void) { return PATCHES; }
unsigned mysynth_patch_index(void) { return patch; }
const char *mysynth_patch_name(unsigned p) { return patches[p % PATCHES].name; }
void mysynth_patch(unsigned p) { patch = p % PATCHES; memcpy(value, patches[patch].value, sizeof value); apply(); }

/* ---- the LEAD's own sound: the same pages and patches, GLIDE in place of OCTAVE ---- */
static const struct px_page lead_pages[PAGES] = {
    {"OSC", {"WAVE", "DTUN", "GLDE", "LVL"}},
    {"FILTER", {"CUT", "RES", "ENV", ""}},
    {"AMP", {"ATK", "DEC", "SUS", "REL"}},
};
static uint8_t lead_value[PARAMS], lead_glide = 40;
static unsigned lead_patch = 5, lead_initialized;   /* LEAD */
static void lead_apply(void)
{
    apply_to(lead_value, &mysynth_lead_params);
    /* glide: 0 at once; else a time constant of ~4 ms .. 0.6 s */
    mysynth_lead_params.glide = lead_glide ? 32768 - block_decay(0.004f * exp2_approx((float)lead_glide * 7.2f / 127.0f)) : 32768;
}
void mysynth_lead_ready(void)
{
    mysynth_ready();   /* (the filter table) */
    if (!lead_initialized) {
        memcpy(lead_value, patches[lead_patch].value, sizeof lead_value);
        memcpy(mysynth_lead_params.ftab, mysynth_params.ftab, sizeof mysynth_lead_params.ftab);
        lead_initialized = 1;
    }
    lead_apply();
}
const struct px_page *mysynth_lead_pages(unsigned *count) { *count = PAGES; return lead_pages; }
void mysynth_lead_knob_text(unsigned pg, unsigned k, char *out)
{
    unsigned param = page_param[pg % PAGES][k & 3U];
    if (param == OCTAVE) { if (lead_glide) px_number(lead_glide, out); else strcpy(out, "OFF"); }
    else if (param < PARAMS) px_number(lead_value[param], out);
    else *out = 0;
}
void mysynth_lead_turn(unsigned pg, unsigned k, int turn)
{
    unsigned param = page_param[pg % PAGES][k & 3U];
    if (!turn || param == NONE) return;
    uint8_t *x = param == OCTAVE ? &lead_glide : &lead_value[param];
    int v = (int)*x + turn * 2;
    *x = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    lead_apply();
}
unsigned mysynth_lead_patch_index(void) { return lead_patch; }
void mysynth_lead_patch(unsigned p) { lead_patch = p % PATCHES; memcpy(lead_value, patches[lead_patch].value, sizeof lead_value); lead_apply(); }
