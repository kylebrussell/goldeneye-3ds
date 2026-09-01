#include "ge_gbi_state.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GeGbiCommand command_of_kind(GeGbiCommandKind kind)
{
    GeGbiCommand command;

    memset(&command, 0, sizeof(command));
    command.kind = kind;
    return command;
}

static void test_vertex_and_triangle_actions(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand vertex = command_of_kind(GE_GBI_COMMAND_VERTEX);
    GeGbiCommand triangle = command_of_kind(GE_GBI_COMMAND_TRIANGLE);

    ge_gbi_state_init(&state);
    vertex.data.vertex.first = 2U;
    vertex.data.vertex.count = 3U;
    vertex.data.vertex.address.raw = UINT32_C(0x05000100);
    vertex.data.vertex.address.segment = 5U;
    vertex.data.vertex.address.offset = UINT32_C(0x100);
    assert(ge_gbi_state_apply(&state, &vertex, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_LOAD_VERTICES);
    assert(action.data.vertices.first == 2U);
    assert(state.valid_vertices == UINT16_C(0x001c));

    triangle.data.geometry.count = 1U;
    triangle.data.geometry.triangles[0].vertex[0] = 2U;
    triangle.data.geometry.triangles[0].vertex[1] = 3U;
    triangle.data.geometry.triangles[0].vertex[2] = 4U;
    assert(ge_gbi_state_apply(&state, &triangle, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES);
    assert(action.data.draw.count == 1U);

    triangle.data.geometry.triangles[0].vertex[2] = 5U;
    assert(ge_gbi_state_apply(&state, &triangle, &action)
            == GE_GBI_STATE_MISSING_VERTEX);
}

static void test_material_and_mode_state(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand command;

    ge_gbi_state_init(&state);
    command = command_of_kind(GE_GBI_COMMAND_SET_GEOMETRY_MODE);
    command.data.geometry_mode.mask = UINT32_C(0x00020404);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.geometry_mode == UINT32_C(0x00020404));
    command.kind = GE_GBI_COMMAND_CLEAR_GEOMETRY_MODE;
    command.data.geometry_mode.mask = UINT32_C(0x00000400);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.geometry_mode == UINT32_C(0x00020004));

    command = command_of_kind(GE_GBI_COMMAND_SET_OTHER_MODE_HIGH);
    command.data.other_mode.shift = 20U;
    command.data.other_mode.length = 2U;
    command.data.other_mode.data = UINT32_C(1) << 20;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.other_mode_high == UINT32_C(1) << 20);
    command.data.other_mode.data = UINT32_C(2) << 20;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.other_mode_high == UINT32_C(2) << 20);

    command = command_of_kind(GE_GBI_COMMAND_SET_PRIM_COLOR);
    command.data.color.red = UINT8_C(0x11);
    command.data.color.green = UINT8_C(0x22);
    command.data.color.blue = UINT8_C(0x33);
    command.data.color.alpha = UINT8_C(0x44);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.primitive_color == UINT32_C(0x11223344));

    command = command_of_kind(GE_GBI_COMMAND_RARE_SET_TEXTURE);
    command.data.rare_texture.texture_id = 123U;
    command.data.rare_texture.detail_texture_id = 456U;
    command.data.rare_texture.tile = 2U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.rare_texture.texture_id == 123U);
    assert(state.rare_texture.detail_texture_id == 456U);
    assert(state.rare_texture.tile == 2U);
    assert(state.rare_texture_valid == 1U);
    assert(state.active_texture_binding == GE_GBI_TEXTURE_BINDING_RARE_ID);

    command = command_of_kind(GE_GBI_COMMAND_SET_TEXTURE_IMAGE);
    command.data.image.address.raw = UINT32_C(0x06001234);
    command.data.image.address.segment = 6U;
    command.data.image.address.offset = UINT32_C(0x1234);
    command.data.image.width = 32U;
    command.data.image.format = 2U;
    command.data.image.size = 1U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.texture_image.address.raw == UINT32_C(0x06001234));
    assert(state.texture_image.width == 32U);
    assert(state.texture_image_valid == 1U);
    assert(state.active_texture_binding == GE_GBI_TEXTURE_BINDING_IMAGE);
}

static void test_tiles_and_control_flow(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand command;

    ge_gbi_state_init(&state);
    command = command_of_kind(GE_GBI_COMMAND_SET_TILE);
    command.data.tile.tile = 3U;
    command.data.tile.line = 8U;
    command.data.tile.tmem = 32U;
    command.data.tile.palette = 2U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.tiles[3].line == 8U);
    assert(state.tiles[3].tmem == 32U);
    assert(state.tiles[3].palette == 2U);

    command = command_of_kind(GE_GBI_COMMAND_SET_TILE_SIZE);
    command.data.tile_rect.tile = 3U;
    command.data.tile_rect.lower_s = 124U;
    command.data.tile_rect.lower_t = 60U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.tiles[3].lower_s == 124U);
    assert(state.tiles[3].lower_t == 60U);

    command = command_of_kind(GE_GBI_COMMAND_DISPLAY_LIST);
    command.data.display_list.address.raw = UINT32_C(0x06001000);
    command.data.display_list.push = 0U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_CALL_DISPLAY_LIST);
    command.data.display_list.push = 1U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_BRANCH_DISPLAY_LIST);

    command = command_of_kind(GE_GBI_COMMAND_END_DISPLAY_LIST);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_END_DISPLAY_LIST);
}

static void test_fill_rectangle_action(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand command = command_of_kind(GE_GBI_COMMAND_FILL_RECTANGLE);

    ge_gbi_state_init(&state);
    command.data.screen_rect.upper_x = 60U;
    command.data.screen_rect.upper_y = 13U;
    command.data.screen_rect.lower_x = 175U;
    command.data.screen_rect.lower_y = 23U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_DRAW_FILL_RECTANGLE);
    assert(action.data.fill_rectangle.upper_x == 60U);
    assert(action.data.fill_rectangle.upper_y == 13U);
    assert(action.data.fill_rectangle.lower_x == 175U);
    assert(action.data.fill_rectangle.lower_y == 23U);
}

static void test_invalid_and_unsupported(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand command = command_of_kind(GE_GBI_COMMAND_UNKNOWN);

    ge_gbi_state_init(&state);
    assert(ge_gbi_state_apply(NULL, &command, &action)
            == GE_GBI_STATE_INVALID_ARGUMENT);
    assert(ge_gbi_state_apply(&state, &command, &action)
            == GE_GBI_STATE_UNSUPPORTED);

    command = command_of_kind(GE_GBI_COMMAND_SET_OTHER_MODE_LOW);
    command.data.other_mode.shift = 31U;
    command.data.other_mode.length = 2U;
    assert(ge_gbi_state_apply(&state, &command, &action)
            == GE_GBI_STATE_INVALID_ARGUMENT);
}

static void test_texture_rectangle_action(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand rectangle =
        command_of_kind(GE_GBI_COMMAND_TEXTURE_RECTANGLE);
    GeGbiCommand half1 = command_of_kind(GE_GBI_COMMAND_RDP_HALF_1);
    GeGbiCommand half2 = command_of_kind(GE_GBI_COMMAND_RDP_HALF_2);

    ge_gbi_state_init(&state);
    rectangle.data.texture_rect.screen.upper_x = 240U;
    rectangle.data.texture_rect.screen.upper_y = 52U;
    rectangle.data.texture_rect.screen.lower_x = 700U;
    rectangle.data.texture_rect.screen.lower_y = 92U;
    rectangle.data.texture_rect.tile = 3U;
    rectangle.data.texture_rect.flipped = 1U;
    half1.data.rdp_half.high = -32;
    half1.data.rdp_half.low = 64;
    half2.data.rdp_half.high = 1024;
    half2.data.rdp_half.low = -1024;

    assert(ge_gbi_state_apply(&state, &rectangle, &action)
           == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_NONE);
    assert(ge_gbi_state_apply(&state, &half1, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_NONE);
    assert(ge_gbi_state_apply(&state, &half2, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE);
    assert(action.data.texture_rectangle.screen.upper_x == 240U);
    assert(action.data.texture_rectangle.screen.lower_x == 700U);
    assert(action.data.texture_rectangle.tile == 3U);
    assert(action.data.texture_rectangle.flipped == 1U);
    assert(action.data.texture_rectangle.s == -32);
    assert(action.data.texture_rectangle.t == 64);
    assert(action.data.texture_rectangle.dsdx == 1024);
    assert(action.data.texture_rectangle.dtdy == -1024);
    assert(state.texture_rectangle_phase == 0U);

    /* Half commands are meaningful only in their authored sequence, and a
     * second rectangle cannot silently replace an incomplete first one. */
    assert(ge_gbi_state_apply(&state, &half2, &action)
           == GE_GBI_STATE_MALFORMED_SEQUENCE);
    assert(ge_gbi_state_apply(&state, &rectangle, &action)
           == GE_GBI_STATE_OK);
    assert(ge_gbi_state_apply(&state, &rectangle, &action)
           == GE_GBI_STATE_MALFORMED_SEQUENCE);
}

static void test_matrix_stack_semantics(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand pop = command_of_kind(GE_GBI_COMMAND_POP_MATRIX);
    GeGbiMatrix base;
    GeGbiMatrix incoming;
    const GeGbiMatrix *top;
    size_t index;

    ge_gbi_state_init(&state);
    ge_gbi_matrix_identity(&base);
    base.elements[3][0] = 10.0f;
    base.elements[3][1] = 20.0f;
    ge_gbi_matrix_identity(&incoming);
    incoming.elements[0][0] = 2.0f;
    incoming.elements[1][1] = 3.0f;

    assert(ge_gbi_state_apply_matrix(&state, &base,
                                     GE_GBI_MTX_LOAD)
           == GE_GBI_STATE_OK);
    assert(ge_gbi_state_apply_matrix(&state, &incoming,
                                     GE_GBI_MTX_MULTIPLY | GE_GBI_MTX_PUSH)
           == GE_GBI_STATE_OK);
    assert(state.modelview_stack.count == 2U);
    top = ge_gbi_matrix_stack_top(&state.modelview_stack);
    assert(top != NULL);
    assert(top->elements[0][0] == 2.0f);
    assert(top->elements[1][1] == 3.0f);
    assert(top->elements[3][0] == 10.0f);
    assert(top->elements[3][1] == 20.0f);

    pop.data.pop_matrix.projection = 0U;
    assert(ge_gbi_state_apply(&state, &pop, &action) == GE_GBI_STATE_OK);
    assert(state.modelview_stack.count == 1U);
    top = ge_gbi_matrix_stack_top(&state.modelview_stack);
    assert(top != NULL && top->elements[3][0] == 10.0f);
    assert(ge_gbi_state_apply(&state, &pop, &action)
           == GE_GBI_STATE_MATRIX_STACK_UNDERFLOW);

    assert(ge_gbi_state_apply_matrix(&state, &incoming,
                                     GE_GBI_MTX_PROJECTION | GE_GBI_MTX_LOAD)
           == GE_GBI_STATE_OK);
    assert(state.projection_stack.entries[0].elements[0][0] == 2.0f);
    assert(state.modelview_stack.entries[0].elements[0][0] == 1.0f);

    for (index = 1U; index < GE_GBI_MATRIX_STACK_CAPACITY; ++index) {
        assert(ge_gbi_state_apply_matrix(&state, &base,
                                         GE_GBI_MTX_LOAD | GE_GBI_MTX_PUSH)
               == GE_GBI_STATE_OK);
    }
    assert(ge_gbi_state_apply_matrix(&state, &base,
                                     GE_GBI_MTX_LOAD | GE_GBI_MTX_PUSH)
           == GE_GBI_STATE_MATRIX_STACK_OVERFLOW);
    assert(ge_gbi_state_apply_matrix(&state, &base, UINT8_C(0x80))
           == GE_GBI_STATE_INVALID_MATRIX_PARAMETERS);
}

static void test_move_word_and_move_memory_state(void)
{
    GeGbiRenderState state;
    GeGbiStateAction action;
    GeGbiCommand command = command_of_kind(GE_GBI_COMMAND_MOVE_WORD);

    ge_gbi_state_init(&state);
    command.data.move_word.index = GE_GBI_MOVE_WORD_SEGMENT;
    command.data.move_word.offset = 12U;
    command.data.move_word.data = UINT32_C(0x00123000);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.segment_bases[3] == UINT32_C(0x00123000));
    assert((state.valid_segment_bases & UINT16_C(0x0008)) != 0U);
    command.data.move_word.offset = 3U;
    assert(ge_gbi_state_apply(&state, &command, &action)
           == GE_GBI_STATE_INVALID_MOVE_WORD);

    command.data.move_word.index = GE_GBI_MOVE_WORD_NUM_LIGHTS;
    command.data.move_word.offset = 0U;
    command.data.move_word.data = UINT32_C(0x80000060);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.directional_light_count == 2U);

    command.data.move_word.index = GE_GBI_MOVE_WORD_FOG;
    command.data.move_word.data = UINT32_C(0xffe00120);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.fog_multiplier == -32);
    assert(state.fog_offset == 288);

    command.data.move_word.index = GE_GBI_MOVE_WORD_PERSP_NORM;
    command.data.move_word.data = UINT32_C(0x1234);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(state.perspective_normalization == UINT16_C(0x1234));

    command = command_of_kind(GE_GBI_COMMAND_MOVE_MEMORY);
    command.data.move_memory.kind = GE_GBI_MOVE_MEMORY_VIEWPORT;
    command.data.move_memory.address.raw = UINT32_C(0x03000000);
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_LOAD_VIEWPORT);
    command.data.move_memory.kind = GE_GBI_MOVE_MEMORY_LIGHT;
    command.data.move_memory.light_slot = 5U;
    assert(ge_gbi_state_apply(&state, &command, &action) == GE_GBI_STATE_OK);
    assert(action.kind == GE_GBI_STATE_ACTION_LOAD_LIGHT);
    assert(action.data.light.slot == 5U);
}

int main(void)
{
    test_vertex_and_triangle_actions();
    test_material_and_mode_state();
    test_tiles_and_control_flow();
    test_fill_rectangle_action();
    test_texture_rectangle_action();
    test_invalid_and_unsupported();
    test_matrix_stack_semantics();
    test_move_word_and_move_memory_state();
    puts("GoldenEye GBI render-state tests passed");
    return 0;
}
