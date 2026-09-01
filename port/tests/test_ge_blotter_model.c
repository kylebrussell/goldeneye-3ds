#include "ge_blotter_model.h"
#include "ge_gbi_matrix_fixture.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void put_s16_be(uint8_t *destination, int16_t value)
{
    const uint16_t bits = (uint16_t)value;

    destination[0] = (uint8_t)(bits >> 8);
    destination[1] = (uint8_t)bits;
}

static void make_vertex(uint8_t *destination, int16_t x, int16_t y,
                        int16_t z, int16_t s, int16_t t,
                        uint8_t color)
{
    memset(destination, 0, 16U);
    put_s16_be(destination, x);
    put_s16_be(destination + 2U, y);
    put_s16_be(destination + 4U, z);
    put_s16_be(destination + 8U, s);
    put_s16_be(destination + 10U, t);
    destination[12] = color;
    destination[13] = color;
    destination[14] = color;
    destination[15] = UINT8_MAX;
}

static GeBlotterModelBlobs make_fixture(uint8_t commands[80],
                                        uint8_t vertices[64],
                                        uint16_t texture_id)
{
    GeBlotterModelBlobs blobs;

    memset(commands, 0, GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES);
    /* Load the caller-owned model-view matrix from segment 3. */
    memcpy(commands, (const uint8_t[]){
        0x01, 0x02, 0x00, 0x40, 0x03, 0x00, 0x00, 0x00
    }, 8U);
    /* Select a Rare texture material. */
    commands[8] = UINT8_C(0xc0);
    commands[14] = (uint8_t)(texture_id >> 8);
    commands[15] = (uint8_t)texture_id;
    /* Four vertices at segment 4 offset zero, starting in cache slot zero. */
    memcpy(commands + 16U, (const uint8_t[]){
        0x04, 0x30, 0x00, 0x40, 0x04, 0x00, 0x00, 0x00
    }, 8U);
    /* Two ordinary Fast3D triangles: (0,1,2) and (0,2,3). */
    memcpy(commands + 24U, (const uint8_t[]){
        0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x14,
        0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x1e,
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }, 24U);

    make_vertex(vertices, -4, 0, -2, 0, 0, 32U);
    make_vertex(vertices + 16U, 4, 0, -2, 64, 0, 64U);
    make_vertex(vertices + 32U, 4, 0, 2, 64, 32, 96U);
    make_vertex(vertices + 48U, -4, 0, 2, 0, 32, 128U);

    blobs.display_list = commands;
    blobs.display_list_size = GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES;
    blobs.vertices = vertices;
    blobs.vertices_size = GE_BLOTTER_MODEL_VERTEX_BYTES;
    blobs.matrix = ge_test_matrix_identity_be;
    blobs.matrix_size = GE_BLOTTER_MODEL_MATRIX_BYTES;
    return blobs;
}

static void test_builds_processed_triangles(void)
{
    uint8_t commands[GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES];
    uint8_t vertices[GE_BLOTTER_MODEL_VERTEX_BYTES];
    GeBlotterModelBlobs blobs = make_fixture(
        commands, vertices, GE_BLOTTER_MODEL_TEXTURE_ID);
    GeBlotterModel model;

    assert(ge_blotter_model_build(&blobs, &model) == GE_BLOTTER_MODEL_OK);
    assert(model.status == GE_BLOTTER_MODEL_OK);
    assert(model.material.texture_id == GE_BLOTTER_MODEL_TEXTURE_ID);
    assert(model.triangle_count == GE_BLOTTER_MODEL_TRIANGLE_COUNT);
    assert(model.vertex_count == GE_BLOTTER_MODEL_VERTEX_COUNT);
    assert(model.pipeline.status == GE_GBI_PIPELINE_OK);
    assert(model.pipeline.draw_calls == 2U);
    assert(model.pipeline.triangles == 2U);
    assert(model.triangles[0].vertices[0].source.x == -4);
    assert(model.triangles[0].vertices[1].source.x == 4);
    assert(model.triangles[1].vertices[2].source.z == 2);
    assert(model.triangles[0].vertices[0].processed.object[0] == -4.0f);
    assert(model.triangles[0].vertices[0].processed.eye[0] == -4.0f);
    assert(model.triangles[0].vertices[1].processed.texture[0] == 64.0f);
    assert(model.triangles[1].vertices[2].processed.rgba[0] == 128U);
}

static void test_rejects_bad_inputs(void)
{
    uint8_t commands[GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES];
    uint8_t vertices[GE_BLOTTER_MODEL_VERTEX_BYTES];
    GeBlotterModelBlobs blobs = make_fixture(
        commands, vertices, GE_BLOTTER_MODEL_TEXTURE_ID);
    GeBlotterModel model;

    assert(ge_blotter_model_build(NULL, &model)
           == GE_BLOTTER_MODEL_INVALID_ARGUMENT);
    assert(ge_blotter_model_build(&blobs, NULL)
           == GE_BLOTTER_MODEL_INVALID_ARGUMENT);
    --blobs.vertices_size;
    assert(ge_blotter_model_build(&blobs, &model)
           == GE_BLOTTER_MODEL_INVALID_BLOB_LAYOUT);
    assert(strcmp(ge_blotter_model_status_name(model.status),
                  "invalid blob layout") == 0);
}

static void test_rejects_wrong_material(void)
{
    uint8_t commands[GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES];
    uint8_t vertices[GE_BLOTTER_MODEL_VERTEX_BYTES];
    GeBlotterModelBlobs blobs = make_fixture(commands, vertices, 181U);
    GeBlotterModel model;

    assert(ge_blotter_model_build(&blobs, &model)
           == GE_BLOTTER_MODEL_UNEXPECTED_MATERIAL);
    assert(model.material.texture_id == 181U);
}

int main(void)
{
    test_builds_processed_triangles();
    test_rejects_bad_inputs();
    test_rejects_wrong_material();
    puts("GoldenEye portable blotter model loader tests passed");
    return 0;
}
