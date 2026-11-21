/* Host renderer: emits PNG screenshots for design review without PSP SDK. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ezt.h"

static uint32_t crc32_bytes(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int)(c & 1));
    }
    return c ^ 0xFFFFFFFFu;
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static int write_chunk(FILE *f, const char tag[4], const uint8_t *data, uint32_t len)
{
    uint8_t n[4], crcbuf[4];
    put_be32(n, len);
    if (fwrite(n, 1, 4, f) != 4 || fwrite(tag, 1, 4, f) != 4) return 0;
    if (len && fwrite(data, 1, len, f) != len) return 0;
    uint8_t *tmp = (uint8_t *)malloc((size_t)len + 4);
    if (!tmp) return 0;
    memcpy(tmp, tag, 4);
    if (len) memcpy(tmp + 4, data, len);
    put_be32(crcbuf, crc32_bytes(tmp, (size_t)len + 4));
    free(tmp);
    return fwrite(crcbuf, 1, 4, f) == 4;
}

static int write_png(const char *path, const ez_fb *fb)
{
    size_t row = (size_t)fb->w * 3 + 1;
    size_t raw_n = row * fb->h;
    uint8_t *raw = (uint8_t *)malloc(raw_n);
    if (!raw) return 0;
    for (int y = 0; y < fb->h; y++) {
        uint8_t *d = raw + (size_t)y * row;
        *d++ = 0;
        for (int x = 0; x < fb->w; x++) {
            uint32_t p = fb->px[(size_t)y * fb->w + x];
            *d++ = (uint8_t)(p >> 16); *d++ = (uint8_t)(p >> 8); *d++ = (uint8_t)p;
        }
    }
    /* zlib stream of uncompressed DEFLATE blocks. */
    size_t blocks = (raw_n + 65534) / 65535;
    size_t z_n = 2 + raw_n + blocks * 5 + 4;
    uint8_t *z = (uint8_t *)malloc(z_n);
    if (!z) { free(raw); return 0; }
    size_t q = 0, off = 0;
    z[q++] = 0x78; z[q++] = 0x01;
    while (off < raw_n) {
        uint16_t n = (uint16_t)((raw_n - off) > 65535 ? 65535 : raw_n - off);
        z[q++] = (off + n == raw_n) ? 1 : 0;
        z[q++] = (uint8_t)n; z[q++] = (uint8_t)(n >> 8);
        uint16_t inv = (uint16_t)~n;
        z[q++] = (uint8_t)inv; z[q++] = (uint8_t)(inv >> 8);
        memcpy(z + q, raw + off, n); q += n; off += n;
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    put_be32(z + q, (b << 16) | a); q += 4;

    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(z); return 0; }
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    uint8_t ihdr[13];
    put_be32(ihdr, (uint32_t)fb->w); put_be32(ihdr + 4, (uint32_t)fb->h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    int ok = fwrite(sig, 1, 8, f) == 8 && write_chunk(f, "IHDR", ihdr, 13) &&
             write_chunk(f, "IDAT", z, (uint32_t)q) && write_chunk(f, "IEND", 0, 0);
    fclose(f); free(raw); free(z);
    return ok;
}

static int render(const char *file, int screen, int theme, int light)
{
    uint32_t *pixels = (uint32_t *)malloc(EZ_SCREEN_W * EZ_SCREEN_H * sizeof(uint32_t));
    if (!pixels) return 0;
    ez_fb fb;
    ez_fb_init(&fb, EZ_SCREEN_W, EZ_SCREEN_H, pixels);
    ez_app app;
    ez_app_init(&app);
    app.theme = theme;
    app.light = light;
    app.scheme = ez_theme_scheme(theme, ez_art(0), light);
    app.scheme_prev = app.scheme;
    app.screen = screen;
    if (screen == EZ_SCREEN_PLAYER) app.playing = 0;
    if (screen == EZ_SCREEN_SETTINGS) app.focus = 0;
    ez_app_render(&app, &fb);
    int ok = write_png(file, &fb);
    free(pixels);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: preview [output.png] [theme-index 0..6] [light 0|1]\n"
             "Without arguments, writes library/settings/player PNG previews.");
        return 0;
    }
    if (argc > 1) {
        const char *out = argv[1];
        int theme = argc > 2 ? atoi(argv[2]) : 0;
        int light = argc > 3 ? atoi(argv[3]) : 0;
        if (!render(out, EZ_SCREEN_LIBRARY, theme, light)) return 1;
        printf("Wrote %s\n", out);
        return 0;
    }
    if (!render("build/preview-library.png", EZ_SCREEN_LIBRARY, 0, 0) ||
        !render("build/preview-settings.png", EZ_SCREEN_SETTINGS, 0, 0) ||
        !render("build/preview-player.png", EZ_SCREEN_PLAYER, 0, 0)) return 1;
    puts("Wrote build/preview-{library,settings,player}.png");
    return 0;
}
