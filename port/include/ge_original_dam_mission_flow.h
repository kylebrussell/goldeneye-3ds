#ifndef GE_ORIGINAL_DAM_MISSION_FLOW_H
#define GE_ORIGINAL_DAM_MISSION_FLOW_H

#include <stdint.h>

typedef struct GeOriginalDamMissionFlowState {
    uint32_t stage_list_id;
    uint32_t ticks;
    uint32_t yield_transitions;
    uint32_t objective_registers;
    uint16_t ai_offset;
    uint16_t previous_ai_offset;
    uint8_t list_resolved;
    uint8_t first_yield_complete;
    uint8_t objective_prop_frontier;
    uint8_t background_actor_count;
    uint8_t background_actor_index;
    uint8_t objective_complete_observed;
    uint8_t terminal_loop_yield_complete;
    uint8_t hud_message_count;
} GeOriginalDamMissionFlowState;

/*
 * Allocates all eight authored Dam background actors through the original
 * stage-AI allocator, then starts list 0x1000 through the original chrai
 * interpreter. The linked tranche ends at its first AI_Yield; objective and
 * tagged-object commands are deliberately not executed by this entry point.
 */
int ge_original_dam_mission_flow_begin(
    GeOriginalDamMissionFlowState *state);

/*
 * Resumes the same original background actor once Dam's authored 0x00040000
 * command value (runtime objective bit 0x00000400) has been set. The original
 * interpreter takes ai_20's label 0x04 branch and yields in its terminal
 * mission loop.
 */
int ge_original_dam_mission_flow_advance_objective_complete(
    GeOriginalDamMissionFlowState *state);

/* Runs one authored stage-actor scheduling quantum. This is the same original
 * ai body used by the level scheduler: it resumes at the actor's published
 * offset and returns only at the next AI_Yield/AI_EndList. */
int ge_original_dam_mission_flow_tick(
    GeOriginalDamMissionFlowState *state);

#endif
