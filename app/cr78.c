/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* CR-78 panel mode. HOME+LFO opens and closes it; the pattern keeps playing
 * with the FM-1 transport after you leave.
 *
 *   white keys      toggle the 16 steps of the selected voice
 *     + KNOB 1      while a step key is held: that step's ratchet, 1..8
 *     + OCT -       hold OCT - and tap a step: cycle its ratchet 1 2 3 4 6 8
 *     + OCT +       hold OCT + and tap a step: toggle the accent on that step
 *   black keys      play KICK SNARE RIM HAT-CL HAT-OP CYMBAL COWBELL CLAVES
 *                   BONGO-H CONGA GUIRO by hand
 *   SELECT          voice row          PRESETS     preset (reloads the pattern)
 *   KNOB 1          swing 50..66       KNOB 2      FM-1 tempo
 *   KNOB 3          voice tune         KNOB 4      voice decay
 *   ALGORITHM       CR-78 level        PLAY        FM-1 play / stop
 *   FX              mute the CR-78
 *
 * Voices: the recipes are dreamengine's cr78 cart, number for number, played by
 * the shared drum engine (drumvoice.c). */
#include "cr78.h"
#include "drumvoice.h"
#include "pixel.h"
#include "instrument.h"
#include <string.h>

enum { V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM, V_METAL, V_COW,
       V_TAMB, V_MAR, V_CLV, V_GUIRO, V_BONH, V_BONL, V_CONGA, V_GUIROL, NV };
static const char *const names[NV] = {
    "KICK", "SNARE", "RIM", "HAT CL", "HAT OP", "CYMBAL", "METAL", "COWBELL",
    "TAMB", "MARACAS", "CLAVES", "GUIRO", "BONGO H", "BONGO L", "CONGA", "GUIRO L"};
static const uint8_t row_color[NV] = {
    PX_RED, PX_EMBER, PX_PINK, PX_GOLD, PX_GOLD, PX_WHITE, PX_CYAN, PX_GREEN,
    PX_PINK, PX_EMBER, PX_EMBER, PX_GREEN, PX_CYAN, PX_CYAN, PX_EMBER, PX_GREEN};

/* ---- the recipes (dreamengine instrument() calls + fire() hits) ---- */
#define R(w, n, flt, cut, res, a, d, s, r, ex, g, v, lvl, pa, pm) \
    {.wave = w, .note = n, .filter = flt, .cutoff = cut, .reso = res, .attack = a, .decay = d, \
     .sustain = s, .release = r, .exp_release = ex, .gate = g, .vol = v, .level = lvl, .pitch = pa, .pitch_ms = pm}
static const struct dv_recipe recipes[NV][3] = {
    [V_KICK]  = {R(DV_SINE, 36, DV_FILTER_LOW, 320, 2, 0, 140, 0, 40, 0, 220, 5, 1.0f, 2.0f, 30)},
    [V_SNARE] = {R(DV_SINE, 63, DV_FILTER_NONE, 0, 0, 0, 60, 0, 20, 0, 100, 3, 0.15f, 2.0f, 15),
                 R(DV_NOISE, 60, DV_FILTER_LOW, 8000, 6, 0, 70, 0, 25, 0, 120, 6, 1.0f, 0, 0)},
    [V_RIM]   = {R(DV_TRI, 90, DV_FILTER_BAND, 1500, 2, 0, 0, 7, 6, 1, 1, 7, 1.0f, 0, 0),
                 R(DV_SINE, 57, DV_FILTER_NONE, 0, 0, 0, 0, 7, 8, 1, 1, 1, 1.0f, 0, 0)},
    [V_HATC]  = {R(DV_NOISE, 90, DV_FILTER_BAND, 8400, 15, 0, 22, 0, 10, 0, 40, 3, 0.38f, 0, 0)},
    [V_HATO]  = {R(DV_NOISE, 90, DV_FILTER_BAND, 8400, 15, 0, 290, 0, 150, 0, 420, 3, 0.38f, 0, 0)},
    [V_CYM]   = {R(DV_NOISE, 90, DV_FILTER_BAND, 8600, 15, 0, 400, 0, 150, 0, 600, 3, 0.21f, 0, 0)},
    [V_METAL] = {R(DV_TRI, 114, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 4, 0.36f, 0, 0),
                 R(DV_TRI, 112, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 4, 0.36f, 0, 0),
                 R(DV_TRI, 107, DV_FILTER_BAND, 5400, 6, 0, 60, 0, 25, 0, 120, 1, 0.36f, 0, 0)},
    [V_COW]   = {R(DV_SQUARE, 79, DV_FILTER_LOW, 1100, 2, 0, 55, 0, 25, 0, 150, 4, 0.35f, 0, 0),
                 R(DV_SQUARE, 73, DV_FILTER_LOW, 1100, 2, 0, 55, 0, 25, 0, 150, 3, 0.35f, 0, 0)},
    [V_TAMB]  = {R(DV_NOISE, 80, DV_FILTER_BAND, 4650, 15, 8, 200, 0, 60, 0, 300, 3, 0.18f, 0, 0)},
    [V_MAR]   = {R(DV_NOISE, 90, DV_FILTER_BAND, 8700, 15, 0, 15, 0, 8, 0, 30, 3, 0.47f, 0, 0)},
    [V_CLV]   = {R(DV_SINE, 99, DV_FILTER_NONE, 0, 0, 0, 40, 0, 12, 0, 60, 4, 1.0f, 0, 0)},
    [V_GUIRO] = {R(DV_SCRAPE, 60, DV_FILTER_NONE, 0, 0, 0, 0, 7, 30, 0, 270, 4, 0.99f, 0, 0)},
    [V_BONH]  = {R(DV_SINE, 74, DV_FILTER_NONE, 0, 0, 0, 40, 0, 25, 0, 140, 4, 1.0f, 0, 0)},
    [V_BONL]  = {R(DV_SINE, 67, DV_FILTER_NONE, 0, 0, 0, 40, 0, 25, 0, 80, 3, 1.0f, 0, 0),
                 R(DV_SINE, 99, DV_FILTER_NONE, 0, 0, 0, 25, 0, 10, 0, 40, 3, 1.0f, 0, 0)},
    [V_CONGA] = {R(DV_SINE, 55, DV_FILTER_NONE, 0, 0, 0, 160, 0, 60, 0, 300, 4, 1.0f, 0, 0)},
    [V_GUIROL]= {R(DV_SCRAPE, 60, DV_FILTER_NONE, 0, 0, 0, 0, 7, 30, 0, 820, 4, 0.75f, 0, 0)},
};
static const uint8_t part_count[NV] = {1, 2, 2, 1, 1, 1, 3, 2, 1, 1, 1, 1, 1, 2, 1, 1};
static const float voice_tune[NV] = {[V_CLV] = -0.23f};
/* scrape: teeth per second (10 * 10^timbre) and the body transposition */
static const float teeth[NV] = {[V_GUIRO] = 74.13f, [V_GUIROL] = 83.18f};

/* ---- presets (the cart's eight) ---- */
struct preset { const char *name; uint8_t tempo, swing; const char *row[NV]; const char *accent; };
static const struct preset presets[] = {
    {"ROCK-1", 118, 50, {[V_KICK] = "x.......x.x.....", [V_SNARE] = "....x.......x...",
                         [V_HATC] = "x.x.x.x.x.x.x.x."}, "x.......x......."},
    {"DISCO-2", 100, 50, {[V_KICK] = "x...x...x...x...", [V_HATC] = "xx.xxx.xxx.xxx.x",
                          [V_HATO] = "..x...x...x...x.", [V_TAMB] = "....x.......x...",
                          [V_BONH] = ".........x.....x", [V_GUIRO] = "............x..."}, "x.......x......."},
    {"BOSSA", 132, 50, {[V_KICK] = "x.....x.x.....x.", [V_RIM] = "x..x..x...x..x..",
                        [V_HATC] = "x.x.x.x.x.x.x.x.", [V_MAR] = "xxxxxxxxxxxxxxxx"}, "x.......x......."},
    {"MAMBO", 176, 50, {[V_KICK] = "x.......x.......", [V_COW] = "x..x..x.x..x..x.",
                        [V_CONGA] = "..x...x...x...x.", [V_BONH] = ".x......x.x.....",
                        [V_CLV] = "x..x..x...x.x..."}, "x.......x......."},
    {"CHA-CHA", 118, 50, {[V_GUIROL] = "x...............", [V_GUIRO] = "........x...x...",
                          [V_COW] = "x...x...x...x...", [V_KICK] = "....x.......x...",
                          [V_CONGA] = "......x.......xx", [V_BONL] = "..x.......x....."}, "x.......x......."},
    {"BEGUINE", 104, 50, {[V_KICK] = "x.....x...x.....", [V_METAL] = "x...x...x...x...",
                          [V_RIM] = "...x...x.....x..", [V_HATC] = "x.x.x.x.x.x.x.x.",
                          [V_BONL] = "..............x."}, "x.........x....."},
    {"SHUFFLE", 96, 66, {[V_KICK] = "x.......x.......", [V_SNARE] = "....x.......x...",
                         [V_HATC] = "x.x.x.x.x.x.x.x."}, "x.......x......."},
    {"SWING", 138, 66, {[V_KICK] = "x.......x.......", [V_HATC] = "x...x.x.x...x.x.",
                        [V_RIM] = "....x.......x..."}, "x.......x......."},
};
enum { PRESETS = sizeof presets / sizeof presets[0] };

static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint8_t black_voice[11] = {V_KICK, V_SNARE, V_RIM, V_HATC, V_HATO, V_CYM,
                                        V_COW, V_CLV, V_BONH, V_CONGA, V_GUIRO};
static const uint8_t ratchet_cycle[] = {1, 2, 3, 4, 6, 8};

static unsigned active, initialized, full_redraw, selected, preset, swing = 50, level = 40;
static int tune[NV];          /* semitones */
static uint8_t decay[NV];     /* percent, 25..200 */
static uint32_t prev_buttons, prev_keys, last_ms, led_state, flash_until[NV];
static uint16_t held_steps;   /* step keys held right now */
static uint32_t notice_until; static const char *notice;

static void build_voice(unsigned v)
{
    struct dv_voice next = {0};
    next.parts = part_count[v];
    for (unsigned p = 0; p < part_count[v]; ++p) {
        struct dv_recipe r = recipes[v][p];
        r.tune = voice_tune[v];
        r.teeth = teeth[v];
        r.body = v == V_GUIROL;
        dv_build_part(&next.part[p], &r, (float)tune[v], (float)decay[v] / 100.0f);
    }
    cr78.voice[v] = next;   /* one copy; a hit mid-update just picks either */
}
static void apply_globals(void)
{
    cr78.swing_q16 = (int32_t)((swing - 50U) * 4U * 65536U / 100U);
    cr78.level = (int32_t)(level * 32768U / 100U);
}
static void load_preset(unsigned index)
{
    const struct preset *p = &presets[index];
    for (unsigned v = 0; v < NV; ++v) {
        uint16_t bits = 0;
        for (unsigned s = 0; s < 16; ++s) {
            if (p->row[v] && p->row[v][s] == 'x') bits |= (uint16_t)(1U << s);
            cr78.ratchet[v][s] = 1;
        }
        cr78.grid[v] = bits;
    }
    uint16_t accent = 0;
    for (unsigned s = 0; s < 16; ++s) if (p->accent && p->accent[s] == 'x') accent |= (uint16_t)(1U << s);
    cr78.accent = accent;
    swing = p->swing;
    apply_globals();
    /* the preset's tempo becomes the FM-1 tempo, as on the real box */
    groove_tempo_edit((int)p->tempo - (int)groove_tempo_current());
}
static void init(void)
{
    dv_build_tables();
    for (unsigned v = 0; v < NV; ++v) { tune[v] = 0; decay[v] = 100; build_voice(v); }
    preset = 1;   /* DISCO-2, obviously */
    load_preset(preset);
    initialized = 1;
}

/* ---- screen ---- */
enum { GRID_X = 13, GRID_Y = 14, ROW = 3, COL = 4, LANE_Y = 78 };
static void draw(uint32_t now)
{
    char text[24];
    px_cls(PX_NIGHT);
    px_print("CR78", 2, 1, PX_GOLD);
    px_print(presets[preset].name, 20, 1, PX_EMBER);
    px_number((int)groove_tempo_current(), text);
    px_print(text, PX_W - 2 - ((int)strlen(text) * 4 - 1), 1, PX_SLATE);
    if (notice && (int32_t)(notice_until - now) > 0) px_print(notice, 2, 7, PX_WHITE);
    else {
        px_print(names[selected], 2, 7, row_color[selected]);
        if (cr78.muted) px_print("MUTE", PX_W - 17, 7, PX_RED);
        else {
            memcpy(text, "SW", 3); px_number((int)swing, text + 2);
            px_print(text, PX_W - 2 - ((int)strlen(text) * 4 - 1), 7, swing > 50 ? PX_EMBER : PX_SLATE);
        }
    }
    unsigned head = cr78_playhead, run = cr78_running;
    if (run) px_rect(GRID_X + (int)head * COL - 1, GRID_Y - 2, COL + 1, NV * ROW + 2, PX_DUSK);
    for (unsigned s = 0; s < 16; ++s)   /* accent strip */
        px_rect(GRID_X + (int)s * COL, GRID_Y - 2, COL - 1, 1, (cr78.accent >> s) & 1U ? PX_EMBER : ((s & 3U) ? PX_NIGHT : PX_DUSK));
    uint16_t hits = cr78_hits; cr78_hits = 0;
    for (unsigned v = 0; v < NV; ++v) {
        if (hits & (1U << v)) flash_until[v] = now + 90U;
        int y = GRID_Y + (int)v * ROW;
        unsigned lit = (int32_t)(flash_until[v] - now) > 0;
        px_rect(1, y, 2, 2, lit ? PX_WHITE : row_color[v]);
        if (v == selected) px_rect(5, y, 6, 2, PX_GOLD);
        for (unsigned s = 0; s < 16; ++s) {
            int x = GRID_X + (int)s * COL;
            unsigned on = (cr78.grid[v] >> s) & 1U;
            if (on) {
                unsigned r = cr78.ratchet[v][s];
                px_rect(x, y, COL - 1, 2, run && s == head ? PX_WHITE : row_color[v]);
                if (r > 1) px_pset(x + 1, y, PX_NIGHT);    /* a notch marks a ratchet */
            } else if (!(s & 3U)) px_pset(x + 1, y + 1, PX_DUSK);
        }
    }
    /* ratchet lane for the selected voice: bar height = hits per step */
    px_rect(GRID_X - 1, LANE_Y - 9, 16 * COL + 1, 1, PX_DUSK);
    for (unsigned s = 0; s < 16; ++s) {
        if (!((cr78.grid[selected] >> s) & 1U)) continue;
        unsigned r = cr78.ratchet[selected][s];
        px_rect(GRID_X + (int)s * COL, LANE_Y - (int)r, COL - 1, (int)r,
                (held_steps >> s) & 1U ? PX_GOLD : (r > 1 ? PX_EMBER : PX_SLATE));
    }
    px_print("R", 2, LANE_Y - 6, PX_SLATE);
}
static void step_leds(uint32_t now)
{
    /* white-key LEDs show the selected voice's steps; the playhead blinks */
    uint32_t want = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned on = (cr78.grid[selected] >> s) & 1U;
        if (cr78_running && s == cr78_playhead) on = !on || ((now / 60U) & 1U);
        if (on) want |= 1U << s;
    }
    for (unsigned s = 0; s < 16; ++s)
        if (((want ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (want >> s) & 1U);
    led_state = want;
}

unsigned cr78_active(void) { return active; }

int cr78_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys;
    unsigned combo = px_combo(c->buttons, pressed, PX_HOME, PX_LFO);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (initialized) cr78.fm1_swing = (uint8_t)groove_sequence_setting(0);
    if (!active) {
        if (!combo) return 0;
        if (!initialized) init();
        cr78.fm1_swing = (uint8_t)groove_sequence_setting(0);
        cr78.armed = 1;
        px_leds_off(); led_state = 0; held_steps = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U;
        notice = "HOME LFO EXIT"; notice_until = now + 1500U;
        return 1;
    }
    if (combo) {
        px_leds_off(); led_state = 0;
        active = 0;
        return 2;
    }
    unsigned changed = 0;
    held_steps = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned key = white_keys[s];
        if ((c->keys >> key) & 1U) held_steps |= (uint16_t)(1U << s);
        if (!((down >> key) & 1U)) continue;
        uint16_t bit = (uint16_t)(1U << s);
        if (c->buttons & PX_BIT(PX_OCT_UP)) {
            cr78.accent ^= bit;
        } else if (c->buttons & PX_BIT(PX_OCT_DOWN)) {
            unsigned r = cr78.ratchet[selected][s], i = 0;
            while (i < sizeof ratchet_cycle && ratchet_cycle[i] <= r) ++i;
            cr78.ratchet[selected][s] = ratchet_cycle[i % sizeof ratchet_cycle];
            cr78.grid[selected] |= bit;
        } else {
            cr78.grid[selected] ^= bit;
            if (cr78.grid[selected] & bit) {
                cr78.ratchet[selected][s] = 1;
                if (!cr78_running) (void)cr78_audition(selected, 0);
            }
        }
        changed = 1;
    }
    for (unsigned i = 0; i < sizeof black_keys; ++i)
        if ((down >> black_keys[i]) & 1U) (void)cr78_audition(black_voice[i], 1);
    const int16_t *e = c->encoders;
    if (e[0]) { selected = (unsigned)(((int)selected + e[0] + NV * 8) % NV); (void)cr78_audition(selected, 0); changed = 1; }
    if (e[6]) { preset = (unsigned)(((int)preset + e[6] + PRESETS * 8) % PRESETS); load_preset(preset); changed = 1; }
    if (e[1]) { int l = (int)level + e[1] * 4; level = (unsigned)(l < 0 ? 0 : l > 100 ? 100 : l); apply_globals(); changed = 1;
                notice = "LEVEL"; notice_until = now + 600U; }
    if (e[2]) {
        if (held_steps) {
            for (unsigned s = 0; s < 16; ++s)
                if ((held_steps >> s) & 1U) {
                    int r = (int)cr78.ratchet[selected][s] + e[2];
                    cr78.ratchet[selected][s] = (uint8_t)(r < 1 ? 1 : r > CR78_RATCHET_MAX ? CR78_RATCHET_MAX : r);
                    cr78.grid[selected] |= (uint16_t)(1U << s);
                }
        } else {
            int sw = (int)swing + e[2] * 2;
            swing = (unsigned)(sw < 50 ? 50 : sw > 66 ? 66 : sw); apply_globals();
        }
        changed = 1;
    }
    if (e[3]) { groove_tempo_edit(e[3]); changed = 1; }
    if (e[4] || e[5]) {
        int t = tune[selected] + e[4], d = (int)decay[selected] + e[5] * 5;
        tune[selected] = t < -12 ? -12 : t > 12 ? 12 : t;
        decay[selected] = (uint8_t)(d < 25 ? 25 : d > 200 ? 200 : d);
        build_voice(selected);
        if (!cr78_running) (void)cr78_audition(selected, 0);
        notice = e[4] ? "TUNE" : "DECAY"; notice_until = now + 600U;
        changed = 1;
    }
    if (pressed & PX_BIT(PX_FX)) { cr78.muted ^= 1U; changed = 1; }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0};
        groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (now - last_ms >= 33U || changed) {
        last_ms = now;
        draw(now);
        step_leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
