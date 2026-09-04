#include "ge_original_model_scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct GeOriginalModelSceneContext {
    const GeOriginalModelSceneInput *input;
    const GeDamRoomSceneStorage *storage;
    GeDamRoomListKind sequence_kinds[4];
    size_t sequence_count;
    GeOriginalModelSceneStatus status;
    size_t vertex_cursor;
    size_t batch_cursor;
    size_t triangle_count;
    uint16_t *matrix_indices;
    size_t matrix_index_capacity;
    uint16_t slot_matrix_indices[GE_GBI_VERTEX_CACHE_SIZE];
    uint16_t active_matrix_index;
    uint8_t write_output;
} GeOriginalModelSceneContext;

static GeGbiAddress model_address(uint32_t offset)
{
    GeGbiAddress address;
    address.raw = UINT32_C(0x05000000) | offset;
    address.offset = offset;
    address.segment = UINT8_C(5);
    return address;
}

static int add_size(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) return 0;
    *result = left + right;
    return 1;
}

static void write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static int encode_segment3_matrices(const GeOriginalModelSceneInput *input,
                                    uint8_t **bytes, size_t *byte_count)
{
    size_t matrix_index;
    size_t required;
    if (input->segment3_matrices == NULL
            || input->segment3_matrix_count == 0U
            || input->segment3_matrix_count > SIZE_MAX / 64U)
        return 0;
    required = input->segment3_matrix_count * 64U;
    *bytes = malloc(required);
    if (*bytes == NULL) return 0;
    for (matrix_index = 0U;
            matrix_index < input->segment3_matrix_count; ++matrix_index) {
        uint8_t *matrix_bytes = *bytes + matrix_index * 64U;
        size_t element;
        for (element = 0U; element < 16U; ++element) {
            const float value = input->segment3_matrices[matrix_index]
                [element / 4U][element % 4U];
            int32_t fixed;
            if (!isfinite(value) || value < -32768.0f
                    || value >= 32768.0f) {
                free(*bytes);
                *bytes = NULL;
                return 0;
            }
            fixed = (int32_t)(value * 65536.0f);
            write_u16_be(matrix_bytes + element * 2U,
                         (uint16_t)((uint32_t)fixed >> 16));
            write_u16_be(matrix_bytes + 32U + element * 2U,
                         (uint16_t)fixed);
        }
    }
    *byte_count = required;
    return 1;
}

static int collect_model_draw(const GeGbiPipelineEvent *event,
                              void *user_data)
{
    GeOriginalModelSceneContext *context = user_data;
    size_t draw_vertices;
    size_t vertex_end;
    size_t batch_end;
    size_t triangle_end;
    uint8_t triangle_index;

    if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_MATRIX
            && event->action.data.matrix.address.segment == UINT8_C(3)
            && (event->action.data.matrix.address.offset & UINT32_C(63)) == 0U) {
        const uint32_t index = event->action.data.matrix.address.offset / 64U;
        const size_t matrix_count =
            context->input->segment3_matrices != NULL
                ? context->input->segment3_matrix_count : 1U;
        if (index > UINT16_MAX || index >= matrix_count) {
            context->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
            return 0;
        }
        context->active_matrix_index = (uint16_t)index;
        return 1;
    }
    if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_VERTICES) {
        const size_t first = event->action.data.vertices.first;
        const size_t count = event->action.data.vertices.count;
        size_t index;
        if (first > GE_GBI_VERTEX_CACHE_SIZE
                || count > GE_GBI_VERTEX_CACHE_SIZE - first) {
            context->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
            return 0;
        }
        for (index = 0U; index < count; index++)
            context->slot_matrix_indices[first + index] =
                context->active_matrix_index;
        return 1;
    }
    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) return 1;
    if (event->sequence_index >= context->sequence_count) {
        context->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        return 0;
    }
    draw_vertices = (size_t)event->action.data.draw.count * 3U;
    if (!add_size(context->vertex_cursor, draw_vertices, &vertex_end)
            || !add_size(context->batch_cursor, 1U, &batch_end)
            || !add_size(context->triangle_count,
                         event->action.data.draw.count, &triangle_end)) {
        context->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        return 0;
    }
    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            ++triangle_index) {
        const GeGbiTriangle *triangle =
            &event->action.data.draw.triangles[triangle_index];
        uint8_t vertex_index;
        for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
            if (triangle->vertex[vertex_index] >= GE_GBI_VERTEX_CACHE_SIZE) {
                context->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
                return 0;
            }
        }
    }

    if (context->write_output != 0U) {
        GeGbiRenderState render_state;
        GePicaMaterial material;
        GeDamRoomDrawBatch *batch;
        size_t output_index = context->vertex_cursor;
        if (vertex_end > context->storage->vertex_capacity
                || batch_end > context->storage->batch_capacity) {
            context->status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
            return 0;
        }
        /* Sizing consumes draw counts and validates the full display-list
         * pipeline, but has no material output. Translation is a total
         * operation for non-NULL arguments; avoid constructing/discarding
         * it once during preflight and doing it again during publication. */
        render_state = *event->state;
        if (context->input->world_zbuffer_enabled != 0U) {
            /* Exact modelRender world-model contract: zbufferenabled is true and
             * modelApplyRenderModeType1/2/3/4 selects Z compare+update for the
             * primary list, then compare-only for a translucent secondary list.
             * Those commands live in the caller, not the ROM-backed child lists
             * flattened by this adapter.  Restore that inherited state here so a
             * character remains behind authored background depth. */
            render_state.geometry_mode |= UINT32_C(0x00000001);
            render_state.other_mode_low |= UINT32_C(0x00000010);
            if (context->sequence_kinds[event->sequence_index]
                    == GE_DAM_ROOM_LIST_PRIMARY) {
                render_state.other_mode_low |= UINT32_C(0x00000020);
            } else {
                render_state.other_mode_low &= ~UINT32_C(0x00000020);
            }
        }
        if (ge_pica_material_translate(&render_state, &material)
                != GE_PICA_MATERIAL_OK) {
            context->status = GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR;
            return 0;
        }
        batch = &context->storage->batches[context->batch_cursor];
        memset(batch, 0, sizeof(*batch));
        batch->room_id = context->input->room_id;
        batch->list_kind = context->sequence_kinds[event->sequence_index];
        /* Keep the zeroed output padding deterministic: traversal addresses
         * are returned by value and need not carry initialized padding. */
        batch->command_address.raw = event->command_address.raw;
        batch->command_address.offset = event->command_address.offset;
        batch->command_address.segment = event->command_address.segment;
        batch->texture = event->state->rare_texture;
        batch->material = material;
        batch->first_vertex = context->vertex_cursor;
        batch->vertex_count = draw_vertices;
        batch->triangle_count = event->action.data.draw.count;
        batch->texture_valid = event->state->rare_texture_valid != 0U
            ? UINT8_C(1) : UINT8_C(0);
        batch->coordinate_space = GE_DAM_ROOM_COORDINATE_RUNTIME;

        for (triangle_index = 0U;
                triangle_index < event->action.data.draw.count;
                ++triangle_index) {
            const GeGbiTriangle *triangle =
                &event->action.data.draw.triangles[triangle_index];
            uint8_t vertex_index;
            for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
                const uint8_t slot = triangle->vertex[vertex_index];
                const GeGbiVertex *source = &event->vertex_cache[slot];
                GeDamRoomWorldVertex *destination =
                    &context->storage->vertices[output_index++];
                size_t axis;

                if (context->matrix_indices != NULL) {
                    const size_t matrix_output = output_index - 1U;
                    if (matrix_output >= context->matrix_index_capacity) {
                        context->status =
                            GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
                        return 0;
                    }
                    context->matrix_indices[matrix_output] =
                        context->slot_matrix_indices[slot];
                }

                destination->source = *source;
                destination->processed = event->processed_vertex_cache[slot];
                for (axis = 0U; axis < 3U; ++axis) {
                    if (context->input->segment3_matrices != NULL) {
                        size_t row;
                        destination->world[axis] =
                            context->input->position[axis];
                        for (row = 0U; row < 4U; ++row)
                            destination->world[axis]
                                += destination->processed.eye[row]
                                    * context->input->matrix[row][axis];
                    } else {
                        destination->world[axis] =
                            context->input->position[axis]
                            + (float)source->x
                                * context->input->matrix[0][axis]
                            + (float)source->y
                                * context->input->matrix[1][axis]
                            + (float)source->z
                                * context->input->matrix[2][axis];
                    }
                }
            }
        }
    }
    context->vertex_cursor = vertex_end;
    context->batch_cursor = batch_end;
    context->triangle_count = triangle_end;
    return 1;
}

static GeOriginalModelSceneStatus execute_lists(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    const uint32_t offsets[2],
    uint8_t write_output,
    size_t *vertex_cursor,
    size_t *batch_cursor,
    size_t *triangle_count,
    size_t *commands_visited,
    uint16_t *matrix_indices,
    size_t matrix_index_capacity)
{
    static const uint8_t identity_matrix_be[64] = {
        0x00,0x01,0x00,0x00, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0x00,0x01,0x00,0x00, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0x00,0x01,0x00,0x00, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0x00,0x01,0x00,0x00
    };
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig traversal = {8U, 4096U};
    GeOriginalModelSceneContext context;
    GeGbiPipelineResult pipeline;
    GeGbiAddress roots[4];
    size_t root_count = 0U;
    size_t list_index;
    uint8_t *matrix_segment = NULL;
    size_t matrix_segment_size = 0U;

    for (list_index = 0U; list_index < 2U; ++list_index) {
        const uint32_t offset = offsets[list_index];
        if (offset == GE_ORIGINAL_MODEL_SCENE_NO_LIST) continue;
        if ((size_t)offset >= input->blob_size
                || (offset & UINT32_C(7)) != 0U)
            return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        if (input->parent_setup_enabled != 0U) {
            const uint32_t setup_offset = (uint32_t)list_index * 128U;
            roots[root_count] = (GeGbiAddress){
                .raw = UINT32_C(0x02000000) | setup_offset,
                .offset = setup_offset, .segment = 2U};
            context.sequence_kinds[root_count++] = list_index == 0U
                ? GE_DAM_ROOM_LIST_PRIMARY : GE_DAM_ROOM_LIST_SECONDARY;
        }
        roots[root_count] = model_address(offset);
        context.sequence_kinds[root_count] = list_index == 0U
            ? GE_DAM_ROOM_LIST_PRIMARY : GE_DAM_ROOM_LIST_SECONDARY;
        ++root_count;
    }
    if (root_count == 0U) return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    ge_gbi_memory_map_init(&memory);
    if (input->parent_setup_enabled != 0U
            && ge_gbi_memory_map_set_segment(&memory, 2U,
                &input->parent_setup[0][0], sizeof(input->parent_setup))
                != GE_GBI_RESOLVE_OK)
        return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    if (ge_gbi_memory_map_set_segment(
            &memory, UINT8_C(5), input->blob, input->blob_size)
            != GE_GBI_RESOLVE_OK)
        return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    if (input->segment3_matrices != NULL) {
        if (!encode_segment3_matrices(input, &matrix_segment,
                                      &matrix_segment_size))
            return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    }
    if (ge_gbi_memory_map_set_segment(
            &memory, UINT8_C(3),
            matrix_segment != NULL ? matrix_segment : identity_matrix_be,
            matrix_segment != NULL ? matrix_segment_size
                                   : sizeof(identity_matrix_be))
            != GE_GBI_RESOLVE_OK) {
        free(matrix_segment);
        return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    }
    if (input->segment4_offset != GE_ORIGINAL_MODEL_SCENE_NO_LIST) {
        if ((size_t)input->segment4_offset >= input->blob_size
                || ge_gbi_memory_map_set_segment(
                    &memory, UINT8_C(4),
                    input->blob + input->segment4_offset,
                    input->blob_size - input->segment4_offset)
                    != GE_GBI_RESOLVE_OK)
            {
                free(matrix_segment);
                return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
            }
    }
    context.input = input;
    context.storage = storage;
    context.sequence_count = root_count;
    context.status = GE_ORIGINAL_MODEL_SCENE_OK;
    context.vertex_cursor = *vertex_cursor;
    context.batch_cursor = *batch_cursor;
    context.triangle_count = *triangle_count;
    context.matrix_indices = matrix_indices;
    context.matrix_index_capacity = matrix_index_capacity;
    memset(context.slot_matrix_indices, 0,
           sizeof(context.slot_matrix_indices));
    context.active_matrix_index = 0U;
    context.write_output = write_output;
    pipeline = ge_gbi_pipeline_execute_sequence(
        &memory, roots, root_count, GE_GBI_BYTE_ORDER_BIG_ENDIAN,
        &traversal, collect_model_draw, &context);
    free(matrix_segment);
    if (context.status != GE_ORIGINAL_MODEL_SCENE_OK) return context.status;
    if (pipeline.status != GE_GBI_PIPELINE_OK
            || pipeline.unsupported_commands != 0U)
        return GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR;
    if (!add_size(*commands_visited, pipeline.traversal.commands_visited,
                  commands_visited))
        return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    *vertex_cursor = context.vertex_cursor;
    *batch_cursor = context.batch_cursor;
    *triangle_count = context.triangle_count;
    return GE_ORIGINAL_MODEL_SCENE_OK;
}

static int input_valid(const GeOriginalModelSceneInput *input,
                       const GeDamRoomSceneStorage *storage)
{
    size_t row;
    size_t column;
    if (input == NULL || input->blob == NULL || input->blob_size == 0U
            || storage == NULL
            || (storage->vertex_capacity != 0U && storage->vertices == NULL)
            || (storage->batch_capacity != 0U && storage->batches == NULL))
        return 0;
    if ((input->segment3_matrices == NULL)
            != (input->segment3_matrix_count == 0U))
        return 0;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (!isfinite(input->matrix[row][column])) return 0;
    for (row = 0U; row < 3U; ++row)
        if (!isfinite(input->position[row])) return 0;
    return input->primary_offset != GE_ORIGINAL_MODEL_SCENE_NO_LIST
        || input->secondary_offset != GE_ORIGINAL_MODEL_SCENE_NO_LIST;
}

static GeOriginalModelSceneStatus ge_original_model_scene_build_internal(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    uint16_t *matrix_indices, size_t matrix_index_capacity,
    const GeOriginalModelScene *query,
    GeOriginalModelScene *scene)
{
    const GeDamRoomSceneStorage empty = {NULL, 0U, NULL, 0U};
    const GeDamRoomSceneStorage *actual_storage =
        storage != NULL ? storage : &empty;
    const uint32_t offsets[2] = {
        input != NULL ? input->primary_offset : GE_ORIGINAL_MODEL_SCENE_NO_LIST,
        input != NULL ? input->secondary_offset : GE_ORIGINAL_MODEL_SCENE_NO_LIST
    };
    size_t vertices = 0U;
    size_t batches = 0U;
    size_t triangles = 0U;
    size_t commands = 0U;
    size_t list_count = 0U;
    size_t list_index;
    GeOriginalModelSceneStatus status;
    /* Callers may reuse their query variable for the published scene. */
    GeOriginalModelScene expected = {0};

    if (scene == NULL) return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    if (query != NULL) expected = *query;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    if (!input_valid(input, actual_storage)) return scene->status;

    for (list_index = 0U; list_index < 2U; ++list_index)
        if (offsets[list_index] != GE_ORIGINAL_MODEL_SCENE_NO_LIST)
            ++list_count;
    if (query != NULL) {
        if ((expected.status != GE_ORIGINAL_MODEL_SCENE_OK
                && expected.status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
                || expected.list_count != list_count
                || expected.triangle_count > SIZE_MAX / 3U
                || expected.required_vertex_count != expected.triangle_count * 3U)
            return scene->status;
        vertices = expected.required_vertex_count;
        batches = expected.required_batch_count;
        triangles = expected.triangle_count;
        commands = expected.commands_visited;
    } else {
        status = execute_lists(input, actual_storage, offsets, UINT8_C(0),
            &vertices, &batches, &triangles, &commands, NULL, 0U);
        if (status != GE_ORIGINAL_MODEL_SCENE_OK) {
            scene->status = status;
            return status;
        }
    }
    scene->list_count = list_count;
    scene->required_vertex_count = vertices;
    scene->required_batch_count = batches;
    scene->triangle_count = triangles;
    scene->commands_visited = commands;
    if (vertices > actual_storage->vertex_capacity
            || batches > actual_storage->batch_capacity
            || (matrix_indices != NULL
                && vertices > matrix_index_capacity)) {
        scene->status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
        return scene->status;
    }

    vertices = 0U;
    batches = 0U;
    triangles = 0U;
    commands = 0U;
    status = execute_lists(input, actual_storage, offsets, UINT8_C(1),
        &vertices, &batches, &triangles, &commands,
        matrix_indices, matrix_index_capacity);
    if (status != GE_ORIGINAL_MODEL_SCENE_OK) {
        scene->status = status;
        return status;
    }
    if (vertices != scene->required_vertex_count
            || batches != scene->required_batch_count
            || triangles != scene->triangle_count
            || commands != scene->commands_visited) {
        scene->status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        return scene->status;
    }
    scene->vertex_count = vertices;
    scene->batch_count = batches;
    scene->triangle_count = triangles;
    scene->commands_visited = commands;
    scene->status = GE_ORIGINAL_MODEL_SCENE_OK;
    return scene->status;
}

GeOriginalModelSceneStatus ge_original_model_scene_build(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    GeOriginalModelScene *scene)
{
    return ge_original_model_scene_build_internal(
        input, storage, NULL, 0U, NULL, scene);
}

GeOriginalModelSceneStatus ge_original_model_scene_build_preflighted(
    const GeOriginalModelSceneInput *input,
    const GeOriginalModelScene *query,
    const GeDamRoomSceneStorage *storage,
    GeOriginalModelScene *scene)
{
    if (query == NULL) {
        if (scene != NULL) {
            memset(scene, 0, sizeof(*scene));
            scene->status = GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
        }
        return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    }
    return ge_original_model_scene_build_internal(
        input, storage, NULL, 0U, query, scene);
}

GeOriginalModelSceneStatus ge_original_model_scene_build_matrix_template(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    uint16_t *matrix_indices, size_t matrix_index_capacity,
    GeOriginalModelScene *scene)
{
    if (matrix_indices == NULL && matrix_index_capacity != 0U)
        return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    return ge_original_model_scene_build_internal(
        input, storage, matrix_indices, matrix_index_capacity, NULL, scene);
}

GeOriginalModelSceneStatus ge_original_model_scene_build_matrix_template_preflighted(
    const GeOriginalModelSceneInput *input,
    const GeOriginalModelScene *query,
    const GeDamRoomSceneStorage *storage,
    uint16_t *matrix_indices, size_t matrix_index_capacity,
    GeOriginalModelScene *scene)
{
    if (query == NULL || (matrix_indices == NULL && matrix_index_capacity != 0U)) {
        if (scene != NULL) {
            memset(scene, 0, sizeof(*scene));
            scene->status = GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
        }
        return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    }
    return ge_original_model_scene_build_internal(
        input, storage, matrix_indices, matrix_index_capacity, query, scene);
}

static uint64_t cache_hash_u64(uint64_t hash, uint64_t value)
{
    size_t byte;
    for (byte = 0U; byte < 8U; ++byte) {
        hash ^= (uint8_t)(value >> (byte * 8U));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t cache_topology_signature(
    const GeOriginalModelSceneInput *inputs, size_t input_count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    hash = cache_hash_u64(hash, input_count);
    for (index = 0U; index < input_count; ++index) {
        const GeOriginalModelSceneInput *input = &inputs[index];
        hash = cache_hash_u64(hash, (uint64_t)(uintptr_t)input->blob);
        hash = cache_hash_u64(hash, input->blob_size);
        hash = cache_hash_u64(hash, input->primary_offset);
        hash = cache_hash_u64(hash, input->secondary_offset);
        hash = cache_hash_u64(hash, input->segment4_offset);
        hash = cache_hash_u64(hash, input->segment3_matrix_count);
        hash = cache_hash_u64(hash, (uint64_t)input->world_zbuffer_enabled
            | (uint64_t)input->parent_setup_enabled << 8);
        if (input->parent_setup_enabled != 0U) {
            size_t byte;
            for (byte = 0U; byte < sizeof(input->parent_setup); ++byte) {
                hash ^= ((const uint8_t *)input->parent_setup)[byte];
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    return hash;
}

/* A live room commonly cycles among more than three aggregate character
 * topologies as LOD, held-weapon, injury and death relations change.  The
 * immutable per-component cache keeps each retained aggregate modest, while
 * eight variants cover the observed combat working set without unbounded
 * growth or changing any canonical model state. */
#define GE_ORIGINAL_MODEL_SCENE_TOPOLOGY_VARIANTS 8U

typedef struct GeOriginalModelSceneTopologyVariant {
    void *storage;
    size_t storage_bytes;
    GeOriginalModelScene *queries;
    size_t *input_component_indices;
    size_t *input_quantized_matrix_offsets;
    uint64_t *input_quantized_matrix_hashes;
    uint64_t *input_publication_signatures;
    uint64_t *published_input_publication_signatures;
    size_t *input_vertex_offsets;
    size_t *input_batch_offsets;
    size_t input_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t triangle_count;
    size_t commands_visited;
    uint64_t topology_signature;
} GeOriginalModelSceneTopologyVariant;

/* Aggregate guard topology changes whenever one authored model relation
 * switches display lists.  Caching only whole aggregate scenes makes those
 * independent switches combinatorial.  Retain each immutable input topology
 * once instead: a new aggregate needs only an ordered set of component
 * indices, while current matrices, positions and rooms are republished below. */
typedef struct GeOriginalModelSceneTopologyComponent {
    const uint8_t *blob;
    size_t blob_size;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t segment4_offset;
    size_t segment3_matrix_count;
    uint8_t world_zbuffer_enabled;
    uint8_t parent_setup_enabled;
    uint8_t parent_setup[2][128];
    GeOriginalModelScene query;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    uint16_t *matrix_indices;
    uint32_t *transform_sources;
} GeOriginalModelSceneTopologyComponent;

static int cache_component_matches(
    const GeOriginalModelSceneTopologyComponent *component,
    const GeOriginalModelSceneInput *input)
{
    return component->blob == input->blob
        && component->blob_size == input->blob_size
        && component->primary_offset == input->primary_offset
        && component->secondary_offset == input->secondary_offset
        && component->segment4_offset == input->segment4_offset
        && component->segment3_matrix_count == input->segment3_matrix_count
        && component->world_zbuffer_enabled
            == input->world_zbuffer_enabled
        && component->parent_setup_enabled == input->parent_setup_enabled
        && (input->parent_setup_enabled == 0U
            || memcmp(component->parent_setup, input->parent_setup,
                sizeof(input->parent_setup)) == 0);
}

static GeOriginalModelSceneTopologyComponent *cache_find_component(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *input)
{
    GeOriginalModelSceneTopologyComponent *components =
        cache->topology_components;
    size_t index;
    for (index = 0U; index < cache->topology_component_count; ++index)
        if (cache_component_matches(&components[index], input))
            return &components[index];
    return NULL;
}

static void cache_component_free(
    GeOriginalModelSceneTopologyComponent *component)
{
    if (component == NULL) return;
    free(component->vertices);
    free(component->batches);
    free(component->matrix_indices);
    free(component->transform_sources);
    memset(component, 0, sizeof(*component));
}

int ge_original_model_scene_cache_template_view(
    const GeOriginalModelSceneCache *cache, size_t input_index,
    GeOriginalModelSceneTemplateView *view)
{
    if (view == NULL) return 0;
    memset(view, 0, sizeof(*view));
    if (cache == NULL || !cache->topology_ready
            || input_index >= cache->input_count
            || cache->input_component_indices == NULL
            || cache->input_component_indices[input_index]
                >= cache->topology_component_count) return 0;
    const GeOriginalModelSceneTopologyComponent *component =
        &((const GeOriginalModelSceneTopologyComponent *)cache->topology_components)
            [cache->input_component_indices[input_index]];
    view->vertices = component->vertices;
    view->batches = component->batches;
    view->matrix_indices = component->matrix_indices;
    view->transform_sources = component->transform_sources;
    return 1;
}

static void cache_build_component_transform_sources(
    const GeDamRoomWorldVertex *vertices, const uint16_t *matrix_indices,
    size_t vertex_count, uint32_t *transform_sources)
{
    size_t table_capacity = 1U;
    uint32_t *table = NULL;
    size_t vertex_index;

    while (vertex_count <= SIZE_MAX / 2U
            && table_capacity < vertex_count * 2U
            && table_capacity <= SIZE_MAX / 2U)
        table_capacity *= 2U;
    if (vertex_count != 0U && table_capacity >= vertex_count
            && table_capacity <= SIZE_MAX / sizeof(*table))
        table = calloc(table_capacity, sizeof(*table));
    for (vertex_index = 0U; vertex_index < vertex_count; ++vertex_index) {
        const GeGbiVertex *source = &vertices[vertex_index].source;
        uint32_t first = (uint32_t)vertex_index;
        if (table != NULL) {
            uint32_t hash = UINT32_C(2166136261);
            size_t slot;
            hash = (hash ^ (uint16_t)source->x) * UINT32_C(16777619);
            hash = (hash ^ (uint16_t)source->y) * UINT32_C(16777619);
            hash = (hash ^ (uint16_t)source->z) * UINT32_C(16777619);
            hash = (hash ^ matrix_indices[vertex_index])
                * UINT32_C(16777619);
            slot = (size_t)hash & (table_capacity - 1U);
            while (table[slot] != 0U) {
                const size_t candidate = (size_t)table[slot] - 1U;
                const GeGbiVertex *candidate_source =
                    &vertices[candidate].source;
                if (matrix_indices[candidate] == matrix_indices[vertex_index]
                        && candidate_source->x == source->x
                        && candidate_source->y == source->y
                        && candidate_source->z == source->z) {
                    first = (uint32_t)candidate;
                    break;
                }
                slot = (slot + 1U) & (table_capacity - 1U);
            }
            if (table[slot] == 0U)
                table[slot] = (uint32_t)vertex_index + 1U;
        }
        transform_sources[vertex_index] = first;
    }
    free(table);
}

static GeOriginalModelSceneStatus cache_get_or_build_component(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *input,
    GeOriginalModelSceneTopologyComponent **result)
{
    GeOriginalModelSceneTopologyComponent *component;
    GeOriginalModelSceneTopologyComponent *components;
    GeOriginalModelScene query;
    GeOriginalModelScene built;
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    uint16_t *matrix_indices = NULL;
    uint32_t *transform_sources = NULL;
    GeDamRoomSceneStorage storage;
    GeOriginalModelSceneStatus status;
    size_t capacity;
    size_t payload_bytes = 0U;

    component = cache_find_component(cache, input);
    if (component != NULL) {
        cache->topology_component_hits++;
        *result = component;
        return GE_ORIGINAL_MODEL_SCENE_OK;
    }
    cache->topology_component_misses++;
    status = ge_original_model_scene_build(input, NULL, &query);
    if (!((status == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
                || (status == GE_ORIGINAL_MODEL_SCENE_OK
                    && query.required_vertex_count == 0U
                    && query.required_batch_count == 0U)))
        return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
    if (query.required_vertex_count > UINT32_MAX
            || query.required_vertex_count
                > SIZE_MAX / sizeof(*vertices)
            || query.required_vertex_count
                > SIZE_MAX / sizeof(*matrix_indices)
            || query.required_batch_count > SIZE_MAX / sizeof(*batches))
        return GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
    if (query.required_vertex_count != 0U) {
        vertices = malloc(query.required_vertex_count * sizeof(*vertices));
        matrix_indices = malloc(
            query.required_vertex_count * sizeof(*matrix_indices));
        transform_sources = malloc(
            query.required_vertex_count * sizeof(*transform_sources));
        if (vertices == NULL || matrix_indices == NULL
                || transform_sources == NULL) goto no_memory;
    }
    if (query.required_batch_count != 0U) {
        batches = malloc(query.required_batch_count * sizeof(*batches));
        if (batches == NULL) goto no_memory;
    }
    storage = (GeDamRoomSceneStorage){
        vertices, query.required_vertex_count,
        batches, query.required_batch_count,
    };
    status = ge_original_model_scene_build_matrix_template_preflighted(
        input, &query, &storage, matrix_indices, query.required_vertex_count, &built);
    if (status != GE_ORIGINAL_MODEL_SCENE_OK
            || built.vertex_count != query.required_vertex_count
            || built.batch_count != query.required_batch_count
            || built.triangle_count != query.triangle_count) {
        status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        goto failed;
    }
    cache_build_component_transform_sources(
        vertices, matrix_indices, built.vertex_count, transform_sources);
    if (cache->topology_component_count
            == cache->topology_component_capacity) {
        capacity = cache->topology_component_capacity != 0U
            ? cache->topology_component_capacity * 2U : 32U;
        if (capacity < cache->topology_component_capacity
                || capacity > SIZE_MAX / sizeof(*components))
            goto no_memory;
        components = realloc(cache->topology_components,
                             capacity * sizeof(*components));
        if (components == NULL) goto no_memory;
        memset(components + cache->topology_component_capacity, 0,
               (capacity - cache->topology_component_capacity)
                   * sizeof(*components));
        cache->topology_components = components;
        cache->topology_component_capacity = capacity;
    }
    components = cache->topology_components;
    component = &components[cache->topology_component_count++];
    component->blob = input->blob;
    component->blob_size = input->blob_size;
    component->primary_offset = input->primary_offset;
    component->secondary_offset = input->secondary_offset;
    component->segment4_offset = input->segment4_offset;
    component->segment3_matrix_count = input->segment3_matrix_count;
    component->world_zbuffer_enabled = input->world_zbuffer_enabled;
    component->parent_setup_enabled = input->parent_setup_enabled;
    memcpy(component->parent_setup, input->parent_setup,
           sizeof(input->parent_setup));
    component->query = built;
    component->vertices = vertices;
    component->batches = batches;
    component->matrix_indices = matrix_indices;
    component->transform_sources = transform_sources;
    cache->topology_transform_maps_built++;
    payload_bytes = sizeof(*component)
        + built.vertex_count * sizeof(*vertices)
        + built.vertex_count * sizeof(*matrix_indices)
        + built.vertex_count * sizeof(*transform_sources)
        + built.batch_count * sizeof(*batches);
    if (payload_bytes <= SIZE_MAX - cache->topology_component_bytes)
        cache->topology_component_bytes += payload_bytes;
    *result = component;
    return GE_ORIGINAL_MODEL_SCENE_OK;

no_memory:
    status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
failed:
    free(transform_sources);
    free(matrix_indices);
    free(batches);
    free(vertices);
    return status;
}

static void cache_variant_free(GeOriginalModelSceneTopologyVariant *variant)
{
    if (variant == NULL) return;
    free(variant->storage);
    memset(variant, 0, sizeof(*variant));
}

static void cache_export_topology(
    const GeOriginalModelSceneCache *cache,
    GeOriginalModelSceneTopologyVariant *variant)
{
    memset(variant, 0, sizeof(*variant));
    variant->storage = cache->topology_storage;
    variant->storage_bytes = cache->topology_storage_bytes;
    variant->queries = cache->queries;
    variant->input_component_indices = cache->input_component_indices;
    variant->input_quantized_matrix_offsets =
        cache->input_quantized_matrix_offsets;
    variant->input_quantized_matrix_hashes =
        cache->input_quantized_matrix_hashes;
    variant->input_publication_signatures =
        cache->input_publication_signatures;
    variant->published_input_publication_signatures =
        cache->published_input_publication_signatures;
    variant->input_vertex_offsets = cache->input_vertex_offsets;
    variant->input_batch_offsets = cache->input_batch_offsets;
    variant->input_count = cache->input_count;
    variant->required_vertex_count = cache->required_vertex_count;
    variant->required_batch_count = cache->required_batch_count;
    variant->triangle_count = cache->triangle_count;
    variant->commands_visited = cache->commands_visited;
    variant->topology_signature = cache->topology_signature;
}

static void cache_import_topology(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneTopologyVariant *variant)
{
    cache->topology_storage = variant->storage;
    cache->topology_storage_bytes = variant->storage_bytes;
    cache->queries = variant->queries;
    cache->input_component_indices = variant->input_component_indices;
    cache->input_quantized_matrix_offsets =
        variant->input_quantized_matrix_offsets;
    cache->input_quantized_matrix_hashes =
        variant->input_quantized_matrix_hashes;
    cache->input_publication_signatures =
        variant->input_publication_signatures;
    cache->published_input_publication_signatures =
        variant->published_input_publication_signatures;
    cache->input_vertex_offsets = variant->input_vertex_offsets;
    cache->input_batch_offsets = variant->input_batch_offsets;
    cache->input_count = variant->input_count;
    cache->required_vertex_count = variant->required_vertex_count;
    cache->required_batch_count = variant->required_batch_count;
    cache->triangle_count = variant->triangle_count;
    cache->commands_visited = variant->commands_visited;
    cache->topology_signature = variant->topology_signature;
    cache->topology_ready = UINT8_C(1);
}

static void cache_clear_topology_references(GeOriginalModelSceneCache *cache)
{
    cache->topology_storage = NULL;
    cache->topology_storage_bytes = 0U;
    cache->queries = NULL;
    cache->input_component_indices = NULL;
    cache->input_quantized_matrix_offsets = NULL;
    cache->input_quantized_matrix_hashes = NULL;
    cache->input_publication_signatures = NULL;
    cache->published_input_publication_signatures = NULL;
    cache->input_vertex_offsets = NULL;
    cache->input_batch_offsets = NULL;
    cache->input_count = 0U;
    cache->required_vertex_count = 0U;
    cache->required_batch_count = 0U;
    cache->triangle_count = 0U;
    cache->commands_visited = 0U;
    cache->topology_signature = 0U;
    cache->topology_ready = UINT8_C(0);
}

static int cache_select_retained_topology(
    GeOriginalModelSceneCache *cache, uint64_t signature,
    size_t input_count)
{
    GeOriginalModelSceneTopologyVariant *variants =
        cache->topology_variants;
    size_t index;
    if (variants == NULL) return 0;
    for (index = 0U; index < cache->topology_variant_count; ++index) {
        GeOriginalModelSceneTopologyVariant outgoing;
        if (variants[index].topology_signature != signature
                || variants[index].input_count != input_count) continue;
        cache_export_topology(cache, &outgoing);
        cache_import_topology(cache, &variants[index]);
        variants[index] = outgoing;
        cache->publication_ready = UINT8_C(0);
        cache->topology_variant_hits++;
        return 1;
    }
    return 0;
}

static void cache_retain_current_topology(GeOriginalModelSceneCache *cache)
{
    GeOriginalModelSceneTopologyVariant *variants;
    size_t slot;
    if (cache == NULL || cache->topology_ready == 0U) return;
    variants = cache->topology_variants;
    if (variants == NULL) {
        variants = calloc(GE_ORIGINAL_MODEL_SCENE_TOPOLOGY_VARIANTS,
                          sizeof(*variants));
        if (variants == NULL) return;
        cache->topology_variants = variants;
    }
    if (cache->topology_variant_count
            < GE_ORIGINAL_MODEL_SCENE_TOPOLOGY_VARIANTS) {
        slot = cache->topology_variant_count++;
    } else {
        slot = cache->topology_variant_next++
            % GE_ORIGINAL_MODEL_SCENE_TOPOLOGY_VARIANTS;
        cache_variant_free(&variants[slot]);
        cache->topology_variant_evictions++;
    }
    cache_export_topology(cache, &variants[slot]);
    cache_clear_topology_references(cache);
    cache->publication_ready = UINT8_C(0);
}

void ge_original_model_scene_cache_close(GeOriginalModelSceneCache *cache)
{
    GeOriginalModelSceneTopologyVariant *variants;
    GeOriginalModelSceneTopologyComponent *components;
    size_t index;
    if (cache == NULL) return;
    variants = cache->topology_variants;
    for (index = 0U; index < cache->topology_variant_count; ++index)
        cache_variant_free(&variants[index]);
    free(variants);
    components = cache->topology_components;
    for (index = 0U; index < cache->topology_component_count; ++index)
        cache_component_free(&components[index]);
    free(components);
    free(cache->topology_storage);
    free(cache->quantized_matrices);
    free(cache->publication_ranges);
    free(cache->publication_layout_scratch);
    memset(cache, 0, sizeof(*cache));
}

void ge_original_model_scene_cache_bind_profile_clock(
    GeOriginalModelSceneCache *cache,
    GeOriginalModelSceneProfileClock clock, void *context)
{
    if (cache == NULL) return;
    cache->profile_clock = clock;
    cache->profile_clock_context = context;
}

static uint64_t cache_profile_now(const GeOriginalModelSceneCache *cache)
{
    return cache != NULL && cache->profile_clock != NULL
        ? cache->profile_clock(cache->profile_clock_context) : 0U;
}

static void cache_profile_add(uint64_t *total, uint64_t start, uint64_t end)
{
    if (total != NULL && end >= start) *total += end - start;
}

/* Lay out typed SoA slices without assuming that size_t and uint64_t have
 * the same alignment (they do not on ARM). Each append is overflow checked
 * before allocation or pointer arithmetic. No canonical model data lives in
 * this block; variants retain only renderer queries, offsets and hashes. */
static int cache_append_topology_storage(size_t *bytes, size_t count,
    size_t element_size, size_t alignment, size_t *offset)
{
    const size_t padding = (*bytes % alignment) != 0U
        ? alignment - (*bytes % alignment) : 0U;
    if (count > SIZE_MAX / element_size
            || !add_size(*bytes, padding, offset)
            || !add_size(*offset, count * element_size, bytes)) return 0;
    return 1;
}

static GeOriginalModelSceneStatus cache_rebuild_templates(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    uint64_t signature)
{
    uint8_t *topology_storage = NULL;
    size_t topology_storage_bytes = 0U;
    GeOriginalModelScene *queries = NULL;
    size_t *component_indices = NULL;
    size_t *vertex_offsets = NULL;
    size_t *batch_offsets = NULL;
    size_t *matrix_offsets = NULL;
    uint64_t *matrix_hashes = NULL;
    uint64_t *publication_signatures = NULL;
    uint64_t *published_publication_signatures = NULL;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t triangle_count = 0U;
    size_t commands_visited = 0U;
    size_t input_index;

    if (input_count != 0U) {
        size_t query_offset, component_offset, vertex_offset, batch_offset;
        size_t matrix_offset, hash_offset, signature_offset, published_offset;
        if (!cache_append_topology_storage(&topology_storage_bytes, input_count,
                sizeof(*queries), _Alignof(GeOriginalModelScene), &query_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(size_t), _Alignof(size_t), &component_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(size_t), _Alignof(size_t), &vertex_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(size_t), _Alignof(size_t), &batch_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(size_t), _Alignof(size_t), &matrix_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(uint64_t), _Alignof(uint64_t), &hash_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(uint64_t), _Alignof(uint64_t), &signature_offset)
                || !cache_append_topology_storage(&topology_storage_bytes, input_count,
                    sizeof(uint64_t), _Alignof(uint64_t), &published_offset))
            goto no_memory;
        topology_storage = calloc(1U, topology_storage_bytes);
        if (topology_storage == NULL) goto no_memory;
        queries = (GeOriginalModelScene *)(topology_storage + query_offset);
        component_indices = (size_t *)(topology_storage + component_offset);
        vertex_offsets = (size_t *)(topology_storage + vertex_offset);
        batch_offsets = (size_t *)(topology_storage + batch_offset);
        matrix_offsets = (size_t *)(topology_storage + matrix_offset);
        matrix_hashes = (uint64_t *)(topology_storage + hash_offset);
        publication_signatures = (uint64_t *)(topology_storage + signature_offset);
        published_publication_signatures = (uint64_t *)(topology_storage + published_offset);
    }
    for (input_index = 0U; input_index < input_count; ++input_index) {
        GeOriginalModelScene *query = &queries[input_index];
        GeOriginalModelSceneTopologyComponent *component = NULL;
        const GeOriginalModelSceneStatus component_status =
            cache_get_or_build_component(
                cache, &inputs[input_index], &component);
        if (component_status != GE_ORIGINAL_MODEL_SCENE_OK
                || component == NULL) {
            if (component_status
                    == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
                goto no_memory;
            goto invalid_layout;
        }
        *query = component->query;
        component_indices[input_index] = (size_t)(component
            - (GeOriginalModelSceneTopologyComponent *)cache->topology_components);
        if (query->required_vertex_count > SIZE_MAX - vertex_count
                || query->required_batch_count > SIZE_MAX - batch_count
                || query->triangle_count > SIZE_MAX - triangle_count
                || query->commands_visited > SIZE_MAX - commands_visited)
            goto invalid_layout;
        vertex_offsets[input_index] = vertex_count;
        batch_offsets[input_index] = batch_count;
        vertex_count += query->required_vertex_count;
        batch_count += query->required_batch_count;
        triangle_count += query->triangle_count;
        commands_visited += query->commands_visited;
    }
    if (vertex_count > UINT32_MAX) goto no_memory;
    /* Aggregate variants own only input order and offsets. Components already
     * own these immutable payloads, so no vertex/material/map concatenation
     * is needed when one authored relation changes. */
    cache->topology_transform_map_vertices_reused += vertex_count;
    free(cache->topology_storage);
    cache->topology_storage = topology_storage;
    cache->topology_storage_bytes = topology_storage_bytes;
    cache->queries = queries;
    cache->input_component_indices = component_indices;
    cache->input_vertex_offsets = vertex_offsets;
    cache->input_batch_offsets = batch_offsets;
    cache->input_quantized_matrix_offsets = matrix_offsets;
    cache->input_quantized_matrix_hashes = matrix_hashes;
    cache->input_publication_signatures = publication_signatures;
    cache->published_input_publication_signatures =
        published_publication_signatures;
    cache->input_count = input_count;
    cache->required_vertex_count = vertex_count;
    cache->required_batch_count = batch_count;
    cache->triangle_count = triangle_count;
    cache->commands_visited = commands_visited;
    cache->topology_signature = signature;
    cache->topology_ready = UINT8_C(1);
    cache->publication_ready = UINT8_C(0);
    cache->topology_rebuilds++;
    return GE_ORIGINAL_MODEL_SCENE_OK;

no_memory:
    free(topology_storage);
    return GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
invalid_layout:
    free(topology_storage);
    return GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
}

static int cache_reserve_matrices(
    GeOriginalModelSceneCache *cache, size_t required)
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

static float cache_quantize_matrix_element(float value)
{
    /* The caller validates finite [-32768,32768) values before conversion.
     * Scaling a binary32 value by 2^16 is exact and stays in signed 32 bits.
     * Use ARM VFP's native truncation instead of a soft 64-bit conversion
     * call for every element of every animated model matrix. */
    const int32_t fixed = (int32_t)(value * 65536.0f);
    return (float)fixed / 65536.0f;
}

static int cache_matrix_is_identity(const float matrix[4][4])
{
    size_t row;
    size_t column;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (matrix[row][column]
                    != (row == column ? 1.0f : 0.0f)) return 0;
    return 1;
}

static uint64_t cache_hash_float(uint64_t hash, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return cache_hash_u64(hash, bits);
}

static int cache_matrix_bank_matches(
    const GeOriginalModelSceneInput *left,
    const GeOriginalModelSceneInput *right)
{
    return left->segment3_matrices == right->segment3_matrices
        && left->segment3_matrix_count == right->segment3_matrix_count;
}

static size_t cache_find_matrix_bank_owner(
    const GeOriginalModelSceneInput *inputs, size_t index,
    const uint64_t *resolved_owners)
{
    if (index != 0U && cache_matrix_bank_matches(&inputs[index], &inputs[index - 1U])) {
        /* During this first pass the previous slot still holds its earliest
         * owner (or UINT64_MAX for a new bank), not its eventual matrix hash.
         * Consecutive parts can reuse that exact search result. */
        const uint64_t owner = resolved_owners[index - 1U];
        return owner != UINT64_MAX ? (size_t)owner : index - 1U;
    }
    size_t prior;
    for (prior = 0U; prior < index; ++prior)
        if (cache_matrix_bank_matches(&inputs[index], &inputs[prior])) break;
    return prior;
}

/* Prepare every distinct canonical segment-3 bank once. Related body, head,
 * hat, and weapon scene parts commonly share the same model-instance bank;
 * the old publication pass converted and hashed all 16 elements once for
 * every part, then converted them all a second time for transformation. */
static GeOriginalModelSceneStatus cache_prepare_publication_matrices(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    uint64_t topology_signature, uint64_t *signature,
    uint64_t *quantization_ticks)
{
    uint64_t hash = cache_hash_u64(
        UINT64_C(1469598103934665603), topology_signature);
    size_t required_matrices = 0U;
    size_t input_index;
    if (cache == NULL || signature == NULL || quantization_ticks == NULL)
        return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    *quantization_ticks = 0U;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        const size_t matrix_count = input->segment3_matrices != NULL
            ? input->segment3_matrix_count : 1U;
        const size_t prior = cache_find_matrix_bank_owner(
            inputs, input_index, cache->input_quantized_matrix_hashes);
        if (prior < input_index) {
            cache->input_quantized_matrix_offsets[input_index] =
                cache->input_quantized_matrix_offsets[prior];
            cache->shared_matrix_banks_reused++;
        } else {
            cache->input_quantized_matrix_offsets[input_index] =
                required_matrices;
            if (!add_size(required_matrices, matrix_count,
                          &required_matrices))
                return GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
        }
        /* This array is topology-owned scratch until the second pass writes
         * the canonical matrix hashes. Retain the already-resolved owner so
         * related body/head/weapon parts do not repeat the O(n^2) pointer
         * search a second time every animated frame. */
        cache->input_quantized_matrix_hashes[input_index] =
            prior < input_index ? (uint64_t)prior : UINT64_MAX;
    }
    if (!cache_reserve_matrices(cache, required_matrices))
        return GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        const size_t matrix_offset =
            cache->input_quantized_matrix_offsets[input_index];
        uint64_t matrix_hash = cache_hash_u64(
            UINT64_C(1469598103934665603),
            input->segment3_matrices != NULL
                ? input->segment3_matrix_count : 0U);
        size_t matrix_index;
        size_t row;
        size_t column;
        const size_t prior =
            (size_t)cache->input_quantized_matrix_hashes[input_index];
        if (prior < input_index) {
            matrix_hash = cache->input_quantized_matrix_hashes[prior];
        } else if (input->segment3_matrices == NULL) {
            const uint64_t quantization_start = cache_profile_now(cache);
            for (row = 0U; row < 4U; ++row)
                for (column = 0U; column < 4U; ++column)
                    cache->quantized_matrices[matrix_offset][row][column] =
                        row == column ? 1.0f : 0.0f;
            {
                const uint64_t quantization_end = cache_profile_now(cache);
                if (quantization_end >= quantization_start)
                    *quantization_ticks +=
                        quantization_end - quantization_start;
            }
        } else {
            const uint64_t quantization_start = cache_profile_now(cache);
            for (matrix_index = 0U;
                    matrix_index < input->segment3_matrix_count;
                    ++matrix_index)
                for (row = 0U; row < 4U; ++row)
                    for (column = 0U; column < 4U; ++column) {
                        const float value = input->segment3_matrices
                            [matrix_index][row][column];
                        float quantized;
                        if (!isfinite(value) || value < -32768.0f
                                || value >= 32768.0f)
                            return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
                        quantized = cache_quantize_matrix_element(value);
                        cache->quantized_matrices[matrix_offset + matrix_index]
                            [row][column] = quantized;
                        matrix_hash = cache_hash_float(matrix_hash, quantized);
                        cache->matrix_elements_quantized++;
                    }
            {
                const uint64_t quantization_end = cache_profile_now(cache);
                if (quantization_end >= quantization_start)
                    *quantization_ticks +=
                        quantization_end - quantization_start;
            }
        }
        cache->input_quantized_matrix_hashes[input_index] = matrix_hash;
    }
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        uint64_t input_hash = UINT64_C(1469598103934665603);
        size_t row;
        size_t column;
        input_hash = cache_hash_u64(input_hash, input->room_id);
        for (row = 0U; row < 3U; ++row)
            input_hash = cache_hash_float(
                input_hash, input->position[row]);
        for (row = 0U; row < 4U; ++row) {
            for (column = 0U; column < 4U; ++column) {
                input_hash = cache_hash_float(
                    input_hash, input->matrix[row][column]);
            }
        }
        input_hash = cache_hash_u64(
            input_hash,
            cache->input_quantized_matrix_hashes[input_index]);
        cache->input_publication_signatures[input_index] = input_hash;
        /* The retained per-input signature already covers room, placement,
         * outer matrix and the quantized joint bank. Fold it into the ordered
         * aggregate once instead of hashing every field a second time. */
        hash = cache_hash_u64(hash, input_hash);
    }
    *signature = hash;
    return GE_ORIGINAL_MODEL_SCENE_OK;
}

/* Keep the canonical scalar accumulation order while making each row source
 * explicit. This removes the inner-loop/object-array overhead and gives the
 * ARM compiler four independent VFP-friendly dot products without enabling
 * reassociation or fast-math. */
static void cache_transform_vertex(
    const float matrix[4][4], const GeGbiVertex *source,
    float transformed[4])
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
        transformed[axis] = value;
    }
}

typedef struct GeModelPublishedInput {
    size_t component, vertex_offset, batch_offset;
    uint64_t signature;
} GeModelPublishedInput;

/* Snapshot before a topology swap moves/frees its metadata. Only the output
 * that was valid on entry can supply reuse, never a retained variant's old
 * publication hashes. Allocation failure simply uses full publication. */
static size_t cache_snapshot_publication(GeOriginalModelSceneCache *cache,
    const GeDamRoomSceneStorage *storage)
{
    GeModelPublishedInput *snapshot = cache->publication_layout_scratch;
    if (!cache->publication_ready || storage == NULL
            || cache->published_vertices != storage->vertices
            || cache->published_batches != storage->batches) return 0U;
    if (cache->input_count > cache->publication_layout_scratch_capacity) {
        if (cache->input_count > SIZE_MAX / sizeof(*snapshot)) return 0U;
        snapshot = realloc(snapshot, cache->input_count * sizeof(*snapshot));
        if (snapshot == NULL) return 0U;
        cache->publication_layout_scratch = snapshot;
        cache->publication_layout_scratch_capacity = cache->input_count;
    }
    for (size_t i = 0U; i < cache->input_count; ++i)
        snapshot[i] = (GeModelPublishedInput){cache->input_component_indices[i],
            cache->input_vertex_offsets[i], cache->input_batch_offsets[i],
            cache->published_input_publication_signatures[i]};
    return cache->input_count;
}

static GeOriginalModelSceneStatus cache_build_impl(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    const GeDamRoomSceneStorage *storage, GeOriginalModelScene *scene,
    int exact_size)
{
    uint64_t signature;
    uint64_t publication_signature;
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t input_index;
    size_t previous_input_count = 0U;
    int reuse_publication_storage;
    uint64_t build_start;
    uint64_t phase_start;
    uint64_t matrix_quantization_ticks = 0U;
    GeOriginalModelSceneStatus status = GE_ORIGINAL_MODEL_SCENE_OK;

    if (cache != NULL) cache->publication_range_count = 0U;
    if (scene == NULL) return GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT;
    if (cache == NULL || (input_count != 0U && inputs == NULL))
        return scene->status;
    build_start = cache_profile_now(cache);
    if (cache->profile_clock != NULL) cache->profile_build_calls++;
    cache->build_attempts++;
    phase_start = cache_profile_now(cache);
    signature = cache_topology_signature(inputs, input_count);
    if (cache->topology_ready
            && (cache->input_count != input_count
                || cache->topology_signature != signature)) {
        previous_input_count = cache_snapshot_publication(cache, storage);
        if (!cache_select_retained_topology(cache, signature, input_count))
            cache_retain_current_topology(cache);
    }
    if (!cache->topology_ready || cache->input_count != input_count
            || cache->topology_signature != signature) {
        cache->topology_ready = UINT8_C(0);
        status = cache_rebuild_templates(
            cache, inputs, input_count, signature);
        if (status != GE_ORIGINAL_MODEL_SCENE_OK) goto done;
    }
    cache_profile_add(&cache->profile_topology_ticks, phase_start,
                      cache_profile_now(cache));
    scene->list_count = input_count;
    scene->required_vertex_count = cache->required_vertex_count;
    scene->required_batch_count = cache->required_batch_count;
    scene->triangle_count = cache->triangle_count;
    scene->commands_visited = cache->commands_visited;
    if (storage == NULL
            || (exact_size && (cache->required_vertex_count != storage->vertex_capacity
                || cache->required_batch_count != storage->batch_capacity))
            || cache->required_vertex_count > storage->vertex_capacity
            || cache->required_batch_count > storage->batch_capacity
            || (cache->required_vertex_count != 0U
                && storage->vertices == NULL)
            || (cache->required_batch_count != 0U
                && storage->batches == NULL)) {
        if (exact_size && storage != NULL
                && cache->publication_ready == 0U
                && cache->required_vertex_count <= storage->vertex_capacity
                && cache->required_batch_count <= storage->batch_capacity
                && (cache->required_vertex_count == 0U || storage->vertices != NULL)
                && (cache->required_batch_count == 0U || storage->batches != NULL)) {
            ++cache->discarded_publications_avoided;
            cache->discarded_vertices_avoided += cache->required_vertex_count;
        }
        status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
        goto done;
    }
    if (input_count > cache->publication_range_capacity) {
        GeOriginalModelScenePublicationRange *ranges;
        if (input_count > SIZE_MAX / sizeof(*ranges)) {
            status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
            goto done;
        }
        ranges = realloc(cache->publication_ranges,
                         input_count * sizeof(*ranges));
        if (ranges == NULL) {
            status = GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED;
            goto done;
        }
        cache->publication_ranges = ranges;
        cache->publication_range_capacity = input_count;
    }
    phase_start = cache_profile_now(cache);
    status = cache_prepare_publication_matrices(
        cache, inputs, input_count, signature, &publication_signature,
        &matrix_quantization_ticks);
    {
        const uint64_t phase_end = cache_profile_now(cache);
        const uint64_t prepare_ticks = phase_end >= phase_start
            ? phase_end - phase_start : 0U;
        cache->profile_publication_signature_ticks +=
            prepare_ticks >= matrix_quantization_ticks
                ? prepare_ticks - matrix_quantization_ticks : 0U;
        cache->profile_matrix_quantization_ticks +=
            matrix_quantization_ticks;
    }
    if (status != GE_ORIGINAL_MODEL_SCENE_OK) goto done;
    if (cache->publication_ready != 0U
            && cache->publication_signature == publication_signature
            && cache->published_vertices == storage->vertices
            && cache->published_batches == storage->batches) {
        scene->vertex_count = cache->required_vertex_count;
        scene->batch_count = cache->required_batch_count;
        cache->unchanged_builds++;
        status = GE_ORIGINAL_MODEL_SCENE_OK;
        goto done;
    }
    reuse_publication_storage = cache->publication_ready != 0U
        && cache->published_vertices == storage->vertices
        && cache->published_batches == storage->batches;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &inputs[input_index];
        const float (*input_matrices)[4][4] =
            cache->quantized_matrices
                + cache->input_quantized_matrix_offsets[input_index];
        const GeOriginalModelScene *query = &cache->queries[input_index];
        const GeOriginalModelSceneTopologyComponent *component =
            &((const GeOriginalModelSceneTopologyComponent *)cache->topology_components)
                [cache->input_component_indices[input_index]];
        size_t local_vertex;
        size_t local_batch;
        const GeModelPublishedInput *previous = input_index < previous_input_count
            ? &((const GeModelPublishedInput *)cache->publication_layout_scratch)[input_index]
            : NULL;
        const int reuse_component_storage = reuse_publication_storage
            || (previous != NULL
                && previous->component == cache->input_component_indices[input_index]
                && previous->vertex_offset == vertex_cursor
                && previous->batch_offset == batch_cursor);
        const uint64_t previous_signature = reuse_publication_storage
            ? cache->published_input_publication_signatures[input_index]
            : previous != NULL ? previous->signature : 0U;
        const int reuse_input_publication = reuse_component_storage
            && previous_signature == cache->input_publication_signatures[input_index];
        const int publish_segment_space =
            cache_matrix_is_identity(input->matrix)
            && input->position[0] == 0.0f
            && input->position[1] == 0.0f
            && input->position[2] == 0.0f;

        if (reuse_input_publication) {
            cache->published_input_publication_signatures[input_index] =
                cache->input_publication_signatures[input_index];
            if (!reuse_publication_storage) cache->cross_topology_inputs_reused++;
            cache->unchanged_input_publications++;
            cache->unchanged_input_vertices_avoided +=
                query->required_vertex_count;
            cache->unchanged_input_batches_avoided +=
                query->required_batch_count;
            vertex_cursor += query->required_vertex_count;
            batch_cursor += query->required_batch_count;
            continue;
        }

        if (publish_segment_space)
            cache->identity_outer_vertices_published +=
                query->required_vertex_count;
        phase_start = cache_profile_now(cache);
        /* On a topology/buffer change the immutable payload is contiguous.
         * Publish it once, then change only positions below. This retains
         * every byte (including padding) without one large struct copy per
         * flattened triangle vertex. */
        if (!reuse_component_storage) {
            if (query->required_vertex_count != 0U)
                memcpy(storage->vertices + vertex_cursor, component->vertices,
                    query->required_vertex_count * sizeof(*storage->vertices));
        } else {
            cache->static_vertex_copies_avoided += query->required_vertex_count;
            if (!reuse_publication_storage)
                cache->cross_topology_static_vertices_reused += query->required_vertex_count;
        }
        for (local_vertex = 0U;
                local_vertex < query->required_vertex_count; ++local_vertex) {
            const GeDamRoomWorldVertex *source =
                &component->vertices[local_vertex];
            GeDamRoomWorldVertex *destination =
                &storage->vertices[vertex_cursor + local_vertex];
            const uint16_t source_matrix =
                component->matrix_indices[local_vertex];
            const uint32_t transform_source =
                component->transform_sources[local_vertex];
            float transformed[4];
            size_t axis;
            size_t row;
            if ((size_t)source_matrix >=
                    (input->segment3_matrices != NULL
                        ? input->segment3_matrix_count : 1U)) {
                cache->topology_ready = UINT8_C(0);
                status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
                goto done;
            }
            if ((size_t)transform_source < local_vertex) {
                const GeDamRoomWorldVertex *prior = &storage->vertices[
                    vertex_cursor + (size_t)transform_source];
                memcpy(destination->processed.eye, prior->processed.eye,
                       sizeof(destination->processed.eye));
                memcpy(destination->world, prior->world,
                       sizeof(destination->world));
                cache->duplicate_vertex_transforms_avoided++;
            } else {
                cache_transform_vertex(
                    input_matrices[source_matrix], &source->source,
                    transformed);
                memcpy(destination->processed.eye, transformed,
                       sizeof(transformed));
                for (axis = 0U; axis < 3U; ++axis) {
                    if (publish_segment_space) {
                        destination->world[axis] = transformed[axis];
                    } else if (input->segment3_matrices != NULL) {
                        destination->world[axis] = input->position[axis];
                        for (row = 0U; row < 4U; ++row)
                            destination->world[axis] += transformed[row]
                                * input->matrix[row][axis];
                    } else {
                        destination->world[axis] = input->position[axis]
                            + (float)source->source.x
                                * input->matrix[0][axis]
                            + (float)source->source.y
                                * input->matrix[1][axis]
                            + (float)source->source.z
                                * input->matrix[2][axis];
                    }
                }
            }
        }
        cache_profile_add(&cache->profile_vertex_transform_ticks,
                          phase_start, cache_profile_now(cache));
        phase_start = cache_profile_now(cache);
        if (reuse_component_storage) {
            /* The retained topology already owns first_vertex and every
             * material field in this exact output buffer.  Avoid loading and
             * rewriting the full batch during animated frames; only the
             * original room publication can change. */
            for (local_batch = 0U;
                    local_batch < query->required_batch_count;
                    ++local_batch) {
                storage->batches[batch_cursor + local_batch].room_id =
                    input->room_id;
                cache->static_batch_copies_avoided++;
            }
        } else {
            if (query->required_batch_count != 0U)
                memcpy(storage->batches + batch_cursor, component->batches,
                    query->required_batch_count * sizeof(*storage->batches));
            for (local_batch = 0U;
                    local_batch < query->required_batch_count;
                    ++local_batch) {
                GeDamRoomDrawBatch *batch =
                    &storage->batches[batch_cursor + local_batch];
                batch->first_vertex += vertex_cursor;
                batch->room_id = input->room_id;
            }
        }
        cache_profile_add(&cache->profile_batch_publication_ticks,
                          phase_start, cache_profile_now(cache));
        cache->published_input_publication_signatures[input_index] =
            cache->input_publication_signatures[input_index];
        if (query->required_vertex_count != 0U
                || query->required_batch_count != 0U) {
            GeOriginalModelScenePublicationRange *range =
                cache->publication_range_count != 0U
                ? &cache->publication_ranges[
                    cache->publication_range_count - 1U] : NULL;
            const uint8_t static_changed =
                reuse_component_storage ? UINT8_C(0) : UINT8_C(1);
            if (range != NULL && range->static_data_changed == static_changed
                    && range->vertex_offset + range->vertex_count
                        == vertex_cursor
                    && range->batch_offset + range->batch_count
                        == batch_cursor) {
                range->vertex_count += query->required_vertex_count;
                range->batch_count += query->required_batch_count;
            } else {
                range = &cache->publication_ranges[
                    cache->publication_range_count++];
                *range = (GeOriginalModelScenePublicationRange){
                    vertex_cursor, query->required_vertex_count,
                    batch_cursor, query->required_batch_count,
                    static_changed,
                };
            }
        }
        vertex_cursor += query->required_vertex_count;
        batch_cursor += query->required_batch_count;
    }
    if (vertex_cursor != cache->required_vertex_count
            || batch_cursor != cache->required_batch_count) {
        cache->topology_ready = UINT8_C(0);
        status = GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT;
        goto done;
    }
    scene->vertex_count = vertex_cursor;
    scene->batch_count = batch_cursor;
    cache->publication_signature = publication_signature;
    cache->published_vertices = storage->vertices;
    cache->published_batches = storage->batches;
    cache->publication_ready = UINT8_C(1);
    cache->cached_builds++;
done:
    /* Never advertise partially transformed output to a GPU consumer. */
    if (status != GE_ORIGINAL_MODEL_SCENE_OK)
        cache->publication_range_count = 0U;
    cache_profile_add(&cache->profile_build_ticks, build_start,
                      cache_profile_now(cache));
    scene->status = status;
    return status;
}

GeOriginalModelSceneStatus ge_original_model_scene_cache_build(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    const GeDamRoomSceneStorage *storage, GeOriginalModelScene *scene)
{
    return cache_build_impl(cache, inputs, input_count, storage, scene, 0);
}

GeOriginalModelSceneStatus ge_original_model_scene_cache_build_exact(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    const GeDamRoomSceneStorage *storage, GeOriginalModelScene *scene)
{
    return cache_build_impl(cache, inputs, input_count, storage, scene, 1);
}

int ge_original_model_scene_visit_textures(
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    void *context, GeOriginalModelSceneTextureVisitor visitor)
{
    size_t batch_index;
    /* Texture IDs are 16-bit. An exact bitmap keeps first-appearance order
     * while avoiding a quadratic scan of all earlier actor draw batches.
     * Per-call storage also preserves visitor reentrancy and stage lifetime. */
    uint8_t visited[(UINT16_MAX + 1U) / 8U] = {0};

    if ((batches == NULL && batch_count != 0U) || visitor == NULL) return 0;
    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        uint16_t texture_id;
        uint8_t mask;
        if (batches[batch_index].texture_valid == 0U
                || batches[batch_index].material.texture_enabled == 0U
                || batches[batch_index].material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) continue;
        texture_id = batches[batch_index].texture.texture_id;
        mask = (uint8_t)(1U << (texture_id & 7U));
        if ((visited[texture_id >> 3U] & mask) != 0U) continue;
        visited[texture_id >> 3U] |= mask;
        if (!visitor(context, texture_id)) return 0;
    }
    return 1;
}

const char *ge_original_model_scene_status_name(
    GeOriginalModelSceneStatus status)
{
    switch (status) {
    case GE_ORIGINAL_MODEL_SCENE_OK: return "ok";
    case GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT: return "invalid layout";
    case GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR: return "pipeline error";
    case GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED: return "capacity exceeded";
    }
    return "unknown";
}
