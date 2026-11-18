/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The launcher: one way into every app mode, instead of a combo per mode.
 *
 *   HOME + ARP   open the launcher (from anywhere in the stock UI)
 *   KNOB 1       move through the list (SELECT works too)
 *   OCT +        open the highlighted mode       HOME / OCT -   back to the FM-1
 *   inside a mode, HOME comes back here
 *
 * Modes keep running after you leave them (the drum machines and the bass
 * keep playing); the launcher only decides which one owns the panel. */
#include "launcher.h"
#include "pixel.h"
#include "bass.h"
#include "cr78.h"
#include "rhythm.h"
#include "mysynth.h"
#include "fx.h"
#include "gbx.h"
#include "deck.h"
#include "tekno.h"
#include "instrument.h"
#include <string.h>

struct mode {
    const char *name, *line1, *line2;
    int (*control)(const struct fm1_controls *);
    unsigned (*active)(void);
};
static const struct mode modes[PX_MODES] = {
    [PX_MODE_BASS] = {"BASS PLAYER", "FOLLOWS THE CHORDS", "PATGEN BASS LINES", bass_control, bass_active},
    [PX_MODE_CR78] = {"CR-78", "16 STEPS", "RATCHETS AND SWING", cr78_control, cr78_active},
    [PX_MODE_RHYTHM] = {"RHYTHM BOX", "101 FACTORY RHYTHMS", "TR-77 AND SIDE MAN", rhythm_control, rhythm_active},
    [PX_MODE_SYNTH] = {"MY SYNTH", "6 VOICE SUBTRACTIVE", "PLAY IT ON THE KEYS", mysynth_control, mysynth_active},
    [PX_MODE_FX] = {"FX", "SPRING CHORUS DRIVE", "AND A KICK DUCK", fx_control, fx_active},
    [PX_MODE_GBX] = {"GROOVEBOX", "4 TRACKS ONE WAY", "", gbx_control, gbx_active},
    [PX_MODE_MINI] = {"GROOVE MINI", "KICK SNARE HAT BASS", "", gbx_mini_control, gbx_mini_active},
    [PX_MODE_DECK] = {"LOOP DECK", "TOGGLE LOOPS LIVE", "", deck_control, deck_active},
    [PX_MODE_TEKNO] = {"TEKNO", "KICK TOMS PHASER", "", tekno_control, tekno_active},
    [PX_MODE_GAME] = {"STARFALL", "CATCH STARS", "WITH THE KEYS", game_control, game_active},
};

static unsigned open, selected, full_redraw;
static int current = -1;   /* the mode that owns the panel, or -1 for the list */
static uint32_t prev_buttons;

unsigned launcher_active(void) { return open; }

static void draw(void)
{
    px_cls(PX_NIGHT);
    px_print("MODES", 2, 1, PX_GOLD);
    unsigned first = selected > 7U ? selected - 7U : 0U;   /* eight rows show; the list scrolls */
    for (unsigned i = first; i < PX_MODES && i < first + 8U; ++i) {
        int y = 8 + (int)(i - first) * 6;
        unsigned on = i == selected;
        if (on) px_rect(0, y - 1, PX_W, 6, PX_DUSK);
        px_print(modes[i].name, 6, y, on ? PX_WHITE : PX_SLATE);
        /* a dot for modes that keep playing in the background */
        if ((i == PX_MODE_BASS && bass.armed) || (i == PX_MODE_CR78 && cr78.armed) || (i == PX_MODE_RHYTHM && rhythm.armed) ||
            (i == PX_MODE_FX && fx_running()))
            px_rect(2, y + 1, 2, 2, PX_GREEN);
    }
    px_print(modes[selected].line1, 2, 58, PX_EMBER);   /* (one line: the list fills the screen) */
    px_fine_cls(PX_NIGHT);
    px_fine_rect(0, 1, PX_W * 3, 1, PX_DUSK);
    px_fine_print("K1", 3, 10, PX_WHITE);   px_fine_print("MOVE", 3, 23, PX_SLATE);
    px_fine_print("OCT+", 63, 10, PX_WHITE); px_fine_print("OPEN", 63, 23, PX_SLATE);
    px_fine_print("HOME", 183, 10, PX_WHITE); px_fine_print("BACK", 183, 23, PX_SLATE);
}

int launcher_control(const struct fm1_controls *c)
{
    uint32_t pressed = c->buttons & ~prev_buttons;
    prev_buttons = c->buttons;
    if (current >= 0) {   /* a mode owns the panel; HOME there brings us back */
        if (modes[current].control(c) == 2) {
            current = -1; full_redraw = 1;
            draw(); px_present(1);
        }
        return 1;
    }
    if (!open) {
        if (!px_combo(c->buttons, pressed, PX_HOME, PX_ARP)) return 0;
        open = 1; full_redraw = 1;
        px_leds_off();
        draw(); px_present(1);
        return 1;
    }
    if (pressed & (PX_BIT(PX_HOME) | PX_BIT(PX_OCT_DOWN))) { open = 0; px_leds_off(); return 2; }
    int move = c->encoders[2] + c->encoders[0];
    if (move) {
        int s = (int)selected + move;
        selected = (unsigned)(s < 0 ? 0 : s >= PX_MODES ? PX_MODES - 1 : s);
    }
    if (pressed & PX_BIT(PX_OCT_UP)) {
        px_mode_request((int)selected);
        current = (int)selected;
        modes[current].control(c);   /* opens it on this poll */
        return 1;
    }
    draw();
    px_present(full_redraw);
    full_redraw = 0;
    return 1;
}
