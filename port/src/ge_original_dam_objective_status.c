#include "ge_original_dam_objective_status.h"

#include <stdio.h>
#include <string.h>

extern char *LmiscE[];
extern void hudmsgBottomShow(char *message);
extern int lvlGetSelectedDifficulty(void);

static GeOriginalDamObjectiveStatusSnapshot ge_dam_last_snapshot;
static GeOriginalDamObjectiveStatusResult ge_dam_last_result =
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED;

enum {
    GE_LMISC_OBJECTIVE = 0x2c,
    GE_LMISC_COMPLETED = 0x2d,
    GE_LMISC_INCOMPLETE = 0x2e,
    GE_LMISC_FAILED = 0x2f
};

static void ge_dam_default_hud_bottom_show(
    void *context, const char *message)
{
    (void)context;
    hudmsgBottomShow((char *)message);
}

static const char *ge_dam_status_text(uint8_t status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE:
        return LmiscE[GE_LMISC_COMPLETED];
    case GE_ORIGINAL_STAGE_OBJECTIVE_INCOMPLETE:
        return LmiscE[GE_LMISC_INCOMPLETE];
    case GE_ORIGINAL_STAGE_OBJECTIVE_FAILED:
        return LmiscE[GE_LMISC_FAILED];
    default:
        return NULL;
    }
}

GeOriginalDamObjectiveStatusResult
ge_original_dam_objective_status_present(
    int32_t selected_difficulty,
    const GeOriginalDamObjectiveStatusProviders *providers,
    GeOriginalDamObjectiveStatusSnapshot *snapshot)
{
    GeOriginalStageObjectiveStatusChange changes[
        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    GeOriginalStageObjectiveEvaluation blocked;
    GeOriginalStageObjectiveRuntimeStatus runtime_status;
    void (*hud_bottom_show)(void *, const char *) =
        providers != NULL ? providers->hud_bottom_show
                          : ge_dam_default_hud_bottom_show;
    void *context = providers != NULL ? providers->context : NULL;
    size_t change_count = 0U, index;
    int mission_complete = 0;
    if (snapshot == NULL)
        return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_INVALID_ARGUMENT;
    memset(snapshot, 0, sizeof(*snapshot));
    if (hud_bottom_show == NULL)
        return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_HUD_UNAVAILABLE;
    runtime_status = ge_original_stage_objective_live_collect_status_changes(
        selected_difficulty, changes, &change_count, &blocked);
    if (runtime_status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
        snapshot->blocked_evaluation = blocked;
        return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED;
    }
    for (index = 0U; index < change_count; ++index) {
        const char *status_text = ge_dam_status_text(changes[index].value);
        char *message = snapshot->messages[snapshot->message_count];
        if (status_text == NULL) continue;
        /* Exact VERSION_US branch: "objective %c: " + canonical status. */
        (void)snprintf(message,
            GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY,
            "%s %c: %s", LmiscE[GE_LMISC_OBJECTIVE],
            (int)changes[index].available_index + 0x61, status_text);
        hud_bottom_show(context, message);
        ++snapshot->message_count;
    }
    runtime_status = ge_original_stage_objective_live_all_complete(
        selected_difficulty, &mission_complete, &blocked);
    if (runtime_status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
        snapshot->blocked_evaluation = blocked;
        return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED;
    }
    snapshot->mission_complete = mission_complete != 0;
    for (index = 0U; index < (size_t)ge_original_stage_objective_live_count();
            ++index) {
        int8_t difficulty;
        GeOriginalStageObjectiveEvaluation evaluation;
        if (!ge_original_stage_objective_live_difficulty(
                (uint8_t)index, &difficulty)
                || (int32_t)difficulty > selected_difficulty)
            continue;
        runtime_status = ge_original_stage_objective_live_evaluate(
            (uint8_t)index, &evaluation);
        if (runtime_status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
            snapshot->blocked_evaluation = evaluation;
            return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED;
        }
        if (evaluation.value == GE_ORIGINAL_STAGE_OBJECTIVE_FAILED)
            snapshot->mission_failed = 1U;
    }
    return GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK;
}

void display_objective_status_text_on_status_change(void)
{
    ge_dam_last_result = ge_original_dam_objective_status_present(
        lvlGetSelectedDifficulty(), NULL, &ge_dam_last_snapshot);
}

bool objectiveIsAllComplete(void)
{
    GeOriginalStageObjectiveEvaluation blocked;
    GeOriginalStageObjectiveRuntimeStatus status;
    int complete = 0;
    memset(&ge_dam_last_snapshot, 0, sizeof(ge_dam_last_snapshot));
    status = ge_original_stage_objective_live_all_complete(
        lvlGetSelectedDifficulty(), &complete, &blocked);
    if (status != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
        ge_dam_last_snapshot.blocked_evaluation = blocked;
        ge_dam_last_result = GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED;
        return false;
    }
    ge_dam_last_snapshot.mission_complete = complete != 0;
    ge_dam_last_result = GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK;
    return complete != 0;
}

GeOriginalDamObjectiveStatusResult
ge_original_dam_objective_status_last(
    GeOriginalDamObjectiveStatusSnapshot *snapshot)
{
    if (snapshot != NULL) *snapshot = ge_dam_last_snapshot;
    return ge_dam_last_result;
}
