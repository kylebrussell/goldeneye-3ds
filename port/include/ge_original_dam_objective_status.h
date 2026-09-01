#ifndef GE_ORIGINAL_DAM_OBJECTIVE_STATUS_H
#define GE_ORIGINAL_DAM_OBJECTIVE_STATUS_H

#include "ge_original_stage_objective_live.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum {
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY = 50
};

typedef enum GeOriginalDamObjectiveStatusResult {
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_OK = 0,
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_INVALID_ARGUMENT,
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_HUD_UNAVAILABLE,
    GE_ORIGINAL_DAM_OBJECTIVE_STATUS_RUNTIME_BLOCKED
} GeOriginalDamObjectiveStatusResult;

typedef struct GeOriginalDamObjectiveStatusProviders {
    void *context;
    void (*hud_bottom_show)(void *context, const char *message);
} GeOriginalDamObjectiveStatusProviders;

typedef struct GeOriginalDamObjectiveStatusSnapshot {
    GeOriginalStageObjectiveEvaluation blocked_evaluation;
    size_t message_count;
    char messages[GE_ORIGINAL_STAGE_OBJECTIVE_MAX]
                 [GE_ORIGINAL_DAM_OBJECTIVE_STATUS_MESSAGE_CAPACITY];
    uint8_t mission_complete;
    uint8_t mission_failed;
} GeOriginalDamObjectiveStatusSnapshot;

/* Dam's VERSION_US display_objective_status_text_on_status_change consumer.
 * Evaluation, difficulty ordering and cache commits remain in the generic
 * exact objective runtime. This boundary supplies only the original LMISC
 * strings/format and bottom-HUD enqueue. A blocker consumes no transition. */
GeOriginalDamObjectiveStatusResult
ge_original_dam_objective_status_present(
    int32_t selected_difficulty,
    const GeOriginalDamObjectiveStatusProviders *providers,
    GeOriginalDamObjectiveStatusSnapshot *snapshot);

/* Canonical live call-site adapter used by bondviewProcessInput. The last
 * blocker/result remains queryable because the original void ABI cannot
 * return the port's explicit dependency failure. */
void display_objective_status_text_on_status_change(void);
bool objectiveIsAllComplete(void);
GeOriginalDamObjectiveStatusResult
ge_original_dam_objective_status_last(
    GeOriginalDamObjectiveStatusSnapshot *snapshot);

#endif
