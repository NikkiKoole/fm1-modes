/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "drumvoice.h"

#define INLINE static inline __attribute__((always_inline))
enum { PARTS = 32, ONE30 = 1 << 30 };
enum { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

struct dv_tables dv;
_Static_assert(sizeof(struct dv_part) % 4 == 0, "dv_trigger copies partials word by word");

static struct partial {
    struct dv_part d;
    uint32_t delay, t, phase, rng, age, inc, base_inc, base_rec, lfo_acc;
    int32_t env, low, band, pitch, dc_x, dc_y, lfo_gain;
    int32_t next, e, lp, hx, hy, x1[2], x2[2], y1[2], y2[2];
    uint8_t stage, jitter, bus;
} parts[PARTS];
static uint32_t age_clock, seed = 0x2545f491u;

INLINE int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
INLINE uint32_t xorshift(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }

/* ======================= audio side ======================= */
FM1_AUDIO_CODE void dv_trigger(const struct dv_voice *voice, int boost, uint32_t delay,
                               int32_t level, uint32_t gate, int note, unsigned bus)
{
    dv_trigger_glide(voice, boost, delay, level, gate, note, bus, 0);
}
FM1_AUDIO_CODE void dv_trigger_glide(const struct dv_voice *voice, int boost, uint32_t delay,
                                     int32_t level, uint32_t gate, int note, unsigned bus, int glide)
{
    if (!dv.ready) return;
    glide = clamp(glide, -DV_PITCH_LOW, 24);
    for (unsigned p = 0; p < voice->parts && p < DV_MAX_PARTS; ++p) {
        const struct dv_part *d = &voice->part[p];
        int vol = clamp((int)d->vol + boost, 0, 7);
        if (!vol) continue;
        unsigned pick = PARTS;
        for (unsigned i = 0; i < PARTS && pick == PARTS; ++i)
            if (parts[i].stage == IDLE) pick = i;
        if (pick == PARTS) {   /* steal the oldest */
            pick = 0;
            for (unsigned i = 1; i < PARTS; ++i)
                if (age_clock - parts[i].age > age_clock - parts[pick].age) pick = i;
        }
        struct partial *s = &parts[pick];
        {   /* word copy: a struct assignment may become a memcpy call, which lives in Flash */
            const uint32_t *src = (const uint32_t *)d;
            volatile uint32_t *dst = (volatile uint32_t *)&s->d;
            for (unsigned w = 0; w < sizeof *d / 4; ++w) dst[w] = src[w];
        }
        int32_t g = (int32_t)(((int64_t)d->gain * vol * 4681) >> 15);   /* vol/7 */
        s->d.gain = (int32_t)(((int64_t)g * level) >> 15);
        if (gate) s->d.gate = gate;
        s->base_inc = d->inc; s->base_rec = d->rec;
        if (note >= 0) {
            int n = clamp(note + d->transpose, 0, 127);
            s->base_inc = dv.note_inc[n]; s->base_rec = dv.note_rec[n];
        }
        s->inc = s->base_inc;
        s->delay = delay + d->offset; s->t = 0; s->phase = 0; s->env = 0;
        s->low = s->band = 0; s->dc_x = s->dc_y = 0;
        s->pitch = d->pitch; s->rng = xorshift(&seed) ^ (pick * 2654435761u);
        if (glide) {   /* start `glide` semitones off and slide onto the note in ~60 ms */
            s->pitch = glide * 65536; s->d.pitch_step = glide * 25;
        }
        s->lfo_acc = 0; s->lfo_gain = 32768; s->bus = (uint8_t)bus;
        s->next = 0; s->e = 0; s->lp = s->hx = s->hy = 0;
        s->x1[0] = s->x1[1] = s->x2[0] = s->x2[1] = s->y1[0] = s->y1[1] = s->y2[0] = s->y2[1] = 0;
        s->stage = d->attack ? ATTACK : DECAY;
        if (!d->attack) s->env = ONE30;
        s->age = ++age_clock;
    }
}

INLINE int32_t sine(uint32_t ph)
{
    unsigned i = ph >> (32 - DV_SINE_BITS);
    int32_t a = dv.sine[i], b = dv.sine[i + 1];
    int32_t frac = (int32_t)((ph >> (16 - DV_SINE_BITS)) & 0xffffU);
    return a + (int32_t)(((int64_t)(b - a) * frac) >> 16);
}
INLINE int32_t blep(uint32_t ph, uint32_t inc, uint32_t rec)
{
    if (ph < inc) { int32_t x = (int32_t)(((uint64_t)ph * rec) >> 32); return x + x - ((x * x) >> 15) - 32768; }
    uint32_t w = 0U - ph;
    if (w <= inc) { int32_t x = -(int32_t)(((uint64_t)w * rec) >> 32); return ((x * x) >> 15) + x + x + 32768; }
    return 0;
}
INLINE int32_t saw(uint32_t ph, uint32_t inc, uint32_t rec)
{ return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc, rec); }
/* tanh of a Q15 value, from the table over [0, 8) */
INLINE int32_t tanh_q15(int32_t u)
{
    int32_t a = u < 0 ? -u : u, r;
    uint32_t i = (uint32_t)a >> 8;
    if (i >= DV_TANH_SIZE) r = dv.tanh[DV_TANH_SIZE];
    else r = dv.tanh[i] + (((dv.tanh[i + 1] - dv.tanh[i]) * (a & 255)) >> 8);
    return u < 0 ? -r : r;
}

/* the guiro: teeth through a two-mode gourd (dreamengine's INSTR_SCRAPE, steady gesture) */
static FM1_AUDIO_CODE int32_t scrape(struct partial *s)
{
    const struct dv_body *b = &dv.body[s->d.body][s->jitter];
    int32_t impulse = 0;
    if (s->next <= 0) {
        s->jitter = (uint8_t)(xorshift(&s->rng) % DV_JITTERS);
        b = &dv.body[s->d.body][s->jitter];
        int32_t level = 32768 - (int32_t)(((xorshift(&s->rng) >> 17) * 19661U) >> 15);  /* 0.4..1 */
        impulse = level;
        s->e = (int32_t)(((int64_t)level * b->noise) >> 15);
        int32_t j = (int32_t)(xorshift(&s->rng) >> 17) - 16384;
        s->next += (int32_t)s->d.tooth_gap + (int32_t)(((int64_t)s->d.tooth_gap * j * 1966) >> 29);  /* ±6% */
    }
    s->next -= 256;
    int32_t white = (int32_t)(xorshift(&s->rng) >> 16) - 32768;
    int32_t nz = (int32_t)(((int64_t)(s->e + b->friction) * white) >> 15);
    s->e = (int32_t)(((int64_t)s->e * b->ring) >> 15);
    s->lp += impulse + (int32_t)(((int64_t)b->lowpass * (nz - s->lp)) >> 15);
    s->hy = (int32_t)(((int64_t)b->highpass * (s->hy + s->lp - s->hx)) >> 15);
    s->hx = s->lp;
    int32_t ex = s->hy, out = 0;
    for (unsigned k = 0; k < 2; ++k) {
        int64_t acc = (int64_t)b->b0[k] * (ex - s->x2[k]) - (int64_t)b->a1[k] * s->y1[k] - (int64_t)b->a2[k] * s->y2[k];
        int32_t y = clamp((int32_t)(acc >> 28), -(1 << 22), 1 << 22);
        s->x2[k] = s->x1[k]; s->x1[k] = ex; s->y2[k] = s->y1[k]; s->y1[k] = y;
        out += k ? (int32_t)(((int64_t)y * b->g2) >> 15) : y;
    }
    return clamp((int32_t)(((int64_t)out * b->gain) >> 8), -98304, 98304);
}

static FM1_AUDIO_CODE void render_partial(struct partial *s, int32_t *mix)
{
    const struct dv_part *d = &s->d;
    uint32_t inc = s->inc;
    for (unsigned i = 0; i < FM1_BLOCK_FRAMES; ++i) {
        if (s->delay) { --s->delay; continue; }
        if (!(s->t & 15U)) {   /* pitch envelope: linear in semitones toward the note */
            if (s->pitch) {
                int32_t p = s->pitch - d->pitch_step * 16;
                if ((p ^ s->pitch) < 0) p = 0;
                s->pitch = p;
                int32_t index = clamp((p + (DV_PITCH_LOW << 16)) >> 13, 0,
                                      (int32_t)(sizeof dv.pitch_ratio / sizeof dv.pitch_ratio[0]) - 1);
                inc = (uint32_t)(((uint64_t)s->base_inc * dv.pitch_ratio[index]) >> 16);
            } else inc = s->base_inc;
            s->inc = inc;
        }
        int32_t x;
        switch (d->wave) {
        case DV_SINE: x = sine(s->phase); break;
        case DV_TRI: { int32_t p = (int32_t)(s->phase >> 16); x = p < 32768 ? p * 2 - 32768 : (65535 - p) * 2 - 32768; break; }
        case DV_SQUARE: x = (saw(s->phase, inc, s->base_rec) - saw(s->phase + 0x80000000U, inc, s->base_rec)) >> 1; break;
        case DV_NOISE: x = (int32_t)(xorshift(&s->rng) >> 16) - 32768; break;
        default: x = scrape(s); break;
        }
        s->phase += inc;
        if (d->filter != DV_FILTER_NONE) {
            int32_t low = s->low, band = s->band, high_sum = 0;
            for (unsigned k = 0; k < d->substeps; ++k) {
                low += (int32_t)(((int64_t)d->f * band) >> 15);
                int32_t high = x - low - (int32_t)(((int64_t)d->damping * band) >> 15);
                band += (int32_t)(((int64_t)d->f * high) >> 15);
                low = clamp(low, -131072, 131072); band = clamp(band, -131072, 131072);
                high_sum += high;
            }
            s->low = low; s->band = band;
            /* the highpass tap is the MEAN of the substeps (dreamengine's svf-hpavg) */
            x = d->filter == DV_FILTER_LOW ? low : d->filter == DV_FILTER_BAND ? band
              : d->substeps == 4 ? high_sum >> 2 : d->substeps == 2 ? high_sum >> 1 : high_sum;
        }
        if (d->drive) {   /* asymmetric tube, normalized so full scale stays full scale */
            int32_t u = clamp((int32_t)(((int64_t)x * (x >= 0 ? d->drive_pos : d->drive_neg)) >> 12), -262143, 262143);
            int32_t y = (int32_t)(((int64_t)tanh_q15(u) * d->drive_norm) >> 15);
            int32_t out = y - s->dc_x + (int32_t)(((int64_t)s->dc_y * 32735) >> 15);   /* ~7 Hz DC block */
            s->dc_x = y; s->dc_y = out; x = out;
        }
        if (d->lfo_period) {   /* sample & hold tremolo */
            s->lfo_acc += 256;
            if (s->lfo_acc >= d->lfo_period) {
                s->lfo_acc -= d->lfo_period;
                s->lfo_gain = 32768 - (int32_t)(((xorshift(&s->rng) >> 17) * (uint32_t)d->lfo_depth) >> 15);
            }
            x = (int32_t)(((int64_t)x * s->lfo_gain) >> 15);
        }
        uint32_t t = s->t++;
        if (t == d->gate && s->stage != RELEASE) s->stage = RELEASE;
        switch (s->stage) {
        case ATTACK:
            s->env += d->attack_step;
            if (s->env >= ONE30 || t + 1 >= d->attack) { s->env = ONE30; s->stage = DECAY; }
            break;
        case DECAY:
            s->env -= d->decay_step;
            if (s->env <= d->sustain) { s->env = d->sustain; s->stage = d->sustain ? SUSTAIN : IDLE; }
            break;
        case RELEASE:
            if (d->exp_release) s->env = (int32_t)(((int64_t)s->env * d->release_k) >> 30);
            else s->env -= d->release_step;
            if (s->env <= (1 << 12) || t > d->gate + d->release) { s->env = 0; s->stage = IDLE; }
            break;
        default: break;
        }
        int32_t y = (int32_t)(((int64_t)x * (s->env >> 15)) >> 15);
        mix[i] += (int32_t)(((int64_t)y * d->gain) >> 15);
        if (s->stage == IDLE) return;
    }
}

FM1_AUDIO_CODE void dv_render(int32_t (*bus)[FM1_BLOCK_FRAMES])
{
    for (unsigned p = 0; p < PARTS; ++p)
        if (parts[p].stage != IDLE) render_partial(&parts[p], bus[parts[p].bus]);
}

/* ======================= control side ======================= */
static const float PI = 3.14159265f, FS = 44100.0f;
static float exp2_approx(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float exp_approx(float x) { return exp2_approx(x * 1.4426950f); }
static float tanh_approx(float x) { float e = exp_approx(2.0f * x); return (e - 1.0f) / (e + 1.0f); }
static float sin_small(float x)
{
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f))));
}
static float cos_small(float x)
{
    float x2 = x * x;
    return 1.0f - x2 / 2.0f * (1.0f - x2 / 12.0f * (1.0f - x2 / 30.0f * (1.0f - x2 / 56.0f)));
}
static float sqrt_approx(float x)
{
    if (x <= 0) return 0;
    float r = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 12; ++i) r = 0.5f * (r + x / r);
    return r;
}
static uint32_t ms(float milliseconds) { return (uint32_t)(milliseconds * 44.1f + 0.5f); }

void dv_build_tables(void)
{
    if (dv.ready) return;
    for (unsigned i = 0; i <= (1U << DV_SINE_BITS); ++i) {
        float x = 2.0f * PI * (float)i / (float)(1U << DV_SINE_BITS);
        float y = x <= PI / 2 ? sin_small(x) : x <= 3 * PI / 2 ? sin_small(PI - x) : -sin_small(2 * PI - x);
        dv.sine[i] = (int16_t)(y * 32767.0f);
    }
    for (unsigned i = 0; i <= DV_TANH_SIZE; ++i)
        dv.tanh[i] = (int16_t)(tanh_approx(8.0f * (float)i / DV_TANH_SIZE) * 32767.0f);
    for (unsigned i = 0; i < sizeof dv.pitch_ratio / sizeof dv.pitch_ratio[0]; ++i)
        dv.pitch_ratio[i] = (uint32_t)(exp2_approx(((float)i / 8.0f - DV_PITCH_LOW) / 12.0f) * 65536.0f);
    for (unsigned n = 0; n < 128; ++n) {
        float inc = 440.0f * exp2_approx(((float)n - 69.0f) / 12.0f) * 4294967296.0f / FS;
        dv.note_inc[n] = (uint32_t)inc;
        dv.note_rec[n] = (uint32_t)(140737488355328.0f / inc);
    }
    /* the guiro's gourd: 6.2 kHz Q17 + 4.7 kHz Q8; body 1 is half a semitone darker */
    for (unsigned b = 0; b < 2; ++b)
        for (unsigned j = 0; j < DV_JITTERS; ++j) {
            struct dv_body *body = &dv.body[b][j];
            float fj = 1.0f + 0.11f * (2.0f * ((float)j + 0.5f) / DV_JITTERS - 1.0f);
            float transpose = b ? exp2_approx(-0.5f / 12.0f) : 1.0f;
            const float fc[2] = {6200.0f * transpose * fj, 4700.0f * transpose * fj}, q[2] = {17.0f, 8.0f};
            float b0 = 0;
            for (unsigned k = 0; k < 2; ++k) {
                float w0 = 2.0f * PI * fc[k] / FS, alpha = sin_small(w0) / (2.0f * q[k]), a0 = 1.0f + alpha;
                body->b0[k] = (int32_t)(alpha / a0 * 268435456.0f);
                body->a1[k] = (int32_t)(-2.0f * cos_small(w0) / a0 * 268435456.0f);
                body->a2[k] = (int32_t)((1.0f - alpha) / a0 * 268435456.0f);
                if (!k) b0 = alpha / a0;
            }
            body->g2 = (int32_t)(0.12f * 32768.0f);
            float gain = 0.5f / (b0 > 1e-4f ? b0 : 1e-4f);
            body->gain = (int32_t)((gain > 200.0f ? 200.0f : gain) * 256.0f);
            body->lowpass = (int32_t)((1.0f - exp_approx(-2.0f * PI * fc[0] * 1.15f / FS)) * 32768.0f);
            body->highpass = (int32_t)(exp_approx(-2.0f * PI * fc[1] * 0.75f / FS) * 32768.0f);
            body->ring = (int32_t)(exp_approx(-1.0f / (1.4e-3f * FS)) * 32768.0f);
            body->noise = (int32_t)(0.20f * 32768.0f);
            body->friction = (int32_t)(0.0013f * 32768.0f);
        }
    for (unsigned bpm = 0; bpm <= 300; ++bpm)
        dv.step_samples[bpm] = bpm < 30 ? 0 : (uint32_t)(FS * 60.0f / (4.0f * (float)bpm) * 256.0f);
    for (unsigned n = 1; n < sizeof dv.reciprocal / sizeof dv.reciprocal[0]; ++n) dv.reciprocal[n] = 65536U / n;
    __asm__ volatile("" ::: "memory");
    dv.ready = 1;
}

void dv_build_part(struct dv_part *d, const struct dv_recipe *r, float tune, float stretch)
{
    struct dv_part p = {0};
    p.wave = r->wave; p.filter = r->filter; p.vol = r->vol; p.exp_release = r->exp_release; p.body = r->body;
    p.transpose = r->transpose;
    float note = (float)r->note + r->tune + tune;
    float hz = 440.0f * exp2_approx((note - 69.0f) / 12.0f);
    float inc = hz * 4294967296.0f / FS;
    p.inc = (uint32_t)inc;
    p.rec = (uint32_t)(140737488355328.0f / (inc > 1.0f ? inc : 1.0f));
    if (r->filter != DV_FILTER_NONE) {
        /* dreamengine's SVF: damping 2 - 0.13*res, 1/2/4 substeps chosen to stay stable */
        float q = 2.0f - (float)r->reso * 0.13f, x = PI * (float)r->cutoff / FS;
        unsigned n = 1;
        float f = 2.0f * sin_small(x);
        if (f < 0.0005f) f = 0.0005f;
        if (!(f <= 0.99f && f * (f + 2.0f * q) <= 2.5f)) {
            n = 2; f = 2.0f * sin_small(x * 0.5f);
            if (f > 0.99f || f * (f + 2.0f * q) > 2.5f) {
                n = 4; f = 2.0f * sin_small(x * 0.25f);
                float fmax = sqrt_approx(q * q + 2.5f) - q;
                if (f > fmax) f = fmax;
                if (f > 0.99f) f = 0.99f;
            }
        }
        p.substeps = (uint8_t)n;
        p.f = (int32_t)(f * 32768.0f);
        p.damping = (int32_t)(q * 32768.0f);
    }
    float sus = (float)r->sustain / 7.0f;
    p.attack = ms(r->attack);
    p.decay = ms((float)r->decay * stretch);
    p.gate = ms((float)r->gate * (r->sustain && r->wave != DV_SCRAPE ? 1.0f : stretch));
    p.release = ms((float)r->release * stretch);
    if (!p.release) p.release = 1;
    p.offset = ms(r->offset);
    p.sustain = (int32_t)(sus * (float)ONE30);
    p.attack_step = p.attack ? (int32_t)((float)ONE30 / (float)p.attack) : ONE30;
    p.decay_step = p.decay ? (int32_t)((1.0f - sus) * (float)ONE30 / (float)p.decay) : ONE30;
    float at_gate = p.gate < p.attack ? (float)p.gate / (float)p.attack
                  : p.gate < p.attack + p.decay ? 1.0f - (1.0f - sus) * (float)(p.gate - p.attack) / (float)(p.decay ? p.decay : 1)
                  : sus;
    p.release_step = (int32_t)(at_gate * (float)ONE30 / (float)p.release) + 1;
    p.release_k = (int32_t)(exp_approx(-6.9f / (float)p.release) * (float)ONE30);
    p.gain = (int32_t)((r->level > 1.0f ? 1.0f : r->level) * 32767.0f);
    p.pitch = (int32_t)(r->pitch * 65536.0f);
    if (r->pitch_ms > 0) {
        int32_t step = (int32_t)(r->pitch * 65536.0f / (float)ms(r->pitch_ms));
        p.pitch_step = step ? step : (r->pitch > 0 ? 1 : -1);
    }
    if (r->wave == DV_SCRAPE) p.tooth_gap = (uint32_t)(FS / r->teeth * 256.0f);
    if (r->drive > 0.001f) {
        float g = r->drive * r->drive * 24.0f;
        p.drive = 1;
        p.drive_pos = (int32_t)(g * 4096.0f);
        p.drive_neg = (int32_t)(g * (1.0f - 0.4f * r->drive) * 4096.0f);
        p.drive_norm = (int32_t)(32768.0f / tanh_approx(g));
    }
    if (r->lfo_hz > 0) {
        p.lfo_period = (uint32_t)(FS / r->lfo_hz * 256.0f);
        p.lfo_depth = (int32_t)(r->lfo_depth * 32768.0f);
    }
    *d = p;   /* control side: a plain copy is fine */
}
