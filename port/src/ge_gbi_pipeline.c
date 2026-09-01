#include "ge_gbi_pipeline.h"

#include <stdlib.h>
#include <string.h>

typedef struct GeGbiPipelineContext {
    GeGbiRenderState state;
    GeGbiVertex vertex_cache[GE_GBI_VERTEX_CACHE_SIZE];
    GeGbiProcessedVertex processed_vertex_cache[GE_GBI_VERTEX_CACHE_SIZE];
    GeGbiTraversalRuntimeState traversal_runtime_state;
    GeGbiPipelineCallback callback;
    void *user_data;
    GeGbiStateStatus state_status;
    size_t actions_emitted;
    size_t draw_calls;
    size_t triangles;
    size_t unsupported_commands;
    size_t sequence_index;
    int callback_stopped;
} GeGbiPipelineContext;

static int ge_gbi_pipeline_visit(const GeGbiTraversalEvent *event,
                                 void *user_data)
{
    GeGbiPipelineContext *context = user_data;
    GeGbiStateAction action;
    GeGbiStateStatus status;

    status = ge_gbi_state_apply(&context->state, &event->command, &action);
    if (status == GE_GBI_STATE_UNSUPPORTED) {
        context->unsupported_commands++;
        return 1;
    }
    if (status != GE_GBI_STATE_OK) {
        context->state_status = status;
        return 0;
    }

    if (action.kind == GE_GBI_STATE_ACTION_LOAD_MATRIX) {
        if (event->has_matrix == 0U) {
            context->state_status = GE_GBI_STATE_INVALID_ARGUMENT;
            return 0;
        }
        status = ge_gbi_state_apply_matrix(&context->state, &event->matrix,
                                           action.data.matrix.parameters);
        if (status != GE_GBI_STATE_OK) {
            context->state_status = status;
            return 0;
        }
        action.data.matrix.has_value = 1U;
        action.data.matrix.value = event->matrix;
    }
    if (action.kind == GE_GBI_STATE_ACTION_LOAD_VIEWPORT) {
        if (event->has_viewport == 0U) {
            context->state_status = GE_GBI_STATE_MISSING_RSP_PAYLOAD;
            return 0;
        }
        context->state.viewport = event->viewport;
        context->state.viewport_valid = 1U;
        action.data.viewport.has_value = 1U;
        action.data.viewport.value = event->viewport;
    }
    if (action.kind == GE_GBI_STATE_ACTION_LOAD_LIGHT) {
        const uint8_t slot = action.data.light.slot;

        if (event->has_light == 0U || slot >= GE_GBI_LIGHT_COUNT) {
            context->state_status = GE_GBI_STATE_MISSING_RSP_PAYLOAD;
            return 0;
        }
        context->state.lights[slot] = event->light;
        context->state.valid_lights |= (uint8_t)(UINT8_C(1) << slot);
        action.data.light.has_value = 1U;
        action.data.light.value = event->light;
    }
    if (action.kind == GE_GBI_STATE_ACTION_LOAD_LOOK_AT) {
        const uint8_t axis = action.data.look_at.axis;

        if (event->has_light == 0U || axis > 1U) {
            context->state_status = GE_GBI_STATE_MISSING_RSP_PAYLOAD;
            return 0;
        }
        context->state.look_at[axis] = event->light;
        context->state.valid_look_at |= (uint8_t)(UINT8_C(1) << axis);
        action.data.look_at.has_value = 1U;
        action.data.look_at.value = event->light;
    }

    if (event->has_vertex_batch != 0U) {
        uint8_t index;

        for (index = 0U; index < event->vertex_count; index++) {
            const GeGbiVertex *vertex = &event->vertices[index];

            if (vertex->cache_slot >= GE_GBI_VERTEX_CACHE_SIZE) {
                context->state_status = GE_GBI_STATE_INVALID_ARGUMENT;
                return 0;
            }
            context->vertex_cache[vertex->cache_slot] = *vertex;
            if (ge_gbi_vertex_process(
                    &context->state, vertex,
                    &context->processed_vertex_cache[vertex->cache_slot])
                    != GE_GBI_VERTEX_PROCESS_OK) {
                context->state_status = GE_GBI_STATE_INVALID_ARGUMENT;
                return 0;
            }
        }
    }

    if (action.kind != GE_GBI_STATE_ACTION_NONE) {
        GeGbiPipelineEvent pipeline_event;

        pipeline_event.action = action;
        pipeline_event.state = &context->state;
        pipeline_event.vertex_cache = context->vertex_cache;
        pipeline_event.command_address = event->command_address;
        pipeline_event.call_depth = event->call_depth;
        pipeline_event.processed_vertex_cache
            = context->processed_vertex_cache;
        pipeline_event.sequence_index = context->sequence_index;
        context->actions_emitted++;
        if (action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
            context->draw_calls++;
            context->triangles += action.data.draw.count;
        } else if (action.kind == GE_GBI_STATE_ACTION_DRAW_FILL_RECTANGLE
                || action.kind
                    == GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE) {
            context->draw_calls++;
            context->triangles += 2U;
        }
        if (context->callback != NULL
                && context->callback(&pipeline_event,
                                     context->user_data) == 0) {
            context->callback_stopped = 1;
            return 0;
        }
    }
    return 1;
}

static int add_size(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) return 0;
    *result = left + right;
    return 1;
}

static int accumulate_traversal(GeGbiTraversalResult *aggregate,
                                const GeGbiTraversalResult *current)
{
    aggregate->status = current->status;
    aggregate->decode_status = current->decode_status;
    aggregate->stop_address = current->stop_address;
    if (!add_size(aggregate->commands_visited,
                  current->commands_visited,
                  &aggregate->commands_visited)
            || !add_size(aggregate->vertex_batches,
                         current->vertex_batches,
                         &aggregate->vertex_batches)
            || !add_size(aggregate->vertices_fetched,
                         current->vertices_fetched,
                         &aggregate->vertices_fetched)
            || !add_size(aggregate->matrices_fetched,
                         current->matrices_fetched,
                         &aggregate->matrices_fetched)
            || !add_size(aggregate->rsp_payloads_fetched,
                         current->rsp_payloads_fetched,
                         &aggregate->rsp_payloads_fetched))
        return 0;
    if (current->maximum_call_depth > aggregate->maximum_call_depth)
        aggregate->maximum_call_depth = current->maximum_call_depth;
    return 1;
}

static void ge_gbi_pipeline_execute_sequence_with_context(
    const GeGbiMemoryMap *memory,
    const GeGbiAddress *root_addresses,
    size_t root_count,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiPipelineCallback callback,
    void *user_data,
    GeGbiPipelineContext *context,
    GeGbiPipelineResult *result)
{
    memset(result, 0, sizeof(*result));
    memset(context, 0, sizeof(*context));
    result->status = GE_GBI_PIPELINE_INVALID_ARGUMENT;
    result->state_status = GE_GBI_STATE_OK;
    ge_gbi_state_init(&context->state);
    context->callback = callback;
    context->user_data = user_data;
    context->state_status = GE_GBI_STATE_OK;

    if (memory == NULL || root_addresses == NULL || root_count == 0U
            || config == NULL) {
        result->final_state = context->state;
        return;
    }

    {
        size_t root_index;
        for (root_index = 0U; root_index < root_count; ++root_index) {
            GeGbiTraversalResult traversal;
            context->sequence_index = root_index;
            traversal = ge_gbi_traverse_display_list_continue(
                memory, root_addresses[root_index], byte_order, config,
                ge_gbi_pipeline_visit, context,
                &context->traversal_runtime_state);
            if (!accumulate_traversal(&result->traversal, &traversal)) {
                result->traversal = traversal;
                result->traversal.status = GE_GBI_TRAVERSAL_INVALID_ARGUMENT;
                break;
            }
            if (context->state_status != GE_GBI_STATE_OK
                    || context->callback_stopped != 0
                    || traversal.status != GE_GBI_TRAVERSAL_OK)
                break;
        }
    }
    result->state_status = context->state_status;
    result->final_state = context->state;
    result->actions_emitted = context->actions_emitted;
    result->draw_calls = context->draw_calls;
    result->triangles = context->triangles;
    result->unsupported_commands = context->unsupported_commands;

    if (context->state_status != GE_GBI_STATE_OK) {
        result->status = GE_GBI_PIPELINE_STATE_ERROR;
    } else if (context->callback_stopped != 0) {
        result->status = GE_GBI_PIPELINE_STOPPED;
    } else if (result->traversal.status == GE_GBI_TRAVERSAL_OK) {
        result->status = GE_GBI_PIPELINE_OK;
    } else if (result->traversal.status == GE_GBI_TRAVERSAL_STOPPED) {
        result->status = GE_GBI_PIPELINE_STOPPED;
    } else {
        result->status = GE_GBI_PIPELINE_TRAVERSAL_ERROR;
    }
}

GeGbiPipelineResult ge_gbi_pipeline_execute(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiPipelineCallback callback,
    void *user_data)
{
    return ge_gbi_pipeline_execute_sequence(
        memory, &root_address, 1U, byte_order, config, callback, user_data);
}

GeGbiPipelineResult ge_gbi_pipeline_execute_sequence(
    const GeGbiMemoryMap *memory,
    const GeGbiAddress *root_addresses,
    size_t root_count,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiPipelineCallback callback,
    void *user_data)
{
    GeGbiPipelineResult result;
    GeGbiPipelineContext *context = malloc(sizeof(*context));

    if (context == NULL) {
        memset(&result, 0, sizeof(result));
        result.status = GE_GBI_PIPELINE_INVALID_ARGUMENT;
        result.state_status = GE_GBI_STATE_OK;
        ge_gbi_state_init(&result.final_state);
        return result;
    }
    ge_gbi_pipeline_execute_sequence_with_context(
        memory, root_addresses, root_count, byte_order, config,
        callback, user_data, context, &result);
    free(context);
    return result;
}
