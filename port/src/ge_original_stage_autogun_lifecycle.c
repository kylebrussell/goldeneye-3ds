#include <ultra64.h>
#include <bondtypes.h>

#include "game/gun.h"
#include "ge_original_stage_autogun_lifecycle.h"

#include <string.h>

extern s32 objTick(PropRecord *prop);
extern void objFree(ObjectRecord *object, s32 free_prop, s32 can_regenerate);

GeOriginalStageSecurityStatus
ge_original_stage_autogun_lifecycle_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance)
{
    GeOriginalStageSecurityStatus status;
    if (request == NULL || request->record == NULL
            || request->record->type != PROPDEF_AUTOGUN)
        return GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION;
    status = ge_original_stage_security_construct(
        request, definition, definition_size, providers, instance);
    if (status != GE_ORIGINAL_STAGE_SECURITY_OK) return status;
    return ge_original_stage_autogun_lifecycle_is_live(instance)
        ? GE_ORIGINAL_STAGE_SECURITY_OK
        : GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED;
}

int ge_original_stage_autogun_lifecycle_is_live(
    const GeOriginalStageSecurityInstance *instance)
{
    const AutogunRecord *autogun;
    const PropRecord *prop;
    if (instance == NULL || instance->type != PROPDEF_AUTOGUN
            || !instance->constructed || !instance->runtime_ready
            || instance->definition == NULL || instance->prop == NULL
            || instance->model == NULL || instance->beam == NULL)
        return 0;
    autogun = instance->definition;
    prop = instance->prop;
    return autogun->type == PROPDEF_AUTOGUN
        && autogun->prop == prop && prop->obj == (ObjectRecord *)autogun
        && autogun->model == instance->model
        && autogun->beam == instance->beam;
}

GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_tick_exact(
    GeOriginalStageSecurityInstance *instance, int32_t *tick_operation)
{
    s32 operation;
    if (instance == NULL || tick_operation == NULL)
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT;
    if (!ge_original_stage_autogun_lifecycle_is_live(instance))
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE;
    operation = objTick(instance->prop);
    *tick_operation = operation;
    if (operation == TICKOP_FREE) {
        instance->runtime_ready = 0U;
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_REMOVED;
    }
    return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK;
}

GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_advance_beam_exact(
    GeOriginalStageSecurityInstance *instance)
{
    AutogunRecord *autogun;
    if (instance == NULL)
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT;
    if (!ge_original_stage_autogun_lifecycle_is_live(instance))
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE;
    autogun = instance->definition;
    gunAdvanceBeamTimer((BeamRecord *)autogun->beam);
    return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK;
}

int ge_original_stage_autogun_lifecycle_beam_snapshot(
    const GeOriginalStageSecurityInstance *instance,
    GeOriginalStageAutogunBeamSnapshot *snapshot)
{
    const AutogunRecord *autogun;
    const struct beam *beam;
    if (snapshot == NULL
            || !ge_original_stage_autogun_lifecycle_is_live(instance))
        return 0;
    autogun = instance->definition;
    beam = autogun->beam;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->age = beam->age;
    snapshot->active = (uint8_t)(beam->age >= 0);
    /* setupAutogun initializes only the age byte in its stage-pool beam.
     * Preserve that canonical ownership: inactive fields are not published
     * until objTick has authored a shot into them. */
    if (!snapshot->active) return 1;
    memcpy(snapshot->origin, beam->from.f, sizeof(snapshot->origin));
    memcpy(snapshot->direction, beam->dir.f, sizeof(snapshot->direction));
    snapshot->maximum_distance = beam->maxdist;
    snapshot->speed = beam->speed;
    snapshot->minimum_distance = beam->mindist;
    snapshot->distance = beam->dist;
    snapshot->weapon_id = beam->weaponnum;
    return 1;
}

int ge_original_stage_autogun_lifecycle_runtime_snapshot(
    const GeOriginalStageSecurityInstance *instance,
    GeOriginalStageAutogunRuntimeSnapshot *snapshot)
{
    const AutogunRecord *autogun;
    const struct beam *beam;
    if (snapshot == NULL
            || !ge_original_stage_autogun_lifecycle_is_live(instance))
        return 0;
    autogun = instance->definition;
    beam = autogun->beam;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->yaw = autogun->unk90;
    snapshot->pitch = autogun->unk9C;
    snapshot->barrel_spin_speed = autogun->unkB0;
    snapshot->pending_damage = autogun->unkD4;
    snapshot->shot_counter = autogun->unkAC;
    snapshot->last_tracking_tick = autogun->unkB8;
    snapshot->last_line_of_sight_tick = autogun->unkBC;
    snapshot->next_sound_tick = autogun->unkC0;
    snapshot->tracking_active = (uint8_t)(autogun->is_active != 0);
    snapshot->sound_slot_mask = (uint8_t)(
        (autogun->unkC4 != NULL ? UINT8_C(1) : UINT8_C(0))
        | (autogun->unkC8 != NULL ? UINT8_C(2) : UINT8_C(0)));
    snapshot->beam_active = (uint8_t)(beam->age >= 0);
    return 1;
}

GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_exact(
    GeOriginalStageSecurityInstance *instance,
    int free_prop, int can_regenerate)
{
    if (instance == NULL)
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT;
    if (!ge_original_stage_autogun_lifecycle_is_live(instance))
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE;
    objFree(instance->definition, free_prop != 0, can_regenerate != 0);
    instance->runtime_ready = 0U;
    instance->prop = NULL;
    instance->model = NULL;
    instance->beam = NULL;
    return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK;
}

GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
    GeOriginalStageSecurityInstance *instance,
    const GeOriginalStageAutogunCleanupProviders *providers)
{
    void *model;
    if (instance == NULL || providers == NULL
            || providers->release_model == NULL)
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT;
    if (!ge_original_stage_autogun_lifecycle_is_live(instance))
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE;
    model = instance->model;
    /* Preserve objFree ordering: sound handles, impacts/embedment, room and
     * child teardown, model relations/header, then prop delist/free. */
    objFree(instance->definition, TRUE, FALSE);
    instance->runtime_ready = 0U;
    instance->prop = NULL;
    instance->beam = NULL;
    if (!providers->release_model(providers->context, model)) {
        /* Retain the stable address so the owning provider can diagnose or
         * perform its stage-wide fallback destroy without guessing it. */
        instance->model = model;
        return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED;
    }
    instance->model = NULL;
    return GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK;
}

const char *ge_original_stage_autogun_lifecycle_status_name(
    GeOriginalStageAutogunLifecycleStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK: return "ok";
    case GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE: return "not live";
    case GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_REMOVED: return "removed";
    case GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED:
        return "model release failed";
    default: return "unknown";
    }
}
