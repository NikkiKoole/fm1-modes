/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* FX audio: the shared rack, the buses, the duck. Integer only, no division,
 * no calls outside the app's audio code, no struct copies or memset/memcpy.
 * Delay lines are int16 to keep RAM small (about 23 KB in all). */
#include "fx.h"
#include "drumvoice.h"   /* dv.tanh, dv.sine */

#define INLINE static inline __attribute__((always_inline))
enum { N = FM1_BLOCK_FRAMES };

struct fx_params fx_params = {
    .crush_step = 1, .crush_rate = 1, .eq_low = 4096, .eq_mid = 4096, .eq_high = 4096,
    .spring_fb = 26214, .spring_damp = 29491, .spring_disp = 20316, .duck_release = 1900,
    .duck_keys = FX_KEY_CR78 | FX_KEY_RHYTHM,
};
volatile int32_t fx_duck_gain = 32768, fx_wet_peak;

INLINE int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
INLINE int16_t sat16(int32_t v) { return (int16_t)clamp(v, -32768, 32767); }
INLINE int32_t mul15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
INLINE int32_t tanh_q15(int32_t u)   /* the drum engine's table, tanh over [0, 8) */
{
    int32_t a = u < 0 ? -u : u, r;
    uint32_t i = (uint32_t)a >> 8;
    if (i >= DV_TANH_SIZE) r = dv.tanh[DV_TANH_SIZE];
    else r = dv.tanh[i] + (((dv.tanh[i + 1] - dv.tanh[i]) * (a & 255)) >> 8);
    return u < 0 ? -r : r;
}

/* ~7 Hz one-pole DC block. The feedback state is kept 8 bits finer: at plain
 * Q15 the rounding settles into a constant offset (a limit cycle near 1000). */
INLINE int32_t dc_block(int32_t in, int32_t *x1, int32_t *y1)
{
    int32_t out = ((in - *x1) << 8) + (int32_t)(((int64_t)*y1 * 32735) >> 15);
    *x1 = in; *y1 = out;
    return out >> 8;
}

/* ---- DUCK (sc_apply, keyed): a kick opens the envelope, it releases slowly ---- */
static uint32_t duck_wait, duck_hold;
static int32_t duck_env;   /* Q30 */
FM1_AUDIO_CODE void fx_kick(unsigned source, uint32_t delay)
{
    perform_kick(delay);   /* the PMP pad hears every kick */
    if (!(fx_params.duck_keys & (int32_t)source) || !fx_params.duck_amount) return;
    if (!duck_wait || delay + 1U < duck_wait) duck_wait = delay + 1U;
}
INLINE int32_t duck_step(int32_t amount, int32_t release)
{
    if (duck_wait && !--duck_wait) duck_hold = 220;          /* 5 ms at the top */
    if (duck_hold) { --duck_hold; duck_env += ((1 << 30) - duck_env) >> 4; }   /* ~0.4 ms attack */
    else duck_env -= (int32_t)(((int64_t)duck_env * release) >> 24);
    return 32768 - mul15(amount, duck_env >> 15);
}

/* ---- COLOR ---- */
static int32_t drive_x1, drive_y1, crush_hold, crush_count, eq_lo, eq_hi;   /* eq states Q23 */
/* ---- MOTION: one modulated delay line (chorus / flanger), one allpass chain (phaser / vibe) ---- */
enum { MOD_LEN = 2048, MOD_MASK = MOD_LEN - 1 };
static int16_t mod_buf[MOD_LEN];
static uint32_t mod_w, lfo_phase;
static int32_t ph_st[4], ph_pv[4], flg_x1, flg_y1;
/* ---- SPACE: dreamengine's spring voicing on the Schroeder core ---- */
enum { PRE_LEN = 1024, PRE_DELAY = 882, SP_STAGES = 8 };
static const uint16_t sp_len[SP_STAGES] = {89, 113, 67, 97, 127, 71, 107, 83};
static const uint16_t sp_off[SP_STAGES] = {0, 89, 202, 269, 366, 493, 564, 671};   /* 754 total */
static const uint16_t comb_len[4] = {1559, 1621, 1493, 1427};
static const uint16_t comb_off[4] = {0, 1559, 3180, 4673};                         /* 6100 total */
static const uint16_t ap_len[3] = {223, 557, 541};                                 /* the last one is R */
static const uint16_t ap_off[3] = {0, 223, 780};                                   /* 1321 total */
static int16_t pre_buf[PRE_LEN], sp_buf[754], comb_buf[6100], ap_buf[1321];
static uint16_t sp_pos[SP_STAGES], comb_pos[4], ap_pos[3], pre_w;
static int32_t sp_hp, sp_lp, comb_lp[4];   /* sp_hp in Q23 */
static uint32_t tail;   /* blocks the rack keeps running after the last send */

INLINE int32_t allpass(int16_t *buf, uint16_t *pos, unsigned len, int32_t in, int32_t g)
{
    int32_t tapped = buf[*pos], out = tapped - mul15(g, in);
    buf[*pos] = sat16(in + mul15(g, out));
    if (++*pos >= len) *pos = 0;
    return out;
}
INLINE int32_t mod_read(uint32_t back_q8)   /* linear read `back` samples behind the write head */
{
    uint32_t at = (mod_w << 8) - back_q8;
    uint32_t i = (at >> 8) & MOD_MASK, frac = at & 255U;
    int32_t a = mod_buf[i], b = mod_buf[(i + 1U) & MOD_MASK];
    return a + (((b - a) * (int32_t)frac) >> 8);
}

static FM1_AUDIO_CODE void rack(int32_t x, int32_t *left, int32_t *right)
{
    const struct fx_params *p = &fx_params;
    /* DRIVE: DRIVE_ASYM, normalized so full scale stays full scale, then a ~7 Hz DC block */
    if (p->drive_pos) {
        int32_t u = clamp((int32_t)(((int64_t)x * (x >= 0 ? p->drive_pos : p->drive_neg)) >> 12), -262143, 262143);
        int32_t y = (int32_t)(((int64_t)tanh_q15(u) * p->drive_norm) >> 15);
        x = dc_block(y, &drive_x1, &drive_y1);
    }
    /* CRUSH: sample-and-hold every n, rounded to the step */
    if (p->crush_rate > 1 || p->crush_step > 1) {
        if (++crush_count >= p->crush_rate) {
            crush_count = 0;
            int32_t step = p->crush_step;
            crush_hold = step > 1 ? (x + (step >> 1)) & ~(step - 1) : x;
        }
        x = crush_hold;
    }
    /* EQ: three fixed bands split at 80 Hz and 6 kHz */
    if (p->eq_on) {
        eq_lo += (int32_t)(((int64_t)((x << 8) - eq_lo) * 371) >> 15);
        eq_hi += (int32_t)(((int64_t)((x << 8) - eq_hi) * 18842) >> 15);
        int32_t low = eq_lo >> 8, high = x - (eq_hi >> 8), mid = x - low - high;
        x = (int32_t)(((int64_t)low * p->eq_low + (int64_t)mid * p->eq_mid + (int64_t)high * p->eq_high) >> 12);
    }
    x = clamp(x, -65536, 65535);
    int32_t l = x, r = x;
    /* MOTION */
    int32_t type = p->motion, mix = p->motion_mix, depth = p->motion_depth;
    lfo_phase += (uint32_t)p->motion_inc;
    uint32_t ph = lfo_phase >> 16;   /* 0..65535 */
    if (type == FX_MOTION_CHORUS) {
        /* chorus_process: rounded triangle, antiphase taps -> a wide stereo pair */
        mod_buf[mod_w & MOD_MASK] = sat16(x); ++mod_w;
        int32_t tri = ph < 32768U ? (int32_t)ph * 2 - 32768 : 98303 - (int32_t)ph * 2;
        tri = mul15(tri, 49152 - (mul15(tri, tri) >> 1));
        int32_t mod = (int32_t)(((int64_t)depth * 141120) >> 15);          /* Q8 samples */
        int32_t d1 = clamp(197568 + mul15(tri, mod), 56448, 338688);
        int32_t d2 = clamp(197568 - mul15(tri, mod), 56448, 338688);
        int32_t w1 = mod_read((uint32_t)d1), w2 = mod_read((uint32_t)d2);
        l = x + mul15(w1 - x, mix); r = x + mul15(w2 - x, mix);
    } else if (type == FX_MOTION_FLANGER) {
        /* flanger_process: a 0.1..10 ms swept tap with feedback */
        int32_t tri = ph < 32768U ? (int32_t)ph : 65535 - (int32_t)ph;     /* 0..32767 */
        int32_t ds = 1129 + (int32_t)(((int64_t)mul15(tri, depth) * 111767) >> 15);
        int32_t wet = mod_read((uint32_t)(ds < 256 ? 256 : ds));
        int32_t hp = dc_block(wet, &flg_x1, &flg_y1);
        mod_buf[mod_w & MOD_MASK] = sat16(clamp(x + mul15(hp, 19661), -49152, 49152)); ++mod_w;
        l = r = x + mul15(wet - x, mix);
    } else if (type == FX_MOTION_PHASER || type == FX_MOTION_VIBE) {
        /* phaser_chan: four first-order allpasses on one swept coefficient */
        int32_t lfo;
        if (type == FX_MOTION_PHASER) lfo = dv.sine[ph >> (16 - DV_SINE_BITS)];
        else {   /* the optical bulb: a slow brighten over 80%, a quick fall */
            int32_t u;
            if (ph < 52429U) { u = 65535 - (int32_t)((ph * 5U) >> 2); u = 65535 - (int32_t)(((int64_t)u * u) >> 16); }
            else u = 65535 - (int32_t)((ph - 52429U) * 5U);
            lfo = clamp(u, 0, 65535) - 32768;
        }
        int32_t coeff = 16384 + mul15(mul15(lfo, depth), 13107);
        int32_t in = clamp(x + (type == FX_MOTION_PHASER ? (ph_st[3] >> 1) : 0), -49152, 49152);
        for (unsigned s = 0; s < 4; ++s) {
            int32_t ap = mul15(coeff, in - ph_st[s]) + ph_pv[s];
            ph_pv[s] = in; ph_st[s] = clamp(ap, -98304, 98304); in = ph_st[s];
        }
        l = r = x + mul15(in - x, mix);
    }
    /* SPACE: the spring, in parallel, fed at a quarter level for headroom in int16 lines */
    int32_t smix = p->spring_mix;
    if (smix) {
        int32_t s_in = (l + r) >> 3;
        int32_t pre = pre_buf[(pre_w - PRE_DELAY) & (PRE_LEN - 1)];
        pre_buf[pre_w & (PRE_LEN - 1)] = sat16(s_in); ++pre_w;
        int32_t d = pre, g = p->spring_disp;
        for (unsigned i = 0; i < SP_STAGES; ++i) d = allpass(sp_buf + sp_off[i], &sp_pos[i], sp_len[i], d, g);
        sp_hp += (int32_t)(((int64_t)((d << 8) - sp_hp) * 328) >> 15); d -= sp_hp >> 8;   /* ~75 Hz out */
        sp_lp += mul15(d - sp_lp, 13763); d = sp_lp;                                        /* ~4 kHz out */
        int32_t fb = p->spring_fb, dc = p->spring_damp, sum = 0;
        for (unsigned c = 0; c < 4; ++c) {
            int16_t *buf = comb_buf + comb_off[c];
            int32_t out = buf[comb_pos[c]];
            comb_lp[c] += mul15(out - comb_lp[c], dc);
            buf[comb_pos[c]] = sat16(d + mul15(comb_lp[c], fb));
            if (++comb_pos[c] >= comb_len[c]) comb_pos[c] = 0;
            sum += out;
        }
        int32_t a1 = allpass(ap_buf + ap_off[0], &ap_pos[0], ap_len[0], sum >> 2, 16384);
        int32_t wl = allpass(ap_buf + ap_off[1], &ap_pos[1], ap_len[1], a1, 16384);
        int32_t wr = allpass(ap_buf + ap_off[2], &ap_pos[2], ap_len[2], a1, 16384);
        l += mul15(wl << 2, smix); r += mul15(wr << 2, smix);
    }
    *left = l; *right = r;
}

FM1_AUDIO_CODE void fx_mix(int32_t *output, int32_t (*bus)[N])
{
    const struct fx_params *p = &fx_params;
    perform_tracks(bus);   /* each track's own pads first */
    int32_t send[FX_BUSES], any = 0;
    for (unsigned b = 0; b < FX_BUSES; ++b) { send[b] = p->send[b]; any |= send[b]; }
    if (any) tail = 1400;   /* ~4 s: lets the spring ring out after the last send */
    const unsigned run = tail != 0;
    if (!any && tail) --tail;
    const int32_t amount = p->duck_amount, release = p->duck_release, duck_fm1 = p->duck_fm1;
    int32_t lowest = 32768, peak = fx_wet_peak;
    for (unsigned i = 0; i < N; ++i) {
        int32_t g = duck_step(amount, release);
        if (g < lowest) lowest = g;
        int32_t dry = 0, wet = 0;
        for (unsigned b = 0; b < FX_BUSES; ++b) {
            int32_t x = bus[b][i];
            if (b == FX_BASS || b == FX_SYNTH) x = mul15(x, g);
            int32_t s = mul15(x, send[b]);
            wet += s; dry += x - s;
        }
        int32_t l = dry, r = dry;
        if (run) {
            int32_t wl, wr;
            rack(wet, &wl, &wr);
            l += wl; r += wr;
            int32_t a = wl < 0 ? -wl : wl;
            if (a > peak) peak = a;
        }
        int64_t ol = output[2 * i], orr = output[2 * i + 1];
        if (duck_fm1) { ol = (ol * g) >> 15; orr = (orr * g) >> 15; }
        ol += (int64_t)clamp(l, -32768, 32767) << 8;
        orr += (int64_t)clamp(r, -32768, 32767) << 8;
        output[2 * i] = (int32_t)(ol > 0x7fffffff ? 0x7fffffff : ol < -0x7fffffff ? -0x7fffffff : ol);
        output[2 * i + 1] = (int32_t)(orr > 0x7fffffff ? 0x7fffffff : orr < -0x7fffffff ? -0x7fffffff : orr);
    }
    fx_duck_gain = lowest;
    fx_wet_peak = peak;
    perform_master(output);   /* the CR-78's pads, on everything */
}
