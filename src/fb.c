/* Software rasterizer: rectangles, rounded shapes, gradients, images, glyph masks. */
#include <math.h>
#include <string.h>

#include "ezt.h"

static inline uint32_t pack_argb(ez_rgb c, int a)
{
    return ((uint32_t)(a & 255) << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static inline void blend_px(uint32_t *dst, ez_rgb c, int a)
{
    if (a <= 0) return;
    if (a >= 255) {
        *dst = 0xFF000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
        return;
    }
    uint32_t d = *dst;
    int ia = 255 - a;
    int r = ((int)c.r * a + (int)((d >> 16) & 255) * ia + 127) / 255;
    int g = ((int)c.g * a + (int)((d >> 8) & 255) * ia + 127) / 255;
    int b = ((int)c.b * a + (int)(d & 255) * ia + 127) / 255;
    *dst = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

static ez_rect clip_rect(ez_fb *fb, ez_rect r)
{
    int x0 = imax(r.x, fb->clip.x);
    int y0 = imax(r.y, fb->clip.y);
    int x1 = imin(r.x + r.w, fb->clip.x + fb->clip.w);
    int y1 = imin(r.y + r.h, fb->clip.y + fb->clip.h);
    ez_rect out = {x0, y0, x1 - x0, y1 - y0};
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

void ez_fb_init(ez_fb *fb, int w, int h, uint32_t *px)
{
    fb->w = w;
    fb->h = h;
    fb->px = px;
    ez_clip_reset(fb);
}

void ez_clip_reset(ez_fb *fb)
{
    ez_rect r = {0, 0, fb->w, fb->h};
    fb->clip = r;
}

void ez_clip_set(ez_fb *fb, ez_rect r)
{
    fb->clip = r;
}

void ez_clear(ez_fb *fb, ez_rgb c)
{
    uint32_t v = 0xFF000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
    int n = fb->w * fb->h;
    for (int i = 0; i < n; i++) fb->px[i] = v;
}

void ez_fill(ez_fb *fb, ez_rect r, ez_rgb c, int alpha)
{
    if (alpha <= 0) return;
    ez_rect a = clip_rect(fb, r);
    for (int y = a.y; y < a.y + a.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        for (int x = a.x; x < a.x + a.w; x++) blend_px(&row[x], c, alpha);
    }
}

static const float EZ_BAYER[16] = {
    0.0f, 8.0f, 2.0f, 10.0f,
    12.0f, 4.0f, 14.0f, 6.0f,
    3.0f, 11.0f, 1.0f, 9.0f,
    15.0f, 7.0f, 13.0f, 5.0f
};

static inline float dither_at(int x, int y)
{
    return (EZ_BAYER[((y & 3) << 2) | (x & 3)] / 16.0f - 0.47f);
}

void ez_vgrad(ez_fb *fb, ez_rect r, ez_rgb top, int a_top, ez_rgb bot, int a_bot)
{
    ez_rect a = clip_rect(fb, r);
    if (a.h <= 0 || a.w <= 0) return;
    float denom = (float)(r.h > 1 ? r.h - 1 : 1);
    for (int y = a.y; y < a.y + a.h; y++) {
        float t = (float)(y - r.y) / denom;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        uint32_t *row = fb->px + (size_t)y * fb->w;
        for (int x = a.x; x < a.x + a.w; x++) {
            float d = dither_at(x, y);
            float ta = (t + d * 0.25f);
            if (ta < 0) ta = 0;
            if (ta > 1) ta = 1;
            ez_rgb c;
            c.r = (uint8_t)(top.r + (bot.r - (int)top.r) * ta);
            c.g = (uint8_t)(top.g + (bot.g - (int)top.g) * ta);
            c.b = (uint8_t)(top.b + (bot.b - (int)top.b) * ta);
            int al = (int)(a_top + (a_bot - a_top) * ta);
            blend_px(&row[x], c, al);
        }
    }
}

void ez_glow(ez_fb *fb, int cx, int cy, int radius, ez_rgb c, int alpha)
{
    if (radius <= 0 || alpha <= 0) return;
    ez_rect r = {cx - radius, cy - radius, radius * 2, radius * 2};
    ez_rect a = clip_rect(fb, r);
    float inv = 1.0f / (float)radius;
    for (int y = a.y; y < a.y + a.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        float dy = (float)(y - cy) * inv;
        for (int x = a.x; x < a.x + a.w; x++) {
            float dx = (float)(x - cx) * inv;
            float d2 = dx * dx + dy * dy;
            if (d2 >= 1.0f) continue;
            float f = 1.0f - sqrtf(d2);
            f = f * f;
            int al = (int)(f * (float)alpha + dither_at(x, y) * 2.0f);
            if (al > 0) blend_px(&row[x], c, al);
        }
    }
}

static inline float sd_round(float px, float py, float hw, float hh, float r)
{
    float qx = fabsf(px) - hw + r;
    float qy = fabsf(py) - hh + r;
    float ax = qx > 0 ? qx : 0;
    float ay = qy > 0 ? qy : 0;
    float outside = sqrtf(ax * ax + ay * ay);
    float inside = qx > qy ? qx : qy;
    if (inside > 0) inside = 0;
    return outside + inside - r;
}

static void paint_round(ez_fb *fb, ez_rect r, int radius, ez_rgb top, int a_top,
                        ez_rgb bot, int a_bot, int gradient)
{
    if (r.w <= 0 || r.h <= 0) return;
    ez_rect a = clip_rect(fb, r);
    if (a.w <= 0 || a.h <= 0) return;
    float hw = r.w * 0.5f;
    float hh = r.h * 0.5f;
    float rad = (float)radius;
    float maxr = hw < hh ? hw : hh;
    if (rad > maxr) rad = maxr;
    if (rad < 0) rad = 0;
    float cx = r.x + hw;
    float cy = r.y + hh;
    float denom = (float)(r.h > 1 ? r.h - 1 : 1);

    for (int y = a.y; y < a.y + a.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        float py = (float)y + 0.5f - cy;
        float t = (float)(y - r.y) / denom;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        ez_rgb c;
        int base_a;
        if (gradient) {
            c.r = (uint8_t)(top.r + (bot.r - (int)top.r) * t);
            c.g = (uint8_t)(top.g + (bot.g - (int)top.g) * t);
            c.b = (uint8_t)(top.b + (bot.b - (int)top.b) * t);
            base_a = (int)(a_top + (a_bot - a_top) * t);
        } else {
            c = top;
            base_a = a_top;
        }
        if (base_a <= 0) continue;
        for (int x = a.x; x < a.x + a.w; x++) {
            float px = (float)x + 0.5f - cx;
            float d = sd_round(px, py, hw, hh, rad);
            float cov = 0.5f - d;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            int al = (int)(cov * (float)base_a + 0.5f);
            blend_px(&row[x], c, al);
        }
    }
}

void ez_round(ez_fb *fb, ez_rect r, int radius, ez_rgb c, int alpha)
{
    paint_round(fb, r, radius, c, alpha, c, alpha, 0);
}

void ez_round_grad(ez_fb *fb, ez_rect r, int radius, ez_rgb top, int a_top,
                   ez_rgb bot, int a_bot)
{
    paint_round(fb, r, radius, top, a_top, bot, a_bot, 1);
}

void ez_round_stroke(ez_fb *fb, ez_rect r, int radius, int width, ez_rgb c, int alpha)
{
    if (width <= 0 || alpha <= 0) return;
    ez_rect a = clip_rect(fb, r);
    if (a.w <= 0 || a.h <= 0) return;
    float hw = r.w * 0.5f, hh = r.h * 0.5f;
    float rad = (float)radius;
    float maxr = hw < hh ? hw : hh;
    if (rad > maxr) rad = maxr;
    float cx = r.x + hw, cy = r.y + hh;
    for (int y = a.y; y < a.y + a.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        float py = (float)y + 0.5f - cy;
        for (int x = a.x; x < a.x + a.w; x++) {
            float px = (float)x + 0.5f - cx;
            float d = fabsf(sd_round(px, py, hw, hh, rad));
            float cov = 0.5f + ((float)width * 0.5f - d);
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            blend_px(&row[x], c, (int)(cov * (float)alpha + 0.5f));
        }
    }
}

static inline void rgb565_to_rgb(uint16_t v, float *r, float *g, float *b)
{
    int ri = (v >> 11) & 31, gi = (v >> 5) & 63, bi = v & 31;
    *r = (float)((ri << 3) | (ri >> 2));
    *g = (float)((gi << 2) | (gi >> 4));
    *b = (float)((bi << 3) | (bi >> 2));
}

static void image_draw(ez_fb *fb, const ez_image *im, ez_rect dst, int radius,
                       int alpha, int cover)
{
    if (!im || im->w <= 0 || im->h <= 0 || dst.w <= 0 || dst.h <= 0) return;
    ez_rect a = clip_rect(fb, dst);
    if (a.w <= 0 || a.h <= 0) return;

    float sx0 = 0.0f, sy0 = 0.0f, sxspan = (float)im->w, syspan = (float)im->h;
    if (cover) {
        float s = (float)dst.w / (float)im->w;
        float s2 = (float)dst.h / (float)im->h;
        if (s2 > s) s = s2;
        float used_w = (float)dst.w / s;
        float used_h = (float)dst.h / s;
        sx0 = ((float)im->w - used_w) * 0.5f;
        sy0 = ((float)im->h - used_h) * 0.5f;
        sxspan = used_w;
        syspan = used_h;
    }

    float hw = dst.w * 0.5f, hh = dst.h * 0.5f;
    float rad = (float)radius;
    float maxr = hw < hh ? hw : hh;
    if (rad > maxr) rad = maxr;
    float cx = dst.x + hw, cy = dst.y + hh;
    int masked = radius >= 0;

    for (int y = a.y; y < a.y + a.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        float py = (float)y + 0.5f - cy;
        float v = ((float)(y - dst.y) + 0.5f) / (float)dst.h;
        float fy = sy0 + v * syspan - 0.5f;
        int iy = (int)floorf(fy);
        float ty = fy - (float)iy;
        for (int x = a.x; x < a.x + a.w; x++) {
            int cov255 = 255;
            if (masked) {
                float px = (float)x + 0.5f - cx;
                float d = sd_round(px, py, hw, hh, rad);
                float cov = 0.5f - d;
                if (cov <= 0.0f) continue;
                if (cov > 1.0f) cov = 1.0f;
                cov255 = (int)(cov * 255.0f);
            }
            float u = ((float)(x - dst.x) + 0.5f) / (float)dst.w;
            float fx = sx0 + u * sxspan - 0.5f;
            int ix = (int)floorf(fx);
            float tx = fx - (float)ix;

            int x0 = ix, x1 = ix + 1, y0 = iy, y1 = iy + 1;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 >= im->w) x1 = im->w - 1;
            if (y1 >= im->h) y1 = im->h - 1;
            if (x0 >= im->w) x0 = im->w - 1;
            if (y0 >= im->h) y0 = im->h - 1;

            uint16_t p00 = im->px[y0 * im->w + x0];
            uint16_t p10 = im->px[y0 * im->w + x1];
            uint16_t p01 = im->px[y1 * im->w + x0];
            uint16_t p11 = im->px[y1 * im->w + x1];

            float r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;
            rgb565_to_rgb(p00, &r00, &g00, &b00);
            rgb565_to_rgb(p10, &r10, &g10, &b10);
            rgb565_to_rgb(p01, &r01, &g01, &b01);
            rgb565_to_rgb(p11, &r11, &g11, &b11);

            float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty);
            float w01 = (1 - tx) * ty, w11 = tx * ty;
            ez_rgb c;
            c.r = (uint8_t)(r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11);
            c.g = (uint8_t)(g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11);
            c.b = (uint8_t)(b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11);

            int al = cov255 * alpha / 255;
            blend_px(&row[x], c, al);
        }
    }
}

void ez_image_round(ez_fb *fb, const ez_image *im, ez_rect dst, int radius, int alpha)
{
    image_draw(fb, im, dst, radius, alpha, 1);
}

void ez_image_cover(ez_fb *fb, const ez_image *im, ez_rect dst, int alpha)
{
    image_draw(fb, im, dst, -1, alpha, 1);
}

void ez_blit_a8(ez_fb *fb, const uint8_t *alpha, int aw, int ah,
                ez_rect src, ez_rect dst, ez_rgb c, int a)
{
    if (!alpha || src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return;
    ez_rect clip = clip_rect(fb, dst);
    for (int y = clip.y; y < clip.y + clip.h; y++) {
        uint32_t *row = fb->px + (size_t)y * fb->w;
        int sy = src.y + (y - dst.y) * src.h / dst.h;
        if (sy < 0 || sy >= ah) continue;
        for (int x = clip.x; x < clip.x + clip.w; x++) {
            int sx = src.x + (x - dst.x) * src.w / dst.w;
            if (sx < 0 || sx >= aw) continue;
            int cov = alpha[sy * aw + sx];
            if (!cov) continue;
            blend_px(&row[x], c, cov * a / 255);
        }
    }
}
