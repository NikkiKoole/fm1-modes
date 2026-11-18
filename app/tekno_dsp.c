/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO audio: the kicks, the toms, hats and clap, the bar-locked phaser. Integer only, no division in the
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
volatile uint16_t tekno_lfo, tekno_vlfo, tekno_alfo, tekno_flfo, tekno_glfo;
volatile int32_t tekno_vowel_at;
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
static uint8_t lanes[TEKNO_LANES][TEKNO_STEPS];
static uint16_t acid_steps[TEKNO_STEPS];
static unsigned gren_playing, gren_droning;
static FM1_AUDIO_CODE float gren_decay(float samples)   /* e^(-3.2 / samples), from 2^x (as the voices do) */
{
    float x = -3.2f * 1.4426950f / samples;   /* small: the series is plenty */
    float a = x * 0.6931471805599453f;
    return 1.0f + a * (1.0f + a * (0.5f + a * (1.0f / 6.0f)));
}
static uint32_t acid_gate;           /* samples until the acid's gate closes (0: none pending) */
static unsigned acid_slide_in;       /* the step just played slides into the next */

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
/* VOWEL: four state-variable band passes, the kick's sub kept aside */
static int32_t vf_low[4], vf_band[4], vf_f[4], vf_damp[4], vf_amp[4], sub;
enum { SUB_K = 692 /* a ~150 Hz one-pole, Q15 */, VOWEL_MAKEUP = 10 };
/* FLANGE: the hats into a short delay line (int16, ~5.8 ms), read back between two samples */
enum { FL_LEN = 256, FL_MIN = 9 << 8 /* 0.2 ms, Q8 samples */, FL_RANGE = 170 << 8 /* up to ~4 ms more */ };
static int16_t fl_buf[FL_LEN];
static unsigned fl_w;
static int32_t fl_d = FL_MIN, fl_target = FL_MIN;
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
/* HATS and CLAP: noise through a state-variable filter */
struct noise_voice { int32_t env, peak, k, low, band; uint32_t t; uint8_t on; };
static struct noise_voice hat;
INLINE uint32_t noise(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
/* the CLAP: four bursts at `at` (their gaps jittered by VARY), the room tail from the hit */
static struct {
    uint32_t t, at[4];
    unsigned n;                      /* bursts fired */
    int32_t peak, benv, renv, f;     /* Q30; Q30; Q30; this hit's band (VARY) */
    int32_t low, band, blow, bband, rlow, rband;   /* the main, body and room bands */
    uint8_t on;
} clap;
enum { BURST_K = 1060300064 /* ~1.8 ms */ };
static FM1_AUDIO_CODE void hat_hit(unsigned code)
{
    hat.k = (code & 3U) == 2U ? tekno.hat_open_k : tekno.hat_closed_k;   /* a closed hat chokes an open one */
    hat.env = (code & 4U) ? ONE30 : ONE30 / 10 * 6; hat.on = 1;
}
INLINE int32_t jitter(void) { return (int32_t)(noise() >> 16) - 32768; }   /* Q15, -1..1 */
static FM1_AUDIO_CODE void clap_hit(unsigned code)
{
    int32_t vary = tekno.clap_vary, gap = (int32_t)tekno.clap_gap;
    clap.at[0] = 0;
    for (unsigned i = 1; i < 4; ++i) {   /* the gaps, each up to +-40 % off with VARY full */
        int32_t g = gap + mul15(mul15(gap, jitter()), mul15(vary, 13107));
        clap.at[i] = clap.at[i - 1] + (uint32_t)(g < 8 ? 8 : g);
    }
    int32_t peak = (code & 4U) ? ONE30 : ONE30 / 10 * 7;
    clap.peak = peak + (int32_t)(((int64_t)peak * mul15(jitter(), mul15(vary, 4915))) >> 15);   /* +-15 % */
    clap.f = tekno.clap_f + mul15(tekno.clap_f, mul15(jitter(), mul15(vary, 3277)));          /* +-10 % */
    clap.renv = mul15(clap.peak >> 15, tekno.clap_room) << 15;
    clap.t = 0; clap.n = 0; clap.benv = 0; clap.on = 1;
}
INLINE int32_t svf(struct noise_voice *v, int32_t x, int32_t f, int32_t damp, unsigned band)
{
    v->low = clamp(v->low + mul15(f, v->band), -131072, 131072);
    int32_t high = x - v->low - mul15(damp, v->band);
    v->band = clamp(v->band + mul15(f, high), -131072, 131072);
    return band ? v->band : high;
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
        unsigned dirty = tekno.lane_dirty;
        if (dirty) {
            for (unsigned l = 0; l < TEKNO_LANES; ++l)
                if ((dirty >> l) & 1U) for (unsigned i = 0; i < TEKNO_STEPS; ++i) lanes[l][i] = tekno.lane_want[l][i];
            tekno.lane_dirty = 0;
        }
        if (tekno.acid_dirty) { for (unsigned i = 0; i < TEKNO_STEPS; ++i) acid_steps[i] = tekno.acid_want[i]; tekno.acid_dirty = 0; }
        tekno_bar = (uint8_t)((p >> 4) & 7U);
    }
    tekno_step = (uint8_t)s;
    unsigned layers = tekno_layers;
    /* one kick a beat: RUMBLE's long tail if RUMBLE is on (with ROLL too, the bass rolls over it), else ROLL's short one */
    if ((layers & (TEKNO_KICK | TEKNO_ROLL)) && !(s & 3U)) kick_hit(delay, !(layers & TEKNO_KICK));
    if ((layers & TEKNO_ROLL) && (s & 3U) && ((tekno.roll_feel >> s) & 1U))   /* (the kick has the beats) */
        bass_hit((s & 3U) == 2U ? ONE30 : (s & 3U) == 3U ? ONE30 / 10 * 8 : ONE30 / 10 * 7);   /* the & loudest */
    const uint8_t *toms = lanes[TEKNO_LANE_TOMS];
    if ((layers & TEKNO_TOMS) && (toms[s] & 3U)) tom_hit((toms[s] & 3U) == 2U, (toms[s] & 4U) != 0);
    if ((layers & TEKNO_HATS) && (lanes[TEKNO_LANE_HATS][s] & 3U)) hat_hit(lanes[TEKNO_LANE_HATS][s]);
    if ((layers & TEKNO_CLAP) && (lanes[TEKNO_LANE_CLAP][s] & 3U)) clap_hit(lanes[TEKNO_LANE_CLAP][s]);
    /* ACID: a note, a tie (the last note held on), or a rest. The gate closes a little past half the
     * step, unless the note slides into the next or the next is tied to it (a division, once a step) */
    if (layers & TEKNO_ACID) {
        unsigned c = acid_steps[s], next = acid_steps[(s + 1U) & 15U];
        if (c & TEKNO_ACID_NOTE) tekno_acid_note((int)tekno.acid_root + (int)(c & 63U), (c & TEKNO_ACID_ACCENT) != 0, (int)acid_slide_in);
        if (c & (TEKNO_ACID_NOTE | TEKNO_ACID_TIE)) {
            acid_slide_in = (c & TEKNO_ACID_SLIDE) != 0;
            acid_gate = acid_slide_in || (next & TEKNO_ACID_TIE) || !rate_q24 ? 0U : (uint32_t)((9ULL << 20) / rate_q24) + 1U;
        } else { tekno_acid_off(); acid_slide_in = 0; acid_gate = 0; }
    } else if (acid_slide_in || acid_gate || tekno_acid_sounding()) { tekno_acid_off(); acid_slide_in = 0; acid_gate = 0; }
    /* GRENADIER: the drone on the acid's note; the gate pulses decay over the pulse (as the cart's
     * exp(-3.2) a pulse; a division and an exp, once a step) */
    if (layers & TEKNO_GREN) {
        unsigned gate = tekno.gren_gate, q = s & 3U;
        if (!gren_playing) { tekno_gren_note((int)tekno.acid_root); gren_playing = 1; if (gate == TEKNO_GATE_DRONE) tekno_gren_pulse(1.0f); }
        unsigned pulse = gate == TEKNO_GATE_16THS || (gate == TEKNO_GATE_8THS && !(s & 1U)) || (gate == TEKNO_GATE_4THS && !q) || (gate == TEKNO_GATE_GALLOP && q != 1U);
        if (gate == TEKNO_GATE_DRONE && !gren_droning) tekno_gren_pulse(1.0f);
        gren_droning = gate == TEKNO_GATE_DRONE;
        if (pulse && rate_q24) {
            float len = (float)(1U << 24) / (float)rate_q24 * (gate == TEKNO_GATE_4THS ? 4.0f : gate == TEKNO_GATE_8THS ? 2.0f : 1.0f);   /* samples in a pulse */
            tekno_gren_pulse(gren_decay(len));
        }
    } else if (gren_playing) { tekno_gren_off(); gren_playing = 0; gren_droning = 0; }
}

FM1_AUDIO_CODE void tekno_render(int32_t *bus)
{
    if (!dv.ready || !tekno.ratio[0]) return;
    unsigned any = queued || tekno_acid_sounding() || tekno_gren_sounding() || kick[0].on || kick[1].on || tom[0].on || tom[1].on || bass.on || hat.on || clap.on;
    if (any) quiet = 0; else if (quiet < 400U) ++quiet;
    if (quiet >= 400U) { stepq += rate_q24 * N; now += N; return; }   /* ~1 s silent: rest */
    const int32_t dk = tekno.kick_decay_k, pk = tekno.kick_pitch_k, tk = tekno.tom_decay_k, tpk = tekno.tom_pitch_k;
    const int32_t dpos = tekno.kick_drive_pos, dneg = tekno.kick_drive_neg, dnorm = tekno.kick_drive_norm;
    const int32_t tone = tekno.kick_tone, klevel = tekno.kick_level, tlevel = tekno.tom_level;
    const int32_t skin_level = tekno.tom_skin;
    const int32_t bk = tekno.roll_decay_k, bf = tekno.roll_f, bdamp = tekno.roll_damp;
    const int32_t bdrive = tekno.roll_drive, bnorm = tekno.roll_drive_norm, blevel = tekno.roll_level;
    const int32_t hf = tekno.hat_f, hdamp = tekno.hat_damp, hlevel = tekno.hat_level;
    const int32_t cdamp = tekno.clap_damp, ctail = tekno.clap_tail_k, clevel = tekno.clap_level, cburst = tekno.clap_burst_k;
    const int32_t cair = tekno.clap_air, cbody = tekno.clap_body, cbf = tekno.clap_body_f, cbdamp = tekno.clap_body_damp;
    const int32_t crf = tekno.clap_room_f, crdamp = tekno.clap_room_damp, cdrive = tekno.clap_drive, cdnorm = tekno.clap_drive_norm;
    const int32_t fdepth = tekno.fl_depth, ffb = tekno.fl_feedback, fmix = tekno.fl_mix;
    const unsigned fshift = 4U - (tekno.fl_shift & 3U), ashift = 4U - (tekno.asw_shift & 3U), ashape = tekno.asw_shape;
    const unsigned gshift = 7U - (tekno.gren_rate > 6U ? 6U : tekno.gren_rate);
    const unsigned sweeping = tekno.acid.sweep_oct != 0.0f;   /* (DEPTH 0: off, RES too) */
    const int32_t vcenter = tekno.vow_center, vdepth = tekno.vow_depth, vmix = tekno.vow_mix;
    const unsigned vshift = 6U - (tekno.vow_rate > 5U ? 5U : tekno.vow_rate);
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
        /* HATS, through the FLANGE: the delay swept between 0.2 ms and up to ~4 ms, on the bars */
        int32_t h = 0;
        if (hat.on) {
            int32_t x = svf(&hat, (int32_t)(noise() >> 16) - 32768, hf, hdamp, 0);
            hat.env = mul30(hat.env, hat.k);
            if (hat.env < (1 << 16)) hat.on = 0;
            h = mul15(mul15(x, hat.env >> 15), hlevel);
        }
        if (fmix) {
            if (update) {
                uint32_t turns = stepq << fshift;
                int32_t up = (32768 - dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)]) >> 1;   /* (1 - cos) / 2: short at the loop's start */
                tekno_flfo = (uint16_t)(up << 1);
                fl_target = FL_MIN + mul15(mul15(up, fdepth), FL_RANGE);
            }
            fl_d += (fl_target - fl_d) >> 6;   /* glide there, no zipper */
            unsigned back = (unsigned)fl_d >> 8, fr = (unsigned)fl_d & 255U;
            int32_t a0 = fl_buf[(fl_w - back) & (FL_LEN - 1U)], a1 = fl_buf[(fl_w - back - 1U) & (FL_LEN - 1U)];
            int32_t del = a0 + (((a1 - a0) * (int32_t)fr) >> 8);
            fl_buf[fl_w] = (int16_t)clamp(h + mul15(del, ffb), -32768, 32767);
            fl_w = (fl_w + 1U) & (FL_LEN - 1U);
            h += mul15(del, fmix);
        }
        t += h;
        /* CLAP: three bursts `gap` apart, the last one rings on */
        if (clap.on) {   /* CLAP: the bursts (band + air + body) and the room, from one noise */
            uint32_t ct = clap.t++;
            if (clap.n < 4U && ct == clap.at[clap.n]) { clap.benv = clap.peak; ++clap.n; }
            clap.benv = mul30(clap.benv, cburst);
            clap.renv = mul30(clap.renv, ctail);
            if (clap.n == 4U && clap.benv < (1 << 16) && clap.renv < (1 << 16)) clap.on = 0;
            int32_t x = (int32_t)(noise() >> 16) - 32768;
            clap.low = clamp(clap.low + mul15(clap.f, clap.band), -131072, 131072);
            int32_t high = x - clap.low - mul15(cdamp, clap.band);
            clap.band = clamp(clap.band + mul15(clap.f, high), -131072, 131072);
            clap.blow = clamp(clap.blow + mul15(cbf, clap.bband), -131072, 131072);
            int32_t bhigh = x - clap.blow - mul15(cbdamp, clap.bband);
            clap.bband = clamp(clap.bband + mul15(cbf, bhigh), -131072, 131072);
            clap.rlow = clamp(clap.rlow + mul15(crf, clap.rband), -131072, 131072);
            int32_t rhigh = x - clap.rlow - mul15(crdamp, clap.rband);
            clap.rband = clamp(clap.rband + mul15(crf, rhigh), -131072, 131072);
            int32_t burst = clap.band + mul15(high, cair) + mul15(clap.bband, cbody);
            int32_t y = mul15(burst, clap.benv >> 15) + mul15(clap.rband, clap.renv >> 15);
            if (cdrive) y = mul15(tanh_q15(clamp((int32_t)(((int64_t)y * cdrive) >> 12), -262143, 262143)), cdnorm);
            t += mul15(y, clevel);
        }
        /* PHASER over the percussion: six allpasses swept once every 16 << bars steps, locked to the bar */
        if (update) {
            uint32_t turns = stepq << shift;
            tekno_lfo = (uint16_t)(turns >> 16);
            int32_t lfo = -dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)];   /* -cos: lowest at the loop's start */
            ph_coeff = 16384 + mul15(mul15(lfo, depth), 15565);
        }
        if (update) {   /* GRENADIER: ALPHA swept on the bars (-cos: lowest at the loop's start) */
            uint32_t turns = stepq << gshift;
            int32_t c = -dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)];
            tekno_glfo = (uint16_t)((c + 32768) >> 0);
            tekno_gren_move((float)c * (1.0f / 32768.0f));
        }
        if (update) {   /* SWEEP: where the acid's ride is (0 closed .. 1 open; 0 when it is off) */
            uint32_t turns = stepq << ashift;
            int32_t u = !sweeping ? 0 : ashape == 1 ? (int32_t)(turns >> 17)                     /* rising */
                      : ashape == 2 ? 32767 - (int32_t)(turns >> 17)             /* falling */
                      : (32768 - dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)]) >> 1;   /* up and down */
            tekno_alfo = (uint16_t)(u << 1);
            tekno_acid_sweep((float)u * (1.0f / 32768.0f));
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
        /* VOWEL over everything: the four bands of the vowel, the kick's sub (which no vowel band
         * passes) put back under, mixed with the dry */
        /* ACID */
        if (acid_gate && !--acid_gate) tekno_acid_off();
        int32_t all = k + t + tekno_acid_sample() + tekno_gren_sample();
        sub += mul15(k - sub, SUB_K);
        if (update) {   /* where on the path, and the bands for it (interpolated between two places) */
            uint32_t turns = stepq << vshift;
            tekno_vlfo = (uint16_t)(turns >> 16);
            int32_t lfo = -dv.sine[(turns + 0x40000000U) >> (32 - DV_SINE_BITS)];
            int32_t at = clamp(vcenter + (int32_t)(((int64_t)lfo * vdepth) >> 15), 0, (TEKNO_VOWELS - 1) * 256 - 1);
            tekno_vowel_at = at;
            const int16_t (*a)[4] = tekno.vowel[at >> 8], (*b)[4] = tekno.vowel[(at >> 8) + 1];
            int32_t fr = at & 255;
            for (unsigned j = 0; j < 4; ++j) {
                vf_f[j] = a[0][j] + (((b[0][j] - a[0][j]) * fr) >> 8);
                vf_damp[j] = a[1][j] + (((b[1][j] - a[1][j]) * fr) >> 8);
                vf_amp[j] = a[2][j] + (((b[2][j] - a[2][j]) * fr) >> 8);
            }
        }
        if (vmix) {
            int32_t wet = 0;
            for (unsigned j = 0; j < 4; ++j) {
                vf_low[j] = clamp(vf_low[j] + mul15(vf_f[j], vf_band[j]), -(1 << 24), 1 << 24);
                int32_t high = all - vf_low[j] - mul15(vf_damp[j], vf_band[j]);
                vf_band[j] = clamp(vf_band[j] + mul15(vf_f[j], high), -(1 << 24), 1 << 24);
                wet += mul15(mul15(vf_band[j], vf_damp[j]), vf_amp[j]);   /* (band x damping: a peak of 1) */
            }
            wet = tanh_q15(wet * VOWEL_MAKEUP) + sub;
            all += mul15(wet - all, vmix);
        }
        /* the bus: a soft saturator, nearly straight at normal levels, rounding stacked hits off
         * below 0.85 of full scale (the records were driven hard; this keeps the mode from clipping) */
        int32_t out = mul15(tanh_q15(mul15(all, 38550 /* 1 / 0.85 */)), 27853 /* 0.85 */);
        int32_t a = out < 0 ? -out : out;
        if (a > peak) peak = a;
        bus[i] += out;
    }
    tekno_peak = peak;
    now += N;
}
