#include "ge_original_stage_music.h"

typedef struct GeOriginalMusicTrackAsset {
    const char *symbol;
    const char *path;
} GeOriginalMusicTrackAsset;

static const GeOriginalMusicTrackAsset
ge_original_music_track_assets[GE_ORIGINAL_MUSIC_TRACK_COUNT] = {
#define GE_ORIGINAL_MUSIC_TRACK(index, symbol, path) [index] = {symbol, path},
#define GE_ORIGINAL_STAGE_MUSIC(...)
#include "ge_original_stage_music_data.inc"
#undef GE_ORIGINAL_STAGE_MUSIC
#undef GE_ORIGINAL_MUSIC_TRACK
};

static const GeOriginalStageMusic ge_original_stage_music_mappings[] = {
#define GE_ORIGINAL_MUSIC_TRACK(...)
#define GE_ORIGINAL_STAGE_MUSIC(level, symbol, main, background, xtrack) \
    {(int32_t)(level), (int16_t)(main), (int16_t)(background), (int16_t)(xtrack)},
#include "ge_original_stage_music_data.inc"
#undef GE_ORIGINAL_STAGE_MUSIC
#undef GE_ORIGINAL_MUSIC_TRACK
};

int ge_original_stage_music_resolve(
    int32_t level_id, GeOriginalStageMusic *music)
{
    size_t index;
    if (music == NULL) return 0;
    for (index = 0U;
         index < sizeof(ge_original_stage_music_mappings)
                     / sizeof(ge_original_stage_music_mappings[0]);
         ++index) {
        if (ge_original_stage_music_mappings[index].level_id == level_id) {
            *music = ge_original_stage_music_mappings[index];
            return 1;
        }
    }
    return 0;
}

size_t ge_original_stage_music_mapping_count(void)
{
    return sizeof(ge_original_stage_music_mappings)
        / sizeof(ge_original_stage_music_mappings[0]);
}

const GeOriginalStageMusic *ge_original_stage_music_mapping_at(size_t index)
{
    return index < ge_original_stage_music_mapping_count()
        ? &ge_original_stage_music_mappings[index] : NULL;
}

const char *ge_original_music_track_asset_path(int32_t track)
{
    return track >= 0 && (uint32_t)track < GE_ORIGINAL_MUSIC_TRACK_COUNT
        ? ge_original_music_track_assets[(size_t)track].path : NULL;
}

const char *ge_original_music_track_symbol(int32_t track)
{
    return track >= 0 && (uint32_t)track < GE_ORIGINAL_MUSIC_TRACK_COUNT
        ? ge_original_music_track_assets[(size_t)track].symbol : NULL;
}
