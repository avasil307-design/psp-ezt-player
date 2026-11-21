#ifndef EZT_MP4_H
#define EZT_MP4_H

#include <stddef.h>
#include <stdint.h>

/* Small, seek-based ISO BMFF reader for ordinary (non-fragmented) MP4 files.
 * The platform supplies read_at/size callbacks; no PSP headers are needed here. */
typedef struct {
    void *user;
    uint64_t size;
    int (*read_at)(void *user, uint64_t offset, void *dst, size_t bytes);
} ez_mp4_io;

typedef struct {
    uint64_t offset;
    uint32_t size;
    uint32_t duration; /* media timescale units */
    uint8_t keyframe;
} ez_mp4_sample;

typedef struct {
    uint32_t timescale;
    uint32_t duration;
    uint32_t width, height;
    uint32_t sample_count;
    uint8_t avc_profile, avc_level, nal_length_size;
    uint8_t sps[64], pps[64];
    uint8_t sps_size, pps_size;
    ez_mp4_sample *samples;
} ez_mp4_video;

typedef enum {
    EZ_MP4_OK = 0,
    EZ_MP4_ERR_IO = -1,
    EZ_MP4_ERR_FORMAT = -2,
    EZ_MP4_ERR_UNSUPPORTED = -3,
    EZ_MP4_ERR_MEMORY = -4,
    EZ_MP4_ERR_RANGE = -5
} ez_mp4_result;

/* Parses a non-fragmented MP4 with a single AVC video track; chooses the first
 * avc1/avc3 track. AAC audio is intentionally ignored at this layer. */
ez_mp4_result ez_mp4_open(const ez_mp4_io *io, ez_mp4_video *out);
void ez_mp4_close(ez_mp4_video *video);
/* Reads a sample and converts length-prefixed AVC NAL units to Annex B. Returns
 * required output bytes, or a negative ez_mp4_result. */
int ez_mp4_read_annexb(const ez_mp4_io *io, const ez_mp4_video *video,
                       uint32_t sample_index, uint8_t *dst, size_t capacity);

#endif
