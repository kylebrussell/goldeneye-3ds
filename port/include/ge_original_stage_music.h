#ifndef GE_ORIGINAL_STAGE_MUSIC_H
#define GE_ORIGINAL_STAGE_MUSIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_ORIGINAL_MUSIC_TRACK_COUNT 63U

typedef struct GeOriginalStageMusic {
    int32_t level_id;
    int16_t main_track;
    int16_t background_track;
    int16_t x_track;
} GeOriginalStageMusic;

/* Exact music_setup_entries lookup. Unlike getmusictrack_or_randomtrack, this
 * reports an unsupported level instead of selecting the original random
 * multiplayer fallback. */
int ge_original_stage_music_resolve(
    int32_t level_id, GeOriginalStageMusic *music);
size_t ge_original_stage_music_mapping_count(void);
const GeOriginalStageMusic *ge_original_stage_music_mapping_at(size_t index);

/* Exact MUSIC_TRACKS ordinal -> US table_music_data CSeq asset. */
const char *ge_original_music_track_asset_path(int32_t track);
const char *ge_original_music_track_symbol(int32_t track);

#ifdef __cplusplus
}
#endif

#endif
