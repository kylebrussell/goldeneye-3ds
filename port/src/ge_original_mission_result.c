#include "ge_original_mission_result.h"

#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>

typedef struct GeMissionResultEntry {
    int32_t mission_num;
} GeMissionResultEntry;

typedef struct GeMissionOutcomeBriefingObjective {
    u16 textid;
    u16 enabled_difficulty;
} GeMissionOutcomeBriefingObjective;

typedef struct GeMissionOutcomeBriefing {
    u16 brief[4];
    GeMissionOutcomeBriefingObjective objective[OBJECTIVES_MAX];
} GeMissionOutcomeBriefing;

_Static_assert(OBJECTIVES_MAX==GE_ORIGINAL_MISSION_OUTCOME_OBJECTIVES,
    "frontend mission outcome objective ABI drift");

extern s32 selected_folder_num;

static GeOriginalMissionResultProviders ge_result_providers;
static GeOriginalMissionResultSnapshot ge_result_snapshot;
static s32 ge_result_briefingpage;
static DIFFICULTY ge_result_selected_difficulty;
static s32 ge_result_append_cheat_single_player;
static s32 ge_result_new_cheat_unlocked;
static s32 ge_result_mission_ticks;
static s32 ge_outcome_mission_failed_or_aborted;
static s32 ge_outcome_bond_kia;
static DIFFICULTY ge_outcome_difficulty;
static GeMissionOutcomeBriefing ge_outcome_briefing;
static s32 ge_outcome_objective_status[OBJECTIVES_MAX];

static DIFFICULTY ge_outcome_get_difficulty(void)
{
    return ge_outcome_difficulty;
}

static s32 ge_outcome_get_objective_status(s32 objective)
{
    return objective>=0&&objective<OBJECTIVES_MAX
        ?ge_outcome_objective_status[objective]:OBJECTIVESTATUS_FAILED;
}

#define mission_failed_or_aborted ge_outcome_mission_failed_or_aborted
#define g_isBondKIA ge_outcome_bond_kia
#define ptrbriefingdata (&ge_outcome_briefing)
#define lvlGetSelectedDifficulty ge_outcome_get_difficulty
#define get_status_of_objective ge_outcome_get_objective_status

/* Exact front.c body. */
s32 ge_original_front_complete_all_objectives_alive_success_exact(void)
{
    s32 i;

    if (mission_failed_or_aborted || g_isBondKIA)
    {
        return 0;
    }

    for (i=0; i<10; i++)
    {
        if (ptrbriefingdata->objective[i].textid != 0
            && lvlGetSelectedDifficulty() >= ptrbriefingdata->objective[i].enabled_difficulty
            && get_status_of_objective(i) != OBJECTIVESTATUS_COMPLETE)
            {
                return 0;
            }
    }

    return 1;
}

#undef get_status_of_objective
#undef lvlGetSelectedDifficulty
#undef ptrbriefingdata
#undef g_isBondKIA
#undef mission_failed_or_aborted

/* Mission-part order and cheat target times copied directly from the
 * decompiled mission_folder_setup_entries and solo_target_time_array.  The
 * compact table has no chapter-header rows, so its index is the canonical
 * LEVEL_SOLO_SEQUENCE value published by the frontend. */
static GeMissionResultEntry ge_result_mission_entries[] = {
    { SP_LEVEL_DAM },
    { SP_LEVEL_FACILITY },
    { SP_LEVEL_RUNWAY },
    { SP_LEVEL_SURFACE1 },
    { SP_LEVEL_BUNKER1 },
    { SP_LEVEL_SILO },
    { SP_LEVEL_FRIGATE },
    { SP_LEVEL_SURFACE2 },
    { SP_LEVEL_BUNKER2 },
    { SP_LEVEL_STATUE },
    { SP_LEVEL_ARCHIVES },
    { SP_LEVEL_STREETS },
    { SP_LEVEL_DEPOT },
    { SP_LEVEL_TRAIN },
    { SP_LEVEL_JUNGLE },
    { SP_LEVEL_CONTROL },
    { SP_LEVEL_CAVERNS },
    { SP_LEVEL_CRADLE },
    { SP_LEVEL_AZTEC },
    { SP_LEVEL_EGYPT },
};
static s16 ge_result_solo_target_times[SP_LEVEL_MAX][3] = {
    { 0, 160, 0 },
    { 0, 0, 125 },
    { 300, 0, 0 },
    { 0, 210, 0 },
    { 0, 0, 240 },
    { 180, 0, 0 },
    { 0, 270, 0 },
    { 0, 0, 255 },
    { 90, 0, 0 },
    { 0, 195, 0 },
    { 0, 0, 80 },
    { 105, 0, 0 },
    { 0, 100, 0 },
    { 0, 0, 325 },
    { 225, 0, 0 },
    { 0, 600, 0 },
    { 0, 0, 570 },
    { 135, 0, 0 },
    { 0, 540, 0 },
    { 0, 0, 360 },
};

static s32 ge_result_get_mission_timer(void)
{
    return ge_result_mission_ticks;
}

static void ge_result_unlock_stage(s32 folder, s32 mission,
                                   DIFFICULTY difficulty, s32 seconds)
{
    ge_result_snapshot.folder = folder;
    ge_result_snapshot.mission = mission;
    ge_result_snapshot.difficulty = difficulty;
    ge_result_snapshot.mission_time_seconds = seconds;
    ge_result_providers.unlock_stage(
        ge_result_providers.context, folder, mission, difficulty, seconds);
    ++ge_result_snapshot.completion_mutations;
}

static void *ge_result_save_for_folder(s32 folder)
{
    return ge_result_providers.save_for_folder(
        ge_result_providers.context, folder);
}

static _Bool ge_result_is_cheat_unlocked(void *save, s32 mission)
{
    return ge_result_providers.is_cheat_unlocked(
        ge_result_providers.context, save, mission) != 0;
}

static void ge_result_unlock_cheat(s32 folder, s32 mission)
{
    ge_result_providers.unlock_cheat(
        ge_result_providers.context, folder, mission);
    ++ge_result_snapshot.cheat_mutations;
}

#define briefingpage ge_result_briefingpage
#define selected_difficulty ge_result_selected_difficulty
#define g_AppendCheatSinglePlayer ge_result_append_cheat_single_player
#define g_NewCheatUnlocked ge_result_new_cheat_unlocked
#define mission_folder_setup_entries ge_result_mission_entries
#define solo_target_time_array ge_result_solo_target_times
#define getMissiontimer ge_result_get_mission_timer
#define fileUnlockStageInFolderAtDifficulty ge_result_unlock_stage
#define fileGetSaveForFoldernum ge_result_save_for_folder
#define fileGetIsCheatUnlocked ge_result_is_cheat_unlocked
#define fileSaveFolderUnlockCheat ge_result_unlock_cheat

/* Exact file.c body. */
void end_of_mission_briefing(void)
{
    s16 var1;

    if ((-1 < briefingpage) && selected_difficulty != DIFFICULTY_007 && g_AppendCheatSinglePlayer == FALSE)
    {
        var1 = solo_target_time_array[mission_folder_setup_entries[briefingpage].mission_num][selected_difficulty];
        fileUnlockStageInFolderAtDifficulty(selected_folder_num, mission_folder_setup_entries[briefingpage].mission_num, selected_difficulty, getMissiontimer() / 0x3c);
        if ((getMissiontimer() / GAME_TICKRATE) <= var1)
        {
            if (!fileGetIsCheatUnlocked(fileGetSaveForFoldernum(selected_folder_num), mission_folder_setup_entries[briefingpage].mission_num))
            {
                fileSaveFolderUnlockCheat(selected_folder_num, mission_folder_setup_entries[briefingpage].mission_num);
                g_NewCheatUnlocked = TRUE;
                return;
            }
        }
#ifdef VERSION_US
        g_NewCheatUnlocked = FALSE;
#endif
    }
}

#undef fileSaveFolderUnlockCheat
#undef fileGetIsCheatUnlocked
#undef fileGetSaveForFoldernum
#undef fileUnlockStageInFolderAtDifficulty
#undef getMissiontimer
#undef solo_target_time_array
#undef mission_folder_setup_entries
#undef g_NewCheatUnlocked
#undef g_AppendCheatSinglePlayer
#undef selected_difficulty
#undef briefingpage

void ge_original_mission_result_reset(void)
{
    memset(&ge_result_providers, 0, sizeof(ge_result_providers));
    memset(&ge_result_snapshot, 0, sizeof(ge_result_snapshot));
    ge_result_briefingpage = 0;
    ge_result_selected_difficulty = DIFFICULTY_AGENT;
    ge_result_append_cheat_single_player = FALSE;
    ge_result_new_cheat_unlocked = FALSE;
    ge_result_mission_ticks = 0;
    ge_result_snapshot.folder = selected_folder_num;
    ge_result_snapshot.mission = SP_LEVEL_DAM;
    ge_result_snapshot.cheat_target_seconds =
        ge_result_solo_target_times[SP_LEVEL_DAM][DIFFICULTY_AGENT];
}

void ge_original_mission_result_bind(
    const GeOriginalMissionResultProviders *providers)
{
    if (providers == NULL) {
        memset(&ge_result_providers, 0, sizeof(ge_result_providers));
        return;
    }
    ge_result_providers = *providers;
}

int ge_original_mission_result_set_current_mission(int32_t mission)
{
    if (mission < SP_LEVEL_DAM || mission >= SP_LEVEL_MAX) return 0;
    ge_result_briefingpage = mission;
    ge_result_snapshot.mission = mission;
    ge_result_snapshot.cheat_target_seconds =
        ge_result_solo_target_times[mission][DIFFICULTY_AGENT];
    return 1;
}

int ge_original_mission_result_apply_exact(int32_t difficulty,
                                            int32_t mission_ticks)
{
    const int32_t mission =
        ge_result_mission_entries[ge_result_briefingpage].mission_num;
    ++ge_result_snapshot.apply_calls;
    ge_result_snapshot.folder = selected_folder_num;
    ge_result_snapshot.mission = mission;
    ge_result_snapshot.difficulty = difficulty;
    ge_result_snapshot.mission_time_seconds = mission_ticks / 0x3c;
    ge_result_snapshot.cheat_target_seconds =
        difficulty >= DIFFICULTY_AGENT && difficulty <= DIFFICULTY_00
            ? ge_result_solo_target_times[mission][difficulty] : 0;
    if (ge_result_providers.unlock_stage == NULL
            || ge_result_providers.save_for_folder == NULL
            || ge_result_providers.is_cheat_unlocked == NULL
            || ge_result_providers.unlock_cheat == NULL) {
        ++ge_result_snapshot.persistence_frontiers;
        return 0;
    }
    ge_result_selected_difficulty = (DIFFICULTY)difficulty;
    ge_result_mission_ticks = mission_ticks;
    end_of_mission_briefing();
    ge_result_snapshot.new_cheat_unlocked = ge_result_new_cheat_unlocked;
    return 1;
}

void ge_original_mission_result_snapshot(
    GeOriginalMissionResultSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    *snapshot = ge_result_snapshot;
}

int ge_original_mission_outcome_evaluate_exact(
    const GeOriginalMissionOutcomeInput *input,
    GeOriginalMissionOutcome *outcome)
{
    size_t index;
    int complete;
    if(input==NULL||outcome==NULL
            ||input->difficulty<DIFFICULTY_AGENT
            ||input->difficulty>DIFFICULTY_007)return 0;
    memset(outcome,0,sizeof(*outcome));
    memset(&ge_outcome_briefing,0,sizeof(ge_outcome_briefing));
    ge_outcome_mission_failed_or_aborted=input->mission_failed_or_aborted;
    ge_outcome_bond_kia=input->bond_kia;
    ge_outcome_difficulty=(DIFFICULTY)input->difficulty;
    for(index=0U;index<OBJECTIVES_MAX;++index){
        ge_outcome_briefing.objective[index].textid=
            input->objectives[index].text_id;
        ge_outcome_briefing.objective[index].enabled_difficulty=
            input->objectives[index].enabled_difficulty;
        ge_outcome_objective_status[index]=input->objectives[index].status;
        if(input->objectives[index].text_id!=0U
                &&input->difficulty
                    >=input->objectives[index].enabled_difficulty){
            ++outcome->enabled_objective_count;
            if(input->objectives[index].status!=OBJECTIVESTATUS_COMPLETE)
                ++outcome->incomplete_objective_count;
        }
    }
    complete=ge_original_front_complete_all_objectives_alive_success_exact();
    outcome->all_objectives_complete_alive=(uint8_t)(complete!=0);
    if(input->bond_kia)outcome->status=GE_ORIGINAL_MISSION_OUTCOME_KIA;
    else if(input->mission_failed_or_aborted)
        outcome->status=GE_ORIGINAL_MISSION_OUTCOME_ABORTED;
    else if(complete)outcome->status=GE_ORIGINAL_MISSION_OUTCOME_COMPLETED;
    else outcome->status=GE_ORIGINAL_MISSION_OUTCOME_FAILED;
    return 1;
}
