#include "ge_dam_sky.h"

#include <math.h>
#include <string.h>

#include "ge_dam_environment.h"

typedef struct GeDamSkyClipVertex {
    float x;
    float y;
    float ray[3];
} GeDamSkyClipVertex;

static float ge_dam_sky_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static int ge_dam_sky_normalize(float value[3])
{
    const float length = sqrtf(ge_dam_sky_dot(value, value));

    if (!isfinite(length) || length <= 0.00001f) return 0;
    value[0] /= length;
    value[1] /= length;
    value[2] /= length;
    return 1;
}

static void ge_dam_sky_cross(float output[3], const float a[3],
                             const float b[3])
{
    output[0] = a[1] * b[2] - a[2] * b[1];
    output[1] = a[2] * b[0] - a[0] * b[2];
    output[2] = a[0] * b[1] - a[1] * b[0];
}

static void ge_dam_sky_ray(GeDamSkyClipVertex *vertex,
                           const GeDamSkyCamera *camera,
                           const float look[3], const float right[3],
                           const float up[3], float tan_vertical)
{
    const float center_x = camera->viewport_left
        + camera->viewport_width * 0.5f;
    const float center_y = camera->viewport_top
        + camera->viewport_height * 0.5f;
    const float horizontal = (vertex->x - center_x)
        / (camera->viewport_width * 0.5f);
    const float vertical = (center_y - vertex->y)
        / (camera->viewport_height * 0.5f);
    const float tan_horizontal = tan_vertical * camera->aspect;

    vertex->ray[0] = look[0] + right[0] * horizontal * tan_horizontal
        + up[0] * vertical * tan_vertical;
    vertex->ray[1] = look[1] + right[1] * horizontal * tan_horizontal
        + up[1] * vertical * tan_vertical;
    vertex->ray[2] = look[2] + right[2] * horizontal * tan_horizontal
        + up[2] * vertical * tan_vertical;
}

static GeDamSkyClipVertex ge_dam_sky_intersection(
    const GeDamSkyClipVertex *start, const GeDamSkyClipVertex *end)
{
    GeDamSkyClipVertex output;
    const float denominator = start->ray[1] - end->ray[1];
    const float fraction = denominator != 0.0f
        ? start->ray[1] / denominator : 0.0f;
    size_t axis;

    output.x = start->x + (end->x - start->x) * fraction;
    output.y = start->y + (end->y - start->y) * fraction;
    for (axis = 0U; axis < 3U; ++axis)
        output.ray[axis] = start->ray[axis]
            + (end->ray[axis] - start->ray[axis]) * fraction;
    output.ray[1] = 0.0f;
    return output;
}

static size_t ge_dam_sky_clip_horizon(const GeDamSkyClipVertex input[4],
                                      GeDamSkyClipVertex output[5])
{
    const GeDamSkyClipVertex *previous = &input[3];
    int previous_inside = previous->ray[1] >= 0.0f;
    size_t output_count = 0U;
    size_t index;

    for (index = 0U; index < 4U; ++index) {
        const GeDamSkyClipVertex *current = &input[index];
        const int current_inside = current->ray[1] >= 0.0f;

        if (current_inside != previous_inside)
            output[output_count++] =
                ge_dam_sky_intersection(previous, current);
        if (current_inside) output[output_count++] = *current;
        previous = current;
        previous_inside = current_inside;
    }
    return output_count;
}

static GeDamSkyVertex ge_dam_sky_emit_vertex(
    const GeDamSkyClipVertex *source, const GeDamSkyCamera *camera,
    const GeOriginalStageEnvironment *environment, float cloud_offset)
{
    GeDamSkyVertex output;
    const float horizontal_length = sqrtf(source->ray[0] * source->ray[0]
        + source->ray[2] * source->ray[2] + 0.0001f);
    float vertical = source->ray[1];
    float intersection_scale;
    float horizontal_distance;
    float sky_strength;
    float world_x;
    float world_z;

    /* skyIsScreenCornerInSky uses +0.01 exactly at the horizon, then caps
     * horizontal reach at 300000 before generating cloud UVs. */
    if (vertical == 0.0f) vertical = 0.01f;
    intersection_scale =
        (environment->cloud_repeat - camera->position[1]) / vertical;
    horizontal_distance = horizontal_length * intersection_scale;
    if (horizontal_distance > 300000.0f)
        intersection_scale *= 300000.0f / horizontal_distance;
    world_x = camera->position[0] + source->ray[0] * intersection_scale;
    world_z = camera->position[2] + source->ray[2] * intersection_scale;

    sky_strength = 2.0f * source->ray[1] / horizontal_length;
    if (sky_strength > 1.0f) sky_strength = 1.0f;
    if (sky_strength < 0.0f) sky_strength = 0.0f;

    output.screen_x = source->x;
    output.screen_y = source->y;
    /* skyRender stores world X/Z at 0.1 texel per unit. The authored image is
     * the 64x64 IMAGE_CLOUDS_GRAYSCALE entry. */
    output.texture_u = world_x * (0.1f / 64.0f);
    output.texture_v = (world_z * 0.1f + cloud_offset) / 64.0f;
    output.red = ((float)environment->red
        + environment->cloud_red
            * (1.0f - (float)environment->red / 255.0f)
            * sky_strength) / 255.0f;
    output.green = ((float)environment->green
        + environment->cloud_green
            * (1.0f - (float)environment->green / 255.0f)
            * sky_strength) / 255.0f;
    output.blue = ((float)environment->blue
        + environment->cloud_blue
            * (1.0f - (float)environment->blue / 255.0f)
            * sky_strength) / 255.0f;
    output.alpha = 1.0f;
    return output;
}

void ge_dam_sky_tick(float *cloud_offset, int32_t clock_timer)
{
    if (cloud_offset == NULL) return;
    *cloud_offset += (float)clock_timer;
    if (*cloud_offset > 4096.0f) *cloud_offset -= 4096.0f;
}

int ge_dam_sky_build_environment(
    const GeDamSkyCamera *camera,
    const GeOriginalStageEnvironment *environment,
    float cloud_offset, GeDamSkyScene *scene)
{
    GeDamSkyClipVertex corners[4];
    GeDamSkyClipVertex polygon[GE_DAM_SKY_MAX_POLYGON_VERTICES];
    GeDamSkyVertex vertices[GE_DAM_SKY_MAX_POLYGON_VERTICES];
    float look[3];
    float right[3];
    float up[3];
    float up_projection;
    float tan_vertical;
    size_t polygon_count;
    size_t index;
    size_t output_index = 0U;

    if (camera == NULL || environment == NULL || scene == NULL
            || camera->viewport_width <= 0.0f
            || camera->viewport_height <= 0.0f
            || camera->vertical_fov_degrees <= 0.0f
            || camera->vertical_fov_degrees >= 180.0f
            || camera->aspect <= 0.0f) return 0;
    memset(scene, 0, sizeof(*scene));
    memcpy(look, camera->look, sizeof(look));
    memcpy(up, camera->up, sizeof(up));
    if (!ge_dam_sky_normalize(look)) return 0;
    up_projection = ge_dam_sky_dot(up, look);
    for (index = 0U; index < 3U; ++index)
        up[index] -= look[index] * up_projection;
    if (!ge_dam_sky_normalize(up)) return 0;
    ge_dam_sky_cross(right, look, up);
    if (!ge_dam_sky_normalize(right)) return 0;
    tan_vertical = tanf(camera->vertical_fov_degrees
        * (3.14159265358979323846f / 360.0f));

    corners[0].x = camera->viewport_left;
    corners[0].y = camera->viewport_top;
    corners[1].x = camera->viewport_left + camera->viewport_width;
    corners[1].y = camera->viewport_top;
    corners[2].x = camera->viewport_left + camera->viewport_width;
    corners[2].y = camera->viewport_top + camera->viewport_height;
    corners[3].x = camera->viewport_left;
    corners[3].y = camera->viewport_top + camera->viewport_height;
    for (index = 0U; index < 4U; ++index)
        ge_dam_sky_ray(&corners[index], camera, look, right, up,
                       tan_vertical);
    polygon_count = ge_dam_sky_clip_horizon(corners, polygon);
    if (polygon_count < 3U || polygon_count > GE_DAM_SKY_MAX_POLYGON_VERTICES)
        return 1;
    for (index = 0U; index < polygon_count; ++index)
        vertices[index] = ge_dam_sky_emit_vertex(
            &polygon[index], camera, environment, cloud_offset);
    for (index = 1U; index + 1U < polygon_count; ++index) {
        scene->vertices[output_index++] = vertices[0];
        scene->vertices[output_index++] = vertices[index];
        scene->vertices[output_index++] = vertices[index + 1U];
    }
    scene->vertex_count = output_index;
    scene->polygon_vertex_count = polygon_count;
    return 1;
}

int ge_dam_sky_build(const GeDamSkyCamera *camera, float cloud_offset,
                     GeDamSkyScene *scene)
{
    const GeOriginalStageEnvironment environment = {
        .red = ge_dam_environment.red,
        .green = ge_dam_environment.green,
        .blue = ge_dam_environment.blue,
        .clouds = ge_dam_environment.clouds,
        .cloud_repeat = ge_dam_environment.cloud_repeat,
        .cloud_red = ge_dam_environment.cloud_red,
        .cloud_green = ge_dam_environment.cloud_green,
        .cloud_blue = ge_dam_environment.cloud_blue,
    };
    return ge_dam_sky_build_environment(
        camera, &environment, cloud_offset, scene);
}
