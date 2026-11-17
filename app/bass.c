/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* BASS PLAYER panel mode. HOME+EDIT opens and closes it; once armed, the bass
 * keeps following and playing after you leave.
 *
 *   SELECT      patgen order: SEQ (root first) UP DOWN PING RAND
 *   PRESETS     rest mask: none, XXX., XX.., X., X.XX.X.XX.
 *   KNOB 1      division: 1/4 1/8 1/8T 1/16 1/16T
 *   KNOB 2      tones: ROOT, ROOT 5, TRIAD, TRIAD 8, 7TH
 *   KNOB 3      octave jumps 0..100%       KNOB 4    tone (filter)
 *   ALGORITHM   level      OCT -/+   register (E1 E2 E3)
 *   FX          mute       REC       hold the last chord through silence
 *   SEQ         listening window per chord: 1 beat, 2 beats, a bar
 *   PLAY        FM-1 play/stop
 *   keys        play chords on track 1: the bass hears them like anything else */
#include "bass.h"
#include "pixel.h"
#include "instrument.h"
#include <string.h>

static const char *const order_name[] = {"SEQ", "UP", "DOWN", "PING", "RAND"};
static const char *const rest_name[] = {"NO RESTS", "XXX.", "XX..", "X.", "X.XX.X.XX."};
static const uint32_t rest_mask[] = {0xFFFFFu, 0x77777u, 0x33333u, 0x55555u, 0x6B5ADu};
static const char *const div_name[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T"};
static const uint8_t div_per_beat[] = {1, 2, 3, 4, 6};
static const char *const tones_name[] = {"ROOT", "ROOT 5", "TRIAD", "TRIAD 8", "7TH"};
static const char *const quality_name[BASS_QUALITIES] = {"", "MIN", "7", "MIN7", "MAJ7", "SUS4", "SUS2", "DIM", "M7B5", "AUG", "5"};
static const char *const pitch_name[12] = {"C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B"};

static unsigned active, initialized, full_redraw, division = 3, tones = BASS_TONES_TRIAD8, jumps = 0, tone = 45, level = 35;
static uint32_t prev_buttons, prev_keys, last_ms, notice_until;
static const char *notice;

static void build_voice(void)
{
    /* a plucked square through a lowpass over a sine an octave down */
    float cutoff = 150.0f + (float)tone * (float)tone * 0.35f;   /* 150 Hz .. 3.6 kHz */
    struct dv_recipe top = {.wave = DV_SQUARE, .note = 40, .filter = DV_FILTER_LOW, .cutoff = (uint16_t)cutoff, .reso = 5,
                            .attack = 2, .decay = 160, .sustain = 3, .release = 50, .gate = 200, .vol = 6, .level = 0.55f,
                            .pitch = 0.6f, .pitch_ms = 12};
    struct dv_recipe sub = {.wave = DV_SINE, .note = 28, .transpose = -12, .attack = 2, .decay = 220, .sustain = 5,
                            .release = 60, .gate = 200, .vol = 6, .level = 0.70f};
    struct dv_voice next = {0};
    next.parts = 2;
    dv_build_part(&next.part[0], &top, 0, 1.0f);
    dv_build_part(&next.part[1], &sub, 0, 1.0f);
    bass.voice = next;
}
static void apply(void)
{
    bass.per_beat = div_per_beat[division];
    bass.tones = (uint8_t)tones;
    bass.octave_chance = jumps * 65536U / 100U;
    bass.level = (int32_t)(level * 32768U / 100U);
    bass.gate = (uint32_t)(0.70f * 65536.0f);
}
static void init(void)
{
    dv_build_tables();
    bass.order = 0; bass.rest = 0; bass.octave = 1; bass.hold = 1; bass.window = 2;
    apply();
    build_voice();
    initialized = 1;
}

/* ---- screen ---- */
static void note_text(int midi, char *out)
{
    strcpy(out, pitch_name[midi % 12]);
    px_number(midi / 12 - 1, out + strlen(out));
}
static void draw(uint32_t now)
{
    char text[32];
    px_cls(PX_NIGHT);
    px_print("BASS", 2, 1, PX_GOLD);
    px_number((int)groove_tempo_current(), text);
    px_print(text, PX_W - 2 - ((int)strlen(text) * 4 - 1), 1, PX_SLATE);
    /* the chord it hears */
    unsigned root = bass_root;
    if (root < 12) {
        strcpy(text, pitch_name[root]);
        if (*quality_name[bass_quality]) { strcat(text, " "); strcat(text, quality_name[bass_quality]); }
    } else strcpy(text, "LISTENING");
    px_print(text, 2, 8, root < 12 ? PX_WHITE : PX_SLATE);
    if (notice && (int32_t)(notice_until - now) > 0) px_print(notice, 40, 8, PX_WHITE);
    else if (bass.muted) px_print("MUTE", PX_W - 17, 8, PX_RED);
    else if (bass.hold) px_print("HOLD", PX_W - 17, 8, PX_SLATE);
    /* order + the rest mask, with patgen's place in it */
    strcpy(text, order_name[bass.order]); strcat(text, " "); strcat(text, rest_name[bass.rest]);
    px_print(text, 2, 15, PX_EMBER);
    unsigned at = bass_rest_pos;
    for (unsigned i = 0; i < 20; ++i) {
        unsigned plays = (rest_mask[bass.rest] >> i) & 1U;
        unsigned colour = i == at && bass_running ? PX_WHITE : plays ? PX_GOLD : PX_DUSK;
        px_rect(1 + (int)i * 4, 22, 3, plays ? 2 : 1, colour);
    }
    /* the latched notes, in press order */
    text[0] = 0;
    for (unsigned i = 0; i < bass_latched_n && i < 5; ++i) {
        char name[8]; note_text(bass_latched[i], name);
        if (i) strcat(text, " ");
        strcat(text, name);
    }
    px_print(*text ? text : "-", 2, 26, PX_CYAN);
    strcpy(text, div_name[division]); strcat(text, " "); strcat(text, tones_name[tones]);
    px_print(text, 2, 33, PX_SLATE);
    strcpy(text, "JUMP "); px_number((int)jumps, text + 5); strcat(text, " TONE "); px_number((int)tone, text + strlen(text));
    strcat(text, bass.window == 1 ? " E1" : bass.window == 2 ? " E2" : " E4");
    px_print(text, 2, 39, PX_SLATE);
    /* the line it played: newest on the right, one row per semitone */
    const int top = 46, rows = 32, low = 22;
    px_rect(0, top - 1, PX_W, 1, PX_DUSK);
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

unsigned bass_active(void) { return active; }

int bass_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys, up = prev_keys & ~c->keys;
    unsigned combo = px_combo(c->buttons, pressed, PX_HOME, PX_EDIT);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!combo) return 0;
        if (!initialized) init();
        bass.armed = 1;
        px_leds_off();
        active = 1; full_redraw = 1; last_ms = now - 100U;
        notice = "HOME EDIT EXIT"; notice_until = now + 1500U;
        return 1;
    }
    if (combo) {
        for (unsigned k = 0; k < PX_KEYS; ++k)   /* let go of any chord we were holding */
            if ((prev_keys >> k) & 1U) { const uint8_t off[3] = {0x80, (uint8_t)(PX_FIRST_NOTE + k), 0}; (void)groove_midi_local(off, 3); }
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
    if (e[0]) { bass.order = (uint8_t)((bass.order + e[0] + 5 * 8) % 5); changed = 1; }
    if (e[6]) { bass.rest = (uint8_t)((bass.rest + e[6] + 5 * 8) % 5); changed = 1; }
    if (e[2]) { int d = (int)division + e[2]; division = (unsigned)(d < 0 ? 0 : d > 4 ? 4 : d); changed = 1; }
    if (e[3]) { int t = (int)tones + e[3]; tones = (unsigned)(t < 0 ? 0 : t >= BASS_TONE_SETS ? BASS_TONE_SETS - 1 : t); changed = 1; }
    if (e[4]) { int j = (int)jumps + e[4] * 5; jumps = (unsigned)(j < 0 ? 0 : j > 100 ? 100 : j); changed = 1; }
    if (e[5]) { int t = (int)tone + e[5] * 3; tone = (unsigned)(t < 0 ? 0 : t > 100 ? 100 : t); rebuild = 1; }
    if (e[1]) { int l = (int)level + e[1] * 4; level = (unsigned)(l < 0 ? 0 : l > 100 ? 100 : l);
                notice = "LEVEL"; notice_until = now + 600U; changed = 1; }
    if ((pressed & PX_BIT(PX_OCT_DOWN)) && bass.octave > 0) { --bass.octave; changed = 1; }
    if ((pressed & PX_BIT(PX_OCT_UP)) && bass.octave < 2) { ++bass.octave; changed = 1; }
    if (pressed & PX_BIT(PX_FX)) { bass.muted ^= 1U; changed = 1; }
    if (pressed & PX_BIT(PX_REC)) { bass.hold ^= 1U; changed = 1; }
    if (pressed & PX_BIT(PX_SEQ)) {
        bass.window = bass.window == 1 ? 2 : bass.window == 2 ? 4 : 1;
        notice = bass.window == 1 ? "EAR 1 BEAT" : bass.window == 2 ? "EAR 2 BEATS" : "EAR 1 BAR";
        notice_until = now + 900U; changed = 1;
    }
    if (pressed & PX_BIT(PX_PLAY)) {
        struct groove_view view = {0};
        groove_get_view(0, 0, &view);
        const uint8_t command = view.playing ? 0xfc : 0xfb;
        (void)groove_midi_local(&command, 1);
        changed = 1;
    }
    if (changed || rebuild) apply();
    if (rebuild) build_voice();
    if (now - last_ms >= 33U || changed || rebuild) {
        last_ms = now;
        draw(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
