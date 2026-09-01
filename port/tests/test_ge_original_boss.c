#include "ge_original_boss.h"

#include <assert.h>

#include "boss.h"

int main(void)
{
    GeOriginalBossState state;

    ge_original_boss_reset();
    ge_original_boss_snapshot(&state);
    assert(state.current_stage == LEVELID_TITLE);
    assert(state.requested_stage == LEVELID_NONE);
    assert(state.debug_menu_open == 0);
    assert(state.show_memory_use == 0);
    assert(state.show_memory_bars == 0);

    ge_original_boss_request_stage(LEVELID_DAM);
    ge_original_boss_snapshot(&state);
    assert(state.current_stage == LEVELID_TITLE);
    assert(state.requested_stage == LEVELID_DAM);

    assert(ge_original_boss_commit_requested_stage() == 1);
    assert(ge_original_boss_commit_requested_stage() == 0);
    ge_original_boss_snapshot(&state);
    assert(state.current_stage == LEVELID_DAM);
    assert(state.requested_stage == LEVELID_NONE);

    bossEnableShowMemUseFlag();
    bossMemBarsFlagToggle();
    ge_original_boss_snapshot(&state);
    assert(state.show_memory_use == 1);
    assert(state.show_memory_bars == 1);
    bossMemBarsFlagToggle();
    ge_original_boss_snapshot(&state);
    assert(state.show_memory_bars == 0);

    bossRunTitleStage();
    assert(ge_original_boss_commit_requested_stage() == 1);
    ge_original_boss_snapshot(&state);
    assert(state.current_stage == LEVELID_TITLE);

    ge_original_boss_snapshot(NULL);
    return 0;
}
