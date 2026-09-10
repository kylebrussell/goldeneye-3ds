/* Offline acceptance tool. Compile via inspect_campaign_save.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The normal provider creates an empty slot when fopen fails. Reuse its
 * exact loader and CRC checks, but make every attempted write impossible in
 * this translation unit, including the missing/unreadable-file fallback. */
static FILE *ge_inspector_fopen(const char *path, const char *mode)
{
    if (strcmp(mode, "rb") != 0) {
        return NULL;
    }
    return fopen(path, mode);
}
#define fopen ge_inspector_fopen
#include "port/src/ge_3ds_save_provider.c"
#undef fopen

s32 selected_folder_num = FOLDER1;

int main(int argc, char **argv)
{
    Ge3dsSaveProvider save;
    int folder = FOLDER1;
    if (argc < 2 || argc > 3 || (argc == 3
            && (strlen(argv[2]) != 1U || argv[2][0] < '0'
                || argv[2][0] > '3'))) {
        fprintf(stderr, "Usage: inspect_campaign_save.sh SAVE [FOLDER 0..3]\n");
        return 2;
    }
    if (argc == 3) folder = argv[2][0] - '0';
    Ge3dsSaveProviderStatus status = ge_3ds_save_provider_init(
        &save, argv[1], folder);
    if (status != GE_3DS_SAVE_PROVIDER_OK || save.loads != 1U
            || save.writes != 0U) {
        fprintf(stderr, "Cannot reload existing save: %s (loads=%u writes=%u)\n",
            ge_3ds_save_provider_status_name(status), save.loads, save.writes);
        return 2;
    }
    const int time = ge_3ds_save_provider_stage_time(
        &save, SP_LEVEL_DAM, DIFFICULTY_00);
    const int dam = ge_3ds_save_provider_stage_status(
        &save, SP_LEVEL_DAM, DIFFICULTY_00);
    const int facility = ge_3ds_save_provider_stage_status(
        &save, SP_LEVEL_FACILITY, DIFFICULTY_00);
    const int complete = dam == STAGESTATUS_COMPLETED && time > 0;
    const int unlocked = facility == STAGESTATUS_UNLOCKED
        || facility == STAGESTATUS_COMPLETED;
    printf("{\"format\":\"GE_CAMPAIGN_SAVE_INSPECTION_1\","
        "\"folder\":%d,\"crc_valid\":true,\"loads\":%u,\"writes\":%u,"
        "\"dam_00_status\":%d,\"dam_00_time_seconds\":%d,"
        "\"dam_00_time_capped\":%s,\"dam_00_completed\":%s,"
        "\"facility_00_status\":%d,\"facility_00_unlocked\":%s,"
        "\"acceptance_passed\":%s}\n",
        folder, save.loads, save.writes, dam, time,
        time == 0x3ff ? "true" : "false", complete ? "true" : "false",
        facility, unlocked ? "true" : "false",
        complete && unlocked ? "true" : "false");
    ge_3ds_save_provider_close(&save);
    return complete && unlocked ? 0 : 1;
}
