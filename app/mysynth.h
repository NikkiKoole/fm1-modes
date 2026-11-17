/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* MY SYNTH: a small subtractive poly synth that lives entirely in APP code.
 *
 * Audio side (mysynth_dsp.c) runs inside fm1_output_mix() in the audio ISR,
 * after the sealed engine has rendered, and ADDS its voices to that block.
 * It is integer-only, calls nothing outside itself, and never allocates, so
 * it is safe in the ISR and during Flash writes.
 *
 * Control side (mysynth.c) owns the panel while the mode is on (from the launcher),
 * turns knob values into ready-to-use coefficients, and queues note events.
 * Every shared word is written whole by control and only read by audio. */
#ifndef FM1_MYSYNTH_H
#define FM1_MYSYNTH_H
#include "fm1.h"

enum { MYSYNTH_VOICES = 6, MYSYNTH_FTAB = 256, MYSYNTH_SCOPE = 512 };

/* Coefficients prepared by control. Q15 = 1.0 at 32768, Q30 = 1.0 at 2^30. */
struct mysynth_params {
    volatile int32_t wave;     /* Q15: 0 saw .. 1 square */
    volatile int32_t detune;   /* Q16: oscillator 2 frequency ratio - 1 */
    volatile int32_t cutoff;   /* 0..255 index into ftab */
    volatile int32_t damping;  /* Q15: SVF 1/Q (lower = more resonance) */
    volatile int32_t envmod;   /* ftab steps added at full envelope */
    volatile int32_t attack;   /* Q30 envelope rise per block */
    volatile int32_t decay;    /* Q15 per-block multiplier toward sustain */
    volatile int32_t sustain;  /* Q30 */
    volatile int32_t release;  /* Q15 per-block multiplier */
    volatile int32_t level;    /* Q15 output gain */
    int32_t ftab[MYSYNTH_FTAB];/* Q15 SVF frequency coefficient at 2x rate */
};
extern struct mysynth_params mysynth_params;

/* Control task -> audio ISR. Returns 0 queued, -1 full. */
enum { MYSYNTH_NOTE_ON = 1, MYSYNTH_NOTE_OFF, MYSYNTH_ALL_OFF };
int mysynth_event(unsigned type, unsigned note, unsigned velocity, uint32_t inc, uint32_t rec);

/* Audio ISR: add one block of voices into bus (mono Q15 per frame). */
FM1_AUDIO_CODE void mysynth_render(int32_t *bus);
/* Last MYSYNTH_SCOPE mono samples (Q15) and the next write position. */
extern int16_t mysynth_scope[MYSYNTH_SCOPE];
extern volatile uint32_t mysynth_scope_pos;
unsigned mysynth_voices_active(void);

/* Panel mode, called from groove_screen_control like game_control. */
int mysynth_control(const struct fm1_controls *controls);
unsigned mysynth_active(void);
#endif
