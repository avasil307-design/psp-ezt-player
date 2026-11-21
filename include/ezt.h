/*
 * EZT Player - Material You video player for PSP (homebrew).
 * Portable core: software renderer + UI, buildable on host for previews.
 */
#ifndef EZT_H
#define EZT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================ basics ================================== */

typedef struct { uint8_t r, g, b; } ez_rgb;
typedef struct { int x, y, w, h; } ez_rect;
typedef struct { int w, h; const uint16_t *px; } ez_image; /* RGB565 */

#define EZ_SCREEN_W 480
#define EZ_SCREEN_H 272

/* ============================ theme =================================== */

#define EZ_ROLE_PLUS(name) + 1

#define EZ_ROLES(X)                    \
    X(primary)                         \
    X(on_primary)                      \
    X(primary_container)               \
    X(on_primary_container)            \
    X(secondary)                       \
    X(on_secondary)                    \
    X(secondary_container)             \
    X(on_secondary_container)          \
    X(tertiary)                        \
    X(on_tertiary)                     \
    X(tertiary_container)              \
    X(on_tertiary_container)           \
    X(surface)                         \
    X(surface_dim)                     \
    X(surface_bright)                  \
    X(surface_container_lowest)        \
    X(surface_container_low)           \
    X(surface_container)               \
    X(surface_container_high)          \
    X(surface_container_highest)       \
    X(on_surface)                      \
    X(on_surface_variant)              \
    X(outline)                         \
    X(outline_variant)                 \
    X(error)                           \
    X(on_error)                        \
    X(scrim)                           \
    X(inverse_surface)                 \
    X(inverse_on_surface)

enum { EZ_ROLE_COUNT = 0 EZ_ROLES(EZ_ROLE_PLUS) };

typedef union ez_scheme {
    struct {
#define EZ_ROLE_DEF(name) ez_rgb name;
        EZ_ROLES(EZ_ROLE_DEF)
#undef EZ_ROLE_DEF
    };
    ez_rgb c[EZ_ROLE_COUNT];
} ez_scheme;

/* Build a Material-3-like scheme from a seed colour (OKLCH tonal palettes). */
ez_scheme ez_scheme_seed(ez_rgb seed, int light);
ez_scheme ez_scheme_lerp(const ez_scheme *a, const ez_scheme *b, float t);
/* Dominant, slightly saturated colour of an image, usable as a seed. */
ez_rgb ez_seed_from_image(const ez_image *im);

#define EZ_THEME_COUNT 7
#define EZ_THEME_DYNAMIC 6
const char *ez_theme_name(int index);
ez_rgb ez_theme_seed(int index);
ez_scheme ez_theme_scheme(int index, const ez_image *art, int light);

/* ========================= framebuffer ================================ */

typedef struct ez_fb {
    int w, h;
    uint32_t *px; /* 0xAARRGGBB, opaque by default */
    ez_rect clip;
} ez_fb;

void ez_fb_init(ez_fb *fb, int w, int h, uint32_t *px);
void ez_clip_reset(ez_fb *fb);
void ez_clip_set(ez_fb *fb, ez_rect r);

void ez_clear(ez_fb *fb, ez_rgb c);
void ez_fill(ez_fb *fb, ez_rect r, ez_rgb c, int alpha);
void ez_vgrad(ez_fb *fb, ez_rect r, ez_rgb top, int a_top, ez_rgb bot, int a_bot);
void ez_glow(ez_fb *fb, int cx, int cy, int radius, ez_rgb c, int alpha);
void ez_round(ez_fb *fb, ez_rect r, int radius, ez_rgb c, int alpha);
void ez_round_grad(ez_fb *fb, ez_rect r, int radius,
                   ez_rgb top, int a_top, ez_rgb bot, int a_bot);
void ez_round_stroke(ez_fb *fb, ez_rect r, int radius, int width, ez_rgb c, int alpha);
void ez_image_round(ez_fb *fb, const ez_image *im, ez_rect dst, int radius, int alpha);
void ez_image_cover(ez_fb *fb, const ez_image *im, ez_rect dst, int alpha);
void ez_blit_a8(ez_fb *fb, const uint8_t *mask, int aw, int ah,
                ez_rect src, ez_rect dst, ez_rgb c, int opacity);

/* ============================ text ==================================== */

typedef struct { int16_t ascent, descent; } ez_style_metric;

typedef struct ez_glyph {
    uint8_t style;
    uint16_t cp;
    int16_t ox, oy;
    uint16_t ax, ay;
    uint8_t w, h;
    int16_t adv;
} ez_glyph;

void ez_text_init(const uint8_t *atlas);
int ez_text_width(int style, const char *utf8);
int ez_text_height(int style);
int ez_icon_width(int style, uint16_t cp);
void ez_text(ez_fb *fb, int x, int y, int style, ez_rgb c, int alpha, const char *utf8);
void ez_text_right(ez_fb *fb, int right, int y, int style, ez_rgb c, int alpha, const char *utf8);
void ez_text_center(ez_fb *fb, int cx, int y, int style, ez_rgb c, int alpha, const char *utf8);
void ez_icon(ez_fb *fb, int x, int y, int style, uint16_t cp, ez_rgb c, int alpha);
void ez_icon_center(ez_fb *fb, int cx, int cy, int style, uint16_t cp, ez_rgb c, int alpha);
uint16_t ez_icon_cp(const char *name); /* "play_arrow" -> codepoint, 0 if unknown */

/* =========================== easing =================================== */

float ez_clamp01(float v);
float ez_lerp(float a, float b, float t);
float ez_approach(float cur, float target, float dt, float speed);
float ez_ease_out_cubic(float t);
float ez_ease_in_out(float t);
float ez_ease_out_back(float t);

/* =========================== library ================================== */

typedef struct ez_video {
    const char *title;
    const char *meta;      /* "24:16 · 1080p" */
    int art;               /* index into demo art */
    const char *duration;  /* "24:16" */
    float progress;        /* 0..1 */
    int secs;              /* total duration in seconds */
} ez_video;

void ez_library_init(void);
int ez_video_count(void);
const ez_video *ez_video_get(int i);
const ez_image *ez_art(int i);
int ez_art_count(void);

/* ============================= app ==================================== */

enum {
    EZ_SCREEN_LIBRARY = 0,
    EZ_SCREEN_SETTINGS,
    EZ_SCREEN_PLAYER
};

enum {
    EZ_ACT_LEFT = 0,
    EZ_ACT_RIGHT,
    EZ_ACT_UP,
    EZ_ACT_DOWN,
    EZ_ACT_CONFIRM,
    EZ_ACT_BACK,
    EZ_ACT_TAB_NEXT,
    EZ_ACT_TAB_PREV,
    EZ_ACT_THEME,
    EZ_ACT_MENU
};

typedef struct ez_app {
    int screen;
    int tab;
    int focus;
    int theme;
    int light;
    int playing;
    float progress;
    float scroll;      /* animated carousel position */
    float palette_t;   /* 0..1 palette cross-fade */
    float open;        /* 0..1 player open animation */
    float toast;       /* 0..1 toast alpha */
    char toast_text[64];
    ez_scheme scheme;
    ez_scheme scheme_prev;
    int tab_focus;
} ez_app;

void ez_app_init(ez_app *a);
void ez_app_update(ez_app *a, float dt);
void ez_app_action(ez_app *a, int action);
void ez_app_render(ez_app *a, ez_fb *fb);
void ez_app_toast(ez_app *a, const char *fmt, ...);
ez_rgb ez_app_art_seed(const ez_app *a);

/* =========================== assets =================================== */

void ez_assets_init(void);
const uint8_t *ez_atlas_data(size_t *len);
const uint8_t *ez_art_data(size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* EZT_H */
