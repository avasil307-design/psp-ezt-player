/* Material-3 style tonal palettes generated from a seed colour via OKLCH. */
#include <math.h>

#include "ezt.h"

#define EZ_PI 3.14159265358979f

static float srgb_to_lin(float c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static float lin_to_srgb(float c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

typedef struct { float l, a, b; } oklab;
typedef struct { float l, c, h; } oklch;

static oklab oklab_from_lin(float r, float g, float b)
{
    float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
    float l_ = cbrtf(l), m_ = cbrtf(m), s_ = cbrtf(s);
    oklab v;
    v.l = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
    v.a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
    v.b = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
    return v;
}

static void oklab_to_lin(oklab v, float *r, float *g, float *b)
{
    float l_ = v.l + 0.3963377774f * v.a + 0.2158037573f * v.b;
    float m_ = v.l - 0.1055613458f * v.a - 0.0638541728f * v.b;
    float s_ = v.l - 0.0894841775f * v.a - 1.2914855480f * v.b;
    float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    *r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    *g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    *b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

static oklch lch_from_rgb(ez_rgb c)
{
    float r = srgb_to_lin(c.r / 255.0f);
    float g = srgb_to_lin(c.g / 255.0f);
    float b = srgb_to_lin(c.b / 255.0f);
    oklab v = oklab_from_lin(r, g, b);
    oklch out;
    out.l = v.l;
    out.c = sqrtf(v.a * v.a + v.b * v.b);
    float h = atan2f(v.b, v.a) * 180.0f / EZ_PI;
    if (h < 0) h += 360.0f;
    out.h = h;
    return out;
}

static ez_rgb rgb_from_lch_ex(float l, float c, float h, int *in_gamut)
{
    float rad = h * EZ_PI / 180.0f;
    oklab v = {l, c * cosf(rad), c * sinf(rad)};
    float r, g, b;
    oklab_to_lin(v, &r, &g, &b);
    int ok = r >= -0.0005f && r <= 1.0005f && g >= -0.0005f && g <= 1.0005f &&
             b >= -0.0005f && b <= 1.0005f;
    if (in_gamut) *in_gamut = ok;
    float sr = lin_to_srgb(r), sg = lin_to_srgb(g), sb = lin_to_srgb(b);
    ez_rgb out;
    out.r = (uint8_t)(sr <= 0 ? 0 : sr >= 1 ? 255 : sr * 255.0f + 0.5f);
    out.g = (uint8_t)(sg <= 0 ? 0 : sg >= 1 ? 255 : sg * 255.0f + 0.5f);
    out.b = (uint8_t)(sb <= 0 ? 0 : sb >= 1 ? 255 : sb * 255.0f + 0.5f);
    return out;
}

/* tone in 0..100, chroma is OKLCH chroma, hue in degrees; gamut-safe. */
static ez_rgb tone(float hue, float chroma, float tone_v)
{
    float l = tone_v / 100.0f;
    if (l <= 0.0f) return (ez_rgb){0, 0, 0};
    if (l >= 1.0f) return (ez_rgb){255, 255, 255};
    int ok = 0;
    ez_rgb direct = rgb_from_lch_ex(l, chroma, hue, &ok);
    if (ok) return direct;
    float lo = 0.0f, hi = chroma;
    for (int i = 0; i < 12; i++) {
        float mid = (lo + hi) * 0.5f;
        rgb_from_lch_ex(l, mid, hue, &ok);
        if (ok) lo = mid;
        else hi = mid;
    }
    return rgb_from_lch_ex(l, lo, hue, 0);
}

static ez_scheme make_scheme(ez_rgb seed, int light)
{
    oklch sc = lch_from_rgb(seed);
    float h = sc.h;
    float c = sc.c;
    if (c < 0.03f) c = 0.03f;
    if (c > 0.20f) c = 0.20f;

    float p = c;
    float sec = c * 0.45f;
    float ter = c * 0.62f;
    float ht = h + 62.0f;
    float n = c * 0.05f;
    float nv = c * 0.11f;
    float eh = 27.0f, ec = 0.17f;

    ez_scheme s;
    if (!light) {
        s.primary = tone(h, p, 80);
        s.on_primary = tone(h, p * 0.30f, 20);
        s.primary_container = tone(h, p * 0.75f, 30);
        s.on_primary_container = tone(h, p * 0.28f, 90);
        s.secondary = tone(h, sec, 80);
        s.on_secondary = tone(h, sec * 0.35f, 20);
        s.secondary_container = tone(h, sec, 30);
        s.on_secondary_container = tone(h, sec * 0.30f, 90);
        s.tertiary = tone(ht, ter, 80);
        s.on_tertiary = tone(ht, ter * 0.35f, 20);
        s.tertiary_container = tone(ht, ter, 30);
        s.on_tertiary_container = tone(ht, ter * 0.30f, 90);
        s.surface = tone(h, n, 6);
        s.surface_dim = tone(h, n, 6);
        s.surface_bright = tone(h, n, 24);
        s.surface_container_lowest = tone(h, n, 4);
        s.surface_container_low = tone(h, n, 10);
        s.surface_container = tone(h, n, 12);
        s.surface_container_high = tone(h, n, 17);
        s.surface_container_highest = tone(h, n, 22);
        s.on_surface = tone(h, n, 90);
        s.on_surface_variant = tone(h, nv, 80);
        s.outline = tone(h, nv, 60);
        s.outline_variant = tone(h, nv, 30);
        s.error = tone(eh, ec, 80);
        s.on_error = tone(eh, ec * 0.4f, 20);
        s.scrim = (ez_rgb){0, 0, 0};
        s.inverse_surface = tone(h, n, 90);
        s.inverse_on_surface = tone(h, n, 20);
    } else {
        s.primary = tone(h, p, 40);
        s.on_primary = tone(h, p * 0.25f, 100);
        s.primary_container = tone(h, p * 0.75f, 90);
        s.on_primary_container = tone(h, p * 0.40f, 10);
        s.secondary = tone(h, sec, 40);
        s.on_secondary = tone(h, sec * 0.25f, 100);
        s.secondary_container = tone(h, sec, 90);
        s.on_secondary_container = tone(h, sec * 0.40f, 10);
        s.tertiary = tone(ht, ter, 40);
        s.on_tertiary = tone(ht, ter * 0.25f, 100);
        s.tertiary_container = tone(ht, ter, 90);
        s.on_tertiary_container = tone(ht, ter * 0.40f, 10);
        s.surface = tone(h, n, 98);
        s.surface_dim = tone(h, n, 87);
        s.surface_bright = tone(h, n, 98);
        s.surface_container_lowest = tone(h, n, 100);
        s.surface_container_low = tone(h, n, 96);
        s.surface_container = tone(h, n, 94);
        s.surface_container_high = tone(h, n, 92);
        s.surface_container_highest = tone(h, n, 90);
        s.on_surface = tone(h, n, 10);
        s.on_surface_variant = tone(h, nv, 30);
        s.outline = tone(h, nv, 50);
        s.outline_variant = tone(h, nv, 80);
        s.error = tone(eh, ec, 40);
        s.on_error = (ez_rgb){255, 255, 255};
        s.scrim = (ez_rgb){0, 0, 0};
        s.inverse_surface = tone(h, n, 20);
        s.inverse_on_surface = tone(h, n, 95);
    }
    return s;
}

ez_scheme ez_scheme_seed(ez_rgb seed, int light)
{
    return make_scheme(seed, light);
}

ez_scheme ez_scheme_lerp(const ez_scheme *a, const ez_scheme *b, float t)
{
    ez_scheme out;
    for (int i = 0; i < EZ_ROLE_COUNT; i++) {
        out.c[i].r = (uint8_t)(a->c[i].r + (b->c[i].r - (int)a->c[i].r) * t);
        out.c[i].g = (uint8_t)(a->c[i].g + (b->c[i].g - (int)a->c[i].g) * t);
        out.c[i].b = (uint8_t)(a->c[i].b + (b->c[i].b - (int)a->c[i].b) * t);
    }
    return out;
}

ez_rgb ez_seed_from_image(const ez_image *im)
{
    if (!im || !im->px || im->w <= 0 || im->h <= 0) return (ez_rgb){103, 80, 164};

    double wr = 0, wg = 0, wb = 0, wsum = 0;
    double ar = 0, ag = 0, ab = 0;
    int n = 0;
    for (int y = 0; y < im->h; y += 2) {
        for (int x = 0; x < im->w; x += 2) {
            uint16_t v = im->px[y * im->w + x];
            int ri = (v >> 11) & 31, gi = (v >> 5) & 63, bi = v & 31;
            double r = ((ri << 3) | (ri >> 2)) / 255.0;
            double g = ((gi << 2) | (gi >> 4)) / 255.0;
            double b = ((bi << 3) | (bi >> 2)) / 255.0;
            double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
            double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
            double sat = mx > 0.001 ? (mx - mn) / mx : 0;
            double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            double w = sat * sat * sat * (0.35 + lum);
            wr += r * w;
            wg += g * w;
            wb += b * w;
            wsum += w;
            ar += r;
            ag += g;
            ab += b;
            n++;
        }
    }
    ez_rgb seed;
    if (wsum > 1e-5) {
        seed.r = (uint8_t)(wr / wsum * 255.0);
        seed.g = (uint8_t)(wg / wsum * 255.0);
        seed.b = (uint8_t)(wb / wsum * 255.0);
    } else if (n > 0) {
        seed.r = (uint8_t)(ar / n * 255.0);
        seed.g = (uint8_t)(ag / n * 255.0);
        seed.b = (uint8_t)(ab / n * 255.0);
    } else {
        seed = (ez_rgb){103, 80, 164};
    }
    /* Push the seed towards a vivid, mid-tone colour so every palette works. */
    oklch sc = lch_from_rgb(seed);
    float c = sc.c;
    if (c < 0.11f) c = 0.11f;
    if (c > 0.17f) c = 0.17f;
    float l = sc.l;
    if (l < 0.55f) l = 0.55f;
    if (l > 0.72f) l = 0.72f;
    return rgb_from_lch_ex(l, c, sc.h, 0);
}

/* ---------------- palette list ---------------- */

static const struct {
    const char *name;
    uint32_t hex;
} THEMES[EZ_THEME_COUNT] = {
    {"Лаванда", 0x6750A4},
    {"Океан", 0x00639B},
    {"Бирюза", 0x00696D},
    {"Хвоя", 0x3F6833},
    {"Закат", 0x9A4600},
    {"Роза", 0x9C4146},
    {"Динамическая", 0x7D5260},
};

const char *ez_theme_name(int index)
{
    if (index < 0 || index >= EZ_THEME_COUNT) index = 0;
    return THEMES[index].name;
}

ez_rgb ez_theme_seed(int index)
{
    if (index < 0 || index >= EZ_THEME_COUNT) index = 0;
    uint32_t v = THEMES[index].hex;
    ez_rgb c = {(uint8_t)((v >> 16) & 255), (uint8_t)((v >> 8) & 255), (uint8_t)(v & 255)};
    return c;
}

ez_scheme ez_theme_scheme(int index, const ez_image *art, int light)
{
    if (index == EZ_THEME_DYNAMIC) return make_scheme(ez_seed_from_image(art), light);
    return make_scheme(ez_theme_seed(index), light);
}
