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
    volatile int32_t duck;     /* Q15 taken off the level (the GROOVEBOX's track level); 0 = none */
    volatile int32_t glide;    /* Q15 of the way to a new pitch each block (the LEAD's; 32768 = at once) */
    int32_t ftab[MYSYNTH_FTAB];/* Q15 SVF frequency coefficient at 2x rate */
};
extern struct mysynth_params mysynth_params, mysynth_lead_params;   /* MY SYNTH (and KEYS); the LEAD */

/* Control task -> audio ISR. Returns 0 queued, -1 full. */
enum { MYSYNTH_NOTE_ON = 1, MYSYNTH_NOTE_OFF, MYSYNTH_ALL_OFF };
int mysynth_event(unsigned type, unsigned note, unsigned velocity, uint32_t inc, uint32_t rec);

/* Audio ISR: a note on or off, from the audio side (the GROOVEBOX's KEYS track). */
FM1_AUDIO_CODE void mysynth_play(unsigned note, unsigned velocity, unsigned on);
/* Audio ISR: the GROOVEBOX's LEAD, one voice of the same engine with its own sound. A note on with
 * slide while a note holds glides to it without a new attack; a note off only ends the note sounding. */
FM1_AUDIO_CODE void mysynth_lead_play(unsigned note, unsigned velocity, unsigned on, unsigned slide);
FM1_AUDIO_CODE void mysynth_lead_render(int32_t *bus);
/* Audio ISR: add one block of voices into bus (mono Q15 per frame). */
FM1_AUDIO_CODE void mysynth_render(int32_t *bus);
/* Last MYSYNTH_SCOPE mono samples (Q15) and the next write position. */
extern int16_t mysynth_scope[MYSYNTH_SCOPE];
extern volatile uint32_t mysynth_scope_pos;
unsigned mysynth_voices_active(void);

/* The sound, for other panels (the GROOVEBOX's KEYS track): OSC FILTER AMP pages and the patches. */
struct px_page;
void mysynth_ready(void);
const struct px_page *mysynth_pages(unsigned *count);
void mysynth_knob_text(unsigned page, unsigned k, char *out);
void mysynth_turn(unsigned page, unsigned k, int turn);
unsigned mysynth_patch_count(void);
unsigned mysynth_patch_index(void);
const char *mysynth_patch_name(unsigned p);
void mysynth_patch(unsigned p);
/* the same for the LEAD's own sound (its OSC page has GLIDE where MY SYNTH has OCTAVE) */
void mysynth_lead_ready(void);
const struct px_page *mysynth_lead_pages(unsigned *count);
void mysynth_lead_knob_text(unsigned page, unsigned k, char *out);
void mysynth_lead_turn(unsigned page, unsigned k, int turn);
unsigned mysynth_lead_patch_index(void);
void mysynth_lead_patch(unsigned p);
/* both sounds (MY SYNTH's and the LEAD's) as bytes, for saving (GROOVEBOX) */
enum { MYSYNTH_STATE_BYTES = 24 };
void mysynth_state_get(uint8_t out[MYSYNTH_STATE_BYTES]);
void mysynth_state_set(const uint8_t in[MYSYNTH_STATE_BYTES]);

/* Panel mode, called from groove_screen_control like game_control. */
int mysynth_control(const struct fm1_controls *controls);
unsigned mysynth_active(void);
#endif
