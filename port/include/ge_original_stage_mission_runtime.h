#ifndef GE_ORIGINAL_STAGE_MISSION_RUNTIME_H
#define GE_ORIGINAL_STAGE_MISSION_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

struct GeOriginalStageSetupRuntime;

typedef enum GeOriginalStageMissionRuntimeStatus {
    GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK = 0,
    GE_ORIGINAL_STAGE_MISSION_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_MISSION_RUNTIME_SETUP_UNBOUND,
    GE_ORIGINAL_STAGE_MISSION_RUNTIME_ALLOCATION_FAILED,
    GE_ORIGINAL_STAGE_MISSION_RUNTIME_ACTOR_MISMATCH
} GeOriginalStageMissionRuntimeStatus;

typedef struct GeOriginalStageMissionRuntime {
    const struct GeOriginalStageSetupRuntime *setup;
    size_t authored_background_list_count;
    size_t live_background_actor_count;
    uint64_t observed_ticks;
    uint64_t ai_offset_hash;
    uint32_t objective_registers;
    uint8_t globals_reset;
    uint8_t initialized;
} GeOriginalStageMissionRuntime;

/* Exact init_guards mission/AI-global reset. Call before authored guards are
 * constructed; background actor allocation remains in begin(), matching the
 * later proplvreset2 boundary. */
void ge_original_stage_mission_runtime_reset_globals(
    GeOriginalStageMissionRuntime *state);

/* Replays the unchanged post-prop-loader background-AI allocation boundary.
 * The resulting ChrRecords are ticked by the canonical chrlvAllChrTick call
 * already inside chrpropTick; this service deliberately does not dispatch a
 * second AI tick. */
GeOriginalStageMissionRuntimeStatus ge_original_stage_mission_runtime_begin(
    GeOriginalStageMissionRuntime *state,
    const struct GeOriginalStageSetupRuntime *setup);

/* Records evidence after one successful canonical chrpropTick. */
GeOriginalStageMissionRuntimeStatus ge_original_stage_mission_runtime_observe_tick(
    GeOriginalStageMissionRuntime *state);

/* Resolves one allocated background actor by its authored setup AI-list ID.
 * This is read-only scheduler evidence; the canonical chrlvAllChrTick body
 * remains the sole owner of advancing the actor and publishing aioffset. */
int ge_original_stage_mission_runtime_actor_offset(
    const GeOriginalStageMissionRuntime *state, int32_t ai_list_id,
    uint16_t *offset);

void ge_original_stage_mission_runtime_close(
    GeOriginalStageMissionRuntime *state);

const char *ge_original_stage_mission_runtime_status_name(
    GeOriginalStageMissionRuntimeStatus status);

#endif
