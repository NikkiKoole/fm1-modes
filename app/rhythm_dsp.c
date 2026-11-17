/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* RHYTHM BOX audio: the count clock. Integer only, no division, no calls
 * outside the app's audio code. */
#include "rhythm.h"
#include "cr78.h"   /* fm1_transport_tempo */

enum { PERIOD = FM1_SAMPLE_RATE * 60U };

struct rhythm_shared rhythm;
volatile uint8_t rhythm_count, rhythm_running;
volatile uint16_t rhythm_hits;

static uint32_t phase;
static unsigned position, first = 1, playing;

FM1_AUDIO_CODE void rhythm_transport(uint8_t status)
{
    if (status == 0xfa || status == 0xff) {
        phase = 0; position = 0; first = 1; playing = status == 0xfa;
    } else if (status == 0xfb) {
        playing = 1;
    } else if (status == 0xfc) {
        playing = 0;
    }
}

static FM1_AUDIO_CODE void fire(const struct rhythm_pattern *p, unsigned count, unsigned tempo)
{
    /* one tick in samples (Q8): a sixteenth * 4 / per_beat */
    uint32_t tick = (uint32_t)(((uint64_t)dv.step_samples[tempo] * 4U * dv.reciprocal[p->per_beat]) >> 16);
    const int32_t level = rhythm.level;
    const struct dv_voice *kit = p->kit == RHYTHM_KIT_TR77 ? rhythm.tr77 : rhythm.sideman;
    unsigned kit_size = p->kit == RHYTHM_KIT_TR77 ? RHYTHM_TR77_VOICES : RHYTHM_SIDEMAN_VOICES;
    for (unsigned l = 0; l < p->lanes; ++l) {
        const struct rhythm_lane *lane = &p->lane[l];
        if (!((lane->fire >> count) & 1U)) continue;
        if (lane->role & RHYTHM_ROLE_CHORD)
            for (unsigned k = 0; k < 3; ++k) dv_trigger(&rhythm.organ, 0, 0, level, 0, rhythm.chord[k]);
        if (lane->role & RHYTHM_ROLE_BASSLO) dv_trigger(&rhythm.bass, 0, 0, level, 0, rhythm.bass_note[0]);
        if (lane->role & RHYTHM_ROLE_BASSHI) dv_trigger(&rhythm.bass, 0, 0, level, 0, rhythm.bass_note[1]);
        if (lane->role & RHYTHM_ROLE_DRUM) {
            uint32_t gate = lane->hold[count] ? (uint32_t)(((uint64_t)tick * lane->hold[count]) >> 9) : 0;
            for (unsigned v = 0; v < lane->voices; ++v)
                if (lane->voice[v] < kit_size) dv_trigger(&kit[lane->voice[v]], 0, 0, level, gate, -1);
        }
        rhythm_hits |= (uint16_t)(1U << l);
    }
}

FM1_AUDIO_CODE void rhythm_tick(void)
{
    rhythm_running = (uint8_t)playing;
    if (!playing) return;
    const struct rhythm_pattern *p = &rhythm.pattern[rhythm.live & 1U];
    unsigned tempo = fm1_transport_tempo, per_beat = p->per_beat;
    if (!per_beat || per_beat > 16 || !p->used) return;
    if (first || phase >= PERIOD) {
        if (first) first = 0;
        else { phase -= PERIOD; ++position; }
        if (position >= p->used) position = 0;     /* also catches a shorter rhythm loaded mid-bar */
        unsigned count = p->order[position];
        rhythm_count = (uint8_t)count;
        if (rhythm.armed && !rhythm.muted && dv.ready) fire(p, count, tempo);
    }
    phase += tempo * per_beat * FM1_BLOCK_FRAMES;
}
