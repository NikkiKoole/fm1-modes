/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO: 90s free-party tekno (French hardtek, Czech tribe), played live. See tekno.c.
 * Its own voices (tekno_dsp.c), on the FM-1's clock: a RUMBLE KICK whose long tuned tail is the
 * bass, or a ROLL (a short kick on the beat, bass notes on the sixteenths between), two tuned TOMS
 * rolling on the sixteenths, HATS and a CLAP, a PHASER locked to the bars over all the percussion,
 * a VOWEL filter (tiny acid jam's formant: four vowel bands) swept on the bars over everything, and
 * an ACID line: a 303 (Open303) with the Devil Fish's controls (tekno_acid.c), and a GRENADIER drone
 * (a triple filterbank after the Grendel RA-99, tekno_gren.c). */
#ifndef FM1_TEKNO_H
#define FM1_TEKNO_H
#include "fm1.h"

enum { TEKNO_KICK = 1, TEKNO_TOMS = 2, TEKNO_ROLL = 4, TEKNO_HATS = 8, TEKNO_CLAP = 16, TEKNO_ACID = 32, TEKNO_GREN = 64 };   /* layers (KICK is the rumble kick) */
enum { TEKNO_LANE_TOMS, TEKNO_LANE_HATS, TEKNO_LANE_CLAP, TEKNO_LANES };   /* the patterned layers */
enum { TEKNO_STEPS = 16, TEKNO_RATIOS = 48 * 8 + 1 };     /* pitch ratios: 0..48 semitones in eighths */
enum { TEKNO_VOWELS = 65 };                               /* the vowel path U O A E I in 64 steps */

/* the acid's steps: the note (semitones over the base) in the low 6 bits, and these */
enum { TEKNO_ACID_NOTE = 0x40, TEKNO_ACID_ACCENT = 0x80, TEKNO_ACID_SLIDE = 0x100, TEKNO_ACID_TIE = 0x200 };
struct tekno_svf { float g, r, h; };          /* a trapezoidal SVF's coefficients */
/* ACID: everything the 303 needs, worked out by the panel (floats; the audio side only reads) */
struct tekno_acid {
    float cutoff, reso, env_scale, env_offset;   /* Hz; Open303's skewed resonance; its envelope mapping */
    float decay, accent_decay, accent, slide;    /* per-sample multipliers; 0..1; the slide's per-sample pull */
    float release, release_acc;                  /* the amp's release after the gate (plain, accented) */
    float ffm, sub, track, level;                /* FILTER FM depth; the sub's level; TRACK 0..1; output */
    int base;                                    /* the note TRACK is centred on */
    float sweep_oct, sweep_res;                  /* SWEEP: octaves opened at its top; resonance added (0..1 of the way to full) */
    float drive_gain, drive_high;                /* RAT (gain 0: off) */
    struct tekno_svf click, notch, drive_pre, drive_post;
    float pre_a, fb_a, post_a, drive_dc_a, b0max;
    float down0[3], down1[3];                    /* the 2x decimator's two low-pass sections */
};

/* GRENADIER: worked out by the panel (floats) */
struct tekno_gren {
    float base_hz, space, r, morph;              /* the first filter; the ratio up to the next; 1/Q; the trapezoid's steepness */
    float move_oct, level;                       /* the bars' sweep, octaves each way; output */
};
enum { TEKNO_GATE_DRONE, TEKNO_GATE_4THS, TEKNO_GATE_8THS, TEKNO_GATE_16THS, TEKNO_GATE_GALLOP, TEKNO_GATES };

/* Control writes whole words; audio only reads (except the readback at the end). */
struct tekno_params {
    volatile uint8_t engaged;                /* the mode owns the output: the stock song is silent under it */
    volatile uint8_t want;                   /* layers asked for; they change on the next bar */
    /* each lane's bar of sixteen steps, copied in at the next bar: 0 rest, | 4 accented, and
     * TOMS 1 high, 2 low; HATS 1 closed, 2 open; CLAP 1 a clap */
    volatile uint8_t lane_want[TEKNO_LANES][TEKNO_STEPS];
    volatile uint8_t lane_dirty;             /* bit l: lane l's pattern above is new (audio clears it) */
    /* KICK */
    volatile uint32_t kick_inc;              /* the tail's note, Q32 turns a sample */
    volatile int32_t kick_pitch;             /* the drop: Q16 semitones above the note at the hit */
    volatile int32_t kick_pitch_k;           /* ... times this every 16 samples, Q30 */
    volatile int32_t kick_decay_k;           /* the tail: the level times this every sample, Q30 */
    volatile int32_t kick_click;             /* Q15 */
    volatile int32_t kick_drive_pos, kick_drive_neg, kick_drive_norm;   /* Q12 input gains (0 off); Q15 1/tanh(g) */
    volatile int32_t kick_tone;              /* a one-pole low pass after the drive, Q15 */
    volatile int32_t kick_level;             /* Q15 */
    /* ROLL: the bass between the kicks, on the kick's note. feel: bit s = a bass note on step s of the bar */
    volatile uint16_t roll_feel;
    volatile int32_t roll_decay_k;           /* Q30 a sample */
    volatile int32_t roll_f, roll_damp;      /* the low pass: SVF Q15 */
    volatile int32_t roll_drive, roll_drive_norm;   /* Q12 gain (0 off); Q15 1/tanh(g) */
    volatile int32_t roll_level;             /* Q15 */
    /* TOMS: high and low */
    volatile uint32_t tom_inc[2];
    volatile int32_t tom_pitch, tom_pitch_k, tom_decay_k, tom_level, tom_skin;   /* skin: the head's noise, Q15 */
    /* HATS: noise through a high pass, closed or open */
    volatile int32_t hat_f, hat_damp, hat_closed_k, hat_open_k, hat_level;   /* SVF Q15; Q30 a sample; Q15 */
    /* CLAP: noise through a band pass, three quick bursts and a tail */
    volatile int32_t clap_f, clap_damp, clap_tail_k, clap_level;   /* SVF Q15; Q30 a sample; Q15 */
    volatile uint32_t clap_gap;              /* samples between the bursts */
    /* PHASER on the percussion: one sweep every 16 << bars_shift steps */
    volatile int32_t ph_depth, ph_feedback, ph_mix;   /* Q15 */
    volatile uint8_t ph_bars_shift;          /* 0 one bar, 1 two, 2 four, 3 eight */
    /* VOWEL: the vowel's place (Q8, 0..64 << 8), the sweep around it (Q8 places at the LFO's peak),
     * one sweep every 4 << rate steps (0 a beat .. 5 eight bars), the mix (Q15) */
    volatile int32_t vow_center, vow_depth, vow_mix;
    volatile uint8_t vow_rate;
    /* SWEEP: the acid's cutoff ridden on the bars (amounts in `acid`), one ride every 16 << shift steps;
     * shape 0 up and down, 1 rising (snapping shut at the loop), 2 falling */
    volatile uint8_t asw_shift, asw_shape;
    /* FLANGE on the hats: a short delay swept on the bars (one sweep every 16 << shift steps) */
    volatile int32_t fl_depth, fl_feedback, fl_mix;   /* Q15 */
    volatile uint8_t fl_shift;
    /* ACID: its bar of steps (copied in at the next bar, like a lane), the note step 0 is on */
    volatile uint16_t acid_want[TEKNO_STEPS];
    volatile uint8_t acid_dirty;
    volatile uint8_t acid_root;
    struct tekno_acid acid;
    /* GRENADIER: the gate; the sweep's rate, 0 half a beat .. 3 a bar .. 6 eight bars (one sweep every 2 << rate steps) */
    volatile uint8_t gren_gate, gren_rate;
    struct tekno_gren gren;
    uint32_t ratio[TEKNO_RATIOS];            /* 2^(i/96), Q16 (control fills it once) */
    int16_t vowel[TEKNO_VOWELS][3][4];       /* per place, the four bands' SVF f, damping, level (Q15) */
};
extern struct tekno_params tekno;
/* readback for the panel (written by audio) */
extern volatile uint8_t tekno_layers, tekno_step, tekno_bar;   /* playing; the step and bar (of 8) */
extern volatile uint16_t tekno_lfo, tekno_vlfo;                /* the phaser's and the vowel's position, 0..65535 */
extern volatile uint16_t tekno_alfo, tekno_flfo, tekno_glfo;               /* the acid sweep's and the flanger's (0 .. 65535 = closed .. open / short .. long) */
extern volatile int32_t tekno_vowel_at;                        /* where the vowel is now, Q8 */
extern volatile int32_t tekno_peak;                            /* Q15, since the panel cleared it */

/* Audio ISR: one FM-1 clock step (from cr78_tick), `late` samples after the step was due; and the
 * block's sound into `bus` (mono Q15, added). */
FM1_AUDIO_CODE void tekno_clock(unsigned step, unsigned tempo, uint32_t late, unsigned restart);
FM1_AUDIO_CODE void tekno_render(int32_t *bus);

/* the ACID voice (tekno_acid.c), audio side */
FM1_AUDIO_CODE void tekno_acid_note(int note, int accent, int slide);
FM1_AUDIO_CODE void tekno_acid_off(void);
FM1_AUDIO_CODE int32_t tekno_acid_sample(void);
FM1_AUDIO_CODE void tekno_acid_sweep(float u);   /* where the SWEEP is, 0..1 */
FM1_AUDIO_CODE unsigned tekno_acid_sounding(void);

/* the GRENADIER voice (tekno_gren.c), audio side */
FM1_AUDIO_CODE void tekno_gren_note(int note);
FM1_AUDIO_CODE void tekno_gren_pulse(float decay);
FM1_AUDIO_CODE void tekno_gren_off(void);
FM1_AUDIO_CODE void tekno_gren_move(float u);   /* the sweep, -1..1 */
FM1_AUDIO_CODE unsigned tekno_gren_sounding(void);
FM1_AUDIO_CODE int32_t tekno_gren_sample(void);

int tekno_control(const struct fm1_controls *controls);
unsigned tekno_active(void);
#endif
