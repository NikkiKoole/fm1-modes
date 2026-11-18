/* SPDX-License-Identifier: MIT
 * Copyright (c) 2009 Robin Schmidt (Open303), 2026 M-VAVE (C adaptation, third_party/synth/bass303.c),
 * 2026 the TEKNO additions (Devil Fish controls). MIT terms: THIRD_PARTY.md.
 *
 * TEKNO's ACID: one 303, made gnarly. The sound is Open303 as the FM-1's own 303 plays it (the
 * envelope mapping measured on a real unit, the TeeBee ladder at twice the rate, the 200 Hz
 * de-clicker, the RAT drive), with Devil Fish's controls bolted on:
 *   - a wider CUTOFF (100 Hz .. 4.9 kHz) and ENV MOD past the stock top
 *   - DECAY from 30 ms (a stock 303 stops at 200) and its own ACCENT DECAY (the two-decay)
 *   - SLIDE time
 *   - FILTER FM: the oscillator sweeps the filter at audio rate (the Devil Fish's growl)
 *   - filter TRACKING (the cutoff follows the note)
 *   - a SUB square an octave down, into the filter with the saw (not a Devil Fish thing: the
 *     sub-osc of dreamengine's acid303.h, for weight)
 * Float (the FM-1 has a hardware FPU; the stock 303 is float too). Every helper is forced inline so
 * nothing runs from Flash in the audio interrupt. Called a sample at a time from tekno_render. */
#include "tekno.h"

#define INLINE static inline __attribute__((always_inline))
INLINE float fmax_(float a, float b) { return a > b ? a : b; }
INLINE float fclamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
INLINE float fexp2(float x)   /* Taylor after reducing to [-0.5, 0.5] (as dsp_math.h) */
{
    x = fclamp(x, -120.0f, 120.0f);
    int n = (int)(x + (x < 0 ? -0.5f : 0.5f));
    float a = (x - (float)n) * 0.6931471805599453f;
    float p = 1.0f + a * (1.0f + a * (0.5f + a * (1.0f / 6.0f + a * (1.0f / 24.0f + a * (1.0f / 120.0f + a / 720.0f)))));
    union { uint32_t u; float f; } s;
    s.u = (uint32_t)(n + 127) << 23;
    return p * s.f;
}
INLINE float ftanh(float x)
{
    if (x >= 8.0f) return 1.0f;
    if (x <= -8.0f) return -1.0f;
    float p = fexp2(2.8853900817779268f * x);
    return (p - 1.0f) / (p + 1.0f);
}
struct svf { float low, band; };
INLINE float svf_run(struct svf *s, const struct tekno_svf *c, float x, unsigned mode)
{
    float hp = (x - (c->r + c->g) * s->band - s->low) * c->h;
    float bp = c->g * hp + s->band; s->band = bp + c->g * hp;
    float lp = c->g * bp + s->low; s->low = lp + c->g * bp;
    return mode == 0 ? lp : mode == 1 ? bp : mode == 2 ? hp : lp + hp;
}
struct hp { float x, y; };
INLINE float hp_run(struct hp *s, float a, float x) { float y = 0.5f * (1.0f + a) * (x - s->x) + a * s->y; s->x = x; s->y = y; return y; }
struct lp { float z1, z2; };
INLINE float lp_run(struct lp *s, const float *k, float x)   /* k: b0 a1 a2 (b1 = 2 b0, b2 = b0) */
{
    float v = k[0] * x, y = v + s->z1;
    s->z1 = 2.0f * v - k[1] * y + s->z2; s->z2 = v - k[2] * y;
    return y;
}
INLINE float blep(float p, float d)
{
    if (p < d) { float x = p / d; return 2.0f * x - x * x - 1.0f; }
    if (p > 1.0f - d) { float x = (p - 1.0f) / d; return x * x + 2.0f * x + 1.0f; }
    return 0.0f;
}

static struct {
    int gate, idle, started, accent_note;
    unsigned control_phase;
    float frequency, pitch, phase, cutoff, env, amp, rc1, rc2, release, sub;
    float y[4], b0, k, comp;
    struct hp pre, feedback, post, drive_dc;
    struct svf click, notch, drive_pre, drive_post;
    struct lp down[2];
    float ap_x, ap_y;
} v = {.idle = 1};

FM1_AUDIO_CODE void tekno_acid_note(int note, int accent, int slide)
{
    const struct tekno_acid *a = &tekno.acid;
    float semis = (float)(note - 69);
    v.frequency = 440.0f * fexp2(semis * (1.0f / 12.0f));
    v.cutoff = a->cutoff * fexp2(a->track * (float)(note - a->base) * (1.0f / 12.0f));   /* TRACK */
    v.accent_note = accent != 0;
    v.release = accent ? a->release_acc : a->release;
    if (!slide || !v.started || !v.gate) { v.pitch = v.frequency; v.env = 1.0f; v.amp = 1.0f; }   /* a slide glides, no new envelope */
    if (v.idle) {   /* after a silence: from cleared filters, as Open303 does */
        for (unsigned i = 0; i < 4; ++i) v.y[i] = 0.0f;
        v.rc1 = v.rc2 = 0.0f;
    }
    v.gate = 1; v.idle = 0; v.started = 1; v.control_phase = 0;
}
FM1_AUDIO_CODE void tekno_acid_off(void) { v.gate = 0; }
FM1_AUDIO_CODE unsigned tekno_acid_sounding(void) { return !v.idle; }

static FM1_AUDIO_CODE void ladder_coeff(float hz)
{
    const struct tekno_acid *a = &tekno.acid;
    float f = fclamp(hz, 200.0f, 20000.0f) * (0.7071067811865f / 88200.0f);
    v.b0 = (0.00045522346f + 6.1922189f * f) / (1.0f + 12.358354f * f + 4.4156345f * f * f);
    float k = f * (f * (f * (f * (f * (f + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f) + 16.998792f;
    v.comp = 2.0f * ((k / 17.0f - 1.0f) * a->reso + 1.0f) * (1.0f + a->reso);
    v.k = k * a->reso;
}

/* one sample, Q15 (added to the mix by the caller) */
FM1_AUDIO_CODE int32_t tekno_acid_sample(void)
{
    if (v.idle) return 0;
    const struct tekno_acid *a = &tekno.acid;
    float decay = v.accent_note ? a->accent_decay : a->decay;
    float accent = v.accent_note ? a->accent : 0.0f;
    v.pitch = v.frequency + a->slide * (v.pitch - v.frequency);
    float env = v.env; v.env *= decay;
    v.rc1 = env; v.rc2 += (accent > 0.0f ? env - v.rc2 : -v.rc2) * 0.001510574f;
    if (!(v.control_phase++ & 3U)) ladder_coeff(v.cutoff * fexp2(a->env_scale * (v.rc1 - a->env_offset) + accent * v.rc2));
    float amp = v.amp; v.amp *= v.gate ? 0.999981565f : v.release;
    if (v.gate) amp += (0.45f + 4.0f * accent) * env;
    amp = svf_run(&v.click, &a->click, amp, 0);
    float sample = 0.0f, inc = v.pitch * (1.0f / 88200.0f);
    float ffm = a->ffm, subl = a->sub, b0max = a->b0max;
    for (unsigned os = 0; os < 2; ++os) {
        float p = v.phase;
        float saw = -(2.0f * p - 1.0f - blep(p, inc));
        v.phase += inc;
        if (v.phase >= 1.0f) { v.phase -= 1.0f; v.sub = v.sub > 0.0f ? -1.0f : 1.0f; }   /* the sub flips every cycle: an octave down */
        float x = saw + subl * v.sub;
        x = hp_run(&v.pre, a->pre_a, x) - hp_run(&v.feedback, a->fb_a, v.k * v.y[3]);
        float b0 = v.b0;
        if (ffm != 0.0f) b0 = fclamp(b0 * (1.0f + ffm * saw), 0.0f, b0max);   /* FILTER FM: the saw opens and shuts the filter */
        v.y[0] += 2.0f * b0 * (x - v.y[0] + v.y[1]);
        v.y[1] += b0 * (v.y[0] - 2.0f * v.y[1] + v.y[2]);
        v.y[2] += b0 * (v.y[1] - 2.0f * v.y[2] + v.y[3]);
        v.y[3] += b0 * (v.y[2] - 2.0f * v.y[3]);
        v.y[3] = fclamp(v.y[3], -8.0f, 8.0f);
        sample = lp_run(&v.down[1], a->down1, lp_run(&v.down[0], a->down0, v.comp * v.y[3]));
    }
    float ap = -0.9980063f * (sample - v.ap_y) + v.ap_x; v.ap_x = sample; v.ap_y = ap;
    sample = svf_run(&v.notch, &a->notch, hp_run(&v.post, a->post_a, ap), 3) * amp;
    if (a->drive_gain > 0.0f) {   /* RAT: the high end pushed harder into the clip, then its tone filter */
        float high = svf_run(&v.drive_pre, &a->drive_pre, sample, 2);
        float x = ftanh((sample + a->drive_high * high) * a->drive_gain) * 0.254f;
        sample = hp_run(&v.drive_dc, a->drive_dc_a, svf_run(&v.drive_post, &a->drive_post, x, 0));
    } else sample *= 0.254f;
    sample = fclamp(sample * a->level, -1.5f, 1.5f);
    if (!v.gate && v.amp < 0.000001f) {   /* quiet: stop until the next note */
        v.idle = 1;
        v.click.low = v.click.band = 0.0f;
    }
    return (int32_t)(sample * 32768.0f);
}
