/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX audio: the BASS, KEYS and LEAD step sequencers, clocked by cr78_tick's own step clock (so
 * they are locked to the FM-1 and the drums). BASS plays the drum engine into the BASS bus; KEYS and
 * LEAD play MY SYNTH's engine (KEYS its six voices, LEAD its own mono voice) through timed events.
 * Integer only, no division, no calls outside the app's audio code. */
#include "gbx.h"
#include "cr78.h"
#include "fx.h"
#include "mysynth.h"

#define BARRIER() __asm__ volatile("" ::: "memory")
enum { AUDITIONS = 8 };

struct gbx_line gbx_bass, gbx_lead;
struct gbx_keys gbx_keys;

/* the KEYS and LEAD tracks' timed note events (swing, strum, note-offs), checked every block */
enum { EVENTS = 48, TO_KEYS = 0, TO_LEAD = 1, SLIDE = 2 };
static struct { uint32_t at; uint8_t note, vel, on, to; } events[EVENTS];   /* to: TO_KEYS / TO_LEAD, | SLIDE */
static unsigned event_count;
static uint32_t now;   /* samples, at the start of this block */
static struct { uint8_t note[GBX_CHORD]; uint8_t to; uint32_t gate; } chord_auditions[4];
static volatile uint32_t chord_head, chord_tail;

static FM1_AUDIO_CODE void push(uint32_t at, unsigned note, unsigned vel, unsigned on, unsigned to)
{
    if (on)   /* a note starting again: the pending note-off of its last time would cut it short */
        for (unsigned i = 0; i < event_count; ++i)
            if (!events[i].on && events[i].note == note && (events[i].to & 1U) == (to & 1U) && (int32_t)(events[i].at - at) >= 0) {
                --event_count; events[i] = events[event_count]; --i;
            }
    if (event_count >= EVENTS) return;
    events[event_count].at = at; events[event_count].note = (uint8_t)note; events[event_count].to = (uint8_t)to;
    events[event_count].vel = (uint8_t)vel; events[event_count].on = (uint8_t)on; ++event_count;
}
static FM1_AUDIO_CODE void play_chord(const volatile uint8_t *notes, unsigned vel, uint32_t at, uint32_t gate)
{
    uint32_t strum = gbx_keys.strum;
    for (unsigned k = 0; k < GBX_CHORD; ++k) {
        unsigned n = notes[k];
        if (!n) continue;
        push(at + k * strum, n, vel, 1, TO_KEYS);
        push(at + k * strum + gate, n, 0, 0, TO_KEYS);
    }
}
static int audition_push(const uint8_t notes[GBX_CHORD], unsigned to, uint32_t gate)
{
    uint32_t head = chord_head, next = (head + 1U) % 4U;
    if (next == chord_tail) return -1;
    for (unsigned k = 0; k < GBX_CHORD; ++k) chord_auditions[head].note[k] = notes[k];
    chord_auditions[head].to = (uint8_t)to; chord_auditions[head].gate = gate;
    BARRIER(); chord_head = next;
    return 0;
}
int gbx_chord_audition(const uint8_t notes[GBX_CHORD], uint32_t gate) { return audition_push(notes, TO_KEYS, gate); }
int gbx_lead_audition(unsigned note, uint32_t gate)
{
    const uint8_t n[GBX_CHORD] = {(uint8_t)note, 0, 0, 0};
    return note > 127U ? -1 : audition_push(n, TO_LEAD, gate);
}
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
    while (chord_tail != chord_head) {
        uint32_t tail = chord_tail; BARRIER();
        if (chord_auditions[tail].to == TO_LEAD) {
            push(now, chord_auditions[tail].note[0], 100, 1, TO_LEAD);
            push(now + chord_auditions[tail].gate, chord_auditions[tail].note[0], 0, 0, TO_LEAD);
        } else play_chord(chord_auditions[tail].note, 100, now, chord_auditions[tail].gate);
        BARRIER(); chord_tail = (tail + 1U) % 4U;
    }
    /* the events due in this block (block-accurate: 2.9 ms) */
    for (unsigned i = 0; i < event_count;) {
        if ((int32_t)(events[i].at - (now + FM1_BLOCK_FRAMES)) < 0) {
            if (events[i].to & TO_LEAD) mysynth_lead_play(events[i].note, events[i].vel, events[i].on, events[i].to & SLIDE);
            else mysynth_play(events[i].note, events[i].vel, events[i].on);
            --event_count; events[i] = events[event_count];
        } else ++i;
    }
    now += FM1_BLOCK_FRAMES;
    while (audition_tail != audition_head) {
        uint32_t tail = audition_tail; BARRIER();
        dv_trigger(&gbx_bass.voice, 0, 0, gbx_bass.level, (uint32_t)dv.step_samples[120] >> 8, auditions[tail].note, FX_BASS);
        BARRIER(); audition_tail = (tail + 1U) % AUDITIONS;
    }
}

static FM1_AUDIO_CODE void keys_clock(unsigned s, uint32_t len, uint32_t delay)
{
    struct gbx_keys *k = &gbx_keys;
    if (!k->armed || k->muted || !((k->on >> s) & 1U)) return;
    uint32_t gate = (len * k->gate[s]) >> 12;
    unsigned vel = k->vel[s] + ((k->accent >> s) & 1U ? 30U : 0U);
    play_chord(k->note[s], vel > 127U ? 127U : vel, now + delay, gate ? gate : 1U);
}

/* LEAD: a slide on a step glides into it from the step before (that one held over, no new attack) */
static FM1_AUDIO_CODE void lead_clock(unsigned s, uint32_t len, uint32_t delay)
{
    struct gbx_line *l = &gbx_lead;
    if (!l->armed || l->muted || !((l->on >> s) & 1U)) return;
    unsigned next = (s + 1U) & 15U;
    uint32_t gate = (len * l->gate[s]) >> 12;
    if (((l->slide >> next) & 1U) && ((l->on >> next) & 1U) && gate < ((len * 18U) >> 12)) gate = (len * 18U) >> 12;
    unsigned vel = l->vel[s] + ((l->accent >> s) & 1U ? 30U : 0U);
    unsigned to = TO_LEAD | ((l->slide >> s) & 1U ? SLIDE : 0U);
    push(now + delay, l->note[s], vel > 127U ? 127U : vel, 1, to);
    push(now + delay + (gate ? gate : 1U), l->note[s], 0, 0, TO_LEAD);
}

FM1_AUDIO_CODE void gbx_clock(unsigned s, unsigned tempo)
{
    struct gbx_line *b = &gbx_bass;
    if (!dv.ready || tempo > 300U) return;
    {   /* KEYS, LEAD */
        uint32_t len = dv.step_samples[tempo];
        uint32_t delay = (s & 3U) == 2U ? (uint32_t)(((uint64_t)len * (uint32_t)cr78.swing_q16) >> 24) : 0;
        keys_clock(s, len, delay);   /* (delay is already in samples) */
        lead_clock(s, len, delay);
    }
    if (!b->armed) return;
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
