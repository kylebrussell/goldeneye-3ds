#ifndef GE_ORIGINAL_DAM_GUARD_AI_TICK_H
#define GE_ORIGINAL_DAM_GUARD_AI_TICK_H

#include <bondtypes.h>

extern u8 ge_original_dam_guard_ai_040d[];
extern u8 ge_original_dam_guard_ai_0413[];
extern u8 ge_original_dam_guard_ai_0414[];

/* Audited canonical command cases. The live Dam combat-list tranche includes
 * authentic near-miss/injury/death alert checks, random selection and the
 * standard guard list's leading death-state gate and armed-guard attack
 * transition. Unknown commands still stop at their current
 * offset so a partially closed list can never silently invent behaviour. */
void ge_original_dam_guard_ai_interpret_exact(
    PropDefHeaderRecord *entity, PROP_TYPE entity_type);
void ge_original_dam_guard_ai_dispatch_exact(
    PropDefHeaderRecord *entity, PROP_TYPE entity_type);

/* Unchanged chrlvActionTick body. Real guards use the retained combat
 * interpreter, while canonical chrnum 0xfe background actors use the original
 * mission-flow ai body already linked from chrai.c. */
void ge_original_dam_guard_action_tick_exact(ChrRecord *self);

/* Unchanged action handlers reached by the three retained Dam setup lists.
 * They remain deliberately separate from the live all-actions dispatcher
 * until ACT_DIE/modelTickAnim and the remaining action frontier are closed. */
void ge_original_dam_guard_tick_stand_exact(ChrRecord *self);
void ge_original_dam_guard_tick_anim_exact(ChrRecord *self);
void ge_original_dam_guard_tick_kneel_exact(ChrRecord *self);
void ge_original_dam_guard_tick_die_exact(ChrRecord *self);
void ge_original_dam_guard_tick_argh_exact(ChrRecord *self);
void ge_original_dam_guard_tick_preargh_exact(ChrRecord *self);
void ge_original_dam_guard_tick_sidestep_exact(ChrRecord *self);
void ge_original_dam_guard_tick_jumpout_exact(ChrRecord *self);
void ge_original_dam_guard_tick_dead_exact(ChrRecord *self);
void ge_original_dam_guard_tick_attack_exact(ChrRecord *self);
void ge_original_dam_guard_tick_attack_walk_exact(ChrRecord *self);
void ge_original_dam_guard_tick_attack_roll_exact(ChrRecord *self);
void ge_original_dam_guard_tick_run_pos_exact(ChrRecord *self);
void ge_original_dam_guard_tick_patrol_exact(ChrRecord *self);
void ge_original_dam_guard_tick_surrender_exact(ChrRecord *self);
void ge_original_dam_guard_tick_test_exact(ChrRecord *self);
void ge_original_dam_guard_tick_surprised_exact(ChrRecord *self);
void ge_original_dam_guard_tick_start_alarm_exact(ChrRecord *self);
void ge_original_dam_guard_tick_throw_grenade_exact(ChrRecord *self);
void ge_original_dam_guard_tick_bond_intro_exact(ChrRecord *self);
void ge_original_dam_guard_tick_bond_die_removed_exact(ChrRecord *self);

#endif
