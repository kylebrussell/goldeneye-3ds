#include "ge_original_first_person_scene.h"

#include <ultra64.h>
#include <bondtypes.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Retain just one previous immutable layout so alternating authored gun
 * switches do not repeatedly decode the same display lists. Matrix banks,
 * model state and output buffers are never stored in this spare slot. */
struct GeOriginalFirstPersonTopology {
    GeOriginalModelScene *queries;
    size_t *input_vertex_offsets;
    size_t *input_batch_offsets;
    GeDamRoomWorldVertex *template_vertices;
    GeDamRoomDrawBatch *template_batches;
    uint16_t *template_matrix_indices;
    uint32_t *template_transform_sources;
    size_t input_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t triangle_count;
    size_t commands_visited;
    uint64_t topology_signature;
    uint8_t topology_ready;
};

static void ge_first_person_topology_close(GeOriginalFirstPersonTopology *layout)
{
    if (layout == NULL) return;
    free(layout->template_transform_sources);
    free(layout->template_matrix_indices);
    free(layout->template_batches);
    free(layout->template_vertices);
    free(layout->input_batch_offsets);
    free(layout->input_vertex_offsets);
    free(layout->queries);
    free(layout);
}

static GeOriginalFirstPersonTopology *ge_first_person_topology_create(
    size_t capacity)
{
    GeOriginalFirstPersonTopology *layout = calloc(1, sizeof(*layout));
    if (layout == NULL) return NULL;
    layout->queries = calloc(capacity, sizeof(*layout->queries));
    layout->input_vertex_offsets = calloc(
        capacity, sizeof(*layout->input_vertex_offsets));
    layout->input_batch_offsets = calloc(
        capacity, sizeof(*layout->input_batch_offsets));
    if (layout->queries == NULL || layout->input_vertex_offsets == NULL
            || layout->input_batch_offsets == NULL) {
        ge_first_person_topology_close(layout);
        return NULL;
    }
    return layout;
}

static void ge_first_person_topology_swap(GeOriginalFirstPersonSceneCache *cache)
{
    GeOriginalFirstPersonTopology *layout = cache->previous_topology;
#define GE_SWAP_LAYOUT(type, field) do { \
    type value = cache->field; cache->field = layout->field; layout->field = value; \
} while (0)
    GE_SWAP_LAYOUT(GeOriginalModelScene *, queries);
    GE_SWAP_LAYOUT(size_t *, input_vertex_offsets);
    GE_SWAP_LAYOUT(size_t *, input_batch_offsets);
    GE_SWAP_LAYOUT(GeDamRoomWorldVertex *, template_vertices);
    GE_SWAP_LAYOUT(GeDamRoomDrawBatch *, template_batches);
    GE_SWAP_LAYOUT(uint16_t *, template_matrix_indices);
    GE_SWAP_LAYOUT(uint32_t *, template_transform_sources);
    GE_SWAP_LAYOUT(size_t, input_count);
    GE_SWAP_LAYOUT(size_t, required_vertex_count);
    GE_SWAP_LAYOUT(size_t, required_batch_count);
    GE_SWAP_LAYOUT(size_t, triangle_count);
    GE_SWAP_LAYOUT(size_t, commands_visited);
    GE_SWAP_LAYOUT(uint64_t, topology_signature);
    GE_SWAP_LAYOUT(uint8_t, topology_ready);
#undef GE_SWAP_LAYOUT
    cache->publication_ready = 0U;
}

static int ge_add_size(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) return 0;
    *result = left + right;
    return 1;
}

static int ge_matrix_valid(const float matrix[4][4])
{
    size_t row;
    size_t column;
    if (matrix == NULL) return 0;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (!isfinite(matrix[row][column])) return 0;
    return 1;
}

static int ge_matrix_is_identity(const float matrix[4][4])
{
    size_t row;
    size_t column;

    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (matrix[row][column]
                    != (row == column ? 1.0f : 0.0f)) return 0;
    return 1;
}

static int ge_blob_offset(const uint8_t *blob, size_t blob_size,
                          const void *pointer, uint32_t *offset)
{
    uintptr_t base;
    uintptr_t address;
    if (blob == NULL || pointer == NULL || offset == NULL) return 0;
    base = (uintptr_t)blob;
    address = (uintptr_t)pointer;
    if (address < base || address - base >= blob_size
            || address - base > UINT32_MAX) return 0;
    *offset = (uint32_t)(address - base);
    return (*offset & UINT32_C(7)) == 0U;
}

static GeOriginalFirstPersonSceneStatus ge_map_model_status(
    GeOriginalModelSceneStatus status)
{
    if (status == GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_PIPELINE_ERROR;
    if (status == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED;
    return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
}

static GeOriginalFirstPersonSceneStatus ge_first_person_scene_inputs(
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    const float view_to_world[4][4], GeOriginalModelSceneInput *inputs,
    size_t input_capacity, size_t *input_count, uint64_t *generation,
    uint32_t *resource_id)
{
    GeOriginalGunLiveHand live;
    const uint8_t *blob;
    size_t blob_size = 0U;
    Model *model;
    ModelNode *node;
    size_t cursor = 0U;
    unsigned asset_slot = 0U;

    if (assets == NULL || hand >= 2U || !ge_matrix_valid(view_to_world)
            || inputs == NULL || input_capacity == 0U
            || input_count == NULL || generation == NULL)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    *input_count = 0U;
    *generation = 0U;
    if (!ge_original_gun_live_hand_snapshot(hand, &live)
            || !live.visible || live.model == NULL || live.matrices == NULL
            || live.matrix_count == 0U)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_HAND_NOT_PUBLISHED;
    model = (Model *)(uintptr_t)live.model;
    if (model->obj == NULL || model->obj->RootNode == NULL
            || model->render_pos == NULL
            || live.matrix_count != (size_t)model->obj->numMatrices)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
    blob = ge_original_first_person_assets_blob_for_root(
        assets, model->obj->RootNode, &blob_size, &asset_slot);
    if (blob == NULL)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;

    node = model->obj->RootNode;
    while (node != NULL) {
        const uint16_t opcode = node->Opcode & UINT16_C(0xff);
        if (opcode == MODELNODE_OPCODE_DL
                || opcode == MODELNODE_OPCODE_DLCOLLISION) {
            const void *primary_pointer = opcode == MODELNODE_OPCODE_DL
                ? (const void *)node->Data->DisplayList.Primary
                : (const void *)node->Data->DisplayListCollisions.Primary;
            const void *secondary_pointer = opcode == MODELNODE_OPCODE_DL
                ? (const void *)node->Data->DisplayList.Secondary
                : (const void *)node->Data->DisplayListCollisions.Secondary;
            GeOriginalModelSceneInput *input;
            uint32_t primary = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            uint32_t secondary = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            size_t row;
            size_t column;

            if (cursor >= input_capacity
                    || (primary_pointer != NULL
                        && !ge_blob_offset(blob, blob_size,
                                           primary_pointer, &primary))
                    || (secondary_pointer != NULL
                        && !ge_blob_offset(blob, blob_size,
                                           secondary_pointer, &secondary)))
                return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
            input = &inputs[cursor++];
            memset(input, 0, sizeof(*input));
            input->blob = blob;
            input->blob_size = blob_size;
            input->primary_offset = primary;
            input->secondary_offset = secondary;
            input->segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
            if (opcode == MODELNODE_OPCODE_DLCOLLISION
                    && !ge_original_first_person_assets_collision_vertex_blob_offset(
                        assets, node, &input->segment4_offset))
                return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
            input->segment3_matrices = live.matrices;
            input->segment3_matrix_count = live.matrix_count;
            for (row = 0U; row < 4U; ++row)
                for (column = 0U; column < 4U; ++column)
                    input->matrix[row][column] = view_to_world[row][column];
        }
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL) {
                if (node->Next != NULL) {
                    node = node->Next;
                    break;
                }
                node = node->Parent;
            }
        }
    }
    if (cursor == 0U)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
    *input_count = cursor;
    *generation = live.generation;
    if (resource_id != NULL)
        *resource_id = (uint32_t)assets->loaded_model[asset_slot];
    return GE_ORIGINAL_FIRST_PERSON_SCENE_OK;
}

GeOriginalFirstPersonSceneStatus ge_original_first_person_scene_build(
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalFirstPersonScene *scene)
{
    GeOriginalModelSceneInput inputs[
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS];
    uint64_t generation = 0U;
    size_t input_count = 0U;
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t triangle_count = 0U;
    size_t commands_visited = 0U;
    size_t input_index;

    if (scene == NULL) return GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    if (storage != NULL
                && ((storage->vertex_capacity != 0U
                        && storage->vertices == NULL)
                    || (storage->batch_capacity != 0U
                        && storage->batches == NULL))) return scene->status;
    scene->status = ge_first_person_scene_inputs(
        assets, hand, view_to_world, inputs,
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
        &input_count, &generation, NULL);
    if (scene->status != GE_ORIGINAL_FIRST_PERSON_SCENE_OK)
        return scene->status;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        GeOriginalModelScene model_scene;
        GeOriginalModelSceneStatus model_status = ge_original_model_scene_build(
            &inputs[input_index], NULL, &model_scene);
        if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
            scene->status = ge_map_model_status(model_status);
            return scene->status;
        }
        if (!ge_add_size(vertex_cursor, model_scene.required_vertex_count,
                         &vertex_cursor)
                || !ge_add_size(batch_cursor,
                                model_scene.required_batch_count,
                                &batch_cursor)
                || !ge_add_size(triangle_count, model_scene.triangle_count,
                                &triangle_count)
                || !ge_add_size(commands_visited,
                                model_scene.commands_visited,
                                &commands_visited)) {
            scene->status = GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
            return scene->status;
        }
    }
    scene->generation = generation;
    scene->display_list_count = input_count;
    scene->required_vertex_count = vertex_cursor;
    scene->required_batch_count = batch_cursor;
    scene->triangle_count = triangle_count;
    scene->commands_visited = commands_visited;
    if (storage == NULL || vertex_cursor > storage->vertex_capacity
            || batch_cursor > storage->batch_capacity) {
        scene->status = GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED;
        return scene->status;
    }

    vertex_cursor = 0U;
    batch_cursor = 0U;
    triangle_count = 0U;
    commands_visited = 0U;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        GeDamRoomSceneStorage slice = {
            storage->vertices + vertex_cursor,
            storage->vertex_capacity - vertex_cursor,
            storage->batches + batch_cursor,
            storage->batch_capacity - batch_cursor,
        };
        GeOriginalModelScene model_scene;
        GeOriginalModelSceneStatus model_status = ge_original_model_scene_build(
            &inputs[input_index], &slice, &model_scene);
        size_t local_batch;
        if (model_status != GE_ORIGINAL_MODEL_SCENE_OK) {
            scene->status = ge_map_model_status(model_status);
            return scene->status;
        }
        for (local_batch = 0U; local_batch < model_scene.batch_count;
                ++local_batch)
            slice.batches[local_batch].first_vertex += vertex_cursor;
        vertex_cursor += model_scene.vertex_count;
        batch_cursor += model_scene.batch_count;
        triangle_count += model_scene.triangle_count;
        commands_visited += model_scene.commands_visited;
    }
    scene->vertex_count = vertex_cursor;
    scene->batch_count = batch_cursor;
    scene->triangle_count = triangle_count;
    scene->commands_visited = commands_visited;
    scene->status = GE_ORIGINAL_FIRST_PERSON_SCENE_OK;
    return scene->status;
}

static uint64_t ge_first_person_cache_mix(uint64_t hash, uint64_t value)
{
    size_t byte;
    for (byte = 0U; byte < sizeof(value); ++byte) {
        hash ^= (uint8_t)(value >> (byte * 8U));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t ge_first_person_topology_signature(
    const GeOriginalModelSceneInput *inputs, size_t input_count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;

    hash = ge_first_person_cache_mix(hash, input_count);
    for (index = 0U; index < input_count; ++index) {
        hash = ge_first_person_cache_mix(
            hash, (uint64_t)(uintptr_t)inputs[index].blob);
        hash = ge_first_person_cache_mix(hash, inputs[index].blob_size);
        hash = ge_first_person_cache_mix(hash, inputs[index].primary_offset);
        hash = ge_first_person_cache_mix(hash, inputs[index].secondary_offset);
        hash = ge_first_person_cache_mix(hash, inputs[index].segment4_offset);
        hash = ge_first_person_cache_mix(
            hash, inputs[index].segment3_matrix_count);
    }
    return hash;
}

int ge_original_first_person_scene_cache_init(
    GeOriginalFirstPersonSceneCache *cache)
{
    if (cache == NULL) return 0;
    if (cache->initialized != 0U) return 1;
    memset(cache, 0, sizeof(*cache));
    cache->inputs = calloc(GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
                           sizeof(*cache->inputs));
    cache->queries = calloc(GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
                            sizeof(*cache->queries));
    cache->input_vertex_offsets = calloc(
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
        sizeof(*cache->input_vertex_offsets));
    cache->input_batch_offsets = calloc(
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
        sizeof(*cache->input_batch_offsets));
    cache->input_quantized_matrix_offsets = calloc(
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
        sizeof(*cache->input_quantized_matrix_offsets));
    cache->input_quantized_matrix_hashes = calloc(
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS,
        sizeof(*cache->input_quantized_matrix_hashes));
    if (cache->inputs == NULL || cache->queries == NULL
            || cache->input_vertex_offsets == NULL
            || cache->input_batch_offsets == NULL
            || cache->input_quantized_matrix_offsets == NULL
            || cache->input_quantized_matrix_hashes == NULL) {
        ge_original_first_person_scene_cache_close(cache);
        return 0;
    }
    cache->capacity = GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS;
    cache->initialized = UINT8_C(1);
    return 1;
}

void ge_original_first_person_scene_cache_close(
    GeOriginalFirstPersonSceneCache *cache)
{
    if (cache == NULL) return;
    ge_first_person_topology_close(cache->previous_topology);
    free(cache->input_quantized_matrix_hashes);
    free(cache->input_quantized_matrix_offsets);
    free(cache->template_matrix_indices);
    free(cache->template_transform_sources);
    free(cache->quantized_matrix_changed);
    free(cache->quantized_matrices);
    free(cache->template_batches);
    free(cache->template_vertices);
    free(cache->input_batch_offsets);
    free(cache->input_vertex_offsets);
    free(cache->queries);
    free(cache->inputs);
    memset(cache, 0, sizeof(*cache));
}

void ge_original_first_person_scene_cache_bind_profile_clock(
    GeOriginalFirstPersonSceneCache *cache,
    GeOriginalFirstPersonSceneProfileClock clock, void *context)
{
    if (cache == NULL) return;
    cache->profile_clock = clock;
    cache->profile_clock_context = context;
}

static uint64_t ge_first_person_profile_now(
    const GeOriginalFirstPersonSceneCache *cache)
{
    return cache != NULL && cache->profile_clock != NULL
        ? cache->profile_clock(cache->profile_clock_context) : 0U;
}

static void ge_first_person_profile_add(uint64_t *counter,
    uint64_t started, uint64_t finished)
{
    if (counter != NULL && finished >= started)
        *counter += finished - started;
}

static int ge_first_person_build_transform_sources(
    const GeDamRoomWorldVertex *vertices, const uint16_t *matrix_indices,
    const uint16_t *matrix_bank_ids, size_t vertex_count,
    uint32_t *transform_sources)
{
    size_t table_capacity = 1U;
    uint32_t *table = NULL;
    size_t local_vertex;

    if (vertices == NULL || matrix_indices == NULL || matrix_bank_ids == NULL
            || transform_sources == NULL || vertex_count > UINT32_MAX)
        return 0;
    while (vertex_count <= SIZE_MAX / 2U
            && table_capacity < vertex_count * 2U
            && table_capacity <= SIZE_MAX / 2U)
        table_capacity *= 2U;
    if (vertex_count != 0U && table_capacity >= vertex_count
            && table_capacity <= SIZE_MAX / sizeof(*table))
        table = calloc(table_capacity, sizeof(*table));
    if (vertex_count != 0U && table == NULL) return 0;
    for (local_vertex = 0U; local_vertex < vertex_count; ++local_vertex) {
        const GeGbiVertex *source = &vertices[local_vertex].source;
        uint32_t first = (uint32_t)local_vertex;
        uint32_t hash = UINT32_C(2166136261);
        size_t slot;
        hash = (hash ^ (uint16_t)source->x) * UINT32_C(16777619);
        hash = (hash ^ (uint16_t)source->y) * UINT32_C(16777619);
        hash = (hash ^ (uint16_t)source->z) * UINT32_C(16777619);
        hash = (hash ^ matrix_indices[local_vertex]) * UINT32_C(16777619);
        hash = (hash ^ matrix_bank_ids[local_vertex])
            * UINT32_C(16777619);
        slot = (size_t)hash & (table_capacity - 1U);
        while (table[slot] != 0U) {
            const size_t candidate = (size_t)table[slot] - 1U;
            const GeGbiVertex *candidate_source =
                &vertices[candidate].source;
            if (matrix_bank_ids[candidate] == matrix_bank_ids[local_vertex]
                    && matrix_indices[candidate]
                        == matrix_indices[local_vertex]
                    && candidate_source->x == source->x
                    && candidate_source->y == source->y
                    && candidate_source->z == source->z) {
                first = (uint32_t)candidate;
                break;
            }
            slot = (slot + 1U) & (table_capacity - 1U);
        }
        if (table[slot] == 0U)
            table[slot] = (uint32_t)local_vertex + 1U;
        transform_sources[local_vertex] = first;
    }
    free(table);
    return 1;
}

static int ge_first_person_matrix_bank_matches(
    const GeOriginalModelSceneInput *left,
    const GeOriginalModelSceneInput *right);

static GeOriginalFirstPersonSceneStatus ge_first_person_decode_templates(
    GeOriginalFirstPersonSceneCache *cache, size_t input_count,
    size_t required_vertices, size_t required_batches)
{
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    uint16_t *matrix_indices = NULL;
    uint16_t *matrix_bank_ids = NULL;
    uint32_t *transform_sources = NULL;
    uint16_t input_matrix_bank_ids[
        GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS] = {0};
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t input_index;

    if (required_vertices != 0U) {
        vertices = malloc(required_vertices * sizeof(*vertices));
        matrix_indices = malloc(required_vertices * sizeof(*matrix_indices));
        matrix_bank_ids = malloc(
            required_vertices * sizeof(*matrix_bank_ids));
        transform_sources = malloc(
            required_vertices * sizeof(*transform_sources));
    }
    if (required_batches != 0U)
        batches = malloc(required_batches * sizeof(*batches));
    if ((required_vertices != 0U
            && (vertices == NULL || matrix_indices == NULL
                || matrix_bank_ids == NULL
                || transform_sources == NULL))
            || (required_batches != 0U && batches == NULL))
        goto fail;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelScene *query = &cache->queries[input_index];
        GeDamRoomSceneStorage storage = {
            vertices + vertex_cursor, query->required_vertex_count,
            batches + batch_cursor, query->required_batch_count,
        };
        GeOriginalModelScene built;
        size_t local_batch;
        size_t prior;
        uint16_t matrix_bank_id;

        cache->input_vertex_offsets[input_index] = vertex_cursor;
        cache->input_batch_offsets[input_index] = batch_cursor;
        if (ge_original_model_scene_build_matrix_template(
                &cache->inputs[input_index], &storage,
                matrix_indices + vertex_cursor,
                query->required_vertex_count, &built)
                != GE_ORIGINAL_MODEL_SCENE_OK
                || built.vertex_count != query->required_vertex_count
                || built.batch_count != query->required_batch_count
                || built.triangle_count != query->triangle_count)
            goto fail;
        for (prior = 0U; prior < input_index; ++prior)
            if (ge_first_person_matrix_bank_matches(
                    &cache->inputs[input_index],
                    &cache->inputs[prior])) break;
        matrix_bank_id = prior < input_index
            ? input_matrix_bank_ids[prior]
            : (uint16_t)input_index;
        input_matrix_bank_ids[input_index] = matrix_bank_id;
        for (local_batch = 0U; local_batch < built.vertex_count;
                ++local_batch)
            matrix_bank_ids[vertex_cursor + local_batch] = matrix_bank_id;
        for (local_batch = 0U; local_batch < built.batch_count; ++local_batch)
            batches[batch_cursor + local_batch].first_vertex += vertex_cursor;
        vertex_cursor += built.vertex_count;
        batch_cursor += built.batch_count;
    }
    if (vertex_cursor != required_vertices
            || batch_cursor != required_batches)
        goto fail;
    if (required_vertices != 0U
            && !ge_first_person_build_transform_sources(
                vertices, matrix_indices, matrix_bank_ids,
                required_vertices, transform_sources)) goto fail;
    free(cache->template_matrix_indices);
    free(cache->template_transform_sources);
    free(cache->template_batches);
    free(cache->template_vertices);
    cache->template_vertices = vertices;
    cache->template_batches = batches;
    cache->template_matrix_indices = matrix_indices;
    cache->template_transform_sources = transform_sources;
    free(matrix_bank_ids);
    return GE_ORIGINAL_FIRST_PERSON_SCENE_OK;

fail:
    free(matrix_bank_ids);
    free(transform_sources);
    free(matrix_indices);
    free(batches);
    free(vertices);
    return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
}

static float ge_first_person_matrix_element(float value)
{
    /* The caller rejects values outside the signed 16.16 domain, so the
     * scaled value is representable by int32_t.  Converting through int64_t
     * emitted four hot __aeabi_f2lz calls in the ARM publication loop even
     * though the subsequent cast immediately discarded the high word. */
    const int32_t fixed = (int32_t)(value * 65536.0f);
    return (float)fixed / 65536.0f;
}

static uint64_t ge_first_person_hash_float(uint64_t hash, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return ge_first_person_cache_mix(hash, bits);
}

static int ge_first_person_cache_reserve_matrices(
    GeOriginalFirstPersonSceneCache *cache, size_t required);

static int ge_first_person_matrix_bank_matches(
    const GeOriginalModelSceneInput *left,
    const GeOriginalModelSceneInput *right)
{
    return left->segment3_matrices == right->segment3_matrices
        && left->segment3_matrix_count == right->segment3_matrix_count;
}

/* Hash exactly the values consumed by the retained-template publication
 * pass.  The original gun/hand tick has already run before this adapter is
 * called; this only suppresses a repeated transform into the same retained
 * output storage when its quantized matrices and camera transform did not
 * change. */
static GeOriginalFirstPersonSceneStatus
ge_first_person_prepare_publication_matrices(
    GeOriginalFirstPersonSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    uint64_t topology_signature, uint64_t *signature,
    size_t *unique_bank_count)
{
    uint64_t hash = ge_first_person_cache_mix(
        UINT64_C(1469598103934665603), topology_signature);
    size_t required_matrices = 0U;
    size_t input_index;

    if (cache == NULL || signature == NULL || unique_bank_count == NULL)
        return GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    *unique_bank_count = 0U;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        size_t prior;
        for (prior = 0U; prior < input_index; ++prior)
            if (ge_first_person_matrix_bank_matches(
                    input, &inputs[prior])) break;
        if (prior < input_index) {
            cache->input_quantized_matrix_offsets[input_index] =
                cache->input_quantized_matrix_offsets[prior];
            cache->shared_matrix_banks_reused++;
        } else {
            cache->input_quantized_matrix_offsets[input_index] =
                required_matrices;
            if (!ge_add_size(required_matrices,
                             input->segment3_matrix_count,
                             &required_matrices))
                return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
            (*unique_bank_count)++;
        }
    }
    if (!ge_first_person_cache_reserve_matrices(cache, required_matrices))
        return GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        const size_t matrix_offset =
            cache->input_quantized_matrix_offsets[input_index];
        uint64_t matrix_hash = ge_first_person_cache_mix(
            UINT64_C(1469598103934665603),
            input->segment3_matrix_count);
        size_t matrix_index;
        size_t row;
        size_t column;
        size_t prior;

        for (prior = 0U; prior < input_index; ++prior)
            if (ge_first_person_matrix_bank_matches(
                    input, &inputs[prior])) break;
        if (prior < input_index) {
            matrix_hash = cache->input_quantized_matrix_hashes[prior];
        } else {
            for (matrix_index = 0U;
                    matrix_index < input->segment3_matrix_count;
                    ++matrix_index) {
                float quantized[4][4];
                int matrix_changed = cache->publication_ready == 0U;
                for (row = 0U; row < 4U; ++row)
                    for (column = 0U; column < 4U; ++column) {
                        const float value =
                            input->segment3_matrices[matrix_index]
                                [row][column];
                        if (!isfinite(value) || value < -32768.0f
                                || value >= 32768.0f)
                            return GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
                        quantized[row][column] =
                                ge_first_person_matrix_element(value);
                        matrix_hash = ge_first_person_hash_float(
                            matrix_hash, quantized[row][column]);
                        cache->matrix_elements_quantized++;
                    }
                if (!matrix_changed
                        && memcmp(cache->quantized_matrices[
                                matrix_offset + matrix_index],
                            quantized, sizeof(quantized)) != 0)
                    matrix_changed = 1;
                memcpy(cache->quantized_matrices[
                        matrix_offset + matrix_index],
                    quantized, sizeof(quantized));
                cache->quantized_matrix_changed[
                    matrix_offset + matrix_index] =
                        (uint8_t)matrix_changed;
            }
        }
        cache->input_quantized_matrix_hashes[input_index] = matrix_hash;
    }
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        size_t row;
        size_t column;
        hash = ge_first_person_cache_mix(hash, input->room_id);
        for (row = 0U; row < 3U; ++row)
            hash = ge_first_person_hash_float(hash, input->position[row]);
        for (row = 0U; row < 4U; ++row)
            for (column = 0U; column < 4U; ++column)
                hash = ge_first_person_hash_float(
                    hash, input->matrix[row][column]);
        hash = ge_first_person_cache_mix(
            hash, cache->input_quantized_matrix_hashes[input_index]);
    }
    *signature = hash;
    return GE_ORIGINAL_FIRST_PERSON_SCENE_OK;
}

static int ge_first_person_cache_reserve_matrices(
    GeOriginalFirstPersonSceneCache *cache, size_t required)
{
    float (*matrices)[4][4];
    uint8_t *changed;
    const size_t old_capacity = cache->quantized_matrix_capacity;

    if (required <= cache->quantized_matrix_capacity) return 1;
    if (required > SIZE_MAX / sizeof(*matrices)) return 0;
    matrices = realloc(cache->quantized_matrices,
                       required * sizeof(*matrices));
    if (matrices == NULL) return 0;
    cache->quantized_matrices = matrices;
    changed = realloc(cache->quantized_matrix_changed,
        required * sizeof(*changed));
    if (changed == NULL) return 0;
    cache->quantized_matrix_changed = changed;
    memset(cache->quantized_matrix_changed + old_capacity, 1,
        required - old_capacity);
    cache->quantized_matrix_capacity = required;
    return 1;
}

static void ge_first_person_transform_vertex(
    const float matrix[4][4], const GeGbiVertex *source, float eye[4])
{
    const float x = (float)source->x;
    const float y = (float)source->y;
    const float z = (float)source->z;
    size_t axis;
    for (axis = 0U; axis < 4U; ++axis) {
        float value = 0.0f;
        value += x * matrix[0][axis];
        value += y * matrix[1][axis];
        value += z * matrix[2][axis];
        value += matrix[3][axis];
        eye[axis] = value;
    }
}

GeOriginalFirstPersonSceneStatus ge_original_first_person_scene_build_cached(
    GeOriginalFirstPersonSceneCache *cache,
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalFirstPersonScene *scene)
{
    size_t input_count = 0U;
    uint64_t generation = 0U;
    uint64_t signature;
    uint32_t resource_id = 0U;
    uint64_t publication_signature;
    size_t input_index;
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t prepared_unique_bank_count = 0U;
    int all_publish_eye_space = 1;
    uint64_t matrix_elements_before_prepare;
    uint64_t build_started = 0U;
    uint64_t phase_started = 0U;
    int reuse_publication_storage;
    GeOriginalFirstPersonSceneStatus status;

    if (scene == NULL) return GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT;
    if (cache == NULL || cache->initialized == 0U
            || cache->inputs == NULL || cache->queries == NULL
            || cache->capacity != GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS
            || (storage != NULL
                && ((storage->vertex_capacity != 0U
                        && storage->vertices == NULL)
                    || (storage->batch_capacity != 0U
                        && storage->batches == NULL))))
        return scene->status;
    build_started = ge_first_person_profile_now(cache);
    phase_started = build_started;
    if (cache->profile_clock != NULL) cache->profile_build_calls++;
    cache->build_attempts++;
    status = ge_first_person_scene_inputs(
        assets, hand, view_to_world, cache->inputs, cache->capacity,
        &input_count, &generation, &resource_id);
    if (status != GE_ORIGINAL_FIRST_PERSON_SCENE_OK) goto done;
    signature = ge_first_person_topology_signature(cache->inputs, input_count);
    /* The two original hand buffers are reused for different ROM resources. */
    signature = ge_first_person_cache_mix(signature, resource_id);
    if (cache->topology_ready == 0U || cache->input_count != input_count
            || cache->topology_signature != signature) {
        GeOriginalFirstPersonTopology *previous = cache->previous_topology;
        if (previous != NULL && previous->topology_ready != 0U
                && previous->input_count == input_count
                && previous->topology_signature == signature) {
            ge_first_person_topology_swap(cache);
            cache->topology_reuses++;
        } else if (cache->topology_ready != 0U) {
            if (previous == NULL)
                cache->previous_topology = ge_first_person_topology_create(
                    cache->capacity);
            /* Allocation failure only disables this renderer optimization. */
            if (cache->previous_topology != NULL)
                ge_first_person_topology_swap(cache);
            cache->topology_ready = 0U;
        }
        cache->topology_publications++;
    }
    if (cache->topology_ready == 0U || cache->input_count != input_count
            || cache->topology_signature != signature) {
        size_t required_vertices = 0U;
        size_t required_batches = 0U;
        size_t triangles = 0U;
        size_t commands = 0U;

        cache->topology_ready = 0U;
        for (input_index = 0U; input_index < input_count; ++input_index) {
            const GeOriginalModelSceneStatus model_status =
                ge_original_model_scene_build(
                    &cache->inputs[input_index], NULL,
                    &cache->queries[input_index]);
            if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
                status = ge_map_model_status(model_status);
                goto done;
            }
            if (!ge_add_size(required_vertices,
                    cache->queries[input_index].required_vertex_count,
                    &required_vertices)
                    || !ge_add_size(required_batches,
                        cache->queries[input_index].required_batch_count,
                        &required_batches)
                    || !ge_add_size(triangles,
                        cache->queries[input_index].triangle_count,
                        &triangles)
                    || !ge_add_size(commands,
                        cache->queries[input_index].commands_visited,
                        &commands)) {
                status = GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
        }
        status = ge_first_person_decode_templates(
            cache, input_count, required_vertices, required_batches);
        if (status != GE_ORIGINAL_FIRST_PERSON_SCENE_OK) goto done;
        cache->input_count = input_count;
        cache->required_vertex_count = required_vertices;
        cache->required_batch_count = required_batches;
        cache->triangle_count = triangles;
        cache->commands_visited = commands;
        cache->topology_signature = signature;
        cache->topology_ready = UINT8_C(1);
        cache->publication_ready = UINT8_C(0);
        cache->topology_rebuilds++;
    }
    if (cache->profile_clock != NULL) {
        const uint64_t finished = ge_first_person_profile_now(cache);
        ge_first_person_profile_add(
            &cache->profile_input_topology_ticks,
            phase_started, finished);
        phase_started = finished;
    }

    scene->generation = generation;
    scene->display_list_count = input_count;
    scene->required_vertex_count = cache->required_vertex_count;
    scene->required_batch_count = cache->required_batch_count;
    scene->triangle_count = cache->triangle_count;
    scene->commands_visited = cache->commands_visited;
    if (storage == NULL
            || cache->required_vertex_count > storage->vertex_capacity
            || cache->required_batch_count > storage->batch_capacity) {
        status = GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED;
        goto done;
    }

    matrix_elements_before_prepare = cache->matrix_elements_quantized;
    status = ge_first_person_prepare_publication_matrices(
        cache, cache->inputs, input_count, signature,
        &publication_signature, &prepared_unique_bank_count);
    if (status != GE_ORIGINAL_FIRST_PERSON_SCENE_OK) goto done;
    if (cache->profile_clock != NULL) {
        const uint64_t finished = ge_first_person_profile_now(cache);
        ge_first_person_profile_add(
            &cache->profile_matrix_signature_ticks,
            phase_started, finished);
        phase_started = finished;
    }
    if (cache->publication_ready != 0U
            && cache->publication_signature == publication_signature
            && cache->published_vertices == storage->vertices
            && cache->published_batches == storage->batches) {
        scene->vertex_count = cache->required_vertex_count;
        scene->batch_count = cache->required_batch_count;
        cache->unchanged_builds++;
        status = GE_ORIGINAL_FIRST_PERSON_SCENE_OK;
        goto done;
    }

    reuse_publication_storage = cache->publication_ready != 0U
        && cache->published_vertices == storage->vertices
        && cache->published_batches == storage->batches;
    cache->matrix_bank_quantizations += prepared_unique_bank_count;
    cache->matrix_elements_requantized_avoided +=
        cache->matrix_elements_quantized - matrix_elements_before_prepare;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &cache->inputs[input_index];
        const float (*input_matrices)[4][4] = cache->quantized_matrices
            + cache->input_quantized_matrix_offsets[input_index];
        const GeOriginalModelScene *query = &cache->queries[input_index];
        const size_t template_vertex_base =
            cache->input_vertex_offsets[input_index];
        const size_t template_batch_base =
            cache->input_batch_offsets[input_index];
        size_t local_vertex;
        size_t local_batch;
        const int publish_eye_space = ge_matrix_is_identity(input->matrix)
            && input->position[0] == 0.0f
            && input->position[1] == 0.0f
            && input->position[2] == 0.0f;

        if (!publish_eye_space) all_publish_eye_space = 0;

        if (publish_eye_space)
            cache->eye_space_vertices_published +=
                query->required_vertex_count;

        if (cache->profile_clock != NULL)
            phase_started = ge_first_person_profile_now(cache);
        for (local_vertex = 0U;
                local_vertex < query->required_vertex_count; ++local_vertex) {
            const size_t template_index = template_vertex_base + local_vertex;
            const GeDamRoomWorldVertex *source =
                &cache->template_vertices[template_index];
            GeDamRoomWorldVertex *destination =
                &storage->vertices[vertex_cursor + local_vertex];
            const uint16_t matrix_index =
                cache->template_matrix_indices[template_index];
            const uint32_t transform_source =
                cache->template_transform_sources[template_index];
            float eye[4];
            size_t axis;
            size_t row;

            if ((size_t)matrix_index >= input->segment3_matrix_count) {
                cache->topology_ready = 0U;
                status = GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
            if (reuse_publication_storage && publish_eye_space
                    && cache->publication_eye_space != 0U
                    && cache->quantized_matrix_changed[
                        cache->input_quantized_matrix_offsets[input_index]
                            + matrix_index] == 0U) {
                cache->unchanged_matrix_vertices_reused++;
                continue;
            }
            if (!reuse_publication_storage) {
                *destination = *source;
            } else {
                cache->static_vertex_copies_avoided++;
            }
            if ((size_t)transform_source < template_index) {
                /* Template and publication inputs retain the same cumulative
                 * vertex ordering.  A prior source from another display-list
                 * input is therefore addressed by its global template index,
                 * just like a prior source inside this input. */
                const GeDamRoomWorldVertex *prior =
                    &storage->vertices[transform_source];
                memcpy(destination->processed.eye, prior->processed.eye,
                       sizeof(destination->processed.eye));
                memcpy(destination->world, prior->world,
                       sizeof(destination->world));
                cache->duplicate_vertex_transforms_avoided++;
                if ((size_t)transform_source < template_vertex_base)
                    cache->cross_input_duplicate_transforms_avoided++;
            } else {
                ge_first_person_transform_vertex(
                    input_matrices[matrix_index], &source->source, eye);
                memcpy(destination->processed.eye, eye, sizeof(eye));
                for (axis = 0U; axis < 3U; ++axis) {
                    if (publish_eye_space) {
                        /* hand->mtxlist is already the canonical N64
                         * eye-space publication. */
                        destination->world[axis] = eye[axis];
                    } else {
                        destination->world[axis] = input->position[axis];
                        for (row = 0U; row < 4U; ++row)
                            destination->world[axis] +=
                                eye[row] * input->matrix[row][axis];
                    }
                }
            }
        }
        if (cache->profile_clock != NULL) {
            const uint64_t finished = ge_first_person_profile_now(cache);
            ge_first_person_profile_add(
                &cache->profile_vertex_transform_ticks,
                phase_started, finished);
            phase_started = finished;
        }
        for (local_batch = 0U;
                local_batch < query->required_batch_count; ++local_batch) {
            GeDamRoomDrawBatch batch =
                cache->template_batches[template_batch_base + local_batch];
            if (batch.first_vertex < template_vertex_base) {
                cache->topology_ready = 0U;
                status = GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
            batch.first_vertex = vertex_cursor
                + batch.first_vertex - template_vertex_base;
            batch.room_id = input->room_id;
            if (!reuse_publication_storage) {
                storage->batches[batch_cursor + local_batch] = batch;
            } else {
                storage->batches[batch_cursor + local_batch].room_id =
                    batch.room_id;
                cache->static_batch_copies_avoided++;
            }
        }
        if (cache->profile_clock != NULL) {
            const uint64_t finished = ge_first_person_profile_now(cache);
            ge_first_person_profile_add(
                &cache->profile_batch_publication_ticks,
                phase_started, finished);
            phase_started = finished;
        }
        vertex_cursor += query->required_vertex_count;
        batch_cursor += query->required_batch_count;
    }
    if (vertex_cursor != cache->required_vertex_count
            || batch_cursor != cache->required_batch_count) {
        cache->topology_ready = 0U;
        status = GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR;
        goto done;
    }
    scene->vertex_count = vertex_cursor;
    scene->batch_count = batch_cursor;
    cache->publication_signature = publication_signature;
    cache->published_vertices = storage->vertices;
    cache->published_batches = storage->batches;
    cache->publication_ready = UINT8_C(1);
    cache->publication_eye_space = (uint8_t)all_publish_eye_space;
    cache->single_pass_builds++;
    status = GE_ORIGINAL_FIRST_PERSON_SCENE_OK;

done:
    scene->status = status;
    if (cache != NULL && cache->profile_clock != NULL)
        ge_first_person_profile_add(&cache->profile_build_ticks,
            build_started, ge_first_person_profile_now(cache));
    return status;
}

const char *ge_original_first_person_scene_status_name(
    GeOriginalFirstPersonSceneStatus status)
{
    switch (status) {
    case GE_ORIGINAL_FIRST_PERSON_SCENE_OK: return "ok";
    case GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_FIRST_PERSON_SCENE_HAND_NOT_PUBLISHED: return "hand not published";
    case GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR: return "model layout error";
    case GE_ORIGINAL_FIRST_PERSON_SCENE_PIPELINE_ERROR: return "GBI/material pipeline error";
    case GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED: return "capacity exceeded";
    default: return "unknown";
    }
}
