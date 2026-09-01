#include "ge_blotter_model.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

typedef struct BlotterLog {
    size_t matrix_actions;
    size_t vertex_actions;
    size_t draw_actions;
} BlotterLog;

static FileData read_file(const char *directory, const char *name)
{
    char path[1024];
    FILE *file;
    long file_size;
    int path_length;
    FileData result = {NULL, 0U};

    path_length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(path_length >= 0);
    assert((size_t)path_length < sizeof(path));
    file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0L, SEEK_END) == 0);
    file_size = ftell(file);
    assert(file_size >= 0L);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    result.size = (size_t)file_size;
    result.bytes = malloc(result.size == 0U ? 1U : result.size);
    assert(result.bytes != NULL);
    assert(fread(result.bytes, 1U, result.size, file) == result.size);
    assert(fclose(file) == 0);
    return result;
}

static GeGbiAddress address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress result;

    result.segment = segment;
    result.offset = offset;
    result.raw = ((uint32_t)segment << 24) | offset;
    return result;
}

static int inspect_action(const GeGbiPipelineEvent *event, void *user_data)
{
    BlotterLog *log = user_data;

    if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_MATRIX) {
        const GeGbiMatrix *matrix = &event->action.data.matrix.value;
        size_t row;
        size_t column;

        ++log->matrix_actions;
        assert(event->action.data.matrix.has_value != 0U);
        for (row = 0U; row < 4U; ++row) {
            for (column = 0U; column < 4U; ++column) {
                const float expected = row == column ? 1.0f : 0.0f;
                assert(matrix->elements[row][column] == expected);
            }
        }
    } else if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_VERTICES) {
        ++log->vertex_actions;
        assert(event->action.data.vertices.count == 4U);
        assert(event->action.data.vertices.first == 0U);
    } else if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        static const int16_t expected_x[] = {360, -360, -360, 360};
        static const int16_t expected_z[] = {-240, -240, 240, 240};
        size_t index;

        ++log->draw_actions;
        assert(event->state->rare_texture.texture_id == 182U);
        assert(event->action.data.draw.count == 2U);
        for (index = 0U; index < 4U; ++index) {
            assert(event->vertex_cache[index].x == expected_x[index]);
            assert(event->vertex_cache[index].y == 0);
            assert(event->vertex_cache[index].z == expected_z[index]);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    FileData display_list;
    FileData vertices;
    FileData matrix;
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig config = {4U, 32U};
    GeGbiPipelineResult result;
    GeBlotterModelBlobs blobs;
    GeBlotterModel model;
    BlotterLog log;

    assert(argc == 2);
    display_list = read_file(argv[1], "display_list.bin");
    vertices = read_file(argv[1], "vertices.bin");
    matrix = read_file(argv[1], "matrix_identity.bin");
    assert(display_list.size == 80U);
    assert(vertices.size == 64U);
    assert(matrix.size == 64U);

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 3U, matrix.bytes,
                                         matrix.size) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 4U, vertices.bytes,
                                         vertices.size) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 5U, display_list.bytes,
                                         display_list.size) == GE_GBI_RESOLVE_OK);

    result = ge_gbi_pipeline_execute(&memory, address(5U, 0U),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                     &config, inspect_action, &log);
    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.state_status == GE_GBI_STATE_OK);
    assert(result.traversal.commands_visited == 10U);
    assert(result.traversal.vertex_batches == 1U);
    assert(result.traversal.vertices_fetched == 4U);
    assert(result.traversal.matrices_fetched == 1U);
    assert(result.draw_calls == 1U);
    assert(result.triangles == 2U);
    assert(result.unsupported_commands == 0U);
    assert(log.matrix_actions == 1U);
    assert(log.vertex_actions == 1U);
    assert(log.draw_actions == 1U);

    blobs.display_list = display_list.bytes;
    blobs.display_list_size = display_list.size;
    blobs.vertices = vertices.bytes;
    blobs.vertices_size = vertices.size;
    blobs.matrix = matrix.bytes;
    blobs.matrix_size = matrix.size;
    assert(ge_blotter_model_build(&blobs, &model) == GE_BLOTTER_MODEL_OK);
    assert(model.material.texture_id == GE_BLOTTER_MODEL_TEXTURE_ID);
    assert(model.triangle_count == GE_BLOTTER_MODEL_TRIANGLE_COUNT);
    assert(model.vertex_count == GE_BLOTTER_MODEL_VERTEX_COUNT);
    assert(model.pipeline.traversal.commands_visited == 10U);
    assert(model.pipeline.draw_calls == 1U);
    assert(model.triangles[0].vertices[0].processed.object[0] == 360.0f);
    assert(model.triangles[0].vertices[0].processed.object[2] == -240.0f);

    free(matrix.bytes);
    free(vertices.bytes);
    free(display_list.bytes);
    puts("GoldenEye blotter model GBI pipeline test passed");
    return 0;
}
