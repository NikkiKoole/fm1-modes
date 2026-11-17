/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX: four tracks (DRUMS BASS KEYS LEAD), one way of doing things. See GROOVEBOX-SPEC.md.
 * Stage 1: the overview and the DRUMS track (the CR-78's voices and sequencer); BASS, KEYS and
 * LEAD are placeholders. */
#ifndef FM1_GBX_H
#define FM1_GBX_H
#include "fm1.h"
#include "drumvoice.h"

/* the BASS track: 16 steps, shared with the audio ISR (control writes whole bytes) */
enum { GBX_STEPS = 16 };
struct gbx_bass {
    struct dv_voice voice;
    volatile uint16_t on, slide, accent;           /* bit s = step s */
    volatile uint8_t note[GBX_STEPS];              /* MIDI note */
    volatile uint8_t gate[GBX_STEPS];              /* sixteenths of a step, 1..16 */
    volatile uint8_t vel[GBX_STEPS];               /* 1..127 */
    volatile int32_t level;                        /* Q15 */
    volatile uint8_t armed, muted;
};
extern struct gbx_bass gbx_bass;
/* Audio ISR: one clock step (from cr78_tick, on the FM-1's clock), and the auditions each block. */
FM1_AUDIO_CODE void gbx_clock(unsigned step, unsigned tempo);
FM1_AUDIO_CODE void gbx_tick(void);
/* Control task: play a bass note now (0 queued, -1 full). */
int gbx_audition(unsigned note);

int gbx_control(const struct fm1_controls *controls);
unsigned gbx_active(void);
#endif
