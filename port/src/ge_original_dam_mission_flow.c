#include "ge_original_dam_mission_flow.h"

#include <string.h>

#include "bondtypes.h"
#include "chrai.h"
#include "ge_original_dam_mission_hud.h"
#include "ge_original_dam_setup.h"

extern stagesetup g_CurrentSetup;
extern ChrRecord *g_ActiveChrs;
extern s32 g_ActiveChrsCount;
extern s32 objectiveregisters1;
extern void alloc_false_GUARDdata_to_exec_global_action(void);
extern void ai(PropDefHeaderRecord *entity, PROP_TYPE entity_type);
extern bool chrHasStageFlag(ChrRecord *self, s32 flags);
extern s32 chraiGoToLabel(AIRecord *list, s32 offset, u8 label);

enum {
    DAM_PRIMARY_STAGE_AI_LIST = 0x1000,
    DAM_PRIMARY_STAGE_FIRST_YIELD_OFFSET = 3
};

int ge_original_dam_mission_flow_begin(
    GeOriginalDamMissionFlowState *state)
{
    stagesetup *setup;
    ChrRecord *stage_entity = NULL;
    AIRecord *list;
    s32 index;

    if (state == NULL) {
        return 0;
    }

    memset(state, 0, sizeof(*state));
    state->stage_list_id = DAM_PRIMARY_STAGE_AI_LIST;
    setup = ge_original_dam_setup_get();
    if (setup == NULL || setup->ailists == NULL) {
        return 0;
    }

    /* The packaged stage loader may already have published its independently
     * relocated but byte-identical Dam AI table. Preserve that complete
     * setup publication so object/active-prop identity remains canonical;
     * standalone slice tests still enter through the linked setup table. */
    if (g_CurrentSetup.ailists == NULL)
        g_CurrentSetup.ailists = setup->ailists;
    list = ailistFindById(DAM_PRIMARY_STAGE_AI_LIST);
    if (list == NULL) {
        return 0;
    }
    state->list_resolved = 1;

    alloc_false_GUARDdata_to_exec_global_action();
    state->background_actor_count = (uint8_t)g_ActiveChrsCount;
    for (index = 0; index < g_ActiveChrsCount; index++) {
        if (g_ActiveChrs[index].ailist == list) {
            stage_entity = &g_ActiveChrs[index];
            state->background_actor_index = (uint8_t)index;
            break;
        }
    }
    if (stage_entity == NULL) {
        return 0;
    }
    ai((PropDefHeaderRecord *)stage_entity, PROP_TYPE_CHR);

    state->ai_offset = stage_entity->aioffset;
    state->first_yield_complete =
        stage_entity->ailist == list
        && stage_entity->aioffset == DAM_PRIMARY_STAGE_FIRST_YIELD_OFFSET;
    state->objective_prop_frontier = state->first_yield_complete;
    return state->first_yield_complete;
}

int ge_original_dam_mission_flow_advance_objective_complete(
    GeOriginalDamMissionFlowState *state)
{
    ChrRecord *stage_entity;

    if (state == NULL || !state->first_yield_complete
            || state->background_actor_index >= g_ActiveChrsCount) {
        return 0;
    }
    stage_entity = &g_ActiveChrs[state->background_actor_index];
    /* ai_20's authored 0x00040000 argument is serialized with the setup
     * macros as bytes 00 00 04 00; the canonical interpreter reads 0x400. */
    if (!chrHasStageFlag(stage_entity, 0x00000400)) {
        return 0;
    }

    state->objective_complete_observed = 1;
    return ge_original_dam_mission_flow_tick(state)
        && state->terminal_loop_yield_complete;
}

int ge_original_dam_mission_flow_tick(
    GeOriginalDamMissionFlowState *state)
{
    ChrRecord *stage_entity;
    AIRecord *primary_list;
    uint16_t previous_offset;

    if (state == NULL || !state->first_yield_complete
            || state->background_actor_index >= g_ActiveChrsCount) {
        return 0;
    }
    primary_list = ailistFindById(DAM_PRIMARY_STAGE_AI_LIST);
    stage_entity = &g_ActiveChrs[state->background_actor_index];
    if (primary_list == NULL || stage_entity->ailist != primary_list) return 0;

    previous_offset = stage_entity->aioffset;
    ai((PropDefHeaderRecord *)stage_entity, PROP_TYPE_CHR);
    state->ticks++;
    state->previous_ai_offset = previous_offset;
    state->ai_offset = stage_entity->aioffset;
    state->objective_registers = (uint32_t)objectiveregisters1;
    state->hud_message_count =
        (uint8_t)ge_original_dam_mission_hud_count();
    if (state->ai_offset != previous_offset) state->yield_transitions++;
    if (chrHasStageFlag(stage_entity, 0x00000400))
        state->objective_complete_observed = 1;
    state->terminal_loop_yield_complete =
        stage_entity->ailist == primary_list
        && stage_entity->aioffset
            == chraiGoToLabel(primary_list, 0, 0x04)
                + 3; /* authored label (2 bytes) followed by yield (1 byte) */
    return 1;
}
