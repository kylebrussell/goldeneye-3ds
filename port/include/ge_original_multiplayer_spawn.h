#ifndef GE_ORIGINAL_MULTIPLAYER_SPAWN_H
#define GE_ORIGINAL_MULTIPLAYER_SPAWN_H
#include "ge_original_stage_setup.h"
#include "ge_original_player_spawn.h"
#include <stdint.h>

typedef struct GeOriginalMultiplayerSpawnProviders {
    const GeOriginalStageSetupRuntime *stage;
    const GeOriginalPlayerSpawnProviders *player;
    void *context;
    float (*floor_y)(void *context, void *stan, float x, float z);
    /* Caller has loaded the original intro inventory, bound distinct hands,
     * and installed the body/model/gait construction providers. */
    int (*life_resources_ready)(void *context, uint32_t slot);
} GeOriginalMultiplayerSpawnProviders;

typedef struct GeOriginalMultiplayerSpawnResult {
    GeOriginalPlayerSpawnConfig placement;
    void *prop;
    uint32_t slot;
    int32_t pad_index;
    int32_t spawn_index;
    int32_t cursor_before;
    int32_t cursor_after;
    float character_perspective;
    uint8_t placed;
    uint8_t finalized;
} GeOriginalMultiplayerSpawnResult;

typedef enum GeOriginalMultiplayerSpawnStatus {
    GE_ORIGINAL_MP_SPAWN_OK = 0,
    GE_ORIGINAL_MP_SPAWN_INVALID_ARGUMENT,
    GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER,
    GE_ORIGINAL_MP_SPAWN_INVALID_SETUP,
    GE_ORIGINAL_MP_SPAWN_INVALID_FLOOR,
    GE_ORIGINAL_MP_SPAWN_COMMIT_FAILED,
    GE_ORIGINAL_MP_SPAWN_LIFE_NOT_READY,
    GE_ORIGINAL_MP_SPAWN_BODY_FAILED
} GeOriginalMultiplayerSpawnStatus;

/* Caller selects slot through the multiplayer owner first. Both calls require
 * current player/permanent/publication bindings to match slot, and retain that
 * selection. Initial placement uses the existing canonical eyeheight; original
 * BeginLife computes character eyeheight later, matching lv.c's ordering.
 * No solo reset, inventory load, team reset or score reset occurs here. */
GeOriginalMultiplayerSpawnStatus ge_original_multiplayer_spawn_initial(
    uint32_t slot, const GeOriginalMultiplayerSpawnProviders *providers,
    GeOriginalPlayerViewState *view, GeOriginalMultiplayerSpawnResult *result);
/* Call once after the original loadout and all per-player resources are ready.
 * Runs the exact original BeginLife; it equips hands and can build the body. */
GeOriginalMultiplayerSpawnStatus ge_original_multiplayer_spawn_finalize(
    const GeOriginalMultiplayerSpawnProviders *providers,
    GeOriginalMultiplayerSpawnResult *result);
#endif
