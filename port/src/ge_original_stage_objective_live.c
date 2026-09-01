#include "ge_original_stage_objective_live.h"

#include <stddef.h>
#include <string.h>

static GeOriginalStageObjectiveRuntime *ge_live_runtime;
static GeOriginalStageObjectiveLiveStatus ge_live_status;

void ge_original_stage_objective_live_bind(
    GeOriginalStageObjectiveRuntime *runtime)
{
    ge_live_runtime = runtime;
    memset(&ge_live_status, 0, sizeof(ge_live_status));
    ge_live_status.bound = runtime != NULL && runtime->bound;
    ge_live_status.last_status = ge_live_status.bound
        ? GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
}

static GeOriginalStageObjectiveRuntimeStatus ge_live_record(
    GeOriginalStageObjectiveLiveEvent event,
    GeOriginalStageObjectiveRuntimeStatus status,
    const GeOriginalStageObjectiveEvaluation *evaluation)
{
    ++ge_live_status.calls[event];
    ge_live_status.last_event = event;
    ge_live_status.last_status = status;
    ge_live_status.last_blocker = status
            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
            && evaluation != NULL
        ? (GeOriginalStageObjectiveRuntimeBlocker)evaluation->blocker
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_READY;
    return status;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_check_room_entered(int32_t room_id)
{
    GeOriginalStageObjectiveEvaluation result;
    GeOriginalStageObjectiveRuntimeStatus status;
    if (ge_live_runtime == NULL)
        return ge_live_record(GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_ROOM,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND, NULL);
    status = ge_original_stage_objective_runtime_check_room_entered(
        ge_live_runtime, room_id, &result);
    return ge_live_record(
        GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_ROOM, status, &result);
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_check_deposit(
    int32_t weapon_num, int32_t room_id)
{
    GeOriginalStageObjectiveEvaluation result;
    GeOriginalStageObjectiveRuntimeStatus status;
    if (ge_live_runtime == NULL)
        return ge_live_record(GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_DEPOSIT,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND, NULL);
    status = ge_original_stage_objective_runtime_check_deposit(
        ge_live_runtime, weapon_num, room_id, &result);
    return ge_live_record(
        GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_DEPOSIT, status, &result);
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_take_picture(void)
{
    GeOriginalStageObjectiveEvaluation result;
    GeOriginalStageObjectiveRuntimeStatus status;
    if (ge_live_runtime == NULL)
        return ge_live_record(GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_PHOTOGRAPH,
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND, NULL);
    status = ge_original_stage_objective_runtime_take_picture(
        ge_live_runtime, &result);
    return ge_live_record(
        GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_PHOTOGRAPH, status, &result);
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_evaluate(
    uint8_t menu, GeOriginalStageObjectiveEvaluation *evaluation)
{
    return ge_live_runtime != NULL
        ? ge_original_stage_objective_runtime_evaluate(
            ge_live_runtime, menu, evaluation)
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_collect_status_changes(
    int32_t selected_difficulty,
    GeOriginalStageObjectiveStatusChange *changes, size_t *change_count,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation)
{
    return ge_live_runtime != NULL
        ? ge_original_stage_objective_runtime_collect_status_changes(
            ge_live_runtime, selected_difficulty, changes, change_count,
            blocked_evaluation)
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_cleanup(void)
{
    return ge_live_runtime != NULL
        ? ge_original_stage_objective_runtime_cleanup(ge_live_runtime)
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
}

int ge_original_stage_objective_live_count(void)
{
    return ge_live_runtime != NULL && ge_live_runtime->bound
            && ge_live_runtime->registry != NULL
        ? ge_live_runtime->registry->objective_count + 1 : 0;
}

int ge_original_stage_objective_live_difficulty(
    uint8_t menu, int8_t *difficulty)
{
    int32_t index;
    if (ge_live_runtime == NULL || !ge_live_runtime->bound
            || ge_live_runtime->registry == NULL || difficulty == NULL
            || menu >= GE_ORIGINAL_STAGE_OBJECTIVE_MAX)
        return 0;
    index = ge_live_runtime->registry->objective_by_menu[menu];
    if (index < 0) return 0;
    *difficulty = ge_live_runtime->registry->objectives[index].difficulty;
    return 1;
}

int ge_original_stage_objective_live_text_id(
    uint8_t menu, uint16_t *text_id)
{
    int32_t index;
    if (ge_live_runtime == NULL || !ge_live_runtime->bound
            || ge_live_runtime->registry == NULL || text_id == NULL
            || menu >= GE_ORIGINAL_STAGE_OBJECTIVE_MAX)
        return 0;
    index = ge_live_runtime->registry->objective_by_menu[menu];
    if (index < 0) return 0;
    *text_id = ge_live_runtime->registry->objectives[index].text_id;
    return 1;
}

GeOriginalStageObjectiveRuntimeStatus
ge_original_stage_objective_live_all_complete(
    int32_t selected_difficulty, int *complete,
    GeOriginalStageObjectiveEvaluation *blocked_evaluation)
{
    int menu, count;
    if (complete == NULL || blocked_evaluation == NULL)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_INVALID_ARGUMENT;
    if (ge_live_runtime == NULL || !ge_live_runtime->bound)
        return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_NOT_BOUND;
    *complete = 1;
    count = ge_original_stage_objective_live_count();
    for (menu = 0; menu < count; ++menu) {
        int8_t difficulty;
        GeOriginalStageObjectiveRuntimeStatus status;
        if (!ge_original_stage_objective_live_difficulty(
                (uint8_t)menu, &difficulty)) continue;
        if ((int32_t)difficulty > selected_difficulty) continue;
        status = ge_original_stage_objective_live_evaluate(
            (uint8_t)menu, blocked_evaluation);
        if (status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) return status;
        if (blocked_evaluation->value
                != GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE) {
            *complete = 0;
            return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
        }
    }
    return GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

void ge_original_stage_objective_live_status(
    GeOriginalStageObjectiveLiveStatus *status)
{
    if (status != NULL) *status = ge_live_status;
}

void objectivestatusCheckRoomEntered(int32_t room_id)
{
    (void)ge_original_stage_objective_live_check_room_entered(room_id);
}

void objectivestatusCheckDeposit(int32_t weapon_num, int32_t room_id)
{
    (void)ge_original_stage_objective_live_check_deposit(
        weapon_num, room_id);
}

void objectiveTakePictureHandler(void)
{
    (void)ge_original_stage_objective_live_take_picture();
}

void cleanupObjectives(void)
{
    (void)ge_original_stage_objective_live_cleanup();
}

int32_t objectiveGetCount(void)
{
    return (int32_t)ge_original_stage_objective_live_count();
}

int32_t objectiveGetStatus_WEAK(int32_t objective_num, int32_t ignored)
{
    GeOriginalStageObjectiveEvaluation evaluation;
    GeOriginalStageObjectiveRuntimeStatus status;
    (void)ignored;
    if (objective_num < 0
            || objective_num >= GE_ORIGINAL_STAGE_OBJECTIVE_MAX) {
        ge_live_status.last_status =
            GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OBJECTIVE_UNAVAILABLE;
        return GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
    }
    status = ge_original_stage_objective_live_evaluate(
        (uint8_t)objective_num, &evaluation);
    ge_live_status.last_status = status;
    ge_live_status.last_blocker = status
            == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED
        ? (GeOriginalStageObjectiveRuntimeBlocker)evaluation.blocker
        : GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_READY;
    return status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK
        ? (int32_t)evaluation.value
        : GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE;
}
