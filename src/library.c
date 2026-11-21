/* Demo library: video entries + decoded art frames from assets/art.bin. */
#include <string.h>

#include "ezt.h"
#include "art.h"

static const ez_video VIDEOS[] = {
    {"Закат над морем", "24:16 · 1080p", 0, "24:16", 0.38f, 1456},
    {"Ночной город", "48:02 · 720p", 1, "48:02", 0.12f, 2882},
    {"Горы в тумане", "12:40 · 4K", 2, "12:40", 0.72f, 760},
    {"Неоновые волны", "03:18 · 1080p", 3, "03:18", 0.0f, 198},
    {"Поле и звёзды", "18:55 · 1080p", 4, "18:55", 0.55f, 1135},
    {"Абстракция", "06:04 · 720p", 5, "06:04", 0.0f, 364},
};

static ez_image ARTS[EZ_ART_COUNT];
static uint16_t ARTS_RAW[EZ_ART_COUNT][EZ_ART_W * EZ_ART_H];
static int inited;

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

void ez_library_init(void)
{
    if (inited) return;
    size_t len = 0;
    const uint8_t *blob = ez_art_data(&len);
    inited = 1;
    if (!blob || len < 10 || memcmp(blob, "EZAR", 4) != 0) return;
    int count = rd16(blob + 4);
    int w = rd16(blob + 6);
    int h = rd16(blob + 8);
    if (count > EZ_ART_COUNT) count = EZ_ART_COUNT;
    if (w != EZ_ART_W || h != EZ_ART_H) return;
    const uint8_t *p = blob + 10;
    for (int i = 0; i < count; i++) {
        size_t bytes = (size_t)w * h * 2;
        if ((size_t)(p - blob) + bytes > len) break;
        memcpy(ARTS_RAW[i], p, bytes);
        p += bytes;
        ARTS[i].w = w;
        ARTS[i].h = h;
        ARTS[i].px = ARTS_RAW[i];
    }
}

int ez_video_count(void)
{
    return (int)(sizeof(VIDEOS) / sizeof(VIDEOS[0]));
}

const ez_video *ez_video_get(int i)
{
    int n = ez_video_count();
    if (n == 0) return 0;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return &VIDEOS[i];
}

const ez_image *ez_art(int i)
{
    if (i < 0 || i >= EZ_ART_COUNT) return 0;
    if (!ARTS[i].px) return 0;
    return &ARTS[i];
}

int ez_art_count(void)
{
    return EZ_ART_COUNT;
}
