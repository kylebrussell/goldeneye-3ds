#ifndef GE_ORIGINAL_MISSION_RESULT_H
#define GE_ORIGINAL_MISSION_RESULT_H

#include <stdint.h>

#define GE_ORIGINAL_MISSION_OUTCOME_OBJECTIVES 10U

typedef struct GeOriginalMissionOutcomeObjective {
    uint16_t text_id;
    uint16_t enabled_difficulty;
    int32_t status;
} GeOriginalMissionOutcomeObjective;

typedef struct GeOriginalMissionOutcomeInput {
    int32_t difficulty;
    int32_t mission_failed_or_aborted;
    int32_t bond_kia;
    GeOriginalMissionOutcomeObjective
        objectives[GE_ORIGINAL_MISSION_OUTCOME_OBJECTIVES];
} GeOriginalMissionOutcomeInput;

typedef enum GeOriginalMissionOutcomeStatus {
    GE_ORIGINAL_MISSION_OUTCOME_FAILED = 0,
    GE_ORIGINAL_MISSION_OUTCOME_COMPLETED,
    GE_ORIGINAL_MISSION_OUTCOME_ABORTED,
    GE_ORIGINAL_MISSION_OUTCOME_KIA
} GeOriginalMissionOutcomeStatus;

typedef struct GeOriginalMissionOutcome {
    GeOriginalMissionOutcomeStatus status;
    uint8_t all_objectives_complete_alive;
    uint8_t enabled_objective_count;
    uint8_t incomplete_objective_count;
} GeOriginalMissionOutcome;

typedef struct GeOriginalMissionResultProviders {
    void *context;
    void (*unlock_stage)(void *context, int32_t folder, int32_t mission,
                         int32_t difficulty, int32_t time_seconds);
    void *(*save_for_folder)(void *context, int32_t folder);
    int (*is_cheat_unlocked)(void *context, void *save, int32_t mission);
    void (*unlock_cheat)(void *context, int32_t folder, int32_t mission);
} GeOriginalMissionResultProviders;

typedef struct GeOriginalMissionResultSnapshot {
    uint32_t apply_calls;
    uint32_t completion_mutations;
    uint32_t cheat_mutations;
    uint32_t persistence_frontiers;
    int32_t folder;
    int32_t mission;
    int32_t difficulty;
    int32_t mission_time_seconds;
    int32_t cheat_target_seconds;
    int32_t new_cheat_unlocked;
} GeOriginalMissionResultSnapshot;

void ge_original_mission_result_reset(void);
void ge_original_mission_result_bind(
    const GeOriginalMissionResultProviders *providers);

/* Publishes the frontend's canonical mission-part selection, then runs the
 * unchanged end_of_mission_briefing body with the live difficulty/timer. */
int ge_original_mission_result_set_current_mission(int32_t mission);
int ge_original_mission_result_apply_exact(int32_t difficulty,
                                            int32_t mission_ticks);
void ge_original_mission_result_snapshot(
    GeOriginalMissionResultSnapshot *snapshot);

/* Exact frontCompleteAllObjectivesAliveSuccess/report-status decision. The
 * caller publishes the already-evaluated canonical objective rows; this body
 * owns difficulty filtering and abort/death precedence. */
int ge_original_mission_outcome_evaluate_exact(
    const GeOriginalMissionOutcomeInput *input,
    GeOriginalMissionOutcome *outcome);

#endif
