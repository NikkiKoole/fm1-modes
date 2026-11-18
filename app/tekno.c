/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* TEKNO: 90s free-party tekno, played live (so far: the kicks, the toms, the phaser).
 *
 *   black key 1       RUMBLE on / off: the rumble kick on every beat, its long tuned tail the bass
 *   black key 2       ROLL on / off: a short kick on the beat, bass notes rolling between (RUMBLE
 *                     and ROLL are one at a time: pressing one swaps them at the next bar)
 *   black key 3       TOMS on / off: two tuned toms rolling on the sixteenths
 *   white keys 1-4    the toms' pattern: RUN, GALLOP, TRIBE, SPARSE
 *   SELECT            pages: KICK (TUNE PUNCH DECAY DRIVE) · ROLL (FEEL DECAY CUTOFF DRIVE)
 *                     · TOMS (TUNE GAP DECAY LEVEL)
 *                     · PHASER (DEPTH BARS FDBK MIX) · MIX (TEMPO LEVEL TONE CLICK)
 *   PLAY              play / stop (the first layer switched on starts it)
 *   HOME              back (it keeps playing)
 * Layers and patterns change on the next bar (their keys blink until then).
 *
 * The sound is set from three references (French hardtek / Czech tribe, 155-174 BPM): the kick's
 * low band holds almost flat for the whole beat, the tail tuned (A1), its 2nd and 3rd harmonics
 * ~22 dB down; the toms a whole tone apart (G#3 / F#3) on every sixteenth; a phaser on the
 * percussion sweeping once every four bars. */
#include "tekno.h"
#include "pixel.h"
#include "drumvoice.h"
#include "instrument.h"
#include "cr78.h"
#include <string.h>

enum { PAGES = 5, PATTERNS = 4, FEELS = 4, LAYERS = 3 };
static const struct px_page pages[PAGES] = {
    {"KICK", {"TUNE", "PNCH", "DCAY", "DRV"}},
    {"ROLL", {"FEEL", "DCAY", "CUT", "DRV"}},
    {"TOMS", {"TUNE", "GAP", "DCAY", "LVL"}},
    {"PHASER", {"DPTH", "BARS", "FDBK", "MIX"}},
    {"MIX", {"TMPO", "KICK", "TONE", "CLK"}},
};
/* knob values: KICK note (MIDI) punch decay drive; ROLL feel(0-3) decay cutoff drive; TOMS note gap decay level;
 * PHASER depth bars(0-3) fb mix; MIX - level tone click */
enum { P_KICK, P_ROLL, P_TOMS, P_PHASER, P_MIX };
static int16_t knob[PAGES][4] = {{33, 60, 15, 26}, {0, 40, 45, 40}, {56, 2, 45, 45}, {80, 2, 50, 50}, {0, 62, 62, 30}};
static const int16_t lo[PAGES][4] = {{28, 0, 0, 0}, {0, 0, 0, 0}, {48, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
static const int16_t hi[PAGES][4] = {{39, 100, 100, 100}, {3, 100, 100, 100}, {72, 12, 100, 100}, {100, 3, 95, 100}, {0, 100, 100, 100}};
/* ROLL's feels: which sixteenths after the kick get a bass note (bit 1 e, 2 &, 3 a) */
static const struct { const char *name; uint8_t mask; } feels[FEELS] = {
    {"GALLOP", 0x0C}, {"ROLL", 0x0E}, {"OFFBEAT", 0x04}, {"PUSH", 0x08}};
/* the layers on the black keys, in order */
static const uint8_t layer_bit[LAYERS] = {TEKNO_KICK, TEKNO_ROLL, TEKNO_TOMS};
static const char *const layer_name[LAYERS] = {"RMBL", "ROLL", "TOMS"};
static const uint8_t layer_color[LAYERS] = {PX_EMBER, PX_GOLD, PX_CYAN};
/* the toms: H / L high / low, upper case accented, '.' rest */
static const struct { const char *name, *steps; } patterns[PATTERNS] = {
    {"RUN", "HlhlHlhlHlhlHlhl"},
    {"GALLOP", "H.hlH.hlH.hlHlhl"},
    {"TRIBE", "L.hlh.HlL.hlhlHh"},
    {"SPARSE", "..h...l...h..hl."},
};
static const char *const pad_name[PX_PADS] = {"RMB", "RLL", "TOM", "", "", "", "", "", "", "", ""};
static const char *const note_name[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[PX_PADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

static unsigned active, opened_once, page, pattern, pattern_playing, full_redraw;
static uint32_t prev_buttons, prev_keys, last_ms, led_state;
static int clamp(int v, int lo_, int hi_) { return v < lo_ ? lo_ : v > hi_ ? hi_ : v; }

/* ---- control-side maths (floats are fine here) ---- */
static float exp2f_(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float expf_(float x) { return exp2f_(x * 1.4426950f); }
static float tanhf_(float x) { float e = expf_(2.0f * x); return (e - 1.0f) / (e + 1.0f); }
static int32_t q30(float x) { return (int32_t)(x * 1073741824.0f); }
static int32_t per_sample(float ms) { return q30(expf_(-1.0f / (ms * 44.1f))); }          /* a decay of time constant ms */
static int32_t per_16(float ms) { return q30(expf_(-16.0f / (ms * 44.1f))); }             /* ... applied every 16 samples */
static int32_t lowpass(float hz) { return (int32_t)((1.0f - expf_(-6.2831853f * hz / 44100.0f)) * 32768.0f); }

/* the knobs into the audio side's words */
static void apply(void)
{
    struct tekno_params *t = &tekno;
    const int16_t *k = knob[0];
    t->kick_inc = dv.note_inc[k[0]];
    t->kick_pitch = (int32_t)((24.0f + 0.24f * (float)k[1]) * 65536.0f);              /* 24..48 semitones above */
    t->kick_pitch_k = per_16(6.0f + 0.3f * (float)k[1]);                                /* 6..36 ms */
    t->kick_decay_k = k[2] ? per_sample(60.0f * exp2f_((float)(100 - k[2]) * 0.07f)) : (1 << 30);   /* flat .. 60 ms */
    if (k[3]) {
        float d = (float)k[3] / 100.0f, g = d * d * 24.0f;
        t->kick_drive_norm = (int32_t)(32768.0f / tanhf_(g));
        t->kick_drive_neg = (int32_t)(g * 0.4f * 4096.0f);   /* lopsided: the 2nd harmonic as loud as the 3rd (as measured) */
        t->kick_drive_pos = (int32_t)(g * 4096.0f);
    } else t->kick_drive_pos = 0;
    k = knob[P_ROLL];
    t->roll_feel = feels[k[0]].mask;
    t->roll_decay_k = per_sample(15.0f + 1.6f * (float)k[1]);                           /* 15..175 ms */
    {
        float hz = 80.0f * exp2f_((float)k[2] * 0.055f), x = 3.1415927f * hz / 44100.0f;   /* 80 Hz .. 3.6 kHz */
        t->roll_f = (int32_t)(2.0f * (x - x * x * x / 6.0f) * 32768.0f);
        t->roll_damp = (int32_t)(1.1f * 32768.0f);
    }
    if (k[3]) {
        float g = 1.0f + (float)k[3] * 0.08f;
        t->roll_drive_norm = (int32_t)(32768.0f / tanhf_(g));
        t->roll_drive = (int32_t)(g * 4096.0f);
    } else t->roll_drive = 0;
    t->roll_level = 20000;
    k = knob[P_TOMS];
    t->tom_inc[0] = dv.note_inc[k[0]];
    t->tom_inc[1] = dv.note_inc[clamp(k[0] - k[1], 0, 127)];
    t->tom_pitch = 5 * 65536;                                                           /* a small drop onto the note */
    t->tom_pitch_k = per_16(15.0f);
    t->tom_decay_k = per_sample(20.0f + 2.8f * (float)k[2]);                            /* 20..300 ms */
    t->tom_level = k[3] * 110;
    t->tom_skin = 9000;
    k = knob[P_PHASER];
    t->ph_depth = k[0] * 327;
    t->ph_bars_shift = (uint8_t)k[1];
    t->ph_feedback = k[2] * 327;
    t->ph_mix = k[3] * 327;
    k = knob[P_MIX];
    t->kick_level = k[1] * 300;
    t->kick_tone = lowpass(120.0f * exp2f_((float)k[2] * 0.06f));                       /* 120 Hz .. 7.7 kHz */
    t->kick_click = k[3] * 200;
}
static void load_pattern(unsigned p)
{
    const char *s = patterns[p].steps;
    for (unsigned i = 0; i < TEKNO_STEPS; ++i) {
        char c = s[i];
        tekno.toms_want[i] = (uint8_t)(c == 'H' || c == 'h' ? 1 : c == 'L' || c == 'l' ? 2 : 0) | (c == 'H' || c == 'L' ? 4 : 0);
    }
    tekno.toms_dirty = 1;
    pattern = p;
}
static void start_if_stopped(void)
{
    struct groove_view view = {0};
    groove_get_view(0, 0, &view);
    if (!view.playing) { const uint8_t command = 0xfb; (void)groove_midi_local(&command, 1); }
}
unsigned tekno_active(void) { return active; }

/* ---- the screen ---- */
static void value_text(unsigned pg, unsigned i, char *out)
{
    int v = knob[pg][i];
    if ((pg == P_KICK || pg == P_TOMS) && i == 0) { strcpy(out, note_name[v % 12]); px_number(v / 12 - 1, out + strlen(out)); }
    else if (pg == P_ROLL && i == 0) strcpy(out, feels[v].name);
    else if (pg == P_PHASER && i == 1) px_number(1 << v, out);
    else if (pg == P_MIX && i == 0) px_number((int)groove_tempo_current(), out);
    else if (pg == P_KICK && i == 2 && !v) strcpy(out, "FLAT");
    else px_number(v, out);
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    for (unsigned i = 0; i < 4; ++i) value_text(page, i, v[i]);
    unsigned blink = (now / 140U) & 1U;
    uint32_t lit = 0;   /* the layers in the black keys' order, waiting ones blinking */
    for (unsigned l = 0; l < LAYERS; ++l) {
        unsigned on = (tekno_layers & layer_bit[l]) != 0, will = (tekno.want & layer_bit[l]) != 0;
        if (on != will && blink) on = !on;
        lit |= on << l;
    }
    px_cls(PX_NIGHT);
    px_chrome_pads("TEKNO", pages, PAGES, page, value, pad_name, lit);
    px_print("TEKNO", 2, 1, PX_GOLD);
    /* the layers */
    for (unsigned l = 0; l < LAYERS; ++l) {
        unsigned on = (lit >> l) & 1U;
        int x = 1 + (int)l * 26;
        px_rect(x, 9, 25, 7, on ? layer_color[l] : PX_DUSK);
        px_print(layer_name[l], x + 5, 10, on ? PX_NIGHT : PX_SLATE);
    }
    /* the toms' pattern: high above, low below, accents tall; the playhead */
    const char *steps = patterns[pattern].steps;
    unsigned step = tekno_step;
    for (unsigned s = 0; s < 16; ++s) {
        int x = 2 + (int)s * 5;
        char c = steps[s];
        unsigned acc = c == 'H' || c == 'L', high = c == 'H' || c == 'h', any = c != '.';
        unsigned color = cr78_running && s == step ? PX_WHITE : (s & 3U) ? PX_SLATE : PX_GOLD;
        if (any) px_rect(x, high ? (acc ? 20 : 22) : 28, 4, acc ? 6 : 4, high ? PX_CYAN : PX_GREEN);
        px_rect(x, 34, 4, 1, color);
    }
    char line[24];
    strcpy(line, "TOMS "); strcat(line, patterns[pattern].name);
    if (pattern != pattern_playing && blink) line[0] = 0;
    px_print(line, 2, 37, PX_CYAN);
    /* the phaser's sweep, and the eight bars */
    int w = (int)((uint32_t)tekno_lfo * 76U >> 16);
    px_rect(2, 45, 76, 3, PX_DUSK);
    px_rect(2 + w, 44, 2, 5, PX_PINK);
    for (unsigned b = 0; b < 8; ++b) px_rect(2 + (int)b * 5, 52, 4, 3, cr78_running && b == tekno_bar ? PX_WHITE : PX_DUSK);
    strcpy(line, ""); px_number((int)groove_tempo_current(), line); strcat(line, " BPM");
    px_print(line, 46, 51, PX_SLATE);
    px_flash_draw(now);
}
static void leds(uint32_t now)
{
    unsigned blink = (now / 140U) & 1U;
    uint32_t layers = 0;
    for (unsigned l = 0; l < LAYERS; ++l) {
        unsigned on = (tekno_layers & layer_bit[l]) != 0, will = (tekno.want & layer_bit[l]) != 0;
        if (on != will && blink) on = !on;
        layers |= on << l;
    }
    uint32_t whites = 1U << pattern;
    if (pattern != pattern_playing && blink) whites = 1U << pattern_playing;
    uint32_t all = whites | (layers << 16);
    for (unsigned s = 0; s < 16; ++s)
        if (((all ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (all >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((all ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (all >> (16 + i)) & 1U);
    led_state = all;
}

/* ---- the panel ---- */
int tekno_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons, down = c->keys & ~prev_keys;
    unsigned home = px_mode_toggle(pressed, PX_MODE_TEKNO, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!home) return 0;
        dv_build_tables();
        if (!opened_once) {   /* the first time: the ratios, the sound, a tempo of the scene */
            for (unsigned i = 0; i < TEKNO_RATIOS; ++i) tekno.ratio[i] = (uint32_t)(exp2f_((float)i / 96.0f) * 65536.0f);
            load_pattern(0); pattern_playing = 0;
            groove_tempo_edit(165 - (int)groove_tempo_current());
            opened_once = 1;
        }
        apply();
        px_leds_off(); led_state = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U;
        px_flash("TEKNO", now, 700U);
        return 1;
    }
    if (home) { px_leds_off(); active = 0; return 2; }
    unsigned changed = 0;
    if (tekno_step == 0 || !cr78_running) {   /* (the audio side took the pattern at the bar) */
        if (!tekno.toms_dirty && pattern_playing != pattern) { pattern_playing = pattern; changed = 1; }
    }
    /* black keys: the layers */
    for (unsigned i = 0; i < LAYERS; ++i) {
        if (!((down >> black_keys[i]) & 1U)) continue;
        uint8_t bit = layer_bit[i], want = tekno.want;
        if (want & bit) want = (uint8_t)(want & ~bit);
        else if (bit & (TEKNO_KICK | TEKNO_ROLL)) want = (uint8_t)((want & ~(TEKNO_KICK | TEKNO_ROLL)) | bit);   /* one kick at a time */
        else want = (uint8_t)(want | bit);
        tekno.want = want;
        tekno.engaged = want != 0;
        if (want & bit) start_if_stopped();
        px_flash(i == 0 ? "RUMBLE" : i == 1 ? "ROLL" : "TOMS", now, 400U);
        changed = 1;
    }
    /* white keys 1-4: the toms' pattern */
    for (unsigned s = 0; s < PATTERNS; ++s) {
        if (!((down >> white_keys[s]) & 1U)) continue;
        load_pattern(s);
        px_flash(patterns[s].name, now, 500U);
        changed = 1;
    }
    /* SELECT: pages */
    if (c->encoders[0]) {
        page = (unsigned)(((int)page + c->encoders[0] + 4 * PAGES) % PAGES);
        px_flash(pages[page].name, now, 500U);
        changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t *k = c->encoders + 2;
    for (unsigned i = 0; i < 4; ++i) {
        if (!k[i]) continue;
        if (page == P_MIX && i == 0) groove_tempo_edit(k[i]);
        else {
            int step = (hi[page][i] - lo[page][i]) > 20 ? 2 : 1;
            knob[page][i] = (int16_t)clamp(knob[page][i] + k[i] * step, lo[page][i], hi[page][i]);
        }
        apply();
        static char text[24];
        strcpy(text, pages[page].knob[i]); strcat(text, " ");
        value_text(page, i, text + strlen(text));
        px_flash(text, now, 700U);
        changed = 1;
    }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0}; groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (now - last_ms >= 33U || changed) {
        last_ms = now;
        draw(now);
        leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
