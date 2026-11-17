/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX audio: the BASS track's step sequencer, clocked by cr78_tick's own step clock (so it is
 * locked to the FM-1 and the drums), played on the drum engine into the BASS bus. Integer only,
 * no division, no calls outside the app's audio code. */
#include "gbx.h"
#include "cr78.h"
#include "fx.h"

#define BARRIER() __asm__ volatile("" ::: "memory")
enum { AUDITIONS = 8 };

struct gbx_bass gbx_bass;
static struct { uint8_t note; } auditions[AUDITIONS];
static volatile uint32_t audition_head, audition_tail;
static unsigned prev_note = 255;   /* what the last step played (255: a rest), for slides */

int gbx_audition(unsigned note)
{
    uint32_t head = audition_head, next = (head + 1U) % AUDITIONS;
    if (note > 127U || next == audition_tail) return -1;
    auditions[head].note = (uint8_t)note;
    BARRIER(); audition_head = next;
    return 0;
}

FM1_AUDIO_CODE void gbx_tick(void)
{
    while (audition_tail != audition_head) {
        uint32_t tail = audition_tail; BARRIER();
        dv_trigger(&gbx_bass.voice, 0, 0, gbx_bass.level, (uint32_t)dv.step_samples[120] >> 8, auditions[tail].note, FX_BASS);
        BARRIER(); audition_tail = (tail + 1U) % AUDITIONS;
    }
}

FM1_AUDIO_CODE void gbx_clock(unsigned s, unsigned tempo)
{
    struct gbx_bass *b = &gbx_bass;
    if (!b->armed || !dv.ready || tempo > 300U) return;
    if (!((b->on >> s) & 1U) || b->muted) { prev_note = 255; return; }
    uint32_t len = dv.step_samples[tempo];   /* Q8 samples a sixteenth */
    uint32_t delay = (s & 3U) == 2U ? (uint32_t)(((uint64_t)len * (uint32_t)cr78.swing_q16) >> 24) : 0;
    unsigned next = (s + 1U) & 15U, note = b->note[s];
    /* a slide on the next step keeps this one sounding into it */
    uint32_t gate = ((b->slide >> next) & 1U) && ((b->on >> next) & 1U) ? (len * 18U) >> 12 : (len * b->gate[s]) >> 12;
    int glide = ((b->slide >> s) & 1U) && prev_note < 128U ? (int)prev_note - (int)note : 0;
    int32_t level = (int32_t)(((int64_t)b->level * b->vel[s]) >> 7);
    dv_trigger_glide(&b->voice, (b->accent >> s) & 1U ? 2 : 0, delay, level, gate ? gate : 1, (int)note, FX_BASS, glide);
    prev_note = note;
}
