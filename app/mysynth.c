/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* MY SYNTH panel mode. HOME+ENV toggles it.
 *   27 keys   play the synth (6 voices), their LEDs show what is held
 *   OCT -/+   octave shift, -2..+2
 *   SELECT WAVE   ALGORITHM DETUNE   KNOB 1-4 CUTOFF RESO ENV ATTACK
 *   PRESETS RELEASE   (encoder indices 0..6 in that order)
 * While the mode is on, the keys play MY SYNTH instead of the tracks; the
 * sequencer, external MIDI and the engine keep running underneath.
 * No libm: the few curves needed here are computed with small helpers. */
#include "mysynth.h"
#include "pixel.h"
#include <string.h>

enum { WAVE, DETUNE, CUTOFF, RESO, ENVMOD, ATTACK, RELEASE, PARAMS };
static const char *const names[PARAMS] = {"WAVE", "DTUN", "CUT", "RES", "ENV", "ATK", "REL"};
static const char *const knobs[PARAMS] = {"SEL", "ALG", "K1", "K2", "K3", "K4", "PRE"};
static const uint8_t defaults[PARAMS] = {24, 36, 64, 50, 70, 4, 50};
static uint8_t value[PARAMS];
static uint8_t held_note[PX_KEYS];
static int octave;
static unsigned active, initialized, frame, touched = PARAMS, full_redraw;
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
static float unit(unsigned parameter) { return (float)value[parameter] / 127.0f; }
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
static void apply(void)
{
    struct mysynth_params *p = &mysynth_params;
    float d = unit(DETUNE);
    float cents = 40.0f * d * d;
    p->wave = (int32_t)(unit(WAVE) * 32767.0f);
    p->detune = (int32_t)((exp2_approx(cents / 1200.0f) - 1.0f) * 65536.0f);
    p->cutoff = value[CUTOFF] * 2;
    p->damping = (int32_t)((1.4f - 1.34f * unit(RESO)) * 32768.0f);
    p->envmod = value[ENVMOD] * 3 / 2;
    float attack = 0.001f * exp2_approx(unit(ATTACK) * 11.0f);          /* 1 ms .. 2 s */
    float blocks = attack * 44100.0f / (float)FM1_BLOCK_FRAMES;
    p->attack = blocks < 1.0f ? (1 << 30) : (int32_t)((float)(1 << 30) / blocks);
    p->decay = block_decay(0.35f);
    p->sustain = (int32_t)(0.6f * (float)(1 << 30));
    p->release = block_decay(0.005f * exp2_approx(unit(RELEASE) * 9.2f)); /* 5 ms .. 3 s */
    p->level = 26000;
}
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
static void draw(uint32_t now)
{
    char text[16];
    px_cls(PX_NIGHT);
    px_print("MY SYNTH", 2, 2, PX_GOLD);
    memcpy(text, "OCT ", 5);
    if (octave > 0) { text[4] = '+'; px_number(octave, text + 5); }
    else px_number(octave, text + 4);
    px_print(text, PX_W - 2 - ((int)strlen(text) * 4 - 1), 2, PX_SLATE);
    px_rect(0, 9, PX_W, 1, PX_DUSK);
    unsigned recent = touched < PARAMS && now - touched_at < 1200U;
    for (unsigned i = 0; i < PARAMS; ++i) {
        int y = 12 + (int)i * 7;
        unsigned hot = recent && touched == i;
        px_print(names[i], 2, y, hot ? PX_GOLD : PX_WHITE);
        px_rect(20, y + 1, 44, 3, PX_DUSK);
        px_rect(20, y + 1, (value[i] * 44 + 63) / 127, 3, hot ? PX_GOLD : PX_CYAN);
        px_print(knobs[i], PX_W - 1 - ((int)strlen(knobs[i]) * 4 - 1), y, PX_SLATE);
    }
    /* scope: find a rising zero crossing, then draw 80 points, 4 samples apart */
    const int top = 63, mid = 71;
    px_rect(0, top - 2, PX_W, 1, PX_DUSK);
    if (!mysynth_voices_active()) {
        px_rect(0, mid, PX_W, 1, PX_DUSK);
        px_print_c("HOME ENV EXIT", 73, PX_SLATE);
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
        if (y > top + 16) y = top + 16;
        for (int j = prev < y ? prev : y; j <= (prev < y ? y : prev); ++j) px_pset(x, j, PX_GREEN);
        prev = y;
    }
}

unsigned mysynth_active(void) { return active; }

/* 0: not running, 1: owns the panel, 2: closing on this poll. */
int mysynth_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys, up = prev_keys & ~c->keys;
    unsigned combo = px_combo(c->buttons, pressed, PX_HOME, PX_ENV);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!combo) return 0;
        if (!initialized) { memcpy(value, defaults, sizeof value); build_ftab(); initialized = 1; }
        apply();
        px_leds_off();
        memset(held_note, 0, sizeof held_note);
        active = 1; full_redraw = 1; last_ms = now - 100U;
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
    for (unsigned i = 0; i < PARAMS; ++i) {
        if (!c->encoders[i]) continue;
        int v = (int)value[i] + c->encoders[i] * 2;
        value[i] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        touched = i; touched_at = now; changed = 1;
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
