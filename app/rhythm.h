/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* RHYTHM BOX: the 101 factory rhythms of dreamengine's rhythmbox.h, read off
 * the manufacturers' own documents (Ace Tone FR-2L, Roland TR-77, the SGS
 * M252/M253/M254/M255 rhythm chips, a 1969 Hammond patent), played on the
 * TR-77 kit (tr77.h) or the Wurlitzer Side Man (sideman.h) through the shared
 * drum engine, locked to the FM-1 tempo and transport.
 *
 * Time: each rhythm says how many counts make a beat (per_beat: 4 on the
 * TR-77, 6 on the FR-2L, 8 for the chips, 12 for a 12/8 slow rock). The clock
 * advances tempo*per_beat per sample against the FM-1's own 44100*60 period,
 * so a beat here is exactly an FM-1 beat. One tick per USED count: counts a
 * rhythm skips really are skipped, which is where its meter comes from.
 *
 * The Hammond and M255 charts carry CHORD and BASS lanes that GATE the chord
 * you hold on the keys, as the organ did. */
#ifndef FM1_RHYTHM_H
#define FM1_RHYTHM_H
#include "fm1.h"
#include "drumvoice.h"

enum { RHYTHM_LANES = 12, RHYTHM_COUNTS = 48, RHYTHM_TR77_VOICES = 15, RHYTHM_SIDEMAN_VOICES = 10 };
enum { RHYTHM_KIT_TR77, RHYTHM_KIT_SIDEMAN };
enum { RHYTHM_ROLE_DRUM = 1, RHYTHM_ROLE_CHORD = 2, RHYTHM_ROLE_BASSLO = 4, RHYTHM_ROLE_BASSHI = 8 };

struct rhythm_lane {
    uint64_t fire;                     /* counts where this lane triggers */
    uint8_t role, voices, voice[2], pad[4];
    uint8_t hold[RHYTHM_COUNTS];       /* gate length in half-ticks, 0 = the voice's own */
};
struct rhythm_pattern {
    uint8_t kit, used, per_beat, lanes;
    uint8_t order[RHYTHM_COUNTS];      /* the counts this rhythm plays, in order */
    struct rhythm_lane lane[RHYTHM_LANES];
};
struct rhythm_shared {
    struct rhythm_pattern pattern[2];  /* control fills the idle one, then flips `live` */
    volatile uint8_t live;
    struct dv_voice tr77[RHYTHM_TR77_VOICES], sideman[RHYTHM_SIDEMAN_VOICES], organ, bass;
    volatile uint8_t armed, muted;
    volatile uint8_t chord[3], bass_note[2];   /* MIDI notes the accompaniment lanes gate */
    volatile int32_t level;            /* Q15 */
};
extern struct rhythm_shared rhythm;

/* Audio ISR. */
FM1_AUDIO_CODE void rhythm_transport(uint8_t status);
FM1_AUDIO_CODE void rhythm_tick(void);
extern volatile uint8_t rhythm_count, rhythm_running;
extern volatile uint16_t rhythm_hits;  /* lanes fired since the panel last cleared it */

/* Control task. */
int rhythm_control(const struct fm1_controls *controls);
unsigned rhythm_active(void);
#endif
