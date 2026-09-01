#include "ge_gbi_pipeline.h"
#include "ge_gbi_matrix_fixture.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct DrawLog {
    size_t actions;
    size_t draws;
    uint32_t primitive_color;
    GeGbiVertex triangle_vertices[3];
    GeGbiProcessedVertex processed_triangle_vertices[3];
    size_t matrix_actions;
    uint8_t maximum_modelview_count;
    uint8_t saw_projection;
    size_t draw_sequence_index;
    size_t fill_rectangles;
    GeGbiScreenRect fill_rectangle;
    size_t texture_rectangles;
    GeGbiTextureRectangle texture_rectangle;
} DrawLog;

static GeGbiAddress address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress result;

    result.segment = segment;
    result.offset = offset;
    result.raw = ((uint32_t)segment << 24) | offset;
    return result;
}

static int log_draw(const GeGbiPipelineEvent *event, void *user_data)
{
    DrawLog *log = user_data;

    log->actions++;
    if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_MATRIX) {
        const uint8_t parameters = event->action.data.matrix.parameters;

        assert(event->action.data.matrix.has_value != 0U);
        ++log->matrix_actions;
        if (event->state->modelview_stack.count
                > log->maximum_modelview_count) {
            log->maximum_modelview_count =
                event->state->modelview_stack.count;
        }
        if ((parameters & GE_GBI_MTX_PROJECTION) != 0U) {
            log->saw_projection = 1U;
        }
    }
    if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        const GeGbiTriangle *triangle = &event->action.data.draw.triangles[0];
        uint8_t index;

        log->draws++;
        log->draw_sequence_index = event->sequence_index;
        log->primitive_color = event->state->primitive_color;
        for (index = 0U; index < 3U; index++) {
            log->triangle_vertices[index] =
                event->vertex_cache[triangle->vertex[index]];
            log->processed_triangle_vertices[index] =
                event->processed_vertex_cache[triangle->vertex[index]];
        }
    }
    if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_FILL_RECTANGLE) {
        log->draws++;
        log->fill_rectangles++;
        log->fill_rectangle = event->action.data.fill_rectangle;
    }
    if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE) {
        log->draws++;
        log->texture_rectangles++;
        log->texture_rectangle = event->action.data.texture_rectangle;
    }
    return 1;
}

static void test_pipeline_texture_rectangle(void)
{
    static const uint8_t commands[] = {
        /* Flipped glyph rectangle and its signed S/T + derivative halves. */
        0xe5, 0x2b, 0xc0, 0x5c, 0x03, 0x0f, 0x00, 0x34,
        0xb4, 0, 0, 0, 0xff, 0xe0, 0x00, 0x40,
        0xb3, 0, 0, 0, 0x04, 0x00, 0xfc, 0x00,
        0xb8, 0, 0, 0, 0, 0, 0, 0,
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 8U};
    GeGbiPipelineResult result;
    DrawLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands))
           == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, log_draw, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.unsupported_commands == 0U);
    assert(result.draw_calls == 1U && result.triangles == 2U);
    assert(log.texture_rectangles == 1U && log.draws == 1U);
    assert(log.texture_rectangle.screen.upper_x == 240U);
    assert(log.texture_rectangle.screen.lower_x == 700U);
    assert(log.texture_rectangle.tile == 3U);
    assert(log.texture_rectangle.flipped == 1U);
    assert(log.texture_rectangle.s == -32);
    assert(log.texture_rectangle.t == 64);
    assert(log.texture_rectangle.dsdx == 1024);
    assert(log.texture_rectangle.dtdy == -1024);
}

static void test_pipeline_fill_rectangle(void)
{
    static const uint8_t commands[] = {
        /* gDPSetFillColor(0x11223344) */
        0xf7, 0, 0, 0, 0x11, 0x22, 0x33, 0x44,
        /* gDPFillRectangle(60, 13, 175, 23) */
        0xf6, 0x2b, 0xc0, 0x5c, 0x00, 0x0f, 0x00, 0x34,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 8U};
    GeGbiPipelineResult result;
    DrawLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, log_draw, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.unsupported_commands == 0U);
    assert(result.draw_calls == 1U && result.triangles == 2U);
    assert(result.final_state.fill_color == UINT32_C(0x11223344));
    assert(log.fill_rectangles == 1U && log.draws == 1U);
    assert(log.fill_rectangle.upper_x == 60U);
    assert(log.fill_rectangle.upper_y == 13U);
    assert(log.fill_rectangle.lower_x == 175U);
    assert(log.fill_rectangle.lower_y == 23U);
}

static void test_pipeline_primary_secondary_continuation(void)
{
    static const uint8_t commands[] = {
        /* Primary: establish a runtime segment, load slots and set state. */
        0xbc, 0x00, 0x0c, 0x06, 0x00, 0x00, 0x10, 0x00,
        0x04, 0x20, 0x00, 0x30, 0x02, 0x00, 0x00, 0x00,
        0xfa, 0x00, 0x00, 0x00, 0x89, 0xab, 0xcd, 0xef,
        0xb8, 0, 0, 0, 0, 0, 0, 0,
        /* Secondary: consume Primary's cache/state and segment table. */
        0xbf, 0, 0, 0, 0, 0, 0x0a, 0x14,
        0x04, 0x20, 0x00, 0x30, 0x03, 0x00, 0x00, 0x00,
        0xbf, 0, 0, 0, 0, 0, 0x0a, 0x14,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t vertices[] = {
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0, 0,
        0, 0, 0, 0, 0x10, 0x20, 0x30, 0xff,
        0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0, 0,
        0, 0, 0, 0, 0x40, 0x50, 0x60, 0xff,
        0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0, 0,
        0, 0, 0, 0, 0x70, 0x80, 0x90, 0xff
    };
    const GeGbiAddress roots[2] = {
        {UINT32_C(0x01000000), 0U, 1U},
        {UINT32_C(0x01000020), UINT32_C(0x20), 1U},
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 8U};
    GeGbiPipelineResult result;
    DrawLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands))
           == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, vertices,
                                         sizeof(vertices))
           == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_physical_region(
               &memory, 0U, UINT32_C(0x1000), vertices,
               sizeof(vertices)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(
        &memory, roots[1], GE_GBI_BYTE_ORDER_BIG_ENDIAN,
        &config, NULL, NULL);
    assert(result.status == GE_GBI_PIPELINE_STATE_ERROR);
    assert(result.state_status == GE_GBI_STATE_MISSING_VERTEX);

    result = ge_gbi_pipeline_execute_sequence(
        &memory, roots, 2U, GE_GBI_BYTE_ORDER_BIG_ENDIAN,
        &config, log_draw, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 8U);
    assert(result.traversal.vertex_batches == 2U);
    assert(result.traversal.vertices_fetched == 6U);
    assert(result.actions_emitted == 6U);
    assert(result.draw_calls == 2U && result.triangles == 2U);
    assert(log.draws == 2U && log.draw_sequence_index == 1U);
    assert(log.primitive_color == UINT32_C(0x89abcdef));
    assert(log.triangle_vertices[0].x == 1);
    assert(log.triangle_vertices[1].y == 5);
    assert(log.triangle_vertices[2].z == 9);
}

static void test_pipeline_matrix_stack(void)
{
    static const uint8_t commands[] = {
        /* Projection load from segment 2 offset 0. */
        0x01, 0x03, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00,
        /* Model-view identity load from offset 0x40. */
        0x01, 0x02, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40,
        /* Push and multiply the nontrivial matrix into model-view. */
        0x01, 0x04, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00,
        /* Pop model-view. */
        0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t matrices[128];
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {4U, 16U};
    GeGbiPipelineResult result;
    DrawLog log;
    const GeGbiMatrix *projection;
    const GeGbiMatrix *modelview;

    memcpy(matrices, ge_test_matrix_fixed_be, 64U);
    memcpy(matrices + 64U, ge_test_matrix_identity_be, 64U);
    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, matrices,
                                         sizeof(matrices)) == GE_GBI_RESOLVE_OK);

    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, log_draw, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 5U);
    assert(result.traversal.matrices_fetched == 3U);
    assert(result.actions_emitted == 4U);
    assert(log.matrix_actions == 3U);
    assert(log.maximum_modelview_count == 2U);
    assert(log.saw_projection != 0U);
    assert(result.final_state.modelview_stack.count == 1U);
    assert(result.final_state.projection_stack.count == 1U);
    projection = ge_gbi_matrix_stack_top(&result.final_state.projection_stack);
    modelview = ge_gbi_matrix_stack_top(&result.final_state.modelview_stack);
    assert(projection != NULL && modelview != NULL);
    assert(projection->elements[0][0] == 1.5f);
    assert(projection->elements[0][1] == -2.25f);
    assert(modelview->elements[0][0] == 1.0f);
    assert(modelview->elements[3][3] == 1.0f);
}

static void test_pipeline_rsp_payloads_and_dynamic_segment(void)
{
    static const uint8_t commands[] = {
        /* gSPSegment(3, 0x1000) */
        0xbc, 0x00, 0x0c, 0x06, 0x00, 0x00, 0x10, 0x00,
        /* gSPViewport(0x03000000) */
        0x03, 0x80, 0x00, 0x10, 0x03, 0x00, 0x00, 0x00,
        /* gSPLight(0x03000010, 1) */
        0x03, 0x86, 0x00, 0x10, 0x03, 0x00, 0x00, 0x10,
        /* gSPLookAtX(0x03000010) */
        0x03, 0x84, 0x00, 0x10, 0x03, 0x00, 0x00, 0x10,
        /* gSPLookAtY(0x03000010) */
        0x03, 0x82, 0x00, 0x10, 0x03, 0x00, 0x00, 0x10,
        /* gSPNumLights(1) */
        0xbc, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x40,
        /* gSPFogFactor(-32, 288) */
        0xbc, 0x00, 0x00, 0x08, 0xff, 0xe0, 0x01, 0x20,
        /* gSPPerspNormalize(0x1234) */
        0xbc, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x12, 0x34,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t rsp_payloads[] = {
        0x02, 0x80, 0x01, 0xe0, 0x03, 0xff, 0x00, 0x00,
        0x02, 0x80, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00,
        0x10, 0x20, 0x30, 0, 0x10, 0x20, 0x30, 0,
        0xff, 0x02, 0xfd, 0, 0, 0, 0, 0
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {4U, 20U};
    GeGbiPipelineResult result;

    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_physical_region(
               &memory, 0U, UINT32_C(0x1000), rsp_payloads,
               sizeof(rsp_payloads)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, NULL, NULL);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 9U);
    assert(result.traversal.rsp_payloads_fetched == 4U);
    assert(result.actions_emitted == 5U);
    assert(result.unsupported_commands == 0U);
    assert(result.final_state.viewport.scale[0] == 640);
    assert(result.final_state.viewport.translation[1] == 480);
    assert((result.final_state.valid_lights & UINT8_C(1)) != 0U);
    assert(result.final_state.lights[0].color[1] == UINT8_C(0x20));
    assert(result.final_state.lights[0].direction[0] == -1);
    assert(result.final_state.valid_look_at == UINT8_C(3));
    assert(result.final_state.look_at[1].direction[2] == -3);
    assert(result.final_state.segment_bases[3] == UINT32_C(0x1000));
    assert(result.final_state.directional_light_count == 1U);
    assert(result.final_state.fog_multiplier == -32);
    assert(result.final_state.fog_offset == 288);
    assert(result.final_state.perspective_normalization == UINT16_C(0x1234));
}

static void test_pipeline_draw(void)
{
    static const uint8_t commands[] = {
        /* gsSPVertex(0x02000000, 3, 0) */
        0x04, 0x20, 0x00, 0x30, 0x02, 0x00, 0x00, 0x00,
        /* gsDPSetPrimColor(0, 0, 0x11, 0x22, 0x33, 0x44) */
        0xfa, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44,
        /* gsSP1Triangle(0, 1, 2, 0) */
        0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x14,
        /* gsSPEndDisplayList() */
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t vertices[] = {
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0, 0,
        0, 0, 0, 0, 0x10, 0x20, 0x30, 0xff,
        0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0, 0,
        0, 0, 0, 0, 0x40, 0x50, 0x60, 0xff,
        0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0, 0,
        0, 0, 0, 0, 0x70, 0x80, 0x90, 0xff
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {4U, 32U};
    GeGbiPipelineResult result;
    DrawLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, vertices,
                                         sizeof(vertices)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, log_draw, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 4U);
    assert(result.actions_emitted == 3U);
    assert(result.draw_calls == 1U);
    assert(result.triangles == 1U);
    assert(result.unsupported_commands == 0U);
    assert(log.actions == 3U && log.draws == 1U);
    assert(log.primitive_color == UINT32_C(0x11223344));
    assert(log.triangle_vertices[0].x == 1);
    assert(log.triangle_vertices[1].y == 5);
    assert(log.triangle_vertices[2].z == 9);
    assert(log.processed_triangle_vertices[0].clip[0] == 1.0f);
    assert(log.processed_triangle_vertices[1].clip[1] == 5.0f);
    assert(log.processed_triangle_vertices[2].clip[2] == 9.0f);
}

static void test_state_failure_and_unknown_skip(void)
{
    static const uint8_t missing_vertex[] = {
        0xbf, 0, 0, 0, 0, 0, 0x0a, 0x14,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t unknown_then_end[] = {
        0xaa, 1, 2, 3, 4, 5, 6, 7,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 8U};
    GeGbiPipelineResult result;

    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, missing_vertex,
                                         sizeof(missing_vertex)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, NULL, NULL);
    assert(result.status == GE_GBI_PIPELINE_STATE_ERROR);
    assert(result.state_status == GE_GBI_STATE_MISSING_VERTEX);

    assert(ge_gbi_memory_map_set_segment(&memory, 1U, unknown_then_end,
                                         sizeof(unknown_then_end)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, NULL, NULL);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.unsupported_commands == 1U);
    assert(result.actions_emitted == 1U);
}

int main(void)
{
    test_pipeline_fill_rectangle();
    test_pipeline_texture_rectangle();
    test_pipeline_draw();
    test_pipeline_primary_secondary_continuation();
    test_pipeline_matrix_stack();
    test_pipeline_rsp_payloads_and_dynamic_segment();
    test_state_failure_and_unknown_skip();
    puts("GoldenEye end-to-end display-list pipeline tests passed");
    return 0;
}
