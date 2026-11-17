/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX: four tracks (DRUMS BASS KEYS LEAD), one way of doing things. See GROOVEBOX-SPEC.md.
 * DRUMS: the CR-78's voices and sequencer. BASS: the drum engine's bass voice. KEYS: chords on MY SYNTH.
 * LEAD: one mono voice of MY SYNTH's engine with its own sound, gliding on slides. */
#ifndef FM1_GBX_H
#define FM1_GBX_H
#include "fm1.h"
#include "drumvoice.h"

/* a melodic line (BASS, LEAD): 16 steps, shared with the audio ISR (control writes whole bytes) */
enum { GBX_STEPS = 16 };
struct gbx_line {
    struct dv_voice voice;                         /* BASS: its sound on the drum engine */
    volatile uint16_t on, slide, accent;           /* bit s = step s */
    volatile uint8_t note[GBX_STEPS];              /* MIDI note */
    volatile uint8_t gate[GBX_STEPS];              /* sixteenths of a step: BASS 1..16, LEAD 1..64 */
    volatile uint8_t vel[GBX_STEPS];               /* 1..127 */
    volatile int32_t level;                        /* Q15 (BASS) */
    volatile uint8_t armed, muted;
};
extern struct gbx_line gbx_bass, gbx_lead;

/* the KEYS track: a chord (up to four notes) a step, played on MY SYNTH */
enum { GBX_CHORD = 4 };
struct gbx_keys {
    volatile uint16_t on, accent;
    volatile uint8_t note[GBX_STEPS][GBX_CHORD];   /* MIDI notes, 0 = none */
    volatile uint8_t gate[GBX_STEPS];              /* sixteenths of a step, 1..64 (up to four steps) */
    volatile uint8_t vel[GBX_STEPS];
    volatile uint16_t strum;                       /* samples between the chord's notes */
    volatile uint8_t armed, muted;
};
extern struct gbx_keys gbx_keys;
/* Control task: play a chord now, for `gate` samples (0 queued, -1 full). */
int gbx_chord_audition(const uint8_t notes[GBX_CHORD], uint32_t gate);
/* Audio ISR: one clock step (from cr78_tick, on the FM-1's clock), and the auditions each block. */
FM1_AUDIO_CODE void gbx_clock(unsigned step, unsigned tempo);
FM1_AUDIO_CODE void gbx_tick(void);
/* Control task: play a bass note now (0 queued, -1 full). */
int gbx_audition(unsigned note);
/* Control task: play a lead note now, for `gate` samples (0 queued, -1 full). */
int gbx_lead_audition(unsigned note, uint32_t gate);

/* Patterns: 16 slots, each all four tracks' steps (the sounds are shared). The tracks above are the
 * pattern playing; a queued pattern takes over at the next bar (at once when stopped): the audio side
 * stores the playing one back into its slot and loads the next, or with `keep` carries the playing
 * one into the new slot as it is (a fresh slot starts as a copy). */
enum { GBX_PATTERNS = 16, GBX_NONE = 255 };
extern volatile uint8_t gbx_pattern_now, gbx_pattern_next;
void gbx_pattern_queue(unsigned p, unsigned keep);

int gbx_control(const struct fm1_controls *controls);
unsigned gbx_active(void);
#endif
