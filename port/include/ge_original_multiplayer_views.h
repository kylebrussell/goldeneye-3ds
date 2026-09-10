#ifndef GE_ORIGINAL_MULTIPLAYER_VIEWS_H
#define GE_ORIGINAL_MULTIPLAYER_VIEWS_H

#include "ge_original_bond_camera.h"

struct player;

typedef enum GeOriginalMultiplayerViewStatus {
    GE_ORIGINAL_MULTIPLAYER_VIEW_OK = 0,
    GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT,
    GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER,
    GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED,
    GE_ORIGINAL_MULTIPLAYER_VIEW_CAMERA_FAILED,
    GE_ORIGINAL_MULTIPLAYER_VIEW_VISIBILITY_FAILED,
    GE_ORIGINAL_MULTIPLAYER_VIEW_CAPTURE_FAILED,
    GE_ORIGINAL_MULTIPLAYER_VIEW_PRESENTATION_FAILED
} GeOriginalMultiplayerViewStatus;

typedef struct GeOriginalMultiplayerViewProviders {
    void *context;
    int (*select_player)(void *context, uint32_t slot);
    /* Publish the shooter's canonical mono room/prop visibility inputs before
     * the caller invokes the unchanged propsTick once for this slot. */
    int (*publish_visibility)(void *context, uint32_t slot,
        const GeOriginalBondCameraConfig *config,
        const GeOriginalBondCameraResult *camera);
    /* Bake/copy the local body, held-item and first-person presentation into
     * durable local scene storage before another slot replaces render_pos.
     * No simulation calls here. Both eye draws consume this same scene. */
    int (*capture_local)(void *context, uint32_t slot,
        const GeOriginalBondCameraConfig *config,
        const GeOriginalBondCameraResult *camera);
    /* Restore only owned local renderer data after the last shooter. This
     * must not select a player or change native camera/frustum/visibility,
     * model state, RNG, or any other canonical state. */
    int (*restore_local_presentation)(void *context, uint32_t slot,
        const GeOriginalBondCameraConfig *config,
        const GeOriginalBondCameraResult *camera);
} GeOriginalMultiplayerViewProviders;

typedef struct GeOriginalMultiplayerViews {
    GeOriginalMultiplayerViewProviders providers;
    GeOriginalBondCameraConfig local_config;
    GeOriginalBondCameraResult local_camera;
    struct player *owners[4];
    uint32_t player_count;
    uint32_t local_slot;
    uint32_t active_slot;
    uint8_t published_mask;
    uint8_t local_captured;
    uint8_t frame_active;
    uint8_t failed;
} GeOriginalMultiplayerViews;

/* Begin once per simulation frame; the caller retains ownership of canonical
 * shuffle order, pre_tick, shared dyn arena and exactly one props pass/slot.
 * A failed callback invalidates this frame: do not repeat a partial pass. */
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_frame(
    GeOriginalMultiplayerViews *views, uint32_t player_count,
    uint32_t local_slot, const GeOriginalMultiplayerViewProviders *providers);

/* config/result must be the original MONO camera for this player after its
 * movement pass. This boundary never invokes propsTick, chrTick or gun ticks.
 * After success the caller runs propsTick -> autoaim/use/interaction/pickups
 * -> gun update in the original lvlRender order, before selecting another
 * shooter. chrTick/playerTick publish body matrices, zDepth and hit lists. */
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_shooter(
    GeOriginalMultiplayerViews *views, uint32_t slot,
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *camera);

GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_capture_local(
    GeOriginalMultiplayerViews *views);

/* Call once after every shuffled shooter pass and local capture. Keeps the
 * last shuffled shooter selected, with its native frustum/visibility intact.
 * Only the provider's owned local renderer data is restored. Both eyes and the
 * canonical state capture therefore share the same completed simulation. */
GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_finish_local(
    GeOriginalMultiplayerViews *views);

#endif
