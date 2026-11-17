/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* FX: our own effects rack for the app modes.
 *
 * The app modes are added after the sealed engine has rendered, so the FM-1's
 * own reverb and delay never see them. Instead every mode renders into its own
 * mono bus, and fx_mix() sums the buses into the output. Each bus SENDS part
 * of itself through one shared rack (the rest stays dry):
 *
 *   send -> DRIVE -> CRUSH -> EQ -> MOTION (chorus / phaser / vibe / flanger)
 *        -> + SPRING reverb (parallel)  -> back next to the dry part
 *
 * and a DUCK keyed by our own kicks (CR-78, rhythm box) pumps the bass, the
 * synth and, if asked, the FM-1's own tracks. The DSP is a port of
 * dreamengine's sound.h effects (chorus_process, phaser_chan, crush_process,
 * drive_shape ASYM, the spring voicing of reverb_core, the keyed sc_apply),
 * done in integers for the audio ISR. Every stage at 0 is a true bypass, and
 * with every send at 0 the rack only runs on until its tail has died away.
 *
 * Control (fx.c) writes whole words into fx_params; audio only reads them. */
#ifndef FM1_FX_H
#define FM1_FX_H
#include "fm1.h"

enum { FX_BASS, FX_CR78, FX_RHYTHM, FX_SYNTH, FX_BUSES };
enum { FX_KEY_CR78 = 1, FX_KEY_RHYTHM = 2 };
enum { FX_MOTION_OFF, FX_MOTION_CHORUS, FX_MOTION_PHASER, FX_MOTION_VIBE, FX_MOTION_FLANGER, FX_MOTIONS };

struct fx_params {
    volatile int32_t send[FX_BUSES];        /* Q15 */
    /* COLOR */
    volatile int32_t drive_pos, drive_neg;  /* Q12 input gains; 0 = off */
    volatile int32_t drive_norm;            /* Q15 1/tanh(g) */
    volatile int32_t crush_step;            /* quantizer step (1 = off) */
    volatile int32_t crush_rate;            /* hold every n samples (1 = off) */
    volatile int32_t eq_low, eq_mid, eq_high, eq_on;   /* Q12 band gains */
    /* MOTION */
    volatile int32_t motion, motion_inc;    /* type; LFO phase step (Q32 turns per sample) */
    volatile int32_t motion_depth, motion_mix;          /* Q15 */
    /* SPACE */
    volatile int32_t spring_mix, spring_fb, spring_damp, spring_disp;   /* Q15 */
    /* DUCK */
    volatile int32_t duck_amount, duck_release;   /* Q15; Q24 per-sample release */
    volatile int32_t duck_keys, duck_fm1;         /* FX_KEY_* mask; duck the FM-1 tracks too */
};
extern struct fx_params fx_params;

/* readback for the panel (written by audio) */
extern volatile int32_t fx_duck_gain;   /* Q15, the last block's lowest */
extern volatile int32_t fx_wet_peak;    /* Q15, rack output since the panel cleared it */

/* Audio ISR. A kick from `source` (FX_KEY_*) starts the duck `delay` samples into this block. */
FM1_AUDIO_CODE void fx_kick(unsigned source, uint32_t delay);
/* Sum the buses (mono Q15) through the rack and add them to output (interleaved Q23 stereo). */
FM1_AUDIO_CODE void fx_mix(int32_t *output, int32_t (*bus)[FM1_BLOCK_FRAMES]);

/* ---- PERFORM: master effects on the WHOLE output (the FM-1's own tracks too), played from the
 * CR-78's black-key pads (perform_dsp.c). Order: drive, crush, filter, sweep, gate, pump.
 * Control writes whole words; a stage is off unless its bit is in `on`. */
enum { PERF_DRIVE = 1, PERF_CRUSH = 2, PERF_LPF = 4, PERF_HPF = 8, PERF_SWEEP = 16, PERF_GATE = 32, PERF_PUMP = 64 };
struct fx_perf {
    volatile int32_t on;
    volatile int32_t drive_pos, drive_neg, drive_norm;        /* as the rack's drive, Q12 / Q15 */
    volatile int32_t drive_mix, drive_level;                  /* Q15 wet; Q15 output level (32768 = unity) */
    volatile int32_t crush_step, crush_rate, crush_mix;       /* Q23 quantizer step; hold n; Q15 */
    volatile int32_t lpf_f, lpf_damp, hpf_f, hpf_damp;        /* Q15 SVF coefficients at 2x */
    volatile int32_t sweep_inc, sweep_depth, sweep_fb, sweep_mix;   /* phaser: Q32 turns/sample; Q15 */
    volatile int32_t gate_shift, gate_len, gate_floor;        /* division (-1 1/8, 0 1/16, 1 1/32); Q16; Q15 */
    volatile int32_t pump_amount, pump_release;              /* Q15; Q24 per sample */
};
extern struct fx_perf fx_perf;
/* readback for the pads' screens (written by audio) */
extern volatile uint32_t fx_perf_lfo;    /* the sweep's LFO phase (Q32 turns) */
extern volatile int32_t fx_perf_gain;    /* the gate and pump's gain, Q15: the lowest of the last block */
/* the step clock for the gate: Q16 steps (whole steps in the high half), set by cr78_tick each
 * block while the transport runs, and the step rate (Q16 steps per sample) */
extern volatile uint32_t fx_step_clock, fx_step_rate;
FM1_AUDIO_CODE void perform_master(int32_t *output);
/* ... and the same chain on each bus by itself (a GROOVEBOX track's own pads), before the mix */
extern struct fx_perf fx_track_perf[FX_BUSES];
FM1_AUDIO_CODE void perform_tracks(int32_t (*bus)[FM1_BLOCK_FRAMES]);
FM1_AUDIO_CODE void perform_kick(uint32_t delay);

/* Control task: the panel mode. */
int fx_control(const struct fm1_controls *controls);
unsigned fx_active(void);
unsigned fx_running(void);   /* any send or the duck is on */
#endif
