/* Material You screens: library carousel, settings, player. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ezt.h"
#include "ezt_ui.h"
#include "atlas.h"

#define CARD_W 167
#define CARD_H 94
#define CARD_GAP 12

static const ez_rgb WHITE = {255, 255, 255};

static void text_mid(ez_fb *fb, int x, int cy, int style, ez_rgb c, int alpha,
                     const char *s)
{
    int h = ez_text_height(style);
    ez_text(fb, x, cy - h / 2, style, c, alpha, s);
}

static void background(ez_fb *fb, const ez_scheme *s, const ez_image *art)
{
    ez_clear(fb, s->surface);
    if (art) ez_image_cover(fb, art, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, 255);
    ez_fill(fb, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, s->surface, 210);
    ez_glow(fb, 24, -36, 300, s->primary, 40);
    ez_glow(fb, 470, 300, 250, s->tertiary, 24);
    ez_vgrad(fb, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, s->surface, 30,
             s->surface, 190);
}

static void icon_button(ez_fb *fb, const ez_scheme *s, int cx, int cy, int r,
                        uint16_t cp, int active)
{
    ez_round(fb, (ez_rect){cx - r, cy - r, r * 2, r * 2}, r,
             active ? s->primary : s->surface_container_high, active ? 255 : 200);
    ez_icon_center(fb, cx, cy, EZ_I18, cp,
                   active ? s->on_primary : s->on_surface_variant, 255);
}

static int chip(ez_fb *fb, const ez_scheme *s, int x, int y, int h, const char *label,
                uint16_t icon, int active, int alpha)
{
    int tw = ez_text_width(EZ_T12M, label);
    int pad = 11;
    int iw = icon ? 18 : 0;
    int w = pad * 2 + tw + (icon ? iw + 5 : 0);
    int rad = h / 2;
    ez_rect r = {x, y, w, h};
    if (active) {
        ez_round(fb, r, rad, s->secondary_container, alpha);
    } else {
        ez_round(fb, r, rad, s->surface_container_low, (alpha * 70) / 255);
        ez_round_stroke(fb, r, rad, 1, s->outline_variant, (alpha * 190) / 255);
    }
    ez_rgb fg = active ? s->on_secondary_container : s->on_surface_variant;
    int tx = x + pad;
    if (icon) {
        ez_icon_center(fb, tx + iw / 2, y + h / 2, EZ_I18, icon, fg, alpha);
        tx += iw + 5;
    }
    text_mid(fb, tx, y + h / 2, EZ_T12M, fg, alpha, label);
    return w;
}

static int hint(ez_fb *fb, const ez_scheme *s, int x, int y, uint16_t i1, uint16_t i2,
                const char *label)
{
    int cx = x;
    if (i1) {
        ez_icon(fb, cx, y, EZ_I14, i1, s->on_surface_variant, 210);
        cx += ez_icon_width(EZ_I14, i1) + 1;
    }
    if (i2) {
        ez_icon(fb, cx, y, EZ_I14, i2, s->on_surface_variant, 210);
        cx += ez_icon_width(EZ_I14, i2) + 1;
    }
    cx += 4;
    ez_text(fb, cx, y + 1, EZ_T12, s->on_surface_variant, 215, label);
    cx += ez_text_width(EZ_T12, label);
    return cx + 16;
}

/* ------------------------------- library ------------------------------- */

static void draw_card(ez_fb *fb, const ez_scheme *s, const ez_video *v, ez_rect r,
                      int focus)
{
    int rad = focus ? 18 : 15;
    const ez_image *art = ez_art(v->art);

    if (focus) {
        ez_round(fb, (ez_rect){r.x + 2, r.y + 5, r.w - 4, r.h}, rad, (ez_rgb){0, 0, 0}, 80);
        ez_glow(fb, r.x + r.w / 2, r.y + r.h / 2, r.w / 2 + 18, (ez_rgb){0, 0, 0}, 55);
    }
    if (art) ez_image_round(fb, art, r, rad, focus ? 255 : 230);
    else ez_round(fb, r, rad, s->surface_container_high, 255);

    /* bottom scrim for the duration pill */
    ez_round_grad(fb, (ez_rect){r.x, r.y + r.h - r.h / 2 - 10, r.w, r.h / 2 + 10}, rad,
                  (ez_rgb){0, 0, 0}, 0, (ez_rgb){0, 0, 0}, focus ? 170 : 120);

    if (!focus) ez_fill(fb, r, s->surface, 45);

    int label_w = ez_text_width(EZ_T12, v->duration) + 14;
    ez_rect pill = {r.x + r.w - 8 - label_w, r.y + r.h - 26, label_w, 18};
    ez_round(fb, pill, 9, (ez_rgb){0, 0, 0}, 120);
    text_mid(fb, pill.x + 7, pill.y + pill.h / 2, EZ_T12, WHITE, 235, v->duration);

    if (focus) ez_round_stroke(fb, r, rad, 2, s->primary, 240);
}

static void library_screen(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    int n = ez_video_count();
    const ez_video *cur = ez_video_get(a->focus);
    background(fb, s, ez_art(cur->art));

    ez_text(fb, 20, 8, EZ_T24M, s->on_surface, 255, "Медиатека");
    char sub[48];
    snprintf(sub, sizeof sub, "%d видео · 2 ч 14 мин", n);
    ez_text(fb, 20, 36, EZ_T12, s->on_surface_variant, 225, sub);
    icon_button(fb, s, 444, 26, 16, ez_icon_cp("palette"), 0);

    /* tabs */
    static const char *TABS[3] = {"Видео", "Недавние", "Плейлисты"};
    static const char *TAB_ICONS[3] = {"movie", "history", "playlist_play"};
    int tx = 20;
    for (int i = 0; i < 3; i++) {
        tx += chip(fb, s, tx, 60, 27, TABS[i], ez_icon_cp(TAB_ICONS[i]), a->tab == i, 255) + 8;
    }

    /* carousel: back to front */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            int focus = (i == a->focus);
            if ((pass == 0) == !!focus) continue;
            float off = (float)i - a->scroll;
            float dist = fabsf(off);
            if (dist > 2.6f) continue;
            float scale = focus ? 1.0f : 0.90f;
            int w = (int)(CARD_W * scale);
            int h = (int)(CARD_H * scale);
            int x = (int)(240.0f - CARD_W * 0.5f + off * (CARD_W + CARD_GAP) + (CARD_W - w) * 0.5f);
            int y = 88 + (focus ? 0 : 5);
            if (x > EZ_SCREEN_W + 8 || x + w < -8) continue;
            ez_rect r = {x, y, w, h};
            ez_clip_set(fb, (ez_rect){0, 80, EZ_SCREEN_W, 120});
            draw_card(fb, s, ez_video_get(i), r, focus);
            ez_clip_reset(fb);
        }
    }

    /* page dots */
    int total = 0;
    for (int i = 0; i < n; i++) total += (i == a->focus ? 14 : 5) + 5;
    total -= 5;
    int dx = 240 - total / 2;
    for (int i = 0; i < n; i++) {
        int w = (i == a->focus) ? 14 : 5;
        ez_round(fb, (ez_rect){dx, 190, w, 5}, 2,
                 i == a->focus ? s->primary : s->on_surface_variant,
                 i == a->focus ? 255 : 130);
        dx += w + 5;
    }

    /* focused item info */
    ez_text(fb, 20, 202, EZ_T16M, s->on_surface, 255, cur->title);
    ez_text(fb, 20, 226, EZ_T12, s->on_surface_variant, 225, cur->meta);

    /* CTA */
    {
        const char *label = "Смотреть";
        int tw = ez_text_width(EZ_T12M, label);
        int w = 11 + 16 + 5 + tw + 11;
        int x = 460 - w;
        ez_rect r = {x, 200, w, 27};
        ez_round(fb, r, 13, s->primary, 255);
        ez_icon_center(fb, x + 11 + 8, r.y + 13, EZ_I14, ez_icon_cp("play_arrow"),
                       s->on_primary, 255);
        text_mid(fb, x + 11 + 16 + 5, r.y + 13, EZ_T12M, s->on_primary, 255, label);
    }

    /* hints */
    int hx = 20;
    hx = hint(fb, s, hx, 250, ez_icon_cp("chevron_left"), ez_icon_cp("chevron_right"), "выбор");
    hx = hint(fb, s, hx, 250, ez_icon_cp("play_arrow"), 0, "играть");
    hx = hint(fb, s, hx, 250, ez_icon_cp("palette"), 0, "тема");
    hint(fb, s, hx, 250, ez_icon_cp("settings"), 0, "меню");
}

/* ------------------------------- settings ------------------------------ */

static void theme_tile(ez_fb *fb, ez_rect r, int index, int selected,
                       const ez_image *art, int light)
{
    ez_scheme ts = ez_theme_scheme(index, art, light);
    if (index == EZ_THEME_DYNAMIC && art) {
        ez_image_round(fb, art, r, 14, 255);
        ez_round(fb, r, 14, ts.surface, 120);
    } else {
        ez_round_grad(fb, r, 14, ts.primary_container, 255, ts.surface_container, 255);
    }
    ez_text(fb, r.x + 10, r.y + 7, EZ_T12M, ts.on_surface, 245, ez_theme_name(index));
    ez_round(fb, (ez_rect){r.x + 10, r.y + r.h - 26, 22, 22}, 11, ts.primary, 255);
    ez_round(fb, (ez_rect){r.x + 36, r.y + r.h - 20, 12, 12}, 6, ts.tertiary, 255);
    if (selected) {
        ez_round_stroke(fb, r, 14, 2, ts.primary, 255);
        ez_icon_center(fb, r.x + r.w - 15, r.y + 15, EZ_I14, ez_icon_cp("check"),
                       ts.on_surface, 235);
    }
}

static void switch_widget(ez_fb *fb, const ez_scheme *s, int x, int y, int on)
{
    int w = 44, h = 24;
    ez_round(fb, (ez_rect){x, y, w, h}, 12, on ? s->primary : s->surface_container_highest, 255);
    if (!on) ez_round_stroke(fb, (ez_rect){x, y, w, h}, 12, 1, s->outline, 210);
    int kx = on ? x + w - 4 - 18 : x + 4;
    ez_round(fb, (ez_rect){kx, y + 3, 18, 18}, 9, on ? s->on_primary : s->outline, 255);
}

static void settings_screen(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    const ez_image *art = ez_art(ez_video_get(a->focus)->art);
    background(fb, s, art);

    icon_button(fb, s, 36, 26, 16, ez_icon_cp("arrow_back"), 0);
    ez_text(fb, 64, 8, EZ_T24M, s->on_surface, 255, "Настройки");

    ez_text(fb, 20, 50, EZ_T12, s->on_surface_variant, 225, "ЦВЕТОВАЯ СХЕМА");

    int tw = 102, th = 58, gap = 10, x0 = 20, y0 = 66;
    for (int i = 0; i < EZ_THEME_COUNT; i++) {
        int col = i % 4, row = i / 4;
        ez_rect r = {x0 + col * (tw + gap), y0 + row * (th + 8), tw, th};
        theme_tile(fb, r, i, i == a->theme, art, a->light);
    }

    ez_text(fb, 20, 200, EZ_T16, s->on_surface, 255, "Светлая тема");
    ez_text(fb, 20, 220, EZ_T12, s->on_surface_variant, 210,
            a->light ? "светлые поверхности" : "тёмные поверхности");
    switch_widget(fb, s, 416, 202, a->light);

    int hx = 20;
    hx = hint(fb, s, hx, 246, ez_icon_cp("chevron_left"), ez_icon_cp("chevron_right"), "палитра");
    hx = hint(fb, s, hx, 246, ez_icon_cp("play_arrow"), 0, "светлая тема");
    hint(fb, s, hx, 246, ez_icon_cp("close"), 0, "назад");
}

/* -------------------------------- player ------------------------------- */

static void fmt_time(char *buf, size_t n, int secs)
{
    if (secs < 0) secs = 0;
    int h = secs / 3600;
    int m = (secs / 60) % 60;
    int sec = secs % 60;
    if (h > 0) snprintf(buf, n, "%d:%02d:%02d", h, m, sec);
    else snprintf(buf, n, "%d:%02d", m, sec);
}

static void player_screen(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    const ez_video *v = ez_video_get(a->focus);
    const ez_image *art = ez_art(v->art);

    ez_fill(fb, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, (ez_rgb){0, 0, 0}, 255);
    if (art) ez_image_cover(fb, art, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, 255);
    ez_fill(fb, (ez_rect){0, 0, EZ_SCREEN_W, EZ_SCREEN_H}, (ez_rgb){0, 0, 0}, 70);
    ez_vgrad(fb, (ez_rect){0, 0, EZ_SCREEN_W, 92}, (ez_rgb){0, 0, 0}, 200, (ez_rgb){0, 0, 0}, 0);
    ez_vgrad(fb, (ez_rect){0, 120, EZ_SCREEN_W, 152}, (ez_rgb){0, 0, 0}, 0,
             (ez_rgb){0, 0, 0}, 225);

    ez_text(fb, 20, 12, EZ_T16M, WHITE, 255, v->title);
    ez_text(fb, 20, 36, EZ_T12, WHITE, 190, v->meta);

    int px = 24, py = 204, pw = 432;
    ez_round(fb, (ez_rect){px, py, pw, 4}, 2, WHITE, 65);
    int fw = (int)(pw * a->progress);
    if (fw > 0) ez_round(fb, (ez_rect){px, py, fw, 4}, 2, s->primary, 255);
    ez_round(fb, (ez_rect){px + fw - 5, py - 3, 10, 10}, 5, s->primary, 255);

    char cur[24], left[24];
    int played = (int)(v->secs * a->progress);
    fmt_time(cur, sizeof cur, played);
    fmt_time(left, sizeof left, v->secs - played);
    ez_text(fb, px, py + 10, EZ_T12, WHITE, 210, cur);
    ez_text_right(fb, px + pw, py + 10, EZ_T12, WHITE, 210, left);

    int cy = 244;
    ez_icon_center(fb, 194, cy, EZ_I24, ez_icon_cp("skip_previous"), WHITE, 230);
    ez_round(fb, (ez_rect){240 - 20, cy - 20, 40, 40}, 20, s->primary, 255);
    ez_icon_center(fb, 240, cy, EZ_I24,
                   ez_icon_cp(a->playing ? "pause" : "play_arrow"), s->on_primary, 255);
    ez_icon_center(fb, 286, cy, EZ_I24, ez_icon_cp("skip_next"), WHITE, 230);

    int hx = 20;
    hx = hint(fb, s, hx, 252, ez_icon_cp("chevron_left"), ez_icon_cp("chevron_right"), "±10 сек");
    hint(fb, s, hx, 252, ez_icon_cp("close"), 0, "стоп");
}

/* -------------------------------- toast -------------------------------- */

void ez_ui_toast(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    if (a->toast <= 0.01f) return;
    int alpha = (int)(a->toast * 255.0f);
    int w = ez_text_width(EZ_T12, a->toast_text) + 30;
    int x = 240 - w / 2, y = 226, h = 26;
    ez_round(fb, (ez_rect){x, y, w, h}, 13, s->inverse_surface, (alpha * 235) / 255);
    text_mid(fb, x + 15, y + h / 2, EZ_T12, s->inverse_on_surface, alpha, a->toast_text);
}

/* ------------------------------ dispatch ------------------------------- */

void ez_ui_library(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    library_screen(a, fb, s);
}

void ez_ui_settings(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    settings_screen(a, fb, s);
}

void ez_ui_player(ez_app *a, ez_fb *fb, const ez_scheme *s)
{
    player_screen(a, fb, s);
}
