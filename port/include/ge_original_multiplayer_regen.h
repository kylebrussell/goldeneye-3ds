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
    GE_ORIGINAL_MP_REGEN_INVALID_ROSTER
} GeOriginalMultiplayerRegenStatus;

/* Invoke once per simulation tick for each prop, including disabled pickups.
 * Runs the selected unchanged chrpropTick countdown and near-player gate.
 * No current-player mutation; all published players must have finite props. */
GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick(
    void *prop, int32_t tick60,
    const GeOriginalMultiplayerRegenProviders *providers);
#endif
