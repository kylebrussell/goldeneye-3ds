#include "ge_gbi_vertex.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_EPSILON 0.0001f

static int nearly_equal(float left, float right)
{
    return fabsf(left - right) <= TEST_EPSILON;
}

static GeGbiVertex vertex_at(int16_t x, int16_t y, int16_t z)
{
    GeGbiVertex vertex;

    memset(&vertex, 0, sizeof(vertex));
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.alpha = UINT8_C(255);
    return vertex;
}

static void test_identity_and_translation(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(1, 2, 3);
    GeGbiVertex original_vertex;
    GeGbiProcessedVertex processed;
    GeGbiMatrix *modelview;

    ge_gbi_state_init(&state);
    original_vertex = vertex;
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(memcmp(&vertex, &original_vertex, sizeof(vertex)) == 0);
    assert(nearly_equal(processed.eye[0], 1.0f));
    assert(nearly_equal(processed.eye[1], 2.0f));
    assert(nearly_equal(processed.eye[2], 3.0f));
    assert(nearly_equal(processed.clip[0], 1.0f));
    assert(nearly_equal(processed.clip[1], 2.0f));
    assert(nearly_equal(processed.clip[2], 3.0f));
    assert(nearly_equal(processed.clip[3], 1.0f));
    assert(processed.has_ndc != 0U);
    assert(processed.has_screen == 0U);

    modelview = &state.modelview_stack.entries[0];
    modelview->elements[3][0] = 10.0f;
    modelview->elements[3][1] = -4.0f;
    modelview->elements[3][2] = 2.0f;
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(nearly_equal(processed.eye[0], 11.0f));
    assert(nearly_equal(processed.eye[1], -2.0f));
    assert(nearly_equal(processed.eye[2], 5.0f));
}

static void test_perspective_viewport(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(2, 0, 4);
    GeGbiProcessedVertex processed;
    GeGbiMatrix *projection;

    ge_gbi_state_init(&state);
    projection = &state.projection_stack.entries[0];
    memset(projection, 0, sizeof(*projection));
    projection->elements[0][0] = 1.0f;
    projection->elements[1][1] = 1.0f;
    projection->elements[2][2] = 1.0f;
    projection->elements[2][3] = 1.0f;
    state.viewport.scale[0] = 640;
    state.viewport.scale[1] = 480;
    state.viewport.scale[2] = 1023;
    state.viewport.translation[0] = 640;
    state.viewport.translation[1] = 480;
    state.viewport.translation[2] = 0;
    state.viewport_valid = 1U;

    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(nearly_equal(processed.clip[0], 2.0f));
    assert(nearly_equal(processed.clip[2], 4.0f));
    assert(nearly_equal(processed.clip[3], 4.0f));
    assert(nearly_equal(processed.ndc[0], 0.5f));
    assert(nearly_equal(processed.ndc[1], 0.0f));
    assert(nearly_equal(processed.ndc[2], 1.0f));
    assert(nearly_equal(processed.screen[0], 240.0f));
    assert(nearly_equal(processed.screen[1], 120.0f));
    assert(nearly_equal(processed.screen[2], 1023.0f));
    assert(processed.has_screen != 0U);
    assert(processed.clip_flags == 0U);
}

static void test_color_passthrough_without_lighting(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(0, 0, 0);
    GeGbiProcessedVertex processed;

    ge_gbi_state_init(&state);
    vertex.red = UINT8_C(7);
    vertex.green = UINT8_C(8);
    vertex.blue = UINT8_C(9);
    vertex.alpha = UINT8_C(200);
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(processed.rgba[0] == UINT8_C(7));
    assert(processed.rgba[1] == UINT8_C(8));
    assert(processed.rgba[2] == UINT8_C(9));
    assert(processed.rgba[3] == UINT8_C(200));
}

static void test_ambient_and_directional_lighting(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(0, 0, 0);
    GeGbiProcessedVertex processed;

    ge_gbi_state_init(&state);
    state.geometry_mode = GE_GBI_GEOMETRY_LIGHTING;
    state.directional_light_count = 1U;
    state.valid_lights = UINT8_C(3);
    state.lights[0].color[0] = UINT8_C(100);
    state.lights[0].color[1] = UINT8_C(50);
    state.lights[0].color[2] = UINT8_C(25);
    state.lights[0].direction[2] = INT8_C(127);
    state.lights[1].color[0] = UINT8_C(10);
    state.lights[1].color[1] = UINT8_C(20);
    state.lights[1].color[2] = UINT8_C(30);
    vertex.blue = UINT8_C(127);
    vertex.alpha = UINT8_C(200);

    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(nearly_equal(processed.normal[0], 0.0f));
    assert(nearly_equal(processed.normal[1], 0.0f));
    assert(nearly_equal(processed.normal[2], 1.0f));
    assert(processed.rgba[0] == UINT8_C(110));
    assert(processed.rgba[1] == UINT8_C(70));
    assert(processed.rgba[2] == UINT8_C(55));
    assert(processed.rgba[3] == UINT8_C(200));
}

static void test_texture_generation_and_clip_flags(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(-2, 2, 2);
    GeGbiProcessedVertex processed;
    GeGbiMatrix *projection;

    ge_gbi_state_init(&state);
    state.geometry_mode = GE_GBI_GEOMETRY_LIGHTING
        | GE_GBI_GEOMETRY_TEXTURE_GEN;
    state.valid_look_at = UINT8_C(3);
    state.look_at[0].direction[0] = INT8_C(127);
    state.look_at[1].direction[1] = INT8_C(127);
    vertex.red = UINT8_C(127);

    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert(processed.texture_generated != 0U);
    assert(nearly_equal(processed.texture[0], 1.0f));
    assert(nearly_equal(processed.texture[1], 0.5f));
    assert((processed.clip_flags & GE_GBI_CLIP_LEFT) != 0U);
    assert((processed.clip_flags & GE_GBI_CLIP_TOP) != 0U);
    assert((processed.clip_flags & GE_GBI_CLIP_FAR) != 0U);

    vertex = vertex_at(2, -2, -2);
    vertex.red = UINT8_C(127);
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert((processed.clip_flags & GE_GBI_CLIP_RIGHT) != 0U);
    assert((processed.clip_flags & GE_GBI_CLIP_BOTTOM) != 0U);
    assert((processed.clip_flags & GE_GBI_CLIP_NEAR) != 0U);

    projection = &state.projection_stack.entries[0];
    projection->elements[3][3] = -1.0f;
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_OK);
    assert((processed.clip_flags & GE_GBI_CLIP_NONPOSITIVE_W) != 0U);
}

static void test_invalid_arguments(void)
{
    GeGbiRenderState state;
    GeGbiVertex vertex = vertex_at(0, 0, 0);
    GeGbiProcessedVertex processed;

    ge_gbi_state_init(&state);
    assert(ge_gbi_vertex_process(NULL, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT);
    assert(ge_gbi_vertex_process(&state, NULL, &processed)
           == GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT);
    assert(ge_gbi_vertex_process(&state, &vertex, NULL)
           == GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT);
    state.modelview_stack.count = 0U;
    assert(ge_gbi_vertex_process(&state, &vertex, &processed)
           == GE_GBI_VERTEX_PROCESS_INVALID_STATE);
}

int main(void)
{
    test_identity_and_translation();
    test_perspective_viewport();
    test_color_passthrough_without_lighting();
    test_ambient_and_directional_lighting();
    test_texture_generation_and_clip_flags();
    test_invalid_arguments();
    puts("GoldenEye portable vertex processing tests passed");
    return 0;
}
