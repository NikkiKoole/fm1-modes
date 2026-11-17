/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* CR-78 audio: the step sequencer. Voices are drumvoice.c's. Integer only, no
 * divisions, no calls outside the app's audio code: safe in the audio ISR. */
#include "cr78.h"
#include "fx.h"
#include "gbx.h"
#include "rhythm.h"
#include "bass.h"

#define BARRIER() __asm__ volatile("" ::: "memory")
enum { EVENTS = 96, AUDITIONS = 16 };
enum { PERIOD = FM1_SAMPLE_RATE * 60U };

struct cr78_shared cr78;
volatile uint8_t cr78_playhead, cr78_running;
volatile uint16_t cr78_hits;
volatile uint8_t fm1_transport_playing;
volatile uint16_t fm1_transport_tempo = 120;

static struct { uint32_t at; uint8_t voice; int8_t boost; } events[EVENTS];
static unsigned event_count;
static struct { uint8_t voice, boost; } auditions[AUDITIONS];
static volatile uint32_t audition_head, audition_tail;
static uint32_t now, phase, step;
static unsigned first = 1, playing;   /* first = 1: the FM-1 clock boots that way too */

int cr78_audition(unsigned voice, unsigned boost)
{
    uint32_t head = audition_head, next = (head + 1U) % AUDITIONS;
    if (voice >= CR78_VOICES || next == audition_tail) return -1;
    auditions[head].voice = (uint8_t)voice; auditions[head].boost = (uint8_t)boost;
    BARRIER(); audition_head = next;
    return 0;
}

/* ---- engine callbacks (audio context, inside fm1_engine_render) ---- */
FM1_AUDIO_CODE void cr78_transport(uint8_t status)
{
    if (status == 0xfa || status == 0xff) {
        phase = 0; step = 0; first = 1; playing = status == 0xfa;
    } else if (status == 0xfb) {
        playing = 1;
    } else if (status == 0xfc) {
        playing = 0;
    }
    if (!playing) event_count = 0;   /* drop queued ratchet tails on stop */
    fm1_transport_playing = (uint8_t)playing;
    rhythm_transport(status);
    bass_transport(status);
}
FM1_AUDIO_CODE void cr78_clock(unsigned bpm)
{
    if (bpm >= 30 && bpm <= 300) fm1_transport_tempo = (uint16_t)bpm;
}

static FM1_AUDIO_CODE void trigger(unsigned voice, int boost, uint32_t delay)
{
    dv_trigger(&cr78.voice[voice], boost, delay, cr78.level, 0, -1, FX_CR78);
    if (voice == 0) fx_kick(FX_KEY_CR78, delay);   /* KICK keys the duck */
    cr78_hits |= (uint16_t)(1U << voice);
}
static FM1_AUDIO_CODE void push(uint32_t at, unsigned voice, int boost)
{
    if (event_count < EVENTS) {
        events[event_count].at = at; events[event_count].voice = (uint8_t)voice;
        events[event_count].boost = (int8_t)boost; ++event_count;
    }
}
/* force_r > 0 replaces every hit's ratchet (a roll); fade lowers the volume (a roll dying away) */
static FM1_AUDIO_CODE void schedule_step(unsigned s, unsigned tempo, unsigned force_r, int fade)
{
    uint32_t len = dv.step_samples[tempo];                      /* Q8 */
    uint32_t delay = (s & 3U) == 2U ? (uint32_t)(((uint64_t)len * (uint32_t)cr78.swing_q16) >> 24) : 0;
    unsigned accent = (cr78.accent >> s) & 1U;
    for (unsigned v = 0; v < CR78_VOICES; ++v) {
        if (!((cr78.grid[v] >> s) & 1U) || ((cr78.voice_mute >> v) & 1U)) continue;
        unsigned r = force_r ? force_r : cr78.ratchet[v][s];
        if (r < 1) r = 1;
        if (r > CR78_RATCHET_MAX) r = CR78_RATCHET_MAX;
        for (unsigned k = 0; k < r; ++k) {
            uint32_t offset = (uint32_t)(((uint64_t)len * k * dv.reciprocal[r]) >> 24);
            /* a roll fades: about three volume steps over its length */
            int boost = (accent ? (int)cr78.accent_boost[v] : 0) - (int)((k * 3U * dv.reciprocal[r]) >> 16) - fade;
            push(now + delay + offset, v, boost);
        }
    }
}

static uint32_t steps;   /* every clock step since boot: the pads' sense of time */
static unsigned last_s, last_hit_s, origin_s, prev_mode, rolling, roll_start, roll_count;
static uint32_t origin_step;
static FM1_AUDIO_CODE uint32_t clamp_u(uint32_t v, uint32_t hi) { return v > hi ? hi : v; }
/* Which pattern step plays on this clock step. Plain: the FM-1's own step. ROLL: repeat the 1, 2 or
 * 4 steps that were playing when it began. HALF and REV run from the step they began on, and on
 * release the pattern lands back where it would have been (slip, as a DJ's loop roll).
 * Returns 16 for a half-time clock step that plays nothing. */
static FM1_AUDIO_CODE unsigned pattern_step(void)
{
    unsigned perf = cr78.perf;
    if (perf & (CR78_PERF_ROLL16 | CR78_PERF_ROLL32)) {
        if (!rolling) { rolling = 1; roll_count = 0; roll_start = last_hit_s; }   /* the last step that played something */
        unsigned len = cr78.roll_len == 4 ? 4U : cr78.roll_len == 2 ? 2U : 1U;
        return (roll_start - (len - 1U) + (roll_count & (len - 1U))) & 15U;
    }
    rolling = 0;
    unsigned mode = perf & (CR78_PERF_HALF | CR78_PERF_REV);
    if (mode != prev_mode) { prev_mode = mode; origin_s = last_s; origin_step = steps; }
    if (!mode) return step & 15U;
    uint32_t d = steps - origin_step;
    if ((mode & CR78_PERF_HALF) && (d & 1U)) return 16U;
    uint32_t k = ((mode & CR78_PERF_HALF) ? d >> 1 : d) + 1U;
    return (mode & CR78_PERF_REV) ? (origin_s - k) & 15U : (origin_s + k) & 15U;
}

FM1_AUDIO_CODE void cr78_tick(void)
{
    unsigned tempo = fm1_transport_tempo;
    cr78_running = (uint8_t)playing;
    fx_step_rate = (tempo * 6493U) >> 16;   /* Q16 steps a sample: tempo * 4 / (44100 * 60) */
    while (audition_tail != audition_head) {
        uint32_t tail = audition_tail; BARRIER();
        trigger(auditions[tail].voice, auditions[tail].boost, 0);
        BARRIER(); audition_tail = (tail + 1U) % AUDITIONS;
    }
    if (playing) {   /* the FM-1 clock, verbatim: one check per block, same period and swing */
        uint32_t period = PERIOD / 100U * (uint32_t)(100 + ((step & 1U) ? -(int)cr78.fm1_swing : (int)cr78.fm1_swing));
        if (first || phase >= period) {
            if (first) first = 0;
            else { phase -= period; step = (step + 1U) & 63U; ++steps; }
            gbx_clock(step & 15U, tempo);   /* the GROOVEBOX's melodic tracks, on the same clock */
            unsigned s = pattern_step(), fade = 0, force = 0;
            if (rolling) {
                force = (cr78.perf & CR78_PERF_ROLL32) ? 2U : 1U;
                /* FADE: one volume step every 4/fade repeats, down to a floor: a held roll dies down, never out */
                fade = (roll_count * cr78.roll_fade) >> 2; if (fade > 2U) fade = 2U;   /* a quiet hat (volume 3) stays audible */
                ++roll_count;
            }
            if (s < 16U) {
                cr78_playhead = (uint8_t)s; last_s = s;
                if (!rolling) for (unsigned v = 0; v < CR78_VOICES; ++v) if ((cr78.grid[v] >> s) & 1U) { last_hit_s = s; break; }
                if (cr78.armed && !cr78.muted && dv.ready) schedule_step(s, tempo, force, (int)fade);
            }
        }
        /* the step clock the PERFORM gate locks to: whole steps, and the fraction of this one */
        fx_step_clock = (steps << 16) | (uint32_t)clamp_u((uint32_t)(((uint64_t)phase * 1623U) >> 16), 65535U);
        phase += tempo * 4U * FM1_BLOCK_FRAMES;
    }
    for (unsigned i = 0; i < event_count;) {
        if ((int32_t)(events[i].at - (now + FM1_BLOCK_FRAMES)) < 0) {
            uint32_t at = events[i].at;
            trigger(events[i].voice, events[i].boost, (int32_t)(at - now) > 0 ? at - now : 0);
            --event_count;
            events[i].at = events[event_count].at; events[i].voice = events[event_count].voice;
            events[i].boost = events[event_count].boost;
        } else ++i;
    }
    now += FM1_BLOCK_FRAMES;
}
