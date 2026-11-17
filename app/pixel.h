/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* A tiny shared "pixel console" for full-screen modes (STARFALL, MY SYNTH):
 * an 80x80 palette canvas scaled 3x to the 240x240 panel, a 3x5 font, and a
 * presenter that only blits the 12-pixel bands that changed. Control task only.
 * Modes are exclusive, so they share one canvas. */
#ifndef FM1_PIXEL_H
#define FM1_PIXEL_H
#include "fm1.h"

enum { PX_W = 80, PX_H = 80 };
enum {
    PX_NIGHT, PX_DUSK, PX_WHITE, PX_GOLD, PX_EMBER,
    PX_PINK, PX_CYAN, PX_GREEN, PX_SLATE, PX_RED, PX_COLORS
};
/* Panel button indices (fm1_controls.buttons bits). */
enum { PX_OCT_DOWN, PX_OCT_UP, PX_FX, PX_SCL, PX_ENV, PX_LFO, PX_EDIT, PX_GLO,
       PX_HOME, PX_SAVE, PX_ARP, PX_SEQ, PX_PLAY, PX_REC };
enum { PX_KEYS = 27, PX_FIRST_NOTE = 53 /* F3 */, PX_KEY_LED = 14 };
#define PX_BIT(b) (1U << (b))

void px_cls(unsigned color);
void px_pset(int x, int y, unsigned color);
void px_rect(int x, int y, int w, int h, unsigned color);
void px_print(const char *s, int x, int y, unsigned color);   /* 0-9, A-Z, space, '-', '+' */
void px_print_c(const char *s, int y, unsigned color);        /* centred */
void px_number(int value, char *out);                         /* out: at least 12 bytes */
/* Blit changed bands; force sends the whole screen (first frame of a mode). */
void px_present(unsigned force);
void px_leds_off(void);
/* 1 on the poll where `a` and `b` are both held and one of them just went down. */
unsigned px_combo(uint32_t buttons, uint32_t pressed, unsigned a, unsigned b);
#endif
