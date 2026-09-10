#ifndef GE_ORIGINAL_MULTIPLAYER_REGEN_H
#define GE_ORIGINAL_MULTIPLAYER_REGEN_H
#include <stdint.h>

typedef struct GeOriginalMultiplayerRegenProviders {
    void *context;
    void (*enable_prop)(void *context, void *prop);
    void (*update_rooms)(void *context, void *prop);
    void (*play_regen_sound)(void *context, const float position[3]);
} GeOriginalMultiplayerRegenProviders;

typedef enum GeOriginalMultiplayerRegenStatus {
    GE_ORIGINAL_MP_REGEN_OK = 0,
    GE_ORIGINAL_MP_REGEN_IDLE,
    GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT,
    GE_ORIGINAL_MP_REGEN_UNSUPPORTED_OBJECT,
    GE_ORIGINAL_MP_REGEN_MISSING_PROVIDER,
    GE_ORIGINAL_MP_REGEN_INVALID_ROSTER,
    GE_ORIGINAL_MP_REGEN_INVALID_POOL
} GeOriginalMultiplayerRegenStatus;

typedef struct GeOriginalMultiplayerRegenBatch {
    uint32_t visited;
    uint32_t ticked;
    uint32_t unsupported;
    int32_t failed_pool_index; /* -1 when no individual prop failed. */
} GeOriginalMultiplayerRegenBatch;

/* Native chrpropTick traversal: active tail -> prev, bounded to the 600-slot
 * pool, including disabled pickups. Call after active_props_pre_tick_exact
 * and before the player shuffle/passes, once per world simulation tick.
 * Free slots are excluded even if they retain stale object/timer fields.
 * Unsupported timer owners are counted and left to their own lifecycle;
 * errors report any earlier completed ticks, so the caller must not retry. */
GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick_all(
    int32_t tick60, GeOriginalMultiplayerRegenBatch *batch);

/* Native resource binding: original chrpropEnable, original object room
 * publication and original positional OBJ_REGEN_SFX. The caller initializes
 * the normal model/room/audio resources before ticking; no silent sound stub. */
int ge_original_multiplayer_regen_native_providers(
    GeOriginalMultiplayerRegenProviders *providers);
/* Invoke once per simulation tick for each prop, including disabled pickups.
 * Runs the selected unchanged chrpropTick countdown and near-player gate.
 * No current-player mutation; all published players must have finite props. */
GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick(
    void *prop, int32_t tick60,
    const GeOriginalMultiplayerRegenProviders *providers);
#endif
