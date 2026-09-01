#include "ge_original_boss.h"

#include "boss.h"

extern s32 g_MainStageNum;
extern s32 g_StageNum;
extern s32 g_ShowMemUseFlag;
extern s32 g_ShowMemBarsFlag;

void ge_original_boss_reset(void)
{
    /* Exercise the original title request before committing the boundary. */
    g_StageNum = LEVELID_NONE;
    bossRunTitleStage();
    (void)ge_original_boss_commit_requested_stage();
    g_ShowMemUseFlag = FALSE;
    g_ShowMemBarsFlag = FALSE;
}

void ge_original_boss_request_stage(int32_t stage)
{
    bossSetLoadedStage((LEVELID)stage);
}

int ge_original_boss_commit_requested_stage(void)
{
    if (g_MainStageNum == LEVELID_NONE) {
        return 0;
    }

    g_StageNum = g_MainStageNum;
    bossSetLoadedStage(LEVELID_NONE);
    return 1;
}

void ge_original_boss_snapshot(GeOriginalBossState *state)
{
    if (state == NULL) {
        return;
    }

    state->current_stage = (int32_t)bossGetStageNum();
    state->requested_stage = (int32_t)g_MainStageNum;
    state->debug_menu_open = (int32_t)bossGetDebugParseFlag();
    state->show_memory_use = (int32_t)g_ShowMemUseFlag;
    state->show_memory_bars = (int32_t)g_ShowMemBarsFlag;
}
