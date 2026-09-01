#include "ge_gbi_traverse.h"

#include <stdlib.h>
#include <string.h>

typedef struct GeGbiReturnFrame {
    GeGbiAddress return_address;
    size_t control_path_mark;
} GeGbiReturnFrame;

typedef struct GeGbiTraversalWorkspace {
    GeGbiReturnFrame return_stack[GE_GBI_TRAVERSE_HARD_MAX_DEPTH];
    GeGbiAddress control_path[GE_GBI_TRAVERSE_HARD_MAX_CONTROL_PATH];
    GeGbiTraversalRuntimeState runtime_segments;
} GeGbiTraversalWorkspace;

static uint16_t ge_gbi_traverse_read_u16(const uint8_t *bytes,
                                         GeGbiByteOrder byte_order)
{
    if (byte_order == GE_GBI_BYTE_ORDER_BIG_ENDIAN) {
        return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    }
    return (uint16_t)(((uint16_t)bytes[1] << 8) | (uint16_t)bytes[0]);
}

static int16_t ge_gbi_traverse_read_s16(const uint8_t *bytes,
                                        GeGbiByteOrder byte_order)
{
    return (int16_t)ge_gbi_traverse_read_u16(bytes, byte_order);
}

static int ge_gbi_traverse_same_address(GeGbiAddress left,
                                        GeGbiAddress right)
{
    return left.segment == right.segment && left.offset == right.offset;
}

static int ge_gbi_traverse_path_contains(const GeGbiAddress *path,
                                         size_t path_count,
                                         GeGbiAddress target)
{
    size_t index;

    for (index = 0U; index < path_count; ++index) {
        if (ge_gbi_traverse_same_address(path[index], target)) {
            return 1;
        }
    }
    return 0;
}

static int ge_gbi_traverse_advance(GeGbiAddress *address)
{
    if (address->offset > UINT32_C(0x00fffff7)) {
        return 0;
    }
    address->offset += 8U;
    address->raw = ((uint32_t)address->segment << 24) | address->offset;
    return 1;
}

static GeGbiTraversalStatus ge_gbi_traverse_map_resolve_status(
    GeGbiResolveStatus status)
{
    if (status == GE_GBI_RESOLVE_UNMAPPED_SEGMENT) {
        return GE_GBI_TRAVERSAL_UNMAPPED_SEGMENT;
    }
    if (status == GE_GBI_RESOLVE_OUT_OF_BOUNDS) {
        return GE_GBI_TRAVERSAL_OUT_OF_BOUNDS;
    }
    return GE_GBI_TRAVERSAL_INVALID_ARGUMENT;
}

static GeGbiTraversalStatus ge_gbi_traverse_fetch_vertices(
    const GeGbiMemoryMap *memory,
    const GeGbiTraversalRuntimeState *runtime_segments,
    GeGbiByteOrder byte_order,
    const GeGbiCommand *command,
    GeGbiTraversalEvent *event)
{
    const uint8_t *bytes;
    size_t byte_count;
    size_t index;
    GeGbiResolveStatus resolve_status;

    if (command->data.vertex.address.segment >= GE_GBI_SEGMENT_COUNT) {
        return GE_GBI_TRAVERSAL_INVALID_ARGUMENT;
    }
    if (command->data.vertex.count > GE_GBI_VERTEX_CACHE_SIZE) {
        return GE_GBI_TRAVERSAL_DECODE_ERROR;
    }
    byte_count = (size_t)command->data.vertex.count * 16U;
    if ((runtime_segments->valid_segment_mask
            & (uint16_t)(UINT16_C(1) << command->data.vertex.address.segment))
            != 0U) {
        const uint64_t physical = (uint64_t)runtime_segments->segment_bases[
            command->data.vertex.address.segment]
            + command->data.vertex.address.offset;

        if (physical > UINT32_MAX) {
            return GE_GBI_TRAVERSAL_OUT_OF_BOUNDS;
        }
        resolve_status = ge_gbi_memory_resolve_physical(
            memory, (uint32_t)physical, byte_count, &bytes);
    } else {
        resolve_status = ge_gbi_memory_resolve(memory,
                                               command->data.vertex.address,
                                               byte_count, &bytes);
    }
    if (resolve_status != GE_GBI_RESOLVE_OK) {
        return ge_gbi_traverse_map_resolve_status(resolve_status);
    }

    event->has_vertex_batch = 1U;
    event->vertex_count = (uint8_t)command->data.vertex.count;
    for (index = 0U; index < (size_t)event->vertex_count; ++index) {
        const uint8_t *source = bytes + index * 16U;
        GeGbiVertex *vertex = &event->vertices[index];

        vertex->x = ge_gbi_traverse_read_s16(source, byte_order);
        vertex->y = ge_gbi_traverse_read_s16(source + 2, byte_order);
        vertex->z = ge_gbi_traverse_read_s16(source + 4, byte_order);
        vertex->flag = ge_gbi_traverse_read_u16(source + 6, byte_order);
        vertex->texture_s = ge_gbi_traverse_read_s16(source + 8, byte_order);
        vertex->texture_t = ge_gbi_traverse_read_s16(source + 10, byte_order);
        vertex->red = source[12];
        vertex->green = source[13];
        vertex->blue = source[14];
        vertex->alpha = source[15];
        vertex->cache_slot = (uint8_t)((size_t)command->data.vertex.first + index);
    }

    return GE_GBI_TRAVERSAL_OK;
}

void ge_gbi_memory_map_init(GeGbiMemoryMap *memory)
{
    if (memory != NULL) {
        memset(memory, 0, sizeof(*memory));
    }
}

GeGbiResolveStatus ge_gbi_memory_map_set_segment(GeGbiMemoryMap *memory,
                                                  uint8_t segment,
                                                  const uint8_t *bytes,
                                                  size_t byte_count)
{
    if (memory == NULL || segment >= GE_GBI_SEGMENT_COUNT
            || (bytes == NULL && byte_count != 0U)) {
        return GE_GBI_RESOLVE_INVALID_ARGUMENT;
    }

    memory->segments[segment].bytes = bytes;
    memory->segments[segment].byte_count = byte_count;
    return GE_GBI_RESOLVE_OK;
}

GeGbiResolveStatus ge_gbi_memory_resolve(const GeGbiMemoryMap *memory,
                                         GeGbiAddress address,
                                         size_t required_bytes,
                                         const uint8_t **resolved_bytes)
{
    const GeGbiMemorySegment *segment;

    if (memory == NULL || resolved_bytes == NULL
            || address.segment >= GE_GBI_SEGMENT_COUNT) {
        return GE_GBI_RESOLVE_INVALID_ARGUMENT;
    }

    segment = &memory->segments[address.segment];
    if (segment->bytes == NULL) {
        return GE_GBI_RESOLVE_UNMAPPED_SEGMENT;
    }
    if ((size_t)address.offset > segment->byte_count
            || required_bytes > segment->byte_count - (size_t)address.offset) {
        return GE_GBI_RESOLVE_OUT_OF_BOUNDS;
    }

    *resolved_bytes = segment->bytes + address.offset;
    return GE_GBI_RESOLVE_OK;
}

GeGbiResolveStatus ge_gbi_memory_map_set_physical_region(
    GeGbiMemoryMap *memory,
    size_t region_index,
    uint32_t base_address,
    const uint8_t *bytes,
    size_t byte_count)
{
    if (memory == NULL || region_index >= GE_GBI_PHYSICAL_REGION_COUNT
            || (bytes == NULL && byte_count != 0U)
            || byte_count > (size_t)UINT32_MAX
            || (uint64_t)base_address + byte_count > UINT64_C(0x100000000)) {
        return GE_GBI_RESOLVE_INVALID_ARGUMENT;
    }

    memory->physical_regions[region_index].base_address = base_address;
    memory->physical_regions[region_index].bytes = bytes;
    memory->physical_regions[region_index].byte_count = byte_count;
    return GE_GBI_RESOLVE_OK;
}

GeGbiResolveStatus ge_gbi_memory_resolve_physical(
    const GeGbiMemoryMap *memory,
    uint32_t physical_address,
    size_t required_bytes,
    const uint8_t **resolved_bytes)
{
    size_t index;

    if (memory == NULL || resolved_bytes == NULL) {
        return GE_GBI_RESOLVE_INVALID_ARGUMENT;
    }
    for (index = 0U; index < GE_GBI_PHYSICAL_REGION_COUNT; ++index) {
        const GeGbiPhysicalRegion *region = &memory->physical_regions[index];
        const uint64_t region_end = (uint64_t)region->base_address
            + region->byte_count;
        const uint64_t request_end = (uint64_t)physical_address
            + required_bytes;

        if (region->bytes != NULL
                && physical_address >= region->base_address
                && request_end <= region_end) {
            *resolved_bytes = region->bytes
                + (size_t)(physical_address - region->base_address);
            return GE_GBI_RESOLVE_OK;
        }
    }
    return GE_GBI_RESOLVE_OUT_OF_BOUNDS;
}

static GeGbiResolveStatus ge_gbi_traverse_resolve_runtime(
    const GeGbiMemoryMap *memory,
    const GeGbiTraversalRuntimeState *runtime_segments,
    GeGbiAddress address,
    size_t required_bytes,
    const uint8_t **resolved_bytes)
{
    if (address.segment >= GE_GBI_SEGMENT_COUNT) {
        return GE_GBI_RESOLVE_INVALID_ARGUMENT;
    }
    if ((runtime_segments->valid_segment_mask
        & (uint16_t)(UINT16_C(1) << address.segment)) != 0U) {
        const uint64_t physical = (uint64_t)runtime_segments->segment_bases[
            address.segment] + address.offset;

        if (physical > UINT32_MAX) {
            return GE_GBI_RESOLVE_OUT_OF_BOUNDS;
        }
        return ge_gbi_memory_resolve_physical(memory, (uint32_t)physical,
                                              required_bytes, resolved_bytes);
    }
    return ge_gbi_memory_resolve(memory, address, required_bytes,
                                 resolved_bytes);
}

static GeGbiTraversalResult ge_gbi_traverse_display_list_with_workspace(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiTraversalCallback callback,
    void *user_data,
    GeGbiTraversalWorkspace *workspace)
{
    GeGbiTraversalResult result;
    GeGbiReturnFrame *return_stack = workspace->return_stack;
    GeGbiAddress *control_path = workspace->control_path;
    GeGbiAddress current = root_address;
    GeGbiTraversalRuntimeState *runtime_segments =
        &workspace->runtime_segments;
    size_t stack_count = 0U;
    size_t path_count = 1U;

    memset(&result, 0, sizeof(result));
    memset(return_stack, 0, sizeof(workspace->return_stack));
    memset(control_path, 0, sizeof(workspace->control_path));
    result.status = GE_GBI_TRAVERSAL_INVALID_ARGUMENT;
    result.decode_status = GE_GBI_STATUS_OK;
    result.stop_address = root_address;

    if (memory == NULL || config == NULL || callback == NULL
            || root_address.segment >= GE_GBI_SEGMENT_COUNT
            || (byte_order != GE_GBI_BYTE_ORDER_BIG_ENDIAN
                && byte_order != GE_GBI_BYTE_ORDER_LITTLE_ENDIAN)
            || config->max_call_depth > GE_GBI_TRAVERSE_HARD_MAX_DEPTH
            || config->max_commands == 0U) {
        return result;
    }

    control_path[0] = root_address;
    for (;;) {
        const uint8_t *command_bytes;
        GeGbiResolveStatus resolve_status;
        GeGbiTraversalEvent event;
        GeGbiStatus decode_status;

        result.stop_address = current;
        if (result.commands_visited >= config->max_commands) {
            result.status = GE_GBI_TRAVERSAL_COMMAND_LIMIT;
            return result;
        }

        resolve_status = ge_gbi_traverse_resolve_runtime(
            memory, runtime_segments, current, 8U, &command_bytes);
        if (resolve_status != GE_GBI_RESOLVE_OK) {
            result.status = ge_gbi_traverse_map_resolve_status(resolve_status);
            return result;
        }

        memset(&event, 0, sizeof(event));
        event.command_address = current;
        event.call_depth = stack_count;
        decode_status = ge_gbi_decode_command(command_bytes, 8U, byte_order,
                                              current.offset, &event.command);
        if (decode_status != GE_GBI_STATUS_OK) {
            result.status = GE_GBI_TRAVERSAL_DECODE_ERROR;
            result.decode_status = decode_status;
            return result;
        }

        if (event.command.kind == GE_GBI_COMMAND_VERTEX) {
            result.status = ge_gbi_traverse_fetch_vertices(
                memory, runtime_segments, byte_order, &event.command, &event);
            if (result.status != GE_GBI_TRAVERSAL_OK) {
                return result;
            }
            ++result.vertex_batches;
            result.vertices_fetched += event.vertex_count;
        }
        if (event.command.kind == GE_GBI_COMMAND_MATRIX) {
            const uint8_t *matrix_bytes;
            GeGbiMatrixStatus matrix_status;

            resolve_status = ge_gbi_traverse_resolve_runtime(
                memory, runtime_segments, event.command.data.dma.address,
                GE_GBI_MATRIX_BYTE_COUNT, &matrix_bytes);
            if (resolve_status != GE_GBI_RESOLVE_OK) {
                result.status = ge_gbi_traverse_map_resolve_status(resolve_status);
                return result;
            }
            matrix_status = ge_gbi_matrix_decode_fixed(
                matrix_bytes, GE_GBI_MATRIX_BYTE_COUNT, byte_order,
                &event.matrix);
            if (matrix_status != GE_GBI_MATRIX_OK) {
                result.status = GE_GBI_TRAVERSAL_DECODE_ERROR;
                return result;
            }
            event.has_matrix = 1U;
            ++result.matrices_fetched;
        }
        if (event.command.kind == GE_GBI_COMMAND_MOVE_MEMORY
                && (event.command.data.move_memory.kind
                        == GE_GBI_MOVE_MEMORY_VIEWPORT
                    || event.command.data.move_memory.kind
                        == GE_GBI_MOVE_MEMORY_LOOK_AT_X
                    || event.command.data.move_memory.kind
                        == GE_GBI_MOVE_MEMORY_LOOK_AT_Y
                    || event.command.data.move_memory.kind
                        == GE_GBI_MOVE_MEMORY_LIGHT)) {
            const uint8_t *payload_bytes;
            GeGbiRspPayloadStatus payload_status;

            resolve_status = ge_gbi_traverse_resolve_runtime(
                memory, runtime_segments,
                event.command.data.move_memory.address,
                event.command.data.move_memory.length, &payload_bytes);
            if (resolve_status != GE_GBI_RESOLVE_OK) {
                result.status = ge_gbi_traverse_map_resolve_status(resolve_status);
                return result;
            }
            if (event.command.data.move_memory.kind
                    == GE_GBI_MOVE_MEMORY_VIEWPORT) {
                payload_status = ge_gbi_viewport_decode(
                    payload_bytes, event.command.data.move_memory.length,
                    byte_order, &event.viewport);
                event.has_viewport = 1U;
            } else {
                payload_status = ge_gbi_light_decode(
                    payload_bytes, event.command.data.move_memory.length,
                    &event.light);
                event.has_light = 1U;
            }
            if (payload_status != GE_GBI_RSP_PAYLOAD_OK) {
                result.status = GE_GBI_TRAVERSAL_DECODE_ERROR;
                return result;
            }
            ++result.rsp_payloads_fetched;
        }

        ++result.commands_visited;
        if (callback(&event, user_data) == 0) {
            result.status = GE_GBI_TRAVERSAL_STOPPED;
            return result;
        }

        if (event.command.kind == GE_GBI_COMMAND_MOVE_WORD
                && event.command.data.move_word.index
                    == GE_GBI_MOVE_WORD_SEGMENT) {
            const uint16_t offset = event.command.data.move_word.offset;
            uint8_t segment;

            if ((offset & UINT16_C(3)) != 0U || offset / 4U >= 16U) {
                result.status = GE_GBI_TRAVERSAL_DECODE_ERROR;
                result.decode_status = GE_GBI_STATUS_MALFORMED;
                return result;
            }
            segment = (uint8_t)(offset / 4U);
            runtime_segments->segment_bases[segment] =
                event.command.data.move_word.data;
            runtime_segments->valid_segment_mask |=
                (uint16_t)(UINT16_C(1) << segment);
        }

        if (event.command.kind == GE_GBI_COMMAND_DISPLAY_LIST) {
            GeGbiAddress target = event.command.data.display_list.address;
            GeGbiAddress return_address = current;

            if (ge_gbi_traverse_path_contains(control_path, path_count,
                                               target)) {
                result.status = GE_GBI_TRAVERSAL_CYCLE;
                result.stop_address = target;
                return result;
            }
            if (path_count >= GE_GBI_TRAVERSE_HARD_MAX_CONTROL_PATH) {
                result.status = GE_GBI_TRAVERSAL_CONTROL_PATH_LIMIT;
                return result;
            }

            if (event.command.data.display_list.push == 0U) {
                if (stack_count >= config->max_call_depth) {
                    result.status = GE_GBI_TRAVERSAL_CALL_DEPTH_LIMIT;
                    return result;
                }
                if (!ge_gbi_traverse_advance(&return_address)) {
                    result.status = GE_GBI_TRAVERSAL_OUT_OF_BOUNDS;
                    return result;
                }
                return_stack[stack_count].return_address = return_address;
                return_stack[stack_count].control_path_mark = path_count;
                ++stack_count;
                if (stack_count > result.maximum_call_depth) {
                    result.maximum_call_depth = stack_count;
                }
            }

            control_path[path_count] = target;
            ++path_count;
            current = target;
            continue;
        }

        if (event.command.kind == GE_GBI_COMMAND_END_DISPLAY_LIST) {
            if (stack_count == 0U) {
                result.status = GE_GBI_TRAVERSAL_OK;
                return result;
            }

            --stack_count;
            current = return_stack[stack_count].return_address;
            path_count = return_stack[stack_count].control_path_mark;
            continue;
        }

        if (!ge_gbi_traverse_advance(&current)) {
            result.status = GE_GBI_TRAVERSAL_OUT_OF_BOUNDS;
            return result;
        }
    }
}

GeGbiTraversalResult ge_gbi_traverse_display_list(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiTraversalCallback callback,
    void *user_data)
{
    GeGbiTraversalRuntimeState runtime_state;
    memset(&runtime_state, 0, sizeof(runtime_state));
    return ge_gbi_traverse_display_list_continue(
        memory, root_address, byte_order, config, callback, user_data,
        &runtime_state);
}

GeGbiTraversalResult ge_gbi_traverse_display_list_continue(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiTraversalCallback callback,
    void *user_data,
    GeGbiTraversalRuntimeState *runtime_state)
{
    GeGbiTraversalResult result;
    GeGbiTraversalWorkspace *workspace = malloc(sizeof(*workspace));

    if (workspace == NULL) {
        memset(&result, 0, sizeof(result));
        result.status = GE_GBI_TRAVERSAL_INVALID_ARGUMENT;
        result.decode_status = GE_GBI_STATUS_OK;
        result.stop_address = root_address;
        return result;
    }
    memset(workspace, 0, sizeof(*workspace));
    if (runtime_state != NULL)
        workspace->runtime_segments = *runtime_state;
    result = ge_gbi_traverse_display_list_with_workspace(
        memory, root_address, byte_order, config, callback, user_data,
        workspace);
    if (runtime_state != NULL)
        *runtime_state = workspace->runtime_segments;
    free(workspace);
    return result;
}

const char *ge_gbi_traversal_status_name(GeGbiTraversalStatus status)
{
    static const char *const names[] = {
        "ok", "stopped", "invalid-argument", "unmapped-segment",
        "out-of-bounds", "decode-error", "call-depth-limit",
        "command-limit", "control-path-limit", "cycle"
    };

    if ((unsigned int)status >= sizeof(names) / sizeof(names[0])) {
        return "invalid";
    }
    return names[status];
}
