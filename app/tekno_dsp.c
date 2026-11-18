/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO audio: the rumble kick, the toms, the bar-locked phaser. Integer only, no division in the
 * sample loops, no calls outside the app's audio code, no struct copies or memset/memcpy.
 *
 * Timing: the FM-1's clock checks its steps once a block (128 samples), so a step is found up to a
 * block late. tekno_clock() is told how late, and the hit is placed exactly, one block on: every
 * hit comes 128 samples (2.9 ms) after its time, all the same, with no jitter.
 *
 * The RUMBLE KICK (measured on the references: the low band holds almost flat for the whole beat,
 * the tail a pure note with its 2nd and 3rd harmonics ~22 dB down): a sine that drops from high
 * above onto the note, a noise click, then one shared drive, a low pass and the level. The tail
 * holds until the next kick cuts it (a 3 ms fade), or a beat and a bit if none comes. */
#include "tekno.h"
#include "fx.h"
#include "drumvoice.h"

#define INLINE static inline __attribute__((always_inline))
enum { N = FM1_BLOCK_FRAMES, ONE30 = 1 << 30, POS_MASK = 127, QUEUE = 4, STAGES = 6 };

struct tekno_params tekno;
volatile uint8_t tekno_layers, tekno_step, tekno_bar;
volatile uint16_t tekno_lfo;
volatile int32_t tekno_peak;

INLINE int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
INLINE int32_t mul15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
INLINE int32_t mul30(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 30); }
INLINE int32_t sine(uint32_t ph)
{
    unsigned i = ph >> (32 - DV_SINE_BITS);
    int32_t a = dv.sine[i], b = dv.sine[i + 1];
    int32_t frac = (int32_t)((ph >> (16 - DV_SINE_BITS)) & 0xffffU);
    return a + (int32_t)(((int64_t)(b - a) * frac) >> 16);
}
INLINE int32_t tanh_q15(int32_t u)
{
    int32_t a = u < 0 ? -u : u, r;
    uint32_t i = (uint32_t)a >> 8;
    if (i >= DV_TANH_SIZE) r = dv.tanh[DV_TANH_SIZE];
    else r = dv.tanh[i] + (((dv.tanh[i + 1] - dv.tanh[i]) * (a & 255)) >> 8);
    return u < 0 ? -r : r;
}

/* ---- the clock: steps queued with their exact sample ---- */
static struct { uint32_t at; uint8_t pos; } queue[QUEUE];
static volatile unsigned queued;
static uint32_t now, rate_q24, stepq;   /* samples; Q24 steps a sample; Q24 steps into the 8-bar loop */
static unsigned pos = POS_MASK;
static uint8_t toms[TEKNO_STEPS];

FM1_AUDIO_CODE void tekno_clock(unsigned step, unsigned tempo, uint32_t late, unsigned restart)
{
    (void)step;
    rate_q24 = (tempo * 1662117U) >> 16;   /* tempo * 4 / (44100 * 60), Q24 */
    if (late > N - 1U) late = N - 1U;
    if (restart) { pos = POS_MASK; queued = 0; }
    pos = (pos + 1U) & POS_MASK;
    if (queued < QUEUE) { queue[queued].at = now + N - late; queue[queued].pos = (uint8_t)pos; ++queued; }
}

/* ---- the voices ---- */
struct drum {
    uint32_t phase, inc, base, gate;   /* gate: samples left before the release */
    int32_t pitch, env, fade;          /* Q16 semitones; Q30; Q15 (dying when > 0) */
    int32_t skin;                      /* TOMS: the head's noise, Q30 */
    uint8_t on;
};
static struct drum kick[2], tom[2];
static unsigned kick_last, quiet;
static int32_t click_env, click_prev, drive_x1, drive_y1, tone_lp;
static uint32_t rng = 0x9e3779b9u;
static int32_t ph_st[STAGES], ph_pv[STAGES], ph_coeff = 16384;
enum { RELEASE_K = 1072525294 /* ~20 ms, Q30 */, FADE_STEP = 250 /* ~3 ms */, SKIN_K = 1072768440 /* ~25 ms */ };
static int32_t skin_prev;
enum { BASS_PITCH_K = 868323340 /* x0.81 every 16 samples: ~1.7 ms */ };

INLINE uint32_t pitched(uint32_t base, int32_t pitch)
{
    int32_t i = clamp(pitch >> 13, 0, TEKNO_RATIOS - 1);
    return (uint32_t)(((uint64_t)base * tekno.ratio[i]) >> 16);
}
static FM1_AUDIO_CODE void kick_hit(uint32_t delay, unsigned roll)
{
    struct drum *old = &kick[kick_last];
    if (old->on) old->fade = 32767;                 /* the next kick cuts the tail */
    kick_last ^= 1U;
    struct drum *k = &kick[kick_last];
    k->phase = 0; k->base = tekno.kick_inc; k->pitch = tekno.kick_pitch;
    k->inc = pitched(k->base, k->pitch);
    k->env = ONE30; k->fade = 0; k->on = 1;
    /* RUMBLE: hold a beat and a bit, 4 steps + 2 ms; ROLL: 3/4 of a step, the bass comes next (a division, once a beat) */
    k->gate = !rate_q24 ? 44100U : roll ? (uint32_t)((3ULL << 22) / rate_q24) : (uint32_t)((4ULL << 24) / rate_q24) + 88U;
    click_env = tekno.kick_click << 15;
    fx_kick(FX_KEY_CR78, delay);                    /* the duck and the PMP pad hear it */
}
/* the ROLL's bass: a saw through a low pass and a drive, a quick drop onto the note, a short decay */
static struct { uint32_t phase, inc, base; int32_t pitch, env, low, band, x1, y1; uint8_t on; } bass;
static FM1_AUDIO_CODE void bass_hit(int32_t level)
{
    bass.base = tekno.kick_inc; bass.pitch = 12 * 65536;   /* an octave's drop in a few ms: a pluck */
    bass.inc = pitched(bass.base, bass.pitch);
    bass.env = level; bass.on = 1;
}
static FM1_AUDIO_CODE void tom_hit(unsigned v, unsigned accent)
{
    struct drum *t = &tom[v];
    t->base = tekno.tom_inc[v]; t->pitch = tekno.tom_pitch;
    t->inc = pitched(t->base, t->pitch);
    t->env = accent ? ONE30 : ONE30 / 20 * 11; t->on = 1;
    t->skin = t->env;   /* the stick on the head: a short noise (it gives the phaser something to sweep) */
}
static FM1_AUDIO_CODE void fire(unsigned p, uint32_t delay)
{
    unsigned s = p & 15U;
    stepq = (uint32_t)p << 24;
    if (!s) {   /* a bar: what was asked for comes in */
        tekno_layers = tekno.want;
        if (tekno.toms_dirty) {
            for (unsigned i = 0; i < TEKNO_STEPS; ++i) toms[i] = tekno.toms_want[i];
            tekno.toms_dirty = 0;
        }
        tekno_bar = (uint8_t)((p >> 4) & 7U);
    }
    tekno_step = (uint8_t)s;
    unsigned layers = tekno_layers;
    if ((layers & (TEKNO_KICK | TEKNO_ROLL)) && !(s & 3U)) kick_hit(delay, (layers & TEKNO_ROLL) != 0);
    if ((layers & TEKNO_ROLL) && ((tekno.roll_feel >> (s & 3U)) & 1U))
        bass_hit((s & 3U) == 2U ? ONE30 : (s & 3U) == 3U ? ONE30 / 10 * 8 : ONE30 / 10 * 7);   /* the & loudest */
    if ((layers & TEKNO_TOMS) && (toms[s] & 3U)) tom_hit((toms[s] & 3U) == 2U, (toms[s] & 4U) != 0);
}

FM1_AUDIO_CODE void tekno_render(int32_t *bus)
{
    if (!dv.ready || !tekno.ratio[0]) return;
    unsigned any = queued || kick[0].on || kick[1].on || tom[0].on || tom[1].on || bass.on;
    if (any) quiet = 0; else if (quiet < 400U) ++quiet;
    if (quiet >= 400U) { stepq += rate_q24 * N; now += N; return; }   /* ~1 s silent: rest */
    const int32_t dk = tekno.kick_decay_k, pk = tekno.kick_pitch_k, tk = tekno.tom_decay_k, tpk = tekno.tom_pitch_k;
    const int32_t dpos = tekno.kick_drive_pos, dneg = tekno.kick_drive_neg, dnorm = tekno.kick_drive_norm;
    const int32_t tone = tekno.kick_tone, klevel = tekno.kick_level, tlevel = tekno.tom_level;
    const int32_t skin_level = tekno.tom_skin;
    const int32_t bk = tekno.roll_decay_k, bf = tekno.roll_f, bdamp = tekno.roll_damp;
    const int32_t bdrive = tekno.roll_drive, bnorm = tekno.roll_drive_norm, blevel = tekno.roll_level;
    const int32_t depth = tekno.ph_depth, fb = tekno.ph_feedback, mix = tekno.ph_mix;
    const unsigned shift = 4U - (tekno.ph_bars_shift & 3U);
    int32_t peak = tekno_peak;
    for (unsigned i = 0; i < N; ++i) {
        while (queued && (int32_t)(now + i - queue[0].at) >= 0) {
            fire(queue[0].pos, i);
            unsigned n = queued - 1U;
            for (unsigned q = 0; q < n; ++q) { queue[q].at = queue[q + 1U].at; queue[q].pos = queue[q + 1U].pos; }
            queued = n;
        }
        unsigned update = !((now + i) & 15U);
        /* KICK */
        int32_t k = 0;
        for (unsigned v = 0; v < 2; ++v) {
            struct drum *d = &kick[v];
            if (!d->on) continue;
            if (update && d->pitch) { d->pitch = mul30(d->pitch, pk); d->inc = pitched(d->base, d->pitch); }
            int32_t x = sine(d->phase); d->phase += d->inc;
            if (d->gate) { --d->gate; d->env = mul30(d->env, dk); }
            else d->env = mul30(d->env, RELEASE_K);
            int32_t y = mul15(x, d->env >> 15);
            if (d->fade) { y = mul15(y, d->fade); d->fade -= FADE_STEP; if (d->fade <= 0) d->on = 0; }
            if (d->env < (1 << 16)) d->on = 0;
            k += y;
        }
        if (click_env > (1 << 12)) {   /* a short burst of high noise on the hit */
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            int32_t n = (int32_t)(rng >> 16) - 32768, hp = n - click_prev;
            click_prev = n;
            k += mul15(hp >> 1, click_env >> 15);
            click_env = mul30(click_env, 1030792151);   /* ~0.5 ms, Q30 0.96 */
        }
        if (dpos) {   /* asymmetric drive, normalized, then a ~7 Hz DC block */
            int32_t u = clamp((int32_t)(((int64_t)k * (k >= 0 ? dpos : dneg)) >> 12), -262143, 262143);
            int32_t y = mul15(tanh_q15(u), dnorm);
            int32_t out = y - drive_x1 + (int32_t)(((int64_t)drive_y1 * 32735) >> 15);
            drive_x1 = y; drive_y1 = out; k = out;
        }
        tone_lp += mul15(k - tone_lp, tone);
        k = mul15(tone_lp, klevel);
        /* ROLL's bass */
        if (bass.on) {
            if (update && bass.pitch) { bass.pitch = mul30(bass.pitch, BASS_PITCH_K); bass.inc = pitched(bass.base, bass.pitch); }
            int32_t x = (int32_t)(bass.phase >> 16) - 32768;
            bass.phase += bass.inc;
            bass.low += mul15(bf, bass.band);
            int32_t high = x - bass.low - mul15(bdamp, bass.band);
            bass.band = clamp(bass.band + mul15(bf, high), -131072, 131072);
            bass.low = clamp(bass.low, -131072, 131072);
            int32_t y = bass.low;
            if (bdrive) {
                int32_t u = clamp((int32_t)(((int64_t)y * bdrive) >> 12), -262143, 262143);
                y = mul15(tanh_q15(u), bnorm);
            }
            bass.env = mul30(bass.env, bk);
            if (bass.env < (1 << 16)) bass.on = 0;
            y = mul15(y, bass.env >> 15);
            int32_t out = y - bass.x1 + (int32_t)(((int64_t)bass.y1 * 32735) >> 15);   /* DC block */
            bass.x1 = y; bass.y1 = out;
            k += mul15(out, blevel);
        }
        /* TOMS */
        int32_t t = 0;
        for (unsigned v = 0; v < 2; ++v) {
            struct drum *d = &tom[v];
            if (!d->on) continue;
            if (update && d->pitch) { d->pitch = mul30(d->pitch, tpk); d->inc = pitched(d->base, d->pitch); }
            int32_t x = sine(d->phase); d->phase += d->inc;
            d->env = mul30(d->env, tk);
            if (d->env < (1 << 16)) d->on = 0;
            t += mul15(x, d->env >> 15);
            if (d->skin > (1 << 14)) {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                int32_t n = (int32_t)(rng >> 16) - 32768, hp = n - skin_prev;
                skin_prev = n;
                t += mul15(mul15(hp, skin_level), d->skin >> 15);
                d->skin = mul30(d->skin, SKIN_K);
            }
        }
        t = mul15(t, tlevel);
        /* PHASER over the toms: six allpasses swept once every 16 << bars steps, locked to the bar */
        if (update) {
            uint32_t turns = stepq << shift;
            tekno_lfo = (uint16_t)(turns >> 16);
            int32_t lfo = -dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)];   /* -cos: lowest at the loop's start */
            ph_coeff = 16384 + mul15(mul15(lfo, depth), 15565);
        }
        stepq += rate_q24;
        if (depth || mix) {
            int32_t in = clamp(t + mul15(ph_st[STAGES - 1], fb), -49152, 49152);
            for (unsigned s = 0; s < STAGES; ++s) {
                int32_t ap = mul15(ph_coeff, in - ph_st[s]) + ph_pv[s];
                ph_pv[s] = in; ph_st[s] = clamp(ap, -98304, 98304); in = ph_st[s];
            }
            t += mul15(in - t, mix);
        }
        int32_t out = k + t;
        int32_t a = out < 0 ? -out : out;
        if (a > peak) peak = a;
        bus[i] += out;
    }
    tekno_peak = peak;
    now += N;
}
