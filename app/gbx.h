/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX: four tracks (DRUMS BASS KEYS LEAD), one way of doing things. The panel's rules are
 * GROOVEBOX-MANUAL.md. GROOVE MINI is the same instrument cut down to DRUMS (kick, snare, hat) and BASS.
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
    volatile uint16_t skip;                        /* bit s: step s was just played live (recorded early): skip it once */
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
    volatile uint16_t skip;
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

/* ---- patterns: 16 slots, each all four tracks' steps (the sounds are shared) ---- */
enum { GBX_PATTERNS = 16, GBX_NONE = 255, GBX_DRUM_VOICES = 16 };
struct gbx_line_steps { uint16_t on, slide, accent; uint8_t note[GBX_STEPS], gate[GBX_STEPS], vel[GBX_STEPS]; };
struct gbx_pattern {
    uint16_t grid[GBX_DRUM_VOICES], accent;
    uint8_t ratchet[GBX_DRUM_VOICES][GBX_STEPS / 2];   /* two a byte, as ratchet - 1 */
    struct gbx_line_steps bass, lead;
    uint16_t keys_on, keys_accent;
    uint8_t keys_note[GBX_STEPS][GBX_CHORD], keys_gate[GBX_STEPS], keys_vel[GBX_STEPS];
};
extern struct gbx_pattern gbx_patterns[GBX_PATTERNS];
/* The tracks above are the pattern playing. A queued pattern takes over at the next bar (at once when
 * stopped): the audio side stores the playing one back into its slot and loads the next; a slot never
 * used starts as a copy of the one playing. A chain (2..16 slots) plays its slots in turn, a bar each. */
extern volatile uint8_t gbx_pattern_now, gbx_pattern_next;
extern volatile uint16_t gbx_patterns_used;
enum { GBX_CHAIN_MAX = 16 };
extern volatile uint8_t gbx_chain[GBX_CHAIN_MAX], gbx_chain_len, gbx_chain_pos;
void gbx_pattern_queue(unsigned p);
void gbx_chain_set(const uint8_t *slots, unsigned n);   /* n = 1: just that pattern */

/* The playing pattern <-> a pattern record (control task: copy, paste, clear, undo, saving).
 * `what`: GBX_ALL, or a track (0 DRUMS, 1 BASS, 2 KEYS, 3 LEAD), | GBX_STEP_BIT * (step + 1) for one step. */
enum { GBX_ALL = 15, GBX_STEP_BIT = 16 };
void gbx_pattern_take(struct gbx_pattern *p);
void gbx_pattern_put(const struct gbx_pattern *p, unsigned what);
void gbx_pattern_wipe(struct gbx_pattern *p, unsigned what);

int gbx_control(const struct fm1_controls *controls);
unsigned gbx_active(void);
int gbx_mini_control(const struct fm1_controls *controls);   /* GROOVE MINI */
unsigned gbx_mini_active(void);
#endif
