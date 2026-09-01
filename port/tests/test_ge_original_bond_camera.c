#include "ge_dam_camera.h"
#include "ge_original_bond_camera.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_EPSILON 0.002f

static int nearly_equal(float left, float right)
{
    return fabsf(left - right) <= TEST_EPSILON;
}

static GeDamRoomWorldVertex world_vertex(float x, float y, float z)
{
    GeDamRoomWorldVertex vertex;

    memset(&vertex, 0, sizeof(vertex));
    vertex.world[0] = x;
    vertex.world[1] = y;
    vertex.world[2] = z;
    vertex.processed.rgba[0] = 255U;
    vertex.processed.rgba[1] = 255U;
    vertex.processed.rgba[2] = 255U;
    vertex.processed.rgba[3] = 255U;
    return vertex;
}

static GeOriginalBondCameraConfig valid_config(void)
{
    GeOriginalBondCameraConfig config;

    memset(&config, 0, sizeof(config));
    config.camera_position[0] = 12.0f;
    config.camera_position[1] = 24.0f;
    config.camera_position[2] = 36.0f;
    config.camera_look_direction[2] = 1.0f;
    config.camera_up[1] = 1.0f;
    config.room_origin[0] = 2.0f;
    config.room_origin[1] = 4.0f;
    config.room_origin[2] = 6.0f;
    config.room_position_scale = 0.25f;
    config.camera_local_scale = 1.0f;
    config.visibility_scale = 0.5f;
    config.viewport_scale[0] = 640;
    config.viewport_scale[1] = 400;
    config.viewport_scale[2] = 511;
    config.viewport_translation[0] = 800;
    config.viewport_translation[1] = 480;
    config.viewport_translation[2] = 511;
    config.room = 135U;
    assert(ge_original_bond_camera_set_perspective(
        &config, 60.0f, 1.6f, 1.0f, 1000.0f)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    return config;
}

static void test_original_camera_path_and_render_handoff(void)
{
    GeOriginalBondCameraConfig config = valid_config();
    GeOriginalBondCameraResult original;
    GeDamCamera camera;
    GeDamRoomWorldVertex input[3] = {
        world_vertex(11.0f, 23.0f, 46.0f),
        world_vertex(13.0f, 23.0f, 46.0f),
        world_vertex(12.0f, 25.0f, 46.0f),
    };
    GeDamCameraVertex output[3];
    GeDamCameraStorage storage = {output, 3U};
    GeDamCameraResult projected;

    assert(ge_original_bond_camera_run(&config, &original)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(original.room == 135U);
    assert(original.matrix_allocations == 5U);
    assert(original.light_allocations == 2U);
    assert(original.room_updates == 1U);
    assert(original.frustum_updates == 1U);
    assert(nearly_equal(original.room_origin[0], 2.0f));
    assert(nearly_equal(original.room_origin[1], 4.0f));
    assert(nearly_equal(original.room_origin[2], 6.0f));
    assert(nearly_equal(original.scaled_room_origin[0], 0.5f));
    assert(nearly_equal(original.scaled_room_origin[1], 1.0f));
    assert(nearly_equal(original.scaled_room_origin[2], 1.5f));
    assert(nearly_equal(original.view_to_world[3][0], 12.0f));
    assert(nearly_equal(original.view_to_world[3][1], 24.0f));
    assert(nearly_equal(original.view_to_world[3][2], 36.0f));
    assert(memcmp(original.projection, config.projection,
                  sizeof(original.projection)) == 0);
    assert(memcmp(original.viewport_scale, config.viewport_scale,
                  sizeof(original.viewport_scale)) == 0);
    assert(memcmp(original.viewport_translation, config.viewport_translation,
                  sizeof(original.viewport_translation)) == 0);

    assert(ge_dam_camera_prepare_rsp_viewport(
        original.view, original.projection,
        original.viewport_scale, original.viewport_translation, &camera)
        == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&camera, input, 3U, &storage, &projected)
        == GE_DAM_CAMERA_OK);
    assert(projected.vertex_count == 3U);
    assert(nearly_equal(output[0].clip[3], 10.0f));
    assert(output[0].screen[0] > 200.0f);
    assert(output[1].screen[0] < 200.0f);
    assert(output[2].screen[1] > 120.0f);
}

static void test_invalid_contract(void)
{
    GeOriginalBondCameraConfig config = valid_config();
    GeOriginalBondCameraResult result;

    assert(ge_original_bond_camera_run(NULL, &result)
        == GE_ORIGINAL_BOND_CAMERA_INVALID_ARGUMENT);
    config.camera_look_direction[2] = 0.0f;
    /* The original function requires nonzero, nonparallel look/up vectors.
     * Reject this at the adapter boundary rather than feeding it NaNs. */
    assert(ge_original_bond_camera_run(&config, &result)
        == GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG);
    assert(strcmp(ge_original_bond_camera_status_name(
        GE_ORIGINAL_BOND_CAMERA_OK), "ok") == 0);
}

int main(void)
{
    test_original_camera_path_and_render_handoff();
    test_invalid_contract();
    puts("original bondview camera slice tests passed");
    return 0;
}
