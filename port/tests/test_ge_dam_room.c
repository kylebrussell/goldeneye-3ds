#include "ge_dam_room.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

static void test_synthetic_multi_room_scene(void)
{
    static const uint8_t points[] = {
        0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0, 0,
        0, 0, 0, 0, 0x10, 0x20, 0x30, 0xff,
        0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0, 0,
        0, 0, 0, 0, 0x40, 0x50, 0x60, 0xff,
        0xff, 0xff, 0xff, 0xfe, 0xff, 0xfd, 0, 0,
        0, 0, 0, 0, 0x70, 0x80, 0x90, 0xff
    };
    static const uint8_t room_a_primary[] = {
        0xbb, 0x00, 0x30, 0x01, 0xff, 0xff, 0xff, 0xff,
        0xc0, 0x08, 0x00, 0x02, 0x00, 0x00, 0x01, 0x01,
        0x04, 0x20, 0x00, 0x30, 0x0e, 0x00, 0x00, 0x00,
        0xb1, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
        0xc0, 0x08, 0x00, 0x02, 0x00, 0x00, 0x01, 0x02,
        0xb1, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t room_a_secondary[] = {
        0xbb, 0x00, 0x30, 0x01, 0xff, 0xff, 0xff, 0xff,
        0xc0, 0x08, 0x00, 0x02, 0x00, 0x00, 0x01, 0x03,
        0x04, 0x20, 0x00, 0x30, 0x0e, 0x00, 0x00, 0x00,
        0xb1, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t room_b_primary[] = {
        0xbb, 0x00, 0x30, 0x01, 0xff, 0xff, 0xff, 0xff,
        0xc0, 0x08, 0x00, 0x02, 0x00, 0x00, 0x02, 0x01,
        0x04, 0x20, 0x00, 0x30, 0x0e, 0x00, 0x00, 0x00,
        0xb1, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    const GeDamRoomBlobDescriptor rooms[] = {
        {
            7U, {100.0f, 200.0f, -50.0f},
            points, sizeof(points),
            room_a_primary, sizeof(room_a_primary),
            room_a_secondary, sizeof(room_a_secondary)
        },
        {
            9U, {-10.0f, 0.0f, 5.0f},
            points, sizeof(points),
            room_b_primary, sizeof(room_b_primary),
            NULL, 0U
        }
    };
    GeDamRoomWorldVertex vertices[12];
    GeDamRoomDrawBatch batches[4];
    const GeDamRoomSceneStorage storage = {
        vertices, sizeof(vertices) / sizeof(vertices[0]),
        batches, sizeof(batches) / sizeof(batches[0])
    };
    const GeDamRoomSceneStorage short_storage = {
        vertices, 11U, batches, 4U
    };
    GeDamRoomBlobDescriptor invalid_room = rooms[0];
    GeDamRoomScene scene;

    assert(ge_dam_rooms_build(rooms, 2U, NULL, NULL, &scene)
            == GE_DAM_ROOM_CAPACITY_EXCEEDED);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    assert(scene.required_vertex_count == 12U);
    assert(scene.required_batch_count == 4U);
    assert(scene.room_count == 2U && scene.list_count == 3U);
    assert(scene.triangle_count == 4U);
    assert(scene.commands_visited == 17U);

    assert(ge_dam_rooms_build(rooms, 2U, NULL, &short_storage, &scene)
            == GE_DAM_ROOM_CAPACITY_EXCEEDED);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);
    assert(scene.required_vertex_count == 12U);
    assert(scene.required_batch_count == 4U);

    assert(ge_dam_rooms_build(rooms, 2U, NULL, &storage, &scene)
            == GE_DAM_ROOM_OK);
    assert(scene.vertex_count == 12U && scene.batch_count == 4U);
    assert(scene.triangle_count == 4U && scene.list_count == 3U);
    assert(batches[0].room_id == 7U);
    assert(batches[0].list_kind == GE_DAM_ROOM_LIST_PRIMARY);
    assert(batches[0].first_vertex == 0U
            && batches[0].vertex_count == 3U);
    assert(batches[0].texture_valid != 0U);
    assert(batches[0].texture.texture_id == UINT16_C(0x101));
    assert(batches[0].material.texture_id == UINT16_C(0x101));
    assert(batches[1].room_id == 7U
            && batches[1].texture.texture_id == UINT16_C(0x102));
    assert(batches[1].first_vertex == 3U);
    assert(batches[2].list_kind == GE_DAM_ROOM_LIST_SECONDARY);
    assert(batches[2].texture.texture_id == UINT16_C(0x103));
    assert(batches[2].first_vertex == 6U);
    assert(batches[3].room_id == 9U
            && batches[3].list_kind == GE_DAM_ROOM_LIST_PRIMARY);
    assert(batches[3].texture.texture_id == UINT16_C(0x201));
    assert(batches[3].first_vertex == 9U);
    assert(vertices[0].source.x == 1 && vertices[0].source.y == 2
            && vertices[0].source.z == 3);
    assert(vertices[0].world[0] == 101.0f
            && vertices[0].world[1] == 202.0f
            && vertices[0].world[2] == -47.0f);
    assert(vertices[9].world[0] == -9.0f
            && vertices[9].world[1] == 2.0f
            && vertices[9].world[2] == 8.0f);

    invalid_room.secondary_gdl_size = 0U;
    assert(ge_dam_rooms_build(&invalid_room, 1U, NULL, &storage, &scene)
            == GE_DAM_ROOM_INVALID_BLOB_LAYOUT);
    assert(scene.vertex_count == 0U && scene.batch_count == 0U);

    assert(ge_dam_rooms_build(NULL, 0U, NULL, NULL, &scene)
            == GE_DAM_ROOM_OK);
    assert(scene.room_count == 0U && scene.vertex_count == 0U
            && scene.batch_count == 0U);

    {
        /* Exact shape used by authored zero-vertex portal/logic rooms (for
         * example Streets 20-54): pipe sync, mode state, end display list,
         * padding. Canonical bgLoadRoomModelData marks these loaded with a
         * NULL vertex pointer. */
        static const uint8_t empty_primary[] = {
            0xe7, 0, 0, 0, 0, 0, 0, 0,
            0xba, 0, 0x0c, 0x02, 0, 0, 0x20, 0,
            0xb8, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0
        };
        const GeDamRoomBlobDescriptor empty_room = {
            20U, {0.0f, 0.0f, 0.0f},
            NULL, 0U, empty_primary, sizeof(empty_primary), NULL, 0U
        };
        GeDamRoomBlobDescriptor invalid_empty_room = empty_room;

        assert(ge_dam_rooms_build(&empty_room, 1U, NULL, NULL, &scene)
                == GE_DAM_ROOM_OK);
        assert(scene.room_count == 1U && scene.list_count == 1U);
        assert(scene.vertex_count == 0U && scene.batch_count == 0U
                && scene.triangle_count == 0U);
        invalid_empty_room.point_table_size = 16U;
        assert(ge_dam_rooms_build(&invalid_empty_room, 1U, NULL, NULL,
                    &scene) == GE_DAM_ROOM_INVALID_BLOB_LAYOUT);
    }
}

static FileData read_file(const char *directory, const char *name)
{
    char path[1024];
    FILE *file;
    long file_size;
    int path_length;
    FileData result = {NULL, 0U};

    path_length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(path_length >= 0 && (size_t)path_length < sizeof(path));
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

int main(int argc, char **argv)
{
    FileData points;
    FileData gdl;
    GeDamRoomBlobs blobs;
    GeDamRoomModel model;
    size_t index;
    int16_t min_x = INT16_MAX;
    int16_t max_x = INT16_MIN;
    int16_t min_y = INT16_MAX;
    int16_t max_y = INT16_MIN;
    int16_t min_z = INT16_MAX;
    int16_t max_z = INT16_MIN;

    test_synthetic_multi_room_scene();

    assert(argc == 2);
    points = read_file(argv[1], "point_table.bin");
    gdl = read_file(argv[1], "primary_gdl.bin");
    blobs.point_table = points.bytes;
    blobs.point_table_size = points.size;
    blobs.primary_gdl = gdl.bytes;
    blobs.primary_gdl_size = gdl.size;
    assert(ge_dam_room1_build(&blobs, &model) == GE_DAM_ROOM_OK);
    assert(model.pipeline.traversal.commands_visited == 22U);
    assert(model.pipeline.traversal.vertex_batches == 3U);
    assert(model.pipeline.traversal.vertices_fetched == 36U);
    assert(model.pipeline.draw_calls == 7U);
    assert(model.pipeline.triangles == 25U);
    assert(model.vertex_count == 75U);
    assert(model.texture.texture_id == 949U);
    assert(model.material.texture_id == 949U);
    for (index = 0U; index < model.vertex_count; index++) {
        const GeGbiVertex *vertex = &model.vertices[index].source;
        if (vertex->x < min_x) min_x = vertex->x;
        if (vertex->x > max_x) max_x = vertex->x;
        if (vertex->y < min_y) min_y = vertex->y;
        if (vertex->y > max_y) max_y = vertex->y;
        if (vertex->z < min_z) min_z = vertex->z;
        if (vertex->z > max_z) max_z = vertex->z;
    }
    assert(min_x == -631 && max_x == 632);
    assert(min_y == -1050 && max_y == 1050);
    assert(min_z == -1068 && max_z == 1068);

    {
        GeDamRoomWorldVertex world_vertices[GE_DAM_ROOM1_RENDER_VERTEX_COUNT];
        GeDamRoomDrawBatch batches[GE_DAM_ROOM1_DRAW_COUNT];
        const GeDamRoomBlobDescriptor room = {
            1U, {3536.0f, 850.0f, -693.0f},
            points.bytes, points.size, gdl.bytes, gdl.size, NULL, 0U
        };
        const GeDamRoomSceneStorage storage = {
            world_vertices,
            sizeof(world_vertices) / sizeof(world_vertices[0]),
            batches, sizeof(batches) / sizeof(batches[0])
        };
        GeDamRoomScene scene;

        assert(ge_dam_rooms_build(&room, 1U, NULL, &storage, &scene)
                == GE_DAM_ROOM_OK);
        assert(scene.room_count == 1U && scene.list_count == 1U);
        assert(scene.commands_visited == 22U);
        assert(scene.batch_count == 7U && scene.triangle_count == 25U);
        assert(scene.vertex_count == 75U);
        for (index = 0U; index < scene.batch_count; ++index) {
            assert(batches[index].room_id == 1U);
            assert(batches[index].list_kind == GE_DAM_ROOM_LIST_PRIMARY);
            assert(batches[index].texture_valid != 0U);
            assert(batches[index].material.texture_id == 949U);
        }
        assert(world_vertices[0].world[0]
                == 3536.0f + (float)world_vertices[0].source.x);
        assert(world_vertices[0].world[1]
                == 850.0f + (float)world_vertices[0].source.y);
        assert(world_vertices[0].world[2]
                == -693.0f + (float)world_vertices[0].source.z);
        printf("Dam scene storage: vertex=%zu batch=%zu scene=%zu bytes\n",
               sizeof(GeDamRoomWorldVertex), sizeof(GeDamRoomDrawBatch),
               sizeof(GeDamRoomScene));
    }
    free(gdl.bytes);
    free(points.bytes);
    puts("GoldenEye Dam room 1 model pipeline test passed");
    return 0;
}
