#ifndef GE_ORIGINAL_PROP_STATE_H
#define GE_ORIGINAL_PROP_STATE_H

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalPropState {
    uint32_t room_capacity;
    uint32_t allocation_calls;
    uint32_t activation_calls;
    uint32_t enable_calls;
    uint32_t room_registration_calls;
} GeOriginalPropState;

typedef struct GeOriginalCharacterSceneState {
    uint8_t flags;
    float zdepth;
} GeOriginalCharacterSceneState;

/* Local to one publication pass. Discard before any canonical list mutation. */
typedef struct GeOriginalPropActiveSet {
    uint8_t active[600];
} GeOriginalPropActiveSet;
int ge_original_prop_state_snapshot_active(GeOriginalPropActiveSet *set);
int ge_original_prop_state_active_set_contains(
    const GeOriginalPropActiveSet *set, const void *prop);

/*
 * Initializes native storage for the original 600-slot prop pool and its
 * 256 room-list chunks. This is a platform allocation boundary; allocation,
 * activation, enabling and room insertion use the exact decompiled bodies.
 */
int ge_original_prop_state_reset(GeOriginalPropState *state,
                                 uint32_t room_capacity);

/* Typed callbacks matching GeOriginalDamWorldProviders. */
void *ge_original_prop_state_allocate(void *context, void *definition);
/* Publishes an already allocated prop into the native ObjectRecord before
 * entering the unchanged objInit preallocated branch. */
int ge_original_prop_state_bind_object(void *definition, void *prop);
/* Sets the canonical one-room/0xff-terminated PropRecord room list before
 * chrpropRegisterRoom publishes it into the stage room blocks. */
int ge_original_prop_state_set_primary_room(void *prop, int16_t room);
/* Typed allocation callback matching GeOriginalPlayerSpawnProviders. */
void *ge_original_prop_state_allocate_player(void *context);
void ge_original_prop_state_activate(void *context, void *prop);
void ge_original_prop_state_enable(void *context, void *prop);
void ge_original_prop_state_register_room(void *context, void *prop,
                                          int16_t room);
/* Exact active-list/room-list/free-list teardown used when a streamed room's
 * ordinary object instance leaves the resident window. */
int ge_original_prop_state_release(void *context, void *prop);

size_t ge_original_prop_state_native_prop_size(void);
uint32_t ge_original_prop_state_active_count(void);
int ge_original_prop_state_is_active(const void *prop);
int ge_original_prop_state_is_enabled(const void *prop);
int ge_original_prop_state_room_contains(int16_t room, const void *prop);

/* Publishes the exact ObjectRecord placement matrix/runtime position and the
 * first registered PropRecord room without exposing native struct layout to
 * the platform frontend. */
int ge_original_prop_state_object_scene_transform(
    const void *definition, const void *prop,
    float matrix[4][4], float position[3], uint8_t *room);

/* Publishes the current canonical Model.render_pos bank without copying or
 * reconstructing any special-object animation.  The unchanged objTick body
 * owns these matrices (including CCTV/autogun joints, rack shelves, vehicle
 * wheels, aircraft rotors and tank turrets); the platform renderer may only
 * consume them after the prop has been published on screen. */
int ge_original_prop_state_object_scene_matrix_bank(
    const void *definition, const void *prop,
    const float (**matrices)[4][4], size_t *matrix_count);

/* Renderer publication boundary for the canonical on-screen prop list. It
 * clears ONSCREEN for non-visible/non-live props; visible active props receive
 * zDepth and their exact ordinary-object model matrices from the original
 * objTick transform convention before ONSCREEN is set. */
int ge_original_prop_state_publish_scene_visibility(
    void *prop, int visible, const float world_to_view[4][4]);
int ge_original_prop_state_publish_scene_visibility_with_active_set(
    void *prop, int visible, const float world_to_view[4][4],
    const GeOriginalPropActiveSet *active);

/* Read-only character renderer observation. The unchanged chrTick body is
 * the sole writer of character ONSCREEN and zDepth; platform scene residency
 * must never manufacture either value for a guard. */
int ge_original_prop_state_observe_character_scene_state(
    const void *prop, GeOriginalCharacterSceneState *state);

/* Exact active-list room-object polygon query. It is valid only after the
 * queried objects have completed their original placement/collision step. */
void *ge_original_prop_state_room_object_at_position(
    const float position[3], int16_t room, float *top, float *bottom);

#endif
