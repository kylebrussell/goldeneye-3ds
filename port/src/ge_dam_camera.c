#include "ge_dam_camera.h"

#include <math.h>
#include <string.h>

#define GE_DAM_CAMERA_VECTOR_EPSILON 0.000001f

typedef struct GeDamCameraClipVertex {
    float world[3];
    float camera[3];
    float eye[4];
    float clip[4];
    float texture[2];
    float rgba[4];
} GeDamCameraClipVertex;

typedef enum GeDamCameraPlane {
    GE_DAM_CAMERA_PLANE_POSITIVE_W = 0,
    GE_DAM_CAMERA_PLANE_LEFT,
    GE_DAM_CAMERA_PLANE_RIGHT,
    GE_DAM_CAMERA_PLANE_BOTTOM,
    GE_DAM_CAMERA_PLANE_TOP,
    GE_DAM_CAMERA_PLANE_NEAR,
    GE_DAM_CAMERA_PLANE_FAR,
    GE_DAM_CAMERA_PLANE_COUNT
} GeDamCameraPlane;

static float ge_dam_camera_dot(const float left[3], const float right[3])
{
    return left[0] * right[0] + left[1] * right[1]
        + left[2] * right[2];
}

static void ge_dam_camera_cross(float output[3], const float left[3],
                                const float right[3])
{
    output[0] = left[1] * right[2] - left[2] * right[1];
    output[1] = left[2] * right[0] - left[0] * right[2];
    output[2] = left[0] * right[1] - left[1] * right[0];
}

static int ge_dam_camera_normalize(float output[3], const float input[3])
{
    const float length_squared = ge_dam_camera_dot(input, input);
    float inverse_length;

    if (!isfinite(length_squared)
            || length_squared <= GE_DAM_CAMERA_VECTOR_EPSILON
                * GE_DAM_CAMERA_VECTOR_EPSILON) {
        return 0;
    }
    inverse_length = 1.0f / sqrtf(length_squared);
    output[0] = input[0] * inverse_length;
    output[1] = input[1] * inverse_length;
    output[2] = input[2] * inverse_length;
    return 1;
}

static int ge_dam_camera_vector_finite(const float vector[3])
{
    return isfinite(vector[0]) && isfinite(vector[1])
        && isfinite(vector[2]);
}

GeDamCameraConfig ge_dam_camera_default_config(void)
{
    GeDamCameraConfig config;

    memset(&config, 0, sizeof(config));
    config.forward[2] = -1.0f;
    config.up[1] = 1.0f;
    config.vertical_fov_radians = GE_DAM_CAMERA_VERTICAL_FOV_RADIANS;
    config.aspect = GE_DAM_CAMERA_ASPECT_5_3;
    config.near_distance = 1.0f;
    config.far_distance = 10000.0f;
    config.viewport_width = 400.0f;
    config.viewport_height = 240.0f;
    return config;
}

GeDamCameraStatus ge_dam_camera_prepare(const GeDamCameraConfig *config,
                                        GeDamCamera *camera)
{
    float right[3];
    float actual_up[3];
    float forward[3];
    float tangent;

    if (config == NULL || camera == NULL) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    memset(camera, 0, sizeof(*camera));
    if (!ge_dam_camera_vector_finite(config->eye)
            || !ge_dam_camera_vector_finite(config->forward)
            || !ge_dam_camera_vector_finite(config->up)
            || !isfinite(config->vertical_fov_radians)
            || !isfinite(config->aspect)
            || !isfinite(config->near_distance)
            || !isfinite(config->far_distance)
            || !isfinite(config->viewport_x)
            || !isfinite(config->viewport_y)
            || !isfinite(config->viewport_width)
            || !isfinite(config->viewport_height)
            || config->vertical_fov_radians <= 0.0f
            || config->vertical_fov_radians >= 3.1415926535897932f
            || config->aspect <= 0.0f || config->near_distance <= 0.0f
            || config->far_distance <= config->near_distance
            || config->viewport_width <= 0.0f
            || config->viewport_height <= 0.0f
            || !ge_dam_camera_normalize(forward, config->forward)) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    ge_dam_camera_cross(right, forward, config->up);
    if (!ge_dam_camera_normalize(right, right)) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    ge_dam_camera_cross(actual_up, right, forward);
    if (!ge_dam_camera_normalize(actual_up, actual_up)) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    tangent = tanf(config->vertical_fov_radians * 0.5f);
    if (!isfinite(tangent) || tangent <= 0.0f) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }

    {
        float view[4][4] = {{0.0f}};
        float projection[4][4] = {{0.0f}};
        const float negative_forward[3] = {
            -forward[0], -forward[1], -forward[2]
        };
        const float cotangent = 1.0f / tangent;

        view[0][0] = right[0];
        view[1][0] = right[1];
        view[2][0] = right[2];
        view[3][0] = -ge_dam_camera_dot(config->eye, right);
        view[0][1] = actual_up[0];
        view[1][1] = actual_up[1];
        view[2][1] = actual_up[2];
        view[3][1] = -ge_dam_camera_dot(config->eye, actual_up);
        view[0][2] = negative_forward[0];
        view[1][2] = negative_forward[1];
        view[2][2] = negative_forward[2];
        view[3][2] = -ge_dam_camera_dot(config->eye, negative_forward);
        view[3][3] = 1.0f;

        projection[0][0] = cotangent / config->aspect;
        projection[1][1] = cotangent;
        projection[2][2] = (config->near_distance + config->far_distance)
            / (config->near_distance - config->far_distance);
        projection[2][3] = -1.0f;
        projection[3][2] = (2.0f * config->near_distance
            * config->far_distance)
            / (config->near_distance - config->far_distance);
        return ge_dam_camera_prepare_matrices(
            view, projection, config->viewport_x, config->viewport_y,
            config->viewport_width, config->viewport_height, camera);
    }
}

GeDamCameraStatus ge_dam_camera_prepare_matrices(
    const float view[4][4],
    const float projection[4][4],
    float viewport_x,
    float viewport_y,
    float viewport_width,
    float viewport_height,
    GeDamCamera *camera)
{
    size_t row;
    size_t column;

    if (view == NULL || projection == NULL || camera == NULL) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    memset(camera, 0, sizeof(*camera));
    if (!isfinite(viewport_x) || !isfinite(viewport_y)
            || !isfinite(viewport_width) || !isfinite(viewport_height)
            || viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            if (!isfinite(view[row][column])
                    || !isfinite(projection[row][column])) {
                return GE_DAM_CAMERA_INVALID_CONFIG;
            }
        }
    }
    memcpy(camera->view, view, sizeof(camera->view));
    memcpy(camera->projection, projection, sizeof(camera->projection));
    camera->viewport_x = viewport_x;
    camera->viewport_y = viewport_y;
    camera->viewport_width = viewport_width;
    camera->viewport_height = viewport_height;
    camera->viewport_scale[0] = viewport_width * 0.5f;
    camera->viewport_scale[1] = viewport_height * -0.5f;
    camera->viewport_translation[0] = viewport_x + viewport_width * 0.5f;
    camera->viewport_translation[1] = viewport_y + viewport_height * 0.5f;
    return GE_DAM_CAMERA_OK;
}

GeDamCameraStatus ge_dam_camera_prepare_rsp_viewport(
    const float view[4][4],
    const float projection[4][4],
    const int16_t viewport_scale[4],
    const int16_t viewport_translation[4],
    GeDamCamera *camera)
{
    float endpoints[2][2];
    GeDamCameraStatus status;
    size_t axis;

    if (viewport_scale == NULL || viewport_translation == NULL) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    if (viewport_scale[0] == 0 || viewport_scale[1] == 0) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    status = ge_dam_camera_prepare_matrices(
        view, projection, 0.0f, 0.0f, 1.0f, 1.0f, camera);
    if (status != GE_DAM_CAMERA_OK) {
        return status;
    }
    for (axis = 0U; axis < 2U; ++axis) {
        camera->viewport_scale[axis] = (float)viewport_scale[axis] * 0.25f;
        camera->viewport_translation[axis] =
            (float)viewport_translation[axis] * 0.25f;
        endpoints[axis][0] = camera->viewport_translation[axis]
            - camera->viewport_scale[axis];
        endpoints[axis][1] = camera->viewport_translation[axis]
            + camera->viewport_scale[axis];
    }
    camera->viewport_x = fminf(endpoints[0][0], endpoints[0][1]);
    camera->viewport_y = fminf(endpoints[1][0], endpoints[1][1]);
    camera->viewport_width = fabsf(endpoints[0][1] - endpoints[0][0]);
    camera->viewport_height = fabsf(endpoints[1][1] - endpoints[1][0]);
    return GE_DAM_CAMERA_OK;
}

GeDamCameraStatus ge_dam_camera_scale_world(
    const GeDamCamera *camera,
    float authored_to_runtime_scale,
    GeDamCamera *scaled_camera)
{
    size_t row;
    size_t column;

    if (camera == NULL || scaled_camera == NULL) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    if (!isfinite(authored_to_runtime_scale)
            || authored_to_runtime_scale <= 0.0f) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    *scaled_camera = *camera;
    for (row = 0U; row < 3U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            scaled_camera->view[row][column] *= authored_to_runtime_scale;
        }
    }
    return GE_DAM_CAMERA_OK;
}

static int ge_dam_camera_input_finite(const GeDamRoomWorldVertex *vertex)
{
    size_t index;

    for (index = 0U; index < 3U; ++index) {
        if (!isfinite(vertex->world[index])) {
            return 0;
        }
    }
    return isfinite(vertex->processed.texture[0])
        && isfinite(vertex->processed.texture[1]);
}

static void ge_dam_camera_transform(float output[4], const float input[4],
                                    const float matrix[4][4])
{
    size_t column;

    for (column = 0U; column < 4U; ++column) {
        size_t row;

        output[column] = 0.0f;
        for (row = 0U; row < 4U; ++row) {
            output[column] += input[row] * matrix[row][column];
        }
    }
}

static void ge_dam_camera_import_vertex(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input,
    GeDamCameraClipVertex *output)
{
    float world[4];
    size_t channel;

    memcpy(output->world, input->world, sizeof(output->world));
    world[0] = input->world[0];
    world[1] = input->world[1];
    world[2] = input->world[2];
    world[3] = 1.0f;
    ge_dam_camera_transform(output->eye, world, camera->view);
    ge_dam_camera_transform(output->clip, output->eye, camera->projection);
    output->camera[0] = output->eye[0];
    output->camera[1] = output->eye[1];
    output->camera[2] = -output->eye[2];
    memcpy(output->texture, input->processed.texture,
           sizeof(output->texture));
    for (channel = 0U; channel < 4U; ++channel) {
        output->rgba[channel] = input->processed.rgba[channel];
    }
}

static float ge_dam_camera_plane_distance(
    const GeDamCamera *camera,
    const GeDamCameraClipVertex *vertex,
    GeDamCameraPlane plane)
{
    const float x = vertex->clip[0];
    const float y = vertex->clip[1];
    const float z = vertex->clip[2];
    const float w = vertex->clip[3];

    (void)camera;

    switch (plane) {
    case GE_DAM_CAMERA_PLANE_POSITIVE_W:
        return w - GE_DAM_CAMERA_VECTOR_EPSILON;
    case GE_DAM_CAMERA_PLANE_LEFT:
        return w + x;
    case GE_DAM_CAMERA_PLANE_RIGHT:
        return w - x;
    case GE_DAM_CAMERA_PLANE_BOTTOM:
        return w + y;
    case GE_DAM_CAMERA_PLANE_TOP:
        return w - y;
    case GE_DAM_CAMERA_PLANE_NEAR:
        return w + z;
    case GE_DAM_CAMERA_PLANE_FAR:
        return w - z;
    case GE_DAM_CAMERA_PLANE_COUNT:
        break;
    }
    return -1.0f;
}

static void ge_dam_camera_interpolate(GeDamCameraClipVertex *output,
                                      const GeDamCameraClipVertex *start,
                                      const GeDamCameraClipVertex *end,
                                      float amount)
{
    size_t index;

    for (index = 0U; index < 3U; ++index) {
        output->world[index] = start->world[index]
            + (end->world[index] - start->world[index]) * amount;
        output->camera[index] = start->camera[index]
            + (end->camera[index] - start->camera[index]) * amount;
    }
    for (index = 0U; index < 4U; ++index) {
        output->eye[index] = start->eye[index]
            + (end->eye[index] - start->eye[index]) * amount;
        output->clip[index] = start->clip[index]
            + (end->clip[index] - start->clip[index]) * amount;
    }
    for (index = 0U; index < 2U; ++index) {
        output->texture[index] = start->texture[index]
            + (end->texture[index] - start->texture[index]) * amount;
    }
    for (index = 0U; index < 4U; ++index) {
        output->rgba[index] = start->rgba[index]
            + (end->rgba[index] - start->rgba[index]) * amount;
    }
}

static GeDamCameraStatus ge_dam_camera_clip_plane(
    const GeDamCamera *camera,
    const GeDamCameraClipVertex *input,
    size_t input_count,
    GeDamCameraClipVertex *output,
    size_t *output_count,
    GeDamCameraPlane plane)
{
    const GeDamCameraClipVertex *previous;
    float previous_distance;
    int previous_inside;
    size_t index;

    *output_count = 0U;
    if (input_count == 0U) {
        return GE_DAM_CAMERA_OK;
    }
    previous = &input[input_count - 1U];
    previous_distance = ge_dam_camera_plane_distance(camera, previous, plane);
    previous_inside = previous_distance >= 0.0f;
    for (index = 0U; index < input_count; ++index) {
        const GeDamCameraClipVertex *current = &input[index];
        const float current_distance = ge_dam_camera_plane_distance(
            camera, current, plane);
        const int current_inside = current_distance >= 0.0f;

        if (current_inside != previous_inside) {
            const float amount = previous_distance
                / (previous_distance - current_distance);

            if (*output_count >= GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES) {
                return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
            }
            ge_dam_camera_interpolate(&output[*output_count], previous,
                                      current, amount);
            ++*output_count;
        }
        if (current_inside) {
            if (*output_count >= GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES) {
                return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
            }
            output[*output_count] = *current;
            ++*output_count;
        }
        previous = current;
        previous_distance = current_distance;
        previous_inside = current_inside;
    }
    return GE_DAM_CAMERA_OK;
}

static GeDamCameraStatus ge_dam_camera_clip_triangle(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex input[3],
    GeDamCameraClipVertex output[GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES],
    size_t *output_count)
{
    GeDamCameraClipVertex scratch[2][
        GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES];
    size_t input_count = 3U;
    size_t plane_index;
    size_t index;
    uint32_t outside_any = 0U;
    uint32_t outside_all = (UINT32_C(1) << GE_DAM_CAMERA_PLANE_COUNT) - 1U;

    for (index = 0U; index < 3U; ++index) {
        uint32_t outside = 0U;
        if (!ge_dam_camera_input_finite(&input[index])) {
            return GE_DAM_CAMERA_NONFINITE_INPUT;
        }
        ge_dam_camera_import_vertex(camera, &input[index], &scratch[0][index]);
        for (plane_index = 0U; plane_index < GE_DAM_CAMERA_PLANE_COUNT;
                ++plane_index) {
            if (ge_dam_camera_plane_distance(
                    camera, &scratch[0][index],
                    (GeDamCameraPlane)plane_index) < 0.0f) {
                outside |= UINT32_C(1) << plane_index;
            }
        }
        outside_any |= outside;
        outside_all &= outside;
    }
    if (outside_any == 0U) {
        memcpy(output, scratch[0], 3U * sizeof(*output));
        *output_count = 3U;
        return GE_DAM_CAMERA_OK;
    }
    if (outside_all != 0U) {
        *output_count = 0U;
        return GE_DAM_CAMERA_OK;
    }
    for (plane_index = 0U; plane_index < GE_DAM_CAMERA_PLANE_COUNT;
            ++plane_index) {
        const size_t source = plane_index & 1U;
        const size_t destination = source ^ 1U;
        GeDamCameraStatus status = ge_dam_camera_clip_plane(
            camera, scratch[source], input_count, scratch[destination],
            &input_count, (GeDamCameraPlane)plane_index);

        if (status != GE_DAM_CAMERA_OK) {
            return status;
        }
        if (input_count < 3U) {
            *output_count = 0U;
            return GE_DAM_CAMERA_OK;
        }
    }
    memcpy(output, scratch[GE_DAM_CAMERA_PLANE_COUNT & 1U],
           input_count * sizeof(*output));
    *output_count = input_count;
    return GE_DAM_CAMERA_OK;
}

static uint8_t ge_dam_camera_color_byte(float color)
{
    if (color <= 0.0f) {
        return 0U;
    }
    if (color >= 255.0f) {
        return UINT8_MAX;
    }
    return (uint8_t)(color + 0.5f);
}

static void ge_dam_camera_project_vertex(const GeDamCamera *camera,
                                         const GeDamCameraClipVertex *input,
                                         GeDamCameraVertex *output)
{
    const float inverse_depth = 1.0f / input->clip[3];
    const float ndc_x = input->clip[0] * inverse_depth;
    const float ndc_y = input->clip[1] * inverse_depth;
    const float ndc_z = input->clip[2] * inverse_depth;
    size_t index;

    memcpy(output->world, input->world, sizeof(output->world));
    memcpy(output->camera, input->camera, sizeof(output->camera));
    memcpy(output->clip, input->clip, sizeof(output->clip));
    output->screen[0] = ndc_x * camera->viewport_scale[0]
        + camera->viewport_translation[0];
    output->screen[1] = ndc_y * camera->viewport_scale[1]
        + camera->viewport_translation[1];
    output->screen[2] = (ndc_z + 1.0f) * 0.5f;
    output->inverse_depth = inverse_depth;
    for (index = 0U; index < 2U; ++index) {
        output->texture[index] = input->texture[index];
        output->texture_over_depth[index] = input->texture[index]
            * inverse_depth;
    }
    for (index = 0U; index < 4U; ++index) {
        output->rgba[index] = ge_dam_camera_color_byte(input->rgba[index]);
    }
}

static int ge_dam_camera_add_size(size_t left, size_t right, size_t *output)
{
    if (right > SIZE_MAX - left) {
        return 0;
    }
    *output = left + right;
    return 1;
}

/* One exact clip/triangulate traversal. A null output counts the required
 * vertices; a non-null output projects the already capacity-validated result.
 * Keeping this primitive below the public atomic APIs lets batched projection
 * reuse its outer count pass instead of recursively recounting every batch. */
static GeDamCameraStatus ge_dam_camera_project_pass(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_triangle_count,
    GeDamCameraVertex *output,
    size_t output_capacity,
    size_t *vertex_count,
    size_t *visible_input_triangle_count,
    size_t *clip_triangle_evaluations)
{
    size_t cursor = 0U;
    size_t visible = 0U;
    size_t triangle_index;

    for (triangle_index = 0U; triangle_index < input_triangle_count;
            ++triangle_index) {
        GeDamCameraClipVertex polygon[
            GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES];
        size_t polygon_count = 0U;
        size_t fan_index;
        GeDamCameraStatus status = ge_dam_camera_clip_triangle(
            camera, &input_vertices[triangle_index * 3U], polygon,
            &polygon_count);

        if (clip_triangle_evaluations != NULL) {
            ++*clip_triangle_evaluations;
        }
        if (status != GE_DAM_CAMERA_OK) return status;
        if (polygon_count >= 3U) ++visible;
        for (fan_index = 1U; fan_index + 1U < polygon_count; ++fan_index) {
            size_t end;

            if (!ge_dam_camera_add_size(cursor, 3U, &end)
                    || (output != NULL && end > output_capacity)) {
                return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
            }
            if (output != NULL) {
                ge_dam_camera_project_vertex(camera, &polygon[0],
                    &output[cursor]);
                ge_dam_camera_project_vertex(camera, &polygon[fan_index],
                    &output[cursor + 1U]);
                ge_dam_camera_project_vertex(camera,
                    &polygon[fan_index + 1U], &output[cursor + 2U]);
            }
            cursor = end;
        }
    }
    *vertex_count = cursor;
    if (visible_input_triangle_count != NULL) {
        *visible_input_triangle_count = visible;
    }
    return GE_DAM_CAMERA_OK;
}

GeDamCameraStatus ge_dam_camera_project(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamCameraStorage *storage,
    GeDamCameraResult *result)
{
    const GeDamCameraStorage empty_storage = {NULL, 0U};
    const GeDamCameraStorage *actual_storage = storage != NULL
        ? storage : &empty_storage;
    size_t required_vertices = 0U;
    size_t visible_inputs = 0U;
    size_t emitted_vertices = 0U;
    size_t clip_evaluations = 0U;
    GeDamCameraStatus status;

    if (result == NULL) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (camera == NULL || (input_vertex_count != 0U
            && input_vertices == NULL) || input_vertex_count % 3U != 0U
            || (actual_storage->vertex_capacity != 0U
                && actual_storage->vertices == NULL)) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    result->input_triangle_count = input_vertex_count / 3U;
    status = ge_dam_camera_project_pass(camera, input_vertices,
        result->input_triangle_count, NULL, 0U, &required_vertices,
        &visible_inputs, &clip_evaluations);
    if (status != GE_DAM_CAMERA_OK) return status;
    result->required_vertex_count = required_vertices;
    result->visible_input_triangle_count = visible_inputs;
    result->output_triangle_count = required_vertices / 3U;
    result->clip_triangle_evaluations = clip_evaluations;
    if (actual_storage->vertex_capacity < required_vertices) {
        return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
    }
    status = ge_dam_camera_project_pass(camera, input_vertices,
        result->input_triangle_count, actual_storage->vertices,
        actual_storage->vertex_capacity, &emitted_vertices, NULL,
        &clip_evaluations);
    if (status != GE_DAM_CAMERA_OK) return status;
    if (emitted_vertices != required_vertices) {
        return GE_DAM_CAMERA_INVALID_CONFIG;
    }
    result->visible_input_triangle_count = visible_inputs;
    result->output_triangle_count = required_vertices / 3U;
    result->required_vertex_count = required_vertices;
    result->vertex_count = required_vertices;
    result->clip_triangle_evaluations = clip_evaluations;
    return GE_DAM_CAMERA_OK;
}

GeDamCameraStatus ge_dam_camera_project_batches(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamRoomDrawBatch *input_batches,
    size_t input_batch_count,
    const GeDamCameraSceneStorage *storage,
    GeDamCameraSceneResult *result)
{
    const GeDamCameraSceneStorage empty_storage = {NULL, 0U, NULL, 0U};
    const GeDamCameraSceneStorage *actual_storage = storage != NULL
        ? storage : &empty_storage;
    size_t required_vertices = 0U;
    size_t required_batches = 0U;
    size_t input_triangles = 0U;
    size_t visible_triangles = 0U;
    size_t output_triangles = 0U;
    size_t batch_index;
    size_t clip_evaluations = 0U;

    if (result == NULL) return GE_DAM_CAMERA_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (camera == NULL
            || (input_vertex_count != 0U && input_vertices == NULL)
            || (input_batch_count != 0U
                && (input_batches == NULL || input_vertices == NULL))
            || (actual_storage->vertex_capacity != 0U
                && actual_storage->vertices == NULL)
            || (actual_storage->batch_capacity != 0U
                && actual_storage->batches == NULL)) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }
    for (batch_index = 0U; batch_index < input_batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &input_batches[batch_index];

        if (batch->vertex_count % 3U != 0U
                || batch->first_vertex > input_vertex_count
                || batch->vertex_count
                    > input_vertex_count - batch->first_vertex) {
            return GE_DAM_CAMERA_INVALID_ARGUMENT;
        }
        if (!ge_dam_camera_add_size(input_triangles,
                batch->vertex_count / 3U, &input_triangles)) {
            return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
        }
    }

    for (batch_index = 0U; batch_index < input_batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &input_batches[batch_index];
        size_t batch_vertices = 0U;
        size_t batch_visible = 0U;
        GeDamCameraStatus status = ge_dam_camera_project_pass(
            camera, input_vertices + batch->first_vertex,
            batch->vertex_count / 3U, NULL, 0U, &batch_vertices,
            &batch_visible, &clip_evaluations);

        if (status != GE_DAM_CAMERA_OK) return status;
        if (!ge_dam_camera_add_size(required_vertices, batch_vertices,
                &required_vertices)
                || !ge_dam_camera_add_size(visible_triangles, batch_visible,
                    &visible_triangles)
                || !ge_dam_camera_add_size(output_triangles,
                    batch_vertices / 3U, &output_triangles)) {
            return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
        }
        if (batch_vertices != 0U
                && !ge_dam_camera_add_size(required_batches, 1U,
                    &required_batches)) {
            return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
        }
    }
    result->input_triangle_count = input_triangles;
    result->visible_input_triangle_count = visible_triangles;
    result->output_triangle_count = output_triangles;
    result->required_vertex_count = required_vertices;
    result->required_batch_count = required_batches;
    result->clip_triangle_evaluations = clip_evaluations;
    if (actual_storage->vertex_capacity < required_vertices
            || actual_storage->batch_capacity < required_batches) {
        return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
    }
    {
        size_t vertex_cursor = 0U;
        size_t batch_cursor = 0U;

        for (batch_index = 0U; batch_index < input_batch_count; ++batch_index) {
            const GeDamRoomDrawBatch *batch = &input_batches[batch_index];
            GeDamCameraVertex *batch_output = actual_storage->vertices != NULL
                ? actual_storage->vertices + vertex_cursor : NULL;
            size_t batch_vertices = 0U;
            GeDamCameraStatus status = ge_dam_camera_project_pass(
                camera, input_vertices + batch->first_vertex,
                batch->vertex_count / 3U,
                batch_output,
                actual_storage->vertex_capacity - vertex_cursor,
                &batch_vertices, NULL, &clip_evaluations);

            if (status != GE_DAM_CAMERA_OK) return status;
            if (batch_vertices != 0U) {
                actual_storage->batches[batch_cursor++] =
                    (GeDamCameraBatch){batch_index, vertex_cursor,
                                       batch_vertices};
                vertex_cursor += batch_vertices;
            }
        }
        if (vertex_cursor != required_vertices
                || batch_cursor != required_batches) {
            return GE_DAM_CAMERA_INVALID_CONFIG;
        }
        result->vertex_count = vertex_cursor;
        result->batch_count = batch_cursor;
    }
    result->clip_triangle_evaluations = clip_evaluations;
    return GE_DAM_CAMERA_OK;
}

GeDamCameraStatus ge_dam_camera_project_batches_bounded(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamRoomDrawBatch *input_batches,
    size_t input_batch_count,
    const GeDamCameraSceneStorage *storage,
    GeDamCameraSceneResult *result)
{
    size_t vertex_cursor = 0U;
    size_t batch_cursor = 0U;
    size_t visible_triangles = 0U;
    size_t input_triangles = 0U;
    size_t clip_evaluations = 0U;
    size_t batch_index;

    if (result == NULL) return GE_DAM_CAMERA_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (camera == NULL || storage == NULL
            || (input_vertex_count != 0U && input_vertices == NULL)
            || (input_batch_count != 0U
                && (input_batches == NULL || input_vertices == NULL))
            || (storage->vertex_capacity != 0U
                && storage->vertices == NULL)
            || (storage->batch_capacity != 0U
                && storage->batches == NULL)) {
        return GE_DAM_CAMERA_INVALID_ARGUMENT;
    }

    for (batch_index = 0U; batch_index < input_batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &input_batches[batch_index];
        size_t batch_vertices = 0U;
        size_t batch_visible = 0U;
        size_t next_input_triangles;
        GeDamCameraStatus status;

        if (batch->vertex_count % 3U != 0U
                || batch->first_vertex > input_vertex_count
                || batch->vertex_count
                    > input_vertex_count - batch->first_vertex
                || !ge_dam_camera_add_size(input_triangles,
                    batch->vertex_count / 3U, &next_input_triangles)) {
            return GE_DAM_CAMERA_INVALID_ARGUMENT;
        }
        input_triangles = next_input_triangles;
        status = ge_dam_camera_project_pass(
            camera, input_vertices + batch->first_vertex,
            batch->vertex_count / 3U,
            storage->vertices != NULL
                ? storage->vertices + vertex_cursor : NULL,
            storage->vertex_capacity - vertex_cursor,
            &batch_vertices, &batch_visible, &clip_evaluations);
        if (status != GE_DAM_CAMERA_OK) return status;
        if (batch_vertices != 0U) {
            if (batch_cursor >= storage->batch_capacity) {
                return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
            }
            storage->batches[batch_cursor++] = (GeDamCameraBatch){
                batch_index, vertex_cursor, batch_vertices,
            };
            vertex_cursor += batch_vertices;
        }
        if (!ge_dam_camera_add_size(visible_triangles, batch_visible,
                                    &visible_triangles)) {
            return GE_DAM_CAMERA_CAPACITY_EXCEEDED;
        }
    }

    result->input_triangle_count = input_triangles;
    result->visible_input_triangle_count = visible_triangles;
    result->output_triangle_count = vertex_cursor / 3U;
    result->required_vertex_count = vertex_cursor;
    result->required_batch_count = batch_cursor;
    result->vertex_count = vertex_cursor;
    result->batch_count = batch_cursor;
    result->clip_triangle_evaluations = clip_evaluations;
    return GE_DAM_CAMERA_OK;
}

const char *ge_dam_camera_status_name(GeDamCameraStatus status)
{
    switch (status) {
    case GE_DAM_CAMERA_OK:
        return "ok";
    case GE_DAM_CAMERA_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_DAM_CAMERA_INVALID_CONFIG:
        return "invalid config";
    case GE_DAM_CAMERA_NONFINITE_INPUT:
        return "nonfinite input";
    case GE_DAM_CAMERA_CAPACITY_EXCEEDED:
        return "capacity exceeded";
    default:
        return "unknown";
    }
}
