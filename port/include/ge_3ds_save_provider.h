#ifndef GE_3DS_SAVE_PROVIDER_H
#define GE_3DS_SAVE_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_mission_result.h"

#define GE_3DS_SAVE_SLOT_BYTES 96U
#define GE_3DS_SAVE_PATH_CAPACITY 256U

/* Canonical file2.h EEPROM option layout, exposed at the durable provider
 * boundary without importing the N64 save/model ABI into platform code. */
#define GE_3DS_SAVE_OPTION_INVERTLOOK    UINT16_C(0x0001)
#define GE_3DS_SAVE_OPTION_AUTOAIM       UINT16_C(0x0002)
#define GE_3DS_SAVE_OPTION_AIMCONTROL    UINT16_C(0x0004)
#define GE_3DS_SAVE_OPTION_SIGHTONSCREEN UINT16_C(0x0008)
#define GE_3DS_SAVE_OPTION_LOOKAHEAD     UINT16_C(0x0010)
#define GE_3DS_SAVE_OPTION_DISPLAYAMMO   UINT16_C(0x0020)
#define GE_3DS_SAVE_OPTION_SCREENWIDE    UINT16_C(0x0040)
#define GE_3DS_SAVE_OPTION_SCREENRATIO   UINT16_C(0x0080)
#define GE_3DS_SAVE_OPTION_CONTROLTYPE   UINT16_C(0x0700)
#define GE_3DS_SAVE_OPTION_SCREENCINEMA  UINT16_C(0x0800)

typedef enum Ge3dsSaveProviderStatus {
    GE_3DS_SAVE_PROVIDER_OK = 0,
    GE_3DS_SAVE_PROVIDER_INVALID_ARGUMENT,
    GE_3DS_SAVE_PROVIDER_PATH_TOO_LONG,
    GE_3DS_SAVE_PROVIDER_READ_FAILED,
    GE_3DS_SAVE_PROVIDER_CORRUPT,
    GE_3DS_SAVE_PROVIDER_WRITE_FAILED
} Ge3dsSaveProviderStatus;

typedef struct Ge3dsSaveProvider {
    uint32_t slot_words[GE_3DS_SAVE_SLOT_BYTES / sizeof(uint32_t)];
    char path[GE_3DS_SAVE_PATH_CAPACITY];
    int32_t folder;
    uint32_t loads;
    uint32_t writes;
    uint32_t completion_updates;
    uint32_t cheat_updates;
    uint32_t settings_updates;
    Ge3dsSaveProviderStatus status;
    uint8_t ready;
} Ge3dsSaveProvider;

Ge3dsSaveProviderStatus ge_3ds_save_provider_init(
    Ge3dsSaveProvider *state, const char *path, int32_t folder);
/* Atomically installs a folder as the live frontend save and publishes the
 * canonical front.c selected_folder_num used by end_of_mission_briefing.
 * Read-only wallet probes must continue to use init/close so they cannot
 * change the selected campaign folder. */
Ge3dsSaveProviderStatus ge_3ds_save_provider_select(
    Ge3dsSaveProvider *state, const char *path, int32_t folder);
void ge_3ds_save_provider_close(Ge3dsSaveProvider *state);
void ge_3ds_save_provider_make_mission_result_providers(
    Ge3dsSaveProvider *state, GeOriginalMissionResultProviders *providers);

int32_t ge_3ds_save_provider_stage_time(
    const Ge3dsSaveProvider *state, int32_t mission, int32_t difficulty);
int32_t ge_3ds_save_provider_stage_status(
    const Ge3dsSaveProvider *state, int32_t mission, int32_t difficulty);
int ge_3ds_save_provider_007_unlocked(const Ge3dsSaveProvider *state);
int ge_3ds_save_provider_cheat_unlocked(
    const Ge3dsSaveProvider *state, int32_t mission);
int ge_3ds_save_provider_highest_completed(
    const Ge3dsSaveProvider *state, int32_t *mission, int32_t *difficulty);
Ge3dsSaveProviderStatus ge_3ds_save_provider_copy_if_empty(
    const Ge3dsSaveProvider *source, Ge3dsSaveProvider *destination);
Ge3dsSaveProviderStatus ge_3ds_save_provider_erase(
    Ge3dsSaveProvider *state);
/* Durable owner for fileClearSavefileForFolder's final mutation.  The caller
 * supplies the values produced by the unchanged fileSaveSettingsForFolder
 * getters; this preserves the rest of the original 0x60-byte slot and writes
 * only when one of the three settings fields changed. */
Ge3dsSaveProviderStatus ge_3ds_save_provider_persist_settings(
    Ge3dsSaveProvider *state, uint8_t music_volume, uint8_t sfx_volume,
    uint16_t options);
const char *ge_3ds_save_provider_status_name(Ge3dsSaveProviderStatus status);

#endif
