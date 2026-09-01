#include "ge_gbi_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t goldeneye_geometry_fixture[] = {
    /* gsSPVertex(0x05001230, 8, 2) */
    0x04, 0x72, 0x00, 0x80, 0x05, 0x00, 0x12, 0x30,
    /* gsSP4Triangles(1,2,3, 4,5,6, 7,8,9, 10,11,12) */
    0xb1, 0x00, 0xc9, 0x63, 0xba, 0x87, 0x54, 0x21,
    /* gsSP1Triangle(2, 5, 7, 0), classic F3D multiplies indices by 10 */
    0xbf, 0x00, 0x00, 0x00, 0x00, 0x14, 0x32, 0x46,
    /* gsSPEndDisplayList() */
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t goldeneye_material_fixture[] = {
    /* gsSPUseTexture(2,3,2,4,5,TEXTURETYPE_DETAIL,0x80,0x123,0x456) */
    0xc0, 0xb9, 0x14, 0x01, 0x80, 0x12, 0x34, 0x56,
    /* gsDPSetTextureImage(G_IM_FMT_RGBA,G_IM_SIZ_16b,32,0x07001000) */
    0xfd, 0x10, 0x00, 0x1f, 0x07, 0x00, 0x10, 0x00,
    /* gsDPSetTile(RGBA,16b,8,0x20,3,2,1,5,6,2,4,7) */
    0xf5, 0x10, 0x10, 0x20, 0x03, 0x25, 0x5a, 0x47,
    /* gsDPSetTileSize(3, 4, 8, 124, 60) */
    0xf2, 0x00, 0x40, 0x08, 0x03, 0x07, 0xc0, 0x3c,
    /* gsDPSetPrimColor(0,0x80,0x11,0x22,0x33,0x44) */
    0xfa, 0x00, 0x00, 0x80, 0x11, 0x22, 0x33, 0x44,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t goldeneye_state_fixture[] = {
    /* gsSPMatrix(0x06002000, 2) */
    0x01, 0x02, 0x00, 0x40, 0x06, 0x00, 0x20, 0x00,
    /* gsSPDisplayList(0x07003000) */
    0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x30, 0x00,
    /* gsSPTexture(0x8000, 0x4000, 2, 3, 1) */
    0xbb, 0x00, 0x13, 0x01, 0x80, 0x00, 0x40, 0x00,
    /* gsSPSetGeometryMode(0x00020005) */
    0xb7, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x05,
    /* gsSPSetOtherMode(G_SETOTHERMODE_H, 12, 2, 0x00300000) */
    0xba, 0x00, 0x0c, 0x02, 0x00, 0x30, 0x00, 0x00,
    /* gsSPSegment(3, 0x80400000) */
    0xbc, 0x00, 0x0c, 0x06, 0x80, 0x40, 0x00, 0x00,
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void test_geometry(void)
{
    GeGbiCommand commands[4];
    GeGbiDecodeResult result = ge_gbi_decode_list(
        goldeneye_geometry_fixture, sizeof(goldeneye_geometry_fixture),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 4U);

    assert(result.status == GE_GBI_STATUS_OK);
    assert(result.commands_decoded == 4U);
    assert(result.bytes_consumed == sizeof(goldeneye_geometry_fixture));
    assert(result.reached_end_display_list == 1U);

    assert(commands[0].kind == GE_GBI_COMMAND_VERTEX);
    assert(commands[0].data.vertex.count == 8U);
    assert(commands[0].data.vertex.first == 2U);
    assert(commands[0].data.vertex.address.segment == 5U);
    assert(commands[0].data.vertex.address.offset == UINT32_C(0x1230));

    assert(commands[1].kind == GE_GBI_COMMAND_TRIANGLE4);
    assert(commands[1].data.geometry.count == 4U);
    assert(commands[1].data.geometry.triangles[0].vertex[0] == 1U);
    assert(commands[1].data.geometry.triangles[0].vertex[1] == 2U);
    assert(commands[1].data.geometry.triangles[0].vertex[2] == 3U);
    assert(commands[1].data.geometry.triangles[3].vertex[0] == 10U);
    assert(commands[1].data.geometry.triangles[3].vertex[1] == 11U);
    assert(commands[1].data.geometry.triangles[3].vertex[2] == 12U);

    assert(commands[2].kind == GE_GBI_COMMAND_TRIANGLE);
    assert(commands[2].data.geometry.triangles[0].vertex[0] == 2U);
    assert(commands[2].data.geometry.triangles[0].vertex[1] == 5U);
    assert(commands[2].data.geometry.triangles[0].vertex[2] == 7U);
    assert(strcmp(ge_gbi_command_kind_name(commands[2].kind), "triangle") == 0);
}

static void test_material(void)
{
    GeGbiCommand commands[6];
    GeGbiDecodeResult result = ge_gbi_decode_list(
        goldeneye_material_fixture, sizeof(goldeneye_material_fixture),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 6U);

    assert(result.status == GE_GBI_STATUS_OK);
    assert(commands[0].kind == GE_GBI_COMMAND_RARE_SET_TEXTURE);
    assert(commands[0].data.rare_texture.clamp_mirror_s == 2U);
    assert(commands[0].data.rare_texture.clamp_mirror_t == 3U);
    assert(commands[0].data.rare_texture.tile == 2U);
    assert(commands[0].data.rare_texture.shift_s == 4U);
    assert(commands[0].data.rare_texture.shift_t == 5U);
    assert(commands[0].data.rare_texture.type == 1U);
    assert(commands[0].data.rare_texture.min_level == UINT8_C(0x80));
    assert(commands[0].data.rare_texture.detail_texture_id == UINT16_C(0x123));
    assert(commands[0].data.rare_texture.texture_id == UINT16_C(0x456));

    assert(commands[1].kind == GE_GBI_COMMAND_SET_TEXTURE_IMAGE);
    assert(commands[1].data.image.format == 0U);
    assert(commands[1].data.image.size == 2U);
    assert(commands[1].data.image.width == 32U);
    assert(commands[1].data.image.address.segment == 7U);
    assert(commands[1].data.image.address.offset == UINT32_C(0x1000));

    assert(commands[2].kind == GE_GBI_COMMAND_SET_TILE);
    assert(commands[2].data.tile.line == 8U);
    assert(commands[2].data.tile.tmem == UINT16_C(0x20));
    assert(commands[2].data.tile.tile == 3U);
    assert(commands[2].data.tile.palette == 2U);
    assert(commands[2].data.tile.clamp_mirror_t == 1U);
    assert(commands[2].data.tile.mask_t == 5U);
    assert(commands[2].data.tile.shift_t == 6U);
    assert(commands[2].data.tile.clamp_mirror_s == 2U);
    assert(commands[2].data.tile.mask_s == 4U);
    assert(commands[2].data.tile.shift_s == 7U);

    assert(commands[3].kind == GE_GBI_COMMAND_SET_TILE_SIZE);
    assert(commands[3].data.tile_rect.upper_s == 4U);
    assert(commands[3].data.tile_rect.upper_t == 8U);
    assert(commands[3].data.tile_rect.lower_s == 124U);
    assert(commands[3].data.tile_rect.lower_t == 60U);
    assert(commands[4].kind == GE_GBI_COMMAND_SET_PRIM_COLOR);
    assert(commands[4].data.color.lod_fraction == UINT8_C(0x80));
    assert(commands[4].data.color.red == UINT8_C(0x11));
    assert(commands[4].data.color.alpha == UINT8_C(0x44));
}

static void test_endianness_and_bounds(void)
{
    static const uint8_t little_endian_end[] = {
        0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t malformed_vertex[] = {
        0x04, 0x10, 0x00, 0x11, 0x01, 0x00, 0x00, 0x00
    };
    GeGbiCommand command;
    GeGbiCommand commands[1];
    GeGbiDecodeResult result;

    assert(ge_gbi_decode_command(little_endian_end,
                                 sizeof(little_endian_end),
                                 GE_GBI_BYTE_ORDER_LITTLE_ENDIAN, 24U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.kind == GE_GBI_COMMAND_END_DISPLAY_LIST);
    assert(command.byte_offset == 24U);

    assert(ge_gbi_decode_command(goldeneye_geometry_fixture, 7U,
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_TRUNCATED);
    assert(ge_gbi_decode_command(malformed_vertex, sizeof(malformed_vertex),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_MALFORMED);

    result = ge_gbi_decode_list(goldeneye_geometry_fixture,
                                sizeof(goldeneye_geometry_fixture),
                                GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 1U);
    assert(result.status == GE_GBI_STATUS_OUTPUT_FULL);
    assert(result.commands_decoded == 1U);
    assert(result.bytes_consumed == 8U);

    result = ge_gbi_decode_list(goldeneye_geometry_fixture,
                                sizeof(goldeneye_geometry_fixture) - 1U,
                                GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 1U);
    assert(result.status == GE_GBI_STATUS_OUTPUT_FULL);

    result = ge_gbi_decode_list(goldeneye_geometry_fixture + 24U, 7U,
                                GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 1U);
    assert(result.status == GE_GBI_STATUS_TRUNCATED);
    assert(result.commands_decoded == 0U);
}

static void test_render_state(void)
{
    GeGbiCommand commands[7];
    GeGbiDecodeResult result = ge_gbi_decode_list(
        goldeneye_state_fixture, sizeof(goldeneye_state_fixture),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, commands, 7U);

    assert(result.status == GE_GBI_STATUS_OK);
    assert(result.commands_decoded == 7U);
    assert(commands[0].kind == GE_GBI_COMMAND_MATRIX);
    assert(commands[0].data.dma.parameters == 2U);
    assert(commands[0].data.dma.length == 64U);
    assert(commands[0].data.dma.address.segment == 6U);
    assert(commands[1].kind == GE_GBI_COMMAND_DISPLAY_LIST);
    assert(commands[1].data.display_list.push == 0U);
    assert(commands[1].data.display_list.address.segment == 7U);
    assert(commands[2].kind == GE_GBI_COMMAND_TEXTURE);
    assert(commands[2].data.texture.level == 2U);
    assert(commands[2].data.texture.tile == 3U);
    assert(commands[2].data.texture.enabled == 1U);
    assert(commands[2].data.texture.scale_s == UINT16_C(0x8000));
    assert(commands[2].data.texture.scale_t == UINT16_C(0x4000));
    assert(commands[3].kind == GE_GBI_COMMAND_SET_GEOMETRY_MODE);
    assert(commands[3].data.geometry_mode.mask == UINT32_C(0x00020005));
    assert(commands[4].kind == GE_GBI_COMMAND_SET_OTHER_MODE_HIGH);
    assert(commands[4].data.other_mode.shift == 12U);
    assert(commands[4].data.other_mode.length == 2U);
    assert(commands[4].data.other_mode.data == UINT32_C(0x00300000));
    assert(commands[5].kind == GE_GBI_COMMAND_MOVE_WORD);
    assert(commands[5].data.move_word.offset == 12U);
    assert(commands[5].data.move_word.index == 6U);
    assert(commands[5].data.move_word.data == UINT32_C(0x80400000));
}

static void test_unknown_is_preserved(void)
{
    static const uint8_t fixture[] = {
        0xaa, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde
    };
    GeGbiCommand command;

    assert(ge_gbi_decode_command(fixture, sizeof(fixture),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.kind == GE_GBI_COMMAND_UNKNOWN);
    assert(command.opcode == UINT8_C(0xaa));
    assert(command.raw_w0 == UINT32_C(0xaa123456));
    assert(command.raw_w1 == UINT32_C(0x789abcde));
}

static void test_classic_matrix_pop(void)
{
    static const uint8_t pop_projection[] = {
        0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    static const uint8_t malformed_pop[] = {
        0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
    };
    GeGbiCommand command;

    assert(ge_gbi_decode_command(pop_projection, sizeof(pop_projection),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.kind == GE_GBI_COMMAND_POP_MATRIX);
    assert(command.data.pop_matrix.projection == 1U);
    assert(ge_gbi_decode_command(malformed_pop, sizeof(malformed_pop),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_MALFORMED);
}

static void test_classic_move_memory_targets(void)
{
    static const uint8_t commands[] = {
        0x03, 0x80, 0x00, 0x10, 0x03, 0x00, 0x00, 0x00,
        0x03, 0x8a, 0x00, 0x10, 0x03, 0x00, 0x00, 0x10,
        0x03, 0x9c, 0x00, 0x10, 0x03, 0x00, 0x00, 0x30
    };
    static const uint8_t malformed_viewport[] = {
        0x03, 0x80, 0x00, 0x0f, 0x03, 0x00, 0x00, 0x00
    };
    GeGbiCommand command;

    assert(ge_gbi_decode_command(commands, sizeof(commands),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.kind == GE_GBI_COMMAND_MOVE_MEMORY);
    assert(command.data.move_memory.kind == GE_GBI_MOVE_MEMORY_VIEWPORT);
    assert(command.data.move_memory.length == 16U);
    assert(ge_gbi_decode_command(commands + 8U, sizeof(commands) - 8U,
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 8U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.data.move_memory.kind == GE_GBI_MOVE_MEMORY_LIGHT);
    assert(command.data.move_memory.light_slot == 2U);
    assert(ge_gbi_decode_command(commands + 16U, 8U,
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 16U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.data.move_memory.kind
           == GE_GBI_MOVE_MEMORY_FORCE_MATRIX_PART);
    assert(command.data.move_memory.matrix_part == 3U);
    assert(ge_gbi_decode_command(malformed_viewport,
                                 sizeof(malformed_viewport),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_MALFORMED);
}

static void test_fill_rectangle(void)
{
    /* gDPFillRectangle(60, 13, 175, 23) from the watch black-box path. */
    static const uint8_t fixture[] = {
        0xf6, 0x2b, 0xc0, 0x5c, 0x00, 0x0f, 0x00, 0x34
    };
    GeGbiCommand command;

    assert(ge_gbi_decode_command(fixture, sizeof(fixture),
                                 GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                 &command) == GE_GBI_STATUS_OK);
    assert(command.kind == GE_GBI_COMMAND_FILL_RECTANGLE);
    assert(command.data.screen_rect.upper_x == 60U);
    assert(command.data.screen_rect.upper_y == 13U);
    assert(command.data.screen_rect.lower_x == 175U);
    assert(command.data.screen_rect.lower_y == 23U);
    assert(strcmp(ge_gbi_command_kind_name(command.kind),
                  "fill-rectangle") == 0);
}

static void test_texture_rectangle_sequence(void)
{
    /* gSPTextureRectangle(60*4,13*4,175*4,23*4,3,-32,64,
     *                     0x400,0xfc00), using GoldenEye's classic Fast3D
     * E4/B4/B3 command sequence. */
    static const uint8_t fixture[] = {
        0xe4, 0x2b, 0xc0, 0x5c, 0x03, 0x0f, 0x00, 0x34,
        0xb4, 0x00, 0x00, 0x00, 0xff, 0xe0, 0x00, 0x40,
        0xb3, 0x00, 0x00, 0x00, 0x04, 0x00, 0xfc, 0x00,
    };
    GeGbiCommand commands[3];
    GeGbiDecodeResult result = ge_gbi_decode_list(
        fixture, sizeof(fixture), GE_GBI_BYTE_ORDER_BIG_ENDIAN,
        commands, 3U);

    assert(result.status == GE_GBI_STATUS_OK);
    assert(result.commands_decoded == 3U);
    assert(commands[0].kind == GE_GBI_COMMAND_TEXTURE_RECTANGLE);
    assert(commands[0].data.texture_rect.screen.upper_x == 240U);
    assert(commands[0].data.texture_rect.screen.upper_y == 52U);
    assert(commands[0].data.texture_rect.screen.lower_x == 700U);
    assert(commands[0].data.texture_rect.screen.lower_y == 92U);
    assert(commands[0].data.texture_rect.tile == 3U);
    assert(commands[0].data.texture_rect.flipped == 0U);
    assert(commands[1].kind == GE_GBI_COMMAND_RDP_HALF_1);
    assert(commands[1].data.rdp_half.word == UINT32_C(0xffe00040));
    assert(commands[1].data.rdp_half.high == -32);
    assert(commands[1].data.rdp_half.low == 64);
    assert(commands[2].kind == GE_GBI_COMMAND_RDP_HALF_2);
    assert(commands[2].data.rdp_half.high == 1024);
    assert(commands[2].data.rdp_half.low == -1024);
    assert(strcmp(ge_gbi_command_kind_name(commands[0].kind),
                  "texture-rectangle") == 0);

    /* E5 has the same payload with the authored S/T axes transposed. */
    {
        uint8_t flipped[8];
        GeGbiCommand command;
        memcpy(flipped, fixture, sizeof(flipped));
        flipped[0] = UINT8_C(0xe5);
        assert(ge_gbi_decode_command(flipped, sizeof(flipped),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U,
                                     &command) == GE_GBI_STATUS_OK);
        assert(command.kind == GE_GBI_COMMAND_TEXTURE_RECTANGLE);
        assert(command.data.texture_rect.flipped == 1U);
    }
}

int main(void)
{
    test_geometry();
    test_material();
    test_render_state();
    test_endianness_and_bounds();
    test_unknown_is_preserved();
    test_classic_matrix_pop();
    test_classic_move_memory_targets();
    test_fill_rectangle();
    test_texture_rectangle_sequence();
    puts("GoldenEye Fast3D decoder tests passed");
    return 0;
}
