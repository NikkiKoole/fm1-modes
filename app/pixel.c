/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "pixel.h"
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
static uint16_t band[PX_W * SCALE * BAND * SCALE];

void px_cls(unsigned color) { memset(canvas, (int)color, sizeof canvas); }
void px_pset(int x, int y, unsigned c)
{
    if (x >= 0 && y >= 0 && x < PX_W && y < PX_H) canvas[y * PX_W + x] = (uint8_t)c;
}
void px_rect(int x, int y, int w, int h, unsigned c)
{
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) px_pset(x + i, y + j, c);
}
void px_print(const char *s, int x, int y, unsigned c)
{
    for (; *s; ++s, x += 4) {
        uint16_t g = 0;
        if (*s >= '0' && *s <= '9') g = glyph_digit[*s - '0'];
        else if (*s >= 'A' && *s <= 'Z') g = glyph_alpha[*s - 'A'];
        else if (*s == '-') g = 000700;
        else if (*s == '+') g = 002720;
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
    for (unsigned top = 0; top < PX_H; top += BAND) {
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
}
void px_leds_off(void)
{
    for (unsigned i = 0; i < PX_KEY_LED + PX_KEYS; ++i) fm1_led_set(i, 0);
}
unsigned px_combo(uint32_t buttons, uint32_t pressed, unsigned a, unsigned b)
{
    uint32_t both = PX_BIT(a) | PX_BIT(b);
    return (buttons & both) == both && (pressed & both);
}
