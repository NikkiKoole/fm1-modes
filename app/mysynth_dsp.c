/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* MY SYNTH audio: 6 voices x (2 PolyBLEP saw/square oscillators -> 2x
 * oversampled Chamberlin state-variable lowpass -> ADSR amp). Integer only. */
#include "mysynth.h"

#define INLINE static inline __attribute__((always_inline))
#define BARRIER() __asm__ volatile("" ::: "memory")
enum { OFF, ATTACK, DECAY, RELEASE };
enum { QUEUE = 32 };
static const int32_t ONE30 = 1 << 30;

struct mysynth_params mysynth_params;
int16_t mysynth_scope[MYSYNTH_SCOPE];
volatile uint32_t mysynth_scope_pos;

static struct voice {
    uint32_t ph1, ph2, inc, rec, age;
    int32_t velocity, env, amp, f, low, band;
    uint8_t stage, note;
} voices[MYSYNTH_VOICES];
static struct { uint8_t type, note, velocity; uint32_t inc, rec; } queue[QUEUE];
static volatile uint32_t queue_head, queue_tail; /* head: control, tail: audio */
static uint32_t clock_age;

int mysynth_event(unsigned type, unsigned note, unsigned velocity, uint32_t inc, uint32_t rec)
{
    uint32_t head = queue_head, next = (head + 1U) % QUEUE;
    if (next == queue_tail) return -1;
    queue[head].type = (uint8_t)type; queue[head].note = (uint8_t)note;
    queue[head].velocity = (uint8_t)velocity; queue[head].inc = inc; queue[head].rec = rec;
    BARRIER();
    queue_head = next;
    return 0;
}
unsigned mysynth_voices_active(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < MYSYNTH_VOICES; ++i) n += voices[i].stage != OFF;
    return n;
}

/* PolyBLEP residual in Q15 for a saw that jumps by -2 at the phase wrap.
 * rec = 2^47 / inc, so (phase * rec) >> 32 is phase/inc in Q15. */
INLINE int32_t blep(uint32_t ph, uint32_t inc, uint32_t rec)
{
    if (ph < inc) {
        int32_t x = (int32_t)(((uint64_t)ph * rec) >> 32);
        return x + x - ((x * x) >> 15) - 32768;
    }
    uint32_t to_wrap = 0U - ph;
    if (to_wrap <= inc) {
        int32_t x = -(int32_t)(((uint64_t)to_wrap * rec) >> 32);
        return ((x * x) >> 15) + x + x + 32768;
    }
    return 0;
}
INLINE int32_t saw(uint32_t ph, uint32_t inc, uint32_t rec)
{
    return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc, rec);
}
INLINE int32_t osc(uint32_t ph, uint32_t inc, uint32_t rec, int32_t wave)
{
    int32_t a = saw(ph, inc, rec);
    int32_t square = (a - saw(ph + 0x80000000U, inc, rec)) >> 1;
    return a + (int32_t)(((int64_t)(square - a) * wave) >> 15);
}
INLINE int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

static FM1_AUDIO_CODE void handle_events(void)
{
    while (queue_tail != queue_head) {
        uint32_t tail = queue_tail;
        BARRIER();
        unsigned type = queue[tail].type, note = queue[tail].note;
        if (type == MYSYNTH_ALL_OFF) {
            for (unsigned i = 0; i < MYSYNTH_VOICES; ++i)
                if (voices[i].stage != OFF) voices[i].stage = RELEASE;
        } else if (type == MYSYNTH_NOTE_OFF) {
            for (unsigned i = 0; i < MYSYNTH_VOICES; ++i)
                if (voices[i].note == note && (voices[i].stage == ATTACK || voices[i].stage == DECAY))
                    voices[i].stage = RELEASE;
        } else if (type == MYSYNTH_NOTE_ON) {
            /* same note retriggers; else a free voice; else steal the oldest */
            unsigned pick = MYSYNTH_VOICES;
            for (unsigned i = 0; i < MYSYNTH_VOICES && pick == MYSYNTH_VOICES; ++i)
                if (voices[i].stage != OFF && voices[i].note == note) pick = i;
            for (unsigned i = 0; i < MYSYNTH_VOICES && pick == MYSYNTH_VOICES; ++i)
                if (voices[i].stage == OFF) pick = i;
            if (pick == MYSYNTH_VOICES) {
                pick = 0;
                for (unsigned i = 1; i < MYSYNTH_VOICES; ++i)
                    if (clock_age - voices[i].age > clock_age - voices[pick].age) pick = i;
            }
            struct voice *v = &voices[pick];
            if (v->stage == OFF) { v->env = v->amp = v->low = v->band = 0; v->ph1 = 0; v->ph2 = 0x40000000U; v->f = -1; }
            v->note = (uint8_t)note; v->inc = queue[tail].inc; v->rec = queue[tail].rec;
            v->velocity = (int32_t)queue[tail].velocity * 258; /* 127 -> ~Q15 1.0 */
            v->stage = ATTACK; v->age = ++clock_age;
        }
        BARRIER();
        queue_tail = (tail + 1U) % QUEUE;
    }
}

FM1_AUDIO_CODE void mysynth_render(int32_t *output)
{
    static int32_t mix[FM1_BLOCK_FRAMES];
    struct mysynth_params *p = &mysynth_params;
    handle_events();
    {   /* volatile keeps this a loop, not a memset call into Flash */
        volatile int32_t *m = mix;
        for (unsigned i = 0; i < FM1_BLOCK_FRAMES; ++i) m[i] = 0;
    }
    const int32_t wave = p->wave, damping = p->damping, detune = p->detune;
    unsigned any = 0;

    for (unsigned n = 0; n < MYSYNTH_VOICES; ++n) {
        struct voice *v = &voices[n];
        if (v->stage == OFF) continue;
        any = 1;
        /* envelope, once per block; amp and cutoff ramp across the block */
        if (v->stage == ATTACK) {
            v->env += p->attack;
            if (v->env >= ONE30) { v->env = ONE30; v->stage = DECAY; }
        } else if (v->stage == DECAY) {
            v->env = p->sustain + (int32_t)(((int64_t)(v->env - p->sustain) * p->decay) >> 15);
        } else {
            v->env = (int32_t)(((int64_t)v->env * p->release) >> 15);
            if (v->env < (1 << 14)) { v->stage = OFF; v->env = 0; }
        }
        int32_t env15 = v->env >> 15;
        int32_t amp_to = (int32_t)(((int64_t)env15 * v->velocity) >> 15);
        int32_t index = p->cutoff + (int32_t)(((int64_t)p->envmod * env15) >> 15) + ((int32_t)v->note - 60);
        int32_t f_to = p->ftab[clamp(index, 0, MYSYNTH_FTAB - 1)];
        if (v->f < 0) v->f = f_to;
        int32_t amp = v->amp, amp_step = (amp_to - amp) / FM1_BLOCK_FRAMES;
        int32_t f = v->f, f_step = (f_to - f) / FM1_BLOCK_FRAMES;

        uint32_t inc1 = v->inc, rec1 = v->rec;
        uint32_t inc2 = inc1 + (uint32_t)(((uint64_t)inc1 * (uint32_t)detune) >> 16);
        uint32_t rec2 = rec1 - (uint32_t)(((uint64_t)rec1 * (uint32_t)detune) >> 16);
        uint32_t ph1 = v->ph1, ph2 = v->ph2;
        int32_t low = v->low, band = v->band;
        for (unsigned i = 0; i < FM1_BLOCK_FRAMES; ++i) {
            int32_t in = (osc(ph1, inc1, rec1, wave) + osc(ph2, inc2, rec2, wave)) >> 2;
            ph1 += inc1; ph2 += inc2;
            for (unsigned k = 0; k < 2; ++k) {   /* 2x oversampled SVF */
                low += (int32_t)(((int64_t)f * band) >> 15);
                int32_t high = in - low - (int32_t)(((int64_t)damping * band) >> 15);
                band += (int32_t)(((int64_t)f * high) >> 15);
                band = clamp(band, -(1 << 20), 1 << 20);
                low = clamp(low, -(1 << 20), 1 << 20);
            }
            mix[i] += (int32_t)(((int64_t)low * amp) >> 15);
            amp += amp_step; f += f_step;
        }
        v->ph1 = ph1; v->ph2 = ph2; v->low = low; v->band = band;
        v->amp = amp_to; v->f = f_to;
    }
    if (!any) return;
    const int32_t level = p->level;
    uint32_t pos = mysynth_scope_pos;
    for (unsigned i = 0; i < FM1_BLOCK_FRAMES; ++i) {
        /* gain, then a cubic soft clip: y = x - 4/27 x^3, flat beyond 1.5 */
        int32_t x = clamp((int32_t)(((int64_t)mix[i] * level) >> 15), -49152, 49152);
        int32_t y = x - (int32_t)(((((int64_t)x * x) >> 15) * x * 4855) >> 30);
        mysynth_scope[pos++ % MYSYNTH_SCOPE] = (int16_t)clamp(y, -32768, 32767);
        output[i] += y;
    }
    mysynth_scope_pos = pos % MYSYNTH_SCOPE;
}
