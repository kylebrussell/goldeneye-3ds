#include "ge_gbi_decoder.h"

#include <string.h>

enum {
    GE_GBI_OPCODE_NOOP = 0x00,
    GE_GBI_OPCODE_MATRIX = 0x01,
    GE_GBI_OPCODE_MOVE_MEMORY = 0x03,
    GE_GBI_OPCODE_VERTEX = 0x04,
    GE_GBI_OPCODE_DISPLAY_LIST = 0x06,
    GE_GBI_OPCODE_TRIANGLE4 = 0xb1,
    GE_GBI_OPCODE_CLEAR_GEOMETRY = 0xb6,
    GE_GBI_OPCODE_SET_GEOMETRY = 0xb7,
    GE_GBI_OPCODE_END_DISPLAY_LIST = 0xb8,
    GE_GBI_OPCODE_SET_OTHER_MODE_LOW = 0xb9,
    GE_GBI_OPCODE_SET_OTHER_MODE_HIGH = 0xba,
    GE_GBI_OPCODE_TEXTURE = 0xbb,
    GE_GBI_OPCODE_MOVE_WORD = 0xbc,
    GE_GBI_OPCODE_POP_MATRIX = 0xbd,
    GE_GBI_OPCODE_TRIANGLE = 0xbf,
    GE_GBI_OPCODE_RARE_SET_TEXTURE = 0xc0,
    GE_GBI_OPCODE_RDP_HALF_2 = 0xb3,
    GE_GBI_OPCODE_RDP_HALF_1 = 0xb4,
    GE_GBI_OPCODE_TEXTURE_RECTANGLE = 0xe4,
    GE_GBI_OPCODE_TEXTURE_RECTANGLE_FLIP = 0xe5,
    GE_GBI_OPCODE_LOAD_TLUT = 0xf0,
    GE_GBI_OPCODE_SET_TILE_SIZE = 0xf2,
    GE_GBI_OPCODE_LOAD_BLOCK = 0xf3,
    GE_GBI_OPCODE_LOAD_TILE = 0xf4,
    GE_GBI_OPCODE_SET_TILE = 0xf5,
    GE_GBI_OPCODE_FILL_RECTANGLE = 0xf6,
    GE_GBI_OPCODE_SET_FILL_COLOR = 0xf7,
    GE_GBI_OPCODE_SET_FOG_COLOR = 0xf8,
    GE_GBI_OPCODE_SET_BLEND_COLOR = 0xf9,
    GE_GBI_OPCODE_SET_PRIM_COLOR = 0xfa,
    GE_GBI_OPCODE_SET_ENV_COLOR = 0xfb,
    GE_GBI_OPCODE_SET_COMBINE = 0xfc,
    GE_GBI_OPCODE_SET_TEXTURE_IMAGE = 0xfd,
    GE_GBI_OPCODE_LOAD_SYNC = 0xe6,
    GE_GBI_OPCODE_PIPE_SYNC = 0xe7,
    GE_GBI_OPCODE_TILE_SYNC = 0xe8,
    GE_GBI_OPCODE_FULL_SYNC = 0xe9
};

static uint32_t ge_gbi_read_word(const uint8_t *bytes, GeGbiByteOrder byte_order)
{
    if (byte_order == GE_GBI_BYTE_ORDER_BIG_ENDIAN) {
        return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
            | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
    }

    return ((uint32_t)bytes[3] << 24) | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[1] << 8) | (uint32_t)bytes[0];
}

static GeGbiAddress ge_gbi_address(uint32_t raw)
{
    GeGbiAddress address;

    address.raw = raw;
    address.segment = (uint8_t)(raw >> 24);
    address.offset = raw & UINT32_C(0x00ffffff);
    return address;
}

static void ge_gbi_decode_tri1(uint32_t word, GeGbiTriangle *triangle)
{
    triangle->vertex[0] = (uint8_t)(((word >> 16) & UINT32_C(0xff)) / 10U);
    triangle->vertex[1] = (uint8_t)(((word >> 8) & UINT32_C(0xff)) / 10U);
    triangle->vertex[2] = (uint8_t)((word & UINT32_C(0xff)) / 10U);
}

static uint8_t ge_gbi_decode_tri4(uint32_t w0, uint32_t w1,
                                  GeGbiTriangle triangles[4])
{
    uint8_t count = 0U;
    unsigned int index;

    for (index = 0U; index < 4U; ++index) {
        const unsigned int xy_shift = index * 8U;
        const unsigned int z_shift = index * 4U;
        GeGbiTriangle *triangle = &triangles[index];

        triangle->vertex[0] = (uint8_t)((w1 >> xy_shift) & UINT32_C(0xf));
        triangle->vertex[1] = (uint8_t)((w1 >> (xy_shift + 4U)) & UINT32_C(0xf));
        triangle->vertex[2] = (uint8_t)((w0 >> z_shift) & UINT32_C(0xf));

        /* Rare encodes unused slots as the otherwise degenerate 0,0,0. */
        if (triangle->vertex[0] != 0U || triangle->vertex[1] != 0U
                || triangle->vertex[2] != 0U) {
            count = (uint8_t)(index + 1U);
        }
    }

    return count;
}

static void ge_gbi_decode_color(uint32_t w0, uint32_t w1,
                                GeGbiCommand *command)
{
    command->data.color.red = (uint8_t)(w1 >> 24);
    command->data.color.green = (uint8_t)(w1 >> 16);
    command->data.color.blue = (uint8_t)(w1 >> 8);
    command->data.color.alpha = (uint8_t)w1;
    command->data.color.min_level = (uint8_t)(w0 >> 8);
    command->data.color.lod_fraction = (uint8_t)w0;
}

static void ge_gbi_decode_tile_rect(uint32_t w0, uint32_t w1,
                                    GeGbiCommand *command)
{
    command->data.tile_rect.upper_s = (uint16_t)((w0 >> 12) & UINT32_C(0xfff));
    command->data.tile_rect.upper_t = (uint16_t)(w0 & UINT32_C(0xfff));
    command->data.tile_rect.tile = (uint8_t)((w1 >> 24) & UINT32_C(7));
    command->data.tile_rect.lower_s = (uint16_t)((w1 >> 12) & UINT32_C(0xfff));
    command->data.tile_rect.lower_t = (uint16_t)(w1 & UINT32_C(0xfff));
}

static GeGbiStatus ge_gbi_decode_words(uint32_t w0, uint32_t w1,
                                       size_t byte_offset,
                                       GeGbiCommand *command)
{
    const uint8_t opcode = (uint8_t)(w0 >> 24);

    memset(command, 0, sizeof(*command));
    command->kind = GE_GBI_COMMAND_UNKNOWN;
    command->byte_offset = byte_offset;
    command->raw_w0 = w0;
    command->raw_w1 = w1;
    command->opcode = opcode;

    switch (opcode) {
    case GE_GBI_OPCODE_NOOP:
        command->kind = GE_GBI_COMMAND_NOOP;
        break;
    case GE_GBI_OPCODE_MATRIX:
        command->kind = GE_GBI_COMMAND_MATRIX;
        command->data.dma.parameters = (uint8_t)(w0 >> 16);
        command->data.dma.length = (uint16_t)w0;
        command->data.dma.address = ge_gbi_address(w1);
        if (command->data.dma.length != 64U) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    case GE_GBI_OPCODE_MOVE_MEMORY:
        command->kind = GE_GBI_COMMAND_MOVE_MEMORY;
        command->data.move_memory.index = (uint8_t)(w0 >> 16);
        command->data.move_memory.length = (uint16_t)w0;
        command->data.move_memory.address = ge_gbi_address(w1);
        if (command->data.move_memory.index == UINT8_C(0x80)) {
            command->data.move_memory.kind = GE_GBI_MOVE_MEMORY_VIEWPORT;
        } else if (command->data.move_memory.index == UINT8_C(0x82)) {
            command->data.move_memory.kind = GE_GBI_MOVE_MEMORY_LOOK_AT_Y;
        } else if (command->data.move_memory.index == UINT8_C(0x84)) {
            command->data.move_memory.kind = GE_GBI_MOVE_MEMORY_LOOK_AT_X;
        } else if (command->data.move_memory.index >= UINT8_C(0x86)
                && command->data.move_memory.index <= UINT8_C(0x94)
                && (command->data.move_memory.index & UINT8_C(1)) == 0U) {
            command->data.move_memory.kind = GE_GBI_MOVE_MEMORY_LIGHT;
            command->data.move_memory.light_slot = (uint8_t)(
                (command->data.move_memory.index - UINT8_C(0x86)) / 2U);
        } else if (command->data.move_memory.index == UINT8_C(0x98)
                || command->data.move_memory.index == UINT8_C(0x9a)
                || command->data.move_memory.index == UINT8_C(0x9c)
                || command->data.move_memory.index == UINT8_C(0x9e)) {
            command->data.move_memory.kind = GE_GBI_MOVE_MEMORY_FORCE_MATRIX_PART;
            command->data.move_memory.matrix_part = command->data.move_memory.index
                == UINT8_C(0x9e) ? 0U
                : (uint8_t)((command->data.move_memory.index
                    - UINT8_C(0x98)) / 2U + 1U);
        }
        if (command->data.move_memory.kind != GE_GBI_MOVE_MEMORY_UNKNOWN
                && command->data.move_memory.length != 16U) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    case GE_GBI_OPCODE_VERTEX: {
        const uint16_t length = (uint16_t)w0;
        const uint8_t parameters = (uint8_t)(w0 >> 16);

        command->kind = GE_GBI_COMMAND_VERTEX;
        command->data.vertex.address = ge_gbi_address(w1);
        command->data.vertex.count = (uint16_t)(length / 16U);
        command->data.vertex.first = (uint8_t)(parameters & UINT8_C(0xf));
        if (length == 0U || (length % 16U) != 0U
                || command->data.vertex.count > 16U
                || (uint16_t)command->data.vertex.first
                    + command->data.vertex.count > 16U
                || (uint8_t)(parameters >> 4)
                    != (uint8_t)(command->data.vertex.count - 1U)) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    }
    case GE_GBI_OPCODE_DISPLAY_LIST:
        command->kind = GE_GBI_COMMAND_DISPLAY_LIST;
        command->data.display_list.push = (uint8_t)(w0 >> 16);
        command->data.display_list.address = ge_gbi_address(w1);
        if (command->data.display_list.push > 1U) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    case GE_GBI_OPCODE_TRIANGLE4:
        command->kind = GE_GBI_COMMAND_TRIANGLE4;
        command->data.geometry.count = ge_gbi_decode_tri4(
            w0, w1, command->data.geometry.triangles);
        break;
    case GE_GBI_OPCODE_CLEAR_GEOMETRY:
        command->kind = GE_GBI_COMMAND_CLEAR_GEOMETRY_MODE;
        command->data.geometry_mode.mask = w1;
        break;
    case GE_GBI_OPCODE_SET_GEOMETRY:
        command->kind = GE_GBI_COMMAND_SET_GEOMETRY_MODE;
        command->data.geometry_mode.mask = w1;
        break;
    case GE_GBI_OPCODE_END_DISPLAY_LIST:
        command->kind = GE_GBI_COMMAND_END_DISPLAY_LIST;
        break;
    case GE_GBI_OPCODE_SET_OTHER_MODE_LOW:
    case GE_GBI_OPCODE_SET_OTHER_MODE_HIGH:
        command->kind = opcode == GE_GBI_OPCODE_SET_OTHER_MODE_LOW
            ? GE_GBI_COMMAND_SET_OTHER_MODE_LOW
            : GE_GBI_COMMAND_SET_OTHER_MODE_HIGH;
        command->data.other_mode.shift = (uint16_t)((w0 >> 8) & UINT32_C(0xffff));
        command->data.other_mode.length = (uint8_t)w0;
        command->data.other_mode.data = w1;
        break;
    case GE_GBI_OPCODE_TEXTURE:
        command->kind = GE_GBI_COMMAND_TEXTURE;
        command->data.texture.reserved = (uint8_t)(w0 >> 16);
        command->data.texture.level = (uint8_t)((w0 >> 11) & UINT32_C(7));
        command->data.texture.tile = (uint8_t)((w0 >> 8) & UINT32_C(7));
        command->data.texture.enabled = (uint8_t)w0;
        command->data.texture.scale_s = (uint16_t)(w1 >> 16);
        command->data.texture.scale_t = (uint16_t)w1;
        break;
    case GE_GBI_OPCODE_MOVE_WORD:
        command->kind = GE_GBI_COMMAND_MOVE_WORD;
        command->data.move_word.offset = (uint16_t)((w0 >> 8) & UINT32_C(0xffff));
        command->data.move_word.index = (uint8_t)w0;
        command->data.move_word.data = w1;
        break;
    case GE_GBI_OPCODE_POP_MATRIX:
        command->kind = GE_GBI_COMMAND_POP_MATRIX;
        command->data.pop_matrix.projection = (uint8_t)w1;
        if (w1 > 1U) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    case GE_GBI_OPCODE_TRIANGLE:
        command->kind = GE_GBI_COMMAND_TRIANGLE;
        command->data.geometry.count = 1U;
        ge_gbi_decode_tri1(w1, &command->data.geometry.triangles[0]);
        if (((w1 >> 16) & UINT32_C(0xff)) % 10U != 0U
                || ((w1 >> 8) & UINT32_C(0xff)) % 10U != 0U
                || (w1 & UINT32_C(0xff)) % 10U != 0U) {
            return GE_GBI_STATUS_MALFORMED;
        }
        break;
    case GE_GBI_OPCODE_RARE_SET_TEXTURE:
        command->kind = GE_GBI_COMMAND_RARE_SET_TEXTURE;
        command->data.rare_texture.clamp_mirror_s = (uint8_t)((w0 >> 22) & UINT32_C(3));
        command->data.rare_texture.clamp_mirror_t = (uint8_t)((w0 >> 20) & UINT32_C(3));
        command->data.rare_texture.tile = (uint8_t)((w0 >> 18) & UINT32_C(3));
        command->data.rare_texture.shift_s = (uint8_t)((w0 >> 14) & UINT32_C(0xf));
        command->data.rare_texture.shift_t = (uint8_t)((w0 >> 10) & UINT32_C(0xf));
        command->data.rare_texture.type = (uint8_t)(w0 & UINT32_C(7));
        command->data.rare_texture.min_level = (uint8_t)(w1 >> 24);
        command->data.rare_texture.detail_texture_id = (uint16_t)((w1 >> 12) & UINT32_C(0xfff));
        command->data.rare_texture.texture_id = (uint16_t)(w1 & UINT32_C(0xfff));
        break;
    case GE_GBI_OPCODE_SET_TEXTURE_IMAGE:
        command->kind = GE_GBI_COMMAND_SET_TEXTURE_IMAGE;
        command->data.image.format = (uint8_t)((w0 >> 21) & UINT32_C(7));
        command->data.image.size = (uint8_t)((w0 >> 19) & UINT32_C(3));
        command->data.image.width = (uint16_t)((w0 & UINT32_C(0xfff)) + 1U);
        command->data.image.address = ge_gbi_address(w1);
        break;
    case GE_GBI_OPCODE_SET_COMBINE:
        command->kind = GE_GBI_COMMAND_SET_COMBINE;
        command->data.combine.mux0 = w0 & UINT32_C(0x00ffffff);
        command->data.combine.mux1 = w1;
        break;
    case GE_GBI_OPCODE_SET_ENV_COLOR:
    case GE_GBI_OPCODE_SET_PRIM_COLOR:
    case GE_GBI_OPCODE_SET_BLEND_COLOR:
    case GE_GBI_OPCODE_SET_FOG_COLOR:
    case GE_GBI_OPCODE_SET_FILL_COLOR:
        if (opcode == GE_GBI_OPCODE_SET_ENV_COLOR) {
            command->kind = GE_GBI_COMMAND_SET_ENV_COLOR;
        } else if (opcode == GE_GBI_OPCODE_SET_PRIM_COLOR) {
            command->kind = GE_GBI_COMMAND_SET_PRIM_COLOR;
        } else if (opcode == GE_GBI_OPCODE_SET_BLEND_COLOR) {
            command->kind = GE_GBI_COMMAND_SET_BLEND_COLOR;
        } else if (opcode == GE_GBI_OPCODE_SET_FOG_COLOR) {
            command->kind = GE_GBI_COMMAND_SET_FOG_COLOR;
        } else {
            command->kind = GE_GBI_COMMAND_SET_FILL_COLOR;
        }
        ge_gbi_decode_color(w0, w1, command);
        break;
    case GE_GBI_OPCODE_FILL_RECTANGLE:
        command->kind = GE_GBI_COMMAND_FILL_RECTANGLE;
        command->data.screen_rect.lower_x =
            (uint16_t)((w0 >> 14) & UINT32_C(0x3ff));
        command->data.screen_rect.lower_y =
            (uint16_t)((w0 >> 2) & UINT32_C(0x3ff));
        command->data.screen_rect.upper_x =
            (uint16_t)((w1 >> 14) & UINT32_C(0x3ff));
        command->data.screen_rect.upper_y =
            (uint16_t)((w1 >> 2) & UINT32_C(0x3ff));
        break;
    case GE_GBI_OPCODE_TEXTURE_RECTANGLE:
    case GE_GBI_OPCODE_TEXTURE_RECTANGLE_FLIP:
        command->kind = GE_GBI_COMMAND_TEXTURE_RECTANGLE;
        command->data.texture_rect.screen.lower_x =
            (uint16_t)((w0 >> 12) & UINT32_C(0xfff));
        command->data.texture_rect.screen.lower_y =
            (uint16_t)(w0 & UINT32_C(0xfff));
        command->data.texture_rect.screen.upper_x =
            (uint16_t)((w1 >> 12) & UINT32_C(0xfff));
        command->data.texture_rect.screen.upper_y =
            (uint16_t)(w1 & UINT32_C(0xfff));
        command->data.texture_rect.tile =
            (uint8_t)((w1 >> 24) & UINT32_C(7));
        command->data.texture_rect.flipped =
            opcode == GE_GBI_OPCODE_TEXTURE_RECTANGLE_FLIP ? 1U : 0U;
        break;
    case GE_GBI_OPCODE_RDP_HALF_1:
    case GE_GBI_OPCODE_RDP_HALF_2:
        command->kind = opcode == GE_GBI_OPCODE_RDP_HALF_1
            ? GE_GBI_COMMAND_RDP_HALF_1 : GE_GBI_COMMAND_RDP_HALF_2;
        command->data.rdp_half.word = w1;
        command->data.rdp_half.high = (int16_t)(w1 >> 16);
        command->data.rdp_half.low = (int16_t)w1;
        break;
    case GE_GBI_OPCODE_SET_TILE:
        command->kind = GE_GBI_COMMAND_SET_TILE;
        command->data.tile.format = (uint8_t)((w0 >> 21) & UINT32_C(7));
        command->data.tile.size = (uint8_t)((w0 >> 19) & UINT32_C(3));
        command->data.tile.line = (uint16_t)((w0 >> 9) & UINT32_C(0x1ff));
        command->data.tile.tmem = (uint16_t)(w0 & UINT32_C(0x1ff));
        command->data.tile.tile = (uint8_t)((w1 >> 24) & UINT32_C(7));
        command->data.tile.palette = (uint8_t)((w1 >> 20) & UINT32_C(0xf));
        command->data.tile.clamp_mirror_t = (uint8_t)((w1 >> 18) & UINT32_C(3));
        command->data.tile.mask_t = (uint8_t)((w1 >> 14) & UINT32_C(0xf));
        command->data.tile.shift_t = (uint8_t)((w1 >> 10) & UINT32_C(0xf));
        command->data.tile.clamp_mirror_s = (uint8_t)((w1 >> 8) & UINT32_C(3));
        command->data.tile.mask_s = (uint8_t)((w1 >> 4) & UINT32_C(0xf));
        command->data.tile.shift_s = (uint8_t)(w1 & UINT32_C(0xf));
        break;
    case GE_GBI_OPCODE_LOAD_TILE:
    case GE_GBI_OPCODE_SET_TILE_SIZE:
        command->kind = opcode == GE_GBI_OPCODE_LOAD_TILE
            ? GE_GBI_COMMAND_LOAD_TILE : GE_GBI_COMMAND_SET_TILE_SIZE;
        ge_gbi_decode_tile_rect(w0, w1, command);
        break;
    case GE_GBI_OPCODE_LOAD_BLOCK:
        command->kind = GE_GBI_COMMAND_LOAD_BLOCK;
        command->data.load_block.upper_s = (uint16_t)((w0 >> 12) & UINT32_C(0xfff));
        command->data.load_block.upper_t = (uint16_t)(w0 & UINT32_C(0xfff));
        command->data.load_block.tile = (uint8_t)((w1 >> 24) & UINT32_C(7));
        command->data.load_block.lower_s = (uint16_t)((w1 >> 12) & UINT32_C(0xfff));
        command->data.load_block.dxt = (uint16_t)(w1 & UINT32_C(0xfff));
        break;
    case GE_GBI_OPCODE_LOAD_TLUT:
        command->kind = GE_GBI_COMMAND_LOAD_TLUT;
        command->data.load_tlut.tile = (uint8_t)((w1 >> 24) & UINT32_C(7));
        command->data.load_tlut.count = (uint16_t)((w1 >> 14) & UINT32_C(0x3ff));
        break;
    case GE_GBI_OPCODE_PIPE_SYNC:
        command->kind = GE_GBI_COMMAND_PIPE_SYNC;
        break;
    case GE_GBI_OPCODE_TILE_SYNC:
        command->kind = GE_GBI_COMMAND_TILE_SYNC;
        break;
    case GE_GBI_OPCODE_LOAD_SYNC:
        command->kind = GE_GBI_COMMAND_LOAD_SYNC;
        break;
    case GE_GBI_OPCODE_FULL_SYNC:
        command->kind = GE_GBI_COMMAND_FULL_SYNC;
        break;
    default:
        break;
    }

    return GE_GBI_STATUS_OK;
}

GeGbiStatus ge_gbi_decode_command(const uint8_t *bytes,
                                  size_t byte_count,
                                  GeGbiByteOrder byte_order,
                                  size_t byte_offset,
                                  GeGbiCommand *command)
{
    uint32_t w0;
    uint32_t w1;

    if (bytes == NULL || command == NULL
            || (byte_order != GE_GBI_BYTE_ORDER_BIG_ENDIAN
                && byte_order != GE_GBI_BYTE_ORDER_LITTLE_ENDIAN)) {
        return GE_GBI_STATUS_INVALID_ARGUMENT;
    }
    if (byte_count < 8U) {
        return GE_GBI_STATUS_TRUNCATED;
    }

    w0 = ge_gbi_read_word(bytes, byte_order);
    w1 = ge_gbi_read_word(bytes + 4, byte_order);
    return ge_gbi_decode_words(w0, w1, byte_offset, command);
}

GeGbiDecodeResult ge_gbi_decode_list(const uint8_t *bytes,
                                     size_t byte_count,
                                     GeGbiByteOrder byte_order,
                                     GeGbiCommand *commands,
                                     size_t command_capacity)
{
    GeGbiDecodeResult result;

    memset(&result, 0, sizeof(result));
    result.status = GE_GBI_STATUS_OK;

    if (bytes == NULL || (commands == NULL && command_capacity != 0U)
            || (byte_order != GE_GBI_BYTE_ORDER_BIG_ENDIAN
                && byte_order != GE_GBI_BYTE_ORDER_LITTLE_ENDIAN)) {
        result.status = GE_GBI_STATUS_INVALID_ARGUMENT;
        return result;
    }

    while (result.bytes_consumed < byte_count) {
        GeGbiStatus status;

        if (byte_count - result.bytes_consumed < 8U) {
            result.status = GE_GBI_STATUS_TRUNCATED;
            return result;
        }
        if (result.commands_decoded >= command_capacity) {
            result.status = GE_GBI_STATUS_OUTPUT_FULL;
            return result;
        }

        status = ge_gbi_decode_command(bytes + result.bytes_consumed, 8U,
                                       byte_order, result.bytes_consumed,
                                       &commands[result.commands_decoded]);
        if (status != GE_GBI_STATUS_OK) {
            result.status = status;
            return result;
        }

        ++result.commands_decoded;
        result.bytes_consumed += 8U;
        if (commands[result.commands_decoded - 1U].kind
                == GE_GBI_COMMAND_END_DISPLAY_LIST) {
            result.reached_end_display_list = 1U;
            return result;
        }
    }

    return result;
}

const char *ge_gbi_command_kind_name(GeGbiCommandKind kind)
{
    static const char *const names[] = {
        "unknown", "noop", "matrix", "move-memory", "vertex",
        "display-list", "triangle", "triangle4", "clear-geometry-mode",
        "set-geometry-mode", "end-display-list", "pop-matrix", "move-word",
        "texture", "set-other-mode-low", "set-other-mode-high",
        "rare-set-texture", "set-texture-image", "set-combine",
        "set-env-color", "set-prim-color", "set-blend-color", "set-fog-color",
        "set-fill-color", "fill-rectangle", "texture-rectangle", "rdp-half-1",
        "rdp-half-2", "set-tile", "load-tile", "load-block",
        "set-tile-size", "load-tlut", "pipe-sync", "tile-sync", "load-sync",
        "full-sync"
    };

    if ((unsigned int)kind >= sizeof(names) / sizeof(names[0])) {
        return "invalid";
    }
    return names[kind];
}
