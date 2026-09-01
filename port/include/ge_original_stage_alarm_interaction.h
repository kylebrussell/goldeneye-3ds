#ifndef GE_ORIGINAL_STAGE_ALARM_INTERACTION_H
#define GE_ORIGINAL_STAGE_ALARM_INTERACTION_H

#include <stdint.h>

enum {
    GE_ORIGINAL_STAGE_ALARM_RUNTIME_ACTIVATED = UINT32_C(1) << 14U
};

typedef enum GeOriginalStageAlarmInteractionStatus {
    GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK = 0,
    GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_ALARM_INTERACTION_NOT_ALARM,
    GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_SFX_SERVICE,
    GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_PICKUP_SERVICE
} GeOriginalStageAlarmInteractionStatus;

typedef struct GeOriginalStageAlarmInteractionProviders {
    void *context;
    /* Platform audio sink for the exact ALARM_SWITCH_SFX request. */
    void (*play_sfx)(void *context, uint32_t sfx_id);
    /* Required only for an authored PROPFLAG_00080000 alarm. Return the exact
     * TICKOP produced by propPickupByPlayer(prop, TRUE). */
    int (*pickup_by_player)(void *context, void *prop, int immediate);
} GeOriginalStageAlarmInteractionProviders;

typedef struct GeOriginalStageAlarmInteractionResult {
    GeOriginalStageAlarmInteractionStatus status;
    uint32_t sfx_id;
    int32_t tick_operation;
    uint8_t was_active;
    uint8_t is_active;
    uint8_t activation_published;
    uint8_t pickup_requested;
} GeOriginalStageAlarmInteractionResult;

/* Exact complete propobjInteract body. Ordinary props (including Dam's
 * authored backup-terminal tags 6/7) take the common ACTIVATED/publication
 * tail; alarms additionally take the original sound/global-alarm branch.
 * Pickup-capable objects require the canonical pickup provider. */
GeOriginalStageAlarmInteractionStatus
ge_original_stage_object_interact_exact(
    void *prop, const GeOriginalStageAlarmInteractionProviders *providers,
    GeOriginalStageAlarmInteractionResult *result);

/* Alarm-only compatibility boundary retained for existing callers/tests. */
GeOriginalStageAlarmInteractionStatus
ge_original_stage_alarm_interact_exact(
    void *prop, const GeOriginalStageAlarmInteractionProviders *providers,
    GeOriginalStageAlarmInteractionResult *result);

const char *ge_original_stage_alarm_interaction_status_name(
    GeOriginalStageAlarmInteractionStatus status);

#endif
