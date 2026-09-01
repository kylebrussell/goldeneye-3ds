#include "ge_3ds_save_provider.h"
#include "ge_original_mission_result.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ultra64.h>
#include <bondconstants.h>

s32 selected_folder_num = FOLDER1;

int main(void)
{
    char path[] = "/tmp/ge-3ds-save.XXXXXX";
    char copy_path[] = "/tmp/ge-3ds-save-copy.XXXXXX";
    Ge3dsSaveProvider provider;
    Ge3dsSaveProvider copy;
    Ge3dsSaveProvider reloaded;
    GeOriginalMissionResultProviders result_providers;
    GeOriginalMissionResultSnapshot result;
    unsigned char byte;
    FILE *stream;
    int descriptor = mkstemp(path);
    int copy_descriptor = mkstemp(copy_path);

    assert(descriptor >= 0);
    close(descriptor);
    assert(unlink(path) == 0);
    assert(copy_descriptor >= 0);
    close(copy_descriptor);
    assert(unlink(copy_path) == 0);

    ge_original_mission_result_reset();
    assert(ge_3ds_save_provider_init(&provider, path, FOLDER1)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(provider.ready == 1U);
    assert(provider.writes == 1U);
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_DAM, DIFFICULTY_00)
        == STAGESTATUS_UNLOCKED);
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_FACILITY, DIFFICULTY_AGENT)
        == STAGESTATUS_LOCKED);
    /* fileClearSavefileForFolder preserves mission progress and replaces only
     * the three settings fields.  Identical settings must not rewrite the
     * durable slot. */
    assert(ge_3ds_save_provider_persist_settings(
        &provider, 0x55U, 0x66U, 0x034aU)==GE_3DS_SAVE_PROVIDER_OK);
    assert(provider.settings_updates==1U&&provider.writes==2U);
    assert(ge_3ds_save_provider_persist_settings(
        &provider, 0x55U, 0x66U, 0x034aU)==GE_3DS_SAVE_PROVIDER_OK);
    assert(provider.settings_updates==1U&&provider.writes==2U);
    ge_3ds_save_provider_make_mission_result_providers(
        &provider, &result_providers);
    ge_original_mission_result_bind(&result_providers);
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_SECRET, 60 * 100));
    ge_original_mission_result_snapshot(&result);
    assert(result.completion_mutations == 1U);
    assert(result.cheat_mutations == 1U);
    assert(result.persistence_frontiers == 0U);
    assert(provider.completion_updates == 1U);
    assert(provider.cheat_updates == 1U);
    assert(provider.writes == 4U);
    assert(ge_3ds_save_provider_stage_time(
        &provider, SP_LEVEL_DAM, DIFFICULTY_SECRET) == 100);
    assert(ge_3ds_save_provider_stage_time(
        &provider, SP_LEVEL_DAM, DIFFICULTY_AGENT) == 0x3ff);
    assert(ge_3ds_save_provider_cheat_unlocked(
        &provider, SP_LEVEL_DAM));
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_FACILITY, DIFFICULTY_AGENT)
        == STAGESTATUS_UNLOCKED);
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_FACILITY, DIFFICULTY_SECRET)
        == STAGESTATUS_UNLOCKED);
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_FACILITY, DIFFICULTY_00)
        == STAGESTATUS_LOCKED);
    assert(ge_original_mission_result_set_current_mission(
        SP_LEVEL_FACILITY));
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_00, 60 * 120));
    ge_original_mission_result_snapshot(&result);
    assert(result.mission == SP_LEVEL_FACILITY);
    assert(result.cheat_target_seconds == 125);
    assert(provider.completion_updates == 2U);
    assert(provider.cheat_updates == 2U);
    assert(provider.writes == 6U);
    assert(ge_3ds_save_provider_stage_time(
        &provider, SP_LEVEL_FACILITY, DIFFICULTY_00) == 120);
    assert(ge_3ds_save_provider_cheat_unlocked(
        &provider, SP_LEVEL_FACILITY));
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_RUNWAY, DIFFICULTY_00)
        == STAGESTATUS_UNLOCKED);
    assert(ge_3ds_save_provider_stage_status(
        &provider, SP_LEVEL_AZTEC, DIFFICULTY_AGENT)
        == STAGESTATUS_LOCKED);
    assert(!ge_3ds_save_provider_007_unlocked(&provider));
    {
        int32_t mission = -99;
        int32_t difficulty = -99;
        assert(ge_3ds_save_provider_highest_completed(
            &provider, &mission, &difficulty));
        /* Exact search is difficulty-major, so 00 Agent Facility wins even
         * though Secret Agent Dam is also complete. */
        assert(mission == SP_LEVEL_FACILITY);
        assert(difficulty == DIFFICULTY_00);
    }
    assert(ge_3ds_save_provider_init(&copy, copy_path, FOLDER2)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(!ge_3ds_save_provider_highest_completed(&copy, NULL, NULL));
    assert(ge_3ds_save_provider_copy_if_empty(&provider, &copy)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(copy.writes == 2U);
    assert(ge_3ds_save_provider_stage_time(
        &copy, SP_LEVEL_FACILITY, DIFFICULTY_00) == 120);
    assert(ge_3ds_save_provider_copy_if_empty(&provider, &copy)
        == GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT);

    /* The live wallet selection must publish front.c's canonical folder
     * before end_of_mission_briefing persists a result. Previously the 3DS
     * callback changed only the provider, leaving this global at FOLDER1;
     * every completion from folders 2-4 then hit a persistence frontier. */
    assert(selected_folder_num == FOLDER1);
    assert(ge_3ds_save_provider_select(&provider, copy_path, FOLDER2)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(provider.ready == 1U && provider.folder == FOLDER2);
    assert(selected_folder_num == FOLDER2);
    ge_3ds_save_provider_make_mission_result_providers(
        &provider, &result_providers);
    ge_original_mission_result_bind(&result_providers);
    assert(ge_original_mission_result_set_current_mission(SP_LEVEL_RUNWAY));
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_AGENT, 60 * 130));
    ge_original_mission_result_snapshot(&result);
    assert(result.folder == FOLDER2);
    assert(result.mission == SP_LEVEL_RUNWAY);
    assert(result.persistence_frontiers == 0U);
    assert(ge_3ds_save_provider_stage_time(
        &provider, SP_LEVEL_RUNWAY, DIFFICULTY_AGENT) == 130);

    /* Re-selecting an already mounted slot still repairs the canonical
     * frontend global without rewriting or reopening the save. */
    selected_folder_num = FOLDER1;
    assert(ge_3ds_save_provider_select(&provider, copy_path, FOLDER2)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(selected_folder_num == FOLDER2);
    {
        char invalid_path[GE_3DS_SAVE_PATH_CAPACITY + 8U];
        memset(invalid_path, 'x', sizeof(invalid_path));
        invalid_path[sizeof(invalid_path) - 1U] = '\0';
        assert(ge_3ds_save_provider_select(
            &provider, invalid_path, FOLDER3)
            == GE_3DS_SAVE_PROVIDER_PATH_TOO_LONG);
        assert(provider.ready == 1U && provider.folder == FOLDER2);
        assert(selected_folder_num == FOLDER2);
    }

    /* Continue the copy/erase coverage on the independently mounted slot. */
    assert(ge_3ds_save_provider_erase(&copy)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(copy.writes == 3U);
    assert(!ge_3ds_save_provider_highest_completed(&copy, NULL, NULL));
    stream = fopen(copy_path, "rb");
    assert(stream != NULL);
    assert(fseek(stream, 8, SEEK_SET) == 0);
    assert(fread(&byte, 1U, 1U, stream) == 1U);
    assert(fclose(stream) == 0);
    assert((byte & SAVEFLAG_FOLDER) == FOLDER2);
    assert((byte & SAVEFLAG_BOND)
        == (((unsigned)FOLDER2 << 5) & SAVEFLAG_BOND));
    /* Canonical delete is a no-op for an empty folder. */
    assert(ge_3ds_save_provider_erase(&copy)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(copy.writes == 3U);
    ge_3ds_save_provider_close(&copy);
    ge_3ds_save_provider_close(&provider);

    assert(ge_3ds_save_provider_init(&reloaded, path, FOLDER1)
        == GE_3DS_SAVE_PROVIDER_OK);
    assert(reloaded.loads == 1U);
    assert(ge_3ds_save_provider_stage_time(
        &reloaded, SP_LEVEL_DAM, DIFFICULTY_SECRET) == 100);
    assert(ge_3ds_save_provider_cheat_unlocked(
        &reloaded, SP_LEVEL_DAM));
    assert(ge_3ds_save_provider_stage_time(
        &reloaded, SP_LEVEL_FACILITY, DIFFICULTY_00) == 120);
    assert(ge_3ds_save_provider_cheat_unlocked(
        &reloaded, SP_LEVEL_FACILITY));
    assert(((const unsigned char *)reloaded.slot_words)[10]==0x55U
        &&((const unsigned char *)reloaded.slot_words)[11]==0x66U);
    /* save_data is native-endian in memory; the provider's disk encoder owns
     * the big-endian byte order independently. */
    {
        uint16_t options;
        memcpy(&options,
            (const unsigned char *)reloaded.slot_words+12U,
            sizeof(options));
        assert(options==0x034aU);
    }
    stream = fopen(path, "rb");
    assert(stream != NULL);
    assert(fseek(stream, 0, SEEK_END) == 0);
    assert(ftell(stream) == GE_3DS_SAVE_SLOT_BYTES);
    assert(fclose(stream) == 0);
    ge_3ds_save_provider_close(&reloaded);

    stream = fopen(path, "r+b");
    assert(stream != NULL);
    assert(fseek(stream, 20, SEEK_SET) == 0);
    assert(fread(&byte, 1U, 1U, stream) == 1U);
    byte ^= 0x80U;
    assert(fseek(stream, 20, SEEK_SET) == 0);
    assert(fwrite(&byte, 1U, 1U, stream) == 1U);
    assert(fclose(stream) == 0);
    assert(ge_3ds_save_provider_init(&reloaded, path, FOLDER1)
        == GE_3DS_SAVE_PROVIDER_CORRUPT);

    assert(unlink(path) == 0);
    assert(unlink(copy_path) == 0);
    puts("3DS save provider: original 0x60-byte CRC slot persists cross-mission completion and cheats");
    return 0;
}
