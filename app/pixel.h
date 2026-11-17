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
void px_print(const char *s, int x, int y, unsigned color);   /* 0-9, A-Z, space, - + / . : */
void px_print_c(const char *s, int y, unsigned color);        /* centred */
void px_number(int value, char *out);                         /* out: at least 12 bytes */
/* Blit changed bands; force sends the whole screen (first frame of a mode). */
void px_present(unsigned force);
void px_leds_off(void);
/* 1 on the poll where `a` and `b` are both held and one of them just went down. */
unsigned px_combo(uint32_t buttons, uint32_t pressed, unsigned a, unsigned b);
void px_print_scaled(const char *s, int x, int y, int scale, unsigned color);

/* ---- the shared UI grammar ------------------------------------------------
 * SELECT pages, the four KNOBs are soft knobs labelled in the bottom bar,
 * PRESETS loads the mode's presets, HOME goes back. The content area between
 * the header and the knob bar is y 8..64. */
enum { PX_CONTENT_TOP = 8, PX_CONTENT_BOTTOM = 64 };
struct px_page { const char *name; const char *knob[4]; };
/* header (mode, page name, one dot per page) + bottom knob bar with values, in the fine strip */
void px_chrome(const char *mode, const struct px_page *pages, unsigned count, unsigned current,
               const char *const value[4]);
/* ... and above the knob bar, the eleven black keys' labels (3 letters fit), `lit` bit i = pad i lit */
enum { PX_PADS = 11 };
void px_chrome_pads(const char *mode, const struct px_page *pages, unsigned count, unsigned current,
                    const char *const value[4], const char *const pad[PX_PADS], uint32_t lit);

/* ---- the fine strip: the screen's bottom 48 rows at full resolution (240 x 48), in place of
 * canvas rows 64..79, drawn with the same 3x5 font at 2x (7 px a character: 3 letters in 20 px).
 * On for any frame that draws into it after px_cls; otherwise the canvas fills the screen. */
enum { PX_FINE_ROW = 64, PX_FINE_H = 48, PX_FINE_ADV = 7 };
void px_fine_cls(unsigned color);
void px_fine_rect(int x, int y, int w, int h, unsigned color);   /* screen pixels, y from the strip's top */
void px_fine_print(const char *s, int x, int y, unsigned color);
int px_fine_width(const char *s);
/* a big centred word for `ms` (page names, confirmations), drawn by px_flash_draw */
void px_flash(const char *text, uint32_t now, uint32_t ms);
void px_flash_draw(uint32_t now);

/* ---- modes and the launcher ---- */
enum { PX_MODE_BASS, PX_MODE_CR78, PX_MODE_RHYTHM, PX_MODE_SYNTH, PX_MODE_FX, PX_MODE_GAME, PX_MODE_GBX, PX_MODES };
void px_mode_request(int mode);   /* the launcher asks `mode` to open on its next control call */
/* a mode's open/close signal: 1 when closed and asked to open, or open and HOME was pressed */
unsigned px_mode_toggle(uint32_t pressed, unsigned mode, unsigned active);
#endif
