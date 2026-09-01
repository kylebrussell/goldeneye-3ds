#include "ge_original_stage_monitor_surface.h"

#include <string.h>

static GePicaTextureWrap monitor_wrap(uint8_t flags)
{
    if ((flags & UINT8_C(2)) != 0U) return GE_PICA_WRAP_CLAMP;
    if ((flags & UINT8_C(1)) != 0U) return GE_PICA_WRAP_MIRROR;
    return GE_PICA_WRAP_REPEAT;
}

static int snapshot_valid(const GeOriginalDamMonitorRenderSnapshot *snapshot)
{
    return snapshot != NULL && snapshot->switch_node != NULL
        && snapshot->texture_config != NULL
        && snapshot->texture_id <= UINT16_MAX
        && snapshot->width != 0U && snapshot->height != 0U
        && snapshot->format <= UINT8_C(4)
        && snapshot->depth <= UINT8_C(3)
        && snapshot->flags_s <= UINT8_C(3)
        && snapshot->flags_t <= UINT8_C(3)
        && (snapshot->texture_mode == 0U
            || snapshot->texture_mode == 1U
            || snapshot->texture_mode == 8U)
        && (snapshot->texture_alpha_mode == 1U
            || snapshot->texture_alpha_mode == 2U);
}

int ge_original_stage_monitor_surface_output_equal(
    const GeOriginalDamMonitorRenderSnapshot *left,
    const GeOriginalDamMonitorRenderSnapshot *right)
{
    if (left == NULL || right == NULL) return 0;
    return left->switch_node == right->switch_node
        && left->texture_config == right->texture_config
        && left->texture_id == right->texture_id
        && left->image_slot == right->image_slot
        && left->width == right->width
        && left->height == right->height
        && left->level == right->level
        && left->format == right->format
        && left->depth == right->depth
        && left->flags_s == right->flags_s
        && left->flags_t == right->flags_t
        && left->texture_mode == right->texture_mode
        && left->texture_alpha_mode == right->texture_alpha_mode
        && left->command_offset == right->command_offset
        && memcmp(left->vertices, right->vertices,
                  sizeof(left->vertices)) == 0;
}

GeOriginalStageMonitorSurfaceStatus ge_original_stage_monitor_surface_apply(
    const GeOriginalDamMonitorRenderSnapshot *snapshot,
    GeDamRoomSceneStorage *scene, size_t batch_index,
    GeOriginalStageMonitorSurfaceResult *result)
{
    static const uint8_t quad_indices[
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT] = {0U,1U,2U,0U,2U,3U};
    GeDamRoomDrawBatch candidate_batch;
    GeDamRoomWorldVertex candidate_vertices[
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT];
    GeDamRoomDrawBatch *batch;
    size_t index;
    GeOriginalStageMonitorSurfaceStatus status =
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
        result->status = status;
    }
    if (snapshot == NULL || scene == NULL || result == NULL
            || scene->vertices == NULL || scene->batches == NULL
            || batch_index >= scene->batch_capacity) return status;
    if (!snapshot_valid(snapshot)) {
        status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT;
        goto done;
    }
    batch = &scene->batches[batch_index];
    if (batch->vertex_count != GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT
            || batch->triangle_count
                != GE_ORIGINAL_STAGE_MONITOR_SURFACE_TRIANGLE_COUNT
            || batch->first_vertex > scene->vertex_capacity
            || batch->vertex_count
                > scene->vertex_capacity - batch->first_vertex) {
        status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH;
        goto done;
    }
    candidate_batch = *batch;
    for (index = 0U;
            index < GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT; ++index) {
        const GeOriginalDamMonitorRenderVertex *dynamic =
            &snapshot->vertices[quad_indices[index]];
        GeDamRoomWorldVertex *candidate = &candidate_vertices[index];
        const GeDamRoomWorldVertex *target =
            &scene->vertices[batch->first_vertex + index];
        if (target->source.x != dynamic->x
                || target->source.y != dynamic->y
                || target->source.z != dynamic->z) {
            status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH;
            goto done;
        }
        *candidate = *target;
        candidate->source.texture_s = dynamic->s;
        candidate->source.texture_t = dynamic->t;
        candidate->source.red = dynamic->red;
        candidate->source.green = dynamic->green;
        candidate->source.blue = dynamic->blue;
        candidate->source.alpha = dynamic->alpha;
        candidate->processed.texture[0] = (float)dynamic->s;
        candidate->processed.texture[1] = (float)dynamic->t;
        candidate->processed.rgba[0] = dynamic->red;
        candidate->processed.rgba[1] = dynamic->green;
        candidate->processed.rgba[2] = dynamic->blue;
        candidate->processed.rgba[3] = dynamic->alpha;
    }

    candidate_batch.texture.texture_id = (uint16_t)snapshot->texture_id;
    candidate_batch.texture.detail_texture_id = 0U;
    candidate_batch.texture.min_level = snapshot->level;
    candidate_batch.texture.tile = 0U;
    candidate_batch.texture.clamp_mirror_s = snapshot->flags_s;
    candidate_batch.texture.clamp_mirror_t = snapshot->flags_t;
    candidate_batch.texture.shift_s = 0U;
    candidate_batch.texture.shift_t = 0U;
    candidate_batch.texture_valid = UINT8_C(1);
    candidate_batch.material.fallback_flags &=
        ~((uint32_t)GE_PICA_FALLBACK_COMBINER
          | (uint32_t)GE_PICA_FALLBACK_TWO_CYCLE
          | (uint32_t)GE_PICA_FALLBACK_COPY_FILL_CYCLE
          | (uint32_t)GE_PICA_FALLBACK_TEXTURE_DETAIL
          | (uint32_t)GE_PICA_FALLBACK_TEXTURE_LOD
          | (uint32_t)GE_PICA_FALLBACK_TEXTURE_LUT
          | (uint32_t)GE_PICA_FALLBACK_CULL_BOTH
          | (uint32_t)GE_PICA_FALLBACK_BLENDER
          | (uint32_t)GE_PICA_FALLBACK_DETAIL_TEXTURE
          | (uint32_t)GE_PICA_FALLBACK_RARE_TEXTURE_TYPE
          | (uint32_t)GE_PICA_FALLBACK_MISSING_TEXTURE);
    candidate_batch.material.texture_id = (uint16_t)snapshot->texture_id;
    candidate_batch.material.detail_texture_id = 0U;
    candidate_batch.material.texture_image_width = snapshot->width;
    candidate_batch.material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    candidate_batch.material.texture_enabled = UINT8_C(1);
    candidate_batch.material.texture_tile = UINT8_C(0);
    candidate_batch.material.texture_type = UINT8_C(0);
    candidate_batch.material.texture_min_level = snapshot->level;
    candidate_batch.material.texture_shift_s = UINT8_C(0);
    candidate_batch.material.texture_shift_t = UINT8_C(0);
    candidate_batch.material.texture_image_format = snapshot->format;
    candidate_batch.material.texture_image_size = snapshot->depth;
    candidate_batch.material.texture_scale_s = UINT16_MAX;
    candidate_batch.material.texture_scale_t = UINT16_MAX;
    candidate_batch.material.wrap_s = monitor_wrap(snapshot->flags_s);
    candidate_batch.material.wrap_t = monitor_wrap(snapshot->flags_t);
    candidate_batch.material.color_combine =
        GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE;
    /* texSelect uses MODULATEI for intensity and MODULATEIA for all other
     * authored monitor formats. */
    candidate_batch.material.alpha_combine = snapshot->format == UINT8_C(4)
        ? GE_PICA_ALPHA_SHADE
        : GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE;
    candidate_batch.material.cull_mode = GE_PICA_CULL_BACK;
    candidate_batch.material.cycle_type = GE_PICA_CYCLE_ONE;
    candidate_batch.material.alpha_test = GE_PICA_ALPHA_TEST_DISABLED;
    candidate_batch.material.alpha_threshold = UINT8_C(0);
    candidate_batch.material.blend_enabled =
        snapshot->texture_alpha_mode == 2U ? UINT8_C(1) : UINT8_C(0);
    candidate_batch.material.depth_test_enabled =
        snapshot->texture_mode != 0U ? UINT8_C(1) : UINT8_C(0);
    candidate_batch.material.depth_write_enabled =
        snapshot->texture_mode == 1U
            && snapshot->texture_alpha_mode == 1U ? UINT8_C(1) : UINT8_C(0);
    candidate_batch.material.depth_mode = snapshot->texture_mode >= 2U
        ? GE_PICA_DEPTH_DECAL
        : snapshot->texture_alpha_mode == 2U
            ? GE_PICA_DEPTH_TRANSLUCENT : GE_PICA_DEPTH_OPAQUE;

    memcpy(scene->vertices + batch->first_vertex, candidate_vertices,
           sizeof(candidate_vertices));
    *batch = candidate_batch;
    status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK;
done:
    result->status = status;
    result->room_id = status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
        ? scene->batches[batch_index].room_id : 0U;
    result->texture_id = status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
        ? snapshot->texture_id : 0U;
    result->batch_index = batch_index;
    result->first_vertex = status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
        ? scene->batches[batch_index].first_vertex : 0U;
    result->vertex_count = status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
        ? GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT : 0U;
    result->triangle_count = status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
        ? GE_ORIGINAL_STAGE_MONITOR_SURFACE_TRIANGLE_COUNT : 0U;
    return status;
}

GeOriginalStageMonitorSurfaceStatus
ge_original_stage_monitor_surface_apply_part(
    const GeOriginalDamMonitorRenderSnapshot *snapshot,
    GeDamRoomSceneStorage *scene, size_t first_batch, size_t batch_count,
    GeOriginalStageMonitorSurfaceResult *result)
{
    static const uint8_t quad_indices[
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT] = {0U,1U,2U,0U,2U,3U};
    size_t batch_index;
    size_t match = 0U;
    size_t match_count = 0U;
    GeOriginalStageMonitorSurfaceStatus status;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
        result->status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT;
    }
    if (snapshot == NULL || scene == NULL || result == NULL
            || scene->vertices == NULL || scene->batches == NULL
            || first_batch > scene->batch_capacity
            || batch_count > scene->batch_capacity - first_batch)
        return GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT;
    if (!snapshot_valid(snapshot)) {
        result->status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT;
        return result->status;
    }
    for (batch_index = first_batch;
            batch_index < first_batch + batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        size_t vertex_index;
        if (batch->vertex_count
                    != GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT
                || batch->triangle_count
                    != GE_ORIGINAL_STAGE_MONITOR_SURFACE_TRIANGLE_COUNT
                || batch->first_vertex > scene->vertex_capacity
                || batch->vertex_count
                    > scene->vertex_capacity - batch->first_vertex)
            continue;
        for (vertex_index = 0U;
                vertex_index < GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT;
                ++vertex_index) {
            const GeDamRoomWorldVertex *target =
                &scene->vertices[batch->first_vertex + vertex_index];
            const GeOriginalDamMonitorRenderVertex *source =
                &snapshot->vertices[quad_indices[vertex_index]];
            if (target->source.x != source->x
                    || target->source.y != source->y
                    || target->source.z != source->z)
                break;
        }
        if (vertex_index
                != GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT) continue;
        match = batch_index;
        ++match_count;
    }
    if (match_count != 1U) {
        result->status = GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH;
        return result->status;
    }
    status = ge_original_stage_monitor_surface_apply(
        snapshot, scene, match, result);
    return status;
}

const char *ge_original_stage_monitor_surface_status_name(
    GeOriginalStageMonitorSurfaceStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK: return "ok";
    case GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT:
        return "invalid snapshot";
    case GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH:
        return "target topology mismatch";
    }
    return "unknown";
}
