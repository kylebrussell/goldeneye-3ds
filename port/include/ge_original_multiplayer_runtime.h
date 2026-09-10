#ifndef GE_ORIGINAL_MULTIPLAYER_RUNTIME_H
#define GE_ORIGINAL_MULTIPLAYER_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include "ge_original_bond_live.h"
#include "ge_original_multiplayer_death.h"
#include "ge_original_multiplayer_hud.h"
#include "ge_original_multiplayer_regen.h"
#include "ge_original_multiplayer_views.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_guard_runtime.h"

typedef enum GeOriginalMultiplayerRuntimeStatus {
    GE_ORIGINAL_MP_RUNTIME_OK = 0,
    GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED,
    GE_ORIGINAL_MP_RUNTIME_SELECT_FAILED,
    GE_ORIGINAL_MP_RUNTIME_DEATH_FAILED,
    GE_ORIGINAL_MP_RUNTIME_MOVE_FAILED,
    GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED,
    GE_ORIGINAL_MP_RUNTIME_HELD_ITEMS_FAILED,
    GE_ORIGINAL_MP_RUNTIME_INTERACTION_FAILED,
    GE_ORIGINAL_MP_RUNTIME_GUN_RESOURCE_FAILED,
    GE_ORIGINAL_MP_RUNTIME_GUN_FAILED,
    GE_ORIGINAL_MP_RUNTIME_BODY_FAILED,
    GE_ORIGINAL_MP_RUNTIME_HUD_FAILED,
    GE_ORIGINAL_MP_RUNTIME_RESPAWN_FAILED
} GeOriginalMultiplayerRuntimeStatus;

typedef struct GeOriginalMultiplayerRuntimeFrame {
    uint64_t tick;
    uint32_t count, local;
    int32_t clock_timer, global_timer;
    float global_timer_delta;
    GeOriginalBondLiveState *lives[4];
    GeOriginalMultiplayerDeathState *death;
    GeOriginalMultiplayerHud *hud;
    GeOriginalMultiplayerViews *views;
    GeOriginalStageActiveProps *active_props;
    GeOriginalStageGuardRuntime *guards;
    GeOriginalMultiplayerViewProviders view_providers;
    void *context;
    /* Select all per-player resource owners; must select the canonical roster
     * and input slot too. MoveBond itself is called directly by this runner. */
    int (*select_player)(void *context, uint32_t slot);
    int (*produce_camera)(void *context, uint32_t slot,
        GeOriginalBondCameraConfig *config, GeOriginalBondCameraResult *camera);
    int (*sync_gun_resources)(void *context, uint32_t slot);
    /* Publish only the local renderer eye union/margin before native matrix
     * and flash preparation. No gameplay or canonical visibility mutation. */
    int (*prepare_local_renderer)(void *context, uint32_t slot,
        const GeOriginalBondCameraConfig *config,
        const GeOriginalBondCameraResult *camera);
    uint32_t last_slot;
    GeOriginalMultiplayerRuntimeStatus status;
} GeOriginalMultiplayerRuntimeFrame;

/* Call after the one committed input/timer tick, before global music. Retains
 * the original all-chr pre-tick, pickup regeneration and one canonical shuffle.
 * Unsupported positive-timer owners fail closed after earlier accepted
 * owners may have ticked; the caller must not retry that partial frame. */
GeOriginalMultiplayerRuntimeStatus ge_original_multiplayer_runtime_begin_world(
    GeOriginalStageActiveProps *active_props, int32_t tick60,
    GeOriginalMultiplayerRegenBatch *regeneration);

/* Current canonical fovy after MoveBond's watch/weapon zoom update. The
 * platform camera must use this value rather than resetting it to 60. */
int ge_original_multiplayer_runtime_camera_fovy(float *fovy);

/* The production and headless paths share this exact loop. Begin one shared
 * gun dynamic frame before entering; finalize it after return. A failure may
 * follow gameplay mutations and requires match abort, never a partial retry.
 * Caller advances tick only on success. Draw callbacks consume durable local
 * publications and must not invoke this runner or advance gameplay. */
GeOriginalMultiplayerRuntimeStatus ge_original_multiplayer_runtime_tick(
    GeOriginalMultiplayerRuntimeFrame *frame);

#endif
