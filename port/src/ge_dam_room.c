#include "ge_dam_room.h"

#include <math.h>
#include <string.h>

typedef struct GeDamRoomBuildContext {
    GeDamRoomModel *model;
    GeDamRoomStatus callback_status;
    uint8_t has_material;
} GeDamRoomBuildContext;

/* Room display lists are invoked by GoldenEye's 3D background task and do
 * not redundantly set G_ZBUFFER themselves.  The extracted per-room blobs
 * therefore inherit this geometry bit from the parent display list.  Keep
 * that parent state at this adapter boundary; the authored render-mode bits
 * in each room still decide whether a particular surface compares/updates Z. */
#define GE_DAM_ROOM_INHERITED_ZBUFFER UINT32_C(0x00000001)

static GeGbiAddress ge_dam_room_address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress address;

    address.raw = ((uint32_t)segment << 24U) | offset;
    address.offset = offset;
    address.segment = segment;
    return address;
}

static int ge_dam_room_texture_equal(const GeGbiRareTextureState *left,
                                     const GeGbiRareTextureState *right)
{
    return left->texture_id == right->texture_id
        && left->detail_texture_id == right->detail_texture_id
        && left->min_level == right->min_level
        && left->type == right->type
        && left->tile == right->tile
        && left->clamp_mirror_s == right->clamp_mirror_s
        && left->clamp_mirror_t == right->clamp_mirror_t
        && left->shift_s == right->shift_s
        && left->shift_t == right->shift_t;
}

static int ge_dam_room_collect(const GeGbiPipelineEvent *event,
                               void *user_data)
{
    GeDamRoomBuildContext *context = user_data;
    uint8_t triangle_index;

    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        return 1;
    }
    if (event->state->rare_texture_valid == 0U) {
        context->callback_status = GE_DAM_ROOM_UNEXPECTED_MATERIAL;
        return 0;
    }
    if (context->has_material == 0U) {
        GeGbiRenderState inherited_state = *event->state;

        inherited_state.geometry_mode |= GE_DAM_ROOM_INHERITED_ZBUFFER;
        context->model->texture = event->state->rare_texture;
        if (ge_pica_material_translate(&inherited_state,
                                       &context->model->material)
                != GE_PICA_MATERIAL_OK) {
            context->callback_status = GE_DAM_ROOM_UNEXPECTED_MATERIAL;
            return 0;
        }
        context->has_material = 1U;
    } else if (!ge_dam_room_texture_equal(&context->model->texture,
                                          &event->state->rare_texture)) {
        context->callback_status = GE_DAM_ROOM_UNEXPECTED_MATERIAL;
        return 0;
    }

    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            triangle_index++) {
        const GeGbiTriangle *triangle =
            &event->action.data.draw.triangles[triangle_index];
        uint8_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
            const uint8_t slot = triangle->vertex[vertex_index];
            GeDamRoomVertex *destination;

            if (slot >= GE_GBI_VERTEX_CACHE_SIZE
                    || context->model->vertex_count
                        >= GE_DAM_ROOM1_RENDER_VERTEX_COUNT) {
                context->callback_status = GE_DAM_ROOM_UNEXPECTED_GEOMETRY;
                return 0;
            }
            destination = &context->model->vertices[
                context->model->vertex_count++];
            destination->source = event->vertex_cache[slot];
            destination->processed = event->processed_vertex_cache[slot];
        }
    }
    return 1;
}

GeDamRoomStatus ge_dam_room1_build(const GeDamRoomBlobs *blobs,
                                   GeDamRoomModel *model)
{
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig config = {4U, 64U};
    GeDamRoomBuildContext context;

    if (model == NULL) {
        return GE_DAM_ROOM_INVALID_ARGUMENT;
    }
    memset(model, 0, sizeof(*model));
    model->status = GE_DAM_ROOM_INVALID_ARGUMENT;
    if (blobs == NULL || blobs->point_table == NULL
            || blobs->primary_gdl == NULL) {
        return model->status;
    }
    if (blobs->point_table_size != GE_DAM_ROOM1_POINT_TABLE_BYTES
            || blobs->primary_gdl_size != GE_DAM_ROOM1_PRIMARY_GDL_BYTES) {
        model->status = GE_DAM_ROOM_INVALID_BLOB_LAYOUT;
        return model->status;
    }

    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 14U, blobs->point_table,
                                      blobs->point_table_size)
                != GE_GBI_RESOLVE_OK
            || ge_gbi_memory_map_set_segment(&memory, 13U,
                                             blobs->primary_gdl,
                                             blobs->primary_gdl_size)
                != GE_GBI_RESOLVE_OK) {
        model->status = GE_DAM_ROOM_INVALID_BLOB_LAYOUT;
        return model->status;
    }

    context.model = model;
    context.callback_status = GE_DAM_ROOM_OK;
    context.has_material = 0U;
    model->pipeline = ge_gbi_pipeline_execute(
        &memory, ge_dam_room_address(13U, 0U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config,
        ge_dam_room_collect, &context);
    if (context.callback_status != GE_DAM_ROOM_OK) {
        model->status = context.callback_status;
    } else if (model->pipeline.status != GE_GBI_PIPELINE_OK
            || model->pipeline.unsupported_commands != 0U) {
        model->status = GE_DAM_ROOM_PIPELINE_ERROR;
    } else if (model->pipeline.traversal.commands_visited
                    != GE_DAM_ROOM1_COMMAND_COUNT
            || model->pipeline.traversal.vertex_batches != 3U
            || model->pipeline.traversal.vertices_fetched
                    != GE_DAM_ROOM1_SOURCE_VERTEX_COUNT
            || model->pipeline.draw_calls != GE_DAM_ROOM1_DRAW_COUNT
            || model->pipeline.triangles != GE_DAM_ROOM1_TRIANGLE_COUNT
            || model->vertex_count != GE_DAM_ROOM1_RENDER_VERTEX_COUNT) {
        model->status = GE_DAM_ROOM_UNEXPECTED_GEOMETRY;
    } else if (context.has_material == 0U
            || model->texture.texture_id != GE_DAM_ROOM1_TEXTURE_ID) {
        model->status = GE_DAM_ROOM_UNEXPECTED_MATERIAL;
    } else {
        model->status = GE_DAM_ROOM_OK;
    }
    return model->status;
}

typedef struct GeDamRoomSceneBuildContext {
    const GeDamRoomBlobDescriptor *room;
    GeDamRoomListKind list_kind;
    const GeDamRoomSceneStorage *storage;
    GeDamRoomStatus callback_status;
    size_t vertex_cursor;
    size_t batch_cursor;
    size_t triangle_count;
    uint8_t write_output;
} GeDamRoomSceneBuildContext;

static int ge_dam_room_size_add(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) {
        return 0;
    }
    *result = left + right;
    return 1;
}

static int ge_dam_room_scene_collect(const GeGbiPipelineEvent *event,
                                     void *user_data)
{
    GeDamRoomSceneBuildContext *context = user_data;
    GeGbiRenderState inherited_state;
    GePicaMaterial material;
    size_t draw_vertices;
    size_t vertex_end;
    size_t batch_end;
    size_t triangle_end;
    uint8_t triangle_index;

    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        return 1;
    }
    inherited_state = *event->state;
    inherited_state.geometry_mode |= GE_DAM_ROOM_INHERITED_ZBUFFER;
    if (ge_pica_material_translate(&inherited_state, &material)
            != GE_PICA_MATERIAL_OK) {
        context->callback_status = GE_DAM_ROOM_UNEXPECTED_MATERIAL;
        return 0;
    }
    draw_vertices = (size_t)event->action.data.draw.count * 3U;
    if (!ge_dam_room_size_add(context->vertex_cursor, draw_vertices,
                              &vertex_end)
            || !ge_dam_room_size_add(context->batch_cursor, 1U, &batch_end)
            || !ge_dam_room_size_add(context->triangle_count,
                    event->action.data.draw.count, &triangle_end)) {
        context->callback_status = GE_DAM_ROOM_UNEXPECTED_GEOMETRY;
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
                context->callback_status = GE_DAM_ROOM_UNEXPECTED_GEOMETRY;
                return 0;
            }
        }
    }

    if (context->write_output != 0U) {
        GeDamRoomDrawBatch *batch;
        size_t output_index = context->vertex_cursor;

        if (vertex_end > context->storage->vertex_capacity
                || batch_end > context->storage->batch_capacity) {
            context->callback_status = GE_DAM_ROOM_CAPACITY_EXCEEDED;
            return 0;
        }
        batch = &context->storage->batches[context->batch_cursor];
        memset(batch, 0, sizeof(*batch));
        batch->room_id = context->room->room_id;
        batch->list_kind = context->list_kind;
        batch->command_address = event->command_address;
        batch->texture = event->state->rare_texture;
        batch->material = material;
        batch->first_vertex = context->vertex_cursor;
        batch->vertex_count = draw_vertices;
        batch->triangle_count = event->action.data.draw.count;
        batch->texture_valid = event->state->rare_texture_valid != 0U
            ? UINT8_C(1) : UINT8_C(0);

        for (triangle_index = 0U;
                triangle_index < event->action.data.draw.count;
                ++triangle_index) {
            const GeGbiTriangle *triangle =
                &event->action.data.draw.triangles[triangle_index];
            uint8_t vertex_index;

            for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
                const uint8_t slot = triangle->vertex[vertex_index];
                GeDamRoomWorldVertex *destination =
                    &context->storage->vertices[output_index++];

                destination->source = event->vertex_cache[slot];
                destination->processed = event->processed_vertex_cache[slot];
                destination->world[0] = context->room->origin[0]
                    + (float)destination->source.x;
                destination->world[1] = context->room->origin[1]
                    + (float)destination->source.y;
                destination->world[2] = context->room->origin[2]
                    + (float)destination->source.z;
            }
        }
    }

    context->vertex_cursor = vertex_end;
    context->batch_cursor = batch_end;
    context->triangle_count = triangle_end;
    return 1;
}

static int ge_dam_room_descriptor_valid(
        const GeDamRoomBlobDescriptor *room)
{
    const int points_empty = room->point_table == NULL
        && room->point_table_size == 0U;
    const int points_present = room->point_table != NULL
        && room->point_table_size != 0U
        && room->point_table_size % 16U == 0U;
    const int secondary_empty = room->secondary_gdl == NULL
        && room->secondary_gdl_size == 0U;
    const int secondary_present = room->secondary_gdl != NULL
        && room->secondary_gdl_size != 0U
        && room->secondary_gdl_size % 8U == 0U;

    /* bgLoadRoomModelData explicitly accepts csize_point_index_binary == 0:
     * several authored outdoor/portal rooms have a no-geometry primary list
     * and no vertex table.  Preserve that exact paired-null representation;
     * a display list which nevertheless references segment 0x0e will still
     * fail in the normal GBI resolver below. */
    return (points_empty || points_present)
        && room->primary_gdl != NULL && room->primary_gdl_size != 0U
        && room->primary_gdl_size % 8U == 0U
        && (secondary_empty || secondary_present)
        && isfinite(room->origin[0]) && isfinite(room->origin[1])
        && isfinite(room->origin[2]);
}

static GeDamRoomStatus ge_dam_room_scene_execute_list(
        const GeDamRoomBlobDescriptor *room,
        GeDamRoomListKind list_kind,
        const uint8_t *display_list,
        size_t display_list_size,
        const GeDamRoomBuildLimits *limits,
        const GeDamRoomSceneStorage *storage,
        uint8_t write_output,
        size_t *vertex_cursor,
        size_t *batch_cursor,
        size_t *triangle_count,
        GeGbiPipelineResult *pipeline)
{
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig traversal;
    GeDamRoomSceneBuildContext context;

    ge_gbi_memory_map_init(&memory);
    if ((room->point_table != NULL && ge_gbi_memory_map_set_segment(
                &memory, 14U, room->point_table, room->point_table_size)
                != GE_GBI_RESOLVE_OK)
            || ge_gbi_memory_map_set_segment(&memory, 13U, display_list,
                                             display_list_size)
                != GE_GBI_RESOLVE_OK) {
        return GE_DAM_ROOM_INVALID_BLOB_LAYOUT;
    }

    traversal.max_call_depth = limits->max_call_depth;
    traversal.max_commands = limits->max_commands_per_list;
    context.room = room;
    context.list_kind = list_kind;
    context.storage = storage;
    context.callback_status = GE_DAM_ROOM_OK;
    context.vertex_cursor = *vertex_cursor;
    context.batch_cursor = *batch_cursor;
    context.triangle_count = *triangle_count;
    context.write_output = write_output;
    *pipeline = ge_gbi_pipeline_execute(
        &memory, ge_dam_room_address(13U, 0U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &traversal,
        ge_dam_room_scene_collect, &context);
    if (context.callback_status != GE_DAM_ROOM_OK) {
        return context.callback_status;
    }
    if (pipeline->status != GE_GBI_PIPELINE_OK
            || pipeline->unsupported_commands != 0U) {
        return GE_DAM_ROOM_PIPELINE_ERROR;
    }
    *vertex_cursor = context.vertex_cursor;
    *batch_cursor = context.batch_cursor;
    *triangle_count = context.triangle_count;
    return GE_DAM_ROOM_OK;
}

GeDamRoomStatus ge_dam_rooms_build(
    const GeDamRoomBlobDescriptor *rooms,
    size_t room_count,
    const GeDamRoomBuildLimits *limits,
    const GeDamRoomSceneStorage *storage,
    GeDamRoomScene *scene)
{
    const GeDamRoomBuildLimits default_limits = {
        GE_DAM_ROOM_DEFAULT_MAX_CALL_DEPTH,
        GE_DAM_ROOM_DEFAULT_MAX_COMMANDS_PER_LIST
    };
    const GeDamRoomSceneStorage empty_storage = {NULL, 0U, NULL, 0U};
    const GeDamRoomBuildLimits *actual_limits = limits != NULL
        ? limits : &default_limits;
    const GeDamRoomSceneStorage *actual_storage = storage != NULL
        ? storage : &empty_storage;
    size_t required_vertices = 0U;
    size_t required_batches = 0U;
    size_t triangle_count = 0U;
    size_t list_count = 0U;
    size_t commands_visited = 0U;
    size_t unsupported_commands = 0U;
    size_t room_index;

    if (scene == NULL) {
        return GE_DAM_ROOM_INVALID_ARGUMENT;
    }
    memset(scene, 0, sizeof(*scene));
    scene->status = GE_DAM_ROOM_INVALID_ARGUMENT;
    scene->room_count = room_count;
    if ((room_count != 0U && rooms == NULL)
            || actual_limits->max_call_depth == 0U
            || actual_limits->max_commands_per_list == 0U
            || (actual_storage->vertex_capacity != 0U
                && actual_storage->vertices == NULL)
            || (actual_storage->batch_capacity != 0U
                && actual_storage->batches == NULL)) {
        return scene->status;
    }

    for (room_index = 0U; room_index < room_count; ++room_index) {
        const GeDamRoomBlobDescriptor *room = &rooms[room_index];
        size_t list_index;

        if (!ge_dam_room_descriptor_valid(room)) {
            scene->status = GE_DAM_ROOM_INVALID_BLOB_LAYOUT;
            return scene->status;
        }
        for (list_index = 0U; list_index < 2U; ++list_index) {
            const GeDamRoomListKind kind = list_index == 0U
                ? GE_DAM_ROOM_LIST_PRIMARY : GE_DAM_ROOM_LIST_SECONDARY;
            const uint8_t *display_list = list_index == 0U
                ? room->primary_gdl : room->secondary_gdl;
            const size_t display_list_size = list_index == 0U
                ? room->primary_gdl_size : room->secondary_gdl_size;
            GeGbiPipelineResult pipeline;
            GeDamRoomStatus status;

            if (display_list == NULL) {
                continue;
            }
            status = ge_dam_room_scene_execute_list(
                room, kind, display_list, display_list_size, actual_limits,
                actual_storage, UINT8_C(0), &required_vertices,
                &required_batches, &triangle_count, &pipeline);
            if (status != GE_DAM_ROOM_OK) {
                scene->status = status;
                return scene->status;
            }
            if (!ge_dam_room_size_add(list_count, 1U, &list_count)
                    || !ge_dam_room_size_add(commands_visited,
                        pipeline.traversal.commands_visited,
                        &commands_visited)
                    || !ge_dam_room_size_add(unsupported_commands,
                        pipeline.unsupported_commands,
                        &unsupported_commands)) {
                scene->status = GE_DAM_ROOM_UNEXPECTED_GEOMETRY;
                return scene->status;
            }
        }
    }

    scene->list_count = list_count;
    scene->triangle_count = triangle_count;
    scene->required_vertex_count = required_vertices;
    scene->required_batch_count = required_batches;
    scene->commands_visited = commands_visited;
    scene->unsupported_commands = unsupported_commands;
    if (actual_storage->vertex_capacity < required_vertices
            || actual_storage->batch_capacity < required_batches) {
        scene->status = GE_DAM_ROOM_CAPACITY_EXCEEDED;
        return scene->status;
    }

    required_vertices = 0U;
    required_batches = 0U;
    triangle_count = 0U;
    for (room_index = 0U; room_index < room_count; ++room_index) {
        const GeDamRoomBlobDescriptor *room = &rooms[room_index];
        size_t list_index;

        for (list_index = 0U; list_index < 2U; ++list_index) {
            const GeDamRoomListKind kind = list_index == 0U
                ? GE_DAM_ROOM_LIST_PRIMARY : GE_DAM_ROOM_LIST_SECONDARY;
            const uint8_t *display_list = list_index == 0U
                ? room->primary_gdl : room->secondary_gdl;
            const size_t display_list_size = list_index == 0U
                ? room->primary_gdl_size : room->secondary_gdl_size;
            GeGbiPipelineResult pipeline;
            GeDamRoomStatus status;

            if (display_list == NULL) {
                continue;
            }
            status = ge_dam_room_scene_execute_list(
                room, kind, display_list, display_list_size, actual_limits,
                actual_storage, UINT8_C(1), &required_vertices,
                &required_batches, &triangle_count, &pipeline);
            if (status != GE_DAM_ROOM_OK) {
                scene->status = status;
                return scene->status;
            }
        }
    }
    scene->vertex_count = required_vertices;
    scene->batch_count = required_batches;
    scene->triangle_count = triangle_count;
    scene->status = GE_DAM_ROOM_OK;
    return scene->status;
}

const char *ge_dam_room_status_name(GeDamRoomStatus status)
{
    switch (status) {
    case GE_DAM_ROOM_OK:
        return "ok";
    case GE_DAM_ROOM_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_DAM_ROOM_INVALID_BLOB_LAYOUT:
        return "invalid blob layout";
    case GE_DAM_ROOM_PIPELINE_ERROR:
        return "pipeline error";
    case GE_DAM_ROOM_UNEXPECTED_GEOMETRY:
        return "unexpected geometry";
    case GE_DAM_ROOM_UNEXPECTED_MATERIAL:
        return "unexpected material";
    case GE_DAM_ROOM_CAPACITY_EXCEEDED:
        return "capacity exceeded";
    default:
        return "unknown";
    }
}
