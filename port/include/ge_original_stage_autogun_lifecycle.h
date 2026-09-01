#ifndef GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_H
#define GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_H

#include "ge_original_stage_security.h"

#include <stdint.h>

typedef enum GeOriginalStageAutogunLifecycleStatus {
    GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK = 0,
    GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE,
    GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_REMOVED,
    GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED
} GeOriginalStageAutogunLifecycleStatus;

typedef struct GeOriginalPitemModelProvider GeOriginalPitemModelProvider;

typedef struct GeOriginalStageAutogunCleanupProviders {
    void *context;
    /* Called after exact objFree has cleared Model::obj. The callback owns
     * the captured stable Model address and must release it at most once. */
    int (*release_model)(void *context, void *model_instance);
} GeOriginalStageAutogunCleanupProviders;

typedef struct GeOriginalStageAutogunBeamSnapshot {
    float origin[3];
    float direction[3];
    float maximum_distance;
    float speed;
    float minimum_distance;
    float distance;
    int32_t weapon_id;
    int32_t age;
    uint8_t active;
} GeOriginalStageAutogunBeamSnapshot;

typedef struct GeOriginalStageAutogunRuntimeSnapshot {
    float yaw;
    float pitch;
    float barrel_spin_speed;
    float pending_damage;
    int32_t shot_counter;
    int32_t last_tracking_tick;
    int32_t last_line_of_sight_tick;
    int32_t next_sound_tick;
    uint8_t tracking_active;
    uint8_t sound_slot_mask;
    uint8_t beam_active;
} GeOriginalStageAutogunRuntimeSnapshot;

/* Runs the existing canonical setupAutogun construction boundary and rejects
 * success unless every pointer needed by the unchanged frame lifecycle is
 * coherently published. */
GeOriginalStageSecurityStatus
ge_original_stage_autogun_lifecycle_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance);

/* Validates the exact ObjectRecord/PropRecord/Model/beam ownership published
 * by setupAutogun before an instance enters the canonical object scheduler. */
int ge_original_stage_autogun_lifecycle_is_live(
    const GeOriginalStageSecurityInstance *instance);

/* Standalone canonical object tick. Do not call this for an instance already
 * visited by propsTick in the same frame. The original tick operation is
 * returned to the active-prop owner for canonical delist/free handling. */
GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_tick_exact(
    GeOriginalStageSecurityInstance *instance, int32_t *tick_operation);

/* Exact chrpropTick beam-age step, called once after the object's frame tick. */
GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_advance_beam_exact(
    GeOriginalStageSecurityInstance *instance);

/* Read-only canonical beam publication for the platform renderer. */
int ge_original_stage_autogun_lifecycle_beam_snapshot(
    const GeOriginalStageSecurityInstance *instance,
    GeOriginalStageAutogunBeamSnapshot *snapshot);

/* Read-only evidence from the unchanged tracking/fire/damage/SFX state.  Raw
 * tick values remain canonical so a platform probe can compare them against
 * its published original game timer without introducing another clock. */
int ge_original_stage_autogun_lifecycle_runtime_snapshot(
    const GeOriginalStageSecurityInstance *instance,
    GeOriginalStageAutogunRuntimeSnapshot *snapshot);

/* Exact objFree lifecycle, including both autogun sound handles before common
 * model/prop teardown. The owning stage must not separately free the prop. */
GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_exact(
    GeOriginalStageSecurityInstance *instance,
    int free_prop, int can_regenerate);

/* Stage-close owner for Pitem-backed autoguns. Canonical objFree runs first
 * with freeprop=TRUE/canregen=FALSE, deactivating both sound handles and
 * returning the PropRecord to the original pool. The provider then releases
 * its captured Model slot exactly once; definition and 0x30 stage beam remain
 * owned by the caller's stage arena. */
GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
    GeOriginalStageSecurityInstance *instance,
    const GeOriginalStageAutogunCleanupProviders *providers);

/* Typed Pitem ownership adapter. This avoids casting the Pitem provider's
 * release function to the generic callback ABI while preserving the same
 * canonical objFree-before-model-release order. */
GeOriginalStageAutogunLifecycleStatus
ge_original_stage_autogun_lifecycle_cleanup_pitem_exact(
    GeOriginalStageSecurityInstance *instance,
    GeOriginalPitemModelProvider *models);

const char *ge_original_stage_autogun_lifecycle_status_name(
    GeOriginalStageAutogunLifecycleStatus status);

#endif
