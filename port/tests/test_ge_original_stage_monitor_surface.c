#include "ge_original_stage_monitor_surface.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t quad_indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};

static GeOriginalDamMonitorRenderSnapshot make_snapshot(void)
{
    GeOriginalDamMonitorRenderSnapshot snapshot;
    size_t index;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.switch_node = (const void *)(uintptr_t)UINT32_C(0x1234);
    snapshot.texture_config = (const void *)(uintptr_t)UINT32_C(0x5678);
    snapshot.texture_id = UINT32_C(2224);
    snapshot.image_slot = UINT8_C(31);
    snapshot.width = UINT8_C(32);
    snapshot.height = UINT8_C(16);
    snapshot.level = UINT8_C(2);
    snapshot.format = UINT8_C(4);
    snapshot.depth = UINT8_C(1);
    snapshot.flags_s = UINT8_C(2);
    snapshot.flags_t = UINT8_C(1);
    snapshot.texture_mode = UINT8_C(8);
    snapshot.texture_alpha_mode = UINT8_C(2);
    for (index = 0U; index < 4U; ++index) {
        snapshot.vertices[index].x = (int16_t)(10 + (int16_t)index);
        snapshot.vertices[index].y = (int16_t)(20 + (int16_t)index * 2);
        snapshot.vertices[index].z = (int16_t)(30 - (int16_t)index);
        snapshot.vertices[index].s = (int16_t)(-64 + (int16_t)index * 128);
        snapshot.vertices[index].t = (int16_t)(256 - (int16_t)index * 32);
        snapshot.vertices[index].red = (uint8_t)(40U + index);
        snapshot.vertices[index].green = (uint8_t)(80U + index);
        snapshot.vertices[index].blue = (uint8_t)(120U + index);
        snapshot.vertices[index].alpha = (uint8_t)(160U + index);
    }
    return snapshot;
}

static void initialize_quad(GeDamRoomWorldVertex *vertices,
                            const GeOriginalDamMonitorRenderSnapshot *snapshot)
{
    size_t index;
    memset(vertices, 0, 6U * sizeof(*vertices));
    for (index = 0U; index < 6U; ++index) {
        const GeOriginalDamMonitorRenderVertex *source =
            &snapshot->vertices[quad_indices[index]];
        vertices[index].source.x = source->x;
        vertices[index].source.y = source->y;
        vertices[index].source.z = source->z;
        vertices[index].source.texture_s = INT16_C(7);
        vertices[index].source.texture_t = INT16_C(9);
        vertices[index].source.red = UINT8_C(1);
        vertices[index].source.green = UINT8_C(2);
        vertices[index].source.blue = UINT8_C(3);
        vertices[index].source.alpha = UINT8_C(4);
        vertices[index].processed.object[0] = 100.0f + (float)index;
        vertices[index].processed.eye[1] = 200.0f + (float)index;
        vertices[index].processed.clip[2] = 300.0f + (float)index;
        vertices[index].world[0] = 400.0f + (float)index;
        vertices[index].world[1] = 500.0f + (float)index;
        vertices[index].world[2] = 600.0f + (float)index;
    }
}

static void initialize_batch(GeDamRoomDrawBatch *batch, size_t first_vertex)
{
    memset(batch, 0, sizeof(*batch));
    batch->room_id = UINT32_C(17);
    batch->first_vertex = first_vertex;
    batch->vertex_count = 6U;
    batch->triangle_count = 2U;
    batch->material.min_filter = GE_PICA_FILTER_LINEAR;
    batch->material.mag_filter = GE_PICA_FILTER_NEAREST;
    batch->material.fallback_flags =
        (uint32_t)GE_PICA_FALLBACK_MISSING_TEXTURE
        | (uint32_t)GE_PICA_FALLBACK_COMBINER;
    batch->material.texture_type = UINT8_C(3);
}

static void test_exact_publication(void)
{
    GeOriginalDamMonitorRenderSnapshot snapshot = make_snapshot();
    GeDamRoomWorldVertex vertices[12];
    GeDamRoomDrawBatch batches[2];
    GeDamRoomSceneStorage scene = {vertices, 12U, batches, 2U};
    GeOriginalStageMonitorSurfaceResult result;
    size_t index;

    initialize_quad(vertices, &snapshot);
    initialize_quad(vertices + 6U, &snapshot);
    initialize_batch(&batches[0], 0U);
    initialize_batch(&batches[1], 6U);
    assert(ge_original_stage_monitor_surface_apply_part(
        &snapshot, &scene, 0U, 1U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK);
    assert(result.status == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
           && result.room_id == UINT32_C(17)
           && result.texture_id == snapshot.texture_id
           && result.batch_index == 0U && result.first_vertex == 0U
           && result.vertex_count == 6U && result.triangle_count == 2U);
    for (index = 0U; index < 6U; ++index) {
        const GeOriginalDamMonitorRenderVertex *dynamic =
            &snapshot.vertices[quad_indices[index]];
        assert(vertices[index].source.texture_s == dynamic->s
               && vertices[index].source.texture_t == dynamic->t
               && vertices[index].source.red == dynamic->red
               && vertices[index].source.green == dynamic->green
               && vertices[index].source.blue == dynamic->blue
               && vertices[index].source.alpha == dynamic->alpha);
        assert(vertices[index].processed.texture[0] == (float)dynamic->s
               && vertices[index].processed.texture[1] == (float)dynamic->t
               && vertices[index].processed.rgba[0] == dynamic->red
               && vertices[index].processed.rgba[1] == dynamic->green
               && vertices[index].processed.rgba[2] == dynamic->blue
               && vertices[index].processed.rgba[3] == dynamic->alpha);
        assert(vertices[index].processed.object[0] == 100.0f + (float)index
               && vertices[index].processed.eye[1] == 200.0f + (float)index
               && vertices[index].processed.clip[2] == 300.0f + (float)index
               && vertices[index].world[0] == 400.0f + (float)index
               && vertices[index].world[1] == 500.0f + (float)index
               && vertices[index].world[2] == 600.0f + (float)index);
    }
    assert(batches[0].texture.texture_id == snapshot.texture_id
           && batches[0].texture.min_level == snapshot.level
           && batches[0].texture.clamp_mirror_s == snapshot.flags_s
           && batches[0].texture.clamp_mirror_t == snapshot.flags_t
           && batches[0].texture_valid == 1U);
    assert(batches[0].material.texture_id == snapshot.texture_id
           && batches[0].material.texture_source
                == GE_PICA_TEXTURE_SOURCE_RARE_ID
           && batches[0].material.texture_enabled == 1U
           && batches[0].material.texture_image_width == snapshot.width
           && batches[0].material.texture_image_format == snapshot.format
           && batches[0].material.texture_image_size == snapshot.depth
           && batches[0].material.texture_min_level == snapshot.level
           && batches[0].material.texture_scale_s == UINT16_MAX
           && batches[0].material.texture_scale_t == UINT16_MAX
           && batches[0].material.wrap_s == GE_PICA_WRAP_CLAMP
           && batches[0].material.wrap_t == GE_PICA_WRAP_MIRROR
           && batches[0].material.color_combine
                == GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE
           && batches[0].material.alpha_combine == GE_PICA_ALPHA_SHADE
           && batches[0].material.cull_mode == GE_PICA_CULL_BACK
           && batches[0].material.cycle_type == GE_PICA_CYCLE_ONE
           && batches[0].material.alpha_test == GE_PICA_ALPHA_TEST_DISABLED
           && batches[0].material.blend_enabled == 1U
           && batches[0].material.depth_test_enabled == 1U
           && batches[0].material.depth_write_enabled == 0U
           && batches[0].material.depth_mode == GE_PICA_DEPTH_DECAL
           && batches[0].material.min_filter == GE_PICA_FILTER_LINEAR
           && batches[0].material.mag_filter == GE_PICA_FILTER_NEAREST
           && batches[0].material.texture_type == 0U
           && batches[0].material.fallback_flags == 0U);

    snapshot.format = UINT8_C(3);
    snapshot.texture_mode = UINT8_C(1);
    snapshot.texture_alpha_mode = UINT8_C(1);
    assert(ge_original_stage_monitor_surface_apply(
        &snapshot, &scene, 0U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK);
    assert(batches[0].material.alpha_combine
                == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE
           && batches[0].material.blend_enabled == 0U
           && batches[0].material.depth_mode == GE_PICA_DEPTH_OPAQUE
           && batches[0].material.depth_test_enabled == 1U
           && batches[0].material.depth_write_enabled == 1U);
}

static void test_rejection_is_atomic(void)
{
    GeOriginalDamMonitorRenderSnapshot snapshot = make_snapshot();
    GeDamRoomWorldVertex vertices[12];
    GeDamRoomWorldVertex before_vertices[12];
    GeDamRoomDrawBatch batches[2];
    GeDamRoomDrawBatch before_batches[2];
    GeDamRoomSceneStorage scene = {vertices, 12U, batches, 2U};
    GeOriginalStageMonitorSurfaceResult result;

    initialize_quad(vertices, &snapshot);
    initialize_quad(vertices + 6U, &snapshot);
    initialize_batch(&batches[0], 0U);
    initialize_batch(&batches[1], 6U);
    memcpy(before_vertices, vertices, sizeof(vertices));
    memcpy(before_batches, batches, sizeof(batches));
    assert(ge_original_stage_monitor_surface_apply_part(
        &snapshot, &scene, 0U, 2U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH);
    assert(memcmp(vertices, before_vertices, sizeof(vertices)) == 0
           && memcmp(batches, before_batches, sizeof(batches)) == 0);

    vertices[1].source.x++;
    memcpy(before_vertices, vertices, sizeof(vertices));
    memcpy(before_batches, batches, sizeof(batches));
    assert(ge_original_stage_monitor_surface_apply(
        &snapshot, &scene, 0U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH);
    assert(memcmp(vertices, before_vertices, sizeof(vertices)) == 0
           && memcmp(batches, before_batches, sizeof(batches)) == 0);

    snapshot.texture_id = UINT32_C(65536);
    assert(ge_original_stage_monitor_surface_apply(
        &snapshot, &scene, 1U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT);
    snapshot.texture_id = UINT32_C(2224);
    snapshot.switch_node = NULL;
    assert(ge_original_stage_monitor_surface_apply(
        &snapshot, &scene, 1U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT);
    assert(ge_original_stage_monitor_surface_apply(
        NULL, &scene, 1U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT);
    assert(ge_original_stage_monitor_surface_apply(
        &snapshot, &scene, 2U, &result)
        == GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT);
}

static void test_unchanged_publication_detection(void)
{
    GeOriginalDamMonitorRenderSnapshot first = make_snapshot();
    GeOriginalDamMonitorRenderSnapshot next = first;
    assert(ge_original_stage_monitor_surface_output_equal(&first, &next));
    next.pause60++;
    assert(ge_original_stage_monitor_surface_output_equal(&first, &next));
    next.vertices[2].s++;
    assert(!ge_original_stage_monitor_surface_output_equal(&first, &next));
    next = first;
    next.texture_id++;
    assert(!ge_original_stage_monitor_surface_output_equal(&first, &next));
    assert(!ge_original_stage_monitor_surface_output_equal(NULL, &next));
}

int main(void)
{
    test_exact_publication();
    test_rejection_is_atomic();
    test_unchanged_publication_detection();
    assert(strcmp(ge_original_stage_monitor_surface_status_name(
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK), "ok") == 0);
    assert(strcmp(ge_original_stage_monitor_surface_status_name(
        GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH),
        "target topology mismatch") == 0);
    puts("canonical monitor surface publication passed");
    return 0;
}
