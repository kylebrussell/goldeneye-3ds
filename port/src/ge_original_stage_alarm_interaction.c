#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_alarm_interaction.h"

#include <string.h>

extern void alarmActivate(void);
extern void alarmDeactivate(void);
extern bool alarmIsActive(void);
extern void sub_GAME_7F03E6A0(PropRecord *prop);

GeOriginalStageAlarmInteractionStatus
ge_original_stage_object_interact_exact(
    void *opaque_prop,
    const GeOriginalStageAlarmInteractionProviders *providers,
    GeOriginalStageAlarmInteractionResult *result)
{
    PropRecord *prop = opaque_prop;
    ObjectRecord *obj;
    TICKOP op = TICKOP_NONE;
    GeOriginalStageAlarmInteractionStatus status =
        GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT;
    uint8_t was_active;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
        result->status = status;
    }
    if (prop == NULL || providers == NULL || result == NULL
            || prop->obj == NULL) return status;
    obj = prop->obj;
    if (obj->type == PROPDEF_ALARM && providers->play_sfx == NULL) {
        status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_SFX_SERVICE;
        goto done;
    }
    if ((obj->flags & PROPFLAG_00080000) != 0U
            && providers->pickup_by_player == NULL) {
        status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_PICKUP_SERVICE;
        goto done;
    }

    /* Exact PROPDEF_ALARM branch and common tail of propobjInteract. The two
     * external services are expressed by the callbacks above; all game-state
     * mutations retain their original ordering. */
    was_active = UINT8_C(0);
    if (obj->type == PROPDEF_ALARM)
    {
        providers->play_sfx(providers->context, (uint32_t)ALARM_SWITCH_SFX);
        was_active = alarmIsActive() != FALSE ? UINT8_C(1) : UINT8_C(0);
        if (was_active != 0U)
        {
            alarmDeactivate();
        }
        else
        {
            alarmActivate();
        }
        result->sfx_id = (uint32_t)ALARM_SWITCH_SFX;
        result->was_active = was_active;
        result->is_active = was_active == 0U ? UINT8_C(1) : UINT8_C(0);
    }

    if (obj->flags & PROPFLAG_00080000)
    {
        op = (TICKOP)providers->pickup_by_player(
            providers->context, prop, TRUE);
        result->pickup_requested = UINT8_C(1);
    }

    obj->runtime_bitflags |= GE_ORIGINAL_STAGE_ALARM_RUNTIME_ACTIVATED;
    sub_GAME_7F03E6A0(prop);

    result->tick_operation = (int32_t)op;
    result->activation_published = UINT8_C(1);
    status = GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK;
done:
    result->status = status;
    return status;
}

GeOriginalStageAlarmInteractionStatus
ge_original_stage_alarm_interact_exact(
    void *opaque_prop,
    const GeOriginalStageAlarmInteractionProviders *providers,
    GeOriginalStageAlarmInteractionResult *result)
{
    PropRecord *prop = opaque_prop;
    if (prop == NULL || prop->obj == NULL
            || ((ObjectRecord *)prop->obj)->type != PROPDEF_ALARM) {
        if (result != NULL) {
            memset(result, 0, sizeof(*result));
            result->status =
                GE_ORIGINAL_STAGE_ALARM_INTERACTION_NOT_ALARM;
        }
        return GE_ORIGINAL_STAGE_ALARM_INTERACTION_NOT_ALARM;
    }
    return ge_original_stage_object_interact_exact(
        opaque_prop, providers, result);
}

const char *ge_original_stage_alarm_interaction_status_name(
    GeOriginalStageAlarmInteractionStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK: return "ok";
    case GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_ALARM_INTERACTION_NOT_ALARM:return "not alarm";
    case GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_SFX_SERVICE:
        return "missing switch SFX service";
    case GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_PICKUP_SERVICE:
        return "missing pickup service";
    }
    return "unknown";
}
