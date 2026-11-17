/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* BASS PLAYER audio: hearing, chord naming, patgen on the FM-1 beat.
 * Integer only, no division, no library calls. */
#include "bass.h"
#include "patgen.h"
#include "instrument.h"
#include "cr78.h"   /* fm1_transport_tempo */

enum { PERIOD = FM1_SAMPLE_RATE * 60U };

struct bass_shared bass;
volatile uint8_t bass_root = BASS_NONE, bass_quality, bass_running, bass_latched_n, bass_rest_pos;
volatile uint8_t bass_latched[8], bass_history[BASS_HISTORY], bass_history_pos;

/* what is sounding, one owner per context */
static uint32_t sequence_bits[GROOVE_TRACKS][4];   /* audio */
static volatile uint8_t local_count[128];           /* control task */
static volatile uint8_t external_count[128];        /* MIDI routing task */

static PatGen pg = {.seed = 0x5eed1u};
/* notes that STARTED since the last chord decision (short sequencer gates fall
 * between ticks), one owner per context: pitch-class mask and lowest note */
static uint16_t onset_mask_audio; static uint8_t onset_low_audio = BASS_NONE;
static volatile uint16_t onset_mask_local, onset_mask_external;
static volatile uint8_t onset_low_local = BASS_NONE, onset_low_external = BASS_NONE;
static unsigned window_mask, window_low = BASS_NONE, window_steps;
static uint32_t phase;
static unsigned first = 1, playing, last_mask, last_order = 255;

/* chord templates (bits = semitones above the root), simplest first */
static const uint16_t templates[BASS_QUALITIES] FM1_AUDIO_DATA = {
    0x091, 0x089, 0x491, 0x489, 0x891, 0x0A1, 0x085, 0x049, 0x449, 0x111, 0x081};
static const uint8_t third_of[BASS_QUALITIES] FM1_AUDIO_DATA = {4, 3, 4, 3, 4, 5, 2, 3, 3, 4, 0};
static const uint8_t fifth_of[BASS_QUALITIES] FM1_AUDIO_DATA = {7, 7, 7, 7, 7, 7, 7, 6, 6, 8, 7};
static const uint8_t seventh_of[BASS_QUALITIES] FM1_AUDIO_DATA = {0, 0, 10, 10, 11, 0, 0, 0, 10, 0, 0};

FM1_AUDIO_CODE void bass_hear_sequence(unsigned track, unsigned note, unsigned velocity)
{
    if (track >= GROOVE_TRACKS || note > 127) return;
    unsigned type = groove_source_type(track);
    if (type == GROOVE_808 || type == GROOVE_SAMPLE) return;   /* drums carry no harmony */
    uint32_t bit = 1U << (note & 31U);
    if (velocity) {
        unsigned pc = note; while (pc >= 12) pc -= 12;
        onset_mask_audio |= (uint16_t)(1U << pc);
        if (note < onset_low_audio) onset_low_audio = (uint8_t)note;
    }
    if (velocity) sequence_bits[track][note >> 5] |= bit;
    else sequence_bits[track][note >> 5] &= ~bit;
}
void bass_hear_local(unsigned note, unsigned velocity)
{
    if (note > 127) return;
    if (velocity) {
        if (local_count[note] < 255) ++local_count[note];
        onset_mask_local |= (uint16_t)(1U << (note % 12U));
        if (note < onset_low_local) onset_low_local = (uint8_t)note;
    } else if (local_count[note]) --local_count[note];
}
void bass_hear_external(unsigned note, unsigned velocity)
{
    if (note > 127) return;
    if (velocity) {
        if (external_count[note] < 255) ++external_count[note];
        onset_mask_external |= (uint16_t)(1U << (note % 12U));
        if (note < onset_low_external) onset_low_external = (uint8_t)note;
    } else if (external_count[note]) --external_count[note];
}

FM1_AUDIO_CODE void bass_transport(uint8_t status)
{
    if (status == 0xfa || status == 0xff) {
        phase = 0; first = 1; playing = status == 0xfa;
        pg_restart(&pg); window_steps = 0;
    } else if (status == 0xfb) playing = 1;
    else if (status == 0xfc) playing = 0;
    if (status == 0xff) {   /* panic: forget everything heard */
        for (unsigned t = 0; t < GROOVE_TRACKS; ++t) sequence_bits[t][0] = sequence_bits[t][1] = sequence_bits[t][2] = sequence_bits[t][3] = 0;
    }
}

static inline __attribute__((always_inline)) unsigned popcount12(unsigned x)
{
    x = x - ((x >> 1) & 0x555U);
    x = (x & 0x333U) + ((x >> 2) & 0x333U);
    return (x + (x >> 4) + (x >> 8)) & 0xfU;
}
static inline __attribute__((always_inline)) unsigned mod12(int x) { while (x < 0) x += 12; while (x >= 12) x -= 12; return (unsigned)x; }

/* the pitch classes sounding now and the lowest one; 0 mask = silence */
static FM1_AUDIO_CODE unsigned listen(unsigned *lowest)
{
    unsigned mask = 0; *lowest = BASS_NONE;
    for (unsigned n = 0; n < 128; ++n) {
        unsigned on = local_count[n] || external_count[n];
        for (unsigned t = 0; t < GROOVE_TRACKS && !on; ++t) on = (sequence_bits[t][n >> 5] >> (n & 31U)) & 1U;
        if (!on) continue;
        mask |= 1U << mod12((int)n);
        if (*lowest == BASS_NONE) *lowest = n;
    }
    return mask;
}
/* best (root, quality) for a pitch-class set: hits, minus missing and extra tones, plus the bass note */
static FM1_AUDIO_CODE void name_chord(unsigned mask, unsigned lowest, unsigned *root, unsigned *quality)
{
    int best = -1000;
    for (unsigned k = 0; k < 12; ++k) {
        unsigned r = mod12((int)lowest + (int)k);   /* the lowest note's root is tried first: ties go to it */
        unsigned rel = ((mask >> r) | (mask << (12U - r))) & 0xfffU;
        for (unsigned q = 0; q < BASS_QUALITIES; ++q) {
            unsigned t = templates[q];
            int score = 4 * (int)popcount12(rel & t) - 3 * (int)popcount12(t & ~rel) - 2 * (int)popcount12(rel & ~t)
                      + (k == 0 ? 3 : 0);
            if (score > best) { best = score; *root = r; *quality = q; }
        }
    }
}
/* the tones to latch, in bass order (root first, so SEQ walks root-fifth-octave) */
static FM1_AUDIO_CODE unsigned chord_tones(unsigned root, unsigned quality, int *out)
{
    int base = 28 + 12 * (int)bass.octave;                         /* E1, E2 or E3 */
    int r = base + (int)mod12((int)root - 4);
    unsigned n = 0, set = bass.tones;
    int third = third_of[quality] ? r + third_of[quality] : -1;
    int fifth = r + fifth_of[quality], seventh = seventh_of[quality] ? r + seventh_of[quality] : r + 12;
    out[n++] = r;
    if (set >= BASS_TONES_ROOT5) out[n++] = fifth;
    if (set == BASS_TONES_ROOT || set == BASS_TONES_ROOT5 || set == BASS_TONES_TRIAD8) out[n++] = r + 12;
    if (set >= BASS_TONES_TRIAD && third >= 0) out[n++] = third;
    if (set == BASS_TONES_SEVENTH) out[n++] = seventh;
    return n;
}
static FM1_AUDIO_CODE void relatch(unsigned root, unsigned quality)
{
    int want[6]; unsigned count = chord_tones(root, quality, want);
    if ((unsigned)pg.n == count) {
        /* same shape: each ROLE takes its new note in place (root for root, fifth for
         * fifth...), so patgen keeps its exact position and the phrase just transposes */
        for (unsigned k = 0; k < count; ++k) pg.notes[k] = want[k];
        bass_latched_n = (uint8_t)pg.n;
        for (int i = 0; i < pg.n && i < 8; ++i) bass_latched[i] = (uint8_t)pg.notes[i];
        return;
    }
    for (int i = pg.n - 1; i >= 0; --i) {                         /* lift what is no longer in the chord */
        int keep = 0;
        for (unsigned k = 0; k < count; ++k) if (pg.notes[i] == want[k]) keep = 1;
        if (!keep) pg_remove(&pg, pg.notes[i]);
    }
    for (unsigned k = 0; k < count; ++k) pg_add(&pg, want[k]);   /* latch what is new */
    bass_latched_n = (uint8_t)pg.n;
    for (int i = 0; i < pg.n && i < 8; ++i) bass_latched[i] = (uint8_t)pg.notes[i];
}

static FM1_AUDIO_CODE void step(unsigned tempo)
{
    /* listen across a window: what is held now, plus every note that started since */
    unsigned lowest, mask = listen(&lowest);
    mask |= onset_mask_audio | onset_mask_local | onset_mask_external;
    unsigned low = lowest;
    if (onset_low_audio < low) low = onset_low_audio;
    if (onset_low_local < low) low = onset_low_local;
    if (onset_low_external < low) low = onset_low_external;
    onset_mask_audio = 0; onset_mask_local = 0; onset_mask_external = 0;
    onset_low_audio = onset_low_local = onset_low_external = BASS_NONE;
    window_mask |= mask;
    if (low < window_low) window_low = low;
    unsigned per_beat = bass.per_beat && bass.per_beat <= 16 ? bass.per_beat : 4;
    unsigned window = (bass.window ? bass.window : 2) * per_beat;
    unsigned settings = (unsigned)bass.tones | ((unsigned)bass.octave << 4);
    /* decide at the window's edge, or at once if nothing is latched yet or the shape changed */
    if (++window_steps >= window || (!pg.n && window_mask) || settings != last_order) {
        if (window_mask && (window_mask != last_mask || settings != last_order)) {
            unsigned root = 0, quality = 0;
            name_chord(window_mask, window_low == BASS_NONE ? 0 : mod12((int)window_low), &root, &quality);
            bass_root = (uint8_t)root; bass_quality = (uint8_t)quality;
            relatch(root, quality);
        } else if (!window_mask && !bass.hold && pg.n) {
            while (pg.n) pg_remove(&pg, pg.notes[0]);
            bass_latched_n = 0; bass_root = BASS_NONE;
        }
        last_mask = window_mask; last_order = settings;
        window_mask = 0; window_low = BASS_NONE; window_steps = 0;
    }
    pg.order = bass.order < PG_NORDER ? bass.order : PG_SEQ;
    pg.rest = bass.rest < PG_NREST ? bass.rest : PG_REST_NONE;
    pg.octave_chance = bass.octave_chance;
    bass_rest_pos = (uint8_t)pg.rest_idx;
    int note = pg_step(&pg);
    bass_history[bass_history_pos] = note == PG_REST ? BASS_NONE : (uint8_t)note;
    bass_history_pos = (uint8_t)((bass_history_pos + 1U) & (BASS_HISTORY - 1U));
    if (note == PG_REST || bass.muted || note < 0 || note > 127) return;
    uint32_t tick = (uint32_t)(((uint64_t)dv.step_samples[tempo] * 4U * dv.reciprocal[per_beat]) >> 16);  /* Q8 */
    uint32_t gate = (uint32_t)(((uint64_t)tick * bass.gate) >> 24);
    dv_trigger(&bass.voice, 0, 0, bass.level, gate ? gate : 1, note);
}

FM1_AUDIO_CODE void bass_tick(void)
{
    bass_running = (uint8_t)playing;
    if (!playing) return;
    unsigned tempo = fm1_transport_tempo, per_beat = bass.per_beat;
    if (!per_beat || per_beat > 16) per_beat = 4;
    if (first || phase >= PERIOD) {   /* the clock always runs, so arming mid-song stays on the beat */
        if (first) first = 0; else phase -= PERIOD;
        if (bass.armed && dv.ready) step(tempo);
    }
    phase += tempo * per_beat * FM1_BLOCK_FRAMES;
}
