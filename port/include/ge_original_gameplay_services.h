#ifndef GE_ORIGINAL_GAMEPLAY_SERVICES_H
#define GE_ORIGINAL_GAMEPLAY_SERVICES_H

#include <stddef.h>
#include <stdint.h>

#include "ge_audio_output.h"
#include "ge_original_sfx_bank.h"

#define GE_ORIGINAL_SFX_CACHE_BYTE_LIMIT (2U * 1024U * 1024U)

typedef struct GeOriginalGameplayServiceStats {
    uint32_t sound_play_calls;
    uint32_t sound_deactivate_calls;
    uint32_t active_sounds;
    uint32_t decoded_sound_starts;
    uint32_t sound_decode_failures;
    uint32_t sound_cache_hits;
    uint32_t sound_cache_misses;
    uint32_t sound_cache_evictions;
    uint32_t sound_cache_bytes;
    uint32_t sound_cache_peak_bytes;
    uint32_t sound_parameter_events;
    uint64_t mixed_audio_frames;
    uint32_t hud_messages;
    uint32_t watch_resets;
    uint32_t interaction_tests;
    uint32_t interaction_hits;
    uint32_t unsupported_object_calls;
    uint32_t settings_persist_calls;
    uint32_t settings_persist_failures;
    int16_t last_sound_id;
    uint32_t music_play_calls;
    int32_t last_music_id;
} GeOriginalGameplayServiceStats;

void ge_original_gameplay_services_bind_audio_profile(uint64_t (*clock)(void *), void *context);
uint64_t ge_original_gameplay_services_audio_decode_ticks(void);
void ge_original_gameplay_services_reset(void);
void ge_original_gameplay_services_bind_visible_props(void *const *props,
                                                       size_t count);
/* Exact reverse propFindForInteract traversal over a caller-owned renderer
 * subset. The unchanged objTestForInteract body owns cone, distance, LOS and
 * g_InteractProp publication. */
void *ge_original_gameplay_services_find_interactable(
    void *const *props, size_t count);
/* Routes a canonical SFX identifier through the already-bound original bank
 * and platform audio sink. */
void ge_original_gameplay_services_play_sfx(uint32_t sfx_id);
/* Sends an authored M_* track identifier into the unchanged musicTrack1Play
 * state machine.  Until native CSeq synthesis is bound, the canonical music
 * port records the explicit unavailable backend frontier. */
void ge_original_gameplay_services_play_music(int32_t music_id);
void ge_original_gameplay_services_bind_audio(
    const GeOriginalSfxBank *bank, GeAudioOutput *output);
/* Supplies the platform relocation owner used by the unchanged modelLoad and
 * weaponLoadProjectileModels bodies for the current stage's PitemZ table. */
void ge_original_gameplay_services_bind_model_loader(
    void *context, int32_t (*model_load)(void *context, int32_t model_id));
/* Durable owner for fileClearSavefileForFolder's final compare/write. The
 * unchanged fileSaveSettingsForFolder value composition stays in the
 * gameplay service; the platform callback only commits those exact fields. */
void ge_original_gameplay_services_bind_settings_persistence(
    void *context,
    int (*persist_settings)(void *context, uint8_t music_volume,
                            uint8_t sfx_volume, uint16_t options));
void ge_original_gameplay_services_set_exact_gun_dispatch(int active);
void ge_original_gameplay_services_tick(void);
void ge_original_gameplay_services_snapshot(
    GeOriginalGameplayServiceStats *stats);

#endif
