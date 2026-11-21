#include "ezt.h"
#include "atlas.h"

extern const uint8_t ez_atlas_blob[];
extern const uint8_t ez_atlas_blob_end[];
extern const uint8_t ez_art_blob[];
extern const uint8_t ez_art_blob_end[];

void ez_assets_init(void)
{
    ez_text_init(ez_atlas_blob);
}

const uint8_t *ez_atlas_data(size_t *len)
{
    if (len) *len = (size_t)(ez_atlas_blob_end - ez_atlas_blob);
    return ez_atlas_blob;
}

const uint8_t *ez_art_data(size_t *len)
{
    if (len) *len = (size_t)(ez_art_blob_end - ez_art_blob);
    return ez_art_blob;
}
