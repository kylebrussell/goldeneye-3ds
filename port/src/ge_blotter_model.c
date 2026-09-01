#include "ge_blotter_model.h"

#include <string.h>

typedef struct GeBlotterBuildContext {
    GeBlotterModel *model;
    GeBlotterModelStatus callback_status;
    uint8_t has_material;
} GeBlotterBuildContext;

static int ge_blotter_material_equal(const GeGbiRareTextureState *left,
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

static GeGbiAddress ge_blotter_address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress address;

    address.raw = ((uint32_t)segment << 24) | offset;
    address.offset = offset;
    address.segment = segment;
    return address;
}

static int ge_blotter_collect(const GeGbiPipelineEvent *event,
                              void *user_data)
{
    GeBlotterBuildContext *context = user_data;
    const GeGbiRareTextureState *material;
    uint8_t triangle_index;

    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        return 1;
    }
    material = &event->state->rare_texture;
    if (context->has_material == 0U) {
        context->model->material = *material;
        context->has_material = 1U;
    } else if (!ge_blotter_material_equal(&context->model->material,
                                          material)) {
        context->callback_status = GE_BLOTTER_MODEL_UNEXPECTED_MATERIAL;
        return 0;
    }

    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            ++triangle_index) {
        const GeGbiTriangle *source_triangle =
            &event->action.data.draw.triangles[triangle_index];
        GeBlotterModelTriangle *destination_triangle;
        uint8_t vertex_index;

        if (context->model->triangle_count
                >= GE_BLOTTER_MODEL_TRIANGLE_COUNT) {
            context->callback_status = GE_BLOTTER_MODEL_UNEXPECTED_GEOMETRY;
            return 0;
        }
        destination_triangle = &context->model->triangles[
            context->model->triangle_count++];
        for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
            const uint8_t slot = source_triangle->vertex[vertex_index];
            GeBlotterModelVertex *destination =
                &destination_triangle->vertices[vertex_index];

            if (slot >= GE_GBI_VERTEX_CACHE_SIZE) {
                context->callback_status = GE_BLOTTER_MODEL_UNEXPECTED_GEOMETRY;
                return 0;
            }
            destination->source = event->vertex_cache[slot];
            destination->processed = event->processed_vertex_cache[slot];
            ++context->model->vertex_count;
        }
    }
    return 1;
}

GeBlotterModelStatus ge_blotter_model_build(const GeBlotterModelBlobs *blobs,
                                             GeBlotterModel *model)
{
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig config = {4U, 64U};
    GeBlotterBuildContext context;

    if (model == NULL) {
        return GE_BLOTTER_MODEL_INVALID_ARGUMENT;
    }
    memset(model, 0, sizeof(*model));
    model->status = GE_BLOTTER_MODEL_INVALID_ARGUMENT;
    if (blobs == NULL || blobs->display_list == NULL
            || blobs->vertices == NULL || blobs->matrix == NULL) {
        return model->status;
    }
    if (blobs->display_list_size != GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES
            || blobs->vertices_size != GE_BLOTTER_MODEL_VERTEX_BYTES
            || blobs->matrix_size != GE_BLOTTER_MODEL_MATRIX_BYTES) {
        model->status = GE_BLOTTER_MODEL_INVALID_BLOB_LAYOUT;
        return model->status;
    }

    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 3U, blobs->matrix,
                                      blobs->matrix_size)
                != GE_GBI_RESOLVE_OK
            || ge_gbi_memory_map_set_segment(&memory, 4U, blobs->vertices,
                                             blobs->vertices_size)
                != GE_GBI_RESOLVE_OK
            || ge_gbi_memory_map_set_segment(&memory, 5U,
                                             blobs->display_list,
                                             blobs->display_list_size)
                != GE_GBI_RESOLVE_OK) {
        model->status = GE_BLOTTER_MODEL_INVALID_BLOB_LAYOUT;
        return model->status;
    }

    context.model = model;
    context.callback_status = GE_BLOTTER_MODEL_OK;
    context.has_material = 0U;
    model->pipeline = ge_gbi_pipeline_execute(
        &memory, ge_blotter_address(5U, 0U), GE_GBI_BYTE_ORDER_BIG_ENDIAN,
        &config, ge_blotter_collect, &context);
    if (context.callback_status != GE_BLOTTER_MODEL_OK) {
        model->status = context.callback_status;
    } else if (model->pipeline.status != GE_GBI_PIPELINE_OK
            || model->pipeline.unsupported_commands != 0U) {
        model->status = GE_BLOTTER_MODEL_PIPELINE_ERROR;
    } else if (model->pipeline.triangles != GE_BLOTTER_MODEL_TRIANGLE_COUNT
            || model->triangle_count != GE_BLOTTER_MODEL_TRIANGLE_COUNT
            || model->vertex_count != GE_BLOTTER_MODEL_VERTEX_COUNT) {
        model->status = GE_BLOTTER_MODEL_UNEXPECTED_GEOMETRY;
    } else if (context.has_material == 0U
            || model->material.texture_id != GE_BLOTTER_MODEL_TEXTURE_ID) {
        model->status = GE_BLOTTER_MODEL_UNEXPECTED_MATERIAL;
    } else {
        model->status = GE_BLOTTER_MODEL_OK;
    }
    return model->status;
}

const char *ge_blotter_model_status_name(GeBlotterModelStatus status)
{
    switch (status) {
    case GE_BLOTTER_MODEL_OK:
        return "ok";
    case GE_BLOTTER_MODEL_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_BLOTTER_MODEL_INVALID_BLOB_LAYOUT:
        return "invalid blob layout";
    case GE_BLOTTER_MODEL_PIPELINE_ERROR:
        return "pipeline error";
    case GE_BLOTTER_MODEL_UNEXPECTED_GEOMETRY:
        return "unexpected geometry";
    case GE_BLOTTER_MODEL_UNEXPECTED_MATERIAL:
        return "unexpected material";
    default:
        return "unknown";
    }
}
