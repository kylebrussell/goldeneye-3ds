#include "ge_3ds_save_provider.h"

#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <random.h>
#include "game/file.h"

extern s32 selected_folder_num;

/* Canonical file2.h EEPROM option defaults, kept local so the durable save
 * adapter does not pull the unrelated character/model ABI into this unit. */
#define OPTION_AUTOAIM       0x0002
#define OPTION_SIGHTONSCREEN 0x0008
#define OPTION_LOOKAHEAD     0x0010
#define OPTION_DISPLAYAMMO   0x0020
#define DEFAULT_OPTIONS \
    (OPTION_AUTOAIM | OPTION_SIGHTONSCREEN | OPTION_LOOKAHEAD \
        | OPTION_DISPLAYAMMO)
#define CHEAT_INPUT_BUFFER_SIZE 20
#ifndef bool
#define bool _Bool
#endif

_Static_assert(sizeof(save_data) == GE_3DS_SAVE_SLOT_BYTES,
    "3DS save payload must retain the original 0x60-byte slot layout");

static Ge3dsSaveProvider *ge_save_active;

static save_data *ge_save_slot(Ge3dsSaveProvider *state)
{
    return state == NULL ? NULL : (save_data *)state->slot_words;
}

static const save_data *ge_save_slot_const(const Ge3dsSaveProvider *state)
{
    return state == NULL ? NULL : (const save_data *)state->slot_words;
}

static u32 ge_save_file_is_007_unlocked(u32 folder)
{
    (void)folder;
    return FALSE;
}

#define fileGetSaveFolder ge_save_get_folder_exact
#define fileSetSaveFoldernum ge_save_set_folder_exact
#define fileGetSaveStageDifficultyTime ge_save_get_stage_time_exact
#define fileSetDifficultyStageTime ge_save_set_stage_time_exact
#define fileCheckSaveStageDifficultyTime ge_save_check_stage_time_exact
#define fileGetIsCheatUnlocked ge_save_is_cheat_unlocked_exact
#define fileSetSaveCheatUnlocked ge_save_set_cheat_unlocked_exact
#define fileIs007ModeUnlocked ge_save_file_is_007_unlocked

/* Exact file2.c bodies used by the durable provider. */
u32 fileGetSaveFolder(save_data *save)
{
  return save->completion_bitflags & SAVEFLAG_FOLDER;
}

void fileSetSaveFoldernum(save_data *save, u32 folder)
{
    save->completion_bitflags &= ~SAVEFLAG_FOLDER;
    save->completion_bitflags |= folder & SAVEFLAG_FOLDER;
}

s32 fileGetSaveStageDifficultyTime(save_data* save, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty)
{
    s32 offset;
    LEVEL_SOLO_SEQUENCE max_level;
    u32 time;
    s32 index;

    max_level = SP_LEVEL_MAX;
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX ) && (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_MAX))
    {
        if (difficulty == DIFFICULTY_007)
        {
            if ( fileIs007ModeUnlocked( fileGetSaveFolder(save)))
            {
                return 0x3FF; //max time
            }
            return 0;
        }

        offset = ((difficulty * max_level) + levelid) * 10; //startbit
        index = (offset >> 3);

        switch(7 - (offset & 7)) //bitmask
        {
            case 7: //no offset agent
                // first 10 bits 8 + 2                    1111 1111                                      1100 0000
                time = ((save->times[index] & 0xFF) << 2) | ((save->times[index + 1] & 0xc0) >> 6);
                break;
            case 5: //offset 2 secret agent
                // next 10 bits 6 + 4                     0011 1111                                      1111 0000
                time =  ((save->times[index] & 0x3f) << 4) | ((save->times[index + 1] & 0xf0) >> 4);
                break;
            case 3: //offset 4 00 agent
                // next 10 bits 4 + 6                     0000 1111                                      1111 1100
                time =  ((save->times[index] & 0xf) << 6) | ((save->times[index + 1] & 0xfc) >> 2);
                break;
            case 1: //offset 6 007
                // next 10 bits 2 + 8                     0000 0011                                      1111 1111
                time = ((save->times[index] & 0x3)  << 8) | ((save->times[index + 1] & 0xFFF));
                break;
            default:
                time = 0; // shouldnt reach
#if DEBUG
                osSyncPrintf("file.c: SHOULDN\'T GET HERE EVER [1]\n");
#endif
        }

        return time;
    }

    return 0;
}

void fileSetDifficultyStageTime(save_data *save, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty, s32 newtime)
{
    s32 offset;
    s32 index;
    LEVEL_SOLO_SEQUENCE max_level;

    max_level = SP_LEVEL_MAX;
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX ) && (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_007))
    {
        if (newtime == 0) {
            newtime = 0x4f;
        } else if (newtime > 0x3ff) {
            newtime = 0x3ff;
        }

        offset = ((difficulty * max_level) + levelid) * 10; //startbit
        index = (offset >> 3);

        switch(7 - (offset & 7)) //bitmask
        {
            case 7: //no offset 4 8 12 etc agent
                save->times[index] &= 0xff00;
                save->times[index + 1] &= 0xff3f;
                save->times[index] |= (newtime >> 2) & 0xff;
                save->times[index + 1] |= (newtime << 6) & 0xc0;
                break;
            case 5: //first offset 5 9 13 etc secret agent
                save->times[index] &= 0xffc0;
                save->times[index + 1] &= 0xff0f;
                save->times[index] |= ((newtime >> 4) & 0x3f);
                save->times[index + 1] |= (newtime << 4) & 0xf0;
                break;
            case 3: //second offset 6 10 14 etc 00 agent
                save->times[index] &= 0xfff0;
                save->times[index + 1] &= 0xff03;
                save->times[index] |= ((newtime >> 6) & 0xf);
                save->times[index + 1] |= (newtime << 2) & 0xfC;
                break;
            case 1: //third offset 7 11 15 etc 007
                save->times[index] &= 0xfffc;
                save->times[index + 1] &= 0xff00;
                save->times[index] |= ((newtime >> 8) & 3);
                save->times[index + 1] |= newtime & 0xfff;
                break;
            default:
#if DEBUG
                osSyncPrintf("file.c: SHOULDN\'T GET HERE EVER [2]\n");
#endif
                break;
        }
    }
}

void fileCheckSaveStageDifficultyTime(save_data *folder, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty, s32 newtime)
{
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX) && (difficulty >= DIFFICULTY_AGENT) && (difficulty <= DIFFICULTY_007))
    {
        s32 time = fileGetSaveStageDifficultyTime(folder, levelid, difficulty);

        if ((time == 0) || (newtime < time))
        {
            fileSetDifficultyStageTime(folder, levelid, difficulty, newtime);
        }
    }
}

bool fileGetIsCheatUnlocked(save_data *save, s32 cheat)
{
    s32 bits;

    if (cheat >= 0 && cheat < CHEAT_INPUT_BUFFER_SIZE)
    {
        bits = save->unlocked_cheats_1 | save->unlocked_cheats_3 << 0x18 | save->unlocked_cheats_3 << 0x10 | save->unlocked_cheats_2 << 8;
        return ((1 << cheat) & bits) != 0;
    }

    return FALSE;
}

void fileSetSaveCheatUnlocked(save_data *save, s32 cheat)
{
    u32 i;
    u32 temp;

    if (cheat >= 0 && cheat < CHEAT_INPUT_BUFFER_SIZE)
    {
        temp = 1 << (cheat);

        for(i = 0; temp > 0xff; i++)
        {
            temp = temp >> 8;
        }

        *(((u8 *)save + i + 0xe)) |= temp & 0xFFu; //save.unlocked_cheats_1[i] |= temp;
    }
}

#undef fileIs007ModeUnlocked
#undef fileSetSaveCheatUnlocked
#undef fileGetIsCheatUnlocked
#undef fileCheckSaveStageDifficultyTime
#undef fileSetDifficultyStageTime
#undef fileGetSaveStageDifficultyTime
#undef fileSetSaveFoldernum
#undef fileGetSaveFolder

/* Exact crc.c body, renamed only to keep the frontend boundary private. */
#define fileGenerateCRC ge_save_generate_crc_exact
void fileGenerateCRC(u8 *addressA, u8 *addressB, save_data *retval)
{
    u8 *byte;
    s32 shift      = 0; // Shift value
    s64 polynormal = 0x8F809F473108B3C1;
    s32 checksum1  = 0; // Final checksum #1
    s32 checksum2  = 0; // Final checksum #2

    for(byte = addressA; byte < addressB; byte++,shift += 7)
    {
        polynormal += *byte << (shift & 0xF) ;
        checksum1 ^= randomGetNextFrom(&polynormal);
    }

    for(byte = addressB - 1; byte >= addressA; byte--,shift += 3)
    {
        polynormal += *byte << (shift & 0xF) ;
        checksum2 ^= randomGetNextFrom(&polynormal);
    }
    retval->chksum1 = checksum1;
    retval->chksum2 = checksum2;
}
#undef fileGenerateCRC

static uint32_t ge_save_read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void ge_save_write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void ge_save_encode(const save_data *save, uint8_t *disk)
{
    save_data crc = {0};
    memset(disk, 0, GE_3DS_SAVE_SLOT_BYTES);
    disk[8] = save->completion_bitflags;
    disk[9] = save->flag_007;
    disk[10] = save->music_vol;
    disk[11] = save->sfx_vol;
    disk[12] = (uint8_t)(save->options >> 8);
    disk[13] = (uint8_t)save->options;
    disk[14] = save->unlocked_cheats_1;
    disk[15] = save->unlocked_cheats_2;
    disk[16] = save->unlocked_cheats_3;
    disk[17] = (uint8_t)save->padding;
    memcpy(disk + 18, save->times, sizeof(save->times));
    ge_save_generate_crc_exact(disk + 8, disk + GE_3DS_SAVE_SLOT_BYTES,
                               &crc);
    ge_save_write_be32(disk, (uint32_t)crc.chksum1);
    ge_save_write_be32(disk + 4, (uint32_t)crc.chksum2);
}

static int ge_save_decode(const uint8_t *disk, save_data *save)
{
    save_data crc = {0};
    ge_save_generate_crc_exact((u8 *)disk + 8,
        (u8 *)disk + GE_3DS_SAVE_SLOT_BYTES, &crc);
    if (ge_save_read_be32(disk) != (uint32_t)crc.chksum1
            || ge_save_read_be32(disk + 4) != (uint32_t)crc.chksum2)
        return 0;
    memset(save, 0, sizeof(*save));
    save->chksum1 = crc.chksum1;
    save->chksum2 = crc.chksum2;
    save->completion_bitflags = disk[8];
    save->flag_007 = disk[9];
    save->music_vol = disk[10];
    save->sfx_vol = disk[11];
    save->options = (u16)(((u16)disk[12] << 8) | disk[13]);
    save->unlocked_cheats_1 = disk[14];
    save->unlocked_cheats_2 = disk[15];
    save->unlocked_cheats_3 = disk[16];
    save->padding = (char)disk[17];
    memcpy(save->times, disk + 18, sizeof(save->times));
    return 1;
}

static Ge3dsSaveProviderStatus ge_save_write(Ge3dsSaveProvider *state)
{
    uint8_t disk[GE_3DS_SAVE_SLOT_BYTES];
    char temporary[GE_3DS_SAVE_PATH_CAPACITY + 5U];
    FILE *stream;
    int written;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", state->path)
            >= (int)sizeof(temporary))
        return GE_3DS_SAVE_PROVIDER_PATH_TOO_LONG;
    ge_save_encode(ge_save_slot_const(state), disk);
    stream = fopen(temporary, "wb");
    if (stream == NULL) return GE_3DS_SAVE_PROVIDER_WRITE_FAILED;
    written = fwrite(disk, 1U, sizeof(disk), stream) == sizeof(disk);
    if (fflush(stream) != 0) written = 0;
    if (fclose(stream) != 0) written = 0;
    if (!written) {
        (void)remove(temporary);
        return GE_3DS_SAVE_PROVIDER_WRITE_FAILED;
    }
    (void)remove(state->path);
    if (rename(temporary, state->path) != 0) {
        (void)remove(temporary);
        return GE_3DS_SAVE_PROVIDER_WRITE_FAILED;
    }
    ++state->writes;
    return GE_3DS_SAVE_PROVIDER_OK;
}

static save_data *ge_save_get_for_folder(u32 folder)
{
    if (ge_save_active == NULL || !ge_save_active->ready
            || folder != (u32)ge_save_active->folder)
        return NULL;
    return ge_save_slot(ge_save_active);
}

static void ge_save_overwrite(save_data *old_save, save_data *new_save)
{
    (void)old_save;
    if (ge_save_active == NULL || new_save == NULL) return;
    *ge_save_slot(ge_save_active) = *new_save;
    ge_save_slot(ge_save_active)->completion_bitflags &= ~SAVEFLAG_DORESET;
    ge_save_active->status = ge_save_write(ge_save_active);
}

#define fileGetSaveForFoldernum ge_save_get_for_folder
#define fileOverwriteSaveSlotWithNewSave ge_save_overwrite
#define fileSetSaveFoldernum ge_save_set_folder_exact
#define fileCheckSaveStageDifficultyTime ge_save_check_stage_time_exact
#define fileGetIsCheatUnlocked ge_save_is_cheat_unlocked_exact
#define fileSetSaveCheatUnlocked ge_save_set_cheat_unlocked_exact

/* Exact file2.c mission-result mutation bodies. */
#define fileUnlockStageInFolderAtDifficulty ge_save_unlock_stage_exact
void fileUnlockStageInFolderAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE stage, DIFFICULTY difficulty, s32 newtime)
{
    if ((foldernum >= 0) && (foldernum < MAX_FOLDER_COUNT) &&
        (stage >= SP_LEVEL_DAM) && (stage < SP_LEVEL_MAX) &&
        (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_MAX))
    {
        save_data new_save = BLANKSAVEDATA;

        save_data *save = fileGetSaveForFoldernum(foldernum);
        s32 i;
        if (save) {
            new_save = *save;
        } else {
            fileSetSaveFoldernum(&new_save, foldernum);
        }

        for (i = difficulty; i >= DIFFICULTY_AGENT; i--)
        {
            if (i == difficulty)
            {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, newtime);
            }
            else
            {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, 99999999);
            }
        }

        fileOverwriteSaveSlotWithNewSave(&save[0], &new_save);
    }
}
#undef fileUnlockStageInFolderAtDifficulty

#define fileSaveFolderUnlockCheat ge_save_unlock_cheat_exact
void fileSaveFolderUnlockCheat(s32 foldernum, s32 cheat)
{
    if ((foldernum >= FOLDER1) && (foldernum < MAX_FOLDER_COUNT) && (cheat >= 0) && (cheat < CHEAT_INPUT_BUFFER_SIZE))
    {
        save_data *save = fileGetSaveForFoldernum(foldernum);

        if (save && fileGetIsCheatUnlocked(save, cheat))
        {
           return;
        }

        {
            save_data new_save = BLANKSAVEDATA;

            if (save)
            {
                new_save = *save;
            }
            else
            {
                fileSetSaveFoldernum(&new_save, foldernum);
            }

            fileSetSaveCheatUnlocked(&new_save, cheat);
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}
#undef fileSaveFolderUnlockCheat

#undef fileSetSaveCheatUnlocked
#undef fileGetIsCheatUnlocked
#undef fileCheckSaveStageDifficultyTime
#undef fileSetSaveFoldernum
#undef fileOverwriteSaveSlotWithNewSave
#undef fileGetSaveForFoldernum

static void ge_save_provider_unlock_stage(
    void *context, int32_t folder, int32_t mission,
    int32_t difficulty, int32_t seconds)
{
    Ge3dsSaveProvider *state = context;
    ge_save_active = state;
    ge_save_unlock_stage_exact(folder, (LEVEL_SOLO_SEQUENCE)mission,
                               (DIFFICULTY)difficulty, seconds);
    ge_save_active = NULL;
    if (state->status == GE_3DS_SAVE_PROVIDER_OK)
        ++state->completion_updates;
}

static void *ge_save_provider_for_folder(void *context, int32_t folder)
{
    Ge3dsSaveProvider *state = context;
    if (state == NULL || !state->ready || folder != state->folder) return NULL;
    return ge_save_slot(state);
}

static int ge_save_provider_is_cheat_unlocked(
    void *context, void *save, int32_t mission)
{
    (void)context;
    if (save == NULL) return 0;
    return ge_save_is_cheat_unlocked_exact(save, mission);
}

static void ge_save_provider_unlock_cheat(
    void *context, int32_t folder, int32_t mission)
{
    Ge3dsSaveProvider *state = context;
    uint32_t writes_before = state->writes;
    ge_save_active = state;
    ge_save_unlock_cheat_exact(folder, mission);
    ge_save_active = NULL;
    if (state->status == GE_3DS_SAVE_PROVIDER_OK
            && state->writes != writes_before)
        ++state->cheat_updates;
}

Ge3dsSaveProviderStatus ge_3ds_save_provider_init(
    Ge3dsSaveProvider *state, const char *path, int32_t folder)
{
    FILE *stream;
    uint8_t disk[GE_3DS_SAVE_SLOT_BYTES];
    int trailing;
    save_data blank = BLANKSAVEDATA;
    size_t length;
    if (state == NULL || path == NULL || folder < FOLDER1
            || folder >= MAX_FOLDER_COUNT)
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;
    memset(state, 0, sizeof(*state));
    length = strlen(path);
    if (length >= sizeof(state->path)) {
        state->status = GE_3DS_SAVE_PROVIDER_PATH_TOO_LONG;
        return state->status;
    }
    memcpy(state->path, path, length + 1U);
    state->folder = folder;
    stream = fopen(path, "rb");
    if (stream != NULL) {
        if (fread(disk, 1U, sizeof(disk), stream) != sizeof(disk)) {
            fclose(stream);
            state->status = GE_3DS_SAVE_PROVIDER_READ_FAILED;
            return state->status;
        }
        trailing = fgetc(stream);
        fclose(stream);
        if (trailing != EOF || !ge_save_decode(disk, ge_save_slot(state))) {
            state->status = GE_3DS_SAVE_PROVIDER_CORRUPT;
            return state->status;
        }
        if (ge_save_get_folder_exact(ge_save_slot(state)) != (u32)folder) {
            state->status = GE_3DS_SAVE_PROVIDER_CORRUPT;
            return state->status;
        }
        ++state->loads;
    } else {
        *ge_save_slot(state) = blank;
        ge_save_set_folder_exact(ge_save_slot(state), folder);
        ge_save_slot(state)->completion_bitflags &= ~SAVEFLAG_DORESET;
        state->status = ge_save_write(state);
        if (state->status != GE_3DS_SAVE_PROVIDER_OK) return state->status;
    }
    state->ready = 1U;
    state->status = GE_3DS_SAVE_PROVIDER_OK;
    return state->status;
}

Ge3dsSaveProviderStatus ge_3ds_save_provider_select(
    Ge3dsSaveProvider *state, const char *path, int32_t folder)
{
    Ge3dsSaveProvider candidate;
    Ge3dsSaveProviderStatus status;

    if (state == NULL || path == NULL || folder < FOLDER1
            || folder >= MAX_FOLDER_COUNT)
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;
    if (state->ready && state->folder == folder) {
        selected_folder_num = folder;
        return GE_3DS_SAVE_PROVIDER_OK;
    }
    status = ge_3ds_save_provider_init(&candidate, path, folder);
    if (status != GE_3DS_SAVE_PROVIDER_OK) return status;
    ge_3ds_save_provider_close(state);
    *state = candidate;
    /* interface_menu05_fileselect publishes the wallet selection before any
     * later end_of_mission_briefing reads it. Keep this assignment after the
     * new slot is known-good so a corrupt/unreadable folder cannot desync the
     * canonical selection from the active durable provider. */
    selected_folder_num = folder;
    return GE_3DS_SAVE_PROVIDER_OK;
}

void ge_3ds_save_provider_close(Ge3dsSaveProvider *state)
{
    if (state == NULL) return;
    state->ready = 0U;
}

void ge_3ds_save_provider_make_mission_result_providers(
    Ge3dsSaveProvider *state, GeOriginalMissionResultProviders *providers)
{
    if (providers == NULL) return;
    memset(providers, 0, sizeof(*providers));
    if (state == NULL || !state->ready) return;
    providers->context = state;
    providers->unlock_stage = ge_save_provider_unlock_stage;
    providers->save_for_folder = ge_save_provider_for_folder;
    providers->is_cheat_unlocked = ge_save_provider_is_cheat_unlocked;
    providers->unlock_cheat = ge_save_provider_unlock_cheat;
}

int32_t ge_3ds_save_provider_stage_time(
    const Ge3dsSaveProvider *state, int32_t mission, int32_t difficulty)
{
    if (state == NULL || !state->ready) return 0;
    return ge_save_get_stage_time_exact((save_data *)ge_save_slot_const(state),
        (LEVEL_SOLO_SEQUENCE)mission, (DIFFICULTY)difficulty);
}

int ge_3ds_save_provider_007_unlocked(const Ge3dsSaveProvider *state)
{
    int32_t mission;
    if (state == NULL || !state->ready) return 0;
    if ((((const save_data *)ge_save_slot_const(state))->flag_007 & 1U)
            != 0U)
        return 1;
    for (mission = SP_LEVEL_DAM; mission < SP_LEVEL_MAX; ++mission)
        if (ge_3ds_save_provider_stage_time(
                state, mission, DIFFICULTY_00) == 0) return 0;
    return 1;
}

int32_t ge_3ds_save_provider_stage_status(
    const Ge3dsSaveProvider *state, int32_t mission, int32_t difficulty)
{
    int32_t candidate;
    int32_t prior;
    if (state == NULL || !state->ready
            || mission < SP_LEVEL_DAM || mission >= SP_LEVEL_MAX
            || difficulty < DIFFICULTY_AGENT
            || difficulty >= DIFFICULTY_MAX)
        return STAGESTATUS_LOCKED;
    if (difficulty == DIFFICULTY_007) {
        return ge_3ds_save_provider_007_unlocked(state)
            ? STAGESTATUS_UNLOCKED : STAGESTATUS_LOCKED;
    }
    if (ge_3ds_save_provider_stage_time(state, mission, difficulty) != 0)
        return STAGESTATUS_COMPLETED;
    if ((mission == SP_LEVEL_AZTEC && difficulty < DIFFICULTY_SECRET)
            || (mission == SP_LEVEL_EGYPT && difficulty < DIFFICULTY_00))
        return STAGESTATUS_LOCKED;
    for (candidate = difficulty; candidate < DIFFICULTY_MAX; ++candidate) {
        for (prior = SP_LEVEL_DAM; prior < mission; ++prior)
            if (ge_3ds_save_provider_stage_time(
                    state, prior, candidate) == 0) break;
        if (mission <= prior) return STAGESTATUS_UNLOCKED;
    }
    if (difficulty < DIFFICULTY_007 && mission < SP_LEVEL_AZTEC) {
        for (candidate = difficulty;
                candidate < DIFFICULTY_MAX; ++candidate)
            if (ge_3ds_save_provider_stage_time(
                    state, mission - 1, candidate) != 0)
                return STAGESTATUS_UNLOCKED;
    }
    if (difficulty < DIFFICULTY_007) {
        for (prior = SP_LEVEL_DAM; prior < SP_LEVEL_AZTEC; ++prior)
            if (ge_3ds_save_provider_stage_time(
                    state, prior, DIFFICULTY_AGENT) == 0) break;
        if (prior >= SP_LEVEL_AZTEC) {
            for (candidate = DIFFICULTY_AGENT;
                    candidate < difficulty; ++candidate)
                if (ge_3ds_save_provider_stage_time(
                        state, mission, candidate) == 0) break;
            if (difficulty <= candidate) return STAGESTATUS_UNLOCKED;
        }
    }
    return mission == SP_LEVEL_DAM
        ? STAGESTATUS_UNLOCKED : STAGESTATUS_LOCKED;
}

int ge_3ds_save_provider_cheat_unlocked(
    const Ge3dsSaveProvider *state, int32_t mission)
{
    if (state == NULL || !state->ready) return 0;
    return ge_save_is_cheat_unlocked_exact(
        (save_data *)ge_save_slot_const(state), mission);
}

int ge_3ds_save_provider_highest_completed(
    const Ge3dsSaveProvider *state, int32_t *mission, int32_t *difficulty)
{
    int32_t stage;
    int32_t candidate;
    if (mission != NULL) *mission = SP_LEVEL_DAM - 1;
    if (difficulty != NULL) *difficulty = DIFFICULTY_MULTI;
    if (state == NULL || !state->ready) return 0;

    /* Exact fileGetHighestStageDifficultyCompletedForFolder search order:
     * the highest difficulty wins before the highest stage at that
     * difficulty.  Keeping this order matters to the wallet summary. */
    for (candidate = DIFFICULTY_007;
            candidate >= DIFFICULTY_AGENT; --candidate) {
        for (stage = SP_LEVEL_EGYPT; stage >= SP_LEVEL_DAM; --stage) {
            const int completed = candidate == DIFFICULTY_007
                ? ge_3ds_save_provider_007_unlocked(state)
                : ge_3ds_save_provider_stage_time(
                    state, stage, candidate) != 0;
            if (completed) {
                if (mission != NULL) *mission = stage;
                if (difficulty != NULL) *difficulty = candidate;
                return 1;
            }
        }
    }
    return 0;
}

Ge3dsSaveProviderStatus ge_3ds_save_provider_copy_if_empty(
    const Ge3dsSaveProvider *source, Ge3dsSaveProvider *destination)
{
    save_data copied;
    if (source == NULL || destination == NULL || source == destination
            || !source->ready || !destination->ready)
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;
    if (!ge_3ds_save_provider_highest_completed(source, NULL, NULL)
            || ge_3ds_save_provider_highest_completed(
                destination, NULL, NULL))
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;

    /* Exact mutation from fileCopyFolderToFirstFree: copy the full authored
     * slot, rewrite only its folder identity, and persist via the same CRC
     * writer used for mission results.  The caller owns the canonical
     * first-free search across its four mounted providers. */
    copied = *ge_save_slot_const(source);
    ge_save_set_folder_exact(&copied, (u32)destination->folder);
    *ge_save_slot(destination) = copied;
    destination->status = ge_save_write(destination);
    return destination->status;
}

Ge3dsSaveProviderStatus ge_3ds_save_provider_erase(
    Ge3dsSaveProvider *state)
{
    save_data blank = BLANKSAVEDATA;
    if (state == NULL || !state->ready)
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;
    /* Exact fileDeleteSaveForFolder gate: an empty wallet is not rewritten. */
    if (!ge_3ds_save_provider_highest_completed(state, NULL, NULL))
        return GE_3DS_SAVE_PROVIDER_OK;
    ge_save_set_folder_exact(&blank, (u32)state->folder);
    blank.completion_bitflags &= ~SAVEFLAG_DORESET;
    /* Remaining state-owning statements from fileDeleteSaveForFolder:
     * a reset wallet selects the Bond associated with its folder number. */
    blank.completion_bitflags &= ~SAVEFLAG_BOND;
    blank.completion_bitflags |=
        ((u32)state->folder << 5) & SAVEFLAG_BOND;
    *ge_save_slot(state) = blank;
    state->status = ge_save_write(state);
    return state->status;
}

Ge3dsSaveProviderStatus ge_3ds_save_provider_persist_settings(
    Ge3dsSaveProvider *state, uint8_t music_volume, uint8_t sfx_volume,
    uint16_t options)
{
    save_data *save;
    if (state == NULL || !state->ready)
        return GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT;
    save = ge_save_slot(state);
    /* Exact fileClearSavefileForFolder behavior after
     * fileSaveSettingsForFolder: preserve every mission/folder/cheat field
     * and skip EEPROM replacement when the settings snapshot is unchanged. */
    if (save->music_vol == music_volume && save->sfx_vol == sfx_volume
            && save->options == options)
        return GE_3DS_SAVE_PROVIDER_OK;
    save->music_vol = music_volume;
    save->sfx_vol = sfx_volume;
    save->options = options;
    state->status = ge_save_write(state);
    if (state->status == GE_3DS_SAVE_PROVIDER_OK)
        ++state->settings_updates;
    return state->status;
}

const char *ge_3ds_save_provider_status_name(Ge3dsSaveProviderStatus status)
{
    switch (status) {
    case GE_3DS_SAVE_PROVIDER_OK: return "ok";
    case GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT: return "invalid_argument";
    case GE_3DS_SAVE_PROVIDER_PATH_TOO_LONG: return "path_too_long";
    case GE_3DS_SAVE_PROVIDER_READ_FAILED: return "read_failed";
    case GE_3DS_SAVE_PROVIDER_CORRUPT: return "corrupt";
    case GE_3DS_SAVE_PROVIDER_WRITE_FAILED: return "write_failed";
    }
    return "unknown";
}
