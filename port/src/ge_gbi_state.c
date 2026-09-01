#include "ge_gbi_state.h"

#include <stddef.h>
#include <string.h>

static uint32_t ge_gbi_pack_color(const GeGbiCommand *command)
{
    return ((uint32_t)command->data.color.red << 24)
        | ((uint32_t)command->data.color.green << 16)
        | ((uint32_t)command->data.color.blue << 8)
        | (uint32_t)command->data.color.alpha;
}

static uint32_t ge_gbi_apply_mode_bits(uint32_t current,
                                       const GeGbiCommand *command)
{
    uint32_t mask;
    uint8_t length = command->data.other_mode.length;
    uint16_t shift = command->data.other_mode.shift;

    if (length >= 32U) {
        mask = UINT32_MAX;
    } else {
        mask = ((UINT32_C(1) << length) - UINT32_C(1)) << shift;
    }
    return (current & ~mask) | (command->data.other_mode.data & mask);
}

static GeGbiStateStatus ge_gbi_apply_triangles(
        const GeGbiRenderState *state,
        const GeGbiCommand *command,
        GeGbiStateAction *action)
{
    uint8_t triangle_index;

    for (triangle_index = 0U;
            triangle_index < command->data.geometry.count;
            triangle_index++) {
        uint8_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
            uint8_t slot = command->data.geometry
                .triangles[triangle_index].vertex[vertex_index];

            if (slot >= 16U
                    || (state->valid_vertices & (uint16_t)(UINT16_C(1) << slot)) == 0U) {
                return GE_GBI_STATE_MISSING_VERTEX;
            }
        }
    }

    action->kind = GE_GBI_STATE_ACTION_DRAW_TRIANGLES;
    action->data.draw.count = command->data.geometry.count;
    memcpy(action->data.draw.triangles, command->data.geometry.triangles,
           sizeof(action->data.draw.triangles));
    return GE_GBI_STATE_OK;
}

void ge_gbi_state_init(GeGbiRenderState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        ge_gbi_matrix_stack_init(&state->modelview_stack);
        ge_gbi_matrix_stack_init(&state->projection_stack);
    }
}

GeGbiStateStatus ge_gbi_state_apply_matrix(GeGbiRenderState *state,
                                           const GeGbiMatrix *matrix,
                                           uint8_t parameters)
{
    GeGbiMatrixStack *stack;
    GeGbiMatrix *top;

    if (state == NULL || matrix == NULL) {
        return GE_GBI_STATE_INVALID_ARGUMENT;
    }
    if ((parameters & (uint8_t)~GE_GBI_MTX_VALID_MASK) != 0U) {
        return GE_GBI_STATE_INVALID_MATRIX_PARAMETERS;
    }

    stack = (parameters & GE_GBI_MTX_PROJECTION) != 0U
        ? &state->projection_stack : &state->modelview_stack;
    if (stack->count == 0U || stack->count > GE_GBI_MATRIX_STACK_CAPACITY) {
        return GE_GBI_STATE_INVALID_ARGUMENT;
    }
    if ((parameters & GE_GBI_MTX_PUSH) != 0U) {
        if (stack->count >= GE_GBI_MATRIX_STACK_CAPACITY) {
            return GE_GBI_STATE_MATRIX_STACK_OVERFLOW;
        }
        stack->entries[stack->count] = stack->entries[stack->count - 1U];
        ++stack->count;
    }

    top = &stack->entries[stack->count - 1U];
    if ((parameters & GE_GBI_MTX_LOAD) != 0U) {
        *top = *matrix;
    } else {
        /* Fast3D uses row vectors, so an incoming model matrix precedes top. */
        ge_gbi_matrix_multiply(top, matrix, top);
    }
    return GE_GBI_STATE_OK;
}

GeGbiStateStatus ge_gbi_state_apply(GeGbiRenderState *state,
                                    const GeGbiCommand *command,
                                    GeGbiStateAction *action)
{
    if (state == NULL || command == NULL || action == NULL) {
        return GE_GBI_STATE_INVALID_ARGUMENT;
    }
    memset(action, 0, sizeof(*action));

    switch (command->kind) {
    case GE_GBI_COMMAND_NOOP:
    case GE_GBI_COMMAND_LOAD_TILE:
    case GE_GBI_COMMAND_LOAD_BLOCK:
    case GE_GBI_COMMAND_LOAD_TLUT:
    case GE_GBI_COMMAND_PIPE_SYNC:
    case GE_GBI_COMMAND_TILE_SYNC:
    case GE_GBI_COMMAND_LOAD_SYNC:
    case GE_GBI_COMMAND_FULL_SYNC:
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_MOVE_MEMORY:
        if (command->data.move_memory.kind == GE_GBI_MOVE_MEMORY_VIEWPORT) {
            action->kind = GE_GBI_STATE_ACTION_LOAD_VIEWPORT;
            action->data.viewport.address = command->data.move_memory.address;
            return GE_GBI_STATE_OK;
        }
        if (command->data.move_memory.kind == GE_GBI_MOVE_MEMORY_LIGHT) {
            if (command->data.move_memory.light_slot >= GE_GBI_LIGHT_COUNT) {
                return GE_GBI_STATE_INVALID_ARGUMENT;
            }
            action->kind = GE_GBI_STATE_ACTION_LOAD_LIGHT;
            action->data.light.address = command->data.move_memory.address;
            action->data.light.slot = command->data.move_memory.light_slot;
            return GE_GBI_STATE_OK;
        }
        if (command->data.move_memory.kind == GE_GBI_MOVE_MEMORY_LOOK_AT_X
                || command->data.move_memory.kind
                    == GE_GBI_MOVE_MEMORY_LOOK_AT_Y) {
            action->kind = GE_GBI_STATE_ACTION_LOAD_LOOK_AT;
            action->data.look_at.address = command->data.move_memory.address;
            action->data.look_at.axis = (uint8_t)(
                command->data.move_memory.kind
                    == GE_GBI_MOVE_MEMORY_LOOK_AT_X ? 0U : 1U);
            return GE_GBI_STATE_OK;
        }
        return GE_GBI_STATE_UNSUPPORTED;

    case GE_GBI_COMMAND_MOVE_WORD:
        switch (command->data.move_word.index) {
        case GE_GBI_MOVE_WORD_SEGMENT: {
            const uint16_t offset = command->data.move_word.offset;
            uint8_t segment;

            if ((offset & UINT16_C(3)) != 0U || offset / 4U >= 16U) {
                return GE_GBI_STATE_INVALID_MOVE_WORD;
            }
            segment = (uint8_t)(offset / 4U);
            state->segment_bases[segment] = command->data.move_word.data;
            state->valid_segment_bases |= (uint16_t)(UINT16_C(1) << segment);
            return GE_GBI_STATE_OK;
        }
        case GE_GBI_MOVE_WORD_NUM_LIGHTS: {
            const uint32_t encoded = command->data.move_word.data
                - UINT32_C(0x80000000);
            uint32_t count;

            if (command->data.move_word.offset != 0U
                    || command->data.move_word.data < UINT32_C(0x80000020)
                    || encoded % 32U != 0U) {
                return GE_GBI_STATE_INVALID_MOVE_WORD;
            }
            count = encoded / 32U - 1U;
            if (count > 7U) {
                return GE_GBI_STATE_INVALID_MOVE_WORD;
            }
            state->directional_light_count = (uint8_t)count;
            return GE_GBI_STATE_OK;
        }
        case GE_GBI_MOVE_WORD_FOG:
            if (command->data.move_word.offset != 0U) {
                return GE_GBI_STATE_INVALID_MOVE_WORD;
            }
            state->fog_multiplier = (int16_t)(command->data.move_word.data >> 16);
            state->fog_offset = (int16_t)command->data.move_word.data;
            return GE_GBI_STATE_OK;
        case GE_GBI_MOVE_WORD_PERSP_NORM:
            if (command->data.move_word.offset != 0U
                    || command->data.move_word.data > UINT16_MAX) {
                return GE_GBI_STATE_INVALID_MOVE_WORD;
            }
            state->perspective_normalization
                = (uint16_t)command->data.move_word.data;
            return GE_GBI_STATE_OK;
        default:
            return GE_GBI_STATE_UNSUPPORTED;
        }

    case GE_GBI_COMMAND_POP_MATRIX: {
        GeGbiMatrixStack *stack = command->data.pop_matrix.projection != 0U
            ? &state->projection_stack : &state->modelview_stack;

        if (command->data.pop_matrix.projection > 1U) {
            return GE_GBI_STATE_INVALID_MATRIX_PARAMETERS;
        }
        if (stack->count <= 1U) {
            return GE_GBI_STATE_MATRIX_STACK_UNDERFLOW;
        }
        --stack->count;
        return GE_GBI_STATE_OK;
    }

    case GE_GBI_COMMAND_MATRIX:
        if ((command->data.dma.parameters
                & (uint8_t)~GE_GBI_MTX_VALID_MASK) != 0U) {
            return GE_GBI_STATE_INVALID_MATRIX_PARAMETERS;
        }
        action->kind = GE_GBI_STATE_ACTION_LOAD_MATRIX;
        action->data.matrix.address = command->data.dma.address;
        action->data.matrix.parameters = command->data.dma.parameters;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_VERTEX: {
        uint16_t index;

        for (index = 0U; index < command->data.vertex.count; index++) {
            state->valid_vertices |= (uint16_t)(UINT16_C(1)
                << (command->data.vertex.first + index));
        }
        action->kind = GE_GBI_STATE_ACTION_LOAD_VERTICES;
        action->data.vertices.address = command->data.vertex.address;
        action->data.vertices.count = command->data.vertex.count;
        action->data.vertices.first = command->data.vertex.first;
        return GE_GBI_STATE_OK;
    }

    case GE_GBI_COMMAND_DISPLAY_LIST:
        action->kind = command->data.display_list.push == 0U
            ? GE_GBI_STATE_ACTION_CALL_DISPLAY_LIST
            : GE_GBI_STATE_ACTION_BRANCH_DISPLAY_LIST;
        action->data.display_list.address = command->data.display_list.address;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_TRIANGLE:
    case GE_GBI_COMMAND_TRIANGLE4:
        return ge_gbi_apply_triangles(state, command, action);

    case GE_GBI_COMMAND_CLEAR_GEOMETRY_MODE:
        state->geometry_mode &= ~command->data.geometry_mode.mask;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_GEOMETRY_MODE:
        state->geometry_mode |= command->data.geometry_mode.mask;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_END_DISPLAY_LIST:
        action->kind = GE_GBI_STATE_ACTION_END_DISPLAY_LIST;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_TEXTURE:
        state->texture.scale_s = command->data.texture.scale_s;
        state->texture.scale_t = command->data.texture.scale_t;
        state->texture.level = command->data.texture.level;
        state->texture.tile = command->data.texture.tile;
        state->texture.enabled = command->data.texture.enabled;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_SET_OTHER_MODE_LOW:
        if (command->data.other_mode.length == 0U
                || command->data.other_mode.shift >= 32U
                || (uint32_t)command->data.other_mode.shift
                    + command->data.other_mode.length > 32U) {
            return GE_GBI_STATE_INVALID_ARGUMENT;
        }
        state->other_mode_low = ge_gbi_apply_mode_bits(
            state->other_mode_low, command);
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_OTHER_MODE_HIGH:
        if (command->data.other_mode.length == 0U
                || command->data.other_mode.shift >= 32U
                || (uint32_t)command->data.other_mode.shift
                    + command->data.other_mode.length > 32U) {
            return GE_GBI_STATE_INVALID_ARGUMENT;
        }
        state->other_mode_high = ge_gbi_apply_mode_bits(
            state->other_mode_high, command);
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_RARE_SET_TEXTURE:
        state->rare_texture.texture_id = command->data.rare_texture.texture_id;
        state->rare_texture.detail_texture_id = command->data.rare_texture.detail_texture_id;
        state->rare_texture.min_level = command->data.rare_texture.min_level;
        state->rare_texture.type = command->data.rare_texture.type;
        state->rare_texture.tile = command->data.rare_texture.tile;
        state->rare_texture.clamp_mirror_s = command->data.rare_texture.clamp_mirror_s;
        state->rare_texture.clamp_mirror_t = command->data.rare_texture.clamp_mirror_t;
        state->rare_texture.shift_s = command->data.rare_texture.shift_s;
        state->rare_texture.shift_t = command->data.rare_texture.shift_t;
        state->rare_texture_valid = 1U;
        state->active_texture_binding = GE_GBI_TEXTURE_BINDING_RARE_ID;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_SET_TEXTURE_IMAGE:
        state->texture_image.address = command->data.image.address;
        state->texture_image.width = command->data.image.width;
        state->texture_image.format = command->data.image.format;
        state->texture_image.size = command->data.image.size;
        state->texture_image_valid = 1U;
        state->active_texture_binding = GE_GBI_TEXTURE_BINDING_IMAGE;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_COMBINE:
        state->combine_mux0 = command->data.combine.mux0;
        state->combine_mux1 = command->data.combine.mux1;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_ENV_COLOR:
        state->environment_color = ge_gbi_pack_color(command);
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_PRIM_COLOR:
        state->primitive_color = ge_gbi_pack_color(command);
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_BLEND_COLOR:
        state->blend_color = ge_gbi_pack_color(command);
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_FOG_COLOR:
        state->fog_color = ge_gbi_pack_color(command);
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_SET_FILL_COLOR:
        state->fill_color = command->raw_w1;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_FILL_RECTANGLE:
        action->kind = GE_GBI_STATE_ACTION_DRAW_FILL_RECTANGLE;
        action->data.fill_rectangle = command->data.screen_rect;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_TEXTURE_RECTANGLE:
        if (state->texture_rectangle_phase != 0U) {
            return GE_GBI_STATE_MALFORMED_SEQUENCE;
        }
        state->pending_texture_rectangle.screen =
            command->data.texture_rect.screen;
        state->pending_texture_rectangle.tile = command->data.texture_rect.tile;
        state->pending_texture_rectangle.flipped =
            command->data.texture_rect.flipped;
        state->texture_rectangle_phase = 1U;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_RDP_HALF_1:
        if (state->texture_rectangle_phase != 1U) {
            return GE_GBI_STATE_MALFORMED_SEQUENCE;
        }
        state->pending_texture_rectangle.s = command->data.rdp_half.high;
        state->pending_texture_rectangle.t = command->data.rdp_half.low;
        state->texture_rectangle_phase = 2U;
        return GE_GBI_STATE_OK;
    case GE_GBI_COMMAND_RDP_HALF_2:
        if (state->texture_rectangle_phase != 2U) {
            return GE_GBI_STATE_MALFORMED_SEQUENCE;
        }
        state->pending_texture_rectangle.dsdx = command->data.rdp_half.high;
        state->pending_texture_rectangle.dtdy = command->data.rdp_half.low;
        action->kind = GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE;
        action->data.texture_rectangle = state->pending_texture_rectangle;
        memset(&state->pending_texture_rectangle, 0,
               sizeof(state->pending_texture_rectangle));
        state->texture_rectangle_phase = 0U;
        return GE_GBI_STATE_OK;

    case GE_GBI_COMMAND_SET_TILE: {
        GeGbiTileState *tile = &state->tiles[command->data.tile.tile];

        tile->line = command->data.tile.line;
        tile->tmem = command->data.tile.tmem;
        tile->format = command->data.tile.format;
        tile->size = command->data.tile.size;
        tile->palette = command->data.tile.palette;
        tile->clamp_mirror_s = command->data.tile.clamp_mirror_s;
        tile->clamp_mirror_t = command->data.tile.clamp_mirror_t;
        tile->mask_s = command->data.tile.mask_s;
        tile->mask_t = command->data.tile.mask_t;
        tile->shift_s = command->data.tile.shift_s;
        tile->shift_t = command->data.tile.shift_t;
        return GE_GBI_STATE_OK;
    }
    case GE_GBI_COMMAND_SET_TILE_SIZE: {
        GeGbiTileState *tile = &state->tiles[command->data.tile_rect.tile];

        tile->upper_s = command->data.tile_rect.upper_s;
        tile->upper_t = command->data.tile_rect.upper_t;
        tile->lower_s = command->data.tile_rect.lower_s;
        tile->lower_t = command->data.tile_rect.lower_t;
        return GE_GBI_STATE_OK;
    }

    case GE_GBI_COMMAND_UNKNOWN:
    default:
        return GE_GBI_STATE_UNSUPPORTED;
    }
}
