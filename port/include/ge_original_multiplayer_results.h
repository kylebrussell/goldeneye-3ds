#ifndef GE_ORIGINAL_MULTIPLAYER_RESULTS_H
#define GE_ORIGINAL_MULTIPLAYER_RESULTS_H
#include "ge_original_multiplayer_frontend.h"
#include <stdint.h>

typedef enum GeOriginalMultiplayerResultsStatus {
    GE_ORIGINAL_MP_RESULTS_OK = 0,
    GE_ORIGINAL_MP_RESULTS_INVALID_ARGUMENT,
    GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER,
    GE_ORIGINAL_MP_RESULTS_INVALID_STATS,
    GE_ORIGINAL_MP_RESULTS_SCORE_FAILED
} GeOriginalMultiplayerResultsStatus;

/* Read only. Invoke when the root's canonical end decision completes, or when
 * root explicitly aborts. The cause is provided by that owner, not inferred
 * from scores. Tied first ranks and winning teammates share winner bits;
 * aborted sessions publish no winner. Cosmetic awards remain their original
 * separate lifecycle and do not run or consume RNG in this snapshot. */
GeOriginalMultiplayerResultsStatus ge_original_multiplayer_results_snapshot(
    uint64_t request_id, GeOriginalMultiplayerFrontendEndReason end_reason,
    GeOriginalMultiplayerFrontendResult *result);
#endif
