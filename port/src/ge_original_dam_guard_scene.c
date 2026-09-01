#include "ge_original_dam_guard_scene.h"

#include <ultra64.h>
#include <bondtypes.h>
#include "game/matrixmath.h"
#include "game/model.h"

#include "ge_original_dam_guard_model.h"
#include "ge_original_dam_guard_weapon_model.h"
#include "ge_original_dam_guards.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern void modelApplyDistanceRelations(Model *model, ModelNode *node);
extern void modelApplyReorderRelations(Model *model, ModelNode *node);
extern void modelApplyHeadRelations(Model *model, ModelNode *node);

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

static const GeOriginalDamGuardDisplayList *ge_find_display_list(
    const ModelNode *node)
{
    const GeOriginalDamGuardDisplayList *lists;
    size_t count;
    size_t index;
    lists = ge_original_dam_guard_model_display_lists(&count);
    for (index = 0U; index < count; ++index)
        if (lists[index].node == node) return &lists[index];
    return NULL;
}

static void ge_make_input(GeOriginalModelSceneInput *input,
                          const uint8_t *blob, size_t blob_size,
                          const GeOriginalDamGuardDisplayList *list,
                          uint8_t room, const Mtxf *view_to_world,
                          const Model *model)
{
    size_t row;
    size_t column;
    memset(input, 0, sizeof(*input));
    input->blob = blob;
    input->blob_size = blob_size;
    input->primary_offset = list->primary_offset;
    input->secondary_offset = list->secondary_offset;
    input->segment4_offset = list->vertex_offset;
    input->room_id = room;
    input->segment3_matrices =
        (const float (*)[4][4])(const void *)model->render_pos;
    input->segment3_matrix_count = model->obj->numMatrices;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            input->matrix[row][column] =
                view_to_world->m[row][column];
}

static GeOriginalDamGuardSceneStatus ge_guard_scene_inputs_internal(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    int include_weapons,
    const float view_to_world[4][4],
    GeOriginalModelSceneInput *inputs, size_t input_capacity,
    size_t *input_count)
{
    Mtxf view_to_world_mtx;
    size_t cursor = 0U;
    size_t guard_index;
    if (input_count != NULL) *input_count = 0U;
    if (model_blob == NULL
            || model_blob_size != GE_ORIGINAL_DAM_GUARD_MODEL_BLOB_SIZE
            || (include_weapons != 0
                && (weapon_blob == NULL
                    || weapon_blob_size
                        != GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_BLOB_SIZE))
            || !ge_matrix_valid(view_to_world) || input_count == NULL
            || (input_capacity != 0U && inputs == NULL))
        return GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT;
    if (ge_original_dam_guards_count() == 0U)
        return GE_ORIGINAL_DAM_GUARD_SCENE_GUARDS_NOT_READY;
    memcpy(view_to_world_mtx.m, view_to_world, sizeof(view_to_world_mtx.m));

    for (guard_index = 0U;
            guard_index < ge_original_dam_guards_count(); ++guard_index) {
        ChrRecord *chr = ge_original_dam_guard_chr(guard_index);
        PropRecord *prop = ge_original_dam_guard_prop(guard_index);
        Model *model;
        ModelNode *node;
        uint8_t render_room;
        if (!ge_original_dam_guard_is_live(guard_index)) continue;
        if (chr == NULL || prop == NULL || prop->stan == NULL
                || (model = chr->model) == NULL || model->obj == NULL
                || model->render_pos == NULL)
            return GE_ORIGINAL_DAM_GUARD_SCENE_GUARDS_NOT_READY;
        /* The unchanged chrTick/posIsOnScreen result owns character
         * visibility. Room 0xff is outside Dam's authored room set, so the
         * existing portal-room renderer rejects an offscreen guard without
         * changing its model topology or gameplay state. */
        render_room = (prop->flags & PROPFLAG_ONSCREEN) != 0U
            ? (uint8_t)prop->stan->room : UINT8_MAX;
        node = model->obj->RootNode;
        while (node != NULL) {
            switch (node->Opcode & 0xffU) {
            case MODELNODE_OPCODE_LOD:
                modelApplyDistanceRelations(model, node);
                break;
            case MODELNODE_OPCODE_BSP:
                modelApplyReorderRelations(model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_DLCOLLISION:
                {
                    const GeOriginalDamGuardDisplayList *list =
                        ge_find_display_list(node);
                    if (list == NULL || modelFindNodeMtx(model, node, 0) == NULL)
                        return GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
                    if (cursor >= input_capacity) {
                        ++cursor;
                    } else {
                        ge_make_input(&inputs[cursor], model_blob,
                                      model_blob_size, list,
                                      render_room,
                                      &view_to_world_mtx, model);
                        ++cursor;
                    }
                }
                break;
            default:
                break;
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
        if (include_weapons != 0) {
            GUNHAND hand;
            const GeOriginalDamGuardDisplayList *weapon_list =
                ge_original_dam_guard_weapon_model_display_list();
            if (weapon_list == NULL)
                return GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
            for (hand = GUNRIGHT; hand <= GUNLEFT; hand++) {
                PropRecord *held_prop = chr->weapons_held[hand];
                ObjectRecord *held_object;
                Model *held_model;
                if (held_prop == NULL) continue;
                held_object = held_prop->obj;
                if (held_object == NULL
                        || (held_object->runtime_bitflags
                            & (RUNTIMEBITFLAG_00000800
                               | RUNTIMEBITFLAG_00000080)) != 0U
                        || (s32)(held_object->flags2 << 12) < 0
                        || (held_model = held_object->model) == NULL
                        || held_model->obj == NULL
                        || held_model->render_pos == NULL
                        || held_model->attachedto != model
                        || held_model->attachedto_objinst == NULL
                        || modelFindNodeMtx(
                            model, held_model->attachedto_objinst, 0) == NULL)
                    continue;
                if (cursor >= input_capacity) {
                    ++cursor;
                } else {
                    ge_make_input(&inputs[cursor], weapon_blob,
                                  weapon_blob_size, weapon_list,
                                  render_room,
                                  &view_to_world_mtx, held_model);
                    ++cursor;
                }
            }
        }
    }
    *input_count = cursor;
    return cursor > input_capacity
        ? GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED
        : GE_ORIGINAL_DAM_GUARD_SCENE_OK;
}

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_inputs(
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    GeOriginalModelSceneInput *inputs, size_t input_capacity,
    size_t *input_count)
{
    return ge_guard_scene_inputs_internal(
        model_blob, model_blob_size, NULL, 0U, 0, view_to_world,
        inputs, input_capacity, input_count);
}

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_inputs_with_weapons(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    GeOriginalModelSceneInput *inputs, size_t input_capacity,
    size_t *input_count)
{
    return ge_guard_scene_inputs_internal(
        model_blob, model_blob_size, weapon_blob, weapon_blob_size, 1,
        view_to_world, inputs, input_capacity, input_count);
}

static GeOriginalDamGuardSceneStatus ge_guard_scene_build_internal(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    int include_weapons,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    GeOriginalModelSceneInput *inputs;
    GeOriginalModelScene *queries;
    size_t input_count = 0U;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t triangle_count = 0U;
    size_t commands_visited = 0U;
    size_t input_index;
    GeOriginalDamGuardSceneStatus status;
    if (scene == NULL) return GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT;
    inputs = calloc(GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS, sizeof(*inputs));
    queries = calloc(GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS, sizeof(*queries));
    if (inputs == NULL || queries == NULL) {
        free(queries);
        free(inputs);
        return scene->status;
    }
    status = ge_guard_scene_inputs_internal(
        model_blob, model_blob_size, weapon_blob, weapon_blob_size,
        include_weapons, view_to_world, inputs,
        GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS, &input_count);
    if (status != GE_ORIGINAL_DAM_GUARD_SCENE_OK) goto done;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneStatus model_status =
            ge_original_model_scene_build(
                &inputs[input_index], NULL, &queries[input_index]);
        if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
            status = model_status == GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR
                ? GE_ORIGINAL_DAM_GUARD_SCENE_PIPELINE_ERROR
                : GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
            goto done;
        }
        if (!ge_add_size(vertex_count,
                         queries[input_index].required_vertex_count,
                         &vertex_count)
                || !ge_add_size(batch_count,
                                queries[input_index].required_batch_count,
                                &batch_count)
                || !ge_add_size(triangle_count,
                                queries[input_index].triangle_count,
                                &triangle_count)
                || !ge_add_size(commands_visited,
                                queries[input_index].commands_visited,
                                &commands_visited)) {
            status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
            goto done;
        }
    }
    scene->guard_count = ge_original_dam_guards_live_count();
    scene->input_count = input_count;
    scene->required_vertex_count = vertex_count;
    scene->required_batch_count = batch_count;
    scene->triangle_count = triangle_count;
    scene->commands_visited = commands_visited;
    if (storage == NULL || vertex_count > storage->vertex_capacity
            || batch_count > storage->batch_capacity
            || (vertex_count != 0U && storage->vertices == NULL)
            || (batch_count != 0U && storage->batches == NULL)) {
        status = GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED;
        goto done;
    }
    vertex_count = 0U;
    batch_count = 0U;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        GeDamRoomSceneStorage local = {
            storage->vertices + vertex_count,
            queries[input_index].required_vertex_count,
            storage->batches + batch_count,
            queries[input_index].required_batch_count,
        };
        GeOriginalModelScene built;
        size_t local_batch;
        const GeOriginalModelSceneStatus model_status =
            ge_original_model_scene_build(
                &inputs[input_index], &local, &built);
        if (model_status != GE_ORIGINAL_MODEL_SCENE_OK) {
            status = model_status == GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR
                ? GE_ORIGINAL_DAM_GUARD_SCENE_PIPELINE_ERROR
                : GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
            goto done;
        }
        for (local_batch = 0U; local_batch < built.batch_count; ++local_batch)
            storage->batches[batch_count + local_batch].first_vertex
                += vertex_count;
        vertex_count += built.vertex_count;
        batch_count += built.batch_count;
    }
    scene->vertex_count = vertex_count;
    scene->batch_count = batch_count;
    status = GE_ORIGINAL_DAM_GUARD_SCENE_OK;

done:
    scene->status = status;
    free(queries);
    free(inputs);
    return status;
}

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build(
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    return ge_guard_scene_build_internal(
        model_blob, model_blob_size, NULL, 0U, 0, view_to_world,
        storage, scene);
}

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build_with_weapons(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    return ge_guard_scene_build_internal(
        model_blob, model_blob_size, weapon_blob, weapon_blob_size, 1,
        view_to_world, storage, scene);
}

static uint64_t ge_guard_topology_mix(uint64_t hash, uint64_t value)
{
    size_t byte;
    for (byte = 0U; byte < sizeof(value); byte++) {
        hash ^= (uint8_t)(value >> (byte * 8U));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t ge_guard_topology_signature(
    const GeOriginalModelSceneInput *inputs, size_t input_count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;

    hash = ge_guard_topology_mix(hash, input_count);
    for (index = 0U; index < input_count; index++) {
        hash = ge_guard_topology_mix(hash, inputs[index].blob_size);
        hash = ge_guard_topology_mix(hash, inputs[index].primary_offset);
        hash = ge_guard_topology_mix(hash, inputs[index].secondary_offset);
        hash = ge_guard_topology_mix(hash, inputs[index].segment4_offset);
        hash = ge_guard_topology_mix(
            hash, inputs[index].segment3_matrix_count);
    }
    return hash;
}

int ge_original_dam_guard_scene_cache_init(
    GeOriginalDamGuardSceneCache *cache)
{
    if (cache == NULL) return 0;
    if (cache->initialized != 0) return 1;
    memset(cache, 0, sizeof(*cache));
    cache->inputs = calloc(GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
                           sizeof(*cache->inputs));
    cache->queries = calloc(GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
                            sizeof(*cache->queries));
    cache->input_vertex_offsets = calloc(
        GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
        sizeof(*cache->input_vertex_offsets));
    cache->input_batch_offsets = calloc(
        GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
        sizeof(*cache->input_batch_offsets));
    if (cache->inputs == NULL || cache->queries == NULL
            || cache->input_vertex_offsets == NULL
            || cache->input_batch_offsets == NULL) {
        ge_original_dam_guard_scene_cache_close(cache);
        return 0;
    }
    cache->capacity = GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS;
    cache->initialized = 1;
    return 1;
}

void ge_original_dam_guard_scene_cache_close(
    GeOriginalDamGuardSceneCache *cache)
{
    if (cache == NULL) return;
    free(cache->input_batch_offsets);
    free(cache->input_vertex_offsets);
    free(cache->quantized_matrices);
    free(cache->template_matrix_indices);
    free(cache->template_batches);
    free(cache->template_vertices);
    free(cache->queries);
    free(cache->inputs);
    memset(cache, 0, sizeof(*cache));
}

static GeOriginalDamGuardSceneStatus ge_guard_cache_decode_templates(
    GeOriginalDamGuardSceneCache *cache, size_t input_count,
    size_t required_vertices, size_t required_batches)
{
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    uint16_t *matrix_indices = NULL;
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t input_index;

    if (required_vertices != 0U) {
        vertices = malloc(required_vertices * sizeof(*vertices));
        matrix_indices = malloc(required_vertices * sizeof(*matrix_indices));
    }
    if (required_batches != 0U)
        batches = malloc(required_batches * sizeof(*batches));
    if ((required_vertices != 0U
            && (vertices == NULL || matrix_indices == NULL))
            || (required_batches != 0U && batches == NULL))
        goto fail;
    for (input_index = 0U; input_index < input_count; input_index++) {
        const GeOriginalModelScene *query = &cache->queries[input_index];
        GeDamRoomSceneStorage storage = {
            vertices + vertex_cursor, query->required_vertex_count,
            batches + batch_cursor, query->required_batch_count,
        };
        GeOriginalModelScene built;
        size_t batch_index;
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
        for (batch_index = 0U; batch_index < built.batch_count; batch_index++)
            batches[batch_cursor + batch_index].first_vertex += vertex_cursor;
        vertex_cursor += built.vertex_count;
        batch_cursor += built.batch_count;
    }
    if (vertex_cursor != required_vertices
            || batch_cursor != required_batches)
        goto fail;
    free(cache->template_matrix_indices);
    free(cache->template_batches);
    free(cache->template_vertices);
    cache->template_vertices = vertices;
    cache->template_batches = batches;
    cache->template_matrix_indices = matrix_indices;
    return GE_ORIGINAL_DAM_GUARD_SCENE_OK;

fail:
    free(matrix_indices);
    free(batches);
    free(vertices);
    return GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
}

static float ge_guard_matrix_element(float value)
{
    const int64_t fixed = (int64_t)(value * 65536.0f);
    return (float)(int32_t)fixed / 65536.0f;
}

static int ge_guard_cache_reserve_matrices(
    GeOriginalDamGuardSceneCache *cache, size_t required)
{
    float (*matrices)[4][4];

    if (required <= cache->quantized_matrix_capacity) return 1;
    if (required > SIZE_MAX / sizeof(*matrices)) return 0;
    matrices = realloc(cache->quantized_matrices,
                       required * sizeof(*matrices));
    if (matrices == NULL) return 0;
    cache->quantized_matrices = matrices;
    cache->quantized_matrix_capacity = required;
    return 1;
}

static GeOriginalDamGuardSceneStatus ge_guard_scene_build_cached_internal(
    GeOriginalDamGuardSceneCache *cache,
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    int include_weapons,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    size_t input_count = 0U;
    size_t vertex_count;
    size_t batch_count;
    size_t input_index;
    uint64_t signature;
    GeOriginalDamGuardSceneStatus status;

    if (scene == NULL) return GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT;
    if (cache == NULL || cache->initialized == 0
            || cache->inputs == NULL || cache->queries == NULL
            || cache->capacity != GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS)
        return scene->status;
    cache->build_attempts++;
    status = ge_guard_scene_inputs_internal(
        model_blob, model_blob_size, weapon_blob, weapon_blob_size,
        include_weapons, view_to_world, cache->inputs,
        cache->capacity, &input_count);
    if (status != GE_ORIGINAL_DAM_GUARD_SCENE_OK) goto done;
    signature = ge_guard_topology_signature(cache->inputs, input_count);
    if (cache->topology_ready == 0 || cache->input_count != input_count
            || cache->topology_signature != signature) {
        size_t required_vertices = 0U;
        size_t required_batches = 0U;
        size_t triangles = 0U;
        size_t commands = 0U;

        cache->topology_ready = 0;
        for (input_index = 0U; input_index < input_count; input_index++) {
            const GeOriginalModelSceneStatus model_status =
                ge_original_model_scene_build(&cache->inputs[input_index],
                    NULL, &cache->queries[input_index]);
            if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
                status = model_status == GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR
                    ? GE_ORIGINAL_DAM_GUARD_SCENE_PIPELINE_ERROR
                    : GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
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
                status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
        }
        status = ge_guard_cache_decode_templates(
            cache, input_count, required_vertices, required_batches);
        if (status != GE_ORIGINAL_DAM_GUARD_SCENE_OK) goto done;
        cache->input_count = input_count;
        cache->required_vertex_count = required_vertices;
        cache->required_batch_count = required_batches;
        cache->triangle_count = triangles;
        cache->commands_visited = commands;
        cache->topology_signature = signature;
        cache->topology_ready = 1;
        cache->topology_rebuilds++;
    }

    scene->guard_count = ge_original_dam_guards_live_count();
    scene->input_count = input_count;
    scene->required_vertex_count = cache->required_vertex_count;
    scene->required_batch_count = cache->required_batch_count;
    scene->triangle_count = cache->triangle_count;
    scene->commands_visited = cache->commands_visited;
    if (storage == NULL
            || cache->required_vertex_count > storage->vertex_capacity
            || cache->required_batch_count > storage->batch_capacity
            || (cache->required_vertex_count != 0U
                && storage->vertices == NULL)
            || (cache->required_batch_count != 0U
                && storage->batches == NULL)) {
        status = GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED;
        goto done;
    }

    {
        size_t maximum_matrices = 0U;
        for (input_index = 0U; input_index < input_count; ++input_index)
            if (cache->inputs[input_index].segment3_matrix_count
                    > maximum_matrices)
                maximum_matrices =
                    cache->inputs[input_index].segment3_matrix_count;
        if (!ge_guard_cache_reserve_matrices(cache, maximum_matrices)) {
            status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
            goto done;
        }
    }

    vertex_count = 0U;
    batch_count = 0U;
    for (input_index = 0U; input_index < input_count; input_index++) {
        const GeOriginalModelScene *query = &cache->queries[input_index];
        const GeOriginalModelSceneInput *input = &cache->inputs[input_index];
        const size_t template_vertex_base =
            cache->input_vertex_offsets[input_index];
        const size_t template_batch_base =
            cache->input_batch_offsets[input_index];
        size_t matrix_index_to_quantize;
        size_t local_vertex;
        size_t local_batch;

        /* Offscreen inputs retain their fixed overlay allocation, but the
         * portal renderer will reject their synthetic room. Avoid the costly
         * joint-matrix quantization and per-vertex transforms until canonical
         * chrTick publishes PROPFLAG_ONSCREEN again. The resident vertices
         * may remain stale because no batch can reference them for drawing. */
        if (input->room_id == UINT8_MAX) {
            for (local_batch = 0U;
                    local_batch < query->required_batch_count; local_batch++) {
                GeDamRoomDrawBatch batch =
                    cache->template_batches[template_batch_base + local_batch];
                if (batch.first_vertex < template_vertex_base) {
                    cache->topology_ready = 0;
                    status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
                    goto done;
                }
                batch.first_vertex = vertex_count
                    + batch.first_vertex - template_vertex_base;
                batch.room_id = UINT8_MAX;
                storage->batches[batch_count + local_batch] = batch;
            }
            vertex_count += query->required_vertex_count;
            batch_count += query->required_batch_count;
            continue;
        }

        for (matrix_index_to_quantize = 0U;
                matrix_index_to_quantize < input->segment3_matrix_count;
                ++matrix_index_to_quantize) {
            size_t matrix_row;
            size_t matrix_column;
            for (matrix_row = 0U; matrix_row < 4U; ++matrix_row)
                for (matrix_column = 0U; matrix_column < 4U;
                        ++matrix_column)
                    cache->quantized_matrices[matrix_index_to_quantize]
                        [matrix_row][matrix_column] = ge_guard_matrix_element(
                            input->segment3_matrices[matrix_index_to_quantize]
                                [matrix_row][matrix_column]);
        }

        for (local_vertex = 0U;
                local_vertex < query->required_vertex_count; local_vertex++) {
            const size_t template_index = template_vertex_base + local_vertex;
            const GeDamRoomWorldVertex *source =
                &cache->template_vertices[template_index];
            GeDamRoomWorldVertex *destination =
                &storage->vertices[vertex_count + local_vertex];
            const uint16_t matrix_index =
                cache->template_matrix_indices[template_index];
            float object[4];
            float eye[4];
            size_t axis;
            size_t row;

            if ((size_t)matrix_index >= input->segment3_matrix_count) {
                cache->topology_ready = 0;
                status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
            *destination = *source;
            object[0] = source->source.x;
            object[1] = source->source.y;
            object[2] = source->source.z;
            object[3] = 1.0f;
            for (axis = 0U; axis < 4U; axis++) {
                eye[axis] = 0.0f;
                for (row = 0U; row < 4U; row++)
                    eye[axis] += object[row]
                        * cache->quantized_matrices[matrix_index][row][axis];
                destination->processed.eye[axis] = eye[axis];
            }
            for (axis = 0U; axis < 3U; axis++) {
                destination->world[axis] = input->position[axis];
                for (row = 0U; row < 4U; row++)
                    destination->world[axis] +=
                        eye[row] * input->matrix[row][axis];
            }
        }
        for (local_batch = 0U;
                local_batch < query->required_batch_count; local_batch++) {
            GeDamRoomDrawBatch batch =
                cache->template_batches[template_batch_base + local_batch];
            if (batch.first_vertex < template_vertex_base) {
                cache->topology_ready = 0;
                status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
                goto done;
            }
            batch.first_vertex = vertex_count
                + batch.first_vertex - template_vertex_base;
            batch.room_id = input->room_id;
            storage->batches[batch_count + local_batch] = batch;
        }
        vertex_count += query->required_vertex_count;
        batch_count += query->required_batch_count;
    }
    if (vertex_count != cache->required_vertex_count
            || batch_count != cache->required_batch_count) {
        cache->topology_ready = 0;
        status = GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR;
        goto done;
    }
    scene->vertex_count = vertex_count;
    scene->batch_count = batch_count;
    cache->single_pass_builds++;
    status = GE_ORIGINAL_DAM_GUARD_SCENE_OK;

done:
    scene->status = status;
    return status;
}


GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build_cached(
    GeOriginalDamGuardSceneCache *cache,
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    return ge_guard_scene_build_cached_internal(
        cache, model_blob, model_blob_size, NULL, 0U, 0,
        view_to_world, storage, scene);
}

GeOriginalDamGuardSceneStatus
ge_original_dam_guard_scene_build_cached_with_weapons(
    GeOriginalDamGuardSceneCache *cache,
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene)
{
    return ge_guard_scene_build_cached_internal(
        cache, model_blob, model_blob_size, weapon_blob, weapon_blob_size, 1,
        view_to_world, storage, scene);
}

const char *ge_original_dam_guard_scene_status_name(
    GeOriginalDamGuardSceneStatus status)
{
    switch (status) {
    case GE_ORIGINAL_DAM_GUARD_SCENE_OK: return "ok";
    case GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_DAM_GUARD_SCENE_GUARDS_NOT_READY:
        return "guards not ready";
    case GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR:
        return "model layout error";
    case GE_ORIGINAL_DAM_GUARD_SCENE_PIPELINE_ERROR:
        return "pipeline error";
    case GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED:
        return "capacity exceeded";
    }
    return "unknown";
}
