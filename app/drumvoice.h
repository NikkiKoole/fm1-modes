/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* A small analog-drum voice engine shared by CR-78 and RHYTHM BOX.
 *
 * Each drum is one to four partials. A partial follows dreamengine's own
 * voice path: oscillator (sine/tri/square/noise or the guiro scrape) -> SVF
 * (low/band/high, 1-4 stable substeps) -> optional asymmetric tube drive + DC
 * block -> optional sample&hold tremolo -> linear A/D/S, gate, linear or
 * exponential release, with an optional signed pitch envelope.
 *
 * Audio side: integer only, no division, no calls outside this file.
 * Control side: dv_build_part() turns a float recipe into a ready partial. */
#ifndef FM1_DRUMVOICE_H
#define FM1_DRUMVOICE_H
#include "fm1.h"

enum { DV_SINE, DV_TRI, DV_SQUARE, DV_NOISE, DV_SCRAPE };
enum { DV_FILTER_NONE, DV_FILTER_LOW, DV_FILTER_BAND, DV_FILTER_HIGH };
enum { DV_MAX_PARTS = 4, DV_JITTERS = 16, DV_SINE_BITS = 10, DV_TANH_SIZE = 1024,
       DV_PITCH_LOW = 8 /* semitones below the note a pitch envelope may reach */ };

struct dv_part {
    uint8_t wave, filter, substeps, exp_release, vol, body, drive;
    int8_t transpose;                  /* semitones added to a per-trigger note */
    uint32_t inc, rec;                 /* phase step; 2^47/inc for PolyBLEP */
    int32_t f, damping;                /* SVF per substep, Q15 */
    uint32_t attack, decay, gate, release, offset;   /* samples */
    int32_t attack_step, decay_step, sustain, release_step, release_k;   /* Q30 */
    int32_t gain;                      /* Q15 level */
    int32_t pitch, pitch_step;         /* Q16 semitones at onset, change per sample toward 0 */
    uint32_t tooth_gap;                /* scrape: Q8 samples between teeth */
    int32_t drive_pos, drive_neg, drive_norm;   /* Q12 pre-gains, Q15 1/tanh(g) */
    uint32_t lfo_period;               /* Q8 samples per S&H step, 0 = off */
    int32_t lfo_depth;                 /* Q15 */
};
struct dv_voice { uint8_t parts; struct dv_part part[DV_MAX_PARTS]; };

struct dv_body {   /* a scrape tooth's two-mode body, one per jitter */
    int32_t b0[2], a1[2], a2[2];       /* Q28 constant-peak bandpasses */
    int32_t g2, gain;                  /* Q15, Q8 */
    int32_t lowpass, highpass, ring, noise, friction;   /* Q15 */
};
struct dv_tables {
    int16_t sine[(1 << DV_SINE_BITS) + 1];
    int16_t tanh[DV_TANH_SIZE + 1];    /* tanh(x), x in [0, 8) */
    uint32_t pitch_ratio[(DV_PITCH_LOW + 24) * 8 + 2];   /* Q16, 1/8 semitone steps */
    uint32_t note_inc[128], note_rec[128];
    struct dv_body body[2][DV_JITTERS];
    uint32_t step_samples[301];        /* Q8 samples per sixteenth, by BPM */
    uint32_t reciprocal[17];           /* Q16 1/n */
    uint8_t ready;
};
extern struct dv_tables dv;

/* Audio ISR. note >= 0 replaces the recipe pitch (organ/bass); gate > 0 holds
 * the voice that many samples instead of its own gate. level is Q15. */
FM1_AUDIO_CODE void dv_trigger(const struct dv_voice *voice, int boost, uint32_t delay,
                               int32_t level, uint32_t gate, int note);
/* Render every sounding partial and add it to the block (interleaved Q23 stereo). */
FM1_AUDIO_CODE void dv_output(int32_t *output);

/* Control task. */
struct dv_recipe {
    uint8_t wave, note, filter, reso, sustain, exp_release, vol, body;
    int8_t transpose;
    uint16_t cutoff, attack, decay, release, gate, offset;   /* Hz, ms */
    float tune, level, pitch, pitch_ms, teeth, drive, lfo_hz, lfo_depth;
};
void dv_build_tables(void);
void dv_build_part(struct dv_part *out, const struct dv_recipe *r, float tune, float stretch);
#endif
