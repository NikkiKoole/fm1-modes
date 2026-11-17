/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* STARFALL: a tiny pixel game that only uses the public FM1 panel API.
 *
 * Opened from the launcher; HOME goes back. Stars fall above the 27 piano keys; a key's LED lights
 * when a star is coming down over it. Press that key (or steer with K1 / the
 * OCT buttons) to move the catcher. Every caught star plays its own key's note
 * on track 1 through fm1_midi_post, so the melody is whatever you catch.
 *
 * Drawing goes through the shared pixel console (pixel.h). */
#include "pixel.h"
#include <string.h>

enum { MAX_STARS = 6, MAX_SPARKS = 24, MAX_OFFS = 8 };
enum { TITLE, PLAY, OVER };
#define BIT PX_BIT
enum { W = PX_W, KEYS = PX_KEYS, FIRST_NOTE = PX_FIRST_NOTE, KEY_LED = PX_KEY_LED,
       B_OCT_DOWN = PX_OCT_DOWN, B_OCT_UP = PX_OCT_UP, B_HOME = PX_HOME, B_ARP = PX_ARP };
static struct { int16_t x, y, vy; uint8_t key; } stars[MAX_STARS];
static struct { int16_t x, y, vx, vy; uint8_t life, color; } sparks[MAX_SPARKS];
static struct { uint32_t at; uint8_t note; } offs[MAX_OFFS];
static unsigned active, state, lives, score, best, frame, spawn_in, hurt, lit_key;
static int catcher; /* centre column */
static uint32_t prev_buttons, prev_keys, last_ms, rng = 0x9e3779b9u;
static unsigned full_redraw;
static uint32_t pending_keys; /* presses seen by polls between game steps */
static int pending_turn;      /* K1 detents likewise */

static unsigned rnd(unsigned n)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return n ? rng % n : 0;
}
static int key_x(unsigned key) { return 1 + (int)key * 3; }

/* ---- sound: notes go into the instrument's own MIDI input (track 1) ---- */
static void note(uint8_t n, uint8_t velocity, uint32_t now, uint32_t length)
{
    uint8_t on[3] = {0x90, n, velocity};
    if (fm1_midi_post(on, 3)) return;
    for (unsigned i = 0; i < MAX_OFFS; ++i)
        if (!offs[i].note) { offs[i].note = n; offs[i].at = now + length; return; }
}
static void notes_off(uint32_t now, unsigned all)
{
    for (unsigned i = 0; i < MAX_OFFS; ++i)
        if (offs[i].note && (all || (int32_t)(now - offs[i].at) >= 0)) {
            uint8_t off[3] = {0x80, offs[i].note, 0};
            (void)fm1_midi_post(off, 3);
            offs[i].note = 0;
        }
}
static void light(unsigned key)
{
    if (key == lit_key) return;
    if (lit_key < KEYS) fm1_led_set(KEY_LED + lit_key, 0);
    if (key < KEYS) fm1_led_set(KEY_LED + key, 1);
    lit_key = key;
}

/* ---- game ---- */
static void start(void)
{
    memset(stars, 0, sizeof stars); memset(sparks, 0, sizeof sparks);
    state = PLAY; lives = 3; score = 0; spawn_in = 30; hurt = 0; catcher = W / 2;
}
static void burst(int x, int y, unsigned color)
{
    for (unsigned n = 0, i = 0; i < MAX_SPARKS && n < 8; ++i)
        if (!sparks[i].life) {
            sparks[i].x = (int16_t)(x * 16); sparks[i].y = (int16_t)(y * 16);
            sparks[i].vx = (int16_t)((int)rnd(33) - 16); sparks[i].vy = (int16_t)(-(int)rnd(24) - 4);
            sparks[i].life = (uint8_t)(10 + rnd(10)); sparks[i].color = (uint8_t)color; ++n;
        }
}
static void step(const struct fm1_controls *c, uint32_t now)
{
    uint32_t pressed_keys = pending_keys; int turn = pending_turn;
    pending_keys = 0; pending_turn = 0;
    ++frame;
    if (state != PLAY) {
        if (pressed_keys || turn) { start(); rng ^= now; }
        return;
    }
    /* steering: a key jumps the catcher to its column; K1 and OCT nudge it */
    for (unsigned k = 0; k < KEYS; ++k) if (pressed_keys & BIT(k)) catcher = key_x(k);
    catcher += turn * 2;
    if (c->buttons & BIT(B_OCT_DOWN)) catcher -= 2;
    if (c->buttons & BIT(B_OCT_UP)) catcher += 2;
    if (catcher < 3) catcher = 3;
    if (catcher > W - 4) catcher = W - 4;

    if (spawn_in) --spawn_in;
    if (!spawn_in) {
        for (unsigned i = 0; i < MAX_STARS; ++i)
            if (!stars[i].vy) {
                stars[i].key = (uint8_t)rnd(KEYS);
                stars[i].x = (int16_t)key_x(stars[i].key);
                stars[i].y = -2 * 16;
                stars[i].vy = (int16_t)(6 + score / 4 + rnd(4));
                break;
            }
        unsigned gap = score < 40 ? 50 - score : 10;
        spawn_in = gap + rnd(gap);
    }
    unsigned lowest = KEYS; int lowest_y = -1000;
    for (unsigned i = 0; i < MAX_STARS; ++i) {
        if (!stars[i].vy) continue;
        stars[i].y = (int16_t)(stars[i].y + stars[i].vy);
        int y = stars[i].y / 16;
        if (y >= 70 && y <= 72 && stars[i].x >= catcher - 4 && stars[i].x <= catcher + 4) {
            ++score; if (score > best) best = score;
            note((uint8_t)(FIRST_NOTE + stars[i].key), 100, now, 180);
            burst(stars[i].x, 70, 3);
            stars[i].vy = 0; continue;
        }
        if (y >= 76) {
            note(36, 70, now, 120);
            burst(stars[i].x, 76, 9);
            stars[i].vy = 0; hurt = 8;
            if (!--lives) { state = OVER; light(KEYS); }
            continue;
        }
        if (y > lowest_y) { lowest_y = y; lowest = stars[i].key; }
    }
    if (state == PLAY) light(lowest);
    for (unsigned i = 0; i < MAX_SPARKS; ++i)
        if (sparks[i].life) {
            sparks[i].x = (int16_t)(sparks[i].x + sparks[i].vx);
            sparks[i].y = (int16_t)(sparks[i].y + sparks[i].vy);
            sparks[i].vy = (int16_t)(sparks[i].vy + 2); --sparks[i].life;
        }
    if (hurt) --hurt;
}
static void draw(void)
{
    px_cls(PX_NIGHT);
    /* fixed backdrop: a few twinkling far stars and a ground line */
    for (unsigned i = 0; i < 24; ++i) {
        unsigned x = (i * 37U + 11U) % W, y = (i * 53U + 7U) % 64U;
        px_pset((int)x, (int)y, ((frame / 8U + i) % 7U) ? 1 : 8);
    }
    px_rect(0, 77, W, 3, hurt & 2 ? 9 : 1);
    for (unsigned k = 0; k < KEYS; ++k) px_pset(key_x(k), 78, k == lit_key ? 3 : 8);

    char line[16];
    if (state == TITLE) {
        px_print_c("STARFALL", 18, 3);
        px_print_c("CATCH STARS", 32, 2);
        px_print_c("WITH THE KEYS", 39, 2);
        if ((frame / 20U) & 1U) px_print_c("PRESS A KEY", 54, 6);
        px_print_c("HOME BACK", 68, 8);
        return;
    }
    for (unsigned i = 0; i < MAX_STARS; ++i)
        if (stars[i].vy) {
            int x = stars[i].x, y = stars[i].y / 16;
            px_pset(x, y, 2); px_pset(x - 1, y, 3); px_pset(x + 1, y, 3); px_pset(x, y - 1, 3); px_pset(x, y + 1, 3);
            px_pset(x, y - 2 - (int)(frame & 1U), 4);   /* flickering tail */
        }
    for (unsigned i = 0; i < MAX_SPARKS; ++i)
        if (sparks[i].life) px_pset(sparks[i].x / 16, sparks[i].y / 16, sparks[i].life > 6 ? sparks[i].color : 4);
    px_rect(catcher - 3, 72, 7, 2, 6); px_rect(catcher - 1, 71, 3, 1, 7);

    px_number((int)score, line); px_print(line, 2, 2, 2);
    for (unsigned i = 0; i < lives; ++i) px_rect(W - 6 - (int)i * 5, 2, 3, 3, 5);
    if (state == OVER) {
        px_rect(10, 26, 60, 26, 1);
        px_print_c("GAME OVER", 30, 9);
        char best_line[16] = "BEST "; px_number((int)best, best_line + 5);
        px_print_c(best_line, 38, 3);
        if ((frame / 20U) & 1U) px_print_c("PRESS A KEY", 45, 6);
    }
}
unsigned game_active(void) { return active; }

/* Called first thing in every control poll. Returns 0 when the game is not
 * running, 1 while it owns the panel, 2 on the poll that closes it. */
int game_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t pressed = c->buttons & ~prev_buttons, pressed_keys = c->keys & ~prev_keys;
    unsigned combo = px_mode_toggle(pressed, PX_MODE_GAME, active);
    prev_buttons = c->buttons; prev_keys = c->keys;
    if (!active) {
        if (!combo) return 0;
        px_leds_off(); /* drop the step LEDs */
        active = 1; state = TITLE; frame = 0; full_redraw = 1; lit_key = KEYS;
        last_ms = now; rng ^= now * 2654435761u;
        return 1;
    }
    if (combo) {
        active = 0; notes_off(now, 1); light(KEYS);
        return 2;
    }
    notes_off(now, 0);
    pending_keys |= pressed_keys; pending_turn += c->encoders[0];
    /* fixed 30 Hz simulation regardless of how often the control task polls */
    unsigned ticks = 0;
    while (now - last_ms >= 33U && ticks < 4) {
        step(c, now);
        last_ms += 33U; ++ticks;
    }
    if (now - last_ms >= 33U) last_ms = now;
    if (ticks) { draw(); px_present(full_redraw); full_redraw = 0; }
    return 1;
}
