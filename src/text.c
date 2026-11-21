/* Text and icon rendering from the baked alpha atlas. */
#include <string.h>

#include "ezt.h"
#include "atlas.h"

static const uint8_t *g_atlas;

void ez_text_init(const uint8_t *atlas)
{
    g_atlas = atlas;
}

static const ez_glyph *find_glyph(int style, uint16_t cp)
{
    if (style < 0 || style >= EZ_STYLE_COUNT) return 0;
    uint32_t key = ((uint32_t)style << 16) | cp;
    int lo = 0, hi = EZ_GLYPH_COUNT;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        uint32_t k = ((uint32_t)ez_glyphs[mid].style << 16) | ez_glyphs[mid].cp;
        if (k < key) lo = mid + 1;
        else hi = mid;
    }
    if (lo < EZ_GLYPH_COUNT) {
        const ez_glyph *g = &ez_glyphs[lo];
        if (g->style == style && g->cp == cp) return g;
    }
    return 0;
}

static uint32_t utf8_next(const char **sp)
{
    const uint8_t *s = (const uint8_t *)*sp;
    uint32_t c = *s;
    if (!c) return 0;
    if (c < 0x80) {
        *sp += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && s[1]) {
        *sp += 2;
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
        *sp += 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        *sp += 4;
        return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) |
               (s[3] & 0x3F);
    }
    *sp += 1;
    return '?';
}

int ez_text_width(int style, const char *utf8)
{
    int w = 0;
    const char *s = utf8;
    for (;;) {
        uint32_t cp = utf8_next(&s);
        if (!cp) break;
        const ez_glyph *g = find_glyph(style, (uint16_t)cp);
        if (g) w += g->adv;
    }
    return w;
}

int ez_text_height(int style)
{
    if (style < 0 || style >= EZ_STYLE_COUNT) return 0;
    return ez_style_metrics[style].ascent + ez_style_metrics[style].descent;
}

static void draw_glyph(ez_fb *fb, const ez_glyph *g, int x, int y, ez_rgb c, int alpha)
{
    if (!g || g->w == 0 || g->h == 0) return;
    ez_rect src = {g->ax, g->ay, g->w, g->h};
    ez_rect dst = {x + g->ox, y + g->oy, g->w, g->h};
    ez_blit_a8(fb, g_atlas, EZ_ATLAS_W, EZ_ATLAS_H, src, dst, c, alpha);
}

void ez_text(ez_fb *fb, int x, int y, int style, ez_rgb c, int alpha, const char *utf8)
{
    if (!g_atlas || !utf8) return;
    int pen = x;
    const char *s = utf8;
    for (;;) {
        uint32_t cp = utf8_next(&s);
        if (!cp) break;
        const ez_glyph *g = find_glyph(style, (uint16_t)cp);
        if (!g) continue;
        if (cp != ' ') draw_glyph(fb, g, pen, y, c, alpha);
        pen += g->adv;
    }
}

void ez_text_right(ez_fb *fb, int right, int y, int style, ez_rgb c, int alpha,
                   const char *utf8)
{
    ez_text(fb, right - ez_text_width(style, utf8), y, style, c, alpha, utf8);
}

void ez_text_center(ez_fb *fb, int cx, int y, int style, ez_rgb c, int alpha,
                    const char *utf8)
{
    ez_text(fb, cx - ez_text_width(style, utf8) / 2, y, style, c, alpha, utf8);
}

void ez_icon(ez_fb *fb, int x, int y, int style, uint16_t cp, ez_rgb c, int alpha)
{
    const ez_glyph *g = find_glyph(style, cp);
    if (!g) return;
    draw_glyph(fb, g, x - g->ox, y - g->oy, c, alpha);
}

void ez_icon_center(ez_fb *fb, int cx, int cy, int style, uint16_t cp, ez_rgb c, int alpha)
{
    const ez_glyph *g = find_glyph(style, cp);
    if (!g) return;
    int x = cx - g->ox - g->w / 2;
    int y = cy - g->oy - g->h / 2;
    draw_glyph(fb, g, x, y, c, alpha);
}

int ez_icon_width(int style, uint16_t cp)
{
    const ez_glyph *g = find_glyph(style, cp);
    return g ? g->w : 0;
}

static const struct {
    const char *name;
    uint16_t cp;
} ICONS[] = {
    {"play_arrow", EZ_IC_play_arrow},
    {"pause", EZ_IC_pause},
    {"skip_next", EZ_IC_skip_next},
    {"skip_previous", EZ_IC_skip_previous},
    {"settings", EZ_IC_settings},
    {"folder", EZ_IC_folder},
    {"movie", EZ_IC_movie},
    {"palette", EZ_IC_palette},
    {"check", EZ_IC_check},
    {"arrow_back", EZ_IC_arrow_back},
    {"more_vert", EZ_IC_more_vert},
    {"grid_view", EZ_IC_grid_view},
    {"fullscreen", EZ_IC_fullscreen},
    {"volume_up", EZ_IC_volume_up},
    {"dark_mode", EZ_IC_dark_mode},
    {"light_mode", EZ_IC_light_mode},
    {"chevron_left", EZ_IC_chevron_left},
    {"chevron_right", EZ_IC_chevron_right},
    {"playlist_play", EZ_IC_playlist_play},
    {"history", EZ_IC_history},
    {"home", EZ_IC_home},
    {"search", EZ_IC_search},
    {"image", EZ_IC_image},
    {"videocam", EZ_IC_videocam},
    {"smart_display", EZ_IC_smart_display},
    {"play_circle", EZ_IC_play_circle},
    {"close", EZ_IC_close},
    {"refresh", EZ_IC_refresh},
    {"tune", EZ_IC_tune},
    {"speed", EZ_IC_speed},
};

uint16_t ez_icon_cp(const char *name)
{
    for (size_t i = 0; i < sizeof(ICONS) / sizeof(ICONS[0]); i++) {
        if (strcmp(ICONS[i].name, name) == 0) return ICONS[i].cp;
    }
    return 0;
}
