#ifndef GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_H
#define GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_H

#include "ge_original_stage_objective_runtime.h"

#include <stdint.h>

typedef enum GeOriginalStageObjectiveLiveEvent {
    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_ROOM = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_DEPOSIT,
    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_PHOTOGRAPH,
    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_EVENT_COUNT
} GeOriginalStageObjectiveLiveEvent;

typedef struct GeOriginalStageObjectiveLiveStatus {
    uint64_t calls[GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_EVENT_COUNT];
    GeOriginalStageObjectiveRuntimeStatus last_status;
    GeOriginalStageObjectiveRuntimeBlocker last_blocker;
    GeOriginalStageObjectiveLiveEvent last_event;
    uint8_t bound;
} GeOriginalStageObjectiveLiveStatus;

void ge_original_stage_objective_live_bind(
    GeOriginalStageObjectiveRuntime *runtime);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_check_room_entered(int32_t room_id);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_check_deposit(
    int32_t weapon_num, int32_t room_id);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_take_picture(void);

/* Safe status-consumer boundary for mission AI/HUD callers. A blocked
 * criterion remains a returned blocker and is never collapsed to incomplete. */
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_evaluate(
    uint8_t menu, GeOriginalStageObjectiveEvaluation *evaluation);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_collect_status_changes(
    int32_t selected_difficulty,
    GeOriginalStageObjectiveStatusChange *changes, size_t *change_count,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_cleanup(void);
int ge_original_stage_objective_live_count(void);
int ge_original_stage_objective_live_difficulty(uint8_t menu,
                                                 int8_t *difficulty);
/* Publishes the authored objective-start text ID for presentation adapters.
 * The string itself remains owned by the original language bank. */
int ge_original_stage_objective_live_text_id(uint8_t menu,
                                              uint16_t *text_id);
GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_all_complete(
    int32_t selected_difficulty, int *complete,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation);
void ge_original_stage_objective_live_status(
    GeOriginalStageObjectiveLiveStatus *status);

/* Canonical decomp call-site names. These are adapters only because the
 * pointer-width-safe registry cannot populate objective_status.c's serialized
 * linked records. */
void objectivestatusCheckRoomEntered(int32_t room_id);
void objectivestatusCheckDeposit(int32_t weapon_num, int32_t room_id);
void objectiveTakePictureHandler(void);
void cleanupObjectives(void);

/* Canonical campaign-AI ABI over the pointer-width-safe live registry. The
 * value mapping is identical to OBJECTIVESTATUS (incomplete/complete/failed).
 * A provider frontier remains visible through the live-status snapshot and
 * fails closed as incomplete. */
int32_t objectiveGetCount(void);
int32_t objectiveGetStatus_WEAK(int32_t objective_num, int32_t ignored);

#endif
