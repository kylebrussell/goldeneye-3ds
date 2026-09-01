#ifndef GE_ORIGINAL_STAGE_ACTIVE_PROPS_H
#define GE_ORIGINAL_STAGE_ACTIVE_PROPS_H

#include <stddef.h>
#include <stdint.h>

struct ChrRecord;
struct GeOriginalStageSetupRuntime;

typedef enum GeOriginalStageActivePropKind {
    GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED = 0,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_DYNAMIC = 1
} GeOriginalStageActivePropKind;

typedef struct GeOriginalStageActivePropInput {
    size_t command_index;
    void *prop;
    GeOriginalStageActivePropKind kind;
} GeOriginalStageActivePropInput;

typedef enum GeOriginalStageActivePropStatus {
    GE_ORIGINAL_STAGE_ACTIVE_PROP_OK = 0,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_PLAYER,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ACTOR,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_DUPLICATE,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_TIMER_UNBOUND,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_PLAYER_UNBOUND,
    GE_ORIGINAL_STAGE_ACTIVE_PROP_SETUP_UNBOUND
} GeOriginalStageActivePropStatus;

typedef struct GeOriginalStageActiveProps {
    GeOriginalStageActivePropInput *ordered;
    const struct GeOriginalStageSetupRuntime *setup;
    struct ChrRecord *chrs;
    void *player_prop;
    size_t count;
    size_t chr_count;
    /* chrlvAllChrTick belongs to lvlManageMpGame, while propsTick belongs to
     * lvlRender. Keep the two unchanged bodies paired without allowing a
     * stalled native render frame to advance background AI twice. */
    uint64_t pre_ticks;
    uint64_t ticks;
    /* Canonical watch/cutscene pauses publish zero clock/delta. The outer
     * gameplay loop suppresses chr/prop advancement for those frames while
     * keeping the authored active-list binding live for the resume frame. */
    uint64_t paused_ticks;
    uint32_t last_binding_mismatch;
    uint8_t pre_tick_pending;
    uint8_t bound;
} GeOriginalStageActiveProps;

/* Rebuilds the exact chrpropActivate ordering: player first, setup props in
 * ascending authored command order, then runtime-created props in their
 * supplied creation order. Globals are published only after full validation. */
GeOriginalStageActivePropStatus ge_original_stage_active_props_compose(
    GeOriginalStageActiveProps *state,
    const struct GeOriginalStageSetupRuntime *setup,
    void *player_prop, struct ChrRecord *chrs, size_t chr_count,
    const GeOriginalStageActivePropInput *inputs, size_t input_count);

/* bossMainloop reaches this boundary from lvlManageMpGame, before its player
 * shuffle and lvlViewMoveTick. It calls the retained unchanged
 * chrlvAllChrTick exactly once and leaves a token for the render-side pass. */
GeOriginalStageActivePropStatus ge_original_stage_active_props_pre_tick_exact(
    GeOriginalStageActiveProps *state);

/* Calls the retained unchanged propsTick at the canonical lvlRender boundary.
 * A pending pre-tick is required, keeping background mission AI before player
 * movement while preventing a partial native frame from double-advancing it. */
GeOriginalStageActivePropStatus ge_original_stage_active_props_tick_exact(
    GeOriginalStageActiveProps *state);
void ge_original_stage_active_props_close(GeOriginalStageActiveProps *state);

#endif
