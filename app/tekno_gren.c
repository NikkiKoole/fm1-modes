/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO's GRENADIER: after dreamengine's grenadier cart (an homage to the Grendel RA-99 Grenadier,
 * Rare Waves 2020): one buzzy trapezoid oscillator (MORPH: triangle .. square) through THREE
 * resonant filters in parallel, a low pass and two band passes (the RA-99 layout), their
 * frequencies stacked BASE, BASE x SPACE, BASE x SPACE^2 and swept together (the pad's ALPHA, here
 * ridden on the bars), summed into a little CMOS grit. A gate chugs it on the sixteenths (or the
 * eighths, or the gallop), each pulse decaying, or it drones.
 * Float, like the acid; every helper inlined; called a sample at a time from tekno_render. */
#include "tekno.h"

#define INLINE static inline __attribute__((always_inline))
INLINE float fclamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
INLINE float fexp2(float x)
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
INLINE float ftan(float x)   /* 0 .. ~1.2 */
{
    float q = x * x;
    float sn = x * (1.0f - q / 6.0f * (1.0f - q / 20.0f * (1.0f - q / 42.0f * (1.0f - q / 72.0f))));
    float cs = 1.0f - q / 2.0f * (1.0f - q / 12.0f * (1.0f - q / 30.0f * (1.0f - q / 56.0f)));
    return sn / cs;
}

static struct {
    float phase, inc, env, decay, amp, move;
    float low[3], band[3], g[3], h[3];
    unsigned tick, idle, on;
} g = {.idle = 1};

FM1_AUDIO_CODE void tekno_gren_note(int note)
{
    g.inc = 440.0f * fexp2((float)(note - 69) * (1.0f / 12.0f)) * (1.0f / 44100.0f);
    if (g.idle) {
        for (unsigned i = 0; i < 3; ++i) g.low[i] = g.band[i] = 0.0f;
        g.tick = 0; g.amp = 0.0f;
    }
    g.idle = 0; g.on = 1;
}
/* a pulse of the gate: back up to full, then down by `decay` a sample (1: a drone) */
FM1_AUDIO_CODE void tekno_gren_pulse(float decay) { g.env = decay >= 1.0f ? 0.5f : 1.0f; g.decay = decay; }   /* (a drone held at half: as loud as the chug) */
FM1_AUDIO_CODE void tekno_gren_off(void) { g.on = 0; }
FM1_AUDIO_CODE void tekno_gren_move(float u) { g.move = u; }
FM1_AUDIO_CODE unsigned tekno_gren_sounding(void) { return !g.idle; }

FM1_AUDIO_CODE int32_t tekno_gren_sample(void)
{
    if (g.idle) return 0;
    const struct tekno_gren *p = &tekno.gren;
    if (!(g.tick++ & 15U)) {   /* the bank's three frequencies, now (ALPHA moved by the bars) */
        float f = p->base_hz * fexp2(p->move_oct * g.move);
        for (unsigned i = 0; i < 3; ++i) {
            float gi = ftan(3.1415927f * fclamp(f, 20.0f, 15000.0f) * (1.0f / 44100.0f));
            g.g[i] = gi;
            g.h[i] = 1.0f / (1.0f + p->r * gi + gi * gi);
            f *= p->space;
        }
    }
    float ph = g.phase;
    g.phase += g.inc; if (g.phase >= 1.0f) g.phase -= 1.0f;
    float tri = ph < 0.5f ? -1.0f + 4.0f * ph : 3.0f - 4.0f * ph;
    float x = fclamp(tri * p->morph, -1.0f, 1.0f);   /* the trapezoid: steeper sides, flatter tops */
    float sum = 0.0f;
    for (unsigned i = 0; i < 3; ++i) {
        float hp = (x - (p->r + g.g[i]) * g.band[i] - g.low[i]) * g.h[i];
        float bp = g.g[i] * hp + g.band[i]; g.band[i] = bp + g.g[i] * hp;
        float lp = g.g[i] * bp + g.low[i]; g.low[i] = lp + g.g[i] * bp;
        sum += i == 0 ? lp * 0.7f : bp * 0.5f;   /* RA-99: a low pass, then two band passes ringing at their resonance */
    }
    float y = ftanh(sum * 1.4f);                    /* the CMOS grit */
    g.env *= g.decay;
    g.amp += ((g.on ? g.env : 0.0f) - g.amp) * 0.02f;   /* (~1 ms: no clicks on the pulses) */
    if (!g.on && g.amp < 0.00001f) { g.idle = 1; return 0; }
    return (int32_t)(fclamp(y * g.amp * p->level, -1.5f, 1.5f) * 32768.0f);
}
