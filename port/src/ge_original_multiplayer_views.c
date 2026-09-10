#include "ge_original_multiplayer_views.h"
#include "ge_original_player_spawn_internal.h"

#include <string.h>

static GeOriginalMultiplayerViewStatus fail(GeOriginalMultiplayerViews *views,
    GeOriginalMultiplayerViewStatus status)
{
    views->failed = 1;
    return status;
}

GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_frame(
    GeOriginalMultiplayerViews *views, uint32_t player_count,
    uint32_t local_slot, const GeOriginalMultiplayerViewProviders *providers)
{
    GeOriginalMultiplayerViewProviders copy;
    if (views == NULL || providers == NULL || player_count < 2
            || player_count > 4 || local_slot >= player_count
            || providers->select_player == NULL
            || providers->publish_visibility == NULL
            || providers->capture_local == NULL
            || providers->restore_local_presentation == NULL)
        return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT;
    copy = *providers;
    memset(views, 0, sizeof(*views));
    views->providers = copy;
    views->player_count = player_count;
    views->local_slot = local_slot;
    views->active_slot = UINT32_MAX;
    views->frame_active = 1;
    return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;
}

GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_begin_shooter(
    GeOriginalMultiplayerViews *views, uint32_t slot,
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *camera)
{
    struct player *owner;
    uint32_t previous;
    if (views == NULL || config == NULL || camera == NULL
            || slot >= views->player_count)
        return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT;
    if (!views->frame_active || views->failed
            || (views->published_mask & (1U << slot)) != 0
            || (views->active_slot == views->local_slot
                && !views->local_captured))
        return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER;
    if (!views->providers.select_player(views->providers.context, slot))
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    owner = ge_original_spawn_player_get();
    if (owner == NULL)
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    for (previous = 0; previous < views->player_count; ++previous)
        if (views->owners[previous] == owner)
            return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    if (!ge_original_bond_camera_publish_live_player(config, camera))
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_CAMERA_FAILED);
    if (!views->providers.publish_visibility(views->providers.context, slot,
            config, camera))
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_VISIBILITY_FAILED);
    views->owners[slot] = owner;
    views->published_mask |= (uint8_t)(1U << slot);
    views->active_slot = slot;
    if (slot == views->local_slot) {
        views->local_config = *config;
        views->local_camera = *camera;
    }
    return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;
}

GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_capture_local(
    GeOriginalMultiplayerViews *views)
{
    if (views == NULL) return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT;
    if (!views->frame_active || views->failed || views->local_captured
            || views->active_slot != views->local_slot
            || views->owners[views->local_slot] == NULL
            || ge_original_spawn_player_get() != views->owners[views->local_slot])
        return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER;
    if (!views->providers.capture_local(views->providers.context,
            views->local_slot, &views->local_config, &views->local_camera))
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_CAPTURE_FAILED);
    views->local_captured = 1;
    return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;
}

GeOriginalMultiplayerViewStatus ge_original_multiplayer_views_finish_local(
    GeOriginalMultiplayerViews *views)
{
    if (views == NULL) return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT;
    if (!views->frame_active || views->failed || !views->local_captured
            || views->published_mask != (1U << views->player_count) - 1U)
        return GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER;
    /* Device-local display choice must not alter the canonical tick boundary.
     * Every peer leaves the same last shuffled shooter and shared frustum/BG
     * publication in place. The local camera is already owned by the capture. */
    if (views->active_slot >= views->player_count
            || ge_original_spawn_player_get() != views->owners[views->active_slot])
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    if (!views->providers.restore_local_presentation(views->providers.context,
            views->local_slot, &views->local_config, &views->local_camera))
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_PRESENTATION_FAILED);
    if (ge_original_spawn_player_get() != views->owners[views->active_slot])
        return fail(views, GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    views->frame_active = 0;
    return GE_ORIGINAL_MULTIPLAYER_VIEW_OK;
}
