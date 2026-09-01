#ifndef GE_ORIGINAL_DOOR_INTERACTION_H
#define GE_ORIGINAL_DOOR_INTERACTION_H

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalDoorInteractionResult {
    GE_ORIGINAL_DOOR_INTERACTION_IDLE = 0,
    GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED,
    GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED,
    GE_ORIGINAL_DOOR_INTERACTION_LOCKED,
    GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE,
    GE_ORIGINAL_DOOR_INTERACTION_MISSING_PADLOCK_PROVIDER,
    GE_ORIGINAL_DOOR_INTERACTION_MISSING_SWITCH_PROVIDER
} GeOriginalDoorInteractionResult;

typedef struct GeOriginalDoorInteractionProviders {
    void *context;
    /* Required only when the canonical PADLOCKEDDOOR bit is set. */
    int (*padlock_free)(void *context, void *door_definition);
    /* Required only when the canonical linked-switch bit is set. */
    void (*activate_linked_switches)(void *context, void *door_prop);
    /* Optional platform presentation for the original locked-door HUD event. */
    void (*show_locked_message)(void *context, void *door_definition);
} GeOriginalDoorInteractionProviders;

typedef struct GeOriginalDoorInteractionState {
    uint32_t ticks;
    uint32_t activate_edges;
    uint32_t interaction_tests;
    uint32_t interaction_hits;
    uint32_t activations;
    uint32_t locked_attempts;
    uint32_t reload_requests;
    uint32_t tick_operations;
    void *last_prop;
    GeOriginalDoorInteractionResult result;
} GeOriginalDoorInteractionState;

void ge_original_door_interaction_bind(
    const GeOriginalDoorInteractionProviders *providers,
    GeOriginalDoorInteractionState *state);

/* Publishes the renderer's near-to-far onscreen door list.  The adapter sets
 * and clears PROPFLAG_ONSCREEN exactly at this platform visibility boundary;
 * the original propFindForInteract traversal still consumes it in reverse. */
int ge_original_door_interaction_bind_visible_doors(
    void *const *door_props, size_t count);

/* Filters the exact depth-sorted g_OnScreenPropList after renderer
 * publication, preserving its far-to-near order for the canonical reverse
 * interaction traversal. */
int ge_original_door_interaction_bind_onscreen_doors(void);

/* One call at the original lv.c interaction point, after input processing and
 * before propsTickPlayer.  The canonical field_D0 activate edge is read from
 * the committed player; RELOAD_REQUESTED preserves lv.c's fallback contract. */
GeOriginalDoorInteractionResult ge_original_door_interaction_tick(void);

const char *ge_original_door_interaction_result_name(
    GeOriginalDoorInteractionResult result);

#endif
