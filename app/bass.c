/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* BASS PLAYER panel mode, in the shared grammar: SELECT pages, the four KNOBs
 * are labelled in the bottom bar, PRESETS loads a bass style, HOME goes back.
 * Opened from the launcher; once armed, the bass keeps following and playing
 * after you leave.
 *
 *   LINE    ORDER  RESTS  DIV    TONES      what patgen plays
 *   SOUND   CUT    RES    DECAY  SUB        the voice
 *   PLAY    GATE   JUMP   OCT    LEVEL      how it is played
 *   EAR     EAR    LISTEN HOLD   -          what it listens to, and for how long
 *
 *   PRESETS  bass style     FX  mute     PLAY  FM-1 play/stop     OCT -/+  register
 *   keys     play chords on track 1: the bass hears them like anything else */
#include "bass.h"
#include "pixel.h"
#include "instrument.h"
#include <string.h>

enum { LINE, SOUND, PLAY, EAR, PAGES };
static const struct px_page pages[PAGES] = {
    {"LINE", {"ORDR", "REST", "DIV", "TONE"}},
    {"SOUND", {"CUT", "RES", "DEC", "SUB"}},
    {"PLAY", {"GATE", "JUMP", "OCT", "LVL"}},
    {"EAR", {"EAR", "LSTN", "HOLD", ""}},
};
static const char *const order_name[] = {"SEQ", "UP", "DOWN", "PING", "RAND"};
static const char *const rest_short[] = {"-", "XXX.", "XX..", "X.", "X.XX"};
static const uint32_t rest_mask[] = {0xFFFFFu, 0x77777u, 0x33333u, 0x55555u, 0x6B5ADu};
static const char *const div_name[] = {"1/4", "1/8", "1/8T", "1/16", "16T"};
static const uint8_t div_per_beat[] = {1, 2, 3, 4, 6};
static const char *const tones_short[] = {"ROOT", "R+5", "TRI", "TRI8", "7TH"};
static const char *const quality_name[BASS_QUALITIES] = {"", "MIN", "7", "MIN7", "MAJ7", "SUS4", "SUS2", "DIM", "M7B5", "AUG", "5"};
static const char *const pitch_name[12] = {"C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B"};
static const char *const register_name[] = {"E1", "E2", "E3"};
/* LISTEN choices: everything, only keys/MIDI in, or one track */
static const uint16_t listen_choice[] = {BASS_LISTEN_ALL, 1U << BASS_LISTEN_KEYS, 1, 2, 4, 8, 16, 32, 64, 128};
static const char *const listen_name[] = {"ALL", "KEYS", "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8"};
enum { LISTEN_CHOICES = sizeof listen_choice / sizeof listen_choice[0] };

/* bass styles (PRESETS): a starting point for the LINE and PLAY pages */
struct style { const char *name; uint8_t order, rest, division, tones, jumps, gate; };
static const struct style styles[] = {
    {"ROOTS", 0, 0, 1, BASS_TONES_ROOT, 0, 70},
    {"PUMP", 0, 0, 3, BASS_TONES_ROOT, 0, 40},
    {"OCTAVES", 0, 0, 3, BASS_TONES_ROOT, 0, 55},
    {"WALK", 1, 0, 0, BASS_TONES_SEVENTH, 0, 90},
    {"DRIFT", 0, 1, 3, BASS_TONES_TRIAD8, 0, 60},
    {"REGGAE", 0, 2, 1, BASS_TONES_ROOT5, 0, 80},
    {"ARP", 3, 0, 3, BASS_TONES_SEVENTH, 10, 50},
    {"SCATTER", 4, 4, 3, BASS_TONES_TRIAD8, 25, 50},
};
enum { STYLES = sizeof styles / sizeof styles[0] };

static unsigned active, initialized, full_redraw, page, style;
static unsigned division = 3, tones = BASS_TONES_TRIAD8, jumps, gate = 70, level = 35, listen;
static unsigned cutoff = 45, resonance = 30, decay = 40, sub = 70;
static uint32_t prev_buttons, prev_keys, last_ms;

static void build_voice(void)
{
    /* a plucked square through a lowpass over a sine an octave down */
    float hz = 150.0f + (float)cutoff * (float)cutoff * 0.35f;      /* 150 Hz .. 3.6 kHz */
    uint16_t ms = (uint16_t)(40 + decay * 6);                        /* 40 .. 640 ms */
    struct dv_recipe top = {.wave = DV_SQUARE, .note = 40, .filter = DV_FILTER_LOW, .cutoff = (uint16_t)hz,
                            .reso = (uint8_t)(resonance * 13 / 100), .attack = 2, .decay = ms, .sustain = 3,
                            .release = 50, .gate = 200, .vol = 6, .level = 0.55f, .pitch = 0.6f, .pitch_ms = 12};
    struct dv_recipe low = {.wave = DV_SINE, .note = 28, .transpose = -12, .attack = 2, .decay = (uint16_t)(ms + 60),
                            .sustain = 5, .release = 60, .gate = 200, .vol = 6, .level = (float)sub / 100.0f};
    struct dv_voice next = {0};
    next.parts = sub ? 2 : 1;
    dv_build_part(&next.part[0], &top, 0, 1.0f);
    if (sub) dv_build_part(&next.part[1], &low, 0, 1.0f);
    bass.voice = next;
}
static void apply(void)
{
    bass.per_beat = div_per_beat[division];
    bass.tones = (uint8_t)tones;
    bass.octave_chance = jumps * 65536U / 100U;
    bass.level = (int32_t)(level * 32768U / 100U);
    bass.gate = gate * 65536U / 100U;
    bass.listen = listen_choice[listen];
}
static void load_style(unsigned index)
{
    const struct style *s = &styles[index];
    bass.order = s->order; bass.rest = s->rest;
    division = s->division; tones = s->tones; jumps = s->jumps; gate = s->gate;
    apply();
}
static void init(void)
{
    dv_build_tables();
    bass.octave = 1; bass.hold = 1; bass.window = 2;
    style = 4;   /* DRIFT: patgen's drifting gap, the reason it is here */
    load_style(style);
    build_voice();
    initialized = 1;
}

static int step(int value, int delta, int lo, int hi)
{
    int v = value + delta;
    return v < lo ? lo : v > hi ? hi : v;
}

/* ---- screen ---- */
static void note_text(int midi, char *out)
{
    strcpy(out, pitch_name[midi % 12]);
    px_number(midi / 12 - 1, out + strlen(out));
}
static void draw_roll(int top, int rows)
{
    const int low = 22;
    unsigned pos = bass_history_pos;
    for (unsigned i = 0; i < BASS_HISTORY; ++i) {
        unsigned n = bass_history[(pos + i) & (BASS_HISTORY - 1U)];
        if (n == BASS_NONE) continue;
        int y = top + rows - 1 - ((int)n - low);
        if (y < top) y = top;
        if (y > top + rows - 1) y = top + rows - 1;
        px_rect((int)i * 5 / 2, y, 2, 1, i == BASS_HISTORY - 1 ? PX_WHITE : PX_GREEN);
    }
}
static void draw(uint32_t now)
{
    char text[32], v[4][8];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    switch (page) {
    case LINE:
        strcpy(v[0], order_name[bass.order]); strcpy(v[1], rest_short[bass.rest]);
        strcpy(v[2], div_name[division]); strcpy(v[3], tones_short[tones]);
        break;
    case SOUND:
        px_number((int)cutoff, v[0]); px_number((int)resonance, v[1]); px_number((int)decay, v[2]); px_number((int)sub, v[3]);
        break;
    case PLAY:
        px_number((int)gate, v[0]); px_number((int)jumps, v[1]); strcpy(v[2], register_name[bass.octave]);
        px_number((int)level, v[3]);
        break;
    default:
        strcpy(v[0], bass.window == 1 ? "1B" : bass.window == 2 ? "2B" : "BAR");
        strcpy(v[1], listen_name[listen]); strcpy(v[2], bass.hold ? "ON" : "OFF"); v[3][0] = 0;
        break;
    }
    px_chrome("BASS", pages, PAGES, page, value);
    /* the chord it hears, and the style */
    unsigned root = bass_root;
    if (root < 12) {
        strcpy(text, pitch_name[root]);
        if (*quality_name[bass_quality]) { strcat(text, " "); strcat(text, quality_name[bass_quality]); }
    } else strcpy(text, "LISTENING");
    px_print(text, 2, 9, root < 12 ? PX_WHITE : PX_SLATE);
    if (bass.muted) px_print("MUTE", PX_W - 17, 9, PX_RED);
    else px_print(styles[style].name, PX_W - 1 - ((int)strlen(styles[style].name) * 4 - 1), 9, PX_SLATE);
    if (page == EAR) {
        /* who it hears: eight tracks and the keys, lit while they play, framed if followed */
        uint16_t heard = bass_heard; bass_heard = 0;
        static uint32_t lit_until[9];
        for (unsigned t = 0; t < 9; ++t) {
            if ((heard >> t) & 1U) lit_until[t] = now + 150U;
            int x = 2 + (int)t * 8, y = 22;
            unsigned followed = (bass.listen >> t) & 1U, lit = (int32_t)(lit_until[t] - now) > 0;
            px_rect(x, y, 6, 6, followed ? PX_DUSK : PX_NIGHT);
            if (lit) px_rect(x + 1, y + 1, 4, 4, followed ? PX_GREEN : PX_SLATE);
            text[0] = t < 8 ? (char)('1' + t) : 'K'; text[1] = 0;
            px_print(text, x + 1, y + 8, followed ? PX_WHITE : PX_SLATE);
        }
        px_print("CHORD EVERY", 2, 42, PX_SLATE);
        px_print(bass.window == 1 ? "BEAT" : bass.window == 2 ? "2 BEATS" : "BAR", 50, 42, PX_GOLD);
        px_print("REC HOLD  FX MUTE", 2, 52, PX_SLATE);
    } else {
        /* LINE / SOUND / PLAY: the rest mask, the latched notes, the line */
        unsigned at = bass_rest_pos;
        for (unsigned i = 0; i < 20; ++i) {
            unsigned plays = (rest_mask[bass.rest] >> i) & 1U;
            px_rect(1 + (int)i * 4, 16, 3, plays ? 2 : 1, i == at && bass_running ? PX_WHITE : plays ? PX_GOLD : PX_DUSK);
        }
        text[0] = 0;
        for (unsigned i = 0; i < bass_latched_n && i < 5; ++i) {
            char name[8]; note_text(bass_latched[i], name);
            if (i) strcat(text, " ");
            strcat(text, name);
        }
        px_print(*text ? text : "-", 2, 20, PX_CYAN);
        draw_roll(28, 36);
    }
    px_flash_draw(now);
}

unsigned bass_active(void) { return active; }

int bass_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys, up = prev_keys & ~c->keys;
    unsigned toggle = px_mode_toggle(pressed, PX_MODE_BASS, active);
    uint32_t held_keys = prev_keys;
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!toggle) return 0;
        if (!initialized) init();
        bass.armed = 1;
        px_leds_off();
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash(pages[page].name, now, 700U);
        return 1;
    }
    if (toggle) {   /* HOME: back to the launcher, letting go of any chord we were holding */
        for (unsigned k = 0; k < PX_KEYS; ++k)
            if ((held_keys >> k) & 1U) { const uint8_t off[3] = {0x80, (uint8_t)(PX_FIRST_NOTE + k), 0}; (void)groove_midi_local(off, 3); }
        px_leds_off(); active = 0;
        return 2;
    }
    /* the keys play chords on track 1; the bass hears them through groove_midi_local */
    for (unsigned k = 0; k < PX_KEYS; ++k) {
        if ((down >> k) & 1U) { const uint8_t on[3] = {0x90, (uint8_t)(PX_FIRST_NOTE + k), 100}; (void)groove_midi_local(on, 3); fm1_led_set(PX_KEY_LED + k, 1); }
        if ((up >> k) & 1U) { const uint8_t off[3] = {0x80, (uint8_t)(PX_FIRST_NOTE + k), 0}; (void)groove_midi_local(off, 3); fm1_led_set(PX_KEY_LED + k, 0); }
    }
    const int16_t *e = c->encoders;
    unsigned changed = 0, rebuild = 0;
    /* SELECT: pages */
    if (e[0]) {
        page = (unsigned)step((int)page, e[0], 0, PAGES - 1);
        px_flash(pages[page].name, now, 700U); changed = 1;
    }
    /* PRESETS: bass styles */
    if (e[6]) {
        style = (unsigned)(((int)style + e[6] + STYLES * 8) % STYLES);
        load_style(style); px_flash(styles[style].name, now, 700U); changed = 1;
    }
    /* KNOBS 1-4: this page's four parameters */
    const int16_t k1 = e[2], k2 = e[3], k3 = e[4], k4 = e[5];
    if (k1 || k2 || k3 || k4) {
        switch (page) {
        case LINE:
            if (k1) bass.order = (uint8_t)step(bass.order, k1, 0, 4);
            if (k2) bass.rest = (uint8_t)step(bass.rest, k2, 0, 4);
            if (k3) division = (unsigned)step((int)division, k3, 0, 4);
            if (k4) tones = (unsigned)step((int)tones, k4, 0, BASS_TONE_SETS - 1);
            break;
        case SOUND:
            cutoff = (unsigned)step((int)cutoff, k1 * 3, 0, 100);
            resonance = (unsigned)step((int)resonance, k2 * 5, 0, 100);
            decay = (unsigned)step((int)decay, k3 * 5, 0, 100);
            sub = (unsigned)step((int)sub, k4 * 5, 0, 100);
            rebuild = 1;
            break;
        case PLAY:
            gate = (unsigned)step((int)gate, k1 * 5, 5, 100);
            jumps = (unsigned)step((int)jumps, k2 * 5, 0, 100);
            if (k3) bass.octave = (uint8_t)step(bass.octave, k3, 0, 2);
            level = (unsigned)step((int)level, k4 * 4, 0, 100);
            break;
        default:
            if (k1) bass.window = (uint8_t)(k1 > 0 ? (bass.window == 1 ? 2 : 4) : (bass.window == 4 ? 2 : 1));
            if (k2) listen = (unsigned)step((int)listen, k2, 0, LISTEN_CHOICES - 1);
            if (k3) bass.hold = k3 > 0;
            break;
        }
        changed = 1;
    }
    if ((pressed & PX_BIT(PX_OCT_DOWN)) && bass.octave > 0) { --bass.octave; changed = 1; }
    if ((pressed & PX_BIT(PX_OCT_UP)) && bass.octave < 2) { ++bass.octave; changed = 1; }
    if (pressed & PX_BIT(PX_FX)) { bass.muted ^= 1U; px_flash(bass.muted ? "MUTE" : "UNMUTE", now, 600U); changed = 1; }
    if (pressed & PX_BIT(PX_REC)) { bass.hold ^= 1U; px_flash(bass.hold ? "HOLD" : "FREE", now, 600U); changed = 1; }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0};
        groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (changed) apply();
    if (rebuild) build_voice();
    if (now - last_ms >= 33U || changed || rebuild) {
        last_ms = now;
        draw(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
