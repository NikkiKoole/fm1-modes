/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* BASS PLAYER: a follower (opened from the launcher). It listens to everything melodic the FM-1 plays
 * (the sequencer and arpeggiator via groove_midi_note, your panel keys via
 * groove_midi_local, external MIDI via fm1_app_midi_received), names the
 * chord, latches the chord tones into patgen (patgen.h) and plays the line on
 * the FM-1 beat with its own voice on the shared drum engine.
 *
 * When the chord changes only the changed tones are swapped, so patgen keeps
 * its place: the drifting phrase carries on across the change. */
#ifndef FM1_BASS_H
#define FM1_BASS_H
#include "fm1.h"
#include "drumvoice.h"

enum { BASS_HISTORY = 32, BASS_NONE = 255 };
enum { BASS_MAJ, BASS_MIN, BASS_DOM7, BASS_MIN7, BASS_MAJ7, BASS_SUS4, BASS_SUS2, BASS_DIM,
       BASS_HALFDIM, BASS_AUG, BASS_POWER, BASS_QUALITIES };
enum { BASS_TONES_ROOT, BASS_TONES_ROOT5, BASS_TONES_TRIAD, BASS_TONES_TRIAD8, BASS_TONES_SEVENTH, BASS_TONE_SETS };

struct bass_shared {
    struct dv_voice voice;
    volatile uint8_t armed, muted, order, rest, per_beat, tones, octave, hold;
    volatile uint8_t window;           /* beats of listening per chord decision: 1, 2 or 4 */
    volatile uint16_t listen;          /* bit t = follow track t; bit 8 = panel keys and MIDI in */
    volatile uint32_t octave_chance;   /* Q16 */
    volatile uint32_t gate;            /* Q16 fraction of a step */
    volatile int32_t level;            /* Q15 */
};
extern struct bass_shared bass;

/* listeners: audio context / control task / MIDI routing task */
FM1_AUDIO_CODE void bass_hear_sequence(unsigned track, unsigned note, unsigned velocity);
void bass_hear_local(unsigned note, unsigned velocity);
void bass_hear_external(unsigned note, unsigned velocity);
/* audio ISR */
FM1_AUDIO_CODE void bass_transport(uint8_t status);
FM1_AUDIO_CODE void bass_tick(void);
/* readback for the panel */
extern volatile uint8_t bass_root, bass_quality, bass_running, bass_latched_n, bass_rest_pos;
extern volatile uint8_t bass_latched[8], bass_history[BASS_HISTORY], bass_history_pos;
extern volatile uint16_t bass_heard;   /* sources that started a note since the panel cleared it */
enum { BASS_LISTEN_KEYS = 8, BASS_LISTEN_ALL = 0x1ff };

int bass_control(const struct fm1_controls *controls);
unsigned bass_active(void);
#endif
