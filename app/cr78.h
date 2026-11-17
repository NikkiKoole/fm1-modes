/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* CR-78: the Roland CR-78 voices (ported from dreamengine's cr78 cart, every
 * voice tuned there against a real CR-78 sample set) + a 16-step sequencer
 * with accents, swing and per-step ratchets. The voices are played by the
 * shared drum engine (drumvoice.h).
 *
 * Audio side (cr78_dsp.c, audio ISR, integer only): a step clock that copies
 * the FM-1 sequencer's own arithmetic (one step when a block phase passes
 * 44100*60, advanced by tempo*4 per sample, checked once per block).
 * Transport and tempo arrive through the engine's own callbacks in
 * midi_router.c on the very block the FM-1 tracks see them, so both stay in
 * lock with no drift. Ratchet sub-hits and swing are sample-accurate.
 *
 * Control side (cr78.c) owns the panel while the mode is on (from the launcher) and
 * prepares every coefficient; leaving the mode keeps the pattern playing. */
#ifndef FM1_CR78_H
#define FM1_CR78_H
#include "fm1.h"
#include "drumvoice.h"

enum { CR78_VOICES = 16, CR78_STEPS = 16, CR78_RATCHET_MAX = 8 };

struct cr78_shared {
    struct dv_voice voice[CR78_VOICES];
    volatile uint16_t grid[CR78_VOICES];   /* bit s = step s */
    volatile uint16_t accent;
    volatile uint8_t ratchet[CR78_VOICES][CR78_STEPS];   /* 1..8 */
    volatile int32_t swing_q16;            /* offbeat delay as a fraction of a step */
    volatile int32_t level;                /* Q15 */
    volatile uint8_t armed, muted, fm1_swing;
    /* the performance pads that bend the pattern (CR78_PERF_*), and the roll's settings */
    volatile uint8_t perf, roll_len, roll_fade;   /* roll_len 1/2/4 steps; roll_fade volume steps per repeat */
    volatile uint8_t accent_boost[CR78_VOICES];
    volatile uint16_t voice_mute;   /* bit v: voice v is muted (the MIX screen's black keys) */
    volatile uint16_t skip[CR78_VOICES];   /* bit s: voice v's hit on step s was just played live: skip it once */
};
enum { CR78_PERF_ROLL16 = 1, CR78_PERF_ROLL32 = 2, CR78_PERF_HALF = 4, CR78_PERF_REV = 8 };
extern struct cr78_shared cr78;

/* Audio ISR: engine callbacks, and one sequencer tick per block. */
FM1_AUDIO_CODE void cr78_transport(uint8_t status);
FM1_AUDIO_CODE void cr78_clock(unsigned tempo);
FM1_AUDIO_CODE void cr78_tick(void);
/* Shared by the drum modes: the FM-1 transport and tempo as last reported. */
extern volatile uint8_t fm1_transport_playing;
extern volatile uint16_t fm1_transport_tempo;
/* Readback for the panel (written by audio). */
extern volatile uint8_t cr78_playhead, cr78_running;
extern volatile uint16_t cr78_hits;   /* voices triggered since the panel last cleared it */

/* Control task. */
int cr78_audition(unsigned voice, unsigned boost);
int cr78_control(const struct fm1_controls *controls);
unsigned cr78_active(void);
/* the kit, for other panels (GROOVEBOX's DRUMS track) */
struct px_page;
void cr78_kit_ready(void);
unsigned cr78_black_voice(unsigned i);                 /* black key i (0..10) -> voice */
const char *const *cr78_voice_row(void);               /* the eleven black keys' labels */
const char *cr78_voice_name(unsigned v);
const char *cr78_voice_short(unsigned v);
unsigned cr78_voice_color(unsigned v);
const struct px_page *cr78_voice_pages(unsigned *count);   /* SOUND PITCH BODY MOD LEVEL */
const char *cr78_voice_knob(unsigned page, unsigned k);
void cr78_voice_text(unsigned v, unsigned page, unsigned k, char *out);
void cr78_voice_turn(unsigned v, unsigned page, unsigned k, int turn);   /* rebuilds the voice */
unsigned cr78_preset_count(void);
unsigned cr78_preset_index(void);
const char *cr78_preset_name(unsigned p);
void cr78_preset(unsigned p);
void cr78_set_ratchet(unsigned v, unsigned s, int r);
void cr78_draw_kit(unsigned v, uint32_t now);
/* the kit's sound settings, for saving (GROOVEBOX) */
unsigned cr78_kit_bytes(void);
void cr78_kit_get(void *out);
void cr78_kit_set(const void *in);   /* rebuilds every voice */
#endif
