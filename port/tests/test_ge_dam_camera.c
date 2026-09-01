#include "ge_dam_camera.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_EPSILON 0.001f

void ge_test_original_camera_matrices(float view[4][4],
    float projection[4][4], uint16_t *perspective_normalize);

static int nearly_equal(float left, float right)
{
    return fabsf(left - right) <= TEST_EPSILON;
}

static GeDamRoomWorldVertex world_vertex(float x, float y, float z,
                                         float s, float t, uint8_t red)
{
    GeDamRoomWorldVertex vertex;

    memset(&vertex, 0, sizeof(vertex));
    vertex.world[0] = x;
    vertex.world[1] = y;
    vertex.world[2] = z;
    vertex.processed.texture[0] = s;
    vertex.processed.texture[1] = t;
    vertex.processed.rgba[0] = red;
    vertex.processed.rgba[1] = 80U;
    vertex.processed.rgba[2] = 40U;
    vertex.processed.rgba[3] = 255U;
    return vertex;
}

static GeDamCamera default_camera(void)
{
    GeDamCameraConfig config = ge_dam_camera_default_config();
    GeDamCamera camera;

    config.near_distance = 1.0f;
    config.far_distance = 100.0f;
    assert(ge_dam_camera_prepare(&config, &camera) == GE_DAM_CAMERA_OK);
    return camera;
}

static void test_defaults_and_center_projection(void)
{
    GeDamCameraConfig config = ge_dam_camera_default_config();
    GeDamCamera camera;
    GeDamRoomWorldVertex input[3] = {
        world_vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f, 10U),
        world_vertex(1.0f, -1.0f, -10.0f, 64.0f, 0.0f, 20U),
        world_vertex(0.0f, 1.0f, -10.0f, 32.0f, 64.0f, 30U),
    };
    GeDamCameraVertex output[3];
    GeDamCameraStorage storage = {output, 3U};
    GeDamCameraResult result;

    assert(nearly_equal(config.vertical_fov_radians,
                        GE_DAM_CAMERA_VERTICAL_FOV_RADIANS));
    assert(nearly_equal(config.aspect, GE_DAM_CAMERA_ASPECT_5_3));
    assert(nearly_equal(config.viewport_width / config.viewport_height,
                        GE_DAM_CAMERA_ASPECT_5_3));
    config.far_distance = 100.0f;
    assert(ge_dam_camera_prepare(&config, &camera) == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&camera, input, 3U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.input_triangle_count == 1U);
    assert(result.visible_input_triangle_count == 1U);
    assert(result.output_triangle_count == 1U);
    assert(result.vertex_count == 3U);
    assert(output[0].screen[0] < 200.0f);
    assert(output[1].screen[0] > 200.0f);
    assert(output[2].screen[1] < 120.0f);
    assert(nearly_equal((output[0].screen[0] + output[1].screen[0]) * 0.5f,
                        200.0f));
    assert(nearly_equal(output[0].inverse_depth, 0.1f));
    assert(nearly_equal(output[1].texture_over_depth[0], 6.4f));
    assert(output[2].rgba[0] == 30U);
    assert(output[0].screen[2] >= 0.0f && output[0].screen[2] <= 1.0f);
}

static void test_exact_fov_edges(void)
{
    GeDamCamera camera = default_camera();
    const float vertical = 10.0f * tanf(
        GE_DAM_CAMERA_VERTICAL_FOV_RADIANS * 0.5f);
    const float horizontal = vertical * GE_DAM_CAMERA_ASPECT_5_3;
    GeDamRoomWorldVertex input[6] = {
        world_vertex(horizontal, 0.0f, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, vertical, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 0.0f, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(-horizontal, 0.0f, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, -vertical, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 0.0f, -10.0f, 0.0f, 0.0f, 0U),
    };
    GeDamCameraVertex output[6];
    GeDamCameraStorage storage = {output, 6U};
    GeDamCameraResult result;

    assert(ge_dam_camera_project(&camera, input, 6U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.vertex_count == 6U);
    assert(nearly_equal(output[0].screen[0], 400.0f));
    assert(nearly_equal(output[1].screen[1], 0.0f));
    assert(nearly_equal(output[3].screen[0], 0.0f));
    assert(nearly_equal(output[4].screen[1], 240.0f));
}

static void test_near_and_side_clipping(void)
{
    GeDamCamera camera = default_camera();
    GeDamRoomWorldVertex near_input[3] = {
        world_vertex(-0.4f, -0.2f, -2.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.4f, -0.2f, -2.0f, 64.0f, 0.0f, 64U),
        world_vertex(0.0f, 0.4f, -0.5f, 32.0f, 64.0f, 255U),
    };
    GeDamRoomWorldVertex side_input[3] = {
        world_vertex(0.0f, -1.0f, -4.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 1.0f, -4.0f, 0.0f, 64.0f, 0U),
        world_vertex(10.0f, 0.0f, -4.0f, 64.0f, 32.0f, 0U),
    };
    GeDamCameraVertex output[6];
    GeDamCameraStorage storage = {output, 6U};
    GeDamCameraResult result;
    size_t index;

    assert(ge_dam_camera_project(&camera, near_input, 3U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.output_triangle_count == 2U);
    assert(result.vertex_count == 6U);
    for (index = 0U; index < result.vertex_count; ++index) {
        assert(output[index].camera[2] >= 1.0f - TEST_EPSILON);
        assert(output[index].screen[0] >= -TEST_EPSILON
               && output[index].screen[0] <= 400.0f + TEST_EPSILON);
        assert(output[index].screen[1] >= -TEST_EPSILON
               && output[index].screen[1] <= 240.0f + TEST_EPSILON);
        assert(nearly_equal(output[index].texture_over_depth[0],
            output[index].texture[0] * output[index].inverse_depth));
    }

    assert(ge_dam_camera_project(&camera, side_input, 3U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.output_triangle_count == 2U);
    for (index = 0U; index < result.vertex_count; ++index) {
        assert(output[index].screen[0] <= 400.0f + TEST_EPSILON);
    }
}

static void test_far_rejection_and_atomic_capacity(void)
{
    GeDamCamera camera = default_camera();
    GeDamRoomWorldVertex rejected[3] = {
        world_vertex(-1.0f, -1.0f, -101.0f, 0.0f, 0.0f, 0U),
        world_vertex(1.0f, -1.0f, -101.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 1.0f, -101.0f, 0.0f, 0.0f, 0U),
    };
    GeDamRoomWorldVertex clipped[3] = {
        world_vertex(-0.4f, -0.2f, -2.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.4f, -0.2f, -2.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 0.4f, -0.5f, 0.0f, 0.0f, 0U),
    };
    GeDamCameraVertex output[5];
    GeDamCameraStorage small = {output, 5U};
    GeDamCameraResult result;

    memset(output, 0x5a, sizeof(output));
    assert(ge_dam_camera_project(&camera, rejected, 3U, NULL, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.required_vertex_count == 0U);
    assert(result.vertex_count == 0U);

    assert(ge_dam_camera_project(&camera, clipped, 3U, NULL, &result)
           == GE_DAM_CAMERA_CAPACITY_EXCEEDED);
    assert(result.required_vertex_count == 6U);
    assert(result.vertex_count == 0U);
    assert(ge_dam_camera_project(&camera, clipped, 3U, &small, &result)
           == GE_DAM_CAMERA_CAPACITY_EXCEEDED);
    assert(result.required_vertex_count == 6U);
    assert(result.vertex_count == 0U);
    assert(((const unsigned char *)output)[0] == 0x5aU);
}

static void test_arbitrary_pose(void)
{
    GeDamCameraConfig config = ge_dam_camera_default_config();
    GeDamCamera camera;
    GeDamRoomWorldVertex input[3] = {
        world_vertex(12.0f, 2.0f, 4.0f, 0.0f, 0.0f, 0U),
        world_vertex(12.0f, 2.0f, 6.0f, 0.0f, 0.0f, 0U),
        world_vertex(12.0f, 4.0f, 5.0f, 0.0f, 0.0f, 0U),
    };
    GeDamCameraVertex output[3];
    GeDamCameraStorage storage = {output, 3U};
    GeDamCameraResult result;

    config.eye[0] = 2.0f;
    config.eye[1] = 2.0f;
    config.eye[2] = 5.0f;
    config.forward[0] = 1.0f;
    config.forward[1] = 0.0f;
    config.forward[2] = 0.0f;
    config.up[0] = 0.0f;
    config.up[1] = 1.0f;
    config.up[2] = 0.0f;
    config.far_distance = 100.0f;
    assert(ge_dam_camera_prepare(&config, &camera) == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&camera, input, 3U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.vertex_count == 3U);
    assert(nearly_equal(output[0].camera[2], 10.0f));
    assert(output[0].screen[0] < 200.0f);
    assert(output[1].screen[0] > 200.0f);
    assert(output[2].screen[1] < 120.0f);
}

static void test_original_matrix_entry_point(void)
{
    const float view[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    const float projection[4][4] = {
        {1.03923048f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.73205081f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.02020202f, -1.0f},
        {0.0f, 0.0f, -2.02020202f, 0.0f},
    };
    GeDamRoomWorldVertex input[3] = {
        world_vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f, 0U),
        world_vertex(1.0f, -1.0f, -10.0f, 64.0f, 0.0f, 0U),
        world_vertex(0.0f, 1.0f, -10.0f, 32.0f, 64.0f, 0U),
    };
    GeDamCamera camera;
    GeDamCameraVertex output[3];
    GeDamCameraStorage storage = {output, 3U};
    GeDamCameraResult result;

    assert(ge_dam_camera_prepare_matrices(view, projection, 0.0f, 0.0f,
        400.0f, 240.0f, &camera) == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&camera, input, 3U, &storage, &result)
           == GE_DAM_CAMERA_OK);
    assert(result.vertex_count == 3U);
    assert(nearly_equal(output[0].clip[3], 10.0f));
    assert(nearly_equal(output[0].camera[2], 10.0f));
    assert(nearly_equal(output[0].inverse_depth, 0.1f));
    assert(output[0].screen[0] < 200.0f);
    assert(output[1].screen[0] > 200.0f);
    assert(output[2].screen[1] < 120.0f);
}

static void test_original_producers_and_rsp_viewport(void)
{
    float view[4][4];
    float projection[4][4];
    uint16_t perspective_normalize = 0U;
    /* viSetupCurrentPlayerView values for a 320x200 rectangle at 40,20. */
    const int16_t viewport_scale[4] = {640, 400, 511, 0};
    const int16_t viewport_translation[4] = {800, 480, 511, 0};
    GeDamRoomWorldVertex input[3] = {
        world_vertex(9.0f, 19.0f, 40.0f, 0.0f, 0.0f, 10U),
        world_vertex(11.0f, 19.0f, 40.0f, 64.0f, 0.0f, 20U),
        world_vertex(10.0f, 21.0f, 40.0f, 32.0f, 64.0f, 30U),
    };
    GeDamCamera camera;
    GeDamCameraVertex output[3];
    GeDamCameraStorage storage = {output, 3U};
    GeDamCameraResult result;

    ge_test_original_camera_matrices(
        view, projection, &perspective_normalize);
    assert(perspective_normalize != 0U);
    assert(ge_dam_camera_prepare_rsp_viewport(
        view, projection, viewport_scale, viewport_translation, &camera)
        == GE_DAM_CAMERA_OK);
    assert(nearly_equal(camera.viewport_x, 40.0f));
    assert(nearly_equal(camera.viewport_y, 20.0f));
    assert(nearly_equal(camera.viewport_width, 320.0f));
    assert(nearly_equal(camera.viewport_height, 200.0f));
    assert(ge_dam_camera_project(&camera, input, 3U, &storage, &result)
        == GE_DAM_CAMERA_OK);
    assert(result.vertex_count == 3U);
    assert(nearly_equal(output[0].clip[3], 10.0f));
    assert(nearly_equal(output[0].camera[2], 10.0f));
    assert(output[0].screen[0] > 200.0f);
    assert(output[1].screen[0] < 200.0f);
    /* RSP Vp uses a positive Y scale, unlike the convenience screen mapping. */
    assert(output[2].screen[1] > 120.0f);
    assert(output[0].screen[0] >= 40.0f && output[0].screen[0] <= 360.0f);
    assert(output[2].screen[1] >= 20.0f && output[2].screen[1] <= 220.0f);

    {
        int16_t invalid_scale[4] = {0, 400, 511, 0};

        assert(ge_dam_camera_prepare_rsp_viewport(
            view, projection, NULL, viewport_translation, &camera)
            == GE_DAM_CAMERA_INVALID_ARGUMENT);
        assert(ge_dam_camera_prepare_rsp_viewport(
            view, projection, invalid_scale, viewport_translation, &camera)
            == GE_DAM_CAMERA_INVALID_CONFIG);
    }
}

static void test_authored_to_runtime_matrix_boundary(void)
{
    GeDamCameraConfig config = ge_dam_camera_default_config();
    GeDamCamera runtime_camera;
    GeDamCamera authored_camera;
    GeDamRoomWorldVertex runtime_input[3] = {
        world_vertex(-4.0f, -4.0f, -40.0f, 0.0f, 0.0f, 10U),
        world_vertex(4.0f, -4.0f, -40.0f, 64.0f, 0.0f, 20U),
        world_vertex(0.0f, 4.0f, -40.0f, 32.0f, 64.0f, 30U),
    };
    GeDamRoomWorldVertex authored_input[3] = {
        world_vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f, 10U),
        world_vertex(1.0f, -1.0f, -10.0f, 64.0f, 0.0f, 20U),
        world_vertex(0.0f, 1.0f, -10.0f, 32.0f, 64.0f, 30U),
    };
    GeDamCameraVertex runtime_output[3];
    GeDamCameraVertex authored_output[3];
    GeDamCameraStorage runtime_storage = {runtime_output, 3U};
    GeDamCameraStorage authored_storage = {authored_output, 3U};
    GeDamCameraResult runtime_result;
    GeDamCameraResult authored_result;
    size_t index;

    config.far_distance = 100.0f;
    assert(ge_dam_camera_prepare(&config, &runtime_camera)
        == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_scale_world(&runtime_camera, 4.0f,
        &authored_camera) == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&runtime_camera, runtime_input, 3U,
        &runtime_storage, &runtime_result) == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project(&authored_camera, authored_input, 3U,
        &authored_storage, &authored_result) == GE_DAM_CAMERA_OK);
    assert(runtime_result.vertex_count == authored_result.vertex_count);
    for (index = 0U; index < runtime_result.vertex_count; ++index) {
        assert(nearly_equal(runtime_output[index].screen[0],
                            authored_output[index].screen[0]));
        assert(nearly_equal(runtime_output[index].screen[1],
                            authored_output[index].screen[1]));
        assert(nearly_equal(runtime_output[index].screen[2],
                            authored_output[index].screen[2]));
    }
    assert(ge_dam_camera_scale_world(&runtime_camera, 0.0f,
        &authored_camera) == GE_DAM_CAMERA_INVALID_CONFIG);
}

static void test_material_batch_remap_after_clipping(void)
{
    GeDamCamera camera = default_camera();
    GeDamRoomWorldVertex input[6] = {
        world_vertex(-1.0f, -1.0f, -101.0f, 0.0f, 0.0f, 1U),
        world_vertex(1.0f, -1.0f, -101.0f, 0.0f, 0.0f, 2U),
        world_vertex(0.0f, 1.0f, -101.0f, 0.0f, 0.0f, 3U),
        world_vertex(-0.4f, -0.2f, -2.0f, 0.0f, 0.0f, 4U),
        world_vertex(0.4f, -0.2f, -2.0f, 64.0f, 0.0f, 5U),
        world_vertex(0.0f, 0.4f, -0.5f, 32.0f, 64.0f, 6U),
    };
    GeDamRoomDrawBatch batches[2];
    GeDamCameraVertex output[6];
    GeDamCameraVertex bounded_output[6];
    GeDamCameraBatch output_batches[1];
    GeDamCameraBatch bounded_batches[1];
    GeDamCameraSceneStorage storage = {
        output, 6U, output_batches, 1U,
    };
    GeDamCameraSceneResult result;
    GeDamCameraSceneResult bounded_result;

    memset(batches, 0, sizeof(batches));
    batches[0].first_vertex = 0U;
    batches[0].vertex_count = 3U;
    batches[1].first_vertex = 3U;
    batches[1].vertex_count = 3U;
    assert(ge_dam_camera_project_batches(&camera, input, 6U, batches, 2U,
        NULL, &result) == GE_DAM_CAMERA_CAPACITY_EXCEEDED);
    assert(result.input_triangle_count == 2U);
    assert(result.visible_input_triangle_count == 1U);
    assert(result.output_triangle_count == 2U);
    assert(result.required_vertex_count == 6U);
    assert(result.required_batch_count == 1U);
    assert(result.clip_triangle_evaluations == 2U);
    assert(ge_dam_camera_project_batches(&camera, input, 6U, batches, 2U,
        &storage, &result) == GE_DAM_CAMERA_OK);
    assert(result.vertex_count == 6U);
    assert(result.batch_count == 1U);
    assert(result.clip_triangle_evaluations == 4U);
    assert(output_batches[0].source_batch == 1U);
    assert(output_batches[0].first_vertex == 0U);
    assert(output_batches[0].vertex_count == 6U);
    storage = (GeDamCameraSceneStorage){
        bounded_output, 6U, bounded_batches, 1U,
    };
    assert(ge_dam_camera_project_batches_bounded(
        &camera, input, 6U, batches, 2U, &storage, &bounded_result)
        == GE_DAM_CAMERA_OK);
    assert(bounded_result.vertex_count == result.vertex_count);
    assert(bounded_result.batch_count == result.batch_count);
    assert(bounded_result.visible_input_triangle_count
           == result.visible_input_triangle_count);
    assert(bounded_result.clip_triangle_evaluations == 2U);
    assert(memcmp(bounded_output, output, sizeof(output)) == 0);
    assert(memcmp(bounded_batches, output_batches,
                  sizeof(output_batches)) == 0);
}

static void test_invalid_input(void)
{
    GeDamCameraConfig config = ge_dam_camera_default_config();
    GeDamCamera camera;
    GeDamCameraResult result;
    GeDamRoomWorldVertex triangle[3] = {
        world_vertex(-1.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0U),
        world_vertex(1.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0U),
        world_vertex(0.0f, 1.0f, -3.0f, 0.0f, 0.0f, 0U),
    };

    config.up[0] = 0.0f;
    config.up[1] = 0.0f;
    config.up[2] = -2.0f;
    assert(ge_dam_camera_prepare(&config, &camera)
           == GE_DAM_CAMERA_INVALID_CONFIG);
    camera = default_camera();
    assert(ge_dam_camera_project(&camera, triangle, 2U, NULL, &result)
           == GE_DAM_CAMERA_INVALID_ARGUMENT);
    triangle[1].world[0] = NAN;
    assert(ge_dam_camera_project(&camera, triangle, 3U, NULL, &result)
           == GE_DAM_CAMERA_NONFINITE_INPUT);
    assert(strcmp(ge_dam_camera_status_name(GE_DAM_CAMERA_OK), "ok") == 0);
}

int main(void)
{
    test_defaults_and_center_projection();
    test_exact_fov_edges();
    test_near_and_side_clipping();
    test_far_rejection_and_atomic_capacity();
    test_arbitrary_pose();
    test_original_matrix_entry_point();
    test_original_producers_and_rsp_viewport();
    test_authored_to_runtime_matrix_boundary();
    test_material_batch_remap_after_clipping();
    test_invalid_input();
    puts("ge_dam_camera tests passed");
    return 0;
}
