#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bondtypes.h"
#include "chrai.h"
#include "ge_original_dam_mission_flow.h"
#include "ge_original_dam_mission_stage_storage.h"
#include "ge_original_dam_setup.h"
#include "memp.h"

stagesetup g_CurrentSetup;
extern ChrRecord *g_ActiveChrs;
extern int32_t g_ActiveChrsCount;

extern AIListRecord ailists[];
extern AIRecord ai_20[];
extern int32_t chraiGetAIListID(AIRecord *list, bool *is_global);
extern int32_t chraiGoToLabel(AIRecord *list, int32_t offset,
                              uint8_t label);
extern void chrSetStageFlags(ChrRecord *self, int32_t flags);
extern int32_t objectiveregisters1;

int main(void)
{
    GeOriginalDamMissionFlowState state;
    AIRecord *resolved;
    uint8_t *first;
    uint8_t *second;
    size_t used_before_ai;
    bool is_global = TRUE;

    ge_original_dam_stage_storage_reset();
    first = mempAllocBytesInBank(31U, MEMPOOL_STAGE);
    second = mempAllocBytesInBank(33U, MEMPOOL_STAGE);
    assert(first != NULL && second != NULL);
    assert(((uintptr_t)first & 15U) == 0U);
    assert(((uintptr_t)second & 15U) == 0U);
    assert(first + 31U <= second);
    memset(first, 0xa5, 31U);
    memset(second, 0x5a, 33U);
    used_before_ai = ge_original_dam_stage_storage_used();
    assert(used_before_ai >= 64U);
    assert(ge_original_dam_stage_storage_capacity() > used_before_ai);

    memset(&g_CurrentSetup, 0, sizeof(g_CurrentSetup));
    assert(!ge_original_dam_mission_flow_begin(NULL));
    assert(ge_original_dam_mission_flow_begin(&state));
    assert(state.stage_list_id == 0x1000);
    assert(state.list_resolved);
    assert(state.first_yield_complete);
    assert(state.ai_offset == 3);
    assert(state.objective_prop_frontier);
    assert(state.background_actor_count == 8);
    assert(state.background_actor_index == 0);
    assert(g_CurrentSetup.ailists == ailists);
    assert(g_ActiveChrs != NULL);
    assert((uint8_t *)g_ActiveChrs >= second + 33U
           || (uint8_t *)g_ActiveChrs
               + (size_t)g_ActiveChrsCount * sizeof(*g_ActiveChrs) <= first);
    assert(g_ActiveChrsCount == 8);
    assert(g_ActiveChrs[0].chrnum == 0xfe);
    assert(g_ActiveChrs[0].actiontype == ACT_NULL);
    assert(g_ActiveChrs[0].aireturnlist == -1);
    assert(g_ActiveChrs[0].aioffset == 3);
    assert(g_ActiveChrs[7].ailist == ailists[27].ailist);
    assert(g_ActiveChrs[7].aioffset == 0);
    assert(first[0] == 0xa5U && first[30] == 0xa5U);
    assert(second[0] == 0x5aU && second[32] == 0x5aU);

    resolved = ailistFindById(0x1000);
    assert(resolved == ai_20);
    assert(chraiGetAIListID(resolved, &is_global) == 0x1000);
    assert(!is_global);
    assert(chraiGoToLabel(resolved, 0, 0x2a) == 0);

    /* Authored bytes: label 0x2a, yield, then objective-bitfield test. */
    assert(resolved[0].cmd == AI_Label);
    assert(resolved[1].cmd == 0x2a);
    assert(resolved[2].cmd == AI_Yield);
    assert(resolved[3].cmd == AI_IFObjectiveBitfieldHas);

    /* The exact objective register accessor gates the exact authored branch. */
    assert(!ge_original_dam_mission_flow_advance_objective_complete(&state));
    chrSetStageFlags(&g_ActiveChrs[0], 0x00000400);
    assert(objectiveregisters1 == 0x00000400);
    assert(ge_original_dam_mission_flow_advance_objective_complete(&state));
    assert(state.objective_complete_observed);
    assert(state.terminal_loop_yield_complete);
    assert(state.ai_offset == chraiGoToLabel(resolved, 0, 0x04) + 3);

    /* A stage reset reuses the arena start, never an already-live address. */
    ge_original_dam_stage_storage_reset();
    assert(ge_original_dam_stage_storage_used() == 0U);
    assert(mempAllocBytesInBank(31U, MEMPOOL_STAGE) == first);

    printf("original Dam mission flow: ok (list 0x1000, first yield 3, "
           "objective-complete yield %u)\n", state.ai_offset);
    return 0;
}
