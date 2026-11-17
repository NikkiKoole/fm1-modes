/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX panel mode (GROOVEBOX-SPEC.md). Three levels:
 *
 *   LAUNCHER  --ALGORITHM-->  OVERVIEW  --ALGORITHM-->  TRACK (DRUMS BASS KEYS LEAD)
 *             <----HOME----             <----HOME----
 *
 * ALGORITHM moves along OVERVIEW, DRUMS, BASS, KEYS, LEAD; HOME is always one level up.
 * The same rules on every track: white keys its steps (ENV: accent), black keys its parts
 * (DRUMS: the voice), hold a step + a knob: that step (DRUMS: KNOB 1 ratchet, KNOB 2 accent),
 * hold a step + a black key: that step's hit, KNOB 1-4 its sound (SELECT: pages), REC: live
 * playing goes into the steps, FX tapped: mute, FX held + a black key: throw an effect, on
 * this track (in the overview: on the mix; REC too: latch).
 *
 * Overview: four lanes; black keys 1-4 mute the tracks; white keys 1-8 the drum patterns (for
 * now); knobs MIX (TEMPO SWING FILTER PUMP), FX (DRIVE CRUSH GATE SWEEP), LEVELS. */
#include "gbx.h"
#include "pixel.h"
#include "cr78.h"
#include "fx.h"
#include "instrument.h"
#include <string.h>

enum { T_DRUMS, T_BASS, T_KEYS, T_LEAD, TRACKS, OVERVIEW = -1 };
static const char *const track_name[TRACKS] = {"DRUMS", "BASS", "KEYS", "LEAD"};
static const char *const track_short[TRACKS] = {"DRM", "BAS", "KEY", "LED"};
static const uint8_t track_color[TRACKS] = {PX_EMBER, PX_GREEN, PX_CYAN, PX_PINK};
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_keys[PX_PADS] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* ---- the overview's knobs ---- */
enum { K_NUM, K_TEMPO, K_FILTER, K_OFF0 };
struct knob { const char *label; uint8_t kind; int16_t lo, hi, step; };
enum { MIX_PAGES = 3 };
static const struct px_page mix_page[MIX_PAGES] = {
    {"MIX", {"TMPO", "SWNG", "FLTR", "PUMP"}}, {"FX", {"DRV", "CRSH", "GATE", "SWP"}},
    {"LEVELS", {"DRM", "BAS", "KEY", "LED"}}};
static const struct knob mix_knob[MIX_PAGES][4] = {
    {{"TMPO", K_TEMPO, 0, 0, 1}, {"SWNG", K_NUM, 50, 66, 2}, {"FLTR", K_FILTER, -100, 100, 4}, {"PUMP", K_OFF0, 0, 100, 5}},
    {{"DRV", K_OFF0, 0, 100, 5}, {"CRSH", K_OFF0, 0, 100, 5}, {"GATE", K_OFF0, 0, 100, 5}, {"SWP", K_OFF0, 0, 100, 5}},
    {{"DRM", K_NUM, 0, 100, 4}, {"BAS", K_NUM, 0, 100, 4}, {"KEY", K_NUM, 0, 100, 4}, {"LED", K_NUM, 0, 100, 4}}};
static int16_t mix_val[MIX_PAGES][4] = {{0, 50, 0, 0}, {0, 0, 0, 0}, {40, 70, 70, 70}};

/* ---- the pads (FX held), the same eleven everywhere ---- */
enum { P_R16, P_R32, P_HLF, P_REV, P_LPF, P_HPF, P_CRS, P_GAT, P_DRV, P_PMP, P_SWP };
static const char *const pad_name[PX_PADS] = {"R16", "R32", "HLF", "REV", "LPF", "HPF", "CRS", "GAT", "DRV", "PMP", "SWP"};
enum { SCOPE_MIX, SCOPE_DRUMS, SCOPES };
static uint32_t pads_held[SCOPES], pads_latched[SCOPES];

static unsigned active, full_redraw, voice, fx_held, fx_used, rec, muted[TRACKS];
static int zoom = OVERVIEW;                  /* OVERVIEW or the track zoomed into */
static unsigned page[TRACKS + 1];             /* each view's SELECT page: [0] the overview */
static uint32_t prev_buttons, prev_keys, last_ms, led_state;
static uint16_t held_steps;

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float exp2_approx(float x)
{
    int whole = (int)(x < 0 ? x - 1.0f : x);
    float f = x - (float)whole, r = 1.0f;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    for (; whole > 0; --whole) r *= 2.0f;
    for (; whole < 0; ++whole) r *= 0.5f;
    return r * p;
}
static float sin_small(float x) { float x2 = x * x; return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f))); }
static float tanh_approx(float x) { float e = exp2_approx(2.0f * x * 1.442695f); return (e - 1.0f) / (e + 1.0f); }
static int32_t svf_f(float hz) { if (hz > 14000.0f) hz = 14000.0f; return (int32_t)(2.0f * sin_small(3.1415927f * hz / 88200.0f) * 32768.0f); }
static void set_drive(struct fx_perf *p, float amount)
{
    float dr = 0.15f + 0.85f * amount, g = dr * dr * 24.0f;
    p->drive_norm = (int32_t)(32768.0f * 0.2f / tanh_approx(0.2f * g));
    p->drive_neg = (int32_t)(g * (1.0f - 0.4f * dr) * 4096.0f); p->drive_pos = (int32_t)(g * 4096.0f);
    p->drive_mix = 32768; p->drive_level = 32768;
}

/* the pads: a fixed throw each, on top of whatever is set; returns the effect bits they add */
static int32_t throw_pads(struct fx_perf *p, uint32_t pads)
{
    int32_t on = 0;
    if (pads & (1U << P_LPF)) { p->lpf_f = svf_f(580.0f); p->lpf_damp = 28800; on |= PERF_LPF; }
    if (pads & (1U << P_HPF)) { p->hpf_f = svf_f(600.0f); p->hpf_damp = 33000; on |= PERF_HPF; }
    if (pads & (1U << P_CRS)) { p->crush_step = 1 << (24 - 6); p->crush_rate = 4; p->crush_mix = 32768; on |= PERF_CRUSH; }
    if (pads & (1U << P_GAT)) { p->gate_shift = 0; p->gate_len = 32768; p->gate_floor = 0; on |= PERF_GATE; }
    if (pads & (1U << P_DRV)) { set_drive(p, 0.7f); on |= PERF_DRIVE; }
    if (pads & (1U << P_PMP)) { p->pump_amount = 26214; p->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (pads & (1U << P_SWP)) { p->sweep_inc = (int32_t)(uint32_t)(0.5f * 97391.55f); p->sweep_depth = 22938; p->sweep_fb = 16384; p->sweep_mix = 19661; on |= PERF_SWEEP; }
    return on;
}
/* everything that sounds: the mix knobs + the mix's pads on the master; the drum pads on the drum bus */
static void apply(void)
{
    struct fx_perf *m = &fx_perf;
    const int16_t filter = mix_val[0][2], pump = mix_val[0][3];
    int32_t on = 0;
    if (filter < -2) { m->lpf_f = svf_f(150.0f * exp2_approx((float)(100 + filter) * 0.069f)); m->lpf_damp = 34000; on |= PERF_LPF; }
    else if (filter > 2) { m->hpf_f = svf_f(20.0f * exp2_approx((float)filter * 0.08f)); m->hpf_damp = 34000; on |= PERF_HPF; }
    if (pump) { m->pump_amount = (int32_t)(pump * 29491 / 100); m->pump_release = (int32_t)(16777216.0f / (150.0f * 44.1f)); on |= PERF_PUMP; }
    if (mix_val[1][0]) { set_drive(m, (float)mix_val[1][0] / 100.0f); on |= PERF_DRIVE; }
    if (mix_val[1][1]) { int bits = 16 - mix_val[1][1] * 12 / 100; m->crush_step = 1 << (24 - bits); m->crush_rate = 1 + mix_val[1][1] * 15 / 100; m->crush_mix = 32768; on |= PERF_CRUSH; }
    if (mix_val[1][2]) { m->gate_shift = 0; m->gate_len = (int32_t)((95 - mix_val[1][2] * 80 / 100) * 65536 / 100); m->gate_floor = 0; on |= PERF_GATE; }
    if (mix_val[1][3]) { m->sweep_inc = (int32_t)(uint32_t)(0.25f * 97391.55f); m->sweep_depth = 26214; m->sweep_fb = 16384; m->sweep_mix = (int32_t)(mix_val[1][3] * 32768 / 100); on |= PERF_SWEEP; }
    uint32_t mix_pads = pads_held[SCOPE_MIX] | pads_latched[SCOPE_MIX];
    m->on = on | throw_pads(m, mix_pads);
    uint32_t drum_pads = pads_held[SCOPE_DRUMS] | pads_latched[SCOPE_DRUMS];
    fx_track_perf[FX_CR78].on = throw_pads(&fx_track_perf[FX_CR78], drum_pads);
    /* the pattern bends: the drums are the only sequencer yet, so the mix's rolls bend them too */
    uint32_t bends = mix_pads | drum_pads;
    unsigned perf = 0;
    if (bends & (1U << P_R16)) perf |= CR78_PERF_ROLL16;
    if (bends & (1U << P_R32)) perf |= CR78_PERF_ROLL32;
    if (bends & (1U << P_HLF)) perf |= CR78_PERF_HALF;
    if (bends & (1U << P_REV)) perf |= CR78_PERF_REV;
    cr78.roll_len = 1; cr78.roll_fade = 1; cr78.perf = (uint8_t)perf;
    cr78.swing_q16 = (int32_t)((mix_val[0][1] - 50) * 4 * 65536 / 100);
    cr78.level = (int32_t)(mix_val[2][0] * 32768 / 100);
    cr78.muted = (uint8_t)muted[T_DRUMS];
}

/* ---- text ---- */
static void mix_text(unsigned pg, unsigned k, char *out)
{
    const struct knob *kn = &mix_knob[pg][k];
    int x = mix_val[pg][k];
    *out = 0;
    switch (kn->kind) {
    case K_TEMPO: px_number((int)groove_tempo_current(), out); break;
    case K_FILTER:
        if (x < -2) { strcpy(out, "LP"); px_number(-x, out + 2); }
        else if (x > 2) { strcpy(out, "HP"); px_number(x, out + 2); }
        else strcpy(out, "OFF");
        break;
    case K_OFF0: if (!x) strcpy(out, "OFF"); else px_number(x, out); break;
    default: px_number(x, out); break;
    }
}
static void readout(const char *who, const char *label, const char *value, uint32_t now)
{
    static char text[24];
    text[0] = 0;
    if (who) { strcpy(text, who); strcat(text, " "); }
    strcat(text, label); strcat(text, " "); strcat(text, value);
    px_flash(text, now, 900U);
}

/* ---- screens ---- */
static unsigned scope(void) { return zoom == T_DRUMS ? SCOPE_DRUMS : SCOPE_MIX; }
static const char *const *key_row(void)
{
    static const char *const lanes[PX_PADS] = {"DRM", "BAS", "KEY", "LED"};
    static const char *const none[PX_PADS] = {""};
    if (fx_held) return pad_name;
    return zoom == OVERVIEW ? lanes : zoom == T_DRUMS ? cr78_voice_row() : none;
}
static uint32_t key_lit(void)
{
    if (fx_held) return pads_held[scope()] | pads_latched[scope()];
    if (zoom == OVERVIEW) { uint32_t m = 0; for (unsigned t = 0; t < TRACKS; ++t) if (muted[t]) m |= 1U << t; return m; }
    if (zoom == T_DRUMS) for (unsigned i = 0; i < PX_PADS; ++i) if (cr78_black_voice(i) == voice) return 1U << i;
    return 0;
}
static void draw_overview(uint32_t now)
{
    /* four lanes, one bar each: a cell per step, brighter the more that track plays on it */
    unsigned head = cr78_playhead, run = cr78_running;
    for (unsigned t = 0; t < TRACKS; ++t) {
        int y = 16 + (int)t * 12;
        unsigned c = muted[t] ? PX_DUSK : track_color[t];
        px_print(track_short[t], 2, y + 2, c);
        if (t != T_DRUMS) { px_print("SOON", 20, y + 2, PX_DUSK); continue; }
        for (unsigned s = 0; s < 16; ++s) {
            unsigned n = 0;
            for (unsigned v = 0; v < CR78_VOICES; ++v) n += (cr78.grid[v] >> s) & 1U;
            int x = 16 + (int)s * 4, h = n > 4 ? 9 : (int)n * 2 + 1;
            if (run && s == head) px_rect(x - 1, y - 1, 5, 11, PX_DUSK);
            if (n) px_rect(x, y + 9 - h, 3, h, run && s == head ? PX_WHITE : c);
            else px_pset(x + 1, y + 8, PX_DUSK);
        }
    }
    (void)now;
}
static void draw_placeholder(void)
{
    px_print_scaled(track_name[zoom], 2, 14, 2, track_color[zoom]);
    px_print("NEXT STAGE", 2, 34, PX_SLATE);
    px_print("HOME: BACK", 2, 44, PX_SLATE);
}
static void draw(uint32_t now)
{
    char v[4][12];
    const char *value[4] = {v[0], v[1], v[2], v[3]};
    px_cls(PX_NIGHT);
    const char *mode = zoom == OVERVIEW ? "GBX" : track_name[zoom];
    unsigned tint = zoom == OVERVIEW ? PX_GOLD : track_color[zoom];
    if (zoom == T_DRUMS && held_steps) {   /* a held step: its ratchet and accent */
        unsigned st = 0; while (!((held_steps >> st) & 1U)) ++st;
        static char title[12];
        strcpy(title, "STEP "); px_number((int)st + 1, title + 5);
        struct px_page step = {title, {"RTCH", "ACC", "", ""}};
        px_number(cr78.ratchet[voice][st], v[0]); strcpy(v[1], (cr78.accent >> st) & 1U ? "ON" : "OFF"); v[2][0] = v[3][0] = 0;
        px_chrome_pads(mode, &step, 1, 0, value, key_row(), key_lit());
    } else if (zoom == OVERVIEW) {
        for (unsigned k = 0; k < 4; ++k) mix_text(page[0], k, v[k]);
        px_chrome_pads(mode, mix_page, MIX_PAGES, page[0], value, key_row(), key_lit());
    } else if (zoom == T_DRUMS) {
        unsigned n; const struct px_page *pages = cr78_voice_pages(&n);
        for (unsigned k = 0; k < 4; ++k) cr78_voice_text(voice, page[1 + T_DRUMS], k, v[k]);
        px_chrome_pads(mode, pages, n, page[1 + T_DRUMS], value, key_row(), key_lit());
    } else {
        struct px_page none = {"", {"", "", "", ""}};
        px_chrome_pads(mode, &none, 1, 0, 0, key_row(), 0);
    }
    px_print(mode, 2, 1, tint);   /* the track's colour on its name */
    /* the line under the header: what is selected, and what is going on */
    if (zoom == T_DRUMS) px_print(cr78_voice_name(voice), 2, 8, cr78_voice_color(voice));
    else if (zoom == OVERVIEW) px_print(cr78_preset_name(cr78_preset_index()), 2, 8, PX_SLATE);
    if (rec) px_print("REC", PX_W - 13, 8, PX_RED);
    else if (zoom >= 0 && muted[zoom]) px_print("MUTE", PX_W - 17, 8, PX_RED);
    else if (pads_latched[scope()]) {
        char on[16] = ""; unsigned n = 0;
        for (unsigned i = 0; i < PX_PADS && n < 3; ++i) if ((pads_latched[scope()] >> i) & 1U) { if (n++) strcat(on, " "); strcat(on, pad_name[i]); }
        px_print(on, PX_W - 1 - ((int)strlen(on) * 4 - 1), 8, PX_EMBER);
    }
    if (zoom == OVERVIEW) draw_overview(now);
    else if (zoom == T_DRUMS) cr78_draw_kit(voice, now);
    else draw_placeholder();
    px_flash_draw(now);
}
static void leds(uint32_t now)
{
    uint32_t want = 0;
    if (zoom == T_DRUMS)   /* the voice's steps, the playhead blinking */
        for (unsigned s = 0; s < 16; ++s) {
            unsigned on = (cr78.grid[voice] >> s) & 1U;
            if (cr78_running && s == cr78_playhead) on = !on || ((now / 60U) & 1U);
            if (on) want |= 1U << s;
        }
    else if (zoom == OVERVIEW) want |= 1U << cr78_preset_index();   /* the pattern playing */
    want |= key_lit() << 16;
    for (unsigned s = 0; s < 16; ++s)
        if (((want ^ led_state) >> s) & 1U) fm1_led_set(PX_KEY_LED + white_keys[s], (want >> s) & 1U);
    for (unsigned i = 0; i < PX_PADS; ++i)
        if (((want ^ led_state) >> (16 + i)) & 1U) fm1_led_set(PX_KEY_LED + black_keys[i], (want >> (16 + i)) & 1U);
    led_state = want;
}
static void flash_view(uint32_t now)
{
    if (zoom == OVERVIEW) px_flash("OVERVIEW", now, 600U);
    else px_flash(track_name[zoom], now, 600U);
}

unsigned gbx_active(void) { return active; }

int gbx_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons;
    uint32_t down = c->keys & ~prev_keys;
    unsigned home = px_mode_toggle(pressed, PX_MODE_GBX, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!home) return 0;
        cr78_kit_ready();
        px_leds_off(); led_state = 0;
        active = 1; full_redraw = 1; last_ms = now - 100U; zoom = OVERVIEW;
        apply();
        flash_view(now);
        return 1;
    }
    if (home) {   /* HOME: one level up */
        pads_held[SCOPE_MIX] = pads_held[SCOPE_DRUMS] = 0; fx_held = 0; held_steps = 0;
        if (zoom != OVERVIEW) { zoom = OVERVIEW; apply(); flash_view(now); full_redraw = 1; return 1; }
        apply(); px_leds_off(); active = 0;
        return 2;
    }
    unsigned changed = 0;
    const int16_t *e = c->encoders;
    /* ALGORITHM: along OVERVIEW, DRUMS, BASS, KEYS, LEAD */
    if (e[1]) {
        int z = clamp(zoom + e[1], OVERVIEW, TRACKS - 1);
        if (z != zoom) { zoom = z; pads_held[SCOPE_MIX] = pads_held[SCOPE_DRUMS] = 0; apply(); flash_view(now); }
        changed = 1;
    }
    /* REC: live playing goes into the steps */
    if (pressed & PX_BIT(PX_REC) && !fx_held) { rec ^= 1U; px_flash(rec ? "REC ON" : "REC OFF", now, 500U); changed = 1; }
    /* FX: tapped, mute (the overview: all tracks); held, the black keys throw effects */
    if (pressed & PX_BIT(PX_FX)) { fx_held = 1; fx_used = 0; changed = 1; }
    if (fx_held && !(c->buttons & PX_BIT(PX_FX))) {
        fx_held = 0; pads_held[scope()] = 0; changed = 1;
        if (!fx_used) {
            if (zoom == OVERVIEW) { unsigned all = muted[0] && muted[1] && muted[2] && muted[3]; for (unsigned t = 0; t < TRACKS; ++t) muted[t] = !all; px_flash(all ? "ALL ON" : "ALL MUTED", now, 600U); }
            else { muted[zoom] ^= 1U; px_flash(muted[zoom] ? "MUTE" : "UNMUTE", now, 600U); }
        }
        apply();
    }
    /* white keys */
    held_steps = 0;
    for (unsigned s = 0; s < 16; ++s) {
        unsigned key = white_keys[s];
        if (zoom == T_DRUMS && ((c->keys >> key) & 1U)) held_steps |= (uint16_t)(1U << s);
        if (!((down >> key) & 1U)) continue;
        if (zoom == OVERVIEW) {   /* a pattern (the drums' eight, for now) */
            if (s < cr78_preset_count()) { cr78_preset(s); mix_val[0][1] = 50; px_flash(cr78_preset_name(s), now, 700U); }
        } else if (zoom == T_DRUMS) {
            uint16_t bit = (uint16_t)(1U << s);
            if (c->buttons & PX_BIT(PX_ENV)) cr78.accent ^= bit;
            else {
                cr78.grid[voice] ^= bit;
                if (cr78.grid[voice] & bit) { cr78.ratchet[voice][s] = 1; if (!cr78_running) (void)cr78_audition(voice, 0); }
            }
        }
        changed = 1;
    }
    /* black keys */
    uint32_t held = 0;
    for (unsigned i = 0; i < PX_PADS; ++i) {
        unsigned key = black_keys[i], hit = (down >> key) & 1U;
        if (fx_held) {   /* the pads */
            if (hit) {
                fx_used = 1; changed = 1;
                if (c->buttons & PX_BIT(PX_REC)) { pads_latched[scope()] ^= 1U << i; px_flash((pads_latched[scope()] >> i) & 1U ? "LATCH" : "OFF", now, 500U); }
                else px_flash(pad_name[i], now, 400U);
            }
            if (((c->keys >> key) & 1U) && !(c->buttons & PX_BIT(PX_REC))) held |= 1U << i;
            continue;
        }
        if (!hit) continue;
        changed = 1;
        if (zoom == OVERVIEW) {   /* mute a track */
            if (i < TRACKS) { muted[i] ^= 1U; static char t[12]; strcpy(t, track_short[i]); strcat(t, muted[i] ? " OFF" : " ON"); px_flash(t, now, 500U); apply(); }
        } else if (zoom == T_DRUMS) {
            unsigned v = cr78_black_voice(i);
            if (held_steps) {   /* a held step: that voice's hit on it */
                for (unsigned s = 0; s < 16; ++s) if ((held_steps >> s) & 1U) cr78.grid[v] ^= (uint16_t)(1U << s);
            } else {
                voice = v;
                if (rec && cr78_running) { cr78.grid[v] |= (uint16_t)(1U << cr78_playhead); cr78.ratchet[v][cr78_playhead] = 1; }
                if (!cr78_running || rec) (void)cr78_audition(v, 1);
            }
        }
    }
    if (held != pads_held[scope()] || (fx_held && changed)) { pads_held[scope()] = held; apply(); }
    /* SELECT: the view's pages; PRESETS: the drum pattern */
    if (e[0]) {
        unsigned n = 1;
        if (zoom == OVERVIEW) n = MIX_PAGES; else if (zoom == T_DRUMS) (void)cr78_voice_pages(&n);
        unsigned *pg = &page[zoom + 1];
        *pg = (unsigned)clamp((int)*pg + e[0], 0, (int)n - 1);
        if (zoom == OVERVIEW) px_flash(mix_page[*pg].name, now, 600U);
        else if (zoom == T_DRUMS) { unsigned m; px_flash(cr78_voice_pages(&m)[*pg].name, now, 600U); }
        changed = 1;
    }
    if (e[6] && zoom <= T_DRUMS) {
        unsigned p = (unsigned)(((int)cr78_preset_index() + e[6] + 8 * (int)cr78_preset_count()) % (int)cr78_preset_count());
        cr78_preset(p); mix_val[0][1] = 50; px_flash(cr78_preset_name(p), now, 700U); changed = 1;
    }
    /* KNOB 1-4 */
    const int16_t *k = e + 2;
    if (k[0] || k[1] || k[2] || k[3]) {
        changed = 1;
        unsigned last = k[3] ? 3U : k[2] ? 2U : k[1] ? 1U : 0U;
        char value[12];
        if (zoom == T_DRUMS && held_steps) {   /* a held step: ratchet, accent */
            for (unsigned s = 0; s < 16; ++s) {
                if (!((held_steps >> s) & 1U)) continue;
                if (k[0]) cr78_set_ratchet(voice, s, (int)cr78.ratchet[voice][s] + k[0]);
                if (k[1]) cr78.accent = (uint16_t)(k[1] > 0 ? cr78.accent | (1U << s) : cr78.accent & ~(1U << s));
            }
        } else if (zoom == OVERVIEW) {
            unsigned pg = page[0];
            for (unsigned i = 0; i < 4; ++i) {
                const struct knob *kn = &mix_knob[pg][i];
                if (!k[i]) continue;
                if (kn->kind == K_TEMPO) { groove_tempo_edit(k[i]); continue; }
                int x = clamp(mix_val[pg][i] + k[i] * kn->step, kn->lo, kn->hi);
                if (kn->kind == K_FILTER && x > -3 && x < 3) x = 0;
                mix_val[pg][i] = (int16_t)x;
            }
            apply();
            mix_text(pg, last, value); readout(0, mix_knob[pg][last].label, value, now);
        } else if (zoom == T_DRUMS) {
            unsigned pg = page[1 + T_DRUMS];
            for (unsigned i = 0; i < 4; ++i) if (k[i]) cr78_voice_turn(voice, pg, i, k[i]);
            if (!cr78_running) (void)cr78_audition(voice, 0);
            if (cr78_voice_knob(pg, last)[0]) { cr78_voice_text(voice, pg, last, value); readout(cr78_voice_short(voice), cr78_voice_knob(pg, last), value, now); }
        }
    }
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
        leds(now);
        px_present(full_redraw);
        full_redraw = 0;
    }
    return 1;
}
