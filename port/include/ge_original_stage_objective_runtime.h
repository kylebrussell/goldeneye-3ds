#ifndef GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_H
#define GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_H

#include "ge_original_stage_objectives.h"

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalStageObjectiveValue {
    GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE = 1,
    GE_ORIGINAL_STAGE_OBJECTIVE_FAILED = 2
} GeOriginalStageObjectiveValue;

typedef enum GeOriginalStageObjectiveRuntimeBlocker {
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_READY = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_REGISTRY_BLOCKED,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVENTORY_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_STAGE_FLAGS_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_KEY_ANALYZER_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PAD_STAN_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_PHOTOGRAPH_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_UNSUPPORTED_CRITERION
} GeOriginalStageObjectiveRuntimeBlocker;

typedef enum GeOriginalStageObjectiveRuntimeStatus {
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NO_MEMORY,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OBJECTIVE_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
} GeOriginalStageObjectiveRuntimeStatus;

/* Boolean callbacks use 1/0 for the canonical true/false result and a
 * negative value when that original service is not live.  The photograph
 * callback owns only objGetOnscreenRenderBounds, projection and live viewport
 * containment; the runtime performs the original prop/onscreen/depth/health
 * prefix before calling it. */
typedef struct GeOriginalStageObjectiveRuntimeProviders {
    void *context;
    int (*prop_in_inventory)(void *context, const void *prop);
    int (*stage_flag_set)(void *context, uint32_t flags);
    int (*key_analyzer_complete)(void *context);
    int (*photograph_bounds_inside_view)(
        void *context, const void *object, const void *prop);
} GeOriginalStageObjectiveRuntimeProviders;

typedef struct GeOriginalStageObjectiveEvaluation {
    size_t objective_index;
    size_t criterion_index;
    uint8_t menu;
    uint8_t value;
    uint8_t blocker;
} GeOriginalStageObjectiveEvaluation;

typedef struct GeOriginalStageObjectiveStatusChange {
    uint8_t menu;
    uint8_t available_index;
    uint8_t value;
    uint8_t reserved;
} GeOriginalStageObjectiveStatusChange;

typedef struct GeOriginalStageObjectiveRuntime {
    GeOriginalStageObjectiveRegistry *registry;
    GeOriginalStageObjectiveRuntimeProviders providers;
    /* Native TagObjectRecord sidecars published in the original setup order.
     * Kept opaque here so 3DS-facing users do not need bondtypes.h. */
    void *native_tags;
    size_t native_tag_count;
    uint8_t displayed_status[GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    uint8_t bound;
} GeOriginalStageObjectiveRuntime;

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_begin(
    GeOriginalStageObjectiveRuntime *runtime,
    GeOriginalStageObjectiveRegistry *registry,
    const GeOriginalStageObjectiveRuntimeProviders *providers);
void ge_original_stage_objective_runtime_close(
    GeOriginalStageObjectiveRuntime *runtime);

/* Unchanged cleanupObjectives rule over the native displayed-status cache. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_cleanup(
    GeOriginalStageObjectiveRuntime *runtime);

/* Exact get_status_of_objective criterion ordering and status precedence. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_evaluate(
    GeOriginalStageObjectiveRuntime *runtime, uint8_t menu,
    GeOriginalStageObjectiveEvaluation *evaluation);

/* Blocker-aware display_objective_status_text_on_status_change state pass.
 * When every objective is evaluable this preserves the original menu order,
 * difficulty filtering and displayed-status commit ordering. On a blocker it
 * returns without committing any displayed status, leaving HUD state valid
 * for a later retry. `changes` has GE_ORIGINAL_STAGE_OBJECTIVE_MAX entries. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_collect_status_changes(
    GeOriginalStageObjectiveRuntime *runtime, int32_t selected_difficulty,
    GeOriginalStageObjectiveStatusChange *changes, size_t *change_count,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation);

/* Exact objectivestatusCheckRoomEntered and CheckDeposit list traversal over
 * the native criterion sidecars. `room_id` must be the live player/object
 * STAN room supplied by their original callers. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_check_room_entered(
    GeOriginalStageObjectiveRuntime *runtime, int32_t room_id,
    GeOriginalStageObjectiveEvaluation *result);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_check_deposit(
    GeOriginalStageObjectiveRuntime *runtime, int32_t weapon_num,
    int32_t room_id, GeOriginalStageObjectiveEvaluation *result);

/* Exact objectiveTakePictureHandler ordering. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_runtime_take_picture(
    GeOriginalStageObjectiveRuntime *runtime,
    GeOriginalStageObjectiveEvaluation *result);

const char *ge_original_stage_objective_runtime_status_name(
    GeOriginalStageObjectiveRuntimeStatus status);
const char *ge_original_stage_objective_runtime_blocker_name(
    GeOriginalStageObjectiveRuntimeBlocker blocker);

/* Ready-to-bind exact live providers. `context` is the current native
 * `struct player *` for inventory/key/photo viewport state. They return -1
 * only when that boundary is not bound. Photograph binding additionally
 * validates the live ObjectRecord/PropRecord/model/BBOX matrix relation
 * before entering the unchanged projection bodies. Stage flags use unchanged
 * chrHasStageFlag. */
int ge_original_stage_objective_prop_in_inventory_exact(
    void *context, const void *prop);
int ge_original_stage_objective_stage_flag_set_exact(
    void *context, uint32_t flags);
int ge_original_stage_objective_key_analyzer_complete_exact(void *context);
int ge_original_stage_objective_photograph_binding_ready_exact(
    void *context, const void *object, const void *prop);
int ge_original_stage_objective_photograph_bounds_inside_view_exact(
    void *context, const void *object, const void *prop);

#endif
