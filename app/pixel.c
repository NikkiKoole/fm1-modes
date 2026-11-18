/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "pixel.h"
#include "fine_font.h"
#include <string.h>

enum { SCALE = 3, BAND = 4 /* canvas rows per blit */ };
#define RGB(r, g, b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

static const uint16_t palette[PX_COLORS] = {
    RGB(12, 14, 32), RGB(30, 36, 78), RGB(255, 246, 214), RGB(255, 206, 64), RGB(255, 128, 48),
    RGB(255, 90, 140), RGB(64, 220, 255), RGB(90, 230, 120), RGB(110, 120, 150), RGB(230, 60, 60)};

/* 3x5 glyphs: one octal digit per row, top row first. */
static const uint16_t glyph_digit[10] = {075557, 026227, 071747, 071717, 055711,
                                         074717, 074757, 071122, 075757, 075717};
static const uint16_t glyph_alpha[26] = {
    025755, 065656, 034443, 065556, 074647, 074644, 034553, 055755, 072227,
    011152, 055655, 044447, 057755, 065555, 025552, 065644, 025563, 065655,
    034216, 072222, 055557, 055552, 055775, 055255, 055222, 071247};

static uint8_t canvas[PX_W * PX_H], shown[PX_W * PX_H];
/* The fine strip: the screen's bottom PX_FINE_H rows at full resolution (240 wide), in place of canvas
 * rows PX_FINE_ROW..79, for text too small for the 3x canvas (the pad row, the knob bar). It is on for
 * a frame when anything draws into it since the last px_cls; a mode that never does (STARFALL) keeps
 * the whole 80x80 canvas. Dirty chunks are found by a hash, not a second copy. */
enum { FINE_W = PX_W * SCALE, FINE_CHUNK = BAND * SCALE /* 12 screen rows: the band buffer's size */ };
static uint8_t fine[FINE_W * PX_FINE_H];
static unsigned fine_used, fine_shown_on;
static uint32_t fine_hash[PX_FINE_H / FINE_CHUNK];
static uint16_t band[PX_W * SCALE * BAND * SCALE];

void px_cls(unsigned color) { memset(canvas, (int)color, sizeof canvas); fine_used = 0; }
void px_pset(int x, int y, unsigned c)
{
    if (x >= 0 && y >= 0 && x < PX_W && y < PX_H) canvas[y * PX_W + x] = (uint8_t)c;
}
void px_rect(int x, int y, int w, int h, unsigned c)
{
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) px_pset(x + i, y + j, c);
}
/* one 3x5 glyph: digits, A-Z, and the few symbols the modes print */
static uint16_t glyph(char ch)
{
    if (ch >= '0' && ch <= '9') return glyph_digit[ch - '0'];
    if (ch >= 'A' && ch <= 'Z') return glyph_alpha[ch - 'A'];
    switch (ch) {
    case '-': return 000700;
    case '+': return 002720;
    case '/': return 011244;
    case '.': return 000002;
    case ':': return 002020;
    case '%': return 051245;
    case '\'': return 022000;
    default: return 0;
    }
}
void px_print(const char *s, int x, int y, unsigned c)
{
    for (; *s; ++s, x += 4) {
        uint16_t g = glyph(*s);
        for (int row = 0; row < 5; ++row) {
            unsigned bits = (g >> ((4 - row) * 3)) & 7U;
            for (int col = 0; col < 3; ++col)
                if (bits & (4U >> col)) px_pset(x + col, y + row, c);
        }
    }
}
void px_print_c(const char *s, int y, unsigned c)
{
    px_print(s, (PX_W - ((int)strlen(s) * 4 - 1)) / 2, y, c);
}
void px_number(int v, char *out)
{
    char tmp[12]; int n = 0;
    unsigned u = v < 0 ? 0U - (unsigned)v : (unsigned)v;
    if (v < 0) *out++ = '-';
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u && n < 11);
    while (n) *out++ = tmp[--n];
    *out = 0;
}
void px_present(unsigned force)
{
    /* The groove UI may still have a DMA band in flight on hardware. */
    if (fm1_screen_pending() == 1) return;
    const unsigned fine_on = fine_used != 0;
    if (fine_on != fine_shown_on) {   /* the strip came or went: everything under it is stale */
        force = 1;
        memset(fine_hash, 0, sizeof fine_hash);
    }
    const unsigned rows = fine_on ? PX_FINE_ROW : PX_H;
    for (unsigned top = 0; top < rows; top += BAND) {
        if (!force && !memcmp(canvas + top * PX_W, shown + top * PX_W, PX_W * BAND)) continue;
        uint16_t *p = band;
        for (unsigned r = 0; r < BAND; ++r) {
            const uint8_t *src = canvas + (top + r) * PX_W;
            uint16_t *row = p;
            for (unsigned x = 0; x < PX_W; ++x) {
                uint16_t c = palette[src[x]];
                *p++ = c; *p++ = c; *p++ = c;
            }
            memcpy(p, row, PX_W * SCALE * 2); p += PX_W * SCALE;
            memcpy(p, row, PX_W * SCALE * 2); p += PX_W * SCALE;
        }
        if (fm1_screen_blit(0, top * SCALE, PX_W * SCALE, BAND * SCALE, band, sizeof band / 2) == 0)
            memcpy(shown + top * PX_W, canvas + top * PX_W, PX_W * BAND);
    }
    if (!fine_on) { fine_shown_on = 0; return; }
    unsigned all = 1;
    for (unsigned k = 0; k < PX_FINE_H / FINE_CHUNK; ++k) {
        const uint8_t *src = fine + k * FINE_CHUNK * FINE_W;
        uint32_t h = 2166136261u;   /* FNV-1a; 0 never stands for a drawn chunk */
        for (unsigned i = 0; i < FINE_CHUNK * FINE_W; ++i) h = (h ^ src[i]) * 16777619u;
        h |= 1U;
        if (!force && h == fine_hash[k]) continue;
        for (unsigned i = 0; i < FINE_CHUNK * FINE_W; ++i) band[i] = palette[src[i]];
        if (fm1_screen_blit(0, PX_FINE_ROW * SCALE + k * FINE_CHUNK, FINE_W, FINE_CHUNK, band, sizeof band / 2) == 0)
            fine_hash[k] = h;
        else all = 0;
    }
    fine_shown_on = all || fine_shown_on;
}

/* ---- the fine strip (screen pixels; y from the strip's top) ---- */
void px_fine_cls(unsigned color) { memset(fine, (int)color, sizeof fine); fine_used = 1; }
void px_fine_rect(int x, int y, int w, int h, unsigned c)
{
    fine_used = 1;
    for (int j = y < 0 ? 0 : y; j < y + h && j < PX_FINE_H; ++j)
        for (int i = x < 0 ? 0 : x; i < x + w && i < FINE_W; ++i) fine[j * FINE_W + i] = (uint8_t)c;
}
int px_fine_width(const char *s) { int n = (int)strlen(s); return n ? n * PX_FINE_ADV - 1 : 0; }
/* the fine strip's letters: Terminus 12 (fine_font.h), drawn from a row above `y` so the capitals (its
 * rows 2..9) sit at y+1 .. y+8 */
void px_fine_print(const char *s, int x, int y, unsigned c)
{
    for (; *s; ++s, x += PX_FINE_ADV) {
        int ch = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
        if (ch < FINE_FONT_FIRST || ch > FINE_FONT_LAST) continue;
        const uint8_t *g = fine_font[ch - FINE_FONT_FIRST];
        for (int row = 0; row < FINE_FONT_H; ++row)
            for (int col = 0; col < FINE_FONT_W; ++col)
                if (g[row] & (0x20U >> col)) px_fine_rect(x + col, y - 1 + row, 1, 1, c);
    }
    fine_used = 1;
}
int px_fine_width_narrow(const char *s) { return px_fine_width(s); }
void px_fine_print_narrow(const char *s, int x, int y, unsigned c) { px_fine_print(s, x, y, c); }
void px_leds_off(void)
{
    for (unsigned i = 0; i < PX_KEY_LED + PX_KEYS; ++i) fm1_led_set(i, 0);
}
unsigned px_combo(uint32_t buttons, uint32_t pressed, unsigned a, unsigned b)
{
    uint32_t both = PX_BIT(a) | PX_BIT(b);
    return (buttons & both) == both && (pressed & both);
}

void px_print_scaled(const char *s, int x, int y, int scale, unsigned c)
{
    for (; *s; ++s, x += 4 * scale) {
        uint16_t g = glyph(*s);
        for (int row = 0; row < 5; ++row) {
            unsigned bits = (g >> ((4 - row) * 3)) & 7U;
            for (int col = 0; col < 3; ++col)
                if (bits & (4U >> col)) px_rect(x + col * scale, y + row * scale, scale, scale, c);
        }
    }
}

void px_chrome_pads(const char *mode, const struct px_page *pages, unsigned count, unsigned current,
                    const char *const value[4], const char *const pad[PX_PADS], uint32_t lit)
{
    px_chrome_pads_colored(mode, pages, count, current, value, pad, lit, 0);
}
void px_chrome_pads_colored(const char *mode, const struct px_page *pages, unsigned count, unsigned current,
                            const char *const value[4], const char *const pad[PX_PADS], uint32_t lit,
                            const uint8_t color[PX_PADS])
{
    px_print(mode, 2, 1, PX_GOLD);
    if (count) px_print(pages[current].name, 2 + (int)strlen(mode) * 4 + 3, 1, PX_EMBER);
    /* one dot per page, right-aligned; when they would run into the title, on a line of their own under it */
    int title_end = 2 + (int)strlen(mode) * 4 + 3 + (count ? (int)strlen(pages[current].name) * 4 : 0);
    int dots_x = PX_W - 2 - (int)count * 4;
    for (unsigned i = 0; count > 1 && i < count; ++i) {
        if (dots_x > title_end) px_rect(dots_x + (int)i * 4, 2, 3, 3, i == current ? PX_WHITE : PX_DUSK);
        else px_rect(2 + (int)i * 4, 7, 3, 1, i == current ? PX_WHITE : PX_DUSK);
    }
    /* the strip: the eleven black keys in one row of boxes (lit: filled; a key with nothing on it: an
     * empty box), then the knobs' names (narrow) and their values */
    px_fine_cls(PX_NIGHT);
    int y = 9;
    if (pad) {
        for (unsigned i = 0; i < PX_PADS; ++i) {
            int x = 2 + (int)i * 21;
            unsigned on = (lit >> i) & 1U, named = pad[i] && *pad[i];
            px_fine_rect(x, 1, 20, 14, on && named ? (color ? color[i] : PX_GOLD) : PX_DUSK);
            if (named) px_fine_print_narrow(pad[i], x + (20 - px_fine_width_narrow(pad[i])) / 2, 3, on ? PX_NIGHT : PX_SLATE);
        }
        y = 20;
    } else px_fine_rect(0, 1, PX_W * 3, 1, PX_DUSK);
    for (unsigned k = 0; k < 4; ++k) {
        int x = 3 + (int)k * 60;
        const char *label = count ? pages[current].knob[k] : 0;
        if (!label || !*label) continue;
        px_fine_print_narrow(label, x, y, PX_SLATE);
        if (value && value[k]) px_fine_print(value[k], x, y + (pad ? 13 : 13), PX_WHITE);
    }
}
void px_chrome(const char *mode, const struct px_page *pages, unsigned count, unsigned current,
               const char *const value[4])
{
    px_chrome_pads(mode, pages, count, current, value, 0, 0);
}

static const char *flash_text;
static uint32_t flash_until;
void px_flash(const char *text, uint32_t now, uint32_t ms) { flash_text = text; flash_until = now + ms; }
void px_flash_draw(uint32_t now)
{
    if (!flash_text || (int32_t)(flash_until - now) <= 0) return;
    int w = (int)strlen(flash_text) * 8 - 2, x = (PX_W - w) / 2;
    if (x < 1) { px_print_c(flash_text, 32, PX_WHITE); return; }
    px_rect(x - 3, 27, w + 6, 16, PX_DUSK);
    px_print_scaled(flash_text, x, 30, 2, PX_WHITE);
}

static int mode_request = -1;
void px_mode_request(int mode) { mode_request = mode; }
unsigned px_mode_toggle(uint32_t pressed, unsigned mode, unsigned active)
{
    if (!active) {
        if (mode_request == (int)mode) { mode_request = -1; return 1; }
        return 0;
    }
    return (pressed & PX_BIT(PX_HOME)) != 0;
}
