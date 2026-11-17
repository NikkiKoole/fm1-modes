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

/* ---- patterns ---- */
struct gbx_pattern gbx_patterns[GBX_PATTERNS];
volatile uint8_t gbx_pattern_now, gbx_pattern_next = GBX_NONE;
volatile uint16_t gbx_patterns_used = 1;
volatile uint8_t gbx_chain[GBX_CHAIN_MAX], gbx_chain_len = 1, gbx_chain_pos;
static unsigned restarted = 1;
struct gbx_pattern gbx_deck[GBX_DECK_BARS];
volatile uint8_t gbx_deck_on, gbx_deck_bar, gbx_deck_dirty;   /* the transport just started: the first bar plays the chain's first slot */

void gbx_pattern_queue(unsigned p) { if (p < GBX_PATTERNS) gbx_pattern_next = (uint8_t)p; }
void gbx_chain_set(const uint8_t *slots, unsigned n)
{
    if (!n) return;
    if (n > GBX_CHAIN_MAX) n = GBX_CHAIN_MAX;
    gbx_chain_len = 1; BARRIER();   /* (the audio side does not move along a chain while it is written) */
    for (unsigned i = 0; i < n; ++i) gbx_chain[i] = slots[i] < GBX_PATTERNS ? slots[i] : 0;
    gbx_chain_pos = 0; BARRIER(); gbx_chain_len = (uint8_t)n;
    if (slots[0] != gbx_pattern_now || gbx_pattern_next != GBX_NONE) gbx_pattern_queue(slots[0]);
}

/* one step of one track, between the tracks (the playing pattern) and a pattern record.
 * (word and byte moves through volatile: no memcpy into Flash) */
static FM1_AUDIO_CODE void step_take(struct gbx_pattern *p, unsigned t, unsigned s)
{
    uint16_t bit = (uint16_t)(1U << s);
    if (t == 0) {
        for (unsigned v = 0; v < GBX_DRUM_VOICES; ++v) {
            p->grid[v] = (uint16_t)((p->grid[v] & ~bit) | (cr78.grid[v] & bit));
            unsigned r = (cr78.ratchet[v][s] - 1U) & 15U, sh = (s & 1U) * 4U;
            p->ratchet[v][s >> 1] = (uint8_t)((p->ratchet[v][s >> 1] & ~(15U << sh)) | (r << sh));
        }
        p->accent = (uint16_t)((p->accent & ~bit) | (cr78.accent & bit));
    } else if (t == 1 || t == 3) {
        const struct gbx_line *l = t == 1 ? &gbx_bass : &gbx_lead;
        struct gbx_line_steps *d = t == 1 ? &p->bass : &p->lead;
        d->on = (uint16_t)((d->on & ~bit) | (l->on & bit)); d->slide = (uint16_t)((d->slide & ~bit) | (l->slide & bit));
        d->accent = (uint16_t)((d->accent & ~bit) | (l->accent & bit));
        d->note[s] = l->note[s]; d->gate[s] = l->gate[s]; d->vel[s] = l->vel[s];
    } else {
        p->keys_on = (uint16_t)((p->keys_on & ~bit) | (gbx_keys.on & bit));
        p->keys_accent = (uint16_t)((p->keys_accent & ~bit) | (gbx_keys.accent & bit));
        for (unsigned k = 0; k < GBX_CHORD; ++k) p->keys_note[s][k] = gbx_keys.note[s][k];
        p->keys_gate[s] = gbx_keys.gate[s]; p->keys_vel[s] = gbx_keys.vel[s];
    }
}
static FM1_AUDIO_CODE void step_put(const struct gbx_pattern *p, unsigned t, unsigned s)
{
    uint16_t bit = (uint16_t)(1U << s);
    if (t == 0) {
        for (unsigned v = 0; v < GBX_DRUM_VOICES; ++v) {
            cr78.ratchet[v][s] = (uint8_t)(((p->ratchet[v][s >> 1] >> ((s & 1U) * 4U)) & 15U) + 1U);
            cr78.grid[v] = (uint16_t)((cr78.grid[v] & ~bit) | (p->grid[v] & bit));
        }
        cr78.accent = (uint16_t)((cr78.accent & ~bit) | (p->accent & bit));
    } else if (t == 1 || t == 3) {
        struct gbx_line *l = t == 1 ? &gbx_bass : &gbx_lead;
        const struct gbx_line_steps *d = t == 1 ? &p->bass : &p->lead;
        l->note[s] = d->note[s]; l->gate[s] = d->gate[s] ? d->gate[s] : 1; l->vel[s] = d->vel[s] ? d->vel[s] : 100;
        l->slide = (uint16_t)((l->slide & ~bit) | (d->slide & bit)); l->accent = (uint16_t)((l->accent & ~bit) | (d->accent & bit));
        l->on = (uint16_t)((l->on & ~bit) | (d->on & bit));
    } else {
        for (unsigned k = 0; k < GBX_CHORD; ++k) gbx_keys.note[s][k] = p->keys_note[s][k];
        gbx_keys.gate[s] = p->keys_gate[s] ? p->keys_gate[s] : 1; gbx_keys.vel[s] = p->keys_vel[s] ? p->keys_vel[s] : 90;
        gbx_keys.accent = (uint16_t)((gbx_keys.accent & ~bit) | (p->keys_accent & bit));
        gbx_keys.on = (uint16_t)((gbx_keys.on & ~bit) | (p->keys_on & bit));
    }
}
static FM1_AUDIO_CODE void pattern_save(struct gbx_pattern *p)
{
    for (unsigned t = 0; t < 4; ++t) for (unsigned s = 0; s < GBX_STEPS; ++s) step_take(p, t, s);
}
static FM1_AUDIO_CODE void pattern_load(const struct gbx_pattern *p)
{
    for (unsigned t = 0; t < 4; ++t) for (unsigned s = 0; s < GBX_STEPS; ++s) step_put(p, t, s);
}
static FM1_AUDIO_CODE void skips_clear(void)
{
    for (unsigned v = 0; v < GBX_DRUM_VOICES; ++v) cr78.skip[v] = 0;
    gbx_bass.skip = gbx_lead.skip = gbx_keys.skip = 0;
}
static FM1_AUDIO_CODE void pattern_swap(void)
{
    unsigned next = gbx_pattern_next, now_p = gbx_pattern_now;
    if (next >= GBX_PATTERNS) return;
    pattern_save(&gbx_patterns[now_p]);
    if ((gbx_patterns_used >> next) & 1U) pattern_load(&gbx_patterns[next]);   /* a slot never used: the playing one carries over */
    gbx_patterns_used = (uint16_t)(gbx_patterns_used | (1U << now_p) | (1U << next));
    gbx_pattern_now = (uint8_t)next; gbx_pattern_next = GBX_NONE;
    skips_clear();
}

/* the control task's side: the playing pattern as a record, and back (whole, a track, or a step) */
void gbx_pattern_take(struct gbx_pattern *p) { pattern_save(p); }
void gbx_pattern_put(const struct gbx_pattern *p, unsigned what)
{
    unsigned step = what / GBX_STEP_BIT, t = what & 15U;
    for (unsigned tt = 0; tt < 4; ++tt) {
        if (t != GBX_ALL && tt != t) continue;
        for (unsigned s = 0; s < GBX_STEPS; ++s) if (!step || s == step - 1U) step_put(p, tt, s);
    }
}
void gbx_pattern_wipe(struct gbx_pattern *p, unsigned what)
{
    unsigned step = what / GBX_STEP_BIT, t = what & 15U;
    uint16_t bits = step ? (uint16_t)(1U << (step - 1U)) : 0xFFFFU;
    if (t == GBX_ALL || t == 0) {
        for (unsigned v = 0; v < GBX_DRUM_VOICES; ++v) {
            p->grid[v] &= (uint16_t)~bits;
            for (unsigned s = 0; s < GBX_STEPS; ++s) if ((bits >> s) & 1U) p->ratchet[v][s >> 1] &= (uint8_t)~(15U << ((s & 1U) * 4U));
        }
        p->accent &= (uint16_t)~bits;
    }
    if (t == GBX_ALL || t == 1) { p->bass.on &= (uint16_t)~bits; p->bass.slide &= (uint16_t)~bits; p->bass.accent &= (uint16_t)~bits; }
    if (t == GBX_ALL || t == 3) { p->lead.on &= (uint16_t)~bits; p->lead.slide &= (uint16_t)~bits; p->lead.accent &= (uint16_t)~bits; }
    if (t == GBX_ALL || t == 2) { p->keys_on &= (uint16_t)~bits; p->keys_accent &= (uint16_t)~bits; }
}

FM1_AUDIO_CODE void gbx_tick(void)
{
    if (!cr78_running) {   /* stopped: at once */
        restarted = 1;
        if (!gbx_deck_on) pattern_swap();
        else if (gbx_deck_dirty) { gbx_deck_dirty = 0; gbx_deck_bar = 0; pattern_load(&gbx_deck[0]); }
    }
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
    if ((k->skip >> s) & 1U) { k->skip = (uint16_t)(k->skip & ~(1U << s)); return; }   /* played live just now */
    uint32_t gate = (len * k->gate[s]) >> 12;
    unsigned vel = k->vel[s] + ((k->accent >> s) & 1U ? 30U : 0U);
    play_chord(k->note[s], vel > 127U ? 127U : vel, now + delay, gate ? gate : 1U);
}

/* LEAD: a slide on a step glides into it from the step before (that one held over, no new attack) */
static FM1_AUDIO_CODE void lead_clock(unsigned s, uint32_t len, uint32_t delay)
{
    struct gbx_line *l = &gbx_lead;
    if (!l->armed || l->muted || !((l->on >> s) & 1U)) return;
    if ((l->skip >> s) & 1U) { l->skip = (uint16_t)(l->skip & ~(1U << s)); return; }
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
    if (!s && gbx_deck_on) {   /* LOOP DECK: the next of its bars, as it is now */
        unsigned bar = restarted ? 0U : (gbx_deck_bar + 1U) & (GBX_DECK_BARS - 1U);
        restarted = 0; gbx_deck_dirty = 0; gbx_deck_bar = (uint8_t)bar;
        pattern_load(&gbx_deck[bar]);
    } else if (!s) {   /* on the bar, before any track reads this step: a queued pattern, or the chain's next */
        if (gbx_pattern_next == GBX_NONE && gbx_chain_len > 1 && !restarted) {
            unsigned pos = gbx_chain_pos + 1U;
            if (pos >= gbx_chain_len) pos = 0;
            gbx_chain_pos = (uint8_t)pos; gbx_pattern_next = gbx_chain[pos];
        }
        restarted = 0;
        pattern_swap();
    }
    if (!dv.ready || tempo > 300U) return;
    {   /* KEYS, LEAD */
        uint32_t len = dv.step_samples[tempo];
        uint32_t delay = (s & 3U) == 2U ? (uint32_t)(((uint64_t)len * (uint32_t)cr78.swing_q16) >> 24) : 0;
        keys_clock(s, len, delay);   /* (delay is already in samples) */
        lead_clock(s, len, delay);
    }
    if (!b->armed) return;
    if (!((b->on >> s) & 1U) || b->muted) { prev_note = 255; return; }
    if ((b->skip >> s) & 1U) { b->skip = (uint16_t)(b->skip & ~(1U << s)); prev_note = b->note[s]; return; }
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
