/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* patgen.h: a LATCHED PATTERN GENERATOR, from dreamengine's runtime/patgen.h
 * (itself ported from CHOMPI TEMPO's ArpeggiatorSequencer, MIT, CHOMPI Club /
 * Chase Bliss). Hold some notes, it plays them back on a clock, with orders,
 * rests and octave jumps.
 *
 * THE HONEST CORE: a REST consumes a clock step but does NOT advance the note,
 * so the gap DRIFTS through the line whenever the number of latched notes
 * doesn't divide the mask's plays-per-period (four notes against XXX. take 16
 * steps to come round). Adding or lifting one note reshapes the whole line.
 *
 * Changed for the FM-1 audio ISR, behaviour kept: no floats (octave chance is
 * Q16) and no division or modulo (a pick is a multiply-high), so every
 * function inlines into audio code with no library calls. */
#ifndef FM1_PATGEN_H
#define FM1_PATGEN_H
#include <stdint.h>

#define PG_MAX  16
#define PG_REST (-1)
enum { PG_SEQ = 0, PG_UP, PG_DOWN, PG_PINGPONG, PG_RANDOM, PG_NORDER };
enum { PG_REST_NONE = 0, PG_REST_3_1, PG_REST_2_2, PG_REST_1_1, PG_REST_10, PG_NREST };
#define PG_REST_STEPS 20
static const uint32_t PG_REST_MASK[PG_NREST] = {
    0xFFFFFu,   /* XXXXXXXXXXXXXXXXXXXX */
    0x77777u,   /* XXX.XXX.XXX.XXX.XXX. */
    0x33333u,   /* XX..XX..XX..XX..XX.. */
    0x55555u,   /* X.X.X.X.X.X.X.X.X.X. */
    0x6B5ADu,   /* X.XX.X.XX.X.XX.X.XX.  (period 10) */
};

typedef struct {
    int notes[PG_MAX];   /* latched MIDI notes, in PRESS order */
    int n, order, rest;
    uint32_t octave_chance;   /* Q16: 65536 = always jump an octave */
    int idx, dir, rest_idx, started, last;
    uint32_t seed;
} PatGen;

static inline uint32_t pg__rand(PatGen *p) { p->seed = (1103515245u * p->seed + 12345u) & 0x7FFFFFFFu; return p->seed; }
/* 0..n-1 from the HIGH bits (an LCG's low bits cycle) */
static inline uint32_t pg__pick(PatGen *p, uint32_t n) { return (uint32_t)(((uint64_t)(pg__rand(p) >> 15) * n) >> 16); }

static inline void pg_init(PatGen *p, uint32_t seed)
{
    p->n = 0; p->order = PG_SEQ; p->rest = PG_REST_NONE; p->octave_chance = 0;
    p->idx = 0; p->dir = 1; p->rest_idx = 0; p->started = 0; p->seed = seed ? seed : 1u; p->last = PG_REST;
}
static inline void pg_restart(PatGen *p) { p->idx = 0; p->dir = 1; p->rest_idx = 0; p->started = 0; }
static inline int pg_has(const PatGen *p, int midi)
{
    for (int i = 0; i < p->n; i++) if (p->notes[i] == midi) return 1;
    return 0;
}
static inline void pg_add(PatGen *p, int midi)
{
    if (pg_has(p, midi) || p->n >= PG_MAX) return;
    p->notes[p->n++] = midi;
}
/* the view an order plays through: press order, or sorted */
static inline void pg__view(const PatGen *p, int *v)
{
    for (int i = 0; i < p->n; i++) v[i] = p->notes[i];
    if (p->order == PG_SEQ || p->order == PG_RANDOM) return;
    for (int i = 1; i < p->n; i++) {
        int x = v[i], j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    if (p->order == PG_DOWN)
        for (int i = 0, j = p->n - 1; i < j; i++, j--) { int t = v[i]; v[i] = v[j]; v[j] = t; }
}
/* unlatching keeps the line pointing at the same NEXT note (TEMPO's removeKey) */
static inline void pg_remove(PatGen *p, int midi)
{
    int v[PG_MAX]; pg__view(p, v);
    int at = -1;
    for (int i = 0; i < p->n; i++) if (v[i] == midi) { at = i; break; }
    if (at < 0) return;
    for (int i = 0, k = 0; i < p->n; i++) if (p->notes[i] != midi) p->notes[k++] = p->notes[i];
    p->n--;
    if (at < p->idx) p->idx--;
    if (p->idx >= p->n) p->idx = p->n > 0 ? p->n - 1 : 0;
}
static inline void pg__advance(PatGen *p)
{
    int n = p->n;
    switch (p->order) {
    case PG_SEQ: case PG_UP: case PG_DOWN: if (++p->idx >= n) p->idx = 0; break;
    case PG_PINGPONG:   /* step, and at an end step back + turn: the ends repeat */
        p->idx += p->dir;
        if (p->idx > n - 1) { p->idx = n - 1; p->dir = -1; }
        if (p->idx < 0) { p->idx = 0; p->dir = 1; }
        break;
    case PG_RANDOM: p->idx = (int)pg__pick(p, (uint32_t)n); break;
    }
}
/* one clock edge -> the note to play, or PG_REST. A rest consumes the step and NOT the note. */
static inline int pg_step(PatGen *p)
{
    if (p->n == 0) { p->last = PG_REST; return PG_REST; }
    int plays = (PG_REST_MASK[p->rest] >> p->rest_idx) & 1u;
    if (++p->rest_idx >= PG_REST_STEPS) p->rest_idx = 0;
    if (!plays) { p->last = PG_REST; return PG_REST; }
    if (!p->started) {
        p->started = 1; p->dir = 1;
        p->idx = p->order == PG_RANDOM ? (int)pg__pick(p, (uint32_t)p->n) : 0;
    } else pg__advance(p);
    if (p->idx >= p->n) p->idx = 0;
    int v[PG_MAX]; pg__view(p, v);
    int midi = v[p->idx];
    if (p->octave_chance && (pg__rand(p) >> 15) < p->octave_chance)
        midi += pg__pick(p, 2u) ? 12 : -12;
    p->last = midi;
    return midi;
}
#endif
