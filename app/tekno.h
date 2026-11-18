/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO: 90s free-party tekno (French hardtek, Czech tribe), played live. See tekno.c.
 * Its own voices (tekno_dsp.c), on the FM-1's clock: a RUMBLE KICK whose long tuned tail is the
 * bass, or a ROLL (a short kick on the beat, bass notes on the sixteenths between), two tuned TOMS
 * rolling on the sixteenths, HATS and a CLAP, a PHASER locked to the bars over all the percussion,
 * and a VOWEL filter (tiny acid jam's formant: four vowel bands) swept on the bars over everything. */
#ifndef FM1_TEKNO_H
#define FM1_TEKNO_H
#include "fm1.h"

enum { TEKNO_KICK = 1, TEKNO_TOMS = 2, TEKNO_ROLL = 4, TEKNO_HATS = 8, TEKNO_CLAP = 16 };   /* layers (KICK is the rumble kick) */
enum { TEKNO_LANE_TOMS, TEKNO_LANE_HATS, TEKNO_LANE_CLAP, TEKNO_LANES };   /* the patterned layers */
enum { TEKNO_STEPS = 16, TEKNO_RATIOS = 48 * 8 + 1 };     /* pitch ratios: 0..48 semitones in eighths */
enum { TEKNO_VOWELS = 65 };                               /* the vowel path U O A E I in 64 steps */

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
    /* ROLL: the bass between the kicks, on the kick's note. feel: bit 1 / 2 / 3 = the e / & / a */
    volatile uint8_t roll_feel;
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
    uint32_t ratio[TEKNO_RATIOS];            /* 2^(i/96), Q16 (control fills it once) */
    int16_t vowel[TEKNO_VOWELS][3][4];       /* per place, the four bands' SVF f, damping, level (Q15) */
};
extern struct tekno_params tekno;
/* readback for the panel (written by audio) */
extern volatile uint8_t tekno_layers, tekno_step, tekno_bar;   /* playing; the step and bar (of 8) */
extern volatile uint16_t tekno_lfo, tekno_vlfo;                /* the phaser's and the vowel's position, 0..65535 */
extern volatile int32_t tekno_vowel_at;                        /* where the vowel is now, Q8 */
extern volatile int32_t tekno_peak;                            /* Q15, since the panel cleared it */

/* Audio ISR: one FM-1 clock step (from cr78_tick), `late` samples after the step was due; and the
 * block's sound into `bus` (mono Q15, added). */
FM1_AUDIO_CODE void tekno_clock(unsigned step, unsigned tempo, uint32_t late, unsigned restart);
FM1_AUDIO_CODE void tekno_render(int32_t *bus);

int tekno_control(const struct fm1_controls *controls);
unsigned tekno_active(void);
#endif
