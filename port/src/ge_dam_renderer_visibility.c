#include "ge_dam_renderer_visibility.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(GE_DAM_PRELOAD_MAX_ROOMS <= GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS,
    "renderer preload membership must hold the complete request queue");

/* Deliberately only this module can call the private interpreter. */
GeOriginalBgVisibilityStatus ge_renderer_private_ge_original_bg_visibility_run(
    const GeOriginalBgVisibilityInput *, GeOriginalBgVisibilityResult *);

int ge_dam_renderer_visibility_eye_input(
    const GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera, const float position[3],
    const GeRendererEyeShift *shift, GeOriginalBgVisibilityInput *input)
{
    if (runtime == NULL || camera == NULL || position == NULL || shift == NULL
            || input == NULL || !runtime->loaded || runtime->room_count == 0U
            || runtime->room_count > GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
            || camera->room >= runtime->room_count
            || !isfinite(shift->constant_px) || !isfinite(shift->coefficient_px))
        return 0;
    const int width = abs(camera->viewport_scale[0]) / 2;
    const int height = abs(camera->viewport_scale[1]) / 2;
    if (width <= 0 || height <= 0 || width > INT16_MAX || height > INT16_MAX)
        return 0;
    for (size_t row = 0U; row < 4U; ++row)
        for (size_t col = 0U; col < 4U; ++col)
            if (!isfinite(camera->view[row][col])
                    || !isfinite(camera->projection[row][col])) return 0;
    if (camera->projection[2][3] != -1.0f
            || camera->projection[3][3] != 0.0f
            || camera->projection[0][3] != 0.0f
            || camera->projection[1][3] != 0.0f
            || camera->projection[0][1] != 0.0f || camera->projection[0][2] != 0.0f
            || camera->projection[1][0] != 0.0f || camera->projection[1][2] != 0.0f
            || camera->projection[3][0] != 0.0f || camera->projection[3][1] != 0.0f
            || camera->projection[0][0] <= 0.0f
            || camera->projection[1][1] <= 0.0f) return 0;
    /* Validate the view basis before using its transpose as its inverse. */
    for (size_t axis = 0U; axis < 3U; ++axis) {
        if (!isfinite(position[axis]) || camera->view[axis][3] != 0.0f) return 0;
        for (size_t other = 0U; other < 3U; ++other) {
            float dot = 0.0f;
            for (size_t row = 0U; row < 3U; ++row)
                dot += camera->view[row][axis] * camera->view[row][other];
            if (fabsf(dot - (axis == other ? 1.0f : 0.0f)) > 0.001f) return 0;
        }
    }
    if (camera->view[3][3] != 1.0f) return 0;
    memset(input, 0, sizeof(*input));
    input->background = runtime->background;
    input->background_size = runtime->background_size;
    input->room_bounds = runtime->room_bounds;
    input->room_count = runtime->room_count;
    input->level_index = runtime->level_index;
    input->current_room = camera->room;
    input->level_scale = runtime->level_scale;
    input->visibility_scale = runtime->visibility_scale;
    input->near_distance = runtime->near_distance;
    input->far_distance = runtime->far_distance;
    input->vertical_fov_degrees = 60.0f;
    input->aspect_ratio = (float)width / (float)height;
    input->view_width = (int16_t)width;
    input->view_height = (int16_t)height;
    input->program = runtime->program;
    memcpy(input->world_to_screen, camera->view, sizeof(input->world_to_screen));
    input->projection_focal_px[0] = camera->projection[0][0] * width * 0.5f;
    input->projection_focal_px[1] = camera->projection[1][1] * height * 0.5f;
    input->projection_offset_px[0] = -camera->projection[2][0] * width * 0.5f
        + shift->coefficient_px;
    input->projection_offset_px[1] = camera->projection[2][1] * height * 0.5f;
    const float translation = shift->constant_px / input->projection_focal_px[0];
    input->world_to_screen[3][0] += translation;
    for (size_t axis = 0U; axis < 3U; ++axis)
        input->player_position[axis] = position[axis]
            - translation * camera->view[axis][0];
    return 1;
}

static int merge_visibility(GeDamRendererVisibility *result,
    const GeOriginalBgVisibilityResult *eye, uint8_t bit, size_t room_count)
{
    if (eye->room_count > GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE) return 0;
    for (size_t i = 0U; i < eye->room_count; ++i) {
        const GeOriginalBgVisibleRoom *room = &eye->rooms[i];
        if (room->room >= room_count) return 0;
        if (result->membership[room->room] == 0U) {
            if (result->visible.room_count == GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE)
                return 0;
            result->visible.rooms[result->visible.room_count++] = *room;
        } else {
            for (size_t j = 0U; j < result->visible.room_count; ++j) {
                GeOriginalBgVisibleRoom *merged = &result->visible.rooms[j];
                if (merged->room != room->room) continue;
                for (size_t axis = 0U; axis < 2U; ++axis) {
                    merged->screen_box[axis] = fminf(merged->screen_box[axis], room->screen_box[axis]);
                    merged->screen_box[axis + 2U] = fmaxf(merged->screen_box[axis + 2U], room->screen_box[axis + 2U]);
                }
                break;
            }
        }
        result->membership[room->room] |= bit;
    }
    return 1;
}

GeOriginalBgVisibilityStatus ge_dam_renderer_visibility_run(
    const GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera, const float position[3],
    const GeOriginalBgVisibilityProviders *providers,
    const GeDamPreloadQueue *queue,
    const GeOriginalBgVisibilityResult *canonical,
    const GeRendererEyeShift shifts[2], GeDamRendererVisibility *result)
{
    GeDamRendererVisibility candidate = {0};
    if (runtime == NULL || !runtime->loaded || camera == NULL || position == NULL
            || canonical == NULL || shifts == NULL || result == NULL
            || runtime->room_count == 0U
            || runtime->room_count > GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
            || (queue != NULL && (queue->room_count != runtime->room_count
                || queue->room_count > GE_DAM_PRELOAD_MAX_ROOMS)))
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
    if (!merge_visibility(&candidate, canonical, 1U, runtime->room_count))
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
    /* Metadata remains canonical; membership is the renderer's separate
     * contract. Active stereo deduplicates rooms to stay within MAX_ROOMS. */
    const size_t unique_count = candidate.visible.room_count;
    GeOriginalBgVisibleRoom unique[GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS];
    memcpy(unique, candidate.visible.rooms, unique_count * sizeof(*unique));
    candidate.visible = *canonical;
    if (queue != NULL)
        memcpy(candidate.preload, queue->visibility_requested,
            sizeof(queue->visibility_requested));
    candidate.stereo = shifts[0].constant_px != 0.0f || shifts[0].coefficient_px != 0.0f
        || shifts[1].constant_px != 0.0f || shifts[1].coefficient_px != 0.0f;
    if (!candidate.stereo) {
        *result = candidate;
        return GE_ORIGINAL_BG_VISIBILITY_OK;
    }
    candidate.visible.room_count = unique_count;
    memcpy(candidate.visible.rooms, unique, unique_count * sizeof(*unique));
    for (size_t eye = 0U; eye < 2U; ++eye) {
        GeOriginalBgVisibilityInput input;
        GeOriginalBgVisibilityResult eye_result;
        GeDamPreloadQueue shadow;
        if (!ge_dam_renderer_visibility_eye_input(runtime, camera, position,
                &shifts[eye], &input)) return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
        if (queue != NULL) shadow = *queue;
        else if (ge_dam_preload_queue_init(&shadow, runtime->room_count,
                runtime->room_count, NULL, 0U) != GE_DAM_PRELOAD_OK)
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
        GeOriginalBgVisibilityProviders isolated = ge_dam_preload_queue_providers(
            &shadow, providers != NULL ? providers->portal_controls : NULL,
            providers != NULL ? providers->portal_control_count : 0U);
        input.providers = &isolated;
        const GeOriginalBgVisibilityStatus status =
            ge_renderer_private_ge_original_bg_visibility_run(&input, &eye_result);
        if (status != GE_ORIGINAL_BG_VISIBILITY_OK) return status;
        if (!merge_visibility(&candidate, &eye_result, (uint8_t)(2U << eye), runtime->room_count))
            return GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED;
        for (size_t room = 0U; room < runtime->room_count; ++room)
            if (shadow.visibility_requested[room]) candidate.preload[room] = 1U;
    }
    for (size_t room = 0U; room < runtime->room_count; ++room)
        if (candidate.membership[room] & 6U) candidate.preload[room] = 1U;
    *result = candidate;
    return GE_ORIGINAL_BG_VISIBILITY_OK;
}
