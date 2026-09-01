#include "ge_asset_pack.h"
#include "ge_original_stage_music.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const int32_t supported_solo_levels[] = {
    33, /* LEVELID_DAM */
    34, /* LEVELID_FACILITY */
#define GE_SOLO_STAGE(symbol, level, ...) level,
#include "ge_solo_stage_registry.inc"
#undef GE_SOLO_STAGE
};

static void assert_stage(
    int32_t level, int16_t main_track, const char *main_path,
    int16_t background_track, int16_t x_track, const char *x_path)
{
    GeOriginalStageMusic music;
    assert(ge_original_stage_music_resolve(level, &music));
    assert(music.level_id == level);
    assert(music.main_track == main_track);
    assert(music.background_track == background_track);
    assert(music.x_track == x_track);
    assert(strcmp(ge_original_music_track_asset_path(main_track), main_path) == 0);
    if (x_track >= 0)
        assert(strcmp(ge_original_music_track_asset_path(x_track), x_path) == 0);
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalStageMusic music;
    size_t index;

    assert(argc == 2);
    assert(ge_original_stage_music_mapping_count() == 23U);
    assert(!ge_original_stage_music_resolve(-1, &music));
    assert(!ge_original_stage_music_resolve(33, NULL));
    assert(ge_original_stage_music_mapping_at(23U) == NULL);
    assert(ge_original_music_track_asset_path(-1) == NULL);
    assert(ge_original_music_track_asset_path(63) == NULL);
    assert(ge_original_music_track_symbol(-1) == NULL);
    assert(ge_original_music_track_symbol(63) == NULL);

    /* Edge names where the enum, level name and file stem intentionally
     * differ. These are the common failure cases for guessed mappings. */
    assert_stage(33, 9, "music/Mdam.bin", -1, 53,
                 "music/Mwindblowing.bin");
    assert_stage(34, 7, "music/Mfacility.bin", -1, 31,
                 "music/Mfacilityx.bin");
    assert_stage(35, 50, "music/Mrunway.bin", -1, 51,
                 "music/Mrunway_plane.bin");
    assert_stage(39, 26, "music/Mwatercaverns.bin", -1, 21,
                 "music/Melevator_wc.bin");
    assert_stage(43, 28, "music/Msurface2.bin", 53, 60,
                 "music/Msurface2_ending.bin");

    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);

    /* The exact US table_music_data ordering provides a packaged path for
     * every MUSIC_TRACKS ordinal, including deliberately named null tracks. */
    for (index = 0U; index < GE_ORIGINAL_MUSIC_TRACK_COUNT; ++index) {
        const char *path = ge_original_music_track_asset_path((int32_t)index);
        const char *symbol = ge_original_music_track_symbol((int32_t)index);
        const GeAssetPackEntry *entry;
        assert(path != NULL && symbol != NULL);
        entry = ge_asset_pack_find(&pack, path);
        assert(entry != NULL && entry->data_size != 0U);
    }

    /* Every stage currently supported by the native solo-stage registry has
     * an exact music_setup_entries row and a nonempty main CSeq in the pack. */
    for (index = 0U;
         index < sizeof(supported_solo_levels) / sizeof(supported_solo_levels[0]);
         ++index) {
        const GeAssetPackEntry *entry;
        assert(ge_original_stage_music_resolve(
            supported_solo_levels[index], &music));
        assert(music.main_track >= 0);
        entry = ge_asset_pack_find(
            &pack, ge_original_music_track_asset_path(music.main_track));
        assert(entry != NULL && entry->data_size >= 68U);
    }
    ge_asset_pack_close(&pack);

    printf("original stage music mapping pass: %zu canonical rows, "
           "%zu solo stages, %u exact CSeq assets\n",
           ge_original_stage_music_mapping_count(),
           sizeof(supported_solo_levels) / sizeof(supported_solo_levels[0]),
           GE_ORIGINAL_MUSIC_TRACK_COUNT);
    return 0;
}
