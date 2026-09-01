#include "ge_gbi_pipeline.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct DamDrawLog {
    size_t draws;
    size_t triangles;
    uint16_t expected_texture_id;
} DamDrawLog;

static GeGbiAddress address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress result;

    result.segment = segment;
    result.offset = offset;
    result.raw = ((uint32_t)segment << 24) | offset;
    return result;
}

static int log_dam_draw(const GeGbiPipelineEvent *event, void *user_data)
{
    DamDrawLog *log = user_data;

    if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        assert(event->state->rare_texture_valid != 0U);
        assert(event->state->active_texture_binding
            == GE_GBI_TEXTURE_BINDING_RARE_ID);
        assert(event->state->rare_texture.texture_id
            == log->expected_texture_id);
        assert(event->state->texture.enabled != 0U);
        ++log->draws;
        log->triangles += event->action.data.draw.count;
    }
    return 1;
}

static void test_synthetic_room_texture_and_segment(void)
{
    static const uint8_t commands[] = {
        /* Enable full-scale texturing. */
        0xbb, 0x00, 0x30, 0x01, 0xff, 0xff, 0xff, 0xff,
        /* Rare C0: tile 2, texture type 2, texture ID 0x123. */
        0xc0, 0x08, 0x00, 0x02, 0x00, 0x00, 0x01, 0x23,
        /* Load three vertices into slots 0..2 from room segment 0x0e. */
        0x04, 0x20, 0x00, 0x30, 0x0e, 0x00, 0x00, 0x00,
        /* Rare TRI4 containing (0, 1, 2) in its first slot. */
        0xb1, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t vertices[] = {
        0xff, 0xf6, 0x00, 0x00, 0x00, 0x00, 0, 0,
        0, 0, 0, 0, 0xff, 0x00, 0x00, 0xff,
        0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0, 0,
        0, 0, 0, 0, 0x00, 0xff, 0x00, 0xff,
        0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0, 0,
        0, 0, 0, 0, 0x00, 0x00, 0xff, 0xff
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 16U};
    GeGbiPipelineResult result;
    DamDrawLog log = {0U, 0U, UINT16_C(0x123)};

    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
        sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 14U, vertices,
        sizeof(vertices)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, log_dam_draw, &log);

    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 5U);
    assert(result.traversal.vertex_batches == 1U);
    assert(result.traversal.vertices_fetched == 3U);
    assert(result.unsupported_commands == 0U);
    assert(result.draw_calls == 1U && result.triangles == 1U);
    assert(log.draws == 1U && log.triangles == 1U);
    assert(result.final_state.rare_texture.type == UINT8_C(2));
    assert(result.final_state.rare_texture.tile == UINT8_C(2));
}

static uint8_t *read_exact(const char *path, size_t expected_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *bytes;

    assert(file != NULL);
    bytes = malloc(expected_size);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, expected_size, file) == expected_size);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    return bytes;
}

static void test_private_room_asset(const char *point_path,
                                    const char *gdl_path)
{
    enum {
        DAM_ROOM1_POINT_SIZE = 576,
        DAM_ROOM1_GDL_SIZE = 176,
        DAM_ROOM1_TEXTURE_ID = 949
    };
    uint8_t *points = read_exact(point_path, DAM_ROOM1_POINT_SIZE);
    uint8_t *gdl = read_exact(gdl_path, DAM_ROOM1_GDL_SIZE);
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 32U};
    GeGbiPipelineResult result;
    DamDrawLog log = {0U, 0U, DAM_ROOM1_TEXTURE_ID};

    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, gdl,
        DAM_ROOM1_GDL_SIZE) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 14U, points,
        DAM_ROOM1_POINT_SIZE) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_pipeline_execute(&memory, address(1U, 0U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, log_dam_draw, &log);

    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.commands_visited == 22U);
    assert(result.traversal.vertex_batches == 3U);
    assert(result.traversal.vertices_fetched == 36U);
    assert(result.unsupported_commands == 0U);
    assert(result.draw_calls == 7U);
    assert(result.triangles == 25U);
    assert(log.draws == 7U && log.triangles == 25U);
    assert(result.final_state.geometry_mode == UINT32_C(0x00002000));
    assert(result.final_state.environment_color == UINT32_C(0x000000ff));
    assert(result.final_state.rare_texture.type == UINT8_C(2));
    assert(result.final_state.rare_texture.tile == UINT8_C(2));

    free(gdl);
    free(points);
}

int main(int argc, char **argv)
{
    test_synthetic_room_texture_and_segment();
    if (argc == 3) {
        test_private_room_asset(argv[1], argv[2]);
        puts("Dam room 1 private-asset GBI integration test passed");
    } else {
        assert(argc == 1);
    }
    puts("Dam room 1 synthetic GBI compatibility test passed");
    return 0;
}
