/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* PERFORM: the effects the pads and the LIVE knobs play. One chain, run in two places:
 *   on the WHOLE output (the sealed engine's tracks plus every app mode), after the FX rack;
 *   on ONE track's bus (a GROOVEBOX track's own pads), before the buses are summed.
 * After dreamengine's acidcandy master section (the DJ filter, the crush and gate lanes, PUMP,
 * SWEEP) and its drive_shape ASYM. Integer only, no division, no calls outside the app's audio
 * code. Signals are Q23 in int32. Order: drive, crush, filter, sweep, gate, pump. */
#include "fx.h"
#include "drumvoice.h"   /* dv.tanh, dv.sine */
#include "cr78.h"        /* fm1_transport_playing */

#define INLINE static inline __attribute__((always_inline))
enum { N = FM1_BLOCK_FRAMES, Q23 = 1 << 23 };

#define PERF_DEFAULTS {.crush_step = 1, .crush_rate = 1, .lpf_f = 32000, .lpf_damp = 32768, .hpf_f = 2000, \
                       .hpf_damp = 32768, .gate_len = 32768, .pump_release = 1900, .drive_mix = 32768, .drive_level = 32768}
struct fx_perf fx_perf = PERF_DEFAULTS;
struct fx_perf fx_track_perf[FX_BUSES] = {PERF_DEFAULTS, PERF_DEFAULTS, PERF_DEFAULTS, PERF_DEFAULTS};
volatile uint32_t fx_step_clock, fx_step_rate = 794;   /* 120 BPM: 8 steps a second */
volatile uint32_t fx_perf_lfo;
volatile int32_t fx_perf_gain = 32768;

INLINE int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
INLINE int32_t mul15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
INLINE int32_t tanh_q15(int32_t u)
{
    int32_t a = u < 0 ? -u : u, r;
    uint32_t i = (uint32_t)a >> 8;
    if (i >= DV_TANH_SIZE) r = dv.tanh[DV_TANH_SIZE];
    else r = dv.tanh[i] + (((dv.tanh[i + 1] - dv.tanh[i]) * (a & 255)) >> 8);
    return u < 0 ? -r : r;
}

/* one chain's state: per channel, and shared */
struct chan {
    int32_t dc_x, dc_y;            /* drive's DC block, y 8 bits finer */
    int32_t hold;                  /* crush */
    int32_t lp_low, lp_band, hp_low, hp_band;   /* SVFs, Q23 */
    int32_t ph_st[6], ph_pv[6];    /* phaser */
};
struct chain {
    struct chan ch[2];
    int32_t crush_count, gate_gain, pump_env;   /* Q15; Q30 */
    uint32_t sweep_phase, clock_q16, pump_wait, pump_hold;
    int32_t lowest;
};
static struct chain master = {.gate_gain = 32768}, track[FX_BUSES] = {
    {.gate_gain = 32768}, {.gate_gain = 32768}, {.gate_gain = 32768}, {.gate_gain = 32768}};

FM1_AUDIO_CODE void perform_kick(uint32_t delay)
{
    if (fx_perf.on & PERF_PUMP)
        if (!master.pump_wait || delay + 1U < master.pump_wait) master.pump_wait = delay + 1U;
    for (unsigned b = 0; b < FX_BUSES; ++b)
        if (fx_track_perf[b].on & PERF_PUMP)
            if (!track[b].pump_wait || delay + 1U < track[b].pump_wait) track[b].pump_wait = delay + 1U;
}

/* Chamberlin SVF, two passes a sample (stable to ~14 kHz); returns low or high */
INLINE int32_t svf(int32_t x, int32_t *low, int32_t *band, int32_t f, int32_t damp, unsigned high)
{
    int32_t lo = *low, bd = *band, hi = 0;
    for (unsigned k = 0; k < 2; ++k) {
        lo += mul15(f, bd);
        hi = x - lo - mul15(damp, bd);
        bd += mul15(f, hi);
        lo = clamp(lo, -4 * Q23, 4 * Q23); bd = clamp(bd, -4 * Q23, 4 * Q23);
    }
    *low = lo; *band = bd;
    return high ? hi : lo;
}

/* Run the chain over a block of `chans` interleaved channels (1 or 2). */
static FM1_AUDIO_CODE void run(const struct fx_perf *p, struct chain *st, int32_t *buf, unsigned chans)
{
    const int32_t on = p->on;
    if (!on && st->gate_gain == 32768) { st->clock_q16 = fx_step_clock; st->lowest = 32768; return; }
    uint32_t rate = fx_step_rate;
    if (fm1_transport_playing) st->clock_q16 = fx_step_clock;   /* locked to the CR-78's step clock */
    int32_t lowest = 32768;
    for (unsigned i = 0; i < N; ++i, st->clock_q16 += rate) {
        /* the one gain shared by the channels: gate (a 1.5 ms ramp) and pump */
        if (on & PERF_GATE) {
            uint32_t c = p->gate_shift > 0 ? st->clock_q16 << p->gate_shift : p->gate_shift < 0 ? st->clock_q16 >> -p->gate_shift : st->clock_q16;
            int32_t want = (int32_t)(c & 0xFFFFU) < p->gate_len ? 32768 : p->gate_floor;
            st->gate_gain += (want - st->gate_gain) >> 6;
        } else st->gate_gain += (32768 - st->gate_gain) >> 6;
        int32_t g = st->gate_gain;
        if (g < lowest) lowest = g;
        if (on & PERF_PUMP) {
            if (st->pump_wait && !--st->pump_wait) st->pump_hold = 220;
            if (st->pump_hold) { --st->pump_hold; st->pump_env += ((1 << 30) - st->pump_env) >> 4; }
            else st->pump_env -= (int32_t)(((int64_t)st->pump_env * p->pump_release) >> 24);
            g = mul15(g, 32768 - mul15(p->pump_amount, st->pump_env >> 15));
            if (g < lowest) lowest = g;
        }
        int32_t coeff = 0;
        if (on & PERF_SWEEP) {
            st->sweep_phase += (uint32_t)p->sweep_inc;
            int32_t lfo = dv.sine[st->sweep_phase >> (32 - DV_SINE_BITS)];
            coeff = 16384 + mul15(mul15(lfo, p->sweep_depth), 13107);
        }
        if (on & PERF_CRUSH) { if (++st->crush_count >= p->crush_rate) st->crush_count = 0; }
        for (unsigned c = 0; c < chans; ++c) {
            struct chan *s = &st->ch[c];
            int32_t x = clamp(buf[chans * i + c], -2 * Q23, 2 * Q23);
            if (on & PERF_DRIVE) {   /* DRIVE_ASYM on Q15, normalized at 0.2, DC blocked */
                int32_t q = x >> 8;
                int32_t u = clamp((int32_t)(((int64_t)q * (q >= 0 ? p->drive_pos : p->drive_neg)) >> 12), -262143, 262143);
                int32_t y = (int32_t)(((int64_t)tanh_q15(u) * p->drive_norm) >> 15) << 8;
                int32_t o = (y - s->dc_x) + (int32_t)(((int64_t)s->dc_y * 32735) >> 15);
                s->dc_x = y; s->dc_y = o;
                x = mul15(x + mul15(o - x, p->drive_mix), p->drive_level);
            }
            if (on & PERF_CRUSH) {
                if (!st->crush_count) { int32_t step = p->crush_step; s->hold = (x + (step >> 1)) & ~(step - 1); }
                x += mul15(s->hold - x, p->crush_mix);
            }
            if (on & PERF_LPF) x = svf(x, &s->lp_low, &s->lp_band, p->lpf_f, p->lpf_damp, 0);
            if (on & PERF_HPF) x = svf(x, &s->hp_low, &s->hp_band, p->hpf_f, p->hpf_damp, 1);
            if (on & PERF_SWEEP) {   /* phaser_chan: six allpasses on the swept coefficient, with feedback */
                int32_t in = clamp(x + mul15(s->ph_st[5], p->sweep_fb), -4 * Q23, 4 * Q23);
                for (unsigned k = 0; k < 6; ++k) {
                    int32_t ap = mul15(coeff, in - s->ph_st[k]) + s->ph_pv[k];
                    s->ph_pv[k] = in; s->ph_st[k] = clamp(ap, -4 * Q23, 4 * Q23); in = s->ph_st[k];
                }
                x += mul15(in - x, p->sweep_mix);
            }
            if (g != 32768) x = mul15(x, g);
            buf[chans * i + c] = x;
        }
    }
    st->lowest = lowest;
}

FM1_AUDIO_CODE void perform_master(int32_t *out)
{
    run(&fx_perf, &master, out, 2);
    fx_perf_lfo = master.sweep_phase;
    fx_perf_gain = master.lowest;
}

FM1_AUDIO_CODE void perform_tracks(int32_t (*bus)[N])
{
    static int32_t q23[N];
    for (unsigned b = 0; b < FX_BUSES; ++b) {
        if (!fx_track_perf[b].on && track[b].gate_gain == 32768) { track[b].clock_q16 = fx_step_clock; continue; }
        for (unsigned i = 0; i < N; ++i) q23[i] = bus[b][i] << 8;   /* the bus is Q15 */
        run(&fx_track_perf[b], &track[b], q23, 1);
        for (unsigned i = 0; i < N; ++i) bus[b][i] = q23[i] >> 8;
    }
}
