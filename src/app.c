#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ezt.h"
#include "ezt_ui.h"

static ez_scheme target_scheme(const ez_app *a)
{
    const ez_video *v = ez_video_get(a->focus);
    return ez_theme_scheme(a->theme, v ? ez_art(v->art) : 0, a->light);
}

static void change_palette(ez_app *a)
{
    a->scheme_prev = a->scheme;
    a->theme = (a->theme + 1) % EZ_THEME_COUNT;
    a->scheme = target_scheme(a);
    a->palette_t = 0.0f;
    ez_app_toast(a, "%s", ez_theme_name(a->theme));
}

static void update_scheme(ez_app *a)
{
    ez_scheme next = target_scheme(a);
    /* A changed focus affects only the dynamic palette; detect it cheaply. */
    if (a->theme == EZ_THEME_DYNAMIC) {
        if (memcmp(&next, &a->scheme, sizeof next) != 0 && a->palette_t >= 1.0f) {
            a->scheme_prev = a->scheme;
            a->scheme = next;
            a->palette_t = 0.0f;
        }
    }
}

void ez_app_init(ez_app *a)
{
    memset(a, 0, sizeof(*a));
    ez_library_init();
    ez_assets_init();
    a->screen = EZ_SCREEN_LIBRARY;
    a->theme = 0;
    a->light = 0;
    a->playing = 0;
    a->focus = 0;
    a->scroll = 0.0f;
    a->progress = ez_video_get(0)->progress;
    a->scheme = target_scheme(a);
    a->scheme_prev = a->scheme;
    a->palette_t = 1.0f;
}

void ez_app_update(ez_app *a, float dt)
{
    if (dt < 0) dt = 0;
    if (dt > 0.1f) dt = 0.1f;
    update_scheme(a);
    a->scroll = ez_approach(a->scroll, (float)a->focus, dt, 10.0f);
    if (a->palette_t < 1.0f) {
        a->palette_t += dt * 4.8f;
        if (a->palette_t >= 1.0f) {
            a->palette_t = 1.0f;
            a->scheme_prev = a->scheme;
        }
    }
    if (a->toast > 0.0f) a->toast -= dt * 1.5f;
    if (a->toast < 0.0f) a->toast = 0.0f;
    if (a->screen == EZ_SCREEN_PLAYER && a->playing) {
        const ez_video *v = ez_video_get(a->focus);
        if (v && v->secs > 0) {
            a->progress += dt / (float)v->secs;
            if (a->progress >= 1.0f) {
                a->progress = 1.0f;
                a->playing = 0;
                ez_app_toast(a, "Видео завершено");
            }
        }
    }
    if (a->open < 1.0f) a->open += dt * 4.0f;
    if (a->open > 1.0f) a->open = 1.0f;
}

void ez_app_toast(ez_app *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->toast_text, sizeof a->toast_text, fmt, ap);
    va_end(ap);
    a->toast = 1.0f;
}

ez_rgb ez_app_art_seed(const ez_app *a)
{
    const ez_video *v = ez_video_get(a->focus);
    const ez_image *im = v ? ez_art(v->art) : 0;
    return ez_seed_from_image(im);
}

static void focus_video(ez_app *a, int delta)
{
    int n = ez_video_count();
    if (n <= 0) return;
    int old = a->focus;
    a->focus = (a->focus + delta + n) % n;
    if (a->focus != old) {
        const ez_video *v = ez_video_get(a->focus);
        a->progress = v ? v->progress : 0.0f;
        a->playing = 0;
        if (a->theme == EZ_THEME_DYNAMIC) {
            a->scheme_prev = a->scheme;
            a->scheme = target_scheme(a);
            a->palette_t = 0.0f;
        }
    }
}

void ez_app_action(ez_app *a, int action)
{
    switch (action) {
    case EZ_ACT_LEFT:
        if (a->screen == EZ_SCREEN_LIBRARY) focus_video(a, -1);
        else if (a->screen == EZ_SCREEN_SETTINGS) change_palette(a);
        else {
            a->progress -= 10.0f / ez_video_get(a->focus)->secs;
            if (a->progress < 0) a->progress = 0;
            ez_app_toast(a, "−10 сек");
        }
        break;
    case EZ_ACT_RIGHT:
        if (a->screen == EZ_SCREEN_LIBRARY) focus_video(a, 1);
        else if (a->screen == EZ_SCREEN_SETTINGS) change_palette(a);
        else {
            a->progress += 10.0f / ez_video_get(a->focus)->secs;
            if (a->progress > 1) a->progress = 1;
            ez_app_toast(a, "+10 сек");
        }
        break;
    case EZ_ACT_UP:
        if (a->screen == EZ_SCREEN_LIBRARY) a->tab = (a->tab + 2) % 3;
        else if (a->screen == EZ_SCREEN_SETTINGS) change_palette(a);
        break;
    case EZ_ACT_DOWN:
        if (a->screen == EZ_SCREEN_LIBRARY) a->tab = (a->tab + 1) % 3;
        else if (a->screen == EZ_SCREEN_SETTINGS) change_palette(a);
        break;
    case EZ_ACT_CONFIRM:
        if (a->screen == EZ_SCREEN_LIBRARY) {
            if (a->tab == 2) ez_app_toast(a, "Плейлисты пока пусты");
            else {
                a->screen = EZ_SCREEN_PLAYER;
                a->playing = 1;
                a->open = 0;
            }
        } else if (a->screen == EZ_SCREEN_SETTINGS) {
            a->light = !a->light;
            a->scheme_prev = a->scheme;
            a->scheme = target_scheme(a);
            a->palette_t = 0.0f;
            ez_app_toast(a, a->light ? "Светлая тема" : "Тёмная тема");
        } else {
            a->playing = !a->playing;
        }
        break;
    case EZ_ACT_BACK:
        if (a->screen == EZ_SCREEN_PLAYER) {
            a->screen = EZ_SCREEN_LIBRARY;
            a->playing = 0;
        } else if (a->screen == EZ_SCREEN_SETTINGS) {
            a->screen = EZ_SCREEN_LIBRARY;
        }
        break;
    case EZ_ACT_TAB_NEXT:
        if (a->screen == EZ_SCREEN_LIBRARY) a->tab = (a->tab + 1) % 3;
        else if (a->screen == EZ_SCREEN_SETTINGS) change_palette(a);
        else if (a->screen == EZ_SCREEN_PLAYER) {
            a->playing = 0;
            focus_video(a, 1);
            a->screen = EZ_SCREEN_PLAYER;
            a->playing = 1;
        }
        break;
    case EZ_ACT_TAB_PREV:
        if (a->screen == EZ_SCREEN_LIBRARY) a->tab = (a->tab + 2) % 3;
        else if (a->screen == EZ_SCREEN_SETTINGS) {
            a->theme = (a->theme + EZ_THEME_COUNT - 1) % EZ_THEME_COUNT;
            a->scheme_prev = a->scheme;
            a->scheme = target_scheme(a);
            a->palette_t = 0;
            ez_app_toast(a, "%s", ez_theme_name(a->theme));
        } else {
            a->playing = 0;
            focus_video(a, -1);
            a->screen = EZ_SCREEN_PLAYER;
            a->playing = 1;
        }
        break;
    case EZ_ACT_THEME:
        change_palette(a);
        break;
    case EZ_ACT_MENU:
        if (a->screen == EZ_SCREEN_SETTINGS) a->screen = EZ_SCREEN_LIBRARY;
        else if (a->screen == EZ_SCREEN_PLAYER) ez_app_toast(a, "L/R: видео · ○: назад");
        else a->screen = EZ_SCREEN_SETTINGS;
        break;
    default:
        break;
    }
}

void ez_app_render(ez_app *a, ez_fb *fb)
{
    ez_scheme draw = a->scheme;
    if (a->palette_t < 1.0f) {
        float t = ez_ease_in_out(ez_clamp01(a->palette_t));
        draw = ez_scheme_lerp(&a->scheme_prev, &a->scheme, t);
    }
    if (a->screen == EZ_SCREEN_SETTINGS) ez_ui_settings(a, fb, &draw);
    else if (a->screen == EZ_SCREEN_PLAYER) ez_ui_player(a, fb, &draw);
    else ez_ui_library(a, fb, &draw);
    ez_ui_toast(a, fb, &draw);
}
