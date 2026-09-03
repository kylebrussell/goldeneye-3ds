#include "ge_original_model_scene.h"
#include "ge_original_model62_runtime.h"
#include "ge_original_model104_runtime.h"
#include "ge_original_model178_runtime.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint8_t *load_blob(const char *path, size_t expected_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *blob;
    if (file == NULL) return NULL;
    blob = malloc(expected_size);
    assert(blob != NULL);
    assert(fread(blob, 1U, expected_size, file) == expected_size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    return blob;
}

static void set_transform(GeOriginalModelSceneInput *input)
{
    memset(input->matrix, 0, sizeof(input->matrix));
    input->matrix[0][0] = 2.0f;
    input->matrix[1][1] = 3.0f;
    input->matrix[2][2] = 4.0f;
    input->matrix[3][3] = 1.0f;
    input->position[0] = 10.0f;
    input->position[1] = 20.0f;
    input->position[2] = 30.0f;
    input->room_id = 135U;
}

static void assert_cached_vertices_match(
    const GeDamRoomWorldVertex *cached,
    const GeDamRoomWorldVertex *canonical, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        size_t axis;
        assert(memcmp(&cached[index].source, &canonical[index].source,
                      sizeof(cached[index].source)) == 0);
        assert(memcmp(cached[index].processed.rgba,
                      canonical[index].processed.rgba, 4U) == 0);
        assert(cached[index].processed.texture[0]
                    == canonical[index].processed.texture[0]
               && cached[index].processed.texture[1]
                    == canonical[index].processed.texture[1]);
        for (axis = 0U; axis < 3U; ++axis)
            assert(fabsf(cached[index].world[axis]
                         - canonical[index].world[axis]) < 0.0001f);
    }
}

static void assert_cached_transform_matches_previous_byte_exact(
    const GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *input,
    const GeDamRoomWorldVertex *published, size_t count)
{
    const float (*matrices)[4][4] = cache->quantized_matrices
        + cache->input_quantized_matrix_offsets[0];
    GeOriginalModelSceneTemplateView view;
    assert(ge_original_model_scene_cache_template_view(cache, 0U, &view));
    size_t index;
    for (index = 0U; index < count; ++index) {
        const GeDamRoomWorldVertex *source = &view.vertices[index];
        const uint16_t matrix_index = view.matrix_indices[index];
        const float object[4] = {
            (float)source->source.x, (float)source->source.y,
            (float)source->source.z, 1.0f
        };
        float transformed[4];
        float world[3];
        size_t axis;
        size_t row;
        for (axis = 0U; axis < 4U; ++axis) {
            transformed[axis] = 0.0f;
            for (row = 0U; row < 4U; ++row)
                transformed[axis] += object[row]
                    * matrices[matrix_index][row][axis];
        }
        for (axis = 0U; axis < 3U; ++axis) {
            if (input->segment3_matrices != NULL) {
                world[axis] = input->position[axis];
                for (row = 0U; row < 4U; ++row)
                    world[axis] += transformed[row]
                        * input->matrix[row][axis];
            } else {
                world[axis] = input->position[axis]
                    + (float)source->source.x * input->matrix[0][axis]
                    + (float)source->source.y * input->matrix[1][axis]
                    + (float)source->source.z * input->matrix[2][axis];
            }
        }
        assert(memcmp(transformed, published[index].processed.eye,
                      sizeof(transformed)) == 0);
        assert(memcmp(world, published[index].world, sizeof(world)) == 0);
    }
}

static uint64_t scene_hash(const GeDamRoomWorldVertex *vertices,
                           size_t vertex_count,
                           const GeDamRoomDrawBatch *batches,
                           size_t batch_count)
{
    const uint8_t *parts[2] = {
        (const uint8_t *)vertices, (const uint8_t *)batches
    };
    const size_t sizes[2] = {
        vertex_count * sizeof(*vertices), batch_count * sizeof(*batches)
    };
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t part;
    for (part = 0U; part < 2U; ++part) {
        size_t byte;
        for (byte = 0U; byte < sizes[part]; ++byte) {
            hash ^= parts[part][byte];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static uint64_t profile_clock(void *context)
{
    uint64_t *tick = context;
    return ++*tick;
}

static void exercise_preflighted(
    const GeOriginalModelSceneInput *input,
    const GeOriginalModelScene *query,
    const GeDamRoomSceneStorage *reference,
    const GeDamRoomSceneStorage *storage)
{
    GeOriginalModelScene scene;
    GeOriginalModelScene stale;
    GeOriginalModelSceneInput invalid = *input;
    GeDamRoomSceneStorage small = *storage;
    const GeDamRoomSceneStorage empty = {NULL, 0U, NULL, 0U};
    const size_t vertex_bytes = query->required_vertex_count
        * sizeof(*storage->vertices);
    const size_t batch_bytes = query->required_batch_count
        * sizeof(*storage->batches);
    clock_t start;
    clock_t regular_ticks;
    clock_t preflighted_ticks;
    size_t iteration;

    assert(ge_original_model_scene_build_preflighted(
        input, query, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(scene.vertex_count == query->required_vertex_count
           && scene.batch_count == query->required_batch_count);
    assert(memcmp(storage->vertices, reference->vertices, vertex_bytes) == 0);
    assert(memcmp(storage->batches, reference->batches, batch_bytes) == 0);
    scene = *query;
    assert(ge_original_model_scene_build_preflighted(
        input, &scene, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    /* A published scene is also a valid preflight; failed queries are not. */
    assert(ge_original_model_scene_build_preflighted(
        input, &scene, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(ge_original_model_scene_build_preflighted(
        input, NULL, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    stale = *query;
    stale.status = GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR;
    assert(ge_original_model_scene_build_preflighted(
        input, &stale, storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    stale = *query;
    stale.list_count++;
    assert(ge_original_model_scene_build_preflighted(
        input, &stale, storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    stale = *query;
    stale.triangle_count = SIZE_MAX;
    assert(ge_original_model_scene_build_preflighted(
        input, &stale, storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    /* A stale command count cannot publish an otherwise valid decode. */
    stale = *query;
    stale.commands_visited++;
    assert(ge_original_model_scene_build_preflighted(
        input, &stale, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    small.vertex_capacity--;
    assert(ge_original_model_scene_build_preflighted(
        input, query, &small, &scene) == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    /* Even a bogus zero-sized query cannot bypass the write callback's
     * bounds checks. No output pointers are supplied to this traversal. */
    stale = *query;
    stale.required_vertex_count = 0U;
    stale.required_batch_count = 0U;
    stale.triangle_count = 0U;
    assert(ge_original_model_scene_build_preflighted(
        input, &stale, &empty, &scene) == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    invalid.primary_offset = (uint32_t)input->blob_size;
    assert(ge_original_model_scene_build_preflighted(
        &invalid, query, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    start = clock();
    for (iteration = 0U; iteration < 16U; ++iteration)
        assert(ge_original_model_scene_build(input, storage, &scene)
            == GE_ORIGINAL_MODEL_SCENE_OK);
    regular_ticks = clock() - start;
    start = clock();
    for (iteration = 0U; iteration < 16U; ++iteration)
        assert(ge_original_model_scene_build_preflighted(
            input, query, storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    preflighted_ticks = clock() - start;
    assert(memcmp(storage->vertices, reference->vertices, vertex_bytes) == 0);
    assert(memcmp(storage->batches, reference->batches, batch_bytes) == 0);
    printf("preflighted model: %zu vertices, regular=%.3f us, preflighted=%.3f us; "
           "byte-exact output and failure bounds verified\n",
           scene.vertex_count,
           (double)regular_ticks * 1000000.0 / (16.0 * CLOCKS_PER_SEC),
           (double)preflighted_ticks * 1000000.0 / (16.0 * CLOCKS_PER_SEC));
}

static void exercise_preflighted_matrix_template(
    const GeOriginalModelSceneInput *input,
    const GeOriginalModelScene *query,
    const GeDamRoomSceneStorage *reference,
    const GeDamRoomSceneStorage *storage)
{
    GeOriginalModelScene scene;
    GeOriginalModelScene stale = *query;
    const size_t count = query->required_vertex_count;
    uint16_t *expected = calloc(count, sizeof(*expected));
    uint16_t *actual = calloc(count, sizeof(*actual));
    clock_t start;
    clock_t regular_ticks;
    clock_t preflighted_ticks;
    size_t iteration;
    assert(count != 0U && expected != NULL && actual != NULL);
    start = clock();
    for (iteration = 0U; iteration < 16U; ++iteration)
        assert(ge_original_model_scene_build_matrix_template(
            input, reference, expected, count, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    regular_ticks = clock() - start;
    start = clock();
    for (iteration = 0U; iteration < 16U; ++iteration)
        assert(ge_original_model_scene_build_matrix_template_preflighted(
            input, query, storage, actual, count, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    preflighted_ticks = clock() - start;
    assert(memcmp(expected, actual, count * sizeof(*actual)) == 0);
    assert(memcmp(storage->vertices, reference->vertices,
        count * sizeof(*storage->vertices)) == 0);
    assert(memcmp(storage->batches, reference->batches,
        query->required_batch_count * sizeof(*storage->batches)) == 0);
    assert(ge_original_model_scene_build_matrix_template_preflighted(
        input, query, storage, actual, count - 1U, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    assert(ge_original_model_scene_build_matrix_template_preflighted(
        input, NULL, storage, actual, count, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    assert(ge_original_model_scene_build_matrix_template_preflighted(
        input, query, storage, NULL, count, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    stale.commands_visited++;
    assert(ge_original_model_scene_build_matrix_template_preflighted(
        input, &stale, storage, actual, count, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    scene = *query;
    assert(ge_original_model_scene_build_matrix_template_preflighted(
        input, &scene, storage, actual, count, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    printf("preflighted matrix template: %zu vertices, regular=%.3f us, "
           "preflighted=%.3f us; exact vertices/materials/matrix indices and bounds\n",
        count, (double)regular_ticks * 1000000.0 / (16.0 * CLOCKS_PER_SEC),
        (double)preflighted_ticks * 1000000.0 / (16.0 * CLOCKS_PER_SEC));
    free(actual);
    free(expected);
    /* Restore transformed output for the existing cache equivalence checks. */
    assert(ge_original_model_scene_build(input, reference, &scene)
        == GE_ORIGINAL_MODEL_SCENE_OK);
}

static void exercise(const char *path, size_t blob_size,
                     uint32_t primary, uint32_t secondary,
                     uint32_t segment4_offset)
{
    uint8_t *blob = load_blob(path, blob_size);
    GeOriginalModelSceneInput input;
    GeOriginalModelScene query;
    GeOriginalModelScene built;
    GeDamRoomSceneStorage storage;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomWorldVertex *cached_vertices;
    GeDamRoomDrawBatch *batches;
    GeDamRoomDrawBatch *cached_batches;
    GeOriginalModelSceneCache cache = {0};
    GeOriginalModelScene cached;
    uint64_t profile_tick = 0U;
    size_t index;

    assert(blob != NULL);
    memset(&input, 0, sizeof(input));
    input.blob = blob;
    input.blob_size = blob_size;
    input.primary_offset = primary;
    input.secondary_offset = secondary;
    input.segment4_offset = segment4_offset;
    input.world_zbuffer_enabled = 1U;
    set_transform(&input);
    {
        GeOriginalModelSceneStatus query_status =
            ge_original_model_scene_build(&input, NULL, &query);
        if (query_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
            fprintf(stderr, "model scene %s query failed: %s\n", path,
                    ge_original_model_scene_status_name(query_status));
        }
        assert(query_status == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    }
    assert(query.list_count >= 1U);
    assert(query.required_vertex_count != 0U);
    assert(query.required_batch_count != 0U);
    assert(query.triangle_count * 3U == query.required_vertex_count);
    vertices = calloc(query.required_vertex_count, sizeof(*vertices));
    batches = calloc(query.required_batch_count, sizeof(*batches));
    cached_vertices = calloc(
        query.required_vertex_count, sizeof(*cached_vertices));
    cached_batches = calloc(query.required_batch_count, sizeof(*cached_batches));
    assert(vertices != NULL && batches != NULL
           && cached_vertices != NULL && cached_batches != NULL);
    ge_original_model_scene_cache_bind_profile_clock(
        &cache, profile_clock, &profile_tick);
    storage = (GeDamRoomSceneStorage){
        vertices, query.required_vertex_count,
        batches, query.required_batch_count
    };
    assert(ge_original_model_scene_build(&input, &storage, &built)
           == GE_ORIGINAL_MODEL_SCENE_OK);
    {
        GeDamRoomSceneStorage cached_storage = {
            cached_vertices, query.required_vertex_count,
            cached_batches, query.required_batch_count
        };
        exercise_preflighted(&input, &query, &storage, &cached_storage);
        exercise_preflighted_matrix_template(&input, &query, &storage, &cached_storage);
        assert(ge_original_model_scene_cache_build(
            &cache, &input, 1U, &cached_storage, &cached)
            == GE_ORIGINAL_MODEL_SCENE_OK);
        assert_cached_vertices_match(
            cached_vertices, vertices, built.vertex_count);
        assert_cached_transform_matches_previous_byte_exact(
            &cache, &input, cached_vertices, built.vertex_count);
        assert(cached.vertex_count == built.vertex_count
               && cached.batch_count == built.batch_count
               && memcmp(cached_batches, batches,
                         built.batch_count * sizeof(*batches)) == 0
               && cache.topology_rebuilds == 1U
               && cache.topology_transform_maps_built == 1U
               && cache.topology_transform_map_vertices_reused
                    == built.vertex_count
               && cache.cached_builds == 1U);
        /* Flattening N64 triangle indices duplicates authored points.  The
         * cache must publish byte-identical vertices while transforming each
         * point/matrix pair once per changed publication. */
        assert(cache.duplicate_vertex_transforms_avoided > 0U);
        {
            const uint64_t retained_hash = scene_hash(
                cached_vertices, cached.vertex_count,
                cached_batches, cached.batch_count);
            assert(ge_original_model_scene_cache_build(
                &cache, &input, 1U, &cached_storage, &cached)
                == GE_ORIGINAL_MODEL_SCENE_OK);
            assert(cache.cached_builds == 1U
                   && cache.unchanged_builds == 1U
                   && retained_hash == scene_hash(
                       cached_vertices, cached.vertex_count,
                       cached_batches, cached.batch_count));
        }
        input.position[0] += 17.0f;
        input.room_id = 42U;
        memset(vertices, 0, query.required_vertex_count * sizeof(*vertices));
        memset(batches, 0, query.required_batch_count * sizeof(*batches));
        assert(ge_original_model_scene_build(&input, &storage, &built)
               == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(ge_original_model_scene_cache_build(
            &cache, &input, 1U, &cached_storage, &cached)
            == GE_ORIGINAL_MODEL_SCENE_OK);
        assert_cached_vertices_match(
            cached_vertices, vertices, built.vertex_count);
        assert_cached_transform_matches_previous_byte_exact(
            &cache, &input, cached_vertices, built.vertex_count);
        assert(memcmp(cached_batches, batches,
                      built.batch_count * sizeof(*batches)) == 0
               && cache.topology_rebuilds == 1U
               && cache.cached_builds == 2U
               && cache.static_vertex_copies_avoided
                    == built.vertex_count
               && cache.static_batch_copies_avoided
                    == built.batch_count);
        if (input.secondary_offset != GE_ORIGINAL_MODEL_SCENE_NO_LIST) {
            const uint32_t saved_secondary = input.secondary_offset;
            input.secondary_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            memset(vertices, 0, query.required_vertex_count * sizeof(*vertices));
            memset(batches, 0, query.required_batch_count * sizeof(*batches));
            assert(ge_original_model_scene_build(&input, &storage, &built)
                   == GE_ORIGINAL_MODEL_SCENE_OK);
            assert(ge_original_model_scene_cache_build(
                &cache, &input, 1U, &cached_storage, &cached)
                == GE_ORIGINAL_MODEL_SCENE_OK);
            assert_cached_vertices_match(
                cached_vertices, vertices, built.vertex_count);
            assert(memcmp(cached_batches, batches,
                          built.batch_count * sizeof(*batches)) == 0
                   && cache.topology_rebuilds == 2U
                   && cache.topology_transform_maps_built == 2U
                   && cache.cached_builds == 3U);
            input.secondary_offset = saved_secondary;
        }
        input.position[0] -= 17.0f;
        input.room_id = 135U;
        assert(ge_original_model_scene_build(&input, &storage, &built)
               == GE_ORIGINAL_MODEL_SCENE_OK);
        {
            const uint64_t rebuilds = cache.topology_rebuilds;
            input.world_zbuffer_enabled = 0U;
            assert(ge_original_model_scene_cache_build(
                &cache, &input, 1U, &cached_storage, &cached)
                == GE_ORIGINAL_MODEL_SCENE_OK
                && cache.topology_rebuilds == rebuilds + 1U);
            input.world_zbuffer_enabled = 1U;
            assert(ge_original_model_scene_cache_build(
                &cache, &input, 1U, &cached_storage, &cached)
                == GE_ORIGINAL_MODEL_SCENE_OK
                && cache.topology_rebuilds == rebuilds + 1U
                && cache.topology_variant_hits >= 1U);
        }
        memset(input.matrix, 0, sizeof(input.matrix));
        input.matrix[0][0] = 1.0f;
        input.matrix[1][1] = 1.0f;
        input.matrix[2][2] = 1.0f;
        input.matrix[3][3] = 1.0f;
        memset(input.position, 0, sizeof(input.position));
        assert(ge_original_model_scene_cache_build(
            &cache, &input, 1U, &cached_storage, &cached)
            == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(cache.identity_outer_vertices_published
            == cached.vertex_count);
        for (index = 0U; index < cached.vertex_count; ++index) {
            size_t axis;
            for (axis = 0U; axis < 3U; ++axis)
                assert(cached_vertices[index].world[axis]
                    == cached_vertices[index].processed.eye[axis]);
        }
        assert(cache.profile_build_calls > 0U
               && cache.profile_build_ticks > 0U
               && cache.profile_topology_ticks > 0U
               && cache.profile_publication_signature_ticks > 0U
               && cache.profile_matrix_quantization_ticks > 0U
               && cache.profile_vertex_transform_ticks > 0U
               && cache.profile_batch_publication_ticks > 0U);
    }
    assert(built.vertex_count == query.required_vertex_count);
    assert(built.batch_count == query.required_batch_count);
    assert(built.commands_visited == query.commands_visited);
    for (index = 0U; index < built.vertex_count; ++index) {
        assert(isfinite(vertices[index].world[0]));
        assert(isfinite(vertices[index].world[1]));
        assert(isfinite(vertices[index].world[2]));
        assert(fabsf(vertices[index].world[0]
                - (10.0f + 2.0f * (float)vertices[index].source.x))
               < 0.0001f);
        assert(fabsf(vertices[index].world[1]
                - (20.0f + 3.0f * (float)vertices[index].source.y))
               < 0.0001f);
        assert(fabsf(vertices[index].world[2]
                - (30.0f + 4.0f * (float)vertices[index].source.z))
               < 0.0001f);
    }
    for (index = 0U; index < built.batch_count; ++index) {
        assert(batches[index].room_id == 135U);
        assert(batches[index].coordinate_space
               == GE_DAM_ROOM_COORDINATE_RUNTIME);
        assert(batches[index].vertex_count != 0U);
        assert(batches[index].texture_valid != 0U);
        assert(batches[index].material.depth_test_enabled == 1U);
        assert(batches[index].material.depth_write_enabled
               == (batches[index].list_kind
                    == GE_DAM_ROOM_LIST_PRIMARY));
    }
    printf("model scene %s: %zu list, %zu cmd, %zu tri, %zu batch, "
           "%llu duplicate transforms avoided, %llu component maps/"
           "%llu aggregate vertices reused\n",
           path, built.list_count, built.commands_visited,
           built.triangle_count, built.batch_count,
           (unsigned long long)cache.duplicate_vertex_transforms_avoided,
           (unsigned long long)cache.topology_transform_maps_built,
           (unsigned long long)cache.topology_transform_map_vertices_reused);
    ge_original_model_scene_cache_close(&cache);
    free(cached_batches);
    free(cached_vertices);
    free(batches);
    free(vertices);
    free(blob);
}

static void exercise_component_map_reuse(const char *path)
{
    uint8_t *blob = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
    GeOriginalModelSceneInput inputs[2];
    GeOriginalModelScene scene;
    GeOriginalModelSceneCache cache = {0};
    uint64_t first_vertices;
    uint64_t second_vertices;

    assert(blob != NULL);
    memset(inputs, 0, sizeof(inputs));
    inputs[0].blob = blob;
    inputs[0].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
    inputs[0].primary_offset = UINT32_C(0x5c8);
    inputs[0].secondary_offset = UINT32_C(0x6b8);
    inputs[0].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[0].world_zbuffer_enabled = 1U;
    inputs[1] = inputs[0];
    inputs[1].secondary_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;

    assert(ge_original_model_scene_cache_build(
        &cache, &inputs[0], 1U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    first_vertices = scene.required_vertex_count;
    assert(cache.topology_transform_maps_built == 1U);
    assert(ge_original_model_scene_cache_build(
        &cache, &inputs[1], 1U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    second_vertices = scene.required_vertex_count;
    assert(cache.topology_transform_maps_built == 2U);

    /* This aggregate topology has not occurred before, but both immutable
     * model components have. A combat visibility change must compose their
     * retained maps rather than rebuilding either vertex hash. */
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 2U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(cache.topology_rebuilds == 3U
           && cache.topology_component_misses == 2U
           && cache.topology_component_hits == 2U
           && cache.topology_transform_maps_built == 2U
           && cache.topology_transform_map_vertices_reused
                == first_vertices * 2U + second_vertices * 2U);
    printf("component topology reuse: 3 aggregate rebuilds, %llu immutable "
           "maps built, %llu component hits\n",
           (unsigned long long)cache.topology_transform_maps_built,
           (unsigned long long)cache.topology_component_hits);
    ge_original_model_scene_cache_close(&cache);
    free(blob);
}

static void exercise_combat_topology_working_set(const char *path)
{
    enum { WORKING_SET = 6 };
    uint8_t *blob = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
    GeOriginalModelSceneInput inputs[WORKING_SET];
    GeOriginalModelScene scene;
    GeOriginalModelSceneCache cache = {0};
    uint64_t rebuilds;
    size_t count;

    assert(blob != NULL);
    memset(inputs, 0, sizeof(inputs));
    for (count = 0U; count < WORKING_SET; ++count) {
        inputs[count].blob = blob;
        inputs[count].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
        inputs[count].primary_offset = UINT32_C(0x5c8);
        inputs[count].secondary_offset = UINT32_C(0x6b8);
        inputs[count].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
        inputs[count].world_zbuffer_enabled = 1U;
    }
    for (count = 1U; count <= WORKING_SET; ++count)
        assert(ge_original_model_scene_cache_build(
            &cache, inputs, count, NULL, &scene)
            == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    rebuilds = cache.topology_rebuilds;
    assert(rebuilds == WORKING_SET);
    for (count = 1U; count <= WORKING_SET; ++count)
        assert(ge_original_model_scene_cache_build(
            &cache, inputs, count, NULL, &scene)
            == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(cache.topology_rebuilds == rebuilds
        && cache.topology_variant_hits == WORKING_SET
        && cache.topology_variant_evictions == 0U);
    printf("combat topology working set: %u aggregate states retained, "
           "second cycle %llu hit/%llu eviction\n",
           WORKING_SET,
           (unsigned long long)cache.topology_variant_hits,
           (unsigned long long)cache.topology_variant_evictions);
    ge_original_model_scene_cache_close(&cache);
    free(blob);
}

static void exercise_unchanged_input_publication_reuse(const char *path)
{
    uint8_t *blob = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
    GeOriginalModelSceneInput inputs[2];
    GeOriginalModelScene query;
    GeOriginalModelScene scene;
    GeOriginalModelSceneCache cache = {0};
    GeDamRoomWorldVertex *vertices;
    GeDamRoomWorldVertex *first_snapshot;
    GeDamRoomWorldVertex *direct_vertices;
    GeDamRoomDrawBatch *batches;
    GeDamRoomDrawBatch *first_batch_snapshot;
    GeDamRoomDrawBatch *direct_batches;
    GeDamRoomSceneStorage storage;
    GeDamRoomSceneStorage direct_storage;

    assert(blob != NULL);
    memset(inputs, 0, sizeof(inputs));
    inputs[0].blob = blob;
    inputs[0].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
    inputs[0].primary_offset = UINT32_C(0x5c8);
    inputs[0].secondary_offset = UINT32_C(0x6b8);
    inputs[0].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[0].room_id = 135U;
    inputs[0].world_zbuffer_enabled = 1U;
    inputs[0].matrix[0][0] = 1.0f;
    inputs[0].matrix[1][1] = 1.0f;
    inputs[0].matrix[2][2] = 1.0f;
    inputs[0].matrix[3][3] = 1.0f;
    inputs[1] = inputs[0];
    inputs[1].position[0] = 500.0f;
    assert(ge_original_model_scene_build(&inputs[0], NULL, &query)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    vertices = calloc(query.required_vertex_count * 2U, sizeof(*vertices));
    batches = calloc(query.required_batch_count * 2U, sizeof(*batches));
    first_snapshot = malloc(
        query.required_vertex_count * sizeof(*first_snapshot));
    first_batch_snapshot = malloc(
        query.required_batch_count * sizeof(*first_batch_snapshot));
    direct_vertices = calloc(
        query.required_vertex_count * 2U, sizeof(*direct_vertices));
    direct_batches = calloc(
        query.required_batch_count * 2U, sizeof(*direct_batches));
    assert(vertices != NULL && batches != NULL && first_snapshot != NULL
        && first_batch_snapshot != NULL && direct_vertices != NULL
        && direct_batches != NULL);
    storage = (GeDamRoomSceneStorage){
        vertices, query.required_vertex_count * 2U,
        batches, query.required_batch_count * 2U,
    };
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 2U, &storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    memcpy(first_snapshot, vertices,
        query.required_vertex_count * sizeof(*first_snapshot));
    memcpy(first_batch_snapshot, batches,
        query.required_batch_count * sizeof(*first_batch_snapshot));

    inputs[1].position[0] += 17.0f;
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 2U, &storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(memcmp(first_snapshot, vertices,
        query.required_vertex_count * sizeof(*first_snapshot)) == 0);
    assert(memcmp(first_batch_snapshot, batches,
        query.required_batch_count * sizeof(*first_batch_snapshot)) == 0);
    assert(cache.unchanged_input_publications == 1U);
    assert(cache.unchanged_input_vertices_avoided
        == query.required_vertex_count);
    assert(cache.unchanged_input_batches_avoided
        == query.required_batch_count);

    /* Publishing the same canonical inputs into a different output buffer
     * forces a full cache publication. It must be byte-identical to the
     * selective same-buffer result above. */
    direct_storage = (GeDamRoomSceneStorage){
        direct_vertices, query.required_vertex_count * 2U,
        direct_batches, query.required_batch_count * 2U,
    };
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 2U, &direct_storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(memcmp(direct_vertices, vertices,
        query.required_vertex_count * 2U * sizeof(*direct_vertices)) == 0);
    assert(memcmp(direct_batches, batches,
        query.required_batch_count * 2U * sizeof(*direct_batches)) == 0);
    {
        const size_t iterations = 20000U;
        const uint64_t skipped_before =
            cache.unchanged_input_publications;
        clock_t selective_start;
        clock_t selective_end;
        clock_t forced_start;
        clock_t forced_end;
        size_t iteration;

        /* Restore the fixed output once, then alternate only the peer input
         * while keeping that renderer-owned output buffer resident. */
        assert(ge_original_model_scene_cache_build(
            &cache, inputs, 2U, &storage, &scene)
            == GE_ORIGINAL_MODEL_SCENE_OK);
        selective_start = clock();
        for (iteration = 0U; iteration < iterations; ++iteration) {
            inputs[1].position[0] += (iteration & 1U) != 0U ? 1.0f : -1.0f;
            assert(ge_original_model_scene_cache_build(
                &cache, inputs, 2U, &storage, &scene)
                == GE_ORIGINAL_MODEL_SCENE_OK);
        }
        selective_end = clock();
        assert(cache.unchanged_input_publications - skipped_before
            == iterations);

        /* Alternating renderer buffers disables same-storage retention and
         * measures the byte-identical full-publication baseline. */
        forced_start = clock();
        for (iteration = 0U; iteration < iterations; ++iteration) {
            GeDamRoomSceneStorage *forced_storage =
                (iteration & 1U) != 0U ? &storage : &direct_storage;
            inputs[1].position[0] += (iteration & 1U) != 0U ? 1.0f : -1.0f;
            assert(ge_original_model_scene_cache_build(
                &cache, inputs, 2U, forced_storage, &scene)
                == GE_ORIGINAL_MODEL_SCENE_OK);
        }
        forced_end = clock();
        printf("per-input benchmark: %zu changed-peer builds, %.3f ms "
               "selective vs %.3f ms full publication\n",
               iterations,
               1000.0 * (double)(selective_end - selective_start)
                   / (double)CLOCKS_PER_SEC,
               1000.0 * (double)(forced_end - forced_start)
                   / (double)CLOCKS_PER_SEC);
    }
    printf("per-input publication reuse: %llu vertices/%llu batches "
           "retained byte-exact while peer input changed\n",
           (unsigned long long)cache.unchanged_input_vertices_avoided,
           (unsigned long long)cache.unchanged_input_batches_avoided);
    ge_original_model_scene_cache_close(&cache);
    free(direct_batches);
    free(direct_vertices);
    free(first_batch_snapshot);
    free(first_snapshot);
    free(batches);
    free(vertices);
    free(blob);
}

static void exercise_dirty_publication_ranges(const char *path)
{
    uint8_t *blob = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
    GeOriginalModelSceneInput inputs[4] = {{0}};
    GeOriginalModelSceneCache cache = {0};
    GeOriginalModelSceneCache reference = {0};
    GeOriginalModelScene scene;
    GeDamRoomSceneStorage storage;
    GeDamRoomSceneStorage direct;
    size_t vertices_per_input;
    size_t batches_per_input;
    size_t index;
    assert(blob != NULL);
    inputs[0].blob = blob;
    inputs[0].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
    inputs[0].primary_offset = UINT32_C(0x5c8);
    inputs[0].secondary_offset = UINT32_C(0x6b8);
    inputs[0].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[0].room_id = 135U;
    inputs[0].world_zbuffer_enabled = 1U;
    for (index = 0U; index < 4U; ++index)
        inputs[0].matrix[index][index] = 1.0f;
    for (index = 1U; index < 4U; ++index) inputs[index] = inputs[0];
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(cache.publication_range_count == 0U);
    vertices_per_input = scene.required_vertex_count / 4U;
    batches_per_input = scene.required_batch_count / 4U;
    storage = (GeDamRoomSceneStorage){
        calloc(scene.required_vertex_count, sizeof(*storage.vertices)),
        scene.required_vertex_count,
        calloc(scene.required_batch_count, sizeof(*storage.batches)),
        scene.required_batch_count,
    };
    direct = (GeDamRoomSceneStorage){
        calloc(storage.vertex_capacity, sizeof(*direct.vertices)),
        storage.vertex_capacity,
        calloc(storage.batch_capacity, sizeof(*direct.batches)),
        storage.batch_capacity,
    };
    assert(storage.vertices && storage.batches && direct.vertices && direct.batches);
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.publication_range_count == 1U);
    assert(cache.publication_ranges[0].vertex_offset == 0U);
    assert(cache.publication_ranges[0].vertex_count == storage.vertex_capacity);
    assert(cache.publication_ranges[0].static_data_changed == 1U);
    memcpy(direct.vertices, storage.vertices,
           storage.vertex_capacity * sizeof(*storage.vertices));
    memcpy(direct.batches, storage.batches,
           storage.batch_capacity * sizeof(*storage.batches));
    inputs[0].position[0] = 17.0f;
    inputs[2].position[0] = 23.0f;
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.publication_range_count == 2U);
    for (index = 0U; index < 2U; ++index) {
        const GeOriginalModelScenePublicationRange *range =
            &cache.publication_ranges[index];
        assert(range->vertex_offset == index * 2U * vertices_per_input);
        assert(range->vertex_count == vertices_per_input);
        assert(range->batch_offset == index * 2U * batches_per_input);
        assert(range->batch_count == batches_per_input);
        assert(range->static_data_changed == 0U);
        memcpy(direct.vertices + range->vertex_offset,
               storage.vertices + range->vertex_offset,
               range->vertex_count * sizeof(*storage.vertices));
        memcpy(direct.batches + range->batch_offset,
               storage.batches + range->batch_offset,
               range->batch_count * sizeof(*storage.batches));
    }
    /* Applying only advertised ranges reproduces the full CPU publication. */
    assert(memcmp(direct.vertices, storage.vertices,
        storage.vertex_capacity * sizeof(*storage.vertices)) == 0);
    assert(memcmp(direct.batches, storage.batches,
        storage.batch_capacity * sizeof(*storage.batches)) == 0);
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.publication_range_count == 0U);
    inputs[0].position[0] += 1.0f;
    inputs[1].position[0] += 1.0f;
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.publication_range_count == 1U);
    assert(cache.publication_ranges[0].vertex_count == 2U * vertices_per_input);
    /* Same-sized topology changes must republish immutable UV/material data. */
    inputs[0].world_zbuffer_enabled = 0U;
    assert(ge_original_model_scene_cache_build(
        &cache, inputs, 4U, &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.publication_range_count == 1U);
    assert(cache.publication_ranges[0].static_data_changed == 1U);
    assert(cache.publication_ranges[0].vertex_count == storage.vertex_capacity);
    assert(ge_original_model_scene_cache_build(
        &reference, inputs, 4U, &direct, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(memcmp(direct.vertices, storage.vertices,
        storage.vertex_capacity * sizeof(*storage.vertices)) == 0);
    assert(memcmp(direct.batches, storage.batches,
        storage.batch_capacity * sizeof(*storage.batches)) == 0);
    {
        float matrices[1][4][4] = {{{0}}};
        static const float edges[] = {
            -32768.0f, -32767.998046875f, 32767.998046875f,
            -1.00001f, 1.00001f, -0.00001f, 0.00001f, -0.0f, 0.0f,
        };
        uint32_t random_bits = UINT32_C(0x78563412);
        size_t sample;
        inputs[0].segment3_matrices = (const float (*)[4][4])matrices;
        inputs[0].segment3_matrix_count = 1U;
        for (sample = 0U; sample < 1024U; ++sample) {
            size_t element;
            for (element = 0U; element < 16U; ++element) {
                float value;
                random_bits ^= random_bits << 13;
                random_bits ^= random_bits >> 17;
                random_bits ^= random_bits << 5;
                /* Exercise signs, fractional truncation and wide exponents
                 * without ever stepping outside the validated s15.16 ABI. */
                value = (float)(int16_t)(random_bits & UINT32_C(0xffff))
                    / (float)(1U << ((random_bits >> 16) & 15U));
                if (sample < sizeof(edges) / sizeof(edges[0]))
                    value = edges[sample];
                matrices[0][element / 4U][element % 4U] = value;
            }
            assert(ge_original_model_scene_cache_build(
                &cache, inputs, 4U, &storage, &scene)
                == GE_ORIGINAL_MODEL_SCENE_OK);
            for (element = 0U; element < 16U; ++element) {
                const float value = matrices[0][element / 4U][element % 4U];
                const int64_t old_fixed = (int64_t)(value * 65536.0f);
                const float expected = (float)(int32_t)old_fixed / 65536.0f;
                const float actual = cache.quantized_matrices[
                    cache.input_quantized_matrix_offsets[0]]
                    [element / 4U][element % 4U];
                assert(memcmp(&expected, &actual, sizeof(expected)) == 0);
            }
        }
        matrices[0][0][0] = 32768.0f;
        assert(ge_original_model_scene_cache_build(
            &cache, inputs, 4U, &storage, &scene)
            == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
        assert(cache.publication_range_count == 0U);
        matrices[0][0][0] = NAN;
        assert(ge_original_model_scene_cache_build(
            &cache, inputs, 4U, &storage, &scene)
            == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
        assert(cache.publication_range_count == 0U);
    }
    assert(ge_original_model_scene_cache_build(
        &cache, NULL, 4U, &storage, &scene)
        == GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT);
    assert(cache.publication_range_count == 0U);
    ge_original_model_scene_cache_close(&reference);
    ge_original_model_scene_cache_close(&cache);
    free(direct.batches); free(direct.vertices);
    free(storage.batches); free(storage.vertices); free(blob);
    puts("dirty ranges: sparse/coalesced/unchanged/topology/failure verified");
}

static void exercise_shared_component_lifetime(const char *path)
{
    enum { COMPONENTS = 40, FRAMES = 120 };
    uint8_t *blobs[COMPONENTS];
    GeOriginalModelSceneInput inputs[COMPONENTS], ordered[COMPONENTS];
    GeOriginalModelSceneCache cache = {0}, reference = {0};
    GeOriginalModelScene scene, single;
    GeOriginalModelSceneTemplateView original, view;
    memset(inputs, 0, sizeof(inputs));
    for (size_t i = 0U; i < COMPONENTS; ++i) {
        blobs[i] = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
        assert(blobs[i]);
        inputs[i].blob = blobs[i];
        inputs[i].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
        inputs[i].primary_offset = UINT32_C(0x5c8);
        inputs[i].secondary_offset = i != 0U && i % 3U == 0U
            ? GE_ORIGINAL_MODEL_SCENE_NO_LIST : UINT32_C(0x6b8);
        inputs[i].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
        inputs[i].world_zbuffer_enabled = 1U;
        set_transform(&inputs[i]);
    }
    assert(!ge_original_model_scene_cache_template_view(&cache, 0U, &view));
    assert(ge_original_model_scene_cache_build(&cache, inputs, 1U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(ge_original_model_scene_cache_template_view(&cache, 0U, &original));
    const size_t nv = scene.required_vertex_count;
    const size_t nb = scene.required_batch_count;
    GeDamRoomSceneStorage storage = {
        calloc(COMPONENTS * nv, sizeof(*storage.vertices)), COMPONENTS * nv,
        calloc(COMPONENTS * nb, sizeof(*storage.batches)), COMPONENTS * nb
    };
    GeDamRoomSceneStorage expected = {
        calloc(nv, sizeof(*expected.vertices)), nv,
        calloc(nb, sizeof(*expected.batches)), nb
    };
    assert(storage.vertices && storage.batches && expected.vertices && expected.batches);
    /* Populate enough distinct ROM-backed components to grow the descriptor
     * array past 32, then swap/evict aggregates while reusing their payloads. */
    assert(ge_original_model_scene_cache_build(&cache, inputs, COMPONENTS,
        &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(cache.topology_component_count == COMPONENTS);
    assert(cache.topology_component_capacity >= COMPONENTS);
    assert(ge_original_model_scene_cache_template_view(&cache, 0U, &view));
    assert(view.vertices == original.vertices && view.batches == original.batches
        && view.matrix_indices == original.matrix_indices
        && view.transform_sources == original.transform_sources);
    for (size_t frame = 0U; frame < FRAMES; ++frame) {
        const size_t count = frame % (COMPONENTS + 1U);
        for (size_t i = 0U; i < count; ++i) {
            /* Repeated inputs share a payload but never their transformed
             * publication; reversed order exercises local->aggregate indices. */
            ordered[i] = inputs[(COMPONENTS - 1U - i + frame / 3U) % COMPONENTS];
            if (i == 1U) ordered[i] = ordered[0];
            ordered[i].position[0] += (float)(frame * 13U + i * 7U);
            ordered[i].room_id = (uint32_t)(i + 1U);
        }
        assert(ge_original_model_scene_cache_build(&cache, ordered, count,
            &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
        size_t vertex_cursor = 0U, batch_cursor = 0U;
        for (size_t i = 0U; i < count; ++i) {
            assert(ge_original_model_scene_cache_build(&reference, &ordered[i], 1U,
                &expected, &single) == GE_ORIGINAL_MODEL_SCENE_OK);
            assert(memcmp(storage.vertices + vertex_cursor, expected.vertices,
                single.vertex_count * sizeof(*expected.vertices)) == 0);
            for (size_t b = 0U; b < single.batch_count; ++b) {
                GeDamRoomDrawBatch batch = expected.batches[b];
                batch.first_vertex += vertex_cursor;
                assert(memcmp(&storage.batches[batch_cursor + b], &batch, sizeof(batch)) == 0);
            }
            vertex_cursor += single.vertex_count;
            batch_cursor += single.batch_count;
        }
        assert(scene.vertex_count == vertex_cursor && scene.batch_count == batch_cursor);
        if (count > 1U) {
            assert(ge_original_model_scene_cache_template_view(&cache, 0U, &view));
            GeOriginalModelSceneTemplateView repeated;
            assert(ge_original_model_scene_cache_template_view(&cache, 1U, &repeated));
            assert(view.vertices == repeated.vertices && view.batches == repeated.batches);
        }
        assert(!ge_original_model_scene_cache_template_view(&cache, count, &view));
    }
    assert(cache.topology_variant_evictions > 0U);
    assert(cache.topology_component_count == COMPONENTS);
    ge_original_model_scene_cache_close(&reference);
    ge_original_model_scene_cache_close(&cache);
    assert(!ge_original_model_scene_cache_template_view(&cache, 0U, &view));
    free(expected.vertices); free(expected.batches);
    free(storage.vertices); free(storage.batches);
    for (size_t i = 0U; i < COMPONENTS; ++i) free(blobs[i]);
    puts("shared templates: 40 ROM components, descriptor growth, aggregate eviction, "
         "empty/reordered/repeated inputs; 120 publications byte-exact");
}

static void exercise_exact_size_publication(const char *path)
{
    uint8_t *blob = load_blob(path, GE_ORIGINAL_MODEL62_BLOB_SIZE);
    GeOriginalModelSceneInput inputs[2] = {{0}};
    GeOriginalModelSceneCache cache = {0}, reference = {0};
    GeOriginalModelScene scene, direct;
    assert(blob);
    inputs[0].blob = blob;
    inputs[0].blob_size = GE_ORIGINAL_MODEL62_BLOB_SIZE;
    inputs[0].primary_offset = UINT32_C(0x5c8);
    inputs[0].secondary_offset = UINT32_C(0x6b8);
    inputs[0].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[0].world_zbuffer_enabled = 1U;
    set_transform(&inputs[0]);
    inputs[1] = inputs[0];
    inputs[1].position[1] += 10.0f;
    assert(ge_original_model_scene_cache_build(&cache, inputs, 2U, NULL, &scene)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    const size_t nv = scene.required_vertex_count, nb = scene.required_batch_count;
    GeDamRoomSceneStorage storage = {calloc(nv, sizeof(*storage.vertices)), nv,
        calloc(nb, sizeof(*storage.batches)), nb};
    GeDamRoomSceneStorage snapshot = {malloc(nv * sizeof(*snapshot.vertices)), nv,
        malloc(nb * sizeof(*snapshot.batches)), nb};
    GeDamRoomSceneStorage expected = {calloc(nv, sizeof(*expected.vertices)), nv,
        calloc(nb, sizeof(*expected.batches)), nb};
    assert(storage.vertices && storage.batches && snapshot.vertices && snapshot.batches
        && expected.vertices && expected.batches);
    assert(ge_original_model_scene_cache_build_exact(&cache, inputs, 2U, &storage,
        &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    for (size_t frame = 0U; frame < 120U; ++frame) {
        const size_t count = frame % 4U == 3U ? 0U : frame % 2U + 1U;
        memcpy(snapshot.vertices, storage.vertices, nv * sizeof(*storage.vertices));
        memcpy(snapshot.batches, storage.batches, nb * sizeof(*storage.batches));
        const uint64_t builds = cache.cached_builds, matrices = cache.matrix_elements_quantized;
        const GeOriginalModelSceneStatus status = ge_original_model_scene_cache_build_exact(
            &cache, inputs, count, &storage, &scene);
        if (storage.vertex_capacity != scene.required_vertex_count
                || storage.batch_capacity != scene.required_batch_count) {
            assert(status == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
            assert(scene.vertex_count == 0U && scene.batch_count == 0U);
            assert(cache.cached_builds == builds && cache.matrix_elements_quantized == matrices);
            assert(cache.publication_range_count == 0U);
            assert(memcmp(snapshot.vertices, storage.vertices, nv * sizeof(*storage.vertices)) == 0);
            assert(memcmp(snapshot.batches, storage.batches, nb * sizeof(*storage.batches)) == 0);
        } else assert(status == GE_ORIGINAL_MODEL_SCENE_OK);
        storage.vertex_capacity = scene.required_vertex_count;
        storage.batch_capacity = scene.required_batch_count;
        assert(ge_original_model_scene_cache_build_exact(&cache, inputs, count, &storage,
            &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(ge_original_model_scene_cache_build(&reference, inputs, count, &expected,
            &direct) == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(scene.vertex_count == direct.vertex_count && scene.batch_count == direct.batch_count);
        assert(memcmp(storage.vertices, expected.vertices, scene.vertex_count * sizeof(*storage.vertices)) == 0);
        assert(memcmp(storage.batches, expected.batches, scene.batch_count * sizeof(*storage.batches)) == 0);
    }
    /* A batch-only mismatch must also be rejected, even with enough bytes. */
    storage.vertex_capacity = nv;
    storage.batch_capacity = nb + 1U;
    const uint64_t builds = cache.cached_builds;
    assert(ge_original_model_scene_cache_build_exact(&cache, inputs, 2U, &storage,
        &scene) == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(cache.cached_builds == builds && cache.publication_range_count == 0U);
    storage.batch_capacity = nb;
    inputs[0].world_zbuffer_enabled = 0U;
    assert(ge_original_model_scene_cache_build_exact(&cache, inputs, 2U, &storage,
        &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(ge_original_model_scene_cache_build(&reference, inputs, 2U, &expected,
        &direct) == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(memcmp(storage.batches, expected.batches, nb * sizeof(*storage.batches)) == 0);
    assert(cache.discarded_publications_avoided > 0U && cache.discarded_vertices_avoided > 0U);
    ge_original_model_scene_cache_close(&cache);
    ge_original_model_scene_cache_close(&reference);
    free(storage.vertices); free(storage.batches);
    free(snapshot.vertices); free(snapshot.batches);
    free(expected.vertices); free(expected.batches); free(blob);
    puts("exact-size publication: 120 shrink/grow/empty transitions, batch-only mismatch, "
        "same-size material update; rejected output untouched, final bytes exact");
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    exercise(argv[1], GE_ORIGINAL_MODEL62_BLOB_SIZE,
             UINT32_C(0x5c8), UINT32_C(0x6b8),
             GE_ORIGINAL_MODEL_SCENE_NO_LIST);
    exercise(argv[2], GE_ORIGINAL_MODEL104_BLOB_SIZE,
             UINT32_C(0x138), UINT32_C(0x150), UINT32_C(0x90));
    exercise(argv[3], GE_ORIGINAL_MODEL178_BLOB_SIZE,
             UINT32_C(0x520), GE_ORIGINAL_MODEL_SCENE_NO_LIST,
             UINT32_C(0xa8));
    exercise_component_map_reuse(argv[1]);
    exercise_combat_topology_working_set(argv[1]);
    exercise_unchanged_input_publication_reuse(argv[1]);
    exercise_dirty_publication_ranges(argv[1]);
    exercise_shared_component_lifetime(argv[1]);
    exercise_exact_size_publication(argv[1]);
    return 0;
}
