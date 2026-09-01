#include "ge_original_mission_result.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>

s32 selected_folder_num = FOLDER1;

typedef struct TestPersistence {
    int unlock_stage_calls;
    int unlock_cheat_calls;
    int cheat_unlocked;
    int folder;
    int mission;
    int difficulty;
    int seconds;
} TestPersistence;

static void unlock_stage(void *context, int32_t folder, int32_t mission,
                         int32_t difficulty, int32_t seconds)
{
    TestPersistence *state = context;
    ++state->unlock_stage_calls;
    state->folder = folder;
    state->mission = mission;
    state->difficulty = difficulty;
    state->seconds = seconds;
}

static void *save_for_folder(void *context, int32_t folder)
{
    TestPersistence *state = context;
    state->folder = folder;
    return state;
}

static int is_cheat_unlocked(void *context, void *save, int32_t mission)
{
    TestPersistence *state = context;
    assert(save == state);
    state->mission = mission;
    return state->cheat_unlocked;
}

static void unlock_cheat(void *context, int32_t folder, int32_t mission)
{
    TestPersistence *state = context;
    ++state->unlock_cheat_calls;
    state->folder = folder;
    state->mission = mission;
    state->cheat_unlocked = 1;
}

int main(void)
{
    TestPersistence persistence;
    GeOriginalMissionResultProviders providers;
    GeOriginalMissionResultSnapshot snapshot;
    GeOriginalMissionOutcomeInput outcome_input;
    GeOriginalMissionOutcome outcome;

    memset(&outcome_input, 0, sizeof(outcome_input));
    outcome_input.difficulty = DIFFICULTY_AGENT;
    outcome_input.objectives[0].text_id = 100U;
    outcome_input.objectives[0].enabled_difficulty = DIFFICULTY_AGENT;
    outcome_input.objectives[0].status = OBJECTIVESTATUS_COMPLETE;
    outcome_input.objectives[1].text_id = 101U;
    outcome_input.objectives[1].enabled_difficulty = DIFFICULTY_SECRET;
    outcome_input.objectives[1].status = OBJECTIVESTATUS_FAILED;
    assert(ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome));
    assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_COMPLETED
        && outcome.all_objectives_complete_alive
        && outcome.enabled_objective_count == 1U
        && outcome.incomplete_objective_count == 0U);
    outcome_input.difficulty = DIFFICULTY_SECRET;
    assert(ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome));
    assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_FAILED
        && !outcome.all_objectives_complete_alive
        && outcome.enabled_objective_count == 2U
        && outcome.incomplete_objective_count == 1U);
    outcome_input.objectives[1].status = OBJECTIVESTATUS_COMPLETE;
    outcome_input.mission_failed_or_aborted = TRUE;
    assert(ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome));
    assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_ABORTED
        && !outcome.all_objectives_complete_alive
        && outcome.incomplete_objective_count == 0U);
    outcome_input.bond_kia = TRUE;
    assert(ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome));
    assert(outcome.status == GE_ORIGINAL_MISSION_OUTCOME_KIA
        && !outcome.all_objectives_complete_alive);
    outcome_input.bond_kia = FALSE;
    outcome_input.mission_failed_or_aborted = FALSE;
    outcome_input.difficulty = DIFFICULTY_007;
    assert(ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome)
        && outcome.status == GE_ORIGINAL_MISSION_OUTCOME_COMPLETED);
    outcome_input.difficulty = DIFFICULTY_007 + 1;
    assert(!ge_original_mission_outcome_evaluate_exact(
        &outcome_input, &outcome));

    memset(&persistence, 0, sizeof(persistence));
    providers = (GeOriginalMissionResultProviders){
        &persistence, unlock_stage, save_for_folder,
        is_cheat_unlocked, unlock_cheat,
    };
    ge_original_mission_result_reset();
    assert(!ge_original_mission_result_apply_exact(
        DIFFICULTY_AGENT, 60 * 90));
    ge_original_mission_result_snapshot(&snapshot);
    assert(snapshot.persistence_frontiers == 1U);
    assert(snapshot.completion_mutations == 0U);

    ge_original_mission_result_bind(&providers);
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_SECRET, 60 * 100));
    ge_original_mission_result_snapshot(&snapshot);
    assert(persistence.unlock_stage_calls == 1);
    assert(persistence.unlock_cheat_calls == 1);
    assert(persistence.folder == FOLDER1);
    assert(persistence.mission == SP_LEVEL_DAM);
    assert(persistence.difficulty == DIFFICULTY_SECRET);
    assert(persistence.seconds == 100);
    assert(snapshot.completion_mutations == 1U);
    assert(snapshot.cheat_mutations == 1U);
    assert(snapshot.new_cheat_unlocked == TRUE);

    persistence.cheat_unlocked = 1;
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_SECRET, 60 * 90));
    ge_original_mission_result_snapshot(&snapshot);
    assert(persistence.unlock_stage_calls == 2);
    assert(persistence.unlock_cheat_calls == 1);
    assert(snapshot.new_cheat_unlocked == FALSE);

    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_007, 60 * 80));
    assert(persistence.unlock_stage_calls == 2);

    /* The same unchanged body must persist the mission selected by the
     * frontend, not silently write Dam again after a stage transition. */
    persistence.cheat_unlocked = 0;
    assert(ge_original_mission_result_set_current_mission(
        SP_LEVEL_FACILITY));
    assert(ge_original_mission_result_apply_exact(
        DIFFICULTY_00, 60 * 120));
    ge_original_mission_result_snapshot(&snapshot);
    assert(persistence.unlock_stage_calls == 3);
    assert(persistence.unlock_cheat_calls == 2);
    assert(persistence.mission == SP_LEVEL_FACILITY);
    assert(persistence.difficulty == DIFFICULTY_00);
    assert(persistence.seconds == 120);
    assert(snapshot.mission == SP_LEVEL_FACILITY);
    assert(snapshot.cheat_target_seconds == 125);
    {
        int32_t mission;
        for(mission=SP_LEVEL_DAM;mission<SP_LEVEL_MAX;++mission){
            assert(ge_original_mission_result_set_current_mission(mission));
            ge_original_mission_result_snapshot(&snapshot);
            assert(snapshot.mission==mission);
        }
    }
    assert(!ge_original_mission_result_set_current_mission(SP_LEVEL_MAX));

    puts("Campaign mission result: exact completion/time/cheat provider boundary retained");
    return 0;
}
